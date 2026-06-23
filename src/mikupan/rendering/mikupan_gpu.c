#include "mikupan_gpu.h"

#include "mikupan/mikupan_logging_c.h"
#include "mikupan_pipeline.h"
#include "mikupan_shader.h"
#include <glad/gl.h>
#include <stdlib.h>
#include <string.h>

#define MIKUPAN_GPU_MAX_BUFFERS 8192
#define MIKUPAN_GPU_MAX_TEXTURES 8192
#define MIKUPAN_GPU_MAX_VAOS 4096
#define MIKUPAN_GPU_MAX_PIPELINES 4096

typedef struct
{
    SDL_GPUBuffer* buffer;
    unsigned int size;
    MikuPan_GPUBufferKind kind;
    void* cpu_data;
} GPUBufferEntry;

typedef struct
{
    SDL_GPUTexture* texture;
    SDL_GPUSampler* sampler;
    int width;
    int height;
    SDL_GPUTextureFormat format;
    SDL_GPUTextureUsageFlags usage;
} GPUTextureEntry;

typedef struct
{
    int pipeline_type;
    unsigned int num_buffers;
    unsigned int buffers[4];
    unsigned int ibo;
} GPUVaoEntry;

typedef struct
{
    int used;
    int shader_index;
    /* The pipeline's vertex layout depends only on the vertex-format type, not
     * on which VAO instance is bound — so the cache keys on pipeline_type. Keying
     * on the VAO id (as it did before) created a duplicate pipeline for every
     * mesh-cache entry and leaked them when entries were destroyed (e.g. each
     * time an enemy model spawned/despawned). */
    int pipeline_type;
    unsigned int primitive;
    unsigned int state_hash;
    SDL_GPUTextureFormat color_format;
    SDL_GPUTextureFormat depth_format;
    int sample_count;
    SDL_GPUGraphicsPipeline* pipeline;
} GPUPipelineEntry;

typedef struct
{
    int depth_test;
    int depth_write;
    SDL_GPUCompareOp compare;
    int blend;
    int additive_blend;
    SDL_GPUCullMode cull_mode;
    SDL_GPUFillMode fill_mode;
    SDL_GPUColorComponentFlags color_mask;
    int color_mask_enabled;
    int depth_bias;
} GPURenderState;

static SDL_GPUDevice* g_device = NULL;
static SDL_Window* g_window = NULL;
static SDL_GPUCommandBuffer* g_cmd = NULL;
static SDL_GPURenderPass* g_pass = NULL;
static SDL_GPUTexture* g_swapchain = NULL;
static SDL_GPUTextureFormat g_swapchain_format = SDL_GPU_TEXTUREFORMAT_INVALID;

static SDL_GPUTexture* g_fallback_texture = NULL;
static SDL_GPUSampler* g_fallback_sampler = NULL;

#define MIKUPAN_XFER_FRAMES_IN_FLIGHT 3
#define MIKUPAN_XFER_POOL_MAX 1024
#define MIKUPAN_XFER_TRIM_FRAMES                                               \
    600 /* ~10s at 60fps before an idle buffer is freed */

typedef struct
{
    SDL_GPUTransferBuffer* buffer;
    unsigned int size;
    Uint64 last_frame;
} GPUTransferPoolEntry;

static GPUTransferPoolEntry g_xfer_pool[MIKUPAN_XFER_POOL_MAX];
static int g_xfer_pool_count = 0;
static Uint64 g_gpu_frame = 0;

/// Acquire a pooled transfer buffer of at least `size`. The pool owns it — the
/// caller maps (cycle=true) and records the copy, but must NOT release it.
static SDL_GPUTransferBuffer* AcquireUploadTransfer(unsigned int size)
{
    int best = -1;

    for (int i = 0; i < g_xfer_pool_count; i++)
    {
        GPUTransferPoolEntry* e = &g_xfer_pool[i];
        if (e->buffer == NULL || e->size < size)
        {
            continue;
        }

        if ((g_gpu_frame - e->last_frame) >= MIKUPAN_XFER_FRAMES_IN_FLIGHT
            && (best < 0 || e->size < g_xfer_pool[best].size))
        {
            best = i;
        }
    }
    if (best >= 0)
    {
        g_xfer_pool[best].last_frame = g_gpu_frame;
        return g_xfer_pool[best].buffer;
    }

    SDL_GPUTransferBufferCreateInfo info = {0};
    info.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
    info.size = size;
    SDL_GPUTransferBuffer* buf = SDL_CreateGPUTransferBuffer(g_device, &info);

    if (buf == NULL)
    {
        return NULL;
    }

    int slot = g_xfer_pool_count;

    if (slot >= MIKUPAN_XFER_POOL_MAX)
    {
        /* Pool full (pathological). Evict the least-recently-used entry; the
         * release is deferred by SDL until the GPU is done, so it's safe. */
        slot = 0;
        for (int i = 1; i < g_xfer_pool_count; i++)
        {
            if (g_xfer_pool[i].last_frame < g_xfer_pool[slot].last_frame)
            {
                slot = i;
            }
        }
        SDL_ReleaseGPUTransferBuffer(g_device, g_xfer_pool[slot].buffer);
    }
    else
    {
        g_xfer_pool_count++;
    }

    g_xfer_pool[slot].buffer = buf;
    g_xfer_pool[slot].size = size;
    g_xfer_pool[slot].last_frame = g_gpu_frame;

    return buf;
}

/// Free pool buffers idle for a long time so a one-off heavy area doesn't pin
/// transfer memory forever. Called once per frame.
static void TrimUploadTransferPool(void)
{
    for (int i = 0; i < g_xfer_pool_count; i++)
    {
        GPUTransferPoolEntry* e = &g_xfer_pool[i];

        if (e->buffer != NULL
            && (g_gpu_frame - e->last_frame) >= MIKUPAN_XFER_TRIM_FRAMES)
        {
            SDL_ReleaseGPUTransferBuffer(g_device, e->buffer);
            *e = g_xfer_pool[g_xfer_pool_count - 1];
            g_xfer_pool_count--;
            i--;
        }
    }
}

static GPUBufferEntry g_buffers[MIKUPAN_GPU_MAX_BUFFERS];
static GPUTextureEntry g_textures[MIKUPAN_GPU_MAX_TEXTURES];
static GPUVaoEntry g_vaos[MIKUPAN_GPU_MAX_VAOS];
static GPUPipelineEntry g_pipeline_cache[MIKUPAN_GPU_MAX_PIPELINES];

static unsigned int g_next_buffer_id = 1;
static unsigned int g_next_texture_id = 1;
static unsigned int g_next_vao_id = 1;

static unsigned int g_scene_texture_id =
    0; /* single-sample resolve target (sampled/blitted) */
static unsigned int g_scene_msaa_color_id =
    0; /* multisampled colour target (0 when MSAA off) */
static unsigned int g_scene_depth_id = 0;
static int g_scene_width = 0;
static int g_scene_height = 0;
static int g_scene_msaa = 1; /* actual sample count in use (1,2,4,8) */
static int g_pipeline_scene_msaa = 1;

static MikuPan_GPUTarget g_target = MIKUPAN_GPU_TARGET_SCENE;
static SDL_GPUTexture* g_target_color = NULL;
static SDL_GPUTexture* g_target_resolve =
    NULL; /* resolve dest when the target is multisampled */
static int g_target_sample_count = 1; /* sample count of the current target */
static SDL_GPUTexture* g_target_depth = NULL;
static SDL_GPUTextureFormat g_target_color_format =
    SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
static SDL_GPUTextureFormat g_target_depth_format =
    SDL_GPU_TEXTUREFORMAT_D24_UNORM;
/* The depth-target format actually supported by this device. D24_UNORM is not
 * universally available (e.g. many Vulkan/AMD drivers on Linux reject it), so
 * it is resolved at init via SDL_GPUTextureSupportsFormat. */
static SDL_GPUTextureFormat g_depth_format = SDL_GPU_TEXTUREFORMAT_D24_UNORM;
static int g_target_width = 0;
static int g_target_height = 0;
static int g_target_clear = 0;
static int g_target_clear_depth = 0;
static int g_target_has_depth = 1;
static int g_target_initialized = 0;
/* The single-sample scene texture is only current after an explicit resolve.
 * Normal scene pass breaks should STORE the MSAA target without resolving;
 * framebuffer reads/copies and final present request a resolve on demand. */
static int g_scene_resolve_dirty = 0;
static int g_scene_resolve_requested = 0;
static int g_scene_resolve_preserve_msaa = 1;
static int g_pass_resolves_scene = 0;

static GPURenderState g_state = {0};
static unsigned int g_bound_vao = 0;
static unsigned int g_bound_textures[2] = {0, 0};
static int g_current_shader = -1;
static int g_viewport[4] = {0, 0, 1, 1};
static int g_scissor_enabled = 0;
static SDL_Rect g_scissor = {0, 0, 1, 1};

/*
 * Per-draw state caching. The light (slot 1) and material (slot 2) uniform
 * blocks change far less often than once per draw, but pushing the full blocks
 * for every draw call is a large chunk of the mesh-render CPU cost. Pushed
 * uniform data persists for the lifetime of the command buffer, so we only
 * re-push these when their data actually changes — plus once whenever a new
 * render pass begins, which both re-arms them safely and matches SDL_GPU's
 * per-pass binding reset for the pipeline below. (Slot 0 holds per-object
 * matrices and still must be pushed every draw.)
 */
static int g_light_uniform_dirty = 1;
static int g_material_uniform_dirty = 1;
/* Slot-0 (g_uniforms) push tracking. The vertex stage reads the per-object
 * matrices, which change every mesh; the fragment stage reads only colours,
 * flags, fog and uShadowMatrix. Tracking the two stages separately lets the
 * common mesh path (matrices only) skip the fragment push entirely. */
static int g_vertex_uniform_dirty = 1;
static int g_fragment_uniform_dirty = 1;
static SDL_GPUGraphicsPipeline* g_last_bound_pipeline = NULL;
/// Vertex storage buffer (bone-matrix palette) bound for the next skinned draw.
/// NULL when the current draw is not skinned; the render path sets it before a
/// skinned draw and clears it afterwards.
static SDL_GPUBuffer* g_vertex_storage_buffer = NULL;

/// Per-draw binding cache. SDL_GPU vertex-buffer and fragment-sampler bindings
/// persist across draws within a pass, so re-issuing identical bindings every
/// draw is pure overhead. Track the last bound set and skip the call when it is
/// unchanged. Both are invalidated at pass begin (a new pass clears bindings).
static SDL_GPUBuffer* g_last_vertex_buffers[4] = {0};
static unsigned int g_last_vertex_buffer_count = 0;
static int g_vertex_binding_valid = 0;

static void BeginTargetPassIfNeeded(void);
static void ResolveSceneTexture(int preserve_msaa);
static SDL_GPUTexture* g_last_sampler_textures[2] = {0};
static SDL_GPUSampler* g_last_sampler_samplers[2] = {0};
static int g_sampler_binding_valid = 0;
static SDL_GPUBuffer* g_last_index_buffer = NULL;
static SDL_GPUIndexElementSize g_last_index_size =
    SDL_GPU_INDEXELEMENTSIZE_32BIT;
static int g_index_binding_valid = 0;

static MikuPan_GPUUniformBlock g_uniforms;
static MikuPan_LightData g_light_data;
static MikuPan_MaterialData g_material_data = {
    .uMatAmbient = {1.0f, 1.0f, 1.0f, 1.0f},
    .uMatDiffuse = {1.0f, 1.0f, 1.0f, 1.0f},
    .uMatSpecular = {0.0f, 0.0f, 0.0f, 1.0f},
    .uMatEmission = {0.0f, 0.0f, 0.0f, 1.0f},
};

extern SDL_GPUShader* MikuPan_GetGPUVertexShader(int idx);
extern SDL_GPUShader* MikuPan_GetGPUFragmentShader(int idx);

