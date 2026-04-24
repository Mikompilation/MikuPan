#include "mikupan_pipeline.h"
#include "mikupan_shader.h"
#include "mikupan/mikupan_logging_c.h"

#include <stdlib.h>

// ─────────────────────────────────────────────────────────────────────────────
// Global state definitions
// ─────────────────────────────────────────────────────────────────────────────

SDL_GPUSampler      *g_gpu_sampler  = NULL;
SDL_GPUBuffer       *g_index_buffer = NULL;
MikuPan_PipelineInfo g_pipelines[MAX_NUMBER_OF_PIPELINES] = {0};

static SDL_GPUGraphicsPipeline *s_gpu_pipelines[MAX_GPU_PIPELINES] = {0};

// ─────────────────────────────────────────────────────────────────────────────
// Static vertex input layout descriptors
// (arrays are passed by pointer to SDL_CreateGPUGraphicsPipeline)
// ─────────────────────────────────────────────────────────────────────────────

// MESH_0x12 : buf0 = pos3(12)+norm3(12) stride 24 | buf1 = uv2 stride 8
static const SDL_GPUVertexBufferDescription s_vb_mesh12[2] = {
    { .slot = 0, .pitch = 24, .input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX, .instance_step_rate = 0 },
    { .slot = 1, .pitch = 8,  .input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX, .instance_step_rate = 0 },
};
static const SDL_GPUVertexAttribute s_va_mesh12[3] = {
    { .location = 0, .buffer_slot = 0, .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3, .offset = 0  },
    { .location = 1, .buffer_slot = 0, .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3, .offset = 12 },
    { .location = 2, .buffer_slot = 1, .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2, .offset = 0  },
};

// MESH_0x2 / 0xA : buf0 = pos4(16)+norm4(16) stride 32 | buf1 = uv2 stride 8
static const SDL_GPUVertexBufferDescription s_vb_mesh02[2] = {
    { .slot = 0, .pitch = 32, .input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX, .instance_step_rate = 0 },
    { .slot = 1, .pitch = 8,  .input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX, .instance_step_rate = 0 },
};
static const SDL_GPUVertexAttribute s_va_mesh02[3] = {
    { .location = 0, .buffer_slot = 0, .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4, .offset = 0  },
    { .location = 1, .buffer_slot = 0, .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4, .offset = 16 },
    { .location = 2, .buffer_slot = 1, .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2, .offset = 0  },
};

// SPRITE : buf0 = uv4(16)+colour4(16)+pos4(16) stride 48
static const SDL_GPUVertexBufferDescription s_vb_sprite[1] = {
    { .slot = 0, .pitch = 48, .input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX, .instance_step_rate = 0 },
};
static const SDL_GPUVertexAttribute s_va_sprite[3] = {
    { .location = 0, .buffer_slot = 0, .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4, .offset = 0  },
    { .location = 1, .buffer_slot = 0, .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4, .offset = 16 },
    { .location = 2, .buffer_slot = 0, .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4, .offset = 32 },
};

// COLOUR4_POSITION4 : buf0 = colour4(16)+pos4(16) stride 32
static const SDL_GPUVertexBufferDescription s_vb_unlit[1] = {
    { .slot = 0, .pitch = 32, .input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX, .instance_step_rate = 0 },
};
static const SDL_GPUVertexAttribute s_va_unlit[2] = {
    { .location = 0, .buffer_slot = 0, .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4, .offset = 0  },
    { .location = 1, .buffer_slot = 0, .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4, .offset = 16 },
};

// POSITION4 (bounding box) : buf0 = pos4 stride 16
static const SDL_GPUVertexBufferDescription s_vb_bbox[1] = {
    { .slot = 0, .pitch = 16, .input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX, .instance_step_rate = 0 },
};
static const SDL_GPUVertexAttribute s_va_bbox[1] = {
    { .location = 0, .buffer_slot = 0, .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4, .offset = 0 },
};

// ─────────────────────────────────────────────────────────────────────────────
// Pipeline creation helper
// ─────────────────────────────────────────────────────────────────────────────

