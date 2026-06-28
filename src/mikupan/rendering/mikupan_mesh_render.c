#include "mikupan_renderer_internal.h"
#include "graphics/graph3d/sglib.h"
#include "graphics/graph3d/sgsu.h"
#include "mikupan/mikupan_utils.h"
#include "mikupan/mikupan_logging_c.h"
#include "mikupan/ui/mikupan_ui.h"
#include "mikupan_gpu.h"
#include "mikupan_meshcache.h"
#include "mikupan_pipeline.h"
#include "mikupan_profiler.h"
#include "mikupan_shader.h"
#include "SDL3/SDL_timer.h"
#include <string.h>

#define MIKUPAN_MESH_BUFFER_CAPACITY (1024 * 1024)

typedef struct
{
    int   indices  [MIKUPAN_MESH_BUFFER_CAPACITY];
    float positions[MIKUPAN_MESH_BUFFER_CAPACITY];
    float normals  [MIKUPAN_MESH_BUFFER_CAPACITY];
    float uvs      [MIKUPAN_MESH_BUFFER_CAPACITY];
    float colors   [MIKUPAN_MESH_BUFFER_CAPACITY];
} MikuPan_MeshBuffers0x32;

typedef struct
{
    int   indices[MIKUPAN_MESH_BUFFER_CAPACITY];
    float uvs    [MIKUPAN_MESH_BUFFER_CAPACITY];
    float colors [MIKUPAN_MESH_BUFFER_CAPACITY];
} MikuPan_MeshBuffers0x82;

typedef struct
{
    int   indices[MIKUPAN_MESH_BUFFER_CAPACITY];
    float uvs    [MIKUPAN_MESH_BUFFER_CAPACITY];
} MikuPan_MeshBuffers0x2;

static MikuPan_MeshBuffers0x32 g_mesh_buffers_0x32 = {0};
static MikuPan_MeshBuffers0x82 g_mesh_buffers_0x82 = {0};
static MikuPan_MeshBuffers0x2  g_mesh_buffers_0x2  = {0};

/// GPU-skinning bone-matrix palette. One storage buffer, re-uploaded only when
/// the palette content hash changes. The hash is computed once per model per
/// pass in sgsu.c (MikuPan_SkinPaletteHash), so this avoids re-hashing per mesh
/// and dedupes the upload across all meshes of a model AND across the scene +
/// mirror-reflection passes (identical content) — only the actual render-pass
/// flush on a genuine change remains.
static unsigned int        g_skin_palette_buffer = 0;
static unsigned long long  g_skin_palette_uploaded_hash = 0;
static int                 g_skin_palette_valid = 0;

/// Upload the current skinned mesh's bone palette (hash-guarded) and bind it to
/// the vertex storage slot for the upcoming draw.
static void MikuPan_BindSkinPalette(void)
{
    int bones = 0;
    const void *pal = MikuPan_SkinPalette(&bones);
    if (pal == NULL || bones <= 0)
    {
        return;
    }

    if (g_skin_palette_buffer == 0)
    {
        /* Generous initial allocation; MikuPan_GPUUploadBuffer grows as needed. */
        g_skin_palette_buffer = MikuPan_GPUCreateBuffer(
            (unsigned int)(256 * 64), MIKUPAN_GPU_BUFFER_STORAGE);
    }

    unsigned long long h = MikuPan_SkinPaletteHash();
    if (!g_skin_palette_valid || g_skin_palette_uploaded_hash != h)
    {
        long long size = (long long)bones * 64; /* sceVu0FMATRIX = 4x float4 */
        MikuPan_GPUUploadBuffer(g_skin_palette_buffer, (unsigned int)size, pal);
        g_skin_palette_uploaded_hash = h;
        g_skin_palette_valid = 1;
    }

    MikuPan_GPUSetVertexStorageBuffer(g_skin_palette_buffer);
}
static float g_zero_floats[MIKUPAN_MESH_BUFFER_CAPACITY] = {0};

static int MikuPan_GetMeshRenderMode()
{
    if (MikuPan_IsShadowPassActive() || MikuPan_IsShadowReceiverPassActive())
    {
        return GL_TRIANGLES;
    }

    return MikuPan_IsWireframeRendering() ? GL_LINES : GL_TRIANGLES;
}

static void MikuPan_SetMeshRenderStateForCurrentPass(void)
{
    if (MikuPan_IsShadowReceiverPassActive())
    {
        MikuPan_SetRenderStateShadowReceiver();
    }
    else if (MikuPan_IsShadowPassActive())
    {
        MikuPan_SetRenderStateShadow();
    }
    else if (MikuPan_IsMirrorReflectionPass())
    {
        /* The reflection flips winding, so cull the front faces instead of the
         * back — otherwise reflected single-sided geometry is culled away. */
        MikuPan_SetRenderState3DMirror();
    }
    else
    {
        MikuPan_SetRenderState3D();
    }
}

/// Which cache namespace the current pass writes to. The same pPUHead is drawn
/// in the lit pass, the shadow caster pass and the shadow receiver pass with
/// different pipelines, so each pass keeps a separate cache entry.
static int MikuPan_CurrentPassCacheKind(void)
{
    if (MikuPan_IsShadowReceiverPassActive()) return MIKUPAN_MESHCACHE_KIND_RECEIVER;
    if (MikuPan_IsShadowPassActive())         return MIKUPAN_MESHCACHE_KIND_CASTER;
    return MIKUPAN_MESHCACHE_KIND_MAIN;
}

/// The mesh cache now keys on the render pass too (see MikuPan_CurrentPassCacheKind),
/// so the shadow caster / receiver passes get their own cached buffers instead of
/// re-uploading all of their geometry every frame.
static int MikuPan_CanUseMeshCacheForCurrentPass(void)
{
    return MikuPan_MeshCache_IsEnabled();
}

static int MikuPan_ShouldSkipFullyTransparentMaterialMesh(void)
{
    if (MikuPan_IsShadowPassActive() || MikuPan_IsShadowReceiverPassActive())
    {
        return 0;
    }

    return MikuPan_IsCurrentMaterialFullyTransparent();
}

static void MikuPan_BindDiffuseTextureForShadowCaster(sceGsTex0 *tex0)
{
    if (tex0 != NULL)
    {
        MikuPan_SetTexture(tex0);
    }
    else
    {
        MikuPan_ActiveTextureCached(GL_TEXTURE0);
        MikuPan_BindTexture2DCached(0);
    }
}