static SDL_GPUTextureFormat PickSupportedDepthFormat(void);

static int CurrentTargetDrawable(void)
{
    return g_cmd != NULL && g_target_color != NULL && g_target_width > 0
           && g_target_height > 0;
}

static unsigned int AllocBufferId(void)
{
    for (unsigned int i = g_next_buffer_id; i < MIKUPAN_GPU_MAX_BUFFERS; i++)
    {
        if (g_buffers[i].buffer == NULL && g_buffers[i].cpu_data == NULL)
        {
            g_next_buffer_id = i + 1;
            return i;
        }
    }
    for (unsigned int i = 1; i < g_next_buffer_id; i++)
    {
        if (g_buffers[i].buffer == NULL && g_buffers[i].cpu_data == NULL)
        {
            g_next_buffer_id = i + 1;
            return i;
        }
    }
    return 0;
}

static unsigned int AllocTextureId(void)
{
    for (unsigned int i = g_next_texture_id; i < MIKUPAN_GPU_MAX_TEXTURES; i++)
    {
        if (g_textures[i].texture == NULL)
        {
            g_next_texture_id = i + 1;
            return i;
        }
    }
    for (unsigned int i = 1; i < g_next_texture_id; i++)
    {
        if (g_textures[i].texture == NULL)
        {
            g_next_texture_id = i + 1;
            return i;
        }
    }
    return 0;
}

static unsigned int AllocVaoId(void)
{
    for (unsigned int i = g_next_vao_id; i < MIKUPAN_GPU_MAX_VAOS; i++)
    {
        if (g_vaos[i].pipeline_type == 0 && i != 0)
        {
            g_next_vao_id = i + 1;
            return i;
        }
    }
    for (unsigned int i = 1; i < g_next_vao_id; i++)
    {
        if (g_vaos[i].pipeline_type == 0)
        {
            g_next_vao_id = i + 1;
            return i;
        }
    }
    return 0;
}

static SDL_GPUSampleCount SampleCountFromMSAA(int msaa)
{
    /* SDL_GPU supports at most 8x; clamp higher UI values (16/32) to 8 rather
     * than falling through to no MSAA. */
    if (msaa >= 8)
    {
        return SDL_GPU_SAMPLECOUNT_8;
    }
    if (msaa >= 4)
    {
        return SDL_GPU_SAMPLECOUNT_4;
    }
    if (msaa >= 2)
    {
        return SDL_GPU_SAMPLECOUNT_2;
    }
    return SDL_GPU_SAMPLECOUNT_1;
}

static SDL_GPUBufferUsageFlags BufferUsageFromKind(MikuPan_GPUBufferKind kind)
{
    switch (kind)
    {
        case MIKUPAN_GPU_BUFFER_INDEX:
            return SDL_GPU_BUFFERUSAGE_INDEX;
        case MIKUPAN_GPU_BUFFER_STORAGE:
            return SDL_GPU_BUFFERUSAGE_GRAPHICS_STORAGE_READ;
        case MIKUPAN_GPU_BUFFER_VERTEX:
        default:
            return SDL_GPU_BUFFERUSAGE_VERTEX;
    }
}

static SDL_GPUPrimitiveType PrimitiveFromGL(unsigned int mode)
{
    switch (mode)
    {
        case GL_TRIANGLE_STRIP:
            return SDL_GPU_PRIMITIVETYPE_TRIANGLESTRIP;
        case GL_LINES:
            return SDL_GPU_PRIMITIVETYPE_LINELIST;
        case GL_LINE_LOOP:
        case GL_LINE_STRIP:
            return SDL_GPU_PRIMITIVETYPE_LINESTRIP;
        case GL_POINTS:
            return SDL_GPU_PRIMITIVETYPE_POINTLIST;
        case GL_TRIANGLES:
        default:
            return SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
    }
}

static SDL_GPUCompareOp CompareFromGL(unsigned int func)
{
    switch (func)
    {
        case GL_NEVER:
            return SDL_GPU_COMPAREOP_NEVER;
        case GL_ALWAYS:
            return SDL_GPU_COMPAREOP_ALWAYS;
        case GL_GEQUAL:
            return SDL_GPU_COMPAREOP_GREATER_OR_EQUAL;
        case GL_GREATER:
            return SDL_GPU_COMPAREOP_GREATER;
        case GL_LESS:
            return SDL_GPU_COMPAREOP_LESS;
        case GL_EQUAL:
            return SDL_GPU_COMPAREOP_EQUAL;
        case GL_NOTEQUAL:
            return SDL_GPU_COMPAREOP_NOT_EQUAL;
        case GL_LEQUAL:
        default:
            return SDL_GPU_COMPAREOP_LESS_OR_EQUAL;
    }
}

static SDL_GPUVertexElementFormat VertexFormatFromSize(int size)
{
    switch (size)
    {
        case 1:
            return SDL_GPU_VERTEXELEMENTFORMAT_FLOAT;
        case 2:
            return SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2;
        case 3:
            return SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3;
        case 4:
        default:
            return SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4;
    }
}

static unsigned int RenderStateHash(void)
{
    unsigned int h = 2166136261u;
    const unsigned int vals[] = {
        (unsigned int) g_state.depth_test,
        (unsigned int) g_state.depth_write,
        (unsigned int) g_state.compare,
        (unsigned int) g_state.blend,
        (unsigned int) g_state.additive_blend,
        (unsigned int) g_state.cull_mode,
        (unsigned int) g_state.fill_mode,
        (unsigned int) g_state.color_mask,
        (unsigned int) g_state.color_mask_enabled,
        (unsigned int) g_state.depth_bias,
    };
    for (unsigned int i = 0; i < sizeof(vals) / sizeof(vals[0]); i++)
    {
        h ^= vals[i];
        h *= 16777619u;
    }
    return h;
}

static void SetDefaultUniforms(void)
{
    memset(&g_uniforms, 0, sizeof(g_uniforms));
    for (int i = 0; i < 4; i++)
    {
        g_uniforms.model[i * 4 + i] = 1.0f;
        g_uniforms.view[i * 4 + i] = 1.0f;
        g_uniforms.projection[i * 4 + i] = 1.0f;
        g_uniforms.mvp[i * 4 + i] = 1.0f;
        g_uniforms.modelView[i * 4 + i] = 1.0f;
        g_uniforms.viewProj[i * 4 + i] = 1.0f;
        g_uniforms.uShadowMatrix[i * 4 + i] = 1.0f;
        g_uniforms.uWorldClipView[i * 4 + i] = 1.0f;
    }

    g_uniforms.normalMatrix[0] = 1.0f;
    g_uniforms.normalMatrix[5] = 1.0f;
    g_uniforms.normalMatrix[10] = 1.0f;
    g_uniforms.viewNormalMatrix[0] = 1.0f;
    g_uniforms.viewNormalMatrix[5] = 1.0f;
    g_uniforms.viewNormalMatrix[10] = 1.0f;

    g_uniforms.uColor[0] = 1.0f;
    g_uniforms.uColor[1] = 1.0f;
    g_uniforms.uColor[2] = 1.0f;
    g_uniforms.uColor[3] = 1.0f;
    g_uniforms.uTextureSize[0] = 1.0f;
    g_uniforms.uTextureSize[1] = 1.0f;
    g_uniforms.uOutputSize[0] = 1.0f;
    g_uniforms.uOutputSize[1] = 1.0f;
    g_uniforms.uPhotoNegativeContentRect[2] = 1.0f;
    g_uniforms.uPhotoNegativeContentRect[3] = 1.0f;
    g_uniforms.uPhotoNegativeRect[2] = 1.0f;
    g_uniforms.uPhotoNegativeRect[3] = 1.0f;
    g_uniforms.uFramebufferUvScale[0] = 1.0f;
    g_uniforms.uFramebufferUvScale[1] = 1.0f;
    g_uniforms.uFramebufferContentUvMax[0] = 1.0f;
    g_uniforms.uFramebufferContentUvMax[1] = 1.0f;
    g_uniforms.uRenderSize[0] = 1.0f;
    g_uniforms.uRenderSize[1] = 1.0f;
    g_uniforms.uParams0[1] = 0.6f;
    g_uniforms.uParams0[2] = 1.0f;
    g_uniforms.uParams0[3] = 1.0f;
}

static void SetDefaultRenderState(void)
{
    g_state.depth_test = 1;
    g_state.depth_write = 1;
    g_state.compare = SDL_GPU_COMPAREOP_LESS_OR_EQUAL;
    g_state.blend = 1;
    g_state.additive_blend = 0;
    g_state.cull_mode = SDL_GPU_CULLMODE_BACK;
    g_state.fill_mode = SDL_GPU_FILLMODE_FILL;
    g_state.color_mask = SDL_GPU_COLORCOMPONENT_R | SDL_GPU_COLORCOMPONENT_G
                         | SDL_GPU_COLORCOMPONENT_B | SDL_GPU_COLORCOMPONENT_A;
    g_state.color_mask_enabled = 0;
    g_state.depth_bias = 0;
}

static void CreateFallbackTexture(void)
{
    SDL_GPUTextureCreateInfo tinfo = {0};
    tinfo.type = SDL_GPU_TEXTURETYPE_2D;
    tinfo.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
    tinfo.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER;
    tinfo.width = 1;
    tinfo.height = 1;
    tinfo.layer_count_or_depth = 1;
    tinfo.num_levels = 1;
    tinfo.sample_count = SDL_GPU_SAMPLECOUNT_1;
    g_fallback_texture = SDL_CreateGPUTexture(g_device, &tinfo);

    SDL_GPUSamplerCreateInfo sinfo = {0};
    sinfo.min_filter = SDL_GPU_FILTER_LINEAR;
    sinfo.mag_filter = SDL_GPU_FILTER_LINEAR;
    sinfo.mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_NEAREST;
    sinfo.address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
    sinfo.address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
    sinfo.address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
    g_fallback_sampler = SDL_CreateGPUSampler(g_device, &sinfo);

    if (g_fallback_texture == NULL)
    {
        return;
    }

    const Uint32 white = 0xFFFFFFFFu;
    SDL_GPUTransferBufferCreateInfo tb = {0};
    tb.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
    tb.size = sizeof(white);
    SDL_GPUTransferBuffer* transfer =
        SDL_CreateGPUTransferBuffer(g_device, &tb);
    if (transfer == NULL)
    {
        return;
    }

    void* mapped = SDL_MapGPUTransferBuffer(g_device, transfer, true);
    if (mapped != NULL)
    {
        memcpy(mapped, &white, sizeof(white));
        SDL_UnmapGPUTransferBuffer(g_device, transfer);
    }

    SDL_GPUCommandBuffer* cmd = SDL_AcquireGPUCommandBuffer(g_device);
    SDL_GPUTextureTransferInfo src = {0};
    src.transfer_buffer = transfer;
    src.pixels_per_row = 1;
    src.rows_per_layer = 1;
    SDL_GPUTextureRegion dst = {0};
    dst.texture = g_fallback_texture;
    dst.w = 1;
    dst.h = 1;
    dst.d = 1;
    SDL_GPUCopyPass* copy = SDL_BeginGPUCopyPass(cmd);
    SDL_UploadToGPUTexture(copy, &src, &dst, false);
    SDL_EndGPUCopyPass(copy);
    SDL_SubmitGPUCommandBuffer(cmd);
    SDL_ReleaseGPUTransferBuffer(g_device, transfer);
}