typedef struct
{
    SDL_GPUShader                        *vert;
    SDL_GPUShader                        *frag;
    const SDL_GPUVertexBufferDescription *vb;
    Uint32                                num_vb;
    const SDL_GPUVertexAttribute         *va;
    Uint32                                num_va;
    SDL_GPUPrimitiveType                  prim;
    SDL_GPUFillMode                       fill;
    SDL_GPUCullMode                       cull;
    bool                                  depth_test;
    bool                                  depth_write;
    bool                                  blend;
    SDL_GPUTextureFormat                  color_fmt;
    SDL_GPUSampleCount                    samples;
} PipelineDesc;

static SDL_GPUGraphicsPipeline *make_pipeline(SDL_GPUDevice *device,
                                              const PipelineDesc *d)
{
    SDL_GPUColorTargetBlendState blend_state = {0};
    if (d->blend)
    {
        blend_state.enable_blend          = true;
        blend_state.src_color_blendfactor = SDL_GPU_BLENDFACTOR_SRC_ALPHA;
        blend_state.dst_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
        blend_state.color_blend_op        = SDL_GPU_BLENDOP_ADD;
        blend_state.src_alpha_blendfactor = SDL_GPU_BLENDFACTOR_SRC_ALPHA;
        blend_state.dst_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
        blend_state.alpha_blend_op        = SDL_GPU_BLENDOP_ADD;
    }

    SDL_GPUColorTargetDescription ctd = {
        .format      = d->color_fmt,
        .blend_state = blend_state,
    };

    SDL_GPUGraphicsPipelineCreateInfo ci = {
        .vertex_shader   = d->vert,
        .fragment_shader = d->frag,
        .vertex_input_state = {
            .vertex_buffer_descriptions = d->vb,
            .num_vertex_buffers         = d->num_vb,
            .vertex_attributes          = d->va,
            .num_vertex_attributes      = d->num_va,
        },
        .primitive_type = d->prim,
        .rasterizer_state = {
            .fill_mode  = d->fill,
            .cull_mode  = d->cull,
            .front_face = SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE,
        },
        .multisample_state = {
            .sample_count = d->samples,
            .sample_mask  = 0xFFFFFFFF,
        },
        .depth_stencil_state = {
            .enable_depth_test  = d->depth_test,
            .enable_depth_write = d->depth_write,
            .compare_op         = SDL_GPU_COMPAREOP_LESS_OR_EQUAL,
        },
        .target_info = {
            .color_target_descriptions = &ctd,
            .num_color_targets         = 1,
            .depth_stencil_format      = SDL_GPU_TEXTUREFORMAT_D32_FLOAT,
            .has_depth_stencil_target  = true,
        },
    };

    SDL_GPUGraphicsPipeline *p = SDL_CreateGPUGraphicsPipeline(device, &ci);
    if (!p)
    {
        info_log("SDL_GPU: pipeline creation failed: %s", SDL_GetError());
    }
    return p;
}

// ─────────────────────────────────────────────────────────────────────────────
// Build all SDL_GPUGraphicsPipeline objects
// Called from MikuPan_InitPipelines and MikuPan_RecreatePipelines
// ─────────────────────────────────────────────────────────────────────────────