void MikuPan_RenderMeshType0x32(SGDPROCUNITHEADER *pVUVN, SGDPROCUNITHEADER *pPUHead)
{
    MIKUPAN_PERF_SCOPE(PERF_SECT_MESH_RENDER);

    char mesh_type = GET_MESH_TYPE(pPUHead);

    if (pPUHead->VUMeshDesc.MeshType.TEX == 0)
    {
        return;
    }

    if (mesh_type != 0x12 && mesh_type != 0x10 && mesh_type != 0x32)
    {
        return;
    }

    if (!MikuPan_IsShadowPassActive())
    {
        if (((mesh_type == 0x12) && !MikuPan_IsMesh0x12Rendering()) || ((mesh_type == 0x10) && !MikuPan_IsMesh0x10Rendering()) ||
            (mesh_type == 0x32 && !MikuPan_IsMesh0x32Rendering()))
        {
            return;
        }
    }

    if (MikuPan_ShouldSkipFullyTransparentMaterialMesh())
    {
        return;
    }

    int per_type_sect = (mesh_type == 0x32) ? PERF_SECT_MESH_0x32 :
                        (mesh_type == 0x12) ? PERF_SECT_MESH_0x12 :
                                              PERF_SECT_MESH_0x10;

    MIKUPAN_PERF_SCOPE(per_type_sect);

    SGDPROCUNITDATA *pProcData = (SGDPROCUNITDATA *) &pPUHead[1];
    SGDPROCUNITDATA *pVUVNData = (SGDPROCUNITDATA *) &pVUVN[1];

    sceGsTex0 *mesh_tex_reg = (sceGsTex0 *) ((int64_t) pProcData + 0x28);

    SGDVUMESHSTDATA *sgdMeshData = (SGDVUMESHSTDATA *)((int64_t) pProcData + (pProcData->VUMeshData_Preset.sOffsetToST - 1) * 4);
    _SGDVUMESHCOLORDATA *pVMCD = (_SGDVUMESHCOLORDATA*) (&pPUHead->pNext + pProcData->VUMeshData_Preset.sOffsetToPrim);

    int desired_pipeline = (mesh_type == 0x32) ? POSITION3_NORMAL3_UV2_SOA : POSITION3_NORMAL3_UV2;

    MikuPan_PipelineInfo *pipeline = MikuPan_GetPipelineInfo(desired_pipeline);
    const int uv_buf_idx    = (mesh_type == 0x32) ? 2 : 1;
    const int color_buf_idx = (mesh_type == 0x32) ? 3 : 2;
    const int uv_stride    = pipeline->buffers[uv_buf_idx].attributes[0].stride;
    const int color_stride = pipeline->buffers[color_buf_idx].attributes[0].stride;

    MikuPan_FlushTexturedSpriteBatch();

    /// Per-frame state setup. Shader/texture/render-state can change between
    /// frames even when the underlying geometry doesn't, so they stay outside
    /// the mesh-buffer cache and run on every draw.
    Uint64 _sc_t0 = SDL_GetPerformanceCounter();
    Uint64 _t = MikuPan_PerfBegin();
    MikuPan_SetCurrentShaderProgram(MESH_0x12_SHADER);
    MikuPan_PerfEnd(PERF_SECT_SC_SHADER, _t);

    if (!MikuPan_IsShadowReceiverPassActive())
    {
        _t = MikuPan_PerfBegin();
        if ((int64_t)mesh_tex_reg < (int64_t)pVMCD)
        {
            MikuPan_BindDiffuseTextureForShadowCaster(mesh_tex_reg);
        }
        else if (MikuPan_IsShadowPassActive())
        {
            MikuPan_BindDiffuseTextureForShadowCaster(NULL);
        }
        MikuPan_PerfEnd(PERF_SECT_SC_TEXTURE, _t);
    }

    _t = MikuPan_PerfBegin();
    MikuPan_SetMeshRenderStateForCurrentPass();
    MikuPan_PerfEnd(PERF_SECT_SC_RS3D, _t);
    MikuPan_PerfEnd(PERF_SECT_STATE_CHANGE, _sc_t0);

    /// ── Cache fast path ──
    /// Static furniture / map geometry: positions, UVs, normals, indices AND
    /// the baked preset vertex colours are all immutable, so a cache hit binds
    /// the VAO and draws with zero per-frame buffer work. Lighting and BW run in
    /// the shader, so they need no CPU colour rebuild.
    int cache_on   = MikuPan_CanUseMeshCacheForCurrentPass();
    const int cache_kind = MikuPan_CurrentPassCacheKind();
    MikuPan_MeshCacheEntry *cache_entry = NULL;

    if (cache_on)
    {
        cache_entry = MikuPan_MeshCache_Lookup(pPUHead, cache_kind);
        if (cache_entry != NULL && cache_entry->sgd_top == sgd_top_addr)
        {
            /// Preset vertex colours are baked, static map/furniture data: the
            /// PS2 lighting and black-and-white passes run in the shader (see
            /// CalcPS2LitColor / ToBlackWhite), and MikuPan_FixColors is an
            /// idempotent static fixup. So the colour VBO uploaded on the cache
            /// miss is already correct — no per-frame rebuild or re-stream is
            /// needed on a hit. (If a mesh ever animates its base colours on the
            /// CPU, it would freeze at its first-frame colours; none do today.)

            _t = MikuPan_PerfBegin();
            MikuPan_BindVAO(cache_entry->vao);
            MikuPan_PerfEnd(PERF_SECT_SC_VAO, _t);

            MikuPan_PerfDrawCall();
            MikuPan_TimedDrawElements(MikuPan_GetMeshRenderMode(),
                                      cache_entry->index_count, GL_UNSIGNED_INT, (void *)0);

            if (!MikuPan_IsShadowPassActive() &&
                !MikuPan_IsShadowReceiverPassActive() &&
                MikuPan_IsNormalsRendering())
            {
                MikuPan_SetCurrentShaderProgram(NORMALS_0x12_SHADER);
                MikuPan_TimedDrawElements(GL_TRIANGLES, cache_entry->index_count,
                                          GL_UNSIGNED_INT, (void *)0);
            }

            MikuPan_PerfMeshCacheHit();
            return;
        }

        /// Miss — allocate a per-mesh entry now and target its static buffers
        /// instead of the shared streaming buffers during the upload phase.
        MikuPan_PerfMeshCacheMissNew();
        cache_entry = MikuPan_MeshCache_Insert(pPUHead, sgd_top_addr, desired_pipeline, cache_kind);
        if (cache_entry == NULL)
        {
            // Could not allocate cache-owned buffers even after LRU eviction;
            // fall back to the shared streaming VAO for this draw.
            cache_on = 0;
            _t = MikuPan_PerfBegin();
            MikuPan_BindVAO(pipeline->vao);
            MikuPan_PerfEnd(PERF_SECT_SC_VAO, _t);
        }
    }
    else
    {
        /// Cache disabled — use the shared streaming VAO so the upload phase
        /// targets the per-frame DYNAMIC buffers (the A/B-comparison baseline).
        _t = MikuPan_PerfBegin();
        MikuPan_BindVAO(pipeline->vao);
        MikuPan_PerfEnd(PERF_SECT_SC_VAO, _t);
    }

    // ── Vertex source ──
    GLfloat *vertices = NULL;
    if (mesh_type == 0x32)
    {
        vertices = (GLfloat *)(pVUVNData->VUVNData_Preset.aui + (pVUVN->VUVNDesc.sNumNormal) * 3 + 10);
        memcpy(g_mesh_buffers_0x32.positions, vertices, (size_t)pVUVN->VUVNDesc.sNumVertex * 3 * sizeof(float));
    }
    else /* 0x12 / 0x10 */
    {
        vertices = (float *)&(((int *)pVUVN)[14]);
        if (!cache_on)
        {
            size_t byte_size = pVUVN->VUVNDesc.sNumVertex * pipeline->buffers[0].attributes[0].stride;
            MikuPan_StreamUploadFull(GL_ARRAY_BUFFER, pipeline->buffers[0].id, (GLsizeiptr)byte_size, vertices);
        }
    }

    // ── Build CPU staging buffers (UVs, colors, normals (SoA), indices) ──
    int vertex_offset = 0;
    int index_write_offset = 0;

    for (int i = 0; i < GET_NUM_MESH(pPUHead); i++)
    {
        pVMCD = (_SGDVUMESHCOLORDATA *) MikuPan_GetNextUnpackAddr((u_int *) pVMCD);
        int vertex_count = pVMCD->VifUnpack.NUM;

        if (pPUHead->VUMeshDesc.MeshType.TEX == 1)
        {
            MikuPan_FixUV((float*)&sgdMeshData->astData, vertex_count);
        }

        if (pPUHead->VUMeshDesc.MeshType.PRE == 1)
        {
            MikuPan_FixColors((float*)&pVMCD->avColor, vertex_count);
        }

        index_write_offset += MikuPan_SetTriangleIndex(g_mesh_buffers_0x32.indices, vertex_count, vertex_offset, index_write_offset);

        memcpy((char *)g_mesh_buffers_0x32.uvs + vertex_offset * uv_stride, sgdMeshData->astData, (size_t)vertex_count * uv_stride);
        memcpy((char *)g_mesh_buffers_0x32.colors + vertex_offset * color_stride, pVMCD->avColor, (size_t)vertex_count * color_stride);

        if (mesh_type == 0x32)
        {
            // Per-submesh normal: tight contiguous fill (compiler auto-vectorizes).
            const GLfloat *normals = (GLfloat *)(pVUVNData->VUVNData_Preset.aui + (i * 3) + 10);
            const float n0 = normals[0], n1 = normals[1], n2 = normals[2];
            float *ndst = g_mesh_buffers_0x32.normals + vertex_offset * 3;

            for (int j = 0; j < vertex_count; j++)
            {
                ndst[j * 3 + 0] = n0;
                ndst[j * 3 + 1] = n1;
                ndst[j * 3 + 2] = n2;
            }
        }

        vertex_offset += vertex_count;
        sgdMeshData = (SGDVUMESHSTDATA *) &sgdMeshData->astData[vertex_count];
        pVMCD = (_SGDVUMESHCOLORDATA *) &pVMCD->avColor[vertex_count];
    }

    const int N = vertex_offset;
    const int pos_stride = pipeline->buffers[0].attributes[0].stride;
    if (cache_on)
    {
        if (desired_pipeline == POSITION3_NORMAL3_UV2_SOA)
        {
            MikuPan_MeshCache_UploadVbo(cache_entry, 0, (long long)(N * pos_stride), g_mesh_buffers_0x32.positions);
            MikuPan_MeshCache_UploadVbo(cache_entry, 1, (long long)(N * pipeline->buffers[1].attributes[0].stride), g_mesh_buffers_0x32.normals);
            MikuPan_MeshCache_UploadVbo(cache_entry, 2, (long long)(N * pipeline->buffers[2].attributes[0].stride), g_mesh_buffers_0x32.uvs);
            MikuPan_MeshCache_StreamVbo(cache_entry, color_buf_idx, (long long)(N * color_stride), g_mesh_buffers_0x32.colors);
        }
        else
        {
            /// 0x12 / 0x10: pos+norm AoS in buffer 0, UVs in 1, colors in 2.
            MikuPan_MeshCache_UploadVbo(cache_entry, 0, (long long)(pVUVN->VUVNDesc.sNumVertex * pos_stride), vertices);
            MikuPan_MeshCache_UploadVbo(cache_entry, 1, (long long)(N * uv_stride),    g_mesh_buffers_0x32.uvs);
            MikuPan_MeshCache_StreamVbo(cache_entry, color_buf_idx, (long long)(N * color_stride), g_mesh_buffers_0x32.colors);
        }
        MikuPan_MeshCache_UploadIbo(cache_entry, (long long)(index_write_offset * (int)sizeof(u_int)), g_mesh_buffers_0x32.indices);

        cache_entry->index_count = index_write_offset;

        /// Bind the freshly-populated VAO. The bind-cache shadow was reset by
        /// MeshCache_Insert, so this issues a real glBindVertexArray.
        _t = MikuPan_PerfBegin();
        MikuPan_BindVAO(cache_entry->vao);
        MikuPan_PerfEnd(PERF_SECT_SC_VAO, _t);
    }
    else
    {
        if (desired_pipeline == POSITION3_NORMAL3_UV2_SOA)
        {
            MikuPan_StreamUploadFull(GL_ARRAY_BUFFER, pipeline->buffers[0].id, (GLsizeiptr)(N * pos_stride), g_mesh_buffers_0x32.positions);
            MikuPan_StreamUploadFull(GL_ARRAY_BUFFER, pipeline->buffers[1].id, (GLsizeiptr)(N * pipeline->buffers[1].attributes[0].stride), g_mesh_buffers_0x32.normals);
            MikuPan_StreamUploadFull(GL_ARRAY_BUFFER, pipeline->buffers[2].id, (GLsizeiptr)(N * pipeline->buffers[2].attributes[0].stride), g_mesh_buffers_0x32.uvs);
            MikuPan_StreamUploadFull(GL_ARRAY_BUFFER, pipeline->buffers[3].id, (GLsizeiptr)(N * pipeline->buffers[3].attributes[0].stride), g_mesh_buffers_0x32.colors);
        }
        else
        {
            MikuPan_StreamUploadFull(GL_ARRAY_BUFFER, pipeline->buffers[1].id, (GLsizeiptr)(N * uv_stride),    g_mesh_buffers_0x32.uvs);
            MikuPan_StreamUploadFull(GL_ARRAY_BUFFER, pipeline->buffers[2].id, (GLsizeiptr)(N * color_stride), g_mesh_buffers_0x32.colors);
        }
        MikuPan_StreamUploadFull(GL_ELEMENT_ARRAY_BUFFER, pipeline->ibo, (GLsizeiptr)(index_write_offset * (int)sizeof(u_int)), g_mesh_buffers_0x32.indices);
    }

    if (MikuPan_IsShadowPassActive())
    {
        MikuPan_ShadowDebugRecordCasterDraw(mesh_type, index_write_offset);
    }
    else if (MikuPan_IsShadowReceiverPassActive())
    {
        MikuPan_ShadowDebugRecordReceiverDraw(mesh_type, index_write_offset);
    }

    MikuPan_PerfDrawCall();
    MikuPan_TimedDrawElements(MikuPan_GetMeshRenderMode(), index_write_offset, GL_UNSIGNED_INT, (void *)0);

    if (!MikuPan_IsShadowPassActive() &&
        !MikuPan_IsShadowReceiverPassActive() &&
        MikuPan_IsNormalsRendering())
    {
        MikuPan_SetCurrentShaderProgram(NORMALS_0x12_SHADER);
        MikuPan_TimedDrawElements(GL_TRIANGLES, index_write_offset, GL_UNSIGNED_INT, (void *)0);
    }
}