int MikuPan_GPUInit(SDL_Window* window, int vsync, const char* gpu_driver,
                    int gpu_debug)
{
    g_window = window;

    const bool debug = gpu_debug ? true : false;
    const char* requested =
        (gpu_driver != NULL && gpu_driver[0] != '\0') ? gpu_driver : NULL;
    g_device =
        SDL_CreateGPUDevice(MIKUPAN_GPU_SHADER_FORMATS, debug, requested);
    if (g_device == NULL && requested != NULL)
    {
        info_log("Could not create SDL_GPU device with driver '%s' (%s), "
                 "falling back to automatic selection",
                 requested, SDL_GetError());
        g_device =
            SDL_CreateGPUDevice(MIKUPAN_GPU_SHADER_FORMATS, debug, NULL);
    }

    if (g_device == NULL)
    {
        info_log("Error creating SDL_GPU device: %s", SDL_GetError());
        return 0;
    }

    if (!SDL_ClaimWindowForGPUDevice(g_device, g_window))
    {
        info_log("Error claiming SDL window for SDL_GPU: %s", SDL_GetError());
        SDL_DestroyGPUDevice(g_device);
        g_device = NULL;
        return 0;
    }

    g_swapchain_format = SDL_GetGPUSwapchainTextureFormat(g_device, g_window);
    g_depth_format = PickSupportedDepthFormat();
    g_target_depth_format = g_depth_format;
    MikuPan_GPUSetVsync(vsync);
    CreateFallbackTexture();
    SetDefaultUniforms();
    SetDefaultRenderState();
    return 1;
}

void MikuPan_GPUShutdown(void)
{
    if (g_device != NULL)
    {
        MikuPan_GPUWaitIdle();
        MikuPan_GPUInvalidatePipelines();
        MikuPan_GPUDestroyInternalBuffer();

        for (unsigned int i = 1; i < MIKUPAN_GPU_MAX_TEXTURES; i++)
        {
            MikuPan_GPUReleaseTexture(i);
        }

        for (unsigned int i = 1; i < MIKUPAN_GPU_MAX_BUFFERS; i++)
        {
            MikuPan_GPUReleaseBuffer(i);
        }

        if (g_fallback_sampler != NULL)
        {
            SDL_ReleaseGPUSampler(g_device, g_fallback_sampler);
            g_fallback_sampler = NULL;
        }

        if (g_fallback_texture != NULL)
        {
            SDL_ReleaseGPUTexture(g_device, g_fallback_texture);
            g_fallback_texture = NULL;
        }

        for (int i = 0; i < g_xfer_pool_count; i++)
        {
            if (g_xfer_pool[i].buffer != NULL)
            {
                SDL_ReleaseGPUTransferBuffer(g_device, g_xfer_pool[i].buffer);
            }
        }

        g_xfer_pool_count = 0;
        SDL_ReleaseWindowFromGPUDevice(g_device, g_window);
        SDL_DestroyGPUDevice(g_device);
    }

    g_device = NULL;
    g_window = NULL;
}

void MikuPan_GPUWaitIdle(void)
{
    if (g_device != NULL)
    {
        SDL_WaitForGPUIdle(g_device);
    }
}

void MikuPan_GPUSetVsync(int vsync)
{
    if (g_device == NULL || g_window == NULL)
    {
        return;
    }

    SDL_SetGPUSwapchainParameters(
        g_device, g_window, SDL_GPU_SWAPCHAINCOMPOSITION_SDR,
        vsync ? SDL_GPU_PRESENTMODE_VSYNC : SDL_GPU_PRESENTMODE_IMMEDIATE);
}

SDL_GPUDevice* MikuPan_GPUGetDevice(void)
{
    return g_device;
}
SDL_GPUCommandBuffer* MikuPan_GPUGetCommandBuffer(void)
{
    return g_cmd;
}
SDL_GPURenderPass* MikuPan_GPUGetRenderPass(void)
{
    return g_pass;
}
SDL_GPUTextureFormat MikuPan_GPUGetSwapchainFormat(void)
{
    return g_swapchain_format;
}

void MikuPan_GPUBeginFrame(void)
{
    g_cmd = SDL_AcquireGPUCommandBuffer(g_device);
    g_pass = NULL;
    g_pass_resolves_scene = 0;
    g_scene_resolve_requested = 0;
    g_scene_resolve_preserve_msaa = 1;
    g_swapchain = NULL;
    g_target_initialized = 0;
    g_gpu_frame++;
    TrimUploadTransferPool();
}

void MikuPan_GPUFlushRenderPass(void)
{
    if (g_pass != NULL)
    {
        SDL_EndGPURenderPass(g_pass);
        g_pass = NULL;
        if (g_pass_resolves_scene)
        {
            g_scene_resolve_dirty = 0;
            g_pass_resolves_scene = 0;
        }
    }
}

void MikuPan_GPUEndFrame(void)
{
    MikuPan_GPUFlushRenderPass();

    if (g_cmd != NULL)
    {
        SDL_SubmitGPUCommandBuffer(g_cmd);
    }

    g_cmd = NULL;
    g_swapchain = NULL;
}

static void ResolveSceneTexture(int preserve_msaa)
{
    if (g_scene_msaa <= 1 || !g_scene_resolve_dirty
        || g_scene_texture_id == 0 || g_scene_msaa_color_id == 0)
    {
        return;
    }

    if (g_target != MIKUPAN_GPU_TARGET_SCENE)
    {
        return;
    }

    /* The resolve store-op is fixed when a render pass starts. End any active
     * scene pass as STORE-only, then open a no-draw pass whose only job is to
     * resolve the stored MSAA texture into the single-sample scene texture. */
    MikuPan_GPUFlushRenderPass();

    if (g_target != MIKUPAN_GPU_TARGET_SCENE || g_target_resolve == NULL
        || g_target_color == NULL)
    {
        return;
    }

    g_scene_resolve_requested = 1;
    g_scene_resolve_preserve_msaa = preserve_msaa != 0;
    BeginTargetPassIfNeeded();
    MikuPan_GPUFlushRenderPass();
}

void MikuPan_GPUResolveSceneForSampling(void)
{
    ResolveSceneTexture(1);
}

void MikuPan_GPUResolveSceneForPresent(void)
{
    ResolveSceneTexture(0);
}

static GPUTextureEntry* TextureEntry(unsigned int id)
{
    if (id == 0 || id >= MIKUPAN_GPU_MAX_TEXTURES)
    {
        return NULL;
    }

    if (g_textures[id].texture == NULL)
    {
        return NULL;
    }

    return &g_textures[id];
}

static GPUBufferEntry* BufferEntry(unsigned int id)
{
    if (id == 0 || id >= MIKUPAN_GPU_MAX_BUFFERS)
    {
        return NULL;
    }

    if (g_buffers[id].buffer == NULL && g_buffers[id].cpu_data == NULL)
    {
        return NULL;
    }

    return &g_buffers[id];
}

static SDL_GPUTextureFormat PickSupportedDepthFormat(void)
{
    const SDL_GPUTextureFormat candidates[] = {
        SDL_GPU_TEXTUREFORMAT_D24_UNORM,
        SDL_GPU_TEXTUREFORMAT_D32_FLOAT,
        SDL_GPU_TEXTUREFORMAT_D16_UNORM,
    };

    for (unsigned int i = 0; i < sizeof(candidates) / sizeof(candidates[0]);
         i++)
    {
        if (SDL_GPUTextureSupportsFormat(
                g_device, candidates[i], SDL_GPU_TEXTURETYPE_2D,
                SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET))
        {
            return candidates[i];
        }
    }

    /* D16_UNORM is guaranteed to be supported by SDL_GPU. */
    return SDL_GPU_TEXTUREFORMAT_D16_UNORM;
}

static void CreateDepthTexture(int width, int height)
{
    if (g_scene_depth_id != 0)
    {
        MikuPan_GPUReleaseTexture(g_scene_depth_id);
        g_scene_depth_id = 0;
    }

    unsigned int id = AllocTextureId();

    if (id == 0)
    {
        return;
    }

    SDL_GPUTextureCreateInfo info = {0};
    info.type = SDL_GPU_TEXTURETYPE_2D;
    info.format = g_depth_format;
    info.usage = SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET;
    info.width = (Uint32) width;
    info.height = (Uint32) height;
    info.layer_count_or_depth = 1;
    info.num_levels = 1;
    /* Depth must be multisampled to match the MSAA colour target. */
    info.sample_count = SampleCountFromMSAA(g_scene_msaa);
    g_textures[id].texture = SDL_CreateGPUTexture(g_device, &info);

    if (g_textures[id].texture == NULL)
    {
        /* The device does not support a multisampled depth target at this
         * sample count. Leave g_scene_depth_id at 0 so the caller can detect
         * the failure and step MSAA down. */
        info_log("Depth texture creation failed (%dx MSAA): %s", g_scene_msaa,
                 SDL_GetError());
        MikuPan_GPUReleaseTexture(id);
        g_scene_depth_id = 0;
        return;
    }

    g_textures[id].width = width;
    g_textures[id].height = height;
    g_textures[id].format = info.format;
    g_textures[id].usage = info.usage;
    g_scene_depth_id = id;
}

static unsigned int CreateMsaaColorTexture(int width, int height,
                                           SDL_GPUSampleCount sample_count)
{
    unsigned int id = AllocTextureId();

    if (id == 0)
    {
        return 0;
    }

    SDL_GPUTextureCreateInfo info = {0};
    info.type = SDL_GPU_TEXTURETYPE_2D;
    info.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
    info.usage = SDL_GPU_TEXTUREUSAGE_COLOR_TARGET;
    info.width = (Uint32) width;
    info.height = (Uint32) height;
    info.layer_count_or_depth = 1;
    info.num_levels = 1;
    info.sample_count = sample_count;
    g_textures[id].texture = SDL_CreateGPUTexture(g_device, &info);
    g_textures[id].width = width;
    g_textures[id].height = height;
    g_textures[id].format = info.format;
    g_textures[id].usage = info.usage;

    if (g_textures[id].texture == NULL)
    {
        info_log("MSAA colour texture creation failed: %s", SDL_GetError());
        MikuPan_GPUReleaseTexture(id);
        return 0;
    }

    return id;
}

void MikuPan_GPUCreateInternalBuffer(int width, int height, int msaa)
{
    if (width <= 0 || height <= 0)
    {
        return;
    }

    MikuPan_GPUDestroyInternalBuffer();
    g_scene_resolve_dirty = 0;
    g_scene_resolve_requested = 0;
    g_scene_resolve_preserve_msaa = 1;
    g_pass_resolves_scene = 0;

    /* Clamp the requested MSAA to a sample count the colour target supports,
     * stepping down toward 1x. The depth-stencil format is deliberately NOT
     * probed here: SDL's D3D12 backend implements SupportsSampleCount for depth
     * formats by querying multisample quality levels with the depth *read*
     * format (D24_UNORM -> R24_UNORM_X8_TYPELESS), which D3D12 reports as
     * unsupported, so probing depth would force MSAA off on D3D12 even though
     * multisampled depth targets work fine. The multisampled depth allocation
     * is verified after creation instead (see below), which is correct on every
     * backend. */
    SDL_GPUSampleCount sc = SampleCountFromMSAA(msaa);
    while (sc != SDL_GPU_SAMPLECOUNT_1
           && !SDL_GPUTextureSupportsSampleCount(
                  g_device, SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM, sc))
    {
        sc = (sc == SDL_GPU_SAMPLECOUNT_8)   ? SDL_GPU_SAMPLECOUNT_4
             : (sc == SDL_GPU_SAMPLECOUNT_4) ? SDL_GPU_SAMPLECOUNT_2
                                             : SDL_GPU_SAMPLECOUNT_1;
    }

    g_scene_msaa = (sc == SDL_GPU_SAMPLECOUNT_8)   ? 8
                   : (sc == SDL_GPU_SAMPLECOUNT_4) ? 4
                   : (sc == SDL_GPU_SAMPLECOUNT_2) ? 2
                                                   : 1;

    g_scene_texture_id = MikuPan_GPUCreateRenderTextureRGBA8(width, height);

    if (g_scene_msaa > 1)
    {
        g_scene_msaa_color_id = CreateMsaaColorTexture(width, height, sc);

        if (g_scene_msaa_color_id == 0)
        {
            g_scene_msaa = 1;
        }
    }

    CreateDepthTexture(width, height);

    /* The multisampled depth target genuinely failed to allocate: this device
     * does not support MSAA depth at this sample count. Drop the MSAA colour
     * target and fall back to single-sample so the scene still renders, then
     * recreate the depth target to match. */
    if (g_scene_msaa > 1 && g_scene_depth_id == 0)
    {
        if (g_scene_msaa_color_id != 0)
        {
            MikuPan_GPUReleaseTexture(g_scene_msaa_color_id);
            g_scene_msaa_color_id = 0;
        }
        g_scene_msaa = 1;
        CreateDepthTexture(width, height);
    }

    g_scene_width = width;
    g_scene_height = height;

    if (g_scene_msaa != g_pipeline_scene_msaa)
    {
        MikuPan_GPUInvalidatePipelines();
        g_pipeline_scene_msaa = g_scene_msaa;
    }
}