static int create_pipelines_internal(SDL_GPUDevice       *device,
                                     SDL_GPUTextureFormat  color_fmt,
                                     SDL_GPUSampleCount    samples)
{
    // Vertex shaders
    SDL_GPUShader *v12  = g_vertex_shaders[MESH_0x12_SHADER];
    SDL_GPUShader *v02  = g_vertex_shaders[MESH_0x2_SHADER];
    SDL_GPUShader *v0A  = g_vertex_shaders[MESH_0xA_SHADER];
    SDL_GPUShader *vSpr = g_vertex_shaders[SPRITE_SHADER];
    SDL_GPUShader *vUnt = g_vertex_shaders[UNTEXTURED_COLOURED_SPRITE_SHADER];
    SDL_GPUShader *vBB  = g_vertex_shaders[BOUNDING_BOX_SHADER];

    // Fragment shaders
    SDL_GPUShader *fLit = g_fragment_shaders[MESH_0x12_SHADER];
    SDL_GPUShader *fSpr = g_fragment_shaders[SPRITE_SHADER];
    SDL_GPUShader *fUnt = g_fragment_shaders[UNTEXTURED_COLOURED_SPRITE_SHADER];

#define MK(V,F,VB,NVB,VA,NVA,PRIM,FILL,CULL,DT,DW,BL) \
    make_pipeline(device, &(PipelineDesc){ \
        .vert=(V), .frag=(F), \
        .vb=(VB), .num_vb=(NVB), .va=(VA), .num_va=(NVA), \
        .prim=(PRIM), .fill=(FILL), .cull=(CULL), \
        .depth_test=(DT), .depth_write=(DW), .blend=(BL), \
        .color_fmt=color_fmt, .samples=samples })

    // ── Mesh 0x12 ──────────────────────────────────────────────────────────
    s_gpu_pipelines[GPU_PIPELINE_MESH_0x12_SOLID] = MK(
        v12, fLit, s_vb_mesh12, 2, s_va_mesh12, 3,
        SDL_GPU_PRIMITIVETYPE_TRIANGLESTRIP, SDL_GPU_FILLMODE_FILL,
        SDL_GPU_CULLMODE_BACK, true, true, false);

    s_gpu_pipelines[GPU_PIPELINE_MESH_0x12_WIREFRAME] = MK(
        v12, fLit, s_vb_mesh12, 2, s_va_mesh12, 3,
        SDL_GPU_PRIMITIVETYPE_LINESTRIP, SDL_GPU_FILLMODE_FILL,
        SDL_GPU_CULLMODE_NONE, true, false, false);

    s_gpu_pipelines[GPU_PIPELINE_MESH_0x12_MIRROR] = MK(
        v12, fLit, s_vb_mesh12, 2, s_va_mesh12, 3,
        SDL_GPU_PRIMITIVETYPE_TRIANGLESTRIP, SDL_GPU_FILLMODE_FILL,
        SDL_GPU_CULLMODE_FRONT, true, true, false);

    // ── Mesh 0x2 ───────────────────────────────────────────────────────────
    s_gpu_pipelines[GPU_PIPELINE_MESH_0x2_SOLID] = MK(
        v02, fLit, s_vb_mesh02, 2, s_va_mesh02, 3,
        SDL_GPU_PRIMITIVETYPE_TRIANGLESTRIP, SDL_GPU_FILLMODE_FILL,
        SDL_GPU_CULLMODE_BACK, true, true, false);

    s_gpu_pipelines[GPU_PIPELINE_MESH_0x2_WIREFRAME] = MK(
        v02, fLit, s_vb_mesh02, 2, s_va_mesh02, 3,
        SDL_GPU_PRIMITIVETYPE_LINESTRIP, SDL_GPU_FILLMODE_FILL,
        SDL_GPU_CULLMODE_NONE, true, false, false);

    s_gpu_pipelines[GPU_PIPELINE_MESH_0x2_MIRROR] = MK(
        v02, fLit, s_vb_mesh02, 2, s_va_mesh02, 3,
        SDL_GPU_PRIMITIVETYPE_TRIANGLESTRIP, SDL_GPU_FILLMODE_FILL,
        SDL_GPU_CULLMODE_FRONT, true, true, false);

    // ── Mesh 0xA (same layout as 0x2; pre-transformed verts, no model mtx) ─
    s_gpu_pipelines[GPU_PIPELINE_MESH_0xA_SOLID] = MK(
        v0A, fLit, s_vb_mesh02, 2, s_va_mesh02, 3,
        SDL_GPU_PRIMITIVETYPE_TRIANGLESTRIP, SDL_GPU_FILLMODE_FILL,
        SDL_GPU_CULLMODE_BACK, true, true, false);

    s_gpu_pipelines[GPU_PIPELINE_MESH_0xA_WIREFRAME] = MK(
        v0A, fLit, s_vb_mesh02, 2, s_va_mesh02, 3,
        SDL_GPU_PRIMITIVETYPE_LINESTRIP, SDL_GPU_FILLMODE_FILL,
        SDL_GPU_CULLMODE_NONE, true, false, false);

    s_gpu_pipelines[GPU_PIPELINE_MESH_0xA_MIRROR] = MK(
        v0A, fLit, s_vb_mesh02, 2, s_va_mesh02, 3,
        SDL_GPU_PRIMITIVETYPE_TRIANGLESTRIP, SDL_GPU_FILLMODE_FILL,
        SDL_GPU_CULLMODE_FRONT, true, true, false);

    // ── Sprites ────────────────────────────────────────────────────────────
    s_gpu_pipelines[GPU_PIPELINE_SPRITE_2D] = MK(
        vSpr, fSpr, s_vb_sprite, 1, s_va_sprite, 3,
        SDL_GPU_PRIMITIVETYPE_TRIANGLESTRIP, SDL_GPU_FILLMODE_FILL,
        SDL_GPU_CULLMODE_NONE, false, false, true);

    s_gpu_pipelines[GPU_PIPELINE_SPRITE_2D_WIREFRAME] = MK(
        vSpr, fSpr, s_vb_sprite, 1, s_va_sprite, 3,
        SDL_GPU_PRIMITIVETYPE_LINESTRIP, SDL_GPU_FILLMODE_FILL,
        SDL_GPU_CULLMODE_NONE, false, false, true);

    s_gpu_pipelines[GPU_PIPELINE_SPRITE_3D] = MK(
        vSpr, fSpr, s_vb_sprite, 1, s_va_sprite, 3,
        SDL_GPU_PRIMITIVETYPE_TRIANGLESTRIP, SDL_GPU_FILLMODE_FILL,
        SDL_GPU_CULLMODE_NONE, true, false, true);

    // ── Untextured coloured geometry ───────────────────────────────────────
    s_gpu_pipelines[GPU_PIPELINE_UNLIT_STRIP_2D] = MK(
        vUnt, fUnt, s_vb_unlit, 1, s_va_unlit, 2,
        SDL_GPU_PRIMITIVETYPE_TRIANGLESTRIP, SDL_GPU_FILLMODE_FILL,
        SDL_GPU_CULLMODE_NONE, false, false, true);

    // Line list: each pair of vertices is an independent segment (GL_LINES)
    s_gpu_pipelines[GPU_PIPELINE_LINE_2D] = MK(
        vUnt, fUnt, s_vb_unlit, 1, s_va_unlit, 2,
        SDL_GPU_PRIMITIVETYPE_LINELIST, SDL_GPU_FILLMODE_FILL,
        SDL_GPU_CULLMODE_NONE, false, false, true);

    // ── Bounding box ───────────────────────────────────────────────────────
    // LINESTRIP with depth; 6 separate draw calls of 5 verts each (face loop)
    s_gpu_pipelines[GPU_PIPELINE_BOUNDING_BOX] = MK(
        vBB, fUnt, s_vb_bbox, 1, s_va_bbox, 1,
        SDL_GPU_PRIMITIVETYPE_LINESTRIP, SDL_GPU_FILLMODE_FILL,
        SDL_GPU_CULLMODE_NONE, true, false, false);

#undef MK

    // ── Blit to swapchain ──────────────────────────────────────────────────
    // Renders offscreen target into swapchain texture.
    // Must have has_depth_stencil_target = false because the swapchain render
    // pass provides no depth attachment.  Creating it with depth=true and
    // binding it in a depth-less pass causes a Metal validation failure that
    // silently drops the draw call.
    {
        SDL_GPUColorTargetDescription blit_ctd = {
            .format      = color_fmt,
            .blend_state = {0},
        };
        SDL_GPUGraphicsPipelineCreateInfo blit_ci = {
            .vertex_shader   = vSpr,
            .fragment_shader = fSpr,
            .vertex_input_state = {
                .vertex_buffer_descriptions = s_vb_sprite,
                .num_vertex_buffers         = 1,
                .vertex_attributes          = s_va_sprite,
                .num_vertex_attributes      = 3,
            },
            .primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLESTRIP,
            .rasterizer_state = {
                .fill_mode  = SDL_GPU_FILLMODE_FILL,
                .cull_mode  = SDL_GPU_CULLMODE_NONE,
                .front_face = SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE,
            },
            .multisample_state = {
                .sample_count = SDL_GPU_SAMPLECOUNT_1,
                .sample_mask  = 0xFFFFFFFF,
            },
            .depth_stencil_state = {
                .enable_depth_test  = false,
                .enable_depth_write = false,
                .compare_op         = SDL_GPU_COMPAREOP_LESS_OR_EQUAL,
            },
            .target_info = {
                .color_target_descriptions = &blit_ctd,
                .num_color_targets         = 1,
                .depth_stencil_format      = 0,
                .has_depth_stencil_target  = false,
            },
        };
        s_gpu_pipelines[GPU_PIPELINE_BLIT_TO_SWAPCHAIN] =
            SDL_CreateGPUGraphicsPipeline(device, &blit_ci);
        if (!s_gpu_pipelines[GPU_PIPELINE_BLIT_TO_SWAPCHAIN])
        {
            info_log("SDL_GPU: blit pipeline creation failed: %s", SDL_GetError());
        }
    }

    for (int i = 0; i < MAX_GPU_PIPELINES; i++)
    {
        if (!s_gpu_pipelines[i])
        {
            info_log("SDL_GPU: pipeline variant %d failed to create", i);
            return -1;
        }
    }

    return 0;
}