/*
 * Draw a shadow caster from SetVUVNDataShadowModel's compact vec4 positions.
 * Regular mesh pipelines include normal/UV/color strides; using them here
 * skips vertices and produces unstable silhouettes.
 */
void MikuPan_RenderShadowSilhouettePrepared(unsigned int *pVUVN,
                                            unsigned int *pPUHead,
                                            const float *shadow_positions)
{
    MIKUPAN_PERF_SCOPE(PERF_SECT_MESH_RENDER);

    if (!MikuPan_IsShadowPassActive())
    {
        return;
    }

    SGDVUMESHPOINTNUM* pMeshInfo = (SGDVUMESHPOINTNUM*) &((unsigned int*) pPUHead)[6];
    VUVN_PRIM *v = ((VUVN_PRIM *) &((int *) pVUVN)[2]);

    const int num_mesh = (int) GET_NUM_MESH(pPUHead);
    const int vnum     = (int) v->vnum;
    const int mesh_type = (int) GET_MESH_TYPE(pPUHead);

    MikuPan_PipelineInfo *pipeline = MikuPan_GetPipelineInfo(SHADOW_POSITION4);
    const int pos_stride = pipeline->buffers[0].attributes[0].stride;

    /* Header sanity — bail (logged once) rather than trust a bad parse. */
    if (shadow_positions == NULL ||
        num_mesh <= 0 || num_mesh > 256 ||
        vnum <= 0 || vnum > MIKUPAN_MESH_BUFFER_CAPACITY)
    {
        static int logged = 0;
        if (!logged)
        {
            logged = 1;
            info_log("Shadow silhouette skipped: mtype=0x%x pos=%p num_mesh=%d vnum=%d",
                     mesh_type, (const void *)shadow_positions, num_mesh, vnum);
        }
        return;
    }

    /* Build positions-only triangle indices from each submesh's strip. */
    int vertex_offset = 0;
    int index_write_offset = 0;
    for (int i = 0; i < num_mesh; i++)
    {
        int vertex_count = (int) pMeshInfo[i].uiPointNum;
        if (vertex_count == 0) continue;

        if (vertex_count < 3 ||
            vertex_offset + vertex_count > vnum ||
            (long long) index_write_offset + (long long) (vertex_count - 2) * 3 >
                (long long) MIKUPAN_MESH_BUFFER_CAPACITY)
        {
            static int logged = 0;
            if (!logged)
            {
                logged = 1;
                info_log("Shadow silhouette skipped: mtype=0x%x submesh %d count=%d off=%d vnum=%d",
                         mesh_type, i, vertex_count, vertex_offset, vnum);
            }
            return;
        }

        index_write_offset += MikuPan_SetTriangleIndex(
            g_mesh_buffers_0x82.indices, vertex_count, vertex_offset, index_write_offset);
        vertex_offset += vertex_count;
    }

    /* One-time layout diagnostic: if vertex_offset (sum of submesh strip lengths)
     * is much less than vnum, the per-submesh counts / mesh count are being
     * under-read and most of the geometry is missing (thin-sliver silhouette).
     * v->nnum / vif_size / vtype tell us the true vertex format. */
    {
        static int diag_count = 0;
        if (diag_count < 6)
        {
            diag_count++;
            info_log("Shadow silhouette diag: mtype=0x%x num_mesh=%d vnum=%d nnum=%d vif_size=%d vtype=%d sum_counts=%d tris=%d first_counts=[%d,%d,%d,%d]",
                     mesh_type, num_mesh, vnum, (int) v->nnum,
                     (int) v->vif_size, (int) v->vtype,
                     vertex_offset, index_write_offset / 3,
                     num_mesh > 0 ? (int) pMeshInfo[0].uiPointNum : -1,
                     num_mesh > 1 ? (int) pMeshInfo[1].uiPointNum : -1,
                     num_mesh > 2 ? (int) pMeshInfo[2].uiPointNum : -1,
                     num_mesh > 3 ? (int) pMeshInfo[3].uiPointNum : -1);
        }
    }

    if (index_write_offset <= 0)
    {
        return;
    }

    /* The silhouette shader is applied via the shadow-pass shader override, so
     * this SetCurrentShaderProgram is redirected to SHADOW_SILHOUETTE_SHADER. */
    MikuPan_FlushTexturedSpriteBatch();
    MikuPan_SetCurrentShaderProgram(SHADOW_SILHOUETTE_SHADER);
    MikuPan_SetMeshRenderStateForCurrentPass();

    /* SetVUVNDataShadowModel already produced world/skinned-space positions. */
    MikuPan_SetWorldSpaceModelTransform();

    MikuPan_BindVAO(pipeline->vao);

    /* Upload positions (interleaved pos+normal; the silhouette shader reads only
     * location 0 = xyz) and the index list. UV/colour VAO attributes are left
     * untouched — they are never sampled by the silhouette shader. */
    MikuPan_StreamUploadFull(GL_ARRAY_BUFFER, pipeline->buffers[0].id,
        (GLsizeiptr) ((long long) vnum * pos_stride), shadow_positions);
    MikuPan_StreamUploadFull(GL_ELEMENT_ARRAY_BUFFER, pipeline->ibo,
        (GLsizeiptr) ((long long) index_write_offset * (int) sizeof(unsigned int)),
        g_mesh_buffers_0x82.indices);

    MikuPan_ShadowDebugRecordCasterDraw(mesh_type, index_write_offset);

    MikuPan_PerfDrawCall();
    MikuPan_TimedDrawElements(MikuPan_GetMeshRenderMode(),
                              index_write_offset, GL_UNSIGNED_INT, (void *) 0);
}