void MikuPan_GPUDestroyInternalBuffer(void)
{
    if (g_scene_texture_id != 0)
    {
        MikuPan_GPUReleaseTexture(g_scene_texture_id);
        g_scene_texture_id = 0;
    }

    if (g_scene_msaa_color_id != 0)
    {
        MikuPan_GPUReleaseTexture(g_scene_msaa_color_id);
        g_scene_msaa_color_id = 0;
    }

    if (g_scene_depth_id != 0)
    {
        MikuPan_GPUReleaseTexture(g_scene_depth_id);
        g_scene_depth_id = 0;
    }

    g_scene_width = 0;
    g_scene_height = 0;
    g_scene_msaa = 1;
    g_scene_resolve_dirty = 0;
    g_scene_resolve_requested = 0;
    g_scene_resolve_preserve_msaa = 1;
    g_pass_resolves_scene = 0;
}

unsigned int MikuPan_GPUGetSceneTextureId(void)
{
    return g_scene_texture_id;
}

unsigned int MikuPan_GPUGetSceneWidth(void)
{
    return (unsigned int) g_scene_width;
}

unsigned int MikuPan_GPUGetSceneHeight(void)
{
    return (unsigned int) g_scene_height;
}

int MikuPan_GPUGetSceneMSAA(void)
{
    return g_scene_msaa;
}

unsigned int MikuPan_GPUCreateBuffer(unsigned int size,
                                     MikuPan_GPUBufferKind kind)
{
    unsigned int id = AllocBufferId();

    if (id == 0 || size == 0)
    {
        return 0;
    }

    SDL_GPUBufferCreateInfo info = {0};
    info.usage = BufferUsageFromKind(kind);
    info.size = size;
    g_buffers[id].buffer = SDL_CreateGPUBuffer(g_device, &info);
    g_buffers[id].size = size;
    g_buffers[id].kind = kind;
    if (g_buffers[id].buffer == NULL)
    {
        info_log("SDL_CreateGPUBuffer failed: %s", SDL_GetError());
        memset(&g_buffers[id], 0, sizeof(g_buffers[id]));
        return 0;
    }
    return id;
}

unsigned int MikuPan_GPUCreateUniformCPUBuffer(unsigned int size)
{
    unsigned int id = AllocBufferId();
    if (id == 0 || size == 0)
    {
        return 0;
    }
    g_buffers[id].cpu_data = calloc(1, size);
    g_buffers[id].size = size;
    g_buffers[id].kind = MIKUPAN_GPU_BUFFER_UNIFORM_CPU;
    return id;
}

void MikuPan_GPUReleaseBuffer(unsigned int id)
{
    if (id == 0 || id >= MIKUPAN_GPU_MAX_BUFFERS)
    {
        return;
    }
    if (g_buffers[id].buffer != NULL)
    {
        SDL_ReleaseGPUBuffer(g_device, g_buffers[id].buffer);
    }
    free(g_buffers[id].cpu_data);
    memset(&g_buffers[id], 0, sizeof(g_buffers[id]));
}

void MikuPan_GPUUploadBuffer(unsigned int id, unsigned int size,
                             const void* data)
{
    GPUBufferEntry* entry = BufferEntry(id);
    if (entry == NULL || entry->buffer == NULL || data == NULL || size == 0)
    {
        return;
    }

    if (size > entry->size)
    {
        SDL_ReleaseGPUBuffer(g_device, entry->buffer);
        SDL_GPUBufferCreateInfo info = {0};
        info.usage = BufferUsageFromKind(entry->kind);
        info.size = size;
        entry->buffer = SDL_CreateGPUBuffer(g_device, &info);
        entry->size = size;
    }

    MikuPan_GPUFlushRenderPass();

    SDL_GPUTransferBuffer* transfer = AcquireUploadTransfer(size);
    if (transfer == NULL)
    {
        info_log("AcquireUploadTransfer failed: %s", SDL_GetError());
        return;
    }

    void* mapped = SDL_MapGPUTransferBuffer(g_device, transfer, true);
    if (mapped != NULL)
    {
        memcpy(mapped, data, size);
        SDL_UnmapGPUTransferBuffer(g_device, transfer);
    }

    SDL_GPUTransferBufferLocation src = {0};
    src.transfer_buffer = transfer;
    src.offset = 0;
    SDL_GPUBufferRegion dst = {0};
    dst.buffer = entry->buffer;
    dst.offset = 0;
    dst.size = size;

    SDL_GPUCopyPass* copy = SDL_BeginGPUCopyPass(g_cmd);
    SDL_UploadToGPUBuffer(copy, &src, &dst, true);
    SDL_EndGPUCopyPass(copy);
}

void MikuPan_GPUUpdateUniformCPUBuffer(unsigned int id, unsigned int size,
                                       const void* data)
{
    GPUBufferEntry* entry = BufferEntry(id);
    if (entry == NULL || entry->cpu_data == NULL || data == NULL)
    {
        return;
    }
    if (size > entry->size)
    {
        size = entry->size;
    }
    memcpy(entry->cpu_data, data, size);
    if (size == sizeof(MikuPan_LightData))
    {
        memcpy(&g_light_data, data, size);
        g_light_uniform_dirty = 1;
    }
    else if (size == sizeof(MikuPan_MaterialData))
    {
        memcpy(&g_material_data, data, size);
        g_material_uniform_dirty = 1;
    }
}

void MikuPan_GPUSetVertexStorageBuffer(unsigned int buffer_id)
{
    if (buffer_id == 0)
    {
        g_vertex_storage_buffer = NULL;
        return;
    }
    GPUBufferEntry* entry = BufferEntry(buffer_id);
    g_vertex_storage_buffer = (entry != NULL) ? entry->buffer : NULL;
}

static SDL_GPUSampler* CreateSampler(int repeat, int mipmaps)
{
    SDL_GPUSamplerCreateInfo info = {0};
    info.min_filter = SDL_GPU_FILTER_LINEAR;
    info.mag_filter = SDL_GPU_FILTER_LINEAR;
    info.mipmap_mode = mipmaps ? SDL_GPU_SAMPLERMIPMAPMODE_LINEAR
                               : SDL_GPU_SAMPLERMIPMAPMODE_NEAREST;
    info.address_mode_u = repeat ? SDL_GPU_SAMPLERADDRESSMODE_REPEAT
                                 : SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
    info.address_mode_v = repeat ? SDL_GPU_SAMPLERADDRESSMODE_REPEAT
                                 : SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
    info.address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
    info.max_lod = mipmaps ? 1000.0f : 0.0f;
    return SDL_CreateGPUSampler(g_device, &info);
}

static unsigned int CreateTexture(int width, int height,
                                  SDL_GPUTextureFormat format,
                                  SDL_GPUTextureUsageFlags usage, int repeat,
                                  int mipmaps)
{
    unsigned int id = AllocTextureId();
    if (id == 0)
    {
        return 0;
    }

    SDL_GPUTextureCreateInfo info = {0};
    info.type = SDL_GPU_TEXTURETYPE_2D;
    info.format = format;
    info.usage = usage;
    info.width = (Uint32) width;
    info.height = (Uint32) height;
    info.layer_count_or_depth = 1;
    info.num_levels = 1;
    info.sample_count = SDL_GPU_SAMPLECOUNT_1;
    g_textures[id].texture = SDL_CreateGPUTexture(g_device, &info);
    g_textures[id].sampler = CreateSampler(repeat, mipmaps);
    g_textures[id].width = width;
    g_textures[id].height = height;
    g_textures[id].format = format;
    g_textures[id].usage = usage;
    if (g_textures[id].texture == NULL)
    {
        info_log("SDL_CreateGPUTexture failed: %s", SDL_GetError());
        MikuPan_GPUReleaseTexture(id);
        return 0;
    }
    return id;
}

unsigned int MikuPan_GPUCreateTextureRGBA8(int width, int height,
                                           const void* pixels, int pitch,
                                           int repeat, int mipmaps)
{
    unsigned int id =
        CreateTexture(width, height, SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM,
                      SDL_GPU_TEXTUREUSAGE_SAMPLER, repeat, mipmaps);
    if (id != 0 && pixels != NULL)
    {
        MikuPan_GPUUploadTextureRGBA8(id, width, height, pixels, pitch);
    }
    return id;
}

unsigned int MikuPan_GPUCreateTextureR8Target(int width, int height)
{
    return CreateTexture(
        width, height, SDL_GPU_TEXTUREFORMAT_R8_UNORM,
        SDL_GPU_TEXTUREUSAGE_SAMPLER | SDL_GPU_TEXTUREUSAGE_COLOR_TARGET, 0, 0);
}

unsigned int MikuPan_GPUCreateRenderTextureRGBA8(int width, int height)
{
    return CreateTexture(
        width, height, SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM,
        SDL_GPU_TEXTUREUSAGE_SAMPLER | SDL_GPU_TEXTUREUSAGE_COLOR_TARGET, 0, 0);
}

void MikuPan_GPUReleaseTexture(unsigned int id)
{
    if (id == 0 || id >= MIKUPAN_GPU_MAX_TEXTURES)
    {
        return;
    }
    if (g_textures[id].sampler != NULL)
    {
        SDL_ReleaseGPUSampler(g_device, g_textures[id].sampler);
    }
    if (g_textures[id].texture != NULL)
    {
        SDL_ReleaseGPUTexture(g_device, g_textures[id].texture);
    }
    memset(&g_textures[id], 0, sizeof(g_textures[id]));
}

void MikuPan_GPUUploadTextureRGBA8(unsigned int id, int width, int height,
                                   const void* pixels, int pitch)
{
    GPUTextureEntry* entry = TextureEntry(id);
    if (entry == NULL || pixels == NULL || width <= 0 || height <= 0)
    {
        return;
    }

    if (pitch <= 0)
    {
        pitch = width * 4;
    }

    MikuPan_GPUFlushRenderPass();

    unsigned int upload_size = (unsigned int) (pitch * height);
    SDL_GPUTransferBuffer* transfer = AcquireUploadTransfer(upload_size);
    if (transfer == NULL)
    {
        return;
    }

    void* mapped = SDL_MapGPUTransferBuffer(g_device, transfer, true);
    if (mapped != NULL)
    {
        memcpy(mapped, pixels, upload_size);
        SDL_UnmapGPUTransferBuffer(g_device, transfer);
    }

    SDL_GPUTextureTransferInfo src = {0};
    src.transfer_buffer = transfer;
    src.offset = 0;
    src.pixels_per_row = (Uint32) (pitch / 4);
    src.rows_per_layer = (Uint32) height;

    SDL_GPUTextureRegion dst = {0};
    dst.texture = entry->texture;
    dst.w = (Uint32) width;
    dst.h = (Uint32) height;
    dst.d = 1;

    SDL_GPUCopyPass* copy = SDL_BeginGPUCopyPass(g_cmd);
    SDL_UploadToGPUTexture(copy, &src, &dst, true);
    SDL_EndGPUCopyPass(copy);
}