// ─────────────────────────────────────────────────────────────────────────────
// Buffer metadata initialisation
// Fills g_pipelines[] with buffer layout info (strides/offsets) and creates
// the SDL_GPUBuffer objects. Called once from MikuPan_InitPipelines.
// ─────────────────────────────────────────────────────────────────────────────

static int init_buffers(SDL_GPUDevice *device)
{
    MikuPan_PipelineInfo    *p;
    MikuPan_BufferObjectInfo *b;

    // ── POSITION3_NORMAL3_UV2 (mesh 0x12 / 0x82) ──────────────────────────
    p = &g_pipelines[POSITION3_NORMAL3_UV2];
    p->num_buffers = 2;
    p->buffers     = calloc(2, sizeof(MikuPan_BufferObjectInfo));

    b = &p->buffers[0]; // interleaved pos3+norm3, stride 24
    b->buffer_length = 1024 * 32;
    b->num_attributes = 2;
    b->attributes    = calloc(2, sizeof(MikuPan_AttributeInfo));
    b->attributes[0] = (MikuPan_AttributeInfo){ .size=3, .index=0, .stride=24, .offset=0  };
    b->attributes[1] = (MikuPan_AttributeInfo){ .size=3, .index=1, .stride=24, .offset=12 };

    b = &p->buffers[1]; // uv2, stride 8
    b->buffer_length = 1024 * 32;
    b->num_attributes = 1;
    b->attributes    = calloc(1, sizeof(MikuPan_AttributeInfo));
    b->attributes[0] = (MikuPan_AttributeInfo){ .size=2, .index=2, .stride=8, .offset=0 };

    // ── POSITION4_NORMAL4_UV2 (mesh 0x2 / 0xA) ────────────────────────────
    p = &g_pipelines[POSITION4_NORMAL4_UV2];
    p->num_buffers = 2;
    p->buffers     = calloc(2, sizeof(MikuPan_BufferObjectInfo));

    b = &p->buffers[0]; // interleaved pos4+norm4, stride 32
    b->buffer_length = 1024 * 32;
    b->num_attributes = 2;
    b->attributes    = calloc(2, sizeof(MikuPan_AttributeInfo));
    b->attributes[0] = (MikuPan_AttributeInfo){ .size=4, .index=0, .stride=32, .offset=0  };
    b->attributes[1] = (MikuPan_AttributeInfo){ .size=4, .index=1, .stride=32, .offset=16 };

    b = &p->buffers[1]; // uv2, stride 8
    b->buffer_length = 1024 * 32;
    b->num_attributes = 1;
    b->attributes    = calloc(1, sizeof(MikuPan_AttributeInfo));
    b->attributes[0] = (MikuPan_AttributeInfo){ .size=2, .index=2, .stride=8, .offset=0 };

    // ── UV4_COLOUR4_POSITION4 (sprite) ─────────────────────────────────────
    p = &g_pipelines[UV4_COLOUR4_POSITION4];
    p->num_buffers = 1;
    p->buffers     = calloc(1, sizeof(MikuPan_BufferObjectInfo));

    b = &p->buffers[0]; // uv4+colour4+pos4, stride 48, 4 vertices
    b->buffer_length = 4 * 48;
    b->num_attributes = 3;
    b->attributes    = calloc(3, sizeof(MikuPan_AttributeInfo));
    b->attributes[0] = (MikuPan_AttributeInfo){ .size=4, .index=0, .stride=48, .offset=0  };
    b->attributes[1] = (MikuPan_AttributeInfo){ .size=4, .index=1, .stride=48, .offset=16 };
    b->attributes[2] = (MikuPan_AttributeInfo){ .size=4, .index=2, .stride=48, .offset=32 };

    // ── COLOUR4_POSITION4 (untextured sprite / lines) ──────────────────────
    p = &g_pipelines[COLOUR4_POSITION4];
    p->num_buffers = 1;
    p->buffers     = calloc(1, sizeof(MikuPan_BufferObjectInfo));

    b = &p->buffers[0]; // colour4+pos4, stride 32, 4 vertices
    b->buffer_length = 4 * 32;
    b->num_attributes = 2;
    b->attributes    = calloc(2, sizeof(MikuPan_AttributeInfo));
    b->attributes[0] = (MikuPan_AttributeInfo){ .size=4, .index=0, .stride=32, .offset=0  };
    b->attributes[1] = (MikuPan_AttributeInfo){ .size=4, .index=1, .stride=32, .offset=16 };

    // ── POSITION4 (bounding box) ───────────────────────────────────────────
    // 6 faces × 5 verts (LINESTRIP closes each face by repeating first vert)
    p = &g_pipelines[POSITION4];
    p->num_buffers = 1;
    p->buffers     = calloc(1, sizeof(MikuPan_BufferObjectInfo));

    b = &p->buffers[0]; // pos4, stride 16, 30 vertices
    b->buffer_length = 30 * 16;
    b->num_attributes = 1;
    b->attributes    = calloc(1, sizeof(MikuPan_AttributeInfo));
    b->attributes[0] = (MikuPan_AttributeInfo){ .size=4, .index=0, .stride=16, .offset=0 };

    // LIGHTING_DATA is a sentinel — no GPU buffers needed
    g_pipelines[LIGHTING_DATA].num_buffers = 0;
    g_pipelines[LIGHTING_DATA].buffers     = NULL;

    // ── Create GPU vertex buffers ──────────────────────────────────────────
    for (int i = 0; i < MAX_NUMBER_OF_PIPELINES; i++)
    {
        MikuPan_PipelineInfo *pi = &g_pipelines[i];
        for (u_int j = 0; j < pi->num_buffers; j++)
        {
            MikuPan_BufferObjectInfo *bi = &pi->buffers[j];
            if (bi->buffer_length == 0) continue;

            SDL_GPUBufferCreateInfo ci = {
                .usage = SDL_GPU_BUFFERUSAGE_VERTEX,
                .size  = (Uint32) bi->buffer_length,
                .props = 0,
            };
            bi->buffer = SDL_CreateGPUBuffer(device, &ci);
            if (!bi->buffer)
            {
                info_log("SDL_GPU: failed to create vertex buffer [%d][%u]: %s",
                         i, j, SDL_GetError());
                return -1;
            }
        }
    }

    // ── Create shared index buffer (4 MB, INT32) ───────────────────────────
    SDL_GPUBufferCreateInfo idx_ci = {
        .usage = SDL_GPU_BUFFERUSAGE_INDEX,
        .size  = sizeof(int) * 1024 * 1024,
        .props = 0,
    };
    g_index_buffer = SDL_CreateGPUBuffer(device, &idx_ci);
    if (!g_index_buffer)
    {
        info_log("SDL_GPU: failed to create index buffer: %s", SDL_GetError());
        return -1;
    }

    return 0;
}