/*
 * 0x80 uses the same inline VUVN position/normal block as 0x82, but has no
 * ST/texture payload. For the shadow map we only need positions and topology,
 * so reuse avt2 and avoid the 0x82 UV parser entirely.
 */
void MikuPan_RenderShadowSilhouette0x80(unsigned int *pVUVN,
                                        unsigned int *pPUHead)
{
    MIKUPAN_PERF_SCOPE(PERF_SECT_MESH_RENDER);

    if (!MikuPan_IsShadowPassActive())
    {
        return;
    }

    SGDVUVNDATA_PRESET *pVUVNData = (SGDVUVNDATA_PRESET *) &(((SGDPROCUNITHEADER *) pVUVN)[1]);
    SGDVUMESHPOINTNUM* pMeshInfo = (SGDVUMESHPOINTNUM*) &((unsigned int*) pPUHead)[6];
    VUVN_PRIM *v = ((VUVN_PRIM *) &((int *) pVUVN)[2]);

    const int num_mesh = (int) GET_NUM_MESH(pPUHead);
    const int vnum = (int) v->vnum;
    const int mesh_type = (int) GET_MESH_TYPE(pPUHead);

    MikuPan_PipelineInfo *pipeline = MikuPan_GetPipelineInfo(POSITION3_NORMAL3_UV2);
    const int pos_stride = pipeline->buffers[0].attributes[0].stride;

    if (num_mesh <= 0 || num_mesh > 256 ||
        vnum <= 0 || vnum > MIKUPAN_MESH_BUFFER_CAPACITY)
    {
        static int logged = 0;
        if (!logged)
        {
            logged = 1;
            info_log("Shadow 0x80 silhouette skipped: mtype=0x%x num_mesh=%d vnum=%d",
                     mesh_type, num_mesh, vnum);
        }
        return;
    }

    /// Per-draw state (shader + render state) is outside the geometry cache and
    /// must run on every draw — set it before the cache fast path.
    MikuPan_FlushTexturedSpriteBatch();
    MikuPan_SetCurrentShaderProgram(SHADOW_SILHOUETTE_SHADER);
    MikuPan_SetMeshRenderStateForCurrentPass();

    /// ── Cache fast path ──
    /// 0x80 casters are static map geometry (fences, stairs); their positions
    /// (avt2) and strip topology never change for the life of the loaded SGD.
    /// Cache the position VBO + IBO + VAO once, then steady-state draws collapse
    /// to bind + glDrawElements — no index rebuild, no per-frame GPU upload.
    int cache_on = MikuPan_CanUseMeshCacheForCurrentPass();
    MikuPan_MeshCacheEntry *cache_entry = NULL;

    if (cache_on)
    {
        cache_entry = MikuPan_MeshCache_Lookup(pPUHead, MIKUPAN_MESHCACHE_KIND_CASTER);
        if (cache_entry != NULL && cache_entry->sgd_top == sgd_top_addr)
        {
            MikuPan_BindVAO(cache_entry->vao);
            MikuPan_ShadowDebugRecordCasterDraw(mesh_type, cache_entry->index_count);
            MikuPan_PerfDrawCall();
            MikuPan_TimedDrawElements(GL_TRIANGLES,
                                      cache_entry->index_count, GL_UNSIGNED_INT, (void *) 0);
            MikuPan_PerfMeshCacheHit();
            return;
        }

        MikuPan_PerfMeshCacheMissNew();
        cache_entry = MikuPan_MeshCache_Insert(pPUHead, sgd_top_addr,
                                               POSITION3_NORMAL3_UV2,
                                               MIKUPAN_MESHCACHE_KIND_CASTER);
        if (cache_entry == NULL)
        {
            cache_on = 0;
            MikuPan_BindVAO(pipeline->vao);
        }
    }
    else
    {
        MikuPan_BindVAO(pipeline->vao);
    }

    int vertex_offset = 0;
    int index_write_offset = 0;
    for (int i = 0; i < num_mesh; i++)
    {
        int vertex_count = (int) pMeshInfo[i].uiPointNum;
        if (vertex_count == 0) continue;

        if (vertex_count < 3 ||
            vertex_offset + vertex_count > vnum ||
            (long long) index_write_offset + (long long) (vertex_count - 2) * 3 >
                (long long) MIKUPAN_MESH_BUFFER_CAPACITY)
        {
            static int logged = 0;
            if (!logged)
            {
                logged = 1;
                info_log("Shadow 0x80 silhouette skipped: mtype=0x%x submesh %d count=%d off=%d vnum=%d",
                         mesh_type, i, vertex_count, vertex_offset, vnum);
            }
            return;
        }

        index_write_offset += MikuPan_SetTriangleIndex(
            g_mesh_buffers_0x82.indices, vertex_count, vertex_offset, index_write_offset);
        vertex_offset += vertex_count;
    }

    if (index_write_offset <= 0)
    {
        return;
    }

    if (cache_on)
    {
        MikuPan_MeshCache_UploadVbo(cache_entry, 0,
            (long long) ((long long) vnum * pos_stride), pVUVNData->avt2);
        MikuPan_MeshCache_UploadIbo(cache_entry,
            (long long) ((long long) index_write_offset * (int) sizeof(unsigned int)),
            g_mesh_buffers_0x82.indices);
        cache_entry->index_count = index_write_offset;
        MikuPan_BindVAO(cache_entry->vao);
    }
    else
    {
        MikuPan_StreamUploadFull(GL_ARRAY_BUFFER, pipeline->buffers[0].id,
            (GLsizeiptr) ((long long) vnum * pos_stride), pVUVNData->avt2);
        MikuPan_StreamUploadFull(GL_ELEMENT_ARRAY_BUFFER, pipeline->ibo,
            (GLsizeiptr) ((long long) index_write_offset * (int) sizeof(unsigned int)),
            g_mesh_buffers_0x82.indices);
    }

    MikuPan_ShadowDebugRecordCasterDraw(mesh_type, index_write_offset);

    MikuPan_PerfDrawCall();
    MikuPan_TimedDrawElements(GL_TRIANGLES,
                              index_write_offset, GL_UNSIGNED_INT, (void *) 0);
}