int MikuPan_GPUReadTextureRGBA8(unsigned int texture_id, int width, int height,
                                unsigned char* out_rgba)
{
    GPUTextureEntry* entry = TextureEntry(texture_id);
    if (entry == NULL || out_rgba == NULL || width <= 0 || height <= 0)
    {
        return 0;
    }

    if (texture_id == g_scene_texture_id)
    {
        MikuPan_GPUResolveSceneForSampling();
    }
    MikuPan_GPUFlushRenderPass();
    const unsigned int pitch = (unsigned int) width * 4u;
    const unsigned int size = pitch * (unsigned int) height;
    SDL_GPUTransferBufferCreateInfo info = {0};
    info.usage = SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD;
    info.size = size;
    SDL_GPUTransferBuffer* transfer =
        SDL_CreateGPUTransferBuffer(g_device, &info);
    if (transfer == NULL)
    {
        return 0;
    }

    SDL_GPUTextureRegion src = {0};
    src.texture = entry->texture;
    src.w = (Uint32) width;
    src.h = (Uint32) height;
    src.d = 1;
    SDL_GPUTextureTransferInfo dst = {0};
    dst.transfer_buffer = transfer;
    dst.pixels_per_row = (Uint32) width;
    dst.rows_per_layer = (Uint32) height;

    SDL_GPUCopyPass* copy = SDL_BeginGPUCopyPass(g_cmd);
    SDL_DownloadFromGPUTexture(copy, &src, &dst);
    SDL_EndGPUCopyPass(copy);

    SDL_GPUFence* fence = SDL_SubmitGPUCommandBufferAndAcquireFence(g_cmd);
    g_cmd = SDL_AcquireGPUCommandBuffer(g_device);
    SDL_WaitForGPUFences(g_device, true, &fence, 1);
    SDL_ReleaseGPUFence(g_device, fence);

    void* mapped = SDL_MapGPUTransferBuffer(g_device, transfer, false);
    if (mapped != NULL)
    {
        memcpy(out_rgba, mapped, size);
        SDL_UnmapGPUTransferBuffer(g_device, transfer);
    }
    SDL_ReleaseGPUTransferBuffer(g_device, transfer);
    return mapped != NULL;
}

int MikuPan_GPUReadTextureR8(unsigned int texture_id, int size,
                             unsigned char* out_r8)
{
    GPUTextureEntry* entry = TextureEntry(texture_id);
    if (entry == NULL || out_r8 == NULL || size <= 0)
    {
        return 0;
    }

    const unsigned int byte_count = (unsigned int) size * (unsigned int) size;
    SDL_GPUTransferBufferCreateInfo info = {0};
    info.usage = SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD;
    info.size = byte_count;
    SDL_GPUTransferBuffer* transfer =
        SDL_CreateGPUTransferBuffer(g_device, &info);
    if (transfer == NULL)
    {
        return 0;
    }

    SDL_GPUTextureRegion src = {0};
    src.texture = entry->texture;
    src.w = (Uint32) size;
    src.h = (Uint32) size;
    src.d = 1;
    SDL_GPUTextureTransferInfo dst = {0};
    dst.transfer_buffer = transfer;
    dst.pixels_per_row = (Uint32) size;
    dst.rows_per_layer = (Uint32) size;

    /* Use a dedicated command buffer for the readback rather than the frame's
     * g_cmd. The swapchain is acquired lazily and bound to g_cmd (see
     * MikuPan_GPUSetTarget); submitting and swapping g_cmd here would leave
     * MikuPan_GPUEndFrame presenting a desynced swapchain and crash. A separate
     * buffer reads the texture's last-submitted contents (the previous frame's
     * shadow map), which is fine for a debug probe and leaves the frame intact. */
    SDL_GPUCommandBuffer* cmd = SDL_AcquireGPUCommandBuffer(g_device);
    if (cmd == NULL)
    {
        SDL_ReleaseGPUTransferBuffer(g_device, transfer);
        return 0;
    }

    SDL_GPUCopyPass* copy = SDL_BeginGPUCopyPass(cmd);
    SDL_DownloadFromGPUTexture(copy, &src, &dst);
    SDL_EndGPUCopyPass(copy);

    SDL_GPUFence* fence = SDL_SubmitGPUCommandBufferAndAcquireFence(cmd);
    if (fence != NULL)
    {
        SDL_WaitForGPUFences(g_device, true, &fence, 1);
        SDL_ReleaseGPUFence(g_device, fence);
    }

    void* mapped = SDL_MapGPUTransferBuffer(g_device, transfer, false);
    if (mapped != NULL)
    {
        memcpy(out_r8, mapped, byte_count);
        SDL_UnmapGPUTransferBuffer(g_device, transfer);
    }
    SDL_ReleaseGPUTransferBuffer(g_device, transfer);
    return mapped != NULL;
}

void MikuPan_GPUCopyTexture(unsigned int src_texture_id,
                            unsigned int dst_texture_id, int width, int height)
{
    GPUTextureEntry* src = TextureEntry(src_texture_id);
    GPUTextureEntry* dst = TextureEntry(dst_texture_id);
    if (src == NULL || dst == NULL || width <= 0 || height <= 0)
    {
        return;
    }

    if (src_texture_id == g_scene_texture_id)
    {
        MikuPan_GPUResolveSceneForSampling();
    }
    MikuPan_GPUFlushRenderPass();

    SDL_GPUTextureLocation src_loc = {0};
    src_loc.texture = src->texture;
    SDL_GPUTextureLocation dst_loc = {0};
    dst_loc.texture = dst->texture;

    SDL_GPUCopyPass* copy = SDL_BeginGPUCopyPass(g_cmd);
    SDL_CopyGPUTextureToTexture(copy, &src_loc, &dst_loc, (Uint32) width,
                                (Uint32) height, 1, true);
    SDL_EndGPUCopyPass(copy);
}

unsigned int MikuPan_GPURegisterVertexArray(int pipeline_type,
                                            unsigned int num_buffers,
                                            const unsigned int* buffers,
                                            unsigned int ibo)
{
    unsigned int id = AllocVaoId();
    if (id == 0)
    {
        return 0;
    }
    MikuPan_GPUUpdateVertexArrayBuffers(id, num_buffers, buffers, ibo);
    g_vaos[id].pipeline_type = pipeline_type + 1;
    return id;
}

void MikuPan_GPUUpdateVertexArrayBuffers(unsigned int vao,
                                         unsigned int num_buffers,
                                         const unsigned int* buffers,
                                         unsigned int ibo)
{
    if (vao == 0 || vao >= MIKUPAN_GPU_MAX_VAOS)
    {
        return;
    }
    if (num_buffers > 4)
    {
        num_buffers = 4;
    }
    g_vaos[vao].num_buffers = num_buffers;
    memset(g_vaos[vao].buffers, 0, sizeof(g_vaos[vao].buffers));
    for (unsigned int i = 0; i < num_buffers; i++)
    {
        g_vaos[vao].buffers[i] = buffers ? buffers[i] : 0;
    }
    g_vaos[vao].ibo = ibo;
}

void MikuPan_GPUBindVertexArray(unsigned int vao)
{
    g_bound_vao = vao;
}

void MikuPan_GPUReleaseVertexArray(unsigned int vao)
{
    if (vao == 0 || vao >= MIKUPAN_GPU_MAX_VAOS)
    {
        return;
    }
    /* pipeline_type == 0 marks the slot free for AllocVaoId to reclaim. */
    memset(&g_vaos[vao], 0, sizeof(g_vaos[vao]));
    if (vao < g_next_vao_id)
    {
        g_next_vao_id = vao;
    }
    if (g_bound_vao == vao)
    {
        g_bound_vao = 0;
    }
}

unsigned int MikuPan_GPUGetBoundVertexArray(void)
{
    return g_bound_vao;
}

void MikuPan_GPUSetCurrentShader(int shader_index)
{
    g_current_shader = shader_index;
}

int MikuPan_GPUGetCurrentShader(void)
{
    return g_current_shader;
}

void MikuPan_GPUInvalidatePipelines(void)
{
    for (int i = 0; i < MIKUPAN_GPU_MAX_PIPELINES; i++)
    {
        if (g_pipeline_cache[i].pipeline != NULL)
        {
            SDL_ReleaseGPUGraphicsPipeline(g_device,
                                           g_pipeline_cache[i].pipeline);
        }
        memset(&g_pipeline_cache[i], 0, sizeof(g_pipeline_cache[i]));
    }
    g_last_bound_pipeline = NULL;
}

static SDL_GPUGraphicsPipeline* GetPipeline(unsigned int primitive)
{
    if (g_current_shader < 0 || g_bound_vao == 0
        || g_bound_vao >= MIKUPAN_GPU_MAX_VAOS)
    {
        return NULL;
    }

    /* Key on the vertex-format type (shared by all VAOs of that type), not the
     * VAO instance — otherwise every mesh-cache entry spawns its own pipeline. */
    int pipeline_type = g_vaos[g_bound_vao].pipeline_type - 1;

    unsigned int state_hash = RenderStateHash();
    for (int i = 0; i < MIKUPAN_GPU_MAX_PIPELINES; i++)
    {
        GPUPipelineEntry* entry = &g_pipeline_cache[i];
        if (entry->used && entry->shader_index == g_current_shader
            && entry->pipeline_type == pipeline_type
            && entry->primitive == primitive && entry->state_hash == state_hash
            && entry->color_format == g_target_color_format
            && entry->depth_format == g_target_depth_format
            && entry->sample_count == g_target_sample_count)
        {
            return entry->pipeline;
        }
    }

    MikuPan_PipelineInfo* pipeline_info =
        MikuPan_GetPipelineInfo((enum MikuPan_PipelineType) pipeline_type);
    if (pipeline_info == NULL)
    {
        return NULL;
    }

    SDL_GPUVertexBufferDescription vb_desc[4] = {0};
    SDL_GPUVertexAttribute attrs[16] = {0};
    unsigned int attr_count = 0;
    for (unsigned int i = 0; i < pipeline_info->num_buffers && i < 4; i++)
    {
        MikuPan_BufferObjectInfo* bo = &pipeline_info->buffers[i];
        vb_desc[i].slot = i;
        vb_desc[i].pitch =
            bo->num_attributes > 0 ? (Uint32) bo->attributes[0].stride : 0;
        vb_desc[i].input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX;
        for (unsigned int j = 0; j < bo->num_attributes && attr_count < 16; j++)
        {
            MikuPan_AttributeInfo* ai = &bo->attributes[j];
            attrs[attr_count].location = ai->index;
            attrs[attr_count].buffer_slot = i;
            attrs[attr_count].format = VertexFormatFromSize(ai->size);
            attrs[attr_count].offset = ai->offset;
            attr_count++;
        }
    }

    SDL_GPUColorTargetBlendState blend = {0};
    blend.enable_blend = g_state.blend ? true : false;
    blend.src_color_blendfactor = SDL_GPU_BLENDFACTOR_SRC_ALPHA;
    blend.dst_color_blendfactor = g_state.additive_blend
                                      ? SDL_GPU_BLENDFACTOR_ONE
                                      : SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
    blend.color_blend_op = SDL_GPU_BLENDOP_ADD;
    /* Alpha channel: keep the destination (framebuffer) alpha opaque instead of
     * blending it like a colour. With SRC_ALPHA here, every semi-transparent 2D
     * sprite drives the framebuffer alpha below 1. D3D12 ignores swapchain alpha
     * so desktop looks fine, but Android/Vulkan SurfaceFlinger composites the
     * surface USING that alpha, making all 2D sprites/HUD translucent against
     * whatever is behind the window. ONE + ONE_MINUS_SRC_ALPHA (and ONE + ONE
     * for additive) leaves a cleared-to-1 target at alpha 1 everywhere. ImGui's
     * backend does the same, which is why its overlay stayed correct. RGB
     * (the visible colour) is unaffected on every backend. */
    blend.src_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE;
    blend.dst_alpha_blendfactor = g_state.additive_blend
                                      ? SDL_GPU_BLENDFACTOR_ONE
                                      : SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
    blend.alpha_blend_op = SDL_GPU_BLENDOP_ADD;
    blend.enable_color_write_mask = g_state.color_mask_enabled ? true : false;
    blend.color_write_mask = g_state.color_mask;

    SDL_GPUColorTargetDescription color_desc = {0};
    color_desc.format = g_target_color_format;
    color_desc.blend_state = blend;

    SDL_GPUGraphicsPipelineCreateInfo info = {0};
    info.vertex_shader = MikuPan_GetGPUVertexShader(g_current_shader);
    info.fragment_shader = MikuPan_GetGPUFragmentShader(g_current_shader);
    info.vertex_input_state.vertex_buffer_descriptions = vb_desc;
    info.vertex_input_state.num_vertex_buffers = pipeline_info->num_buffers;
    info.vertex_input_state.vertex_attributes = attrs;
    info.vertex_input_state.num_vertex_attributes = attr_count;
    info.primitive_type = PrimitiveFromGL(primitive);
    info.rasterizer_state.fill_mode = g_state.fill_mode;
    info.rasterizer_state.cull_mode = g_state.cull_mode;
    info.rasterizer_state.front_face = SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE;
    info.rasterizer_state.enable_depth_clip = true;
    info.rasterizer_state.enable_depth_bias = g_state.depth_bias ? true : false;
    info.rasterizer_state.depth_bias_constant_factor = -1.0f;
    info.rasterizer_state.depth_bias_slope_factor = -1.0f;
    info.multisample_state.sample_count =
        SampleCountFromMSAA(g_target_sample_count);
    info.depth_stencil_state.enable_depth_test =
        g_state.depth_test ? true : false;
    info.depth_stencil_state.enable_depth_write =
        g_state.depth_write ? true : false;
    info.depth_stencil_state.compare_op = g_state.compare;
    info.target_info.color_target_descriptions = &color_desc;
    info.target_info.num_color_targets = 1;
    info.target_info.has_depth_stencil_target =
        g_target_has_depth ? true : false;
    info.target_info.depth_stencil_format = g_target_depth_format;

    SDL_GPUGraphicsPipeline* pipeline =
        SDL_CreateGPUGraphicsPipeline(g_device, &info);
    if (pipeline == NULL)
    {
        info_log(
            "SDL_CreateGPUGraphicsPipeline failed: shader=%d vao=%u err=%s",
            g_current_shader, g_bound_vao, SDL_GetError());
        return NULL;
    }

    for (int i = 0; i < MIKUPAN_GPU_MAX_PIPELINES; i++)
    {
        if (!g_pipeline_cache[i].used)
        {
            g_pipeline_cache[i].used = 1;
            g_pipeline_cache[i].shader_index = g_current_shader;
            g_pipeline_cache[i].pipeline_type = pipeline_type;
            g_pipeline_cache[i].primitive = primitive;
            g_pipeline_cache[i].state_hash = state_hash;
            g_pipeline_cache[i].color_format = g_target_color_format;
            g_pipeline_cache[i].depth_format = g_target_depth_format;
            g_pipeline_cache[i].sample_count = g_target_sample_count;
            g_pipeline_cache[i].pipeline = pipeline;
            break;
        }
    }
    return pipeline;
}