// ─────────────────────────────────────────────────────────────────────────────
// Sampler creation
// ─────────────────────────────────────────────────────────────────────────────

static int create_sampler(SDL_GPUDevice *device)
{
    SDL_GPUSamplerCreateInfo sci = {
        .min_filter        = SDL_GPU_FILTER_LINEAR,
        .mag_filter        = SDL_GPU_FILTER_LINEAR,
        .mipmap_mode       = SDL_GPU_SAMPLERMIPMAPMODE_LINEAR,
        .address_mode_u    = SDL_GPU_SAMPLERADDRESSMODE_REPEAT,
        .address_mode_v    = SDL_GPU_SAMPLERADDRESSMODE_REPEAT,
        .address_mode_w    = SDL_GPU_SAMPLERADDRESSMODE_REPEAT,
        .mip_lod_bias      = 0.0f,
        .enable_anisotropy = true,
        .max_anisotropy    = 16.0f,
        .min_lod           = 0.0f,
        .max_lod           = 1000.0f,
        .props             = 0,
    };
    g_gpu_sampler = SDL_CreateGPUSampler(device, &sci);
    if (!g_gpu_sampler)
    {
        info_log("SDL_GPU: failed to create sampler: %s", SDL_GetError());
        return -1;
    }
    return 0;
}