void MikuPan_RenderMeshType0x82(unsigned int *pVUVN, unsigned int *pPUHead)
{
    MIKUPAN_PERF_SCOPE(PERF_SECT_MESH_RENDER);

    if (!MikuPan_IsShadowPassActive() && !MikuPan_IsMesh0x82Rendering())
    {
        return;
    }

    if (MikuPan_ShouldSkipFullyTransparentMaterialMesh())
    {
        return;
    }

    MIKUPAN_PERF_SCOPE(PERF_SECT_MESH_0x82);

    SGDVUVNDATA_PRESET *pVUVNData = (SGDVUVNDATA_PRESET *) &(((SGDPROCUNITHEADER *) pVUVN)[1]);
    SGDVUMESHPOINTNUM *pMeshInfo = (SGDVUMESHPOINTNUM *) &(((SGDPROCUNITHEADER *) pPUHead)[4]);
    SGDPROCUNITDATA *pProcData = (SGDPROCUNITDATA *) &(((SGDPROCUNITHEADER *) pPUHead)[1]);
    SGDVUMESHSTREGSET* sgdVuMeshStRegSet = (SGDVUMESHSTREGSET *) &pMeshInfo[GET_NUM_MESH(pPUHead)];
    SGDVUMESHSTDATA* sgdMeshData = (SGDVUMESHSTDATA *) &sgdVuMeshStRegSet->auiVifCode[3];
    VUVN_PRIM *v = ((VUVN_PRIM *) &((int*)pVUVN)[2]);
    sceGsTex0 *mesh_tex_reg = (sceGsTex0 *) ((int64_t) pProcData + 0x18);

    MikuPan_PipelineInfo *pipeline = MikuPan_GetPipelineInfo(POSITION3_NORMAL3_UV2);

    // ── State setup ──
    MikuPan_FlushTexturedSpriteBatch();
    Uint64 _sc_t0 = SDL_GetPerformanceCounter();
    Uint64 _t = MikuPan_PerfBegin();
    MikuPan_SetCurrentShaderProgram(MESH_0x12_SHADER);
    MikuPan_PerfEnd(PERF_SECT_SC_SHADER, _t);

    if (!MikuPan_IsShadowReceiverPassActive())
    {
        _t = MikuPan_PerfBegin();
        MikuPan_BindDiffuseTextureForShadowCaster(mesh_tex_reg);
        MikuPan_PerfEnd(PERF_SECT_SC_TEXTURE, _t);
    }

    _t = MikuPan_PerfBegin();
    MikuPan_SetMeshRenderStateForCurrentPass();
    MikuPan_PerfEnd(PERF_SECT_SC_RS3D, _t);
    MikuPan_PerfEnd(PERF_SECT_STATE_CHANGE, _sc_t0);

    /// ── Cache fast path ──
    int cache_on   = MikuPan_CanUseMeshCacheForCurrentPass();
    const int cache_kind = MikuPan_CurrentPassCacheKind();
    MikuPan_MeshCacheEntry *cache_entry = NULL;

    if (cache_on)
    {
        cache_entry = MikuPan_MeshCache_Lookup(pPUHead, cache_kind);
        if (cache_entry != NULL && cache_entry->sgd_top == sgd_top_addr)
        {
            _t = MikuPan_PerfBegin();
            MikuPan_BindVAO(cache_entry->vao);
            MikuPan_PerfEnd(PERF_SECT_SC_VAO, _t);

            MikuPan_PerfDrawCall();
            MikuPan_TimedDrawElements(MikuPan_GetMeshRenderMode(),
                                      cache_entry->index_count, GL_UNSIGNED_INT, (void *)0);

            if (!MikuPan_IsShadowPassActive() &&
                !MikuPan_IsShadowReceiverPassActive() &&
                MikuPan_IsNormalsRendering())
            {
                MikuPan_SetCurrentShaderProgram(NORMALS_0x12_SHADER);
                MikuPan_TimedDrawElements(GL_TRIANGLES, cache_entry->index_count,
                                          GL_UNSIGNED_INT, (void *)0);
            }

            MikuPan_PerfMeshCacheHit();
            return;
        }

        MikuPan_PerfMeshCacheMissNew();
        cache_entry = MikuPan_MeshCache_Insert(pPUHead, sgd_top_addr, POSITION3_NORMAL3_UV2, cache_kind);
        if (cache_entry == NULL)
        {
            cache_on = 0;
            _t = MikuPan_PerfBegin();
            MikuPan_BindVAO(pipeline->vao);
            MikuPan_PerfEnd(PERF_SECT_SC_VAO, _t);
        }
    }
    else
    {
        _t = MikuPan_PerfBegin();
        MikuPan_BindVAO(pipeline->vao);
        MikuPan_PerfEnd(PERF_SECT_SC_VAO, _t);
    }

    // ── Build UV / color (zeros for NVL) / index staging ──
    int vertex_offset = 0;
    int index_write_offset = 0;
    for (int i = 0; i < GET_NUM_MESH(pPUHead); i++)
    {
        int vertex_count = pMeshInfo[i].uiPointNum;
        if (vertex_count == 0) continue;

        MikuPan_FixUV((float*)sgdMeshData->astData, vertex_count);
        index_write_offset += MikuPan_SetTriangleIndex(
            g_mesh_buffers_0x82.indices, vertex_count, vertex_offset, index_write_offset);

        memcpy((char *)g_mesh_buffers_0x82.uvs + vertex_offset * pipeline->buffers[1].attributes[0].stride,
               sgdMeshData->astData,
               (size_t)vertex_count * pipeline->buffers[1].attributes[0].stride);

        memcpy((char *)g_mesh_buffers_0x82.colors + vertex_offset * pipeline->buffers[2].attributes[0].stride,
               g_zero_floats,
               (size_t)vertex_count * pipeline->buffers[2].attributes[0].stride);

        sgdMeshData = (SGDVUMESHSTDATA*) &sgdMeshData->astData[vertex_count];
        vertex_offset += vertex_count;
    }

    // ── Upload pos+norm + UV + color + indices ──
    const int N = vertex_offset;
    if (cache_on)
    {
        MikuPan_MeshCache_UploadVbo(cache_entry, 0,
            (long long)(v->vnum * pipeline->buffers[0].attributes[0].stride),
            pVUVNData->avt2);
        MikuPan_MeshCache_UploadVbo(cache_entry, 1,
            (long long)(N * pipeline->buffers[1].attributes[0].stride),
            g_mesh_buffers_0x82.uvs);
        MikuPan_MeshCache_UploadVbo(cache_entry, 2,
            (long long)(N * pipeline->buffers[2].attributes[0].stride),
            g_mesh_buffers_0x82.colors);
        MikuPan_MeshCache_UploadIbo(cache_entry,
            (long long)(index_write_offset * (int)sizeof(u_int)),
            g_mesh_buffers_0x82.indices);

        cache_entry->index_count = index_write_offset;

        _t = MikuPan_PerfBegin();
        MikuPan_BindVAO(cache_entry->vao);
        MikuPan_PerfEnd(PERF_SECT_SC_VAO, _t);
    }
    else
    {
        MikuPan_StreamUploadFull(GL_ARRAY_BUFFER, pipeline->buffers[0].id,
            (GLsizeiptr)(v->vnum * pipeline->buffers[0].attributes[0].stride),
            pVUVNData->avt2);
        MikuPan_StreamUploadFull(GL_ARRAY_BUFFER, pipeline->buffers[1].id,
            (GLsizeiptr)(N * pipeline->buffers[1].attributes[0].stride),
            g_mesh_buffers_0x82.uvs);
        MikuPan_StreamUploadFull(GL_ARRAY_BUFFER, pipeline->buffers[2].id,
            (GLsizeiptr)(N * pipeline->buffers[2].attributes[0].stride),
            g_mesh_buffers_0x82.colors);
        MikuPan_StreamUploadFull(GL_ELEMENT_ARRAY_BUFFER, pipeline->ibo,
            (GLsizeiptr)(index_write_offset * (int)sizeof(u_int)),
            g_mesh_buffers_0x82.indices);
    }

    // ── Draw ──
    if (MikuPan_IsShadowPassActive())
    {
        MikuPan_ShadowDebugRecordCasterDraw(0x82, index_write_offset);
    }
    else if (MikuPan_IsShadowReceiverPassActive())
    {
        MikuPan_ShadowDebugRecordReceiverDraw(0x82, index_write_offset);
    }

    MikuPan_PerfDrawCall();
    MikuPan_TimedDrawElements(MikuPan_GetMeshRenderMode(),
                              index_write_offset, GL_UNSIGNED_INT, (void *)0);

    if (!MikuPan_IsShadowPassActive() &&
        !MikuPan_IsShadowReceiverPassActive() &&
        MikuPan_IsNormalsRendering())
    {
        MikuPan_SetCurrentShaderProgram(NORMALS_0x12_SHADER);
        MikuPan_TimedDrawElements(GL_TRIANGLES,
                                  index_write_offset, GL_UNSIGNED_INT, (void *)0);
    }
}