static void BeginTargetPassIfNeeded(void)
{
    if (g_pass != NULL)
    {
        return;
    }

    if (!g_target_initialized)
    {
        MikuPan_GPUSetTarget(g_target, g_target_clear);
    }

    if (!CurrentTargetDrawable())
    {
        return;
    }

    SDL_GPUColorTargetInfo color = {0};
    const int resolving_scene = g_target_resolve != NULL
                                && g_scene_resolve_requested;
    color.texture = g_target_color;
    color.clear_color = (SDL_FColor) {0.0f, 0.0f, 0.0f, 1.0f};
    color.load_op = (!resolving_scene && g_target_clear)
                        ? SDL_GPU_LOADOP_CLEAR
                        : SDL_GPU_LOADOP_LOAD;
    color.store_op = SDL_GPU_STOREOP_STORE;
    color.cycle = false;

    if (resolving_scene)
    {
        color.store_op = g_scene_resolve_preserve_msaa
                             ? SDL_GPU_STOREOP_RESOLVE_AND_STORE
                             : SDL_GPU_STOREOP_RESOLVE;
        color.resolve_texture = g_target_resolve;
    }

    /* A depth-only clear (mirror reflection prep) must NOT wipe colour. */
    const int clear_depth =
        !resolving_scene && (g_target_clear || g_target_clear_depth);

    SDL_GPUDepthStencilTargetInfo depth = {0};
    SDL_GPUDepthStencilTargetInfo* depth_ptr = NULL;
    if (!resolving_scene && g_target_has_depth && g_target_depth != NULL)
    {
        depth.texture = g_target_depth;
        depth.clear_depth = 1.0f;
        depth.load_op =
            clear_depth ? SDL_GPU_LOADOP_CLEAR : SDL_GPU_LOADOP_LOAD;
        depth.store_op = SDL_GPU_STOREOP_STORE;
        depth.stencil_load_op =
            clear_depth ? SDL_GPU_LOADOP_CLEAR : SDL_GPU_LOADOP_LOAD;
        depth.stencil_store_op = SDL_GPU_STOREOP_DONT_CARE;
        depth.clear_stencil = 0;
        depth_ptr = &depth;
    }

    g_pass = SDL_BeginGPURenderPass(g_cmd, &color, 1, depth_ptr);
    g_pass_resolves_scene = resolving_scene && g_pass != NULL;

    if (g_pass == NULL)
    {
        info_log("SDL_BeginGPURenderPass failed: %s", SDL_GetError());
        return;
    }

    if (g_pass != NULL && g_target == MIKUPAN_GPU_TARGET_SCENE
        && g_target_resolve != NULL && !resolving_scene)
    {
        g_scene_resolve_dirty = 1;
    }

    g_scene_resolve_requested = 0;
    g_scene_resolve_preserve_msaa = 1;
    g_target_clear = 0;
    g_target_clear_depth = 0;

    /* New pass: pipeline binding is reset by SDL_GPU, and re-pushing the
     * shared uniform blocks here keeps them valid regardless of command-buffer
     * boundaries. */
    g_last_bound_pipeline = NULL;
    g_light_uniform_dirty = 1;
    g_material_uniform_dirty = 1;
    g_vertex_uniform_dirty = 1;
    g_fragment_uniform_dirty = 1;
    g_vertex_binding_valid = 0;
    g_sampler_binding_valid = 0;
    g_index_binding_valid = 0;

    SDL_GPUViewport vp = {(float) g_viewport[0],
                          (float) g_viewport[1],
                          (float) g_viewport[2],
                          (float) g_viewport[3],
                          0.0f,
                          1.0f};

    SDL_SetGPUViewport(g_pass, &vp);

    if (g_scissor_enabled)
    {
        SDL_SetGPUScissor(g_pass, &g_scissor);
    }
}

void MikuPan_GPUBeginRenderPass(void)
{
    BeginTargetPassIfNeeded();
}

static void ResetColorWriteMask(void)
{
    g_state.color_mask = SDL_GPU_COLORCOMPONENT_R | SDL_GPU_COLORCOMPONENT_G
                         | SDL_GPU_COLORCOMPONENT_B | SDL_GPU_COLORCOMPONENT_A;
    g_state.color_mask_enabled = 0;
}

void MikuPan_GPUSetRenderState3D(void)
{
    g_state.depth_test = 1;
    g_state.depth_write = 1;
    g_state.compare = SDL_GPU_COMPAREOP_LESS_OR_EQUAL;
    g_state.blend = 1;
    g_state.additive_blend = 0;
    g_state.cull_mode = SDL_GPU_CULLMODE_BACK;
    g_state.fill_mode = SDL_GPU_FILLMODE_FILL;
    g_state.depth_bias = 0;
    ResetColorWriteMask();
}

void MikuPan_GPUSetRenderState3DMirror(void)
{
    MikuPan_GPUSetRenderState3D();
    g_state.cull_mode = SDL_GPU_CULLMODE_FRONT;
}

void MikuPan_GPUSetRenderState2D(void)
{
    g_state.depth_test = 0;
    g_state.depth_write = 0;
    g_state.blend = 1;
    g_state.additive_blend = 0;
    g_state.cull_mode = SDL_GPU_CULLMODE_NONE;
    g_state.fill_mode = SDL_GPU_FILLMODE_FILL;
    g_state.depth_bias = 0;
    ResetColorWriteMask();
}

void MikuPan_GPUSetRenderState2DDepth(void)
{
    g_state.depth_test = 1;
    g_state.depth_write = 1;
    g_state.compare = SDL_GPU_COMPAREOP_LESS_OR_EQUAL;
    g_state.blend = 1;
    g_state.additive_blend = 0;
    g_state.cull_mode = SDL_GPU_CULLMODE_NONE;
    g_state.fill_mode = SDL_GPU_FILLMODE_FILL;
    g_state.depth_bias = 0;
    ResetColorWriteMask();
}

void MikuPan_GPUSetRenderStateSprite3D(void)
{
    g_state.depth_test = 1;
    g_state.depth_write = 1;
    g_state.compare = SDL_GPU_COMPAREOP_LESS_OR_EQUAL;
    g_state.blend = 1;
    g_state.additive_blend = 0;
    g_state.cull_mode = SDL_GPU_CULLMODE_NONE;
    g_state.fill_mode = SDL_GPU_FILLMODE_FILL;
    g_state.depth_bias = 0;
    ResetColorWriteMask();
}

void MikuPan_GPUSetRenderStateShadow(void)
{
    g_state.depth_test = 0;
    g_state.depth_write = 0;
    g_state.blend = 0;
    g_state.additive_blend = 0;
    g_state.cull_mode = SDL_GPU_CULLMODE_NONE;
    g_state.fill_mode = SDL_GPU_FILLMODE_FILL;
    g_state.depth_bias = 0;
    ResetColorWriteMask();
}

void MikuPan_GPUSetRenderStateShadowReceiver(void)
{
    g_state.depth_test = 1;
    g_state.depth_write = 0;
    g_state.compare = SDL_GPU_COMPAREOP_LESS_OR_EQUAL;
    g_state.blend = 1;
    g_state.additive_blend = 0;
    g_state.cull_mode = SDL_GPU_CULLMODE_NONE;
    g_state.fill_mode = SDL_GPU_FILLMODE_FILL;
    g_state.depth_bias = 1;
    ResetColorWriteMask();
}

void MikuPan_GPUSetDepthWrite(int enabled)
{
    g_state.depth_write = enabled ? 1 : 0;
}
void MikuPan_GPUSetDepthFunc(unsigned int gl_func)
{
    g_state.compare = CompareFromGL(gl_func);
}
void MikuPan_GPUSetBlend(int enabled, int additive)
{
    g_state.blend = enabled ? 1 : 0;
    g_state.additive_blend = additive ? 1 : 0;
}
void MikuPan_GPUSetCullBack(void)
{
    g_state.cull_mode = SDL_GPU_CULLMODE_BACK;
}
void MikuPan_GPUSetCullFront(void)
{
    g_state.cull_mode = SDL_GPU_CULLMODE_FRONT;
}
void MikuPan_GPUSetCullNone(void)
{
    g_state.cull_mode = SDL_GPU_CULLMODE_NONE;
}
void MikuPan_GPUSetColorWriteMask(int r, int g, int b, int a)
{
    g_state.color_mask = 0;

    if (r)
    {
        g_state.color_mask |= SDL_GPU_COLORCOMPONENT_R;
    }

    if (g)
    {
        g_state.color_mask |= SDL_GPU_COLORCOMPONENT_G;
    }

    if (b)
    {
        g_state.color_mask |= SDL_GPU_COLORCOMPONENT_B;
    }

    if (a)
    {
        g_state.color_mask |= SDL_GPU_COLORCOMPONENT_A;
    }

    g_state.color_mask_enabled = 1;
}
void MikuPan_GPUSetFillLine(int enabled)
{
    g_state.fill_mode = enabled ? SDL_GPU_FILLMODE_LINE : SDL_GPU_FILLMODE_FILL;
}