// ─────────────────────────────────────────────────────────────────────────────
// Public API
// ─────────────────────────────────────────────────────────────────────────────

int MikuPan_InitPipelines(SDL_GPUDevice       *device,
                          SDL_GPUTextureFormat  color_format,
                          SDL_GPUSampleCount    sample_count)
{
    if (create_sampler(device) != 0)   return -1;
    if (init_buffers(device)   != 0)   return -1;
    if (create_pipelines_internal(device, color_format, sample_count) != 0) return -1;

    info_log("SDL_GPU: initialized %d GPU pipeline variants", MAX_GPU_PIPELINES);
    return 0;
}

void MikuPan_DestroyPipelines(SDL_GPUDevice *device)
{
    for (int i = 0; i < MAX_GPU_PIPELINES; i++)
    {
        if (s_gpu_pipelines[i])
        {
            SDL_ReleaseGPUGraphicsPipeline(device, s_gpu_pipelines[i]);
            s_gpu_pipelines[i] = NULL;
        }
    }

    for (int i = 0; i < MAX_NUMBER_OF_PIPELINES; i++)
    {
        MikuPan_PipelineInfo *p = &g_pipelines[i];
        for (u_int j = 0; j < p->num_buffers; j++)
        {
            if (p->buffers[j].buffer)
            {
                SDL_ReleaseGPUBuffer(device, p->buffers[j].buffer);
                p->buffers[j].buffer = NULL;
            }
            free(p->buffers[j].attributes);
            p->buffers[j].attributes = NULL;
        }
        free(p->buffers);
        p->buffers     = NULL;
        p->num_buffers = 0;
    }

    if (g_index_buffer)
    {
        SDL_ReleaseGPUBuffer(device, g_index_buffer);
        g_index_buffer = NULL;
    }

    if (g_gpu_sampler)
    {
        SDL_ReleaseGPUSampler(device, g_gpu_sampler);
        g_gpu_sampler = NULL;
    }
}

int MikuPan_RecreatePipelines(SDL_GPUDevice       *device,
                              SDL_GPUTextureFormat  color_format,
                              SDL_GPUSampleCount    sample_count)
{
    for (int i = 0; i < MAX_GPU_PIPELINES; i++)
    {
        if (s_gpu_pipelines[i])
        {
            SDL_ReleaseGPUGraphicsPipeline(device, s_gpu_pipelines[i]);
            s_gpu_pipelines[i] = NULL;
        }
    }
    return create_pipelines_internal(device, color_format, sample_count);
}

SDL_GPUGraphicsPipeline *MikuPan_GetGpuPipeline(MikuPan_GpuPipelineType type)
{
    if (type < 0 || type >= MAX_GPU_PIPELINES)
    {
        return NULL;
    }
    return s_gpu_pipelines[type];
}

MikuPan_PipelineInfo *MikuPan_GetPipelineInfo(enum MikuPan_PipelineType pipeline_type)
{
    if (pipeline_type >= MAX_NUMBER_OF_PIPELINES)
    {
        return NULL;
    }
    return &g_pipelines[pipeline_type];
}
