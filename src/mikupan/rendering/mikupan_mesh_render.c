#include "mikupan_renderer_internal.h"
#include "graphics/graph3d/sglib.h"
#include "graphics/graph3d/sgsu.h"
#include "mikupan/mikupan_utils.h"
#include "mikupan/ui/mikupan_ui.h"
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
static float g_zero_floats[MIKUPAN_MESH_BUFFER_CAPACITY] = {0};

static int MikuPan_GetMeshRenderMode()
{
    return MikuPan_IsWireframeRendering() ? GL_LINES : GL_TRIANGLES;
}

static int MikuPan_BuildPresetColorStream(SGDPROCUNITHEADER *pPUHead,
                                          SGDPROCUNITDATA *pProcData,
                                          MikuPan_PipelineInfo *pipeline,
                                          int color_buf_idx)
{
    _SGDVUMESHCOLORDATA *pVMCD =
        (_SGDVUMESHCOLORDATA *)(&pPUHead->pNext +
                                pProcData->VUMeshData_Preset.sOffsetToPrim);
    const int color_stride = pipeline->buffers[color_buf_idx].attributes[0].stride;
    int vertex_offset = 0;

    for (int i = 0; i < GET_NUM_MESH(pPUHead); i++)
    {
        pVMCD = (_SGDVUMESHCOLORDATA *)MikuPan_GetNextUnpackAddr((u_int *)pVMCD);
        int vertex_count = pVMCD->VifUnpack.NUM;

        if (pPUHead->VUMeshDesc.MeshType.PRE == 1)
        {
            MikuPan_FixColors((float *)&pVMCD->avColor, vertex_count);
        }

        memcpy((char *)g_mesh_buffers_0x32.colors + vertex_offset * color_stride,
               pVMCD->avColor,
               (size_t)vertex_count * color_stride);

        vertex_offset += vertex_count;
        pVMCD = (_SGDVUMESHCOLORDATA *)&pVMCD->avColor[vertex_count];
    }

    return vertex_offset;
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

    if ((int64_t)mesh_tex_reg < (int64_t)pVMCD)
    {
        _t = MikuPan_PerfBegin();
        MikuPan_SetTexture(mesh_tex_reg);
        MikuPan_PerfEnd(PERF_SECT_SC_TEXTURE, _t);
    }

    _t = MikuPan_PerfBegin();
    MikuPan_SetRenderState3D();
    MikuPan_PerfEnd(PERF_SECT_SC_RS3D, _t);
    MikuPan_PerfEnd(PERF_SECT_STATE_CHANGE, _sc_t0);

    /// ── Cache fast path ──
    /// Static furniture / map geometry: positions, UVs, normals, and indices
    /// can be cached, but preset vertex colours are rewritten by lighting/BW
    /// paths. Refresh only that colour VBO on cache hits.
    const int cache_on = MikuPan_MeshCache_IsEnabled();
    MikuPan_MeshCacheEntry *cache_entry = NULL;

    if (cache_on)
    {
        cache_entry = MikuPan_MeshCache_Lookup(pPUHead);
        if (cache_entry != NULL && cache_entry->sgd_top == sgd_top_addr)
        {
            int color_count = MikuPan_BuildPresetColorStream(pPUHead, pProcData,
                                                             pipeline, color_buf_idx);
            MikuPan_MeshCache_StreamVbo(cache_entry, color_buf_idx,
                                        (long)(color_count * color_stride),
                                        g_mesh_buffers_0x32.colors);

            _t = MikuPan_PerfBegin();
            MikuPan_BindVAO(cache_entry->vao);
            MikuPan_PerfEnd(PERF_SECT_SC_VAO, _t);

            MikuPan_PerfDrawCall();
            MikuPan_TimedDrawElements(MikuPan_GetMeshRenderMode(),
                                      cache_entry->index_count, GL_UNSIGNED_INT, (void *)0);

            if (MikuPan_IsNormalsRendering())
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
        cache_entry = MikuPan_MeshCache_Insert(pPUHead, sgd_top_addr, desired_pipeline);
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
            MikuPan_MeshCache_UploadVbo(cache_entry, 0, (long)(N * pos_stride), g_mesh_buffers_0x32.positions);
            MikuPan_MeshCache_UploadVbo(cache_entry, 1, (long)(N * pipeline->buffers[1].attributes[0].stride), g_mesh_buffers_0x32.normals);
            MikuPan_MeshCache_UploadVbo(cache_entry, 2, (long)(N * pipeline->buffers[2].attributes[0].stride), g_mesh_buffers_0x32.uvs);
            MikuPan_MeshCache_StreamVbo(cache_entry, color_buf_idx, (long)(N * color_stride), g_mesh_buffers_0x32.colors);
        }
        else
        {
            /// 0x12 / 0x10: pos+norm AoS in buffer 0, UVs in 1, colors in 2.
            MikuPan_MeshCache_UploadVbo(cache_entry, 0, (long)(pVUVN->VUVNDesc.sNumVertex * pos_stride), vertices);
            MikuPan_MeshCache_UploadVbo(cache_entry, 1, (long)(N * uv_stride),    g_mesh_buffers_0x32.uvs);
            MikuPan_MeshCache_StreamVbo(cache_entry, color_buf_idx, (long)(N * color_stride), g_mesh_buffers_0x32.colors);
        }
        MikuPan_MeshCache_UploadIbo(cache_entry, (long)(index_write_offset * (int)sizeof(u_int)), g_mesh_buffers_0x32.indices);

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

    MikuPan_PerfDrawCall();
    MikuPan_TimedDrawElements(MikuPan_GetMeshRenderMode(), index_write_offset, GL_UNSIGNED_INT, (void *)0);

    if (MikuPan_IsNormalsRendering())
    {
        MikuPan_SetCurrentShaderProgram(NORMALS_0x12_SHADER);
        MikuPan_TimedDrawElements(GL_TRIANGLES, index_write_offset, GL_UNSIGNED_INT, (void *)0);
    }
}

void MikuPan_RenderMeshType0x82(unsigned int *pVUVN, unsigned int *pPUHead)
{
    MIKUPAN_PERF_SCOPE(PERF_SECT_MESH_RENDER);

    if (!MikuPan_IsShadowPassActive() && !MikuPan_IsMesh0x82Rendering())
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

    _t = MikuPan_PerfBegin();
    MikuPan_SetTexture(mesh_tex_reg);
    MikuPan_PerfEnd(PERF_SECT_SC_TEXTURE, _t);

    _t = MikuPan_PerfBegin();
    MikuPan_SetRenderState3D();
    MikuPan_PerfEnd(PERF_SECT_SC_RS3D, _t);
    MikuPan_PerfEnd(PERF_SECT_STATE_CHANGE, _sc_t0);

    /// ── Cache fast path ──
    const int cache_on = MikuPan_MeshCache_IsEnabled();
    MikuPan_MeshCacheEntry *cache_entry = NULL;

    if (cache_on)
    {
        cache_entry = MikuPan_MeshCache_Lookup(pPUHead);
        if (cache_entry != NULL && cache_entry->sgd_top == sgd_top_addr)
        {
            _t = MikuPan_PerfBegin();
            MikuPan_BindVAO(cache_entry->vao);
            MikuPan_PerfEnd(PERF_SECT_SC_VAO, _t);

            MikuPan_PerfDrawCall();
            MikuPan_TimedDrawElements(MikuPan_GetMeshRenderMode(),
                                      cache_entry->index_count, GL_UNSIGNED_INT, (void *)0);

            if (MikuPan_IsNormalsRendering())
            {
                MikuPan_SetCurrentShaderProgram(NORMALS_0x12_SHADER);
                MikuPan_TimedDrawElements(GL_TRIANGLES, cache_entry->index_count,
                                          GL_UNSIGNED_INT, (void *)0);
            }

            MikuPan_PerfMeshCacheHit();
            return;
        }

        MikuPan_PerfMeshCacheMissNew();
        cache_entry = MikuPan_MeshCache_Insert(pPUHead, sgd_top_addr, POSITION3_NORMAL3_UV2);
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
            (long)(v->vnum * pipeline->buffers[0].attributes[0].stride),
            pVUVNData->avt2);
        MikuPan_MeshCache_UploadVbo(cache_entry, 1,
            (long)(N * pipeline->buffers[1].attributes[0].stride),
            g_mesh_buffers_0x82.uvs);
        MikuPan_MeshCache_UploadVbo(cache_entry, 2,
            (long)(N * pipeline->buffers[2].attributes[0].stride),
            g_mesh_buffers_0x82.colors);
        MikuPan_MeshCache_UploadIbo(cache_entry,
            (long)(index_write_offset * (int)sizeof(u_int)),
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
    MikuPan_PerfDrawCall();
    MikuPan_TimedDrawElements(MikuPan_GetMeshRenderMode(),
                              index_write_offset, GL_UNSIGNED_INT, (void *)0);

    if (MikuPan_IsNormalsRendering())
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

    int per_type_sect = (mesh_type == 0x2) ? PERF_SECT_MESH_0x2
                                           : PERF_SECT_MESH_0xA;
    MIKUPAN_PERF_SCOPE(per_type_sect);

    VUVN_PRIM *v = ((VUVN_PRIM *) &((int*)pVUVN)[2]);
    SGDVUMESHPOINTNUM *pMeshInfo = (SGDVUMESHPOINTNUM *) &pPUHead[4];
    SGDVUMESHSTREGSET *sgdVuMeshStRegSet = (SGDVUMESHSTREGSET *) &pMeshInfo[GET_NUM_MESH(pPUHead)];
    SGDVUMESHSTDATA *sgdMeshData = (SGDVUMESHSTDATA *) &sgdVuMeshStRegSet->auiVifCode[3];
    SGDPROCUNITDATA *pProcData = (SGDPROCUNITDATA *) &pPUHead[1];
    sceGsTex0 *mesh_tex_reg = (sceGsTex0 *) ((int64_t) pProcData + 0x18);

    MikuPan_PipelineInfo *pipeline = MikuPan_GetPipelineInfo(POSITION4_NORMAL4_UV2);

    // ── State setup (per-frame, always) ──
    MikuPan_FlushTexturedSpriteBatch();
    Uint64 _sc_t0 = SDL_GetPerformanceCounter();
    Uint64 _t = MikuPan_PerfBegin();
    MikuPan_SetCurrentShaderProgram(shader);
    MikuPan_PerfEnd(PERF_SECT_SC_SHADER, _t);

    _t = MikuPan_PerfBegin();
    MikuPan_SetTexture(mesh_tex_reg);
    MikuPan_PerfEnd(PERF_SECT_SC_TEXTURE, _t);

    _t = MikuPan_PerfBegin();
    MikuPan_SetRenderState3D();
    MikuPan_PerfEnd(PERF_SECT_SC_RS3D, _t);
    MikuPan_PerfEnd(PERF_SECT_STATE_CHANGE, _sc_t0);

    /// ── Partial cache ──
    /// 0x2/0xA meshes are CPU-skinned: positions+normals change every frame,
    /// but the triangle topology and UVs are immutable. Cache the static
    /// streams (UV VBO + IBO) and the VAO; stream only pos+norm.
    const int cache_on = MikuPan_MeshCache_IsEnabled();
    MikuPan_MeshCacheEntry *cache_entry = NULL;
    int need_static_upload = 0;

    if (cache_on)
    {
        cache_entry = MikuPan_MeshCache_Lookup(pPUHead);
        if (cache_entry == NULL || cache_entry->sgd_top != sgd_top_addr)
        {
            cache_entry = MikuPan_MeshCache_Insert(pPUHead, sgd_top_addr, POSITION4_NORMAL4_UV2);
            need_static_upload = 1;
            MikuPan_PerfMeshCacheMissNew();
        }
        else
        {
            MikuPan_PerfMeshCacheHit();
        }

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

    // ── Pos+norm AoS streamed every frame (CPU-skinned) ──
    if (cache_on)
    {
        MikuPan_MeshCache_StreamVbo(cache_entry, 0,
            (long)(v->vnum * pipeline->buffers[0].attributes[0].stride),
            vertices);
    }
    else
    {
        MikuPan_StreamUploadFull(GL_ARRAY_BUFFER, pipeline->buffers[0].id,
            (GLsizeiptr)(v->vnum * pipeline->buffers[0].attributes[0].stride),
            vertices);
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
                (long)(N * pipeline->buffers[1].attributes[0].stride),
                g_mesh_buffers_0x2.uvs);
            MikuPan_MeshCache_UploadIbo(cache_entry,
                (long)(index_write_offset * (int)sizeof(u_int)),
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
    MikuPan_PerfDrawCall();
    MikuPan_TimedDrawElements(MikuPan_GetMeshRenderMode(),
                              index_count, GL_UNSIGNED_INT, (void *)0);

    if (MikuPan_IsNormalsRendering())
    {
        MikuPan_SetCurrentShaderProgram(NORMALS_0x2_SHADER);
        MikuPan_TimedDrawElements(GL_TRIANGLES,
                                  index_count, GL_UNSIGNED_INT, (void *)0);
    }
}