void MikuPan_GPUSetViewport(int x, int y, int w, int h)
{
    g_viewport[0] = x;
    g_viewport[1] = y;
    g_viewport[2] = w;
    g_viewport[3] = h;

    if (g_pass != NULL)
    {
        SDL_GPUViewport vp = {(float) x, (float) y, (float) w,
                              (float) h, 0.0f,      1.0f};
        SDL_SetGPUViewport(g_pass, &vp);
    }
}

void MikuPan_GPUSetScissor(int x, int y, int w, int h)
{
    g_scissor = (SDL_Rect) {x, y, w, h};
    g_scissor_enabled = 1;

    if (g_pass != NULL)
    {
        SDL_SetGPUScissor(g_pass, &g_scissor);
    }
}

void MikuPan_GPUDisableScissor(void)
{
    g_scissor_enabled = 0;

    if (g_pass != NULL)
    {
        SDL_Rect sc = {0, 0, g_target_width, g_target_height};
        SDL_SetGPUScissor(g_pass, &sc);
    }
}

void MikuPan_GPUClearDepth(void)
{
    MikuPan_GPUFlushRenderPass();
    /* Depth/stencil only — leave the colour buffer (and the mirror reflection
     * already rendered into it) intact. */
    g_target_clear_depth = 1;
}

void MikuPan_GPUSetTarget(MikuPan_GPUTarget target, int clear)
{
    MikuPan_GPUFlushRenderPass();
    g_target = target;
    g_target_clear = clear;
    g_target_initialized = 1;
    g_target_has_depth = 0;
    g_target_depth = NULL;
    g_target_depth_format = SDL_GPU_TEXTUREFORMAT_INVALID;
    g_target_resolve = NULL;
    g_target_sample_count = 1;

    if (target == MIKUPAN_GPU_TARGET_WINDOW)
    {
        Uint32 swapchain_width = 0;
        Uint32 swapchain_height = 0;

        g_target_color = NULL;
        g_target_color_format = g_swapchain_format;
        g_target_width = 0;
        g_target_height = 0;

        if (g_cmd == NULL || g_window == NULL)
        {
            return;
        }

        if (g_swapchain == NULL)
        {
            if (!SDL_WaitAndAcquireGPUSwapchainTexture(g_cmd, g_window,
                                                       &g_swapchain,
                                                       &swapchain_width,
                                                       &swapchain_height))
            {
                info_log("SDL_WaitAndAcquireGPUSwapchainTexture failed: %s",
                         SDL_GetError());
                return;
            }
        }

        g_target_color = g_swapchain;
        if (swapchain_width > 0 && swapchain_height > 0)
        {
            g_target_width = (int)swapchain_width;
            g_target_height = (int)swapchain_height;
        }
        else if (g_target_color != NULL)
        {
            SDL_GetWindowSizeInPixels(g_window, &g_target_width,
                                      &g_target_height);
        }
    }
    else
    {
        GPUTextureEntry* resolve = TextureEntry(g_scene_texture_id);
        GPUTextureEntry* depth = TextureEntry(g_scene_depth_id);

        if (g_scene_msaa > 1)
        {
            /* Render into the multisampled colour target and resolve into the
             * single-sample scene texture. */
            GPUTextureEntry* msaa = TextureEntry(g_scene_msaa_color_id);
            g_target_color =
                msaa ? msaa->texture : (resolve ? resolve->texture : NULL);
            g_target_resolve = resolve ? resolve->texture : NULL;
        }
        else
        {
            g_target_color = resolve ? resolve->texture : NULL;
        }

        g_target_sample_count = g_scene_msaa;
        g_target_depth = depth ? depth->texture : NULL;
        g_target_color_format =
            resolve ? resolve->format : SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
        g_target_depth_format = depth ? depth->format : g_depth_format;
        g_target_width = resolve ? resolve->width : 1;
        g_target_height = resolve ? resolve->height : 1;
        g_target_has_depth = 1;
    }
}

void MikuPan_GPUSetShadowTarget(unsigned int texture_id, int size, int clear)
{
    MikuPan_GPUFlushRenderPass();
    GPUTextureEntry* tex = TextureEntry(texture_id);
    g_target = MIKUPAN_GPU_TARGET_SHADOW;
    g_target_color = tex ? tex->texture : NULL;
    g_target_color_format = tex ? tex->format : SDL_GPU_TEXTUREFORMAT_R8_UNORM;
    g_target_width = size;
    g_target_height = size;
    g_target_has_depth = 0;
    g_target_depth = NULL;
    g_target_depth_format = SDL_GPU_TEXTUREFORMAT_INVALID;
    g_target_resolve = NULL;
    g_target_sample_count = 1;
    g_target_clear = clear;
    g_target_initialized = 1;
}

void MikuPan_GPUSetScreenCopyTarget(unsigned int texture_id, int width,
                                    int height, int clear)
{
    MikuPan_GPUFlushRenderPass();
    GPUTextureEntry* tex = TextureEntry(texture_id);
    g_target = MIKUPAN_GPU_TARGET_SCREEN_COPY;
    g_target_color = tex ? tex->texture : NULL;
    g_target_color_format =
        tex ? tex->format : SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
    g_target_width = width;
    g_target_height = height;
    g_target_has_depth = 0;
    g_target_depth = NULL;
    g_target_depth_format = SDL_GPU_TEXTUREFORMAT_INVALID;
    g_target_resolve = NULL;
    g_target_sample_count = 1;
    g_target_clear = clear;
    g_target_initialized = 1;
}

void MikuPan_GPUBindTextureSlot(int slot, unsigned int texture_id)
{
    if (slot < 0 || slot > 1)
    {
        return;
    }

    g_bound_textures[slot] = texture_id;
}

unsigned int MikuPan_GPUGetBoundTexture0(void)
{
    return g_bound_textures[0];
}

SDL_GPUTexture* MikuPan_GPUGetTextureHandle(unsigned int id)
{
    GPUTextureEntry* entry = TextureEntry(id);
    return entry ? entry->texture : NULL;
}

static void BindDrawState(unsigned int primitive)
{
    BeginTargetPassIfNeeded();
    if (g_pass == NULL)
    {
        return;
    }

    SDL_GPUGraphicsPipeline* pipeline = GetPipeline(primitive);
    if (pipeline == NULL)
    {
        return;
    }
    if (pipeline != g_last_bound_pipeline)
    {
        SDL_BindGPUGraphicsPipeline(g_pass, pipeline);
        g_last_bound_pipeline = pipeline;
    }

    /* Skinned draws supply a bone-matrix palette in vertex storage slot 0. The
     * render path sets this only for skinned meshes and clears it afterwards,
     * so non-skinned pipelines (which declare zero storage buffers) never have
     * one bound. */
    if (g_vertex_storage_buffer != NULL)
    {
        SDL_BindGPUVertexStorageBuffers(g_pass, 0, &g_vertex_storage_buffer, 1);
    }

    /* Slot 0 holds per-object matrices (vertex) plus colours/flags/fog
     * (fragment). The vertex matrices change every mesh, but the fragment-read
     * fields usually do not, so push each stage only when its data changed. */
    if (g_vertex_uniform_dirty)
    {
        SDL_PushGPUVertexUniformData(g_cmd, 0, &g_uniforms, sizeof(g_uniforms));
        g_vertex_uniform_dirty = 0;
    }

    if (g_fragment_uniform_dirty)
    {
        SDL_PushGPUFragmentUniformData(g_cmd, 0, &g_uniforms,
                                       sizeof(g_uniforms));
        g_fragment_uniform_dirty = 0;
    }

    /* Slots 1 (lights) and 2 (material) persist across draws; only re-push
     * when their contents changed or a new pass re-armed them. */
    if (g_light_uniform_dirty)
    {
        SDL_PushGPUVertexUniformData(g_cmd, 1, &g_light_data,
                                     sizeof(g_light_data));
        SDL_PushGPUFragmentUniformData(g_cmd, 1, &g_light_data,
                                       sizeof(g_light_data));
        g_light_uniform_dirty = 0;
    }

    if (g_material_uniform_dirty)
    {
        SDL_PushGPUVertexUniformData(g_cmd, 2, &g_material_data,
                                     sizeof(g_material_data));
        SDL_PushGPUFragmentUniformData(g_cmd, 2, &g_material_data,
                                       sizeof(g_material_data));
        g_material_uniform_dirty = 0;
    }

    GPUVaoEntry* vao = &g_vaos[g_bound_vao];
    SDL_GPUBufferBinding bindings[4] = {0};
    unsigned int binding_count = 0;

    for (unsigned int i = 0; i < vao->num_buffers && i < 4; i++)
    {
        GPUBufferEntry* bo = BufferEntry(vao->buffers[i]);
        if (bo != NULL && bo->buffer != NULL)
        {
            bindings[binding_count].buffer = bo->buffer;
            bindings[binding_count].offset = 0;
            binding_count++;
        }
    }

    /* Skip the re-bind when the buffer set is identical to the last draw's.
     * Compare handles (not just the VAO id) so a buffer that grew — and was
     * recreated with a new handle — is correctly re-bound. */
    if (binding_count > 0)
    {
        int changed = !g_vertex_binding_valid
                      || binding_count != g_last_vertex_buffer_count;
        for (unsigned int i = 0; !changed && i < binding_count; i++)
        {
            if (bindings[i].buffer != g_last_vertex_buffers[i])
            {
                changed = 1;
            }
        }
        if (changed)
        {
            SDL_BindGPUVertexBuffers(g_pass, 0, bindings, binding_count);
            for (unsigned int i = 0; i < binding_count; i++)
            {
                g_last_vertex_buffers[i] = bindings[i].buffer;
            }
            g_last_vertex_buffer_count = binding_count;
            g_vertex_binding_valid = 1;
        }
    }

    // Every fragment shader is created with num_samplers = 2 (uTexture +
    // uAuxTexture), so SDL_GPU requires exactly two fragment samplers bound at
    // every draw. Fill any unbound slot with the 1x1 white fallback so we never
    // trip "Missing fragment sampler binding!".
    SDL_GPUTextureSamplerBinding samplers[2] = {0};
    for (int i = 0; i < 2; i++)
    {
        GPUTextureEntry* tex = TextureEntry(g_bound_textures[i]);
        if (tex != NULL && tex->texture != NULL && tex->sampler != NULL)
        {
            samplers[i].texture = tex->texture;
            samplers[i].sampler = tex->sampler;
        }
        else
        {
            samplers[i].texture = g_fallback_texture;
            samplers[i].sampler = g_fallback_sampler;
        }
    }

    /* Skip the re-bind when both sampler slots are unchanged from the last draw. */
    {
        int changed = !g_sampler_binding_valid;
        for (int i = 0; !changed && i < 2; i++)
        {
            if (samplers[i].texture != g_last_sampler_textures[i]
                || samplers[i].sampler != g_last_sampler_samplers[i])
            {
                changed = 1;
            }
        }
        if (changed)
        {
            SDL_BindGPUFragmentSamplers(g_pass, 0, samplers, 2);
            for (int i = 0; i < 2; i++)
            {
                g_last_sampler_textures[i] = samplers[i].texture;
                g_last_sampler_samplers[i] = samplers[i].sampler;
            }
            g_sampler_binding_valid = 1;
        }
    }
}