void MikuPan_RenderMeshType0x2(SGDPROCUNITHEADER *pVUVN, SGDPROCUNITHEADER *pPUHead, float* vertices)
{
    MIKUPAN_PERF_SCOPE(PERF_SECT_MESH_RENDER);

    // Visibility filter is skipped during the shadow pass so the silhouette
    // captures all of the caster regardless of UI debug toggles.
    if (!MikuPan_IsShadowPassActive() && !MikuPan_IsMesh0x2Rendering())
    {
        return;
    }

    char mesh_type = GET_MESH_TYPE(pPUHead);
    int shader;
    if (mesh_type == 0x2)      shader = MESH_0x2_SHADER;
    else if (mesh_type == 0xA) shader = MESH_0xA_SHADER;
    else return;

    /// GPU skinning: when SetVUVNDataPost handed this mesh's skin off to the
    /// GPU (non-shadow pass, vtype 2/3), the bone-blend runs in the vertex
    /// shader. We then upload the static bind-pose data once (instead of the
    /// per-frame transformed stream) and the bone palette per frame.
    const int is_skin = (MikuPan_SkinMode() != 0);
    if (is_skin)
    {
        shader = (mesh_type == 0x2) ? MESH_0x2_SKINNED_SHADER
                                    : MESH_0xA_SKINNED_SHADER;
    }
    const int pipeline_type = is_skin ? SKIN_POSITION4X2_NORMAL4X2_UV2
                                      : POSITION4_NORMAL4_UV2;

    if (MikuPan_ShouldSkipFullyTransparentMaterialMesh())
    {
        return;
    }

    int per_type_sect = (mesh_type == 0x2) ? PERF_SECT_MESH_0x2
                                           : PERF_SECT_MESH_0xA;
    MIKUPAN_PERF_SCOPE(per_type_sect);

    VUVN_PRIM *v = ((VUVN_PRIM *) &((int*)pVUVN)[2]);
    SGDVUMESHPOINTNUM *pMeshInfo = (SGDVUMESHPOINTNUM *) &pPUHead[4];
    SGDVUMESHSTREGSET *sgdVuMeshStRegSet = (SGDVUMESHSTREGSET *) &pMeshInfo[GET_NUM_MESH(pPUHead)];
    SGDVUMESHSTDATA *sgdMeshData = (SGDVUMESHSTDATA *) &sgdVuMeshStRegSet->auiVifCode[3];
    SGDPROCUNITDATA *pProcData = (SGDPROCUNITDATA *) &pPUHead[1];
    sceGsTex0 *mesh_tex_reg = (sceGsTex0 *) ((int64_t) pProcData + 0x18);

    MikuPan_PipelineInfo *pipeline =
        MikuPan_GetPipelineInfo((enum MikuPan_PipelineType)pipeline_type);

    // ── State setup (per-frame, always) ──
    MikuPan_FlushTexturedSpriteBatch();
    Uint64 _sc_t0 = SDL_GetPerformanceCounter();
    Uint64 _t = MikuPan_PerfBegin();
    MikuPan_SetCurrentShaderProgram(shader);
    MikuPan_PerfEnd(PERF_SECT_SC_SHADER, _t);

    if (!MikuPan_IsShadowReceiverPassActive())
    {
        _t = MikuPan_PerfBegin();
        MikuPan_BindDiffuseTextureForShadowCaster(mesh_tex_reg);
        MikuPan_PerfEnd(PERF_SECT_SC_TEXTURE, _t);
    }

    _t = MikuPan_PerfBegin();
    MikuPan_SetMeshRenderStateForCurrentPass();
    MikuPan_PerfEnd(PERF_SECT_SC_RS3D, _t);
    MikuPan_PerfEnd(PERF_SECT_STATE_CHANGE, _sc_t0);

    /// ── Partial cache ──
    /// 0x2/0xA meshes are CPU-skinned: positions+normals change every frame,
    /// but the triangle topology and UVs are immutable. Cache the static
    /// streams (UV VBO + IBO) and the VAO; stream only pos+norm.
    int cache_on   = MikuPan_CanUseMeshCacheForCurrentPass();
    const int cache_kind = MikuPan_CurrentPassCacheKind();
    MikuPan_MeshCacheEntry *cache_entry = NULL;
    int need_static_upload = 0;

    if (cache_on)
    {
        cache_entry = MikuPan_MeshCache_Lookup(pPUHead, cache_kind);
        if (cache_entry == NULL || cache_entry->sgd_top != sgd_top_addr ||
            cache_entry->pipeline_type != pipeline_type)
        {
            cache_entry = MikuPan_MeshCache_Insert(pPUHead, sgd_top_addr, pipeline_type, cache_kind);
            need_static_upload = 1;
            MikuPan_PerfMeshCacheMissNew();
            if (cache_entry == NULL)
            {
                cache_on = 0;
                need_static_upload = 0;
            }
        }
        else
        {
            MikuPan_PerfMeshCacheHit();
        }

        if (cache_on)
        {
            _t = MikuPan_PerfBegin();
            MikuPan_BindVAO(cache_entry->vao);
            MikuPan_PerfEnd(PERF_SECT_SC_VAO, _t);
        }
        else
        {
            _t = MikuPan_PerfBegin();
            MikuPan_BindVAO(pipeline->vao);
            MikuPan_PerfEnd(PERF_SECT_SC_VAO, _t);
        }
    }
    else
    {
        _t = MikuPan_PerfBegin();
        MikuPan_BindVAO(pipeline->vao);
        MikuPan_PerfEnd(PERF_SECT_SC_VAO, _t);
    }

    if (is_skin)
    {
        // ── Static bind-pose data (GPU-skinned) ──
        // Buffer 0 holds the immutable per-vertex bind data (m0/m1/n0/n1); the
        // VS blends it with the bone palette. Uploaded once on a cache miss, so
        // the steady state streams nothing per frame.
        const long long bind_bytes =
            (long long)MikuPan_SkinVertexCount() *
            pipeline->buffers[0].attributes[0].stride;
        if (cache_on)
        {
            if (need_static_upload)
            {
                MikuPan_MeshCache_UploadVbo(cache_entry, 0, bind_bytes,
                                            MikuPan_SkinBindData());
            }
        }
        else
        {
            MikuPan_StreamUploadFull(GL_ARRAY_BUFFER, pipeline->buffers[0].id,
                (GLsizeiptr)bind_bytes, MikuPan_SkinBindData());
        }
    }
    else
    {
        // ── Pos+norm AoS streamed every frame (CPU-skinned) ──
        if (cache_on)
        {
            MikuPan_MeshCache_StreamVbo(cache_entry, 0,
                (long long)(v->vnum * pipeline->buffers[0].attributes[0].stride),
                vertices);
        }
        else
        {
            MikuPan_StreamUploadFull(GL_ARRAY_BUFFER, pipeline->buffers[0].id,
                (GLsizeiptr)(v->vnum * pipeline->buffers[0].attributes[0].stride),
                vertices);
        }
    }

    /// ── Static streams (UV + indices) — only built on cache miss, OR every
    /// frame when the cache is disabled. With caching on, the steady-state
    /// hot path skips the entire per-submesh loop, the UV memcpys, and the
    /// FixUV / SetTriangleIndex calls.
    int index_count;
    if (need_static_upload || !cache_on)
    {
        int vertex_offset = 0;
        int index_write_offset = 0;
        for (int i = 0; i < GET_NUM_MESH(pPUHead); i++)
        {
            int vertex_count = pMeshInfo[i].uiPointNum;

            if (pPUHead->VUMeshDesc.MeshType.TEX == 1)
            {
                MikuPan_FixUV((float*)&sgdMeshData->astData, vertex_count);
            }

            index_write_offset += MikuPan_SetTriangleIndex(
                g_mesh_buffers_0x2.indices, vertex_count, vertex_offset, index_write_offset);

            memcpy((char *)g_mesh_buffers_0x2.uvs + vertex_offset * pipeline->buffers[1].attributes[0].stride,
                   sgdMeshData->astData,
                   (size_t)vertex_count * pipeline->buffers[1].attributes[0].stride);

            sgdMeshData = (SGDVUMESHSTDATA *) &sgdMeshData->astData[vertex_count];
            vertex_offset += vertex_count;
        }

        const int N = vertex_offset;
        if (cache_on)
        {
            MikuPan_MeshCache_UploadVbo(cache_entry, 1,
                (long long)(N * pipeline->buffers[1].attributes[0].stride),
                g_mesh_buffers_0x2.uvs);
            MikuPan_MeshCache_UploadIbo(cache_entry,
                (long long)(index_write_offset * (int)sizeof(u_int)),
                g_mesh_buffers_0x2.indices);
            cache_entry->index_count = index_write_offset;

            /// MeshCache_Insert unbound the VAO and reset the bind cache, so
            /// rebind to capture the streamed pos+norm + freshly-uploaded UV
            /// bindings into the next draw.
            MikuPan_BindVAO(cache_entry->vao);
        }
        else
        {
            MikuPan_StreamUploadFull(GL_ARRAY_BUFFER, pipeline->buffers[1].id,
                (GLsizeiptr)(N * pipeline->buffers[1].attributes[0].stride),
                g_mesh_buffers_0x2.uvs);
            MikuPan_StreamUploadFull(GL_ELEMENT_ARRAY_BUFFER, pipeline->ibo,
                (GLsizeiptr)(index_write_offset * (int)sizeof(u_int)),
                g_mesh_buffers_0x2.indices);
        }

        index_count = index_write_offset;
    }
    else
    {
        index_count = cache_entry->index_count;
    }

    // ── Draw ──
    if (MikuPan_IsShadowPassActive())
    {
        MikuPan_ShadowDebugRecordCasterDraw(mesh_type, index_count);
    }
    else if (MikuPan_IsShadowReceiverPassActive())
    {
        MikuPan_ShadowDebugRecordReceiverDraw(mesh_type, index_count);
    }

    if (is_skin)
    {
        /* Hash-guarded palette upload + bind into the vertex storage slot for
         * this draw (the bind is consumed inside BindDrawState). */
        MikuPan_BindSkinPalette();
    }

    MikuPan_PerfDrawCall();
    MikuPan_TimedDrawElements(MikuPan_GetMeshRenderMode(),
                              index_count, GL_UNSIGNED_INT, (void *)0);

    if (is_skin)
    {
        /* Clear the binding so the next non-skinned draw (which uses a pipeline
         * declaring zero storage buffers) never carries the palette. */
        MikuPan_GPUSetVertexStorageBuffer(0);
    }

    /* The normals overlay shader reads the legacy non-skinned vertex format, so
     * it is only valid on the CPU-skinned path. */
    if (!is_skin &&
        !MikuPan_IsShadowPassActive() &&
        !MikuPan_IsShadowReceiverPassActive() &&
        MikuPan_IsNormalsRendering())
    {
        MikuPan_SetCurrentShaderProgram(NORMALS_0x2_SHADER);
        MikuPan_TimedDrawElements(GL_TRIANGLES,
                                  index_count, GL_UNSIGNED_INT, (void *)0);
    }
}