void MikuPan_GPUDrawArrays(unsigned int gl_mode, int first, int count)
{
    if (count <= 0)
    {
        return;
    }

    BindDrawState(gl_mode);

    if (g_pass != NULL)
    {
        SDL_DrawGPUPrimitives(g_pass, (Uint32) count, 1, (Uint32) first, 0);
    }
}

void MikuPan_GPUDrawElements(unsigned int gl_mode, int count,
                             unsigned int gl_index_type, const void* indices)
{
    (void) indices;

    if (count <= 0 || g_bound_vao == 0)
    {
        return;
    }

    BindDrawState(gl_mode);
    GPUVaoEntry* vao = &g_vaos[g_bound_vao];
    GPUBufferEntry* ibo = BufferEntry(vao->ibo);

    if (g_pass != NULL && ibo != NULL && ibo->buffer != NULL)
    {
        SDL_GPUIndexElementSize index_size =
            gl_index_type == GL_UNSIGNED_SHORT ? SDL_GPU_INDEXELEMENTSIZE_16BIT
                                               : SDL_GPU_INDEXELEMENTSIZE_32BIT;
        /* Index binding persists across draws in a pass; skip the re-bind when
         * the same index buffer + element size is already bound. */
        if (!g_index_binding_valid || ibo->buffer != g_last_index_buffer
            || index_size != g_last_index_size)
        {
            SDL_GPUBufferBinding binding = {0};
            binding.buffer = ibo->buffer;
            binding.offset = 0;
            SDL_BindGPUIndexBuffer(g_pass, &binding, index_size);
            g_last_index_buffer = ibo->buffer;
            g_last_index_size = index_size;
            g_index_binding_valid = 1;
        }

        SDL_DrawGPUIndexedPrimitives(g_pass, (Uint32) count, 1, 0, 0, 0);
    }
}

MikuPan_GPUUniformBlock* MikuPan_GPUUniforms(void)
{
    return &g_uniforms;
}
MikuPan_LightData* MikuPan_GPULightData(void)
{
    return &g_light_data;
}
MikuPan_MaterialData* MikuPan_GPUMaterialData(void)
{
    return &g_material_data;
}

static void CopyMatrix3Std140(float out[12], const float* mat)
{
    out[0] = mat[0];
    out[1] = mat[1];
    out[2] = mat[2];
    out[3] = 0.0f;
    out[4] = mat[3];
    out[5] = mat[4];
    out[6] = mat[5];
    out[7] = 0.0f;
    out[8] = mat[6];
    out[9] = mat[7];
    out[10] = mat[8];
    out[11] = 0.0f;
}

void MikuPan_GPUSetMatrix4(const char* name, const float* mat)
{
    if (strcmp(name, "model") == 0)
    {
        memcpy(g_uniforms.model, mat, sizeof(g_uniforms.model));
    }
    else if (strcmp(name, "view") == 0)
    {
        memcpy(g_uniforms.view, mat, sizeof(g_uniforms.view));
    }
    else if (strcmp(name, "projection") == 0)
    {
        memcpy(g_uniforms.projection, mat, sizeof(g_uniforms.projection));
    }
    else if (strcmp(name, "mvp") == 0)
    {
        memcpy(g_uniforms.mvp, mat, sizeof(g_uniforms.mvp));
    }
    else if (strcmp(name, "modelView") == 0)
    {
        memcpy(g_uniforms.modelView, mat, sizeof(g_uniforms.modelView));
    }
    else if (strcmp(name, "viewProj") == 0)
    {
        memcpy(g_uniforms.viewProj, mat, sizeof(g_uniforms.viewProj));
    }
    else if (strcmp(name, "uShadowMatrix") == 0)
    {
        memcpy(g_uniforms.uShadowMatrix, mat, sizeof(g_uniforms.uShadowMatrix));
        g_fragment_uniform_dirty = 1;
    }
    else if (strcmp(name, "uWorldClipView") == 0)
    {
        memcpy(g_uniforms.uWorldClipView, mat,
               sizeof(g_uniforms.uWorldClipView));
    }

    /* All matrices feed the vertex stage; uShadowMatrix additionally feeds the
     * lighting fragment shader (handled above). */
    g_vertex_uniform_dirty = 1;
}

void MikuPan_GPUSetMatrix3(const char* name, const float* mat)
{
    if (strcmp(name, "normalMatrix") == 0)
    {
        CopyMatrix3Std140(g_uniforms.normalMatrix, mat);
    }
    else if (strcmp(name, "viewNormalMatrix") == 0)
    {
        CopyMatrix3Std140(g_uniforms.viewNormalMatrix, mat);
    }

    g_vertex_uniform_dirty = 1;
}

void MikuPan_GPUSetVec4(const char* name, const float* vec)
{
    if (strcmp(name, "uColor") == 0)
    {
        memcpy(g_uniforms.uColor, vec, sizeof(g_uniforms.uColor));
    }
    else if (strcmp(name, "uFog") == 0)
    {
        memcpy(g_uniforms.uFog, vec, sizeof(g_uniforms.uFog));
    }
    else if (strcmp(name, "uFogColor") == 0)
    {
        memcpy(g_uniforms.uFogColor, vec, sizeof(g_uniforms.uFogColor));
    }
    else if (strcmp(name, "uPhotoNegativeContentRect") == 0)
    {
        memcpy(g_uniforms.uPhotoNegativeContentRect, vec,
               sizeof(g_uniforms.uPhotoNegativeContentRect));
    }
    else if (strcmp(name, "uPhotoNegativeRect") == 0)
    {
        memcpy(g_uniforms.uPhotoNegativeRect, vec,
               sizeof(g_uniforms.uPhotoNegativeRect));
    }

    g_vertex_uniform_dirty = 1;
    g_fragment_uniform_dirty = 1;
}

void MikuPan_GPUSetInt(const char* name, int value)
{
    if (strcmp(name, "renderNormals") == 0)
    {
        g_uniforms.uFlags0[0] = value;
    }
    else if (strcmp(name, "disableLighting") == 0)
    {
        g_uniforms.uFlags0[1] = value;
    }
    else if (strcmp(name, "staticLighting") == 0)
    {
        g_uniforms.uFlags0[2] = value;
    }
    else if (strcmp(name, "uMeshLightingMode") == 0)
    {
        g_uniforms.uFlags0[3] = value;
    }
    else if (strcmp(name, "uMirrorSurfacePass") == 0)
    {
        g_uniforms.uFlags1[0] = value;
    }
    else if (strcmp(name, "uShadowEnabled") == 0)
    {
        g_uniforms.uFlags1[1] = value;
    }
    else if (strcmp(name, "uShadowDebugView") == 0)
    {
        g_uniforms.uFlags1[2] = value;
    }
    else if (strcmp(name, "uCrtEnabled") == 0)
    {
        g_uniforms.uFlags1[3] = value;
    }
    else if (strcmp(name, "uBlackWhiteMode") == 0)
    {
        g_uniforms.uFlags2[0] = value;
    }
    else if (strcmp(name, "uPhotoNegativeEnabled") == 0)
    {
        g_uniforms.uFlags2[1] = value;
    }
    else if (strcmp(name, "uPhotoNegativeSourceEnabled") == 0)
    {
        g_uniforms.uFlags2[2] = value;
    }
    else if (strcmp(name, "uUseScreenPos") == 0)
    {
        g_uniforms.uFlags2[3] = value;
    }

    (void) name;
    g_vertex_uniform_dirty = 1;
    g_fragment_uniform_dirty = 1;
}

void MikuPan_GPUSetFloat(const char* name, float value)
{
    if (strcmp(name, "uNormalLength") == 0)
    {
        g_uniforms.uParams0[0] = value;
    }
    else if (strcmp(name, "uShadowStrength") == 0)
    {
        g_uniforms.uParams0[1] = value;
    }
    else if (strcmp(name, "uBrightness") == 0)
    {
        g_uniforms.uParams0[2] = value;
    }
    else if (strcmp(name, "uGamma") == 0)
    {
        g_uniforms.uParams0[3] = value;
    }
    else if (strcmp(name, "uCrtStrength") == 0)
    {
        g_uniforms.uCrt0[0] = value;
    }
    else if (strcmp(name, "uCrtCurvature") == 0)
    {
        g_uniforms.uCrt0[1] = value;
    }
    else if (strcmp(name, "uCrtOverscan") == 0)
    {
        g_uniforms.uCrt0[2] = value;
    }
    else if (strcmp(name, "uCrtScanlineStrength") == 0)
    {
        g_uniforms.uCrt0[3] = value;
    }
    else if (strcmp(name, "uCrtScanlineScale") == 0)
    {
        g_uniforms.uCrt1[0] = value;
    }
    else if (strcmp(name, "uCrtScanlineThickness") == 0)
    {
        g_uniforms.uCrt1[1] = value;
    }
    else if (strcmp(name, "uCrtMaskStrength") == 0)
    {
        g_uniforms.uCrt1[2] = value;
    }
    else if (strcmp(name, "uCrtMaskScale") == 0)
    {
        g_uniforms.uCrt1[3] = value;
    }
    else if (strcmp(name, "uCrtVignetteStrength") == 0)
    {
        g_uniforms.uCrt2[0] = value;
    }
    else if (strcmp(name, "uCrtVignetteSize") == 0)
    {
        g_uniforms.uCrt2[1] = value;
    }
    else if (strcmp(name, "uCrtChromaOffset") == 0)
    {
        g_uniforms.uCrt2[2] = value;
    }
    else if (strcmp(name, "uCrtBlendStrength") == 0)
    {
        g_uniforms.uCrt2[3] = value;
    }
    else if (strcmp(name, "uCrtBlendRadius") == 0)
    {
        g_uniforms.uCrt3[0] = value;
    }
    else if (strcmp(name, "uCrtNoiseStrength") == 0)
    {
        g_uniforms.uCrt3[1] = value;
    }
    else if (strcmp(name, "uCrtFlickerStrength") == 0)
    {
        g_uniforms.uCrt3[2] = value;
    }
    else if (strcmp(name, "uCrtGlowStrength") == 0)
    {
        g_uniforms.uCrt3[3] = value;
    }
    else if (strcmp(name, "uTime") == 0)
    {
        g_uniforms.uParams1[0] = value;
    }
    else if (strcmp(name, "uPhotoNegativeStrength") == 0)
    {
        g_uniforms.uParams1[1] = value;
    }

    g_vertex_uniform_dirty = 1;
    g_fragment_uniform_dirty = 1;
}

void MikuPan_GPUSetVec2(const char* name, float x, float y)
{
    if (strcmp(name, "uTextureSize") == 0)
    {
        g_uniforms.uTextureSize[0] = x;
        g_uniforms.uTextureSize[1] = y;
    }
    else if (strcmp(name, "uOutputSize") == 0)
    {
        g_uniforms.uOutputSize[0] = x;
        g_uniforms.uOutputSize[1] = y;
    }
    else if (strcmp(name, "uShadowSize") == 0)
    {
        g_uniforms.uShadowSize[0] = x;
        g_uniforms.uShadowSize[1] = y;
    }
    else if (strcmp(name, "uFramebufferUvOffset") == 0)
    {
        g_uniforms.uFramebufferUvOffset[0] = x;
        g_uniforms.uFramebufferUvOffset[1] = y;
    }
    else if (strcmp(name, "uFramebufferUvScale") == 0)
    {
        g_uniforms.uFramebufferUvScale[0] = x;
        g_uniforms.uFramebufferUvScale[1] = y;
    }
    else if (strcmp(name, "uFramebufferContentUvMax") == 0)
    {
        g_uniforms.uFramebufferContentUvMax[0] = x;
        g_uniforms.uFramebufferContentUvMax[1] = y;
    }
    else if (strcmp(name, "uRenderSize") == 0)
    {
        g_uniforms.uRenderSize[0] = x;
        g_uniforms.uRenderSize[1] = y;
    }

    g_vertex_uniform_dirty = 1;
    g_fragment_uniform_dirty = 1;
}
