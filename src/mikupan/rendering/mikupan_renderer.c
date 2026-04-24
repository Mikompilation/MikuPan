#include "mikupan_renderer.h"
#include "../mikupan_types.h"
#include "SDL3/SDL_hints.h"
#include "SDL3/SDL_init.h"
#include "cglm/cglm.h"
#include "graphics/graph2d/message.h"
#include "graphics/graph3d/sgsu.h"
#include "mikupan/gs/mikupan_gs_c.h"
#include "mikupan/gs/mikupan_texture_manager_c.h"
#include "mikupan/mikupan_logging_c.h"
#include "mikupan/ui/mikupan_ui_c.h"
#include "mikupan_shader.h"
#include "mikupan_pipeline.h"
#include <stdlib.h>

#include "graphics/graph3d/sglib.h"
#include "main/glob.h"
#include "mikupan/mikupan_utils.h"

// ─────────────────────────────────────────────────────────────────────────────
// Frame-local scratch buffers (CPU side, same sizes as before)
// ─────────────────────────────────────────────────────────────────────────────

static int   s_vertex_index[1024 * 1024];
static float s_temp_render_buffer[1024 * 1024];

static int  s_state_changes = 0;
static int  s_draw_calls    = 0;
static bool s_mirror_mode   = false;

static bool MikuPan_IsMirrorRendering(void) { return s_mirror_mode; }

// ─────────────────────────────────────────────────────────────────────────────
// Renderer state
// ─────────────────────────────────────────────────────────────────────────────

MikuPan_RenderWindow mikupan_render = {0};

static MikuPan_TextureInfo *s_fnt_texture[6]   = {0};
static MikuPan_TextureInfo *s_curr_fnt_texture = NULL;

// View / projection matrices (cglm column-major)
static mat4 WorldClipView = {0};
static mat4 WorldView     = {0};
static mat4 projection    = {0};
static mat4 ViewClip      = {0};
static mat4 WorldClip     = {0};

static MikuPan_LightData s_light_data = {0};

// Internal render target (PS2-resolution offscreen texture)
static SDL_GPUTexture *s_color_tex  = NULL;
static SDL_GPUTexture *s_depth_tex  = NULL;
static SDL_GPUTexture *s_resolve_tex = NULL; // MSAA resolve target
static int  s_render_w  = 640;
static int  s_render_h  = 448;
static int  s_msaa_r    = 0;    // 0 = no MSAA, 2/4/8 = sample count

// SDL_GPU per-frame objects
static SDL_GPUCommandBuffer  *s_cmd_buf     = NULL;
static SDL_GPURenderPass     *s_render_pass = NULL;

// Large streaming transfer buffer (32 MB) for CPU→GPU vertex / index uploads
#define TRANSFER_BUF_SIZE (32 * 1024 * 1024)
static SDL_GPUTransferBuffer *s_transfer_buf  = NULL;
static void                  *s_transfer_ptr  = NULL; // mapped pointer
static Uint32                 s_transfer_used = 0;

// ─────────────────────────────────────────────────────────────────────────────
// Transfer buffer helpers
// ─────────────────────────────────────────────────────────────────────────────

// Allocate a region in the mapped transfer buffer; returns offset into it.
// Returns UINT32_MAX on overflow (should not happen with 32 MB budget).
static Uint32 transfer_alloc(Uint32 size, Uint32 alignment)
{
    Uint32 aligned = (s_transfer_used + alignment - 1) & ~(alignment - 1);
    if (aligned + size > TRANSFER_BUF_SIZE)
    {
        info_log("SDL_GPU: transfer buffer overflow!");
        return UINT32_MAX;
    }
    s_transfer_used = aligned + size;
    return aligned;
}

// Copy data into the transfer buffer and record a GPU buffer copy command.
// Must be called before the render pass begins (i.e., inside MikuPan_Clear or
// before the first draw in EndFrame).
static void upload_to_gpu_buffer(SDL_GPUBuffer *dst, Uint32 dst_offset,
                                 const void *src, Uint32 size)
{
    if (!dst || size == 0) return;

    Uint32 src_offset = transfer_alloc(size, 4);
    if (src_offset == UINT32_MAX) return;

    SDL_memcpy((Uint8 *)s_transfer_ptr + src_offset, src, size);

    // Copy commands must run in a copy pass (before the render pass).
    // We batch them all in MikuPan_FlushCopyPass called from EndFrame.
    // For simplicity, store them in a pending list — here we just write to
    // the transfer buffer and rely on EndFrame to do the copy pass.
}

// ─────────────────────────────────────────────────────────────────────────────
// Pending upload queue
// All glBufferSubData-style uploads are queued here during game logic and
// executed in a single copy pass at the start of EndFrame.
// ─────────────────────────────────────────────────────────────────────────────

#define MAX_PENDING_UPLOADS 256

typedef struct
{
    SDL_GPUBuffer *dst;
    Uint32         dst_offset;
    Uint32         src_offset; // into s_transfer_buf
    Uint32         size;
    bool           is_index;   // use INDEX buffer binding in copy pass
} PendingUpload;

static PendingUpload s_pending_uploads[MAX_PENDING_UPLOADS];
static int           s_num_pending = 0;

static void queue_upload(SDL_GPUBuffer *dst, Uint32 dst_offset,
                         const void *src, Uint32 size, bool is_index)
{
    if (!dst || size == 0 || s_num_pending >= MAX_PENDING_UPLOADS) return;

    Uint32 src_offset = transfer_alloc(size, 4);
    if (src_offset == UINT32_MAX) return;

    if (s_transfer_ptr == NULL) return;

    SDL_memcpy((Uint8 *)s_transfer_ptr + src_offset, src, size);

    s_pending_uploads[s_num_pending++] = (PendingUpload){
        .dst        = dst,
        .dst_offset = dst_offset,
        .src_offset = src_offset,
        .size       = size,
        .is_index   = is_index,
    };
}

static void flush_uploads(SDL_GPUCommandBuffer *cmd)
{
    if (s_num_pending == 0) return;

    SDL_GPUTransferBufferLocation src_loc = {
        .transfer_buffer = s_transfer_buf,
    };
    SDL_GPUBufferRegion dst_reg = {0};

    SDL_GPUCopyPass *cp = SDL_BeginGPUCopyPass(cmd);

    for (int i = 0; i < s_num_pending; i++)
    {
        PendingUpload *u = &s_pending_uploads[i];
        src_loc.offset = u->src_offset;
        dst_reg.buffer = u->dst;
        dst_reg.offset = u->dst_offset;
        dst_reg.size   = u->size;
        SDL_UploadToGPUBuffer(cp, &src_loc, &dst_reg, false);
    }

    SDL_EndGPUCopyPass(cp);
    s_num_pending    = 0;
    // s_transfer_used is NOT reset here — each pending upload's src_offset
    // is a unique position in the frame's linear transfer buffer.  Resetting
    // mid-frame causes the next queue_upload to overwrite already-recorded
    // source data before the GPU copies it.  Only MikuPan_Clear resets this.
}

// ─────────────────────────────────────────────────────────────────────────────
// Texture upload queue (for new textures created during a frame)
// ─────────────────────────────────────────────────────────────────────────────

#define MAX_PENDING_TEX 64

typedef struct
{
    SDL_GPUTexture *dst;
    Uint32          src_offset;
    Uint32          width;
    Uint32          height;
} PendingTexUpload;

static PendingTexUpload s_pending_tex[MAX_PENDING_TEX];
static int              s_num_pending_tex = 0;

static void queue_tex_upload(SDL_GPUTexture *dst, const void *pixels,
                              Uint32 width, Uint32 height)
{
    if (!dst || s_num_pending_tex >= MAX_PENDING_TEX) return;

    Uint32 size       = width * height * 4;
    Uint32 src_offset = transfer_alloc(size, 4);
    if (src_offset == UINT32_MAX) return;

    SDL_memcpy((Uint8 *)s_transfer_ptr + src_offset, pixels, size);

    s_pending_tex[s_num_pending_tex++] = (PendingTexUpload){
        .dst        = dst,
        .src_offset = src_offset,
        .width      = width,
        .height     = height,
    };
}

static void flush_tex_uploads(SDL_GPUCommandBuffer *cmd)
{
    if (s_num_pending_tex == 0) return;

    SDL_GPUCopyPass *cp = SDL_BeginGPUCopyPass(cmd);

    for (int i = 0; i < s_num_pending_tex; i++)
    {
        PendingTexUpload *u = &s_pending_tex[i];

        SDL_GPUTextureTransferInfo src = {
            .transfer_buffer = s_transfer_buf,
            .offset          = u->src_offset,
            .pixels_per_row  = u->width,
            .rows_per_layer  = u->height,
        };
        SDL_GPUTextureRegion dst = {
            .texture   = u->dst,
            .mip_level = 0,
            .layer     = 0,
            .x = 0, .y = 0, .z = 0,
            .w = u->width, .h = u->height, .d = 1,
        };
        SDL_UploadToGPUTexture(cp, &src, &dst, false);
    }

    SDL_EndGPUCopyPass(cp);
    s_num_pending_tex = 0;
}

// ─────────────────────────────────────────────────────────────────────────────
// Render-pass push helpers (called within the active render pass)
// ─────────────────────────────────────────────────────────────────────────────

// Bind a pipeline and push its uniform data for the current draw.
static void bind_pipeline_uniforms(SDL_GPURenderPass        *pass,
                                   SDL_GPUGraphicsPipeline  *pipeline,
                                   int                       shader_type)
{
    SDL_BindGPUGraphicsPipeline(pass, pipeline);

    switch (shader_type)
    {
        case MESH_0x12_SHADER:
        case MESH_0x2_SHADER:
        case MESH_0xA_SHADER:
            SDL_PushGPUVertexUniformData(s_cmd_buf, 0,
                &g_vert_uniforms, sizeof(g_vert_uniforms));
            SDL_PushGPUFragmentUniformData(s_cmd_buf, 0,
                &s_light_data, sizeof(s_light_data));
            SDL_PushGPUFragmentUniformData(s_cmd_buf, 1,
                &g_frag_misc_uniforms, sizeof(g_frag_misc_uniforms));
            break;

        case BOUNDING_BOX_SHADER:
            SDL_PushGPUVertexUniformData(s_cmd_buf, 0,
                &g_bb_uniforms, sizeof(g_bb_uniforms));
            break;

        default:
            break;
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// SDL_GPU device and window setup
// ─────────────────────────────────────────────────────────────────────────────

void MikuPan_SetupGpuDevice(void)
{
    // Prefer Vulkan → Metal → D3D12 in that order; fall back to whatever works
    mikupan_render.device = SDL_CreateGPUDevice(
        SDL_GPU_SHADERFORMAT_SPIRV | SDL_GPU_SHADERFORMAT_MSL |
        SDL_GPU_SHADERFORMAT_DXIL,
        false, NULL);
}

SDL_AppResult MikuPan_Init(void)
{
    SDL_SetAppMetadata("MikuPan", "1.0", "MikuPan");
    info_log("Initializing SDL");

    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD | SDL_INIT_JOYSTICK
                  | SDL_INIT_HAPTIC))
    {
        info_log("Couldn't initialize SDL: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    info_log("Working directory: %s", SDL_GetCurrentDirectory());

    SDL_SetHint(SDL_HINT_MAIN_CALLBACK_RATE, "60");

    info_log("Loading SDL_GameControllerDB");

    if (!SDL_AddGamepadMappingsFromFile("./resources/gamecontrollerdb.txt"))
    {
        info_log("Error creating the controller db %s", SDL_GetError());
    }

    info_log("Creating SDL Window");

    mikupan_render.window = SDL_CreateWindow("MikuPan", 1920, 1080,
                                             SDL_WINDOW_RESIZABLE);
    if (!mikupan_render.window)
    {
        info_log("Error creating the window %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    SDL_GetWindowSize(mikupan_render.window,
                      &mikupan_render.width, &mikupan_render.height);

    SDL_Surface *iconSurface = SDL_LoadPNG("resources/mikupan.png");
    if (!SDL_SetWindowIcon(mikupan_render.window, iconSurface))
    {
        info_log("Error setting the program icon %s", SDL_GetError());
    }

    SDL_DestroySurface(iconSurface);

    info_log("Creating SDL_GPU device");

    MikuPan_SetupGpuDevice();

    if (!mikupan_render.device)
    {
        info_log("SDL_GPU: failed to create device: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    if (!SDL_ClaimWindowForGPUDevice(mikupan_render.device, mikupan_render.window))
    {
        info_log("SDL_GPU: failed to claim window: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    // Create the streaming transfer buffer
    SDL_GPUTransferBufferCreateInfo tci = {
        .usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,
        .size  = TRANSFER_BUF_SIZE,
        .props = 0,
    };
    s_transfer_buf = SDL_CreateGPUTransferBuffer(mikupan_render.device, &tci);
    if (!s_transfer_buf)
    {
        info_log("SDL_GPU: failed to create transfer buffer: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    MikuPan_InitUi(mikupan_render.window, mikupan_render.device);

    info_log("SDL_GPU: driver = %s", SDL_GetGPUDeviceDriver(mikupan_render.device));

    if (MikuPan_InitShaders(mikupan_render.device) != 0)
    {
        info_log("SDL_GPU: failed to initialize shaders");
        return SDL_APP_FAILURE;
    }

    SDL_GPUTextureFormat swapchain_fmt =
        SDL_GetGPUSwapchainTextureFormat(mikupan_render.device, mikupan_render.window);

    SDL_GPUSampleCount sample_count = SDL_GPU_SAMPLECOUNT_1;

    MikuPan_InitPipelines(mikupan_render.device, swapchain_fmt, sample_count);
    MikuPan_Setup3D();
    MikuPan_CreateInternalBuffer(s_render_w, s_render_h, s_msaa_r);

    return SDL_APP_CONTINUE;
}

// ─────────────────────────────────────────────────────────────────────────────
// Internal render-target management
// ─────────────────────────────────────────────────────────────────────────────

void MikuPan_DestroyInternalBuffer(void)
{
    if (s_color_tex)
    {
        SDL_ReleaseGPUTexture(mikupan_render.device, s_color_tex);
        s_color_tex = NULL;
    }
    if (s_depth_tex)
    {
        SDL_ReleaseGPUTexture(mikupan_render.device, s_depth_tex);
        s_depth_tex = NULL;
    }
    if (s_resolve_tex)
    {
        SDL_ReleaseGPUTexture(mikupan_render.device, s_resolve_tex);
        s_resolve_tex = NULL;
    }
}

void MikuPan_CreateInternalBuffer(int w, int h, int msaa)
{
    s_render_w = w;
    s_render_h = h;
    s_msaa_r   = msaa;

    SDL_GPUSampleCount sc = SDL_GPU_SAMPLECOUNT_1;
    if      (msaa >= 8) sc = SDL_GPU_SAMPLECOUNT_8;
    else if (msaa >= 4) sc = SDL_GPU_SAMPLECOUNT_4;
    else if (msaa >= 2) sc = SDL_GPU_SAMPLECOUNT_2;

    SDL_GPUTextureFormat swapchain_fmt =
        SDL_GetGPUSwapchainTextureFormat(mikupan_render.device, mikupan_render.window);

    // Colour target (MSAA if requested)
    SDL_GPUTextureCreateInfo color_ci = {
        .type                  = SDL_GPU_TEXTURETYPE_2D,
        .format                = swapchain_fmt,
        .usage                 = SDL_GPU_TEXTUREUSAGE_COLOR_TARGET |
                                 SDL_GPU_TEXTUREUSAGE_SAMPLER,
        .width                 = (Uint32)w,
        .height                = (Uint32)h,
        .layer_count_or_depth  = 1,
        .num_levels            = 1,
        .sample_count          = sc,
        .props                 = 0,
    };
    s_color_tex = SDL_CreateGPUTexture(mikupan_render.device, &color_ci);

    // Depth target
    SDL_GPUTextureCreateInfo depth_ci = {
        .type                  = SDL_GPU_TEXTURETYPE_2D,
        .format                = SDL_GPU_TEXTUREFORMAT_D32_FLOAT,
        .usage                 = SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET,
        .width                 = (Uint32)w,
        .height                = (Uint32)h,
        .layer_count_or_depth  = 1,
        .num_levels            = 1,
        .sample_count          = sc,
        .props                 = 0,
    };
    s_depth_tex = SDL_CreateGPUTexture(mikupan_render.device, &depth_ci);

    // MSAA resolve target (non-MSAA, sampled for the final blit)
    if (sc != SDL_GPU_SAMPLECOUNT_1)
    {
        SDL_GPUTextureCreateInfo resolve_ci = {
            .type                  = SDL_GPU_TEXTURETYPE_2D,
            .format                = swapchain_fmt,
            .usage                 = SDL_GPU_TEXTUREUSAGE_COLOR_TARGET |
                                     SDL_GPU_TEXTUREUSAGE_SAMPLER,
            .width                 = (Uint32)w,
            .height                = (Uint32)h,
            .layer_count_or_depth  = 1,
            .num_levels            = 1,
            .sample_count          = SDL_GPU_SAMPLECOUNT_1,
            .props                 = 0,
        };
        s_resolve_tex = SDL_CreateGPUTexture(mikupan_render.device, &resolve_ci);
    }

    // Recreate pipelines if MSAA changed
    MikuPan_RecreatePipelines(mikupan_render.device, swapchain_fmt, sc);

    if (!s_color_tex || !s_depth_tex)
    {
        info_log("SDL_GPU: failed to create render targets: %s", SDL_GetError());
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Frame begin / end
// ─────────────────────────────────────────────────────────────────────────────

void MikuPan_Clear(void)
{
    MikuPan_RenderSetDebugValues();

    int curr_w    = MikuPan_GetRenderResolutionWidth();
    int curr_h    = MikuPan_GetRenderResolutionHeight();
    int curr_msaa = MikuPan_GetMSAA();

    if (s_render_w != curr_w || s_render_h != curr_h || s_msaa_r != curr_msaa)
    {
        MikuPan_DestroyInternalBuffer();
        MikuPan_CreateInternalBuffer(curr_w, curr_h, curr_msaa);
    }

    // Acquire a command buffer for this frame
    s_cmd_buf = SDL_AcquireGPUCommandBuffer(mikupan_render.device);

    // Clear the offscreen render targets at the START of each frame so that
    // all game rendering (which uses LOADOP_LOAD) builds on a clean slate.
    if (s_color_tex && s_depth_tex)
    {
        SDL_GPUColorTargetInfo clear_cti = {
            .texture               = s_color_tex,
            .clear_color           = { 0.0f, 0.0f, 0.0f, 1.0f },
            .load_op               = SDL_GPU_LOADOP_CLEAR,
            .store_op              = s_resolve_tex
                                         ? SDL_GPU_STOREOP_RESOLVE
                                         : SDL_GPU_STOREOP_STORE,
            .resolve_texture       = s_resolve_tex,
            .resolve_mip_level     = 0,
            .resolve_layer         = 0,
            .cycle                 = true,
            .cycle_resolve_texture = (s_resolve_tex != NULL),
        };
        SDL_GPUDepthStencilTargetInfo clear_dti = {
            .texture          = s_depth_tex,
            .clear_depth      = 1.0f,
            .load_op          = SDL_GPU_LOADOP_CLEAR,
            .store_op         = SDL_GPU_STOREOP_DONT_CARE,
            .stencil_load_op  = SDL_GPU_LOADOP_CLEAR,
            .stencil_store_op = SDL_GPU_STOREOP_DONT_CARE,
            .cycle            = true,
        };
        SDL_GPURenderPass *clear_pass =
            SDL_BeginGPURenderPass(s_cmd_buf, &clear_cti, 1, &clear_dti);
        SDL_EndGPURenderPass(clear_pass);
    }

    // Map the transfer buffer for vertex/index uploads this frame
    s_transfer_ptr  = SDL_MapGPUTransferBuffer(mikupan_render.device,
                                               s_transfer_buf, true);
    s_transfer_used = 0;
    s_num_pending   = 0;
    s_num_pending_tex = 0;
}

void MikuPan_EndFrame(void)
{
    if (!s_cmd_buf) return;

    // 1. Unmap transfer buffer so GPU can read from it
    SDL_UnmapGPUTransferBuffer(mikupan_render.device, s_transfer_buf);
    s_transfer_ptr = NULL;

    // 2. Copy pass: flush any remaining pending vertex / index uploads
    flush_tex_uploads(s_cmd_buf);
    flush_uploads(s_cmd_buf);

    // 3. Blit the offscreen render target to the swapchain
    // (The clear happened at frame start in MikuPan_Clear; game rendering used
    //  LOADOP_LOAD to accumulate draws on top of it.  Nothing to clear here.)
    SDL_GPUTexture *render_target = s_resolve_tex ? s_resolve_tex : s_color_tex;

    // 4. Acquire swapchain texture and blit the offscreen result to it
    SDL_GPUTexture *swapchain_tex = NULL;
    Uint32 sw_w, sw_h;
    SDL_AcquireGPUSwapchainTexture(s_cmd_buf, mikupan_render.window,
                                   &swapchain_tex, &sw_w, &sw_h);

    // Build ImGui draw lists (includes ImGui::Render()) and upload
    // vertex/index data to GPU — must happen outside a render pass.
    MikuPan_DrawUi();
    MikuPan_PrepareUi(s_cmd_buf);

    if (swapchain_tex)
    {
        // Compute letterbox/pillarbox viewport
        float target_aspect = (float)s_render_w / (float)s_render_h;
        float window_aspect = (float)sw_w / (float)sw_h;
        float vp_x, vp_y, vp_w, vp_h;

        if (window_aspect > target_aspect)
        {
            vp_h = (float)sw_h;
            vp_w = vp_h * target_aspect;
            vp_x = ((float)sw_w - vp_w) * 0.5f;
            vp_y = 0;
        }
        else
        {
            vp_w = (float)sw_w;
            vp_h = vp_w / target_aspect;
            vp_x = 0;
            vp_y = ((float)sw_h - vp_h) * 0.5f;
        }

        // Blit offscreen → swapchain using a full-screen sprite pass
        // V=0 is the top row of the offscreen texture; NDC Y=+1 is the top of the
        // swapchain. Swap V so that the top of the offscreen maps to the top of the
        // screen — without this the entire rendered scene is flipped upside-down.
        float quad[4][12] = {
            { 0,1,0,0,  1,1,1,1,  -1,-1, 0,1 },
            { 1,1,0,0,  1,1,1,1,   1,-1, 0,1 },
            { 0,0,0,0,  1,1,1,1,  -1, 1, 0,1 },
            { 1,0,0,0,  1,1,1,1,   1, 1, 0,1 },
        };

        MikuPan_PipelineInfo *spr_pipeline = MikuPan_GetPipelineInfo(UV4_COLOUR4_POSITION4);

        // Re-map to append the blit quad into the frame's transfer buffer.
        // Do NOT reset s_transfer_used — earlier draw calls recorded copy
        // commands whose src_offset values must remain valid until submission.
        // cycle=false: the buffer is already tracked (referenceCount > 0) so
        // cycle=true would allocate a fresh 32 MB Metal buffer here.  We write
        // the quad at a fresh offset beyond all previously recorded regions, so
        // the same physical buffer is safe to reuse.
        s_transfer_ptr = SDL_MapGPUTransferBuffer(mikupan_render.device,
                                                  s_transfer_buf, false);
        Uint32 quad_offset = transfer_alloc(sizeof(quad), 4);
        if (quad_offset != UINT32_MAX)
            SDL_memcpy((Uint8 *)s_transfer_ptr + quad_offset, quad, sizeof(quad));

        SDL_UnmapGPUTransferBuffer(mikupan_render.device, s_transfer_buf);
        s_transfer_ptr = NULL;

        if (quad_offset != UINT32_MAX)
        {
            SDL_GPUTransferBufferLocation src_loc = {
                .transfer_buffer = s_transfer_buf, .offset = quad_offset };
            SDL_GPUBufferRegion dst_reg = {
                .buffer = spr_pipeline->buffers[0].buffer,
                .offset = 0, .size = sizeof(quad) };
            SDL_GPUCopyPass *cp2 = SDL_BeginGPUCopyPass(s_cmd_buf);
            SDL_UploadToGPUBuffer(cp2, &src_loc, &dst_reg, false);
            SDL_EndGPUCopyPass(cp2);
        }

        // Swapchain render pass
        SDL_GPUColorTargetInfo sw_cti = {
            .texture     = swapchain_tex,
            .clear_color = { 0.0f, 0.0f, 0.0f, 1.0f },
            .load_op     = SDL_GPU_LOADOP_CLEAR,
            .store_op    = SDL_GPU_STOREOP_STORE,
        };
        SDL_GPURenderPass *sw_pass = SDL_BeginGPURenderPass(s_cmd_buf, &sw_cti, 1, NULL);

        SDL_GPUViewport sw_vp = {
            .x = vp_x, .y = vp_y, .w = vp_w, .h = vp_h,
            .min_depth = 0.0f, .max_depth = 1.0f,
        };
        SDL_SetGPUViewport(sw_pass, &sw_vp);

        SDL_GPUGraphicsPipeline *blit_pipe =
            MikuPan_GetGpuPipeline(GPU_PIPELINE_BLIT_TO_SWAPCHAIN);
        SDL_BindGPUGraphicsPipeline(sw_pass, blit_pipe);

        SDL_GPUTextureSamplerBinding tex_bind = {
            .texture = render_target,
            .sampler = g_gpu_sampler,
        };
        SDL_BindGPUFragmentSamplers(sw_pass, 0, &tex_bind, 1);

        SDL_GPUBufferBinding vb_bind = {
            .buffer = spr_pipeline->buffers[0].buffer, .offset = 0 };
        SDL_BindGPUVertexBuffers(sw_pass, 0, &vb_bind, 1);

        SDL_DrawGPUPrimitives(sw_pass, 4, 1, 0, 0);

        MikuPan_RenderUi(s_cmd_buf, sw_pass);

        SDL_EndGPURenderPass(sw_pass);
    }

    SDL_SubmitGPUCommandBuffer(s_cmd_buf);
    s_cmd_buf = NULL;

    s_draw_calls    = 0;
    s_state_changes = 0;
}

// ─────────────────────────────────────────────────────────────────────────────
// Window / render mode queries
// ─────────────────────────────────────────────────────────────────────────────

void MikuPan_UpdateWindowSize(int width, int height)
{
    mikupan_render.width  = width;
    mikupan_render.height = height;
}

int MikuPan_GetWindowWidth(void)  { return s_render_w; }
int MikuPan_GetWindowHeight(void) { return s_render_h; }

bool MikuPan_GetRenderWireframe(void)
{
    return MikuPan_IsWireframeRendering();
}

void MikuPan_RenderSetDebugValues(void)
{
    MikuPan_SetUniform1iToAllShaders(MikuPan_IsNormalsRendering(), "renderNormals");
    float *cc = MikuPan_GetLightColor();
    MikuPan_SetUniform1fToAllShaders(cc[0], "uColorScale");
}

// ─────────────────────────────────────────────────────────────────────────────
// Texture management
// ─────────────────────────────────────────────────────────────────────────────

MikuPan_TextureInfo *MikuPan_CreateGPUTexture(sceGsTex0 *tex0)
{
    int      width  = 1 << tex0->TW;
    int      height = 1 << tex0->TH;
    uint64_t hash   = 0;
    void    *pixels = MikuPan_GsDownloadTexture(tex0, &hash);

    if (hash == 0) return NULL;

    // MikuPan_GsDownloadTexture returns nullptr (with non-zero hash) when the
    // texture is already in the cache.  Return the cached entry directly.
    if (!pixels) return MikuPan_GetTextureInfo(hash);

    SDL_GPUTextureCreateInfo tci = {
        .type                 = SDL_GPU_TEXTURETYPE_2D,
        .format               = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM,
        .usage                = SDL_GPU_TEXTUREUSAGE_SAMPLER,
        .width                = (Uint32)width,
        .height               = (Uint32)height,
        .layer_count_or_depth = 1,
        .num_levels           = 1,
        .sample_count         = SDL_GPU_SAMPLECOUNT_1,
        .props                = 0,
    };
    SDL_GPUTexture *tex = SDL_CreateGPUTexture(mikupan_render.device, &tci);
    if (!tex)
    {
        info_log("SDL_GPU: failed to create texture: %s", SDL_GetError());
        free(pixels);
        return NULL;
    }

    Uint32 tex_size = (Uint32)(width * height * 4);

    SDL_GPUTextureRegion tex_dst_reg = {
        .texture = tex, .mip_level = 0, .layer = 0,
        .x = 0, .y = 0, .z = 0,
        .w = (Uint32)width, .h = (Uint32)height, .d = 1,
    };

    bool need_temp_cmd    = (s_cmd_buf == NULL);
    bool shared_was_mapped = (s_transfer_ptr != NULL);

    // ── Path A: reuse the shared streaming transfer buffer ───────────────────
    // Preferred when inside a frame (s_cmd_buf live) and the shared buffer is
    // mapped with enough space.  Avoids a per-texture allocation: each dedicated
    // SDL_GPUTransferBuffer is held in Metal's retain graph until the command
    // buffer completes (~1–2 frames), causing RAM to spike by 2× the texture
    // size for every new texture rendered.  Using the shared buffer keeps the
    // overhead to just the permanent GPU texture.
    //
    // cycle=true on remap is safe here: SDL GPU only actually cycles a transfer
    // buffer when the GPU is actively executing commands that reference it.
    // Since s_cmd_buf has not been submitted yet there is no such execution in
    // flight, so the same physical page is always returned and all previously
    // recorded src_offsets remain valid.
    bool used_shared = false;
    if (!need_temp_cmd && shared_was_mapped)
    {
        // Metal requires texture source offsets to be 256-byte aligned.
        Uint32 tex_src_offset = transfer_alloc(tex_size, 256);
        if (tex_src_offset != UINT32_MAX)
        {
            SDL_memcpy((Uint8 *)s_transfer_ptr + tex_src_offset, pixels, tex_size);

            SDL_UnmapGPUTransferBuffer(mikupan_render.device, s_transfer_buf);
            s_transfer_ptr = NULL;

            SDL_GPUCopyPass *cp = SDL_BeginGPUCopyPass(s_cmd_buf);
            SDL_GPUTextureTransferInfo tex_src = {
                .transfer_buffer = s_transfer_buf,
                .offset          = tex_src_offset,
                .pixels_per_row  = (Uint32)width,
                .rows_per_layer  = (Uint32)height,
            };
            SDL_UploadToGPUTexture(cp, &tex_src, &tex_dst_reg, false);
            SDL_EndGPUCopyPass(cp);

            // cycle=false: the buffer is already tracked by the copy pass we
            // just recorded (referenceCount > 0).  cycle=true would allocate a
            // new 32 MB Metal buffer for every texture upload.  Writing at
            // offsets beyond tex_src_offset is safe; the GPU reads only from
            // the exact offset captured in the copy command.
            s_transfer_ptr = SDL_MapGPUTransferBuffer(mikupan_render.device,
                                                      s_transfer_buf, false);
            used_shared = true;
        }
    }

    // ── Path B: dedicated per-texture upload buffer ──────────────────────────
    // Fallback for font setup (s_cmd_buf == NULL), an unmapped shared buffer,
    // or when the shared buffer has no room for the texture data.
    if (!used_shared)
    {
        SDL_GPUTransferBufferCreateInfo tex_tci = {
            .usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,
            .size  = tex_size,
            .props = 0,
        };
        SDL_GPUTransferBuffer *tex_upload_buf =
            SDL_CreateGPUTransferBuffer(mikupan_render.device, &tex_tci);
        if (tex_upload_buf)
        {
            void *tex_ptr = SDL_MapGPUTransferBuffer(mikupan_render.device,
                                                     tex_upload_buf, false);
            SDL_memcpy(tex_ptr, pixels, tex_size);
            SDL_UnmapGPUTransferBuffer(mikupan_render.device, tex_upload_buf);

            SDL_GPUCommandBuffer *upload_cmd = need_temp_cmd
                ? SDL_AcquireGPUCommandBuffer(mikupan_render.device)
                : s_cmd_buf;

            if (shared_was_mapped && !need_temp_cmd)
            {
                SDL_UnmapGPUTransferBuffer(mikupan_render.device, s_transfer_buf);
                s_transfer_ptr = NULL;
            }

            SDL_GPUCopyPass *cp = SDL_BeginGPUCopyPass(upload_cmd);
            SDL_GPUTextureTransferInfo tex_src = {
                .transfer_buffer = tex_upload_buf,
                .offset          = 0,
                .pixels_per_row  = (Uint32)width,
                .rows_per_layer  = (Uint32)height,
            };
            SDL_UploadToGPUTexture(cp, &tex_src, &tex_dst_reg, false);
            SDL_EndGPUCopyPass(cp);

            if (need_temp_cmd)
                SDL_SubmitGPUCommandBuffer(upload_cmd);

            SDL_ReleaseGPUTransferBuffer(mikupan_render.device, tex_upload_buf);

            if (shared_was_mapped && !need_temp_cmd)
            {
                // cycle=false: same reasoning as Path A — write at fresh
                // offsets, no new Metal buffer allocation.
                s_transfer_ptr = SDL_MapGPUTransferBuffer(mikupan_render.device,
                                                          s_transfer_buf, false);
            }
        }
    }

    MikuPan_TextureInfo *info = malloc(sizeof(MikuPan_TextureInfo));
    info->width   = width;
    info->height  = height;
    info->texture = tex;
    info->tex0    = *(uint64_t *)tex0;
    info->hash    = hash;

    MikuPan_AddTexture(hash, info);
    free(pixels);
    return info;
}

void MikuPan_SetTexture(sceGsTex0 *tex0)
{
    uint64_t hash = MikuPan_GetTextureHash(tex0);
    MikuPan_TextureInfo *info = MikuPan_GetTextureInfo(hash);
    if (!info) info = MikuPan_CreateGPUTexture(tex0);
    // Actual binding happens in the per-draw render pass commands; the texture
    // pointer is accessed via MikuPan_GetTextureInfo at draw time.
    (void)info;
}

void MikuPan_DeleteTexture(MikuPan_TextureInfo *info)
{
    if (!info) return;

    for (int i = 0; i < 6; i++)
    {
        if (s_fnt_texture[i] && s_fnt_texture[i]->texture == info->texture)
            return;
    }

    SDL_ReleaseGPUTexture(mikupan_render.device, info->texture);
    free(info);
}

bool MikuPan_IsFontTexture(MikuPan_TextureInfo *info)
{
    if (!info) return false;
    for (int i = 0; i < 6; i++)
    {
        if (s_fnt_texture[i] && s_fnt_texture[i]->texture == info->texture)
            return true;
    }
    return false;
}

void MikuPan_SetupFntTexture(void)
{
    for (int i = 0; i < 6; i++)
    {
        if (!s_fnt_texture[i])
        {
            s_fnt_texture[i] =
                MikuPan_CreateGPUTexture((sceGsTex0 *)&fntdat[i].tex0);
        }
    }
    s_curr_fnt_texture = s_fnt_texture[0];
}

void MikuPan_SetFontTexture(int fnt)
{
    s_curr_fnt_texture = s_fnt_texture[fnt];
}

// ─────────────────────────────────────────────────────────────────────────────
// Camera / matrix setup
// ─────────────────────────────────────────────────────────────────────────────

void MikuPan_SetupCamera(MikuPan_Camera *cam)
{
    vec3 center = {0}, up = {0};
    glm_vec3_add(cam->p, cam->zd, center);

    mat4 roll_mat = GLM_MAT4_IDENTITY_INIT;
    vec3 axis = {0.0f, 0.0f, 1.0f};
    glm_rotate(roll_mat, -cam->roll, axis);
    glm_mat4_mulv3(roll_mat, cam->yd, 1.0f, up);
    glm_lookat(cam->p, center, up, WorldView);

    // Vulkan-convention projection (Z in [0,1])
    glm_mat4_zero(projection);
    float nearz = 10.0f;
    float farz  = cam->farz;
    float halfW = (float)MikuPan_GetWindowWidth()  * 0.5f;
    float halfH = (float)MikuPan_GetWindowHeight() * 0.25f;
    float scrz  = halfH / tanf(cam->fov * 0.5f) * 2.0f;
    float rscrz = nearz / scrz;
    float gsxv  = halfW * rscrz;
    float gsyv  = halfH * rscrz;
    float l = -gsxv, r = gsxv, b = -gsyv, t = gsyv;

    projection[0][0] = (2.0f * nearz) / (r - l);
    projection[1][1] = (2.0f * nearz) / (t - b);
    projection[0][2] = (r + l) / (r - l);
    projection[1][2] = (t + b) / (t - b);
    // Vulkan Z: map [near,far] → [0,1]
    projection[2][2] = -farz / (farz - nearz);
    projection[2][3] = -1.0f;
    projection[3][2] = -(farz * nearz) / (farz - nearz);
    projection[3][3] = 0.0f;
    projection[0][0] *= cam->ax;
    projection[1][1] *= cam->ay;

    glm_mat4_mul(projection, WorldView, WorldClipView);
    MikuPan_SetUniformMatrix4fvToAllShaders((float *)WorldView,  "view");
    MikuPan_SetUniformMatrix4fvToAllShaders((float *)projection, "projection");

    float gs_width  = 4096.0f, gs_height = 4096.0f;
    float scaleX = (float)MikuPan_GetWindowWidth()  / gs_width;
    float scaleY = (float)MikuPan_GetWindowHeight() / gs_height;

    mat4 vc = GLM_MAT4_IDENTITY_INIT;
    vc[0][0] = (((nearz + nearz) * cam->ax) / gs_width)  * scaleX;
    vc[1][1] = (((nearz + nearz) * cam->ay) / gs_height) * scaleY;
    vc[2][2] = (farz + nearz) / (farz - nearz);
    vc[2][3] = 1.0f;
    vc[3][2] = ((farz * nearz) * -2.0f) / (farz - nearz);
    vc[3][3] = 0.0f;

    glm_mat4_mul(vc, WorldView, WorldClip);
    SgSetClipMtx(WorldClip);
    SgSetClipVMtx(WorldClipView);
    s_mirror_mode = false; // camera reset clears mirror mode
}

void MikuPan_SetupMirrorMtx(float *mtx)
{
    mat4 out = {0}, m = {0};
    glm_mat4_make(mtx, m);
    glm_mat4_mul(WorldView, m, out);
    glm_mat4_mul(projection, out, WorldClipView);
    MikuPan_SetUniformMatrix4fvToAllShaders((float *)out, "view");
    s_mirror_mode = true;
}

float *MikuPan_GetWorldClipView(void) { return (float *)WorldClipView; }
float *MikuPan_GetWorldClip(void)     { return (float *)WorldView;     }

void MikuPan_SetWorldClipView(void)
{
    MikuPan_SetModelTransformMatrix(WorldClipView);
}

void MikuPan_SetModelTransformMatrix(sceVu0FVECTOR *m)
{
    s_state_changes++;
    MikuPan_SetUniformMatrix4fvToAllShaders((float *)m, "model");
}

void MikuPan_Setup3D(void) { /* render state is baked into pipelines */ }

// ─────────────────────────────────────────────────────────────────────────────
// Lighting
// ─────────────────────────────────────────────────────────────────────────────

void MikuPan_SetupAmbientLighting(const LIGHT_PACK *lp)
{
#define MAX_LIGHTS 3
    s_light_data.uAmbient[0] = lp->ambient[0];
    s_light_data.uAmbient[1] = lp->ambient[2];
    s_light_data.uAmbient[2] = lp->ambient[1];
    s_light_data.uAmbient[3] = lp->ambient[3];

    int parCount = lp->parallel_num > MAX_LIGHTS ? MAX_LIGHTS : lp->parallel_num;
    s_light_data.uParCount[0] = parCount;

    mat3 view3;
    glm_mat4_pick3(WorldView, view3);

    for (int i = 0; i < parCount; i++)
    {
        vec4 dirWS = { lp->parallel[i].direction[0], lp->parallel[i].direction[1],
                       lp->parallel[i].direction[2], lp->parallel[i].direction[3] };
        vec4 dirVS;
        glm_mat3_mulv(view3, dirWS, dirVS);
        s_light_data.uParDir[i][0] = dirVS[0];
        s_light_data.uParDir[i][1] = dirVS[1];
        s_light_data.uParDir[i][2] = dirVS[2];
        s_light_data.uParDir[i][3] = 1.0f;
        s_light_data.uParDiffuse[i][0] = lp->parallel[i].diffuse[0];
        s_light_data.uParDiffuse[i][1] = lp->parallel[i].diffuse[2];
        s_light_data.uParDiffuse[i][2] = lp->parallel[i].diffuse[1];
        s_light_data.uParDiffuse[i][3] = lp->parallel[i].diffuse[3] / 128.0f;
    }

    int pointCount = lp->point_num > MAX_LIGHTS ? MAX_LIGHTS : lp->point_num;
    s_light_data.uPointCount[0] = pointCount;

    for (int i = 0; i < pointCount; i++)
    {
        vec4 posWS = { lp->point[i].pos[0], lp->point[i].pos[1],
                       lp->point[i].pos[2], lp->point[i].pos[3] };
        vec4 posVS;
        glm_mat4_mulv(WorldView, posWS, posVS);
        s_light_data.uPointPos[i][0] = posVS[0];
        s_light_data.uPointPos[i][1] = posVS[1];
        s_light_data.uPointPos[i][2] = posVS[2];
        s_light_data.uPointPos[i][3] = 1.0f;
        s_light_data.uPointDiffuse[i][0] = lp->point[i].diffuse[0];
        s_light_data.uPointDiffuse[i][1] = lp->point[i].diffuse[2];
        s_light_data.uPointDiffuse[i][2] = lp->point[i].diffuse[1];
        s_light_data.uPointDiffuse[i][3] = lp->point[i].diffuse[3] / 128.0f;
        s_light_data.uPointPower[i][0]   = lp->point[i].power;
    }

    int spotCount = lp->spot_num > MAX_LIGHTS ? MAX_LIGHTS : lp->spot_num;
    s_light_data.uSpotCount[0] = spotCount;

    for (int i = 0; i < spotCount; i++)
    {
        vec4 posWS = { lp->spot[i].pos[0], lp->spot[i].pos[1],
                       lp->spot[i].pos[2], lp->spot[i].pos[3] };
        vec4 posVS;
        glm_mat4_mulv(WorldView, posWS, posVS);
        s_light_data.uSpotPos[i][0] = posVS[0];
        s_light_data.uSpotPos[i][1] = posVS[1];
        s_light_data.uSpotPos[i][2] = posVS[2];
        s_light_data.uSpotPos[i][3] = 1.0f;

        vec4 dirWS = { lp->spot[i].direction[0], lp->spot[i].direction[1],
                       lp->spot[i].direction[2], lp->spot[i].direction[3] };
        vec4 dirVS;
        glm_mat3_mulv(view3, dirWS, dirVS);
        glm_vec3_normalize(dirVS);
        s_light_data.uSpotDir[i][0] = dirVS[0];
        s_light_data.uSpotDir[i][1] = dirVS[1];
        s_light_data.uSpotDir[i][2] = dirVS[2];
        s_light_data.uSpotDir[i][3] = 1.0f;
        s_light_data.uSpotDiffuse[i][0] = lp->spot[i].diffuse[0];
        s_light_data.uSpotDiffuse[i][1] = lp->spot[i].diffuse[2];
        s_light_data.uSpotDiffuse[i][2] = lp->spot[i].diffuse[1];
        s_light_data.uSpotDiffuse[i][3] = lp->spot[i].diffuse[3];
        s_light_data.uSpotPower[i][0]   = lp->spot[i].power;
        s_light_data.uSpotIntens[i][0]  = lp->spot[i].intens;
    }
#undef MAX_LIGHTS
}

// ─────────────────────────────────────────────────────────────────────────────
// 2D sprite rendering
// ─────────────────────────────────────────────────────────────────────────────

static void draw_sprite_pass(SDL_GPUGraphicsPipeline *pipe,
                             MikuPan_PipelineInfo    *pl_info,
                             MikuPan_TextureInfo     *tex_info,
                             const float             *verts,
                             Uint32                   vert_bytes)
{
    // Queue vertex upload
    queue_upload(pl_info->buffers[0].buffer, 0, verts, vert_bytes, false);

    // Deferred: actual draw recorded in the render pass (called from EndFrame).
    // Since we only have one render pass in EndFrame, we flush uploads there
    // before the pass begins.  For now, store the draw command inline.
    // (In a full deferred architecture a draw list would be recorded here.)
    // ── inline render pass ──────────────────────────────────────────────────
    // Flush queued uploads first.  Remap with cycle=false: after flush_uploads
    // the buffer is tracked (referenceCount > 0), so cycle=true would allocate
    // a new 32 MB Metal buffer on every draw call.  Subsequent queue_upload
    // calls write at monotonically-increasing offsets beyond what the GPU has
    // already been told to read — the same physical buffer is safe to reuse.
    SDL_UnmapGPUTransferBuffer(mikupan_render.device, s_transfer_buf);
    s_transfer_ptr = NULL;
    flush_uploads(s_cmd_buf);
    s_transfer_ptr = SDL_MapGPUTransferBuffer(mikupan_render.device,
                                              s_transfer_buf, false);

    SDL_GPUColorTargetInfo cti = {
        .texture  = s_resolve_tex ? s_resolve_tex : s_color_tex,
        .load_op  = SDL_GPU_LOADOP_LOAD,
        .store_op = SDL_GPU_STOREOP_STORE,
    };
    SDL_GPUDepthStencilTargetInfo dti = {
        .texture          = s_depth_tex,
        .load_op          = SDL_GPU_LOADOP_LOAD,
        .store_op         = SDL_GPU_STOREOP_STORE,
        .stencil_load_op  = SDL_GPU_LOADOP_LOAD,
        .stencil_store_op = SDL_GPU_STOREOP_STORE,
    };
    SDL_GPURenderPass *pass = SDL_BeginGPURenderPass(s_cmd_buf, &cti, 1, &dti);

    SDL_GPUViewport vp = {
        .x = 0, .y = 0, .w = (float)s_render_w, .h = (float)s_render_h,
        .min_depth = 0.0f, .max_depth = 1.0f,
    };
    SDL_SetGPUViewport(pass, &vp);
    SDL_BindGPUGraphicsPipeline(pass, pipe);

    if (tex_info)
    {
        SDL_GPUTextureSamplerBinding tb = {
            .texture = tex_info->texture, .sampler = g_gpu_sampler };
        SDL_BindGPUFragmentSamplers(pass, 0, &tb, 1);
    }

    SDL_GPUBufferBinding vb = { .buffer = pl_info->buffers[0].buffer, .offset = 0 };
    SDL_BindGPUVertexBuffers(pass, 0, &vb, 1);
    SDL_DrawGPUPrimitives(pass, 4, 1, 0, 0);

    SDL_EndGPURenderPass(pass);
    s_draw_calls++;
}

void MikuPan_Render2DMessage(DISP_SPRT *sprite)
{
    MikuPan_Rect dst = { (float)sprite->x, (float)sprite->y,
                         (float)sprite->w, (float)sprite->h };
    MikuPan_Rect src = { (float)sprite->u, (float)sprite->v,
                         (float)sprite->w, (float)sprite->h };
    MikuPan_RenderSprite(src, dst,
                         sprite->r, sprite->g, sprite->b, sprite->alpha,
                         s_curr_fnt_texture);
}

void MikuPan_RenderSprite(MikuPan_Rect src, MikuPan_Rect dst,
                          u_char r, u_char g, u_char b, u_char a,
                          MikuPan_TextureInfo *tex_info)
{
    if (!tex_info) return;

    float ndc[4] = {0};
    MikuPan_ConvertPs2ScreenCoordToNDCMaintainAspectRatio(
        ndc,   (float)s_render_w, (float)s_render_h, dst.x, dst.y);
    MikuPan_ConvertPs2ScreenCoordToNDCMaintainAspectRatio(
        &ndc[2], (float)s_render_w, (float)s_render_h,
        dst.x + src.w, dst.y + src.h);

    float texW = (float)tex_info->width, texH = (float)tex_info->height;
    float halfU = 0.5f / texW, halfV = 0.5f / texH;
    float u0 = (src.x / texW) + halfU;
    float v0 = (src.y / texH) + halfV;
    float u1 = ((src.x + dst.w) / texW) - halfU;
    float v1 = ((src.y + dst.h) / texH) - halfV;

    float col[4] = {
        MikuPan_ConvertScaleColor(r), MikuPan_ConvertScaleColor(g),
        MikuPan_ConvertScaleColor(b), MikuPan_ConvertScaleColor(a)
    };

    float verts[4][12] = {
        { u0,v0,0,0, col[0],col[1],col[2],col[3], ndc[0],ndc[1],0,1 },
        { u1,v0,0,0, col[0],col[1],col[2],col[3], ndc[2],ndc[1],0,1 },
        { u0,v1,0,0, col[0],col[1],col[2],col[3], ndc[0],ndc[3],0,1 },
        { u1,v1,0,0, col[0],col[1],col[2],col[3], ndc[2],ndc[3],0,1 },
    };

    bool wire = MikuPan_IsWireframeRendering();
    SDL_GPUGraphicsPipeline *pipe = MikuPan_GetGpuPipeline(
        wire ? GPU_PIPELINE_SPRITE_2D_WIREFRAME : GPU_PIPELINE_SPRITE_2D);
    draw_sprite_pass(pipe, MikuPan_GetPipelineInfo(UV4_COLOUR4_POSITION4),
                     tex_info, (float *)verts, sizeof(verts));
}

void MikuPan_RenderSprite2D(sceGsTex0 *tex, float *buffer)
{
    uint64_t hash       = MikuPan_GetTextureHash(tex);
    MikuPan_TextureInfo *info = MikuPan_GetTextureInfo(hash);
    if (!info) info = MikuPan_CreateGPUTexture(tex);

    bool wire = MikuPan_IsWireframeRendering();

    SDL_GPUGraphicsPipeline *pipe = MikuPan_GetGpuPipeline(
        wire ? GPU_PIPELINE_SPRITE_2D_WIREFRAME : GPU_PIPELINE_SPRITE_2D);
    draw_sprite_pass(pipe, MikuPan_GetPipelineInfo(UV4_COLOUR4_POSITION4),
                     info, buffer, (Uint32)(4 * 48));
}

void MikuPan_RenderSprite3D(sceGsTex0 *tex, float *buffer)
{
    uint64_t hash       = MikuPan_GetTextureHash(tex);
    MikuPan_TextureInfo *info = MikuPan_GetTextureInfo(hash);
    if (!info) info = MikuPan_CreateGPUTexture(tex);

    SDL_GPUGraphicsPipeline *pipe = MikuPan_GetGpuPipeline(GPU_PIPELINE_SPRITE_3D);
    draw_sprite_pass(pipe, MikuPan_GetPipelineInfo(UV4_COLOUR4_POSITION4),
                     info, buffer, (Uint32)(4 * 48));
}

void MikuPan_RenderUntexturedSprite(float *buffer)
{
    bool wire = MikuPan_IsWireframeRendering();
    SDL_GPUGraphicsPipeline *pipe = MikuPan_GetGpuPipeline(
        wire ? GPU_PIPELINE_LINE_2D : GPU_PIPELINE_UNLIT_STRIP_2D);
    draw_sprite_pass(pipe, MikuPan_GetPipelineInfo(COLOUR4_POSITION4),
                     NULL, buffer, (Uint32)(4 * 32));
}

// ─────────────────────────────────────────────────────────────────────────────
// Line rendering
// ─────────────────────────────────────────────────────────────────────────────

void MikuPan_RenderLine(float x1, float y1, float x2, float y2,
                        u_char r, u_char g, u_char b, u_char a)
{
    float sx1 = (300.0f + x1) / PS2_RESOLUTION_X_FLOAT;
    float sy1 = (200.0f + y1) / PS2_RESOLUTION_Y_FLOAT;
    float sx2 = (300.0f + x2) / PS2_RESOLUTION_X_FLOAT;
    float sy2 = (200.0f + y2) / PS2_RESOLUTION_Y_FLOAT;

    float col[4] = {
        MikuPan_ConvertScaleColor(r), MikuPan_ConvertScaleColor(g),
        MikuPan_ConvertScaleColor(b), MikuPan_ConvertScaleColor(a)
    };

    float verts[2][8] = {
        { col[0],col[1],col[2],col[3], sx1*2.0f-1.0f, 1.0f-sy1*2.0f, 0.0f, 1.0f },
        { col[0],col[1],col[2],col[3], sx2*2.0f-1.0f, 1.0f-sy2*2.0f, 0.0f, 1.0f },
    };

    MikuPan_PipelineInfo *pl_info = MikuPan_GetPipelineInfo(COLOUR4_POSITION4);
    queue_upload(pl_info->buffers[0].buffer, 0, verts, sizeof(verts), false);

    SDL_UnmapGPUTransferBuffer(mikupan_render.device, s_transfer_buf);
    s_transfer_ptr = NULL;
    flush_uploads(s_cmd_buf);
    s_transfer_ptr = SDL_MapGPUTransferBuffer(mikupan_render.device,
                                              s_transfer_buf, false); // cycle=false: see draw_sprite_pass

    SDL_GPUColorTargetInfo cti = {
        .texture  = s_resolve_tex ? s_resolve_tex : s_color_tex,
        .load_op  = SDL_GPU_LOADOP_LOAD,
        .store_op = SDL_GPU_STOREOP_STORE,
    };
    // GPU_PIPELINE_LINE_2D has has_depth_stencil_target=true (from make_pipeline),
    // so the render pass must also provide a depth attachment or Metal drops the draw.
    SDL_GPUDepthStencilTargetInfo dti = {
        .texture          = s_depth_tex,
        .load_op          = SDL_GPU_LOADOP_LOAD,
        .store_op         = SDL_GPU_STOREOP_STORE,
        .stencil_load_op  = SDL_GPU_LOADOP_LOAD,
        .stencil_store_op = SDL_GPU_STOREOP_STORE,
    };
    SDL_GPURenderPass *pass = SDL_BeginGPURenderPass(s_cmd_buf, &cti, 1, &dti);

    SDL_GPUViewport vp = {
        .x=0, .y=0, .w=(float)s_render_w, .h=(float)s_render_h,
        .min_depth=0.0f, .max_depth=1.0f
    };
    SDL_SetGPUViewport(pass, &vp);
    SDL_BindGPUGraphicsPipeline(pass, MikuPan_GetGpuPipeline(GPU_PIPELINE_LINE_2D));

    SDL_GPUBufferBinding vb = { .buffer = pl_info->buffers[0].buffer, .offset = 0 };
    SDL_BindGPUVertexBuffers(pass, 0, &vb, 1);
    SDL_DrawGPUPrimitives(pass, 2, 1, 0, 0);

    SDL_EndGPURenderPass(pass);
}

// ─────────────────────────────────────────────────────────────────────────────
// Bounding box rendering
// Input: 24 float4 vertices (6 faces × 4 verts). Reformatted to 30 (5 per face)
// by repeating each face's first vertex, then drawn as 6 LINESTRIP calls.
// ─────────────────────────────────────────────────────────────────────────────

void MikuPan_RenderBoundingBox(sceVu0FVECTOR *vertices)
{
    if (!MikuPan_IsBoundingBoxRendering()) return;

    float bb_color[4] = { 0.0f, 1.0f, 0.0f, 1.0f };
    MikuPan_SetUniform4fvToCurrentShader(bb_color, "uColor");

    // Build a 30-vertex buffer: 6 faces × 5 verts (v0,v1,v2,v3,v0)
    float bb_verts[30][4];
    for (int f = 0; f < 6; f++)
    {
        for (int v = 0; v < 4; v++)
        {
            bb_verts[f * 5 + v][0] = vertices[f * 4 + v][0];
            bb_verts[f * 5 + v][1] = vertices[f * 4 + v][1];
            bb_verts[f * 5 + v][2] = vertices[f * 4 + v][2];
            bb_verts[f * 5 + v][3] = vertices[f * 4 + v][3];
        }
        // Close the loop
        bb_verts[f * 5 + 4][0] = vertices[f * 4][0];
        bb_verts[f * 5 + 4][1] = vertices[f * 4][1];
        bb_verts[f * 5 + 4][2] = vertices[f * 4][2];
        bb_verts[f * 5 + 4][3] = vertices[f * 4][3];
    }

    MikuPan_PipelineInfo *pl_info = MikuPan_GetPipelineInfo(POSITION4);
    queue_upload(pl_info->buffers[0].buffer, 0, bb_verts, sizeof(bb_verts), false);

    SDL_UnmapGPUTransferBuffer(mikupan_render.device, s_transfer_buf);
    s_transfer_ptr = NULL;
    flush_uploads(s_cmd_buf);
    s_transfer_ptr = SDL_MapGPUTransferBuffer(mikupan_render.device,
                                              s_transfer_buf, false); // cycle=false: see draw_sprite_pass

    SDL_GPUColorTargetInfo cti = {
        .texture  = s_resolve_tex ? s_resolve_tex : s_color_tex,
        .load_op  = SDL_GPU_LOADOP_LOAD,
        .store_op = SDL_GPU_STOREOP_STORE,
    };
    SDL_GPUDepthStencilTargetInfo dti = {
        .texture          = s_depth_tex,
        .load_op          = SDL_GPU_LOADOP_LOAD,
        .store_op         = SDL_GPU_STOREOP_STORE,
        .stencil_load_op  = SDL_GPU_LOADOP_LOAD,
        .stencil_store_op = SDL_GPU_STOREOP_STORE,
    };
    SDL_GPURenderPass *pass = SDL_BeginGPURenderPass(s_cmd_buf, &cti, 1, &dti);

    SDL_GPUViewport vp = {
        .x=0, .y=0, .w=(float)s_render_w, .h=(float)s_render_h,
        .min_depth=0.0f, .max_depth=1.0f
    };
    SDL_SetGPUViewport(pass, &vp);

    SDL_GPUGraphicsPipeline *pipe = MikuPan_GetGpuPipeline(GPU_PIPELINE_BOUNDING_BOX);
    SDL_BindGPUGraphicsPipeline(pass, pipe);
    SDL_PushGPUVertexUniformData(s_cmd_buf, 0, &g_bb_uniforms, sizeof(g_bb_uniforms));

    SDL_GPUBufferBinding vb = { .buffer = pl_info->buffers[0].buffer, .offset = 0 };
    SDL_BindGPUVertexBuffers(pass, 0, &vb, 1);

    for (int f = 0; f < 6; f++)
    {
        SDL_DrawGPUPrimitives(pass, 5, 1, (Uint32)(f * 5), 0);
    }

    SDL_EndGPURenderPass(pass);
}

// ─────────────────────────────────────────────────────────────────────────────
// 3D mesh rendering helpers
// ─────────────────────────────────────────────────────────────────────────────

// Execute an indexed mesh draw with uploads already queued.
static void draw_mesh_indexed(SDL_GPURenderPass       *pass,
                              SDL_GPUGraphicsPipeline *pipe,
                              MikuPan_PipelineInfo    *pl_info,
                              MikuPan_TextureInfo     *tex_info,
                              int                      shader_type,
                              Uint32                   index_count)
{
    SDL_BindGPUGraphicsPipeline(pass, pipe);
    SDL_PushGPUVertexUniformData(s_cmd_buf, 0, &g_vert_uniforms, sizeof(g_vert_uniforms));
    SDL_PushGPUFragmentUniformData(s_cmd_buf, 0, &s_light_data, sizeof(s_light_data));
    SDL_PushGPUFragmentUniformData(s_cmd_buf, 1, &g_frag_misc_uniforms, sizeof(g_frag_misc_uniforms));

    if (tex_info)
    {
        SDL_GPUTextureSamplerBinding tb = {
            .texture = tex_info->texture, .sampler = g_gpu_sampler };
        SDL_BindGPUFragmentSamplers(pass, 0, &tb, 1);
    }

    SDL_GPUBufferBinding vb0 = { .buffer = pl_info->buffers[0].buffer, .offset = 0 };
    SDL_GPUBufferBinding vb1 = { .buffer = pl_info->buffers[1].buffer, .offset = 0 };
    SDL_BindGPUVertexBuffers(pass, 0, &vb0, 1);
    SDL_BindGPUVertexBuffers(pass, 1, &vb1, 1);

    SDL_GPUBufferBinding ib = { .buffer = g_index_buffer, .offset = 0 };
    SDL_BindGPUIndexBuffer(pass, &ib, SDL_GPU_INDEXELEMENTSIZE_32BIT);

    SDL_DrawGPUIndexedPrimitives(pass, index_count, 1, 0, 0, 0);
    s_draw_calls++;
}

// ─────────────────────────────────────────────────────────────────────────────
// MikuPan_RenderMeshType0x32
// ─────────────────────────────────────────────────────────────────────────────

void MikuPan_RenderMeshType0x32(SGDPROCUNITHEADER *pVUVN,
                                 SGDPROCUNITHEADER *pPUHead)
{
    char mesh_type = GET_MESH_TYPE(pPUHead);

    if ((mesh_type != 0x12 && mesh_type != 0x10 && mesh_type != 0x32) ||
        ((mesh_type == 0x12 || mesh_type == 0x10) && !MikuPan_IsMesh0x12Rendering()) ||
        (mesh_type == 0x32 && !MikuPan_IsMesh0x32Rendering()))
    {
        return;
    }

    bool wire = MikuPan_IsWireframeRendering();
    bool mirror = MikuPan_IsMirrorRendering();
    MikuPan_GpuPipelineType pt = mirror ? GPU_PIPELINE_MESH_0x12_MIRROR
                                : wire   ? GPU_PIPELINE_MESH_0x12_WIREFRAME
                                         : GPU_PIPELINE_MESH_0x12_SOLID;

    MikuPan_PipelineInfo *pl_info = MikuPan_GetPipelineInfo(POSITION3_NORMAL3_UV2);

    SGDPROCUNITDATA      *pVUVNData  = (SGDPROCUNITDATA *)&pVUVN[1];
    SGDPROCUNITDATA      *pProcData  = (SGDPROCUNITDATA *)&pPUHead[1];
    SGDVUMESHPOINTNUM    *pMeshInfo  = (SGDVUMESHPOINTNUM *)&pPUHead[4];
    SGDVUMESHSTDATA      *sgdMeshData =
        (SGDVUMESHSTDATA *)((int64_t)pProcData +
                            (pProcData->VUMeshData_Preset.sOffsetToST - 1) * 4);
    _SGDVUMESHCOLORDATA  *pVMCD =
        (_SGDVUMESHCOLORDATA *)(&pPUHead->pNext +
                                pProcData->VUMeshData_Preset.sOffsetToPrim);

    sceGsTex0 *mesh_tex_reg = (sceGsTex0 *)((int64_t)pProcData + 0x28);
    uint64_t hash = MikuPan_GetTextureHash(mesh_tex_reg);
    MikuPan_TextureInfo *tex_info = MikuPan_GetTextureInfo(hash);
    if (!tex_info) tex_info = MikuPan_CreateGPUTexture(mesh_tex_reg);

    float *vertices = NULL;
    if (mesh_type == 0x32)
        vertices = (float *)(pVUVNData->VUVNData_Preset.aui
                             + pVUVN->VUVNDesc.sNumNormal * 3 + 10);
    else
        vertices = (float *)&((int *)pVUVN)[14];

    int vertex_offset = 0;
    int num_mesh      = GET_NUM_MESH(pPUHead);
    SGDMESHVERTEXDATA_TYPE2 *buf = (SGDMESHVERTEXDATA_TYPE2 *)s_temp_render_buffer;

    for (int i = 0; i < num_mesh; i++)
    {
        pVMCD = (_SGDVUMESHCOLORDATA *)MikuPan_GetNextUnpackAddr((u_int *)pVMCD);
        int vertex_count = pVMCD->VifUnpack.NUM;

        MikuPan_FixUV((float *)&sgdMeshData->astData, vertex_count);
        MikuPan_SetTriangleIndex(s_vertex_index, vertex_count, vertex_offset, i);

        if (mesh_type == 0x32)
        {
            float *normals = (float *)(pVUVNData->VUVNData_Preset.aui + i * 3 + 10);
            for (int j = 0; j < vertex_count; j++)
            {
                int k = vertex_offset + j;
                buf[k].vVertex[0] = vertices[k * 3 + 0];
                buf[k].vVertex[1] = vertices[k * 3 + 1];
                buf[k].vVertex[2] = vertices[k * 3 + 2];
                buf[k].vNormal[0] = normals[0];
                buf[k].vNormal[1] = normals[1];
                buf[k].vNormal[2] = normals[2];
            }
        }

        queue_upload(pl_info->buffers[1].buffer,
                     (Uint32)(vertex_offset * pl_info->buffers[1].attributes[0].stride),
                     sgdMeshData->astData,
                     (Uint32)(vertex_count * pl_info->buffers[1].attributes[0].stride),
                     false);

        vertex_offset += vertex_count;
        sgdMeshData = (SGDVUMESHSTDATA *)&sgdMeshData->astData[vertex_count];
        pVMCD = (_SGDVUMESHCOLORDATA *)&pVMCD->avColor[vertex_count];
    }

    size_t vert_bytes = pVUVN->VUVNDesc.sNumVertex *
                        (size_t)pl_info->buffers[0].attributes[0].stride;
    if (mesh_type == 0x32)
        queue_upload(pl_info->buffers[0].buffer, 0, buf, (Uint32)vert_bytes, false);
    else
        queue_upload(pl_info->buffers[0].buffer, 0, vertices, (Uint32)vert_bytes, false);

    Uint32 index_count = (Uint32)(pVUVN->VUVNDesc.sNumVertex
                                  + 2 * (num_mesh - 1));
    queue_upload(g_index_buffer, 0,
                 s_vertex_index, index_count * sizeof(int), true);

    // Flush uploads and draw.  Remap with cycle=false — see draw_sprite_pass.
    SDL_UnmapGPUTransferBuffer(mikupan_render.device, s_transfer_buf);
    s_transfer_ptr = NULL;
    flush_uploads(s_cmd_buf);
    s_transfer_ptr = SDL_MapGPUTransferBuffer(mikupan_render.device,
                                              s_transfer_buf, false);

    SDL_GPUColorTargetInfo cti = {
        .texture  = s_resolve_tex ? s_resolve_tex : s_color_tex,
        .load_op  = SDL_GPU_LOADOP_LOAD, .store_op = SDL_GPU_STOREOP_STORE,
    };
    SDL_GPUDepthStencilTargetInfo dti = {
        .texture  = s_depth_tex,
        .load_op  = SDL_GPU_LOADOP_LOAD, .store_op = SDL_GPU_STOREOP_STORE,
        .stencil_load_op = SDL_GPU_LOADOP_LOAD,
        .stencil_store_op = SDL_GPU_STOREOP_STORE,
    };
    SDL_GPURenderPass *pass = SDL_BeginGPURenderPass(s_cmd_buf, &cti, 1, &dti);
    SDL_GPUViewport vp = { .x=0,.y=0,.w=(float)s_render_w,.h=(float)s_render_h,
                           .min_depth=0,.max_depth=1 };
    SDL_SetGPUViewport(pass, &vp);

    draw_mesh_indexed(pass, MikuPan_GetGpuPipeline(pt), pl_info,
                      tex_info, MESH_0x12_SHADER, index_count);

    SDL_EndGPURenderPass(pass);
}

// ─────────────────────────────────────────────────────────────────────────────
// MikuPan_RenderMeshType0x82
// ─────────────────────────────────────────────────────────────────────────────

void MikuPan_RenderMeshType0x82(unsigned int *pVUVN, unsigned int *pPUHead)
{
    if (!MikuPan_IsMesh0x82Rendering()) return;

    SGDVUVNDATA_PRESET   *pVUVNData     = (SGDVUVNDATA_PRESET *)&(((SGDPROCUNITHEADER *)pVUVN)[1]);
    SGDVUMESHPOINTNUM    *pMeshInfo     = (SGDVUMESHPOINTNUM *)&(((SGDPROCUNITHEADER *)pPUHead)[4]);
    SGDPROCUNITDATA      *pProcData     = (SGDPROCUNITDATA *)&(((SGDPROCUNITHEADER *)pPUHead)[1]);
    SGDVUMESHSTREGSET    *sgdStRegSet   = (SGDVUMESHSTREGSET *)&pMeshInfo[GET_NUM_MESH(pPUHead)];
    SGDVUMESHSTDATA      *sgdMeshData   = (SGDVUMESHSTDATA *)&sgdStRegSet->auiVifCode[3];
    VUVN_PRIM            *v             = (VUVN_PRIM *)&((int *)pVUVN)[2];

    bool wire = MikuPan_IsWireframeRendering();
    bool mirror = MikuPan_IsMirrorRendering();
    MikuPan_GpuPipelineType pt = mirror ? GPU_PIPELINE_MESH_0x12_MIRROR
                                : wire   ? GPU_PIPELINE_MESH_0x12_WIREFRAME
                                         : GPU_PIPELINE_MESH_0x12_SOLID;

    MikuPan_PipelineInfo *pl_info = MikuPan_GetPipelineInfo(POSITION3_NORMAL3_UV2);

    sceGsTex0 *mesh_tex_reg = (sceGsTex0 *)((int64_t)pProcData + 0x18);
    uint64_t hash = MikuPan_GetTextureHash(mesh_tex_reg);
    MikuPan_TextureInfo *tex_info = MikuPan_GetTextureInfo(hash);
    if (!tex_info) tex_info = MikuPan_CreateGPUTexture(mesh_tex_reg);

    queue_upload(pl_info->buffers[0].buffer, 0,
                 pVUVNData->avt2,
                 (Uint32)(v->vnum * pl_info->buffers[0].attributes[0].stride),
                 false);

    int vertex_offset = 0;
    int num_mesh = GET_NUM_MESH(pPUHead);

    for (int i = 0; i < num_mesh; i++)
    {
        int vertex_count = pMeshInfo[i].uiPointNum;
        if (vertex_count == 0) continue;

        MikuPan_FixUV((float *)sgdMeshData->astData, vertex_count);
        MikuPan_SetTriangleIndex(s_vertex_index, vertex_count, vertex_offset, i);

        queue_upload(pl_info->buffers[1].buffer,
                     (Uint32)(vertex_offset * pl_info->buffers[1].attributes[0].stride),
                     sgdMeshData->astData,
                     (Uint32)(vertex_count * pl_info->buffers[1].attributes[0].stride),
                     false);

        sgdMeshData = (SGDVUMESHSTDATA *)&sgdMeshData->astData[vertex_count];
        vertex_offset += vertex_count;
    }

    Uint32 index_count = (Uint32)(v->nnum + 2 * (num_mesh - 1));
    queue_upload(g_index_buffer, 0,
                 s_vertex_index, index_count * sizeof(int), true);

    // cycle=false: see draw_sprite_pass for the full explanation.
    SDL_UnmapGPUTransferBuffer(mikupan_render.device, s_transfer_buf);
    s_transfer_ptr = NULL;
    flush_uploads(s_cmd_buf);
    s_transfer_ptr = SDL_MapGPUTransferBuffer(mikupan_render.device,
                                              s_transfer_buf, false);

    SDL_GPUColorTargetInfo cti = {
        .texture  = s_resolve_tex ? s_resolve_tex : s_color_tex,
        .load_op  = SDL_GPU_LOADOP_LOAD, .store_op = SDL_GPU_STOREOP_STORE,
    };
    SDL_GPUDepthStencilTargetInfo dti = {
        .texture  = s_depth_tex,
        .load_op  = SDL_GPU_LOADOP_LOAD, .store_op = SDL_GPU_STOREOP_STORE,
        .stencil_load_op = SDL_GPU_LOADOP_LOAD,
        .stencil_store_op = SDL_GPU_STOREOP_STORE,
    };
    SDL_GPURenderPass *pass = SDL_BeginGPURenderPass(s_cmd_buf, &cti, 1, &dti);
    SDL_GPUViewport vp = { .x=0,.y=0,.w=(float)s_render_w,.h=(float)s_render_h,
                           .min_depth=0,.max_depth=1 };
    SDL_SetGPUViewport(pass, &vp);
    draw_mesh_indexed(pass, MikuPan_GetGpuPipeline(pt), pl_info,
                      tex_info, MESH_0x12_SHADER, index_count);
    SDL_EndGPURenderPass(pass);
}

// ─────────────────────────────────────────────────────────────────────────────
// MikuPan_RenderMeshType0x2
// ─────────────────────────────────────────────────────────────────────────────

void MikuPan_RenderMeshType0x2(SGDPROCUNITHEADER *pVUVN,
                                SGDPROCUNITHEADER *pPUHead,
                                float             *vertices)
{
    if (!MikuPan_IsMesh0x2Rendering()) return;

    char mesh_type = GET_MESH_TYPE(pPUHead);
    int  shader_idx;
    MikuPan_GpuPipelineType pt_solid, pt_wire, pt_mirror;

    if (mesh_type == 0x2)
    {
        shader_idx = MESH_0x2_SHADER;
        pt_solid   = GPU_PIPELINE_MESH_0x2_SOLID;
        pt_wire    = GPU_PIPELINE_MESH_0x2_WIREFRAME;
        pt_mirror  = GPU_PIPELINE_MESH_0x2_MIRROR;
    }
    else if (mesh_type == 0xA)
    {
        shader_idx = MESH_0xA_SHADER;
        pt_solid   = GPU_PIPELINE_MESH_0xA_SOLID;
        pt_wire    = GPU_PIPELINE_MESH_0xA_WIREFRAME;
        pt_mirror  = GPU_PIPELINE_MESH_0xA_MIRROR;
    }
    else
    {
        return;
    }

    bool wire   = MikuPan_IsWireframeRendering();
    bool mirror = MikuPan_IsMirrorRendering();
    MikuPan_GpuPipelineType pt = mirror ? pt_mirror : wire ? pt_wire : pt_solid;

    MikuPan_PipelineInfo *pl_info = MikuPan_GetPipelineInfo(POSITION4_NORMAL4_UV2);
    VUVN_PRIM            *v          = (VUVN_PRIM *)&((int *)pVUVN)[2];
    SGDVUMESHPOINTNUM    *pMeshInfo  = (SGDVUMESHPOINTNUM *)&pPUHead[4];
    SGDVUMESHSTREGSET    *sgdStRegSet = (SGDVUMESHSTREGSET *)&pMeshInfo[GET_NUM_MESH(pPUHead)];
    SGDVUMESHSTDATA      *sgdMeshData = (SGDVUMESHSTDATA *)&sgdStRegSet->auiVifCode[3];
    SGDPROCUNITDATA      *pProcData   = (SGDPROCUNITDATA *)&pPUHead[1];

    sceGsTex0 *mesh_tex_reg = (sceGsTex0 *)((int64_t)pProcData + 0x18);
    uint64_t hash = MikuPan_GetTextureHash(mesh_tex_reg);
    MikuPan_TextureInfo *tex_info = MikuPan_GetTextureInfo(hash);
    if (!tex_info) tex_info = MikuPan_CreateGPUTexture(mesh_tex_reg);

    queue_upload(pl_info->buffers[0].buffer, 0,
                 vertices,
                 (Uint32)(v->vnum * pl_info->buffers[0].attributes[0].stride),
                 false);

    int vertex_offset = 0;
    int num_mesh = GET_NUM_MESH(pPUHead);

    for (int i = 0; i < num_mesh; i++)
    {
        int vertex_count = pMeshInfo[i].uiPointNum;

        if (pPUHead->VUMeshDesc.MeshType.TEX == 1)
            MikuPan_FixUV((float *)&sgdMeshData->astData, vertex_count);

        MikuPan_SetTriangleIndex(s_vertex_index, vertex_count, vertex_offset, i);

        queue_upload(pl_info->buffers[1].buffer,
                     (Uint32)(vertex_offset * pl_info->buffers[1].attributes[0].stride),
                     sgdMeshData->astData,
                     (Uint32)(vertex_count * pl_info->buffers[1].attributes[0].stride),
                     false);

        sgdMeshData = (SGDVUMESHSTDATA *)&sgdMeshData->astData[vertex_count];
        vertex_offset += vertex_count;
    }

    Uint32 index_count = (Uint32)(v->nnum + 2 * (num_mesh - 1));
    queue_upload(g_index_buffer, 0,
                 s_vertex_index, index_count * sizeof(int), true);

    // cycle=false: see draw_sprite_pass for the full explanation.
    SDL_UnmapGPUTransferBuffer(mikupan_render.device, s_transfer_buf);
    s_transfer_ptr = NULL;
    flush_uploads(s_cmd_buf);
    s_transfer_ptr = SDL_MapGPUTransferBuffer(mikupan_render.device,
                                              s_transfer_buf, false);

    SDL_GPUColorTargetInfo cti = {
        .texture  = s_resolve_tex ? s_resolve_tex : s_color_tex,
        .load_op  = SDL_GPU_LOADOP_LOAD, .store_op = SDL_GPU_STOREOP_STORE,
    };
    SDL_GPUDepthStencilTargetInfo dti = {
        .texture  = s_depth_tex,
        .load_op  = SDL_GPU_LOADOP_LOAD, .store_op = SDL_GPU_STOREOP_STORE,
        .stencil_load_op = SDL_GPU_LOADOP_LOAD,
        .stencil_store_op = SDL_GPU_STOREOP_STORE,
    };
    SDL_GPURenderPass *pass = SDL_BeginGPURenderPass(s_cmd_buf, &cti, 1, &dti);
    SDL_GPUViewport vp = { .x=0,.y=0,.w=(float)s_render_w,.h=(float)s_render_h,
                           .min_depth=0,.max_depth=1 };
    SDL_SetGPUViewport(pass, &vp);
    draw_mesh_indexed(pass, MikuPan_GetGpuPipeline(pt), pl_info,
                      tex_info, shader_idx, index_count);
    SDL_EndGPURenderPass(pass);
}

// ─────────────────────────────────────────────────────────────────────────────
// Shutdown
// ─────────────────────────────────────────────────────────────────────────────

void MikuPan_Shutdown(void)
{
    // Release all cached game textures before font textures so that
    // MikuPan_IsFontTexture() can still identify them via s_fnt_texture[].
    MikuPan_ShutdownTextureCache();

    for (int i = 0; i < 6; i++)
    {
        if (s_fnt_texture[i])
        {
            SDL_ReleaseGPUTexture(mikupan_render.device, s_fnt_texture[i]->texture);
            free(s_fnt_texture[i]);
            s_fnt_texture[i] = NULL;
        }
    }

    MikuPan_DestroyInternalBuffer();
    MikuPan_DestroyPipelines(mikupan_render.device);
    MikuPan_DestroyShaders(mikupan_render.device);

    if (s_transfer_buf)
    {
        SDL_ReleaseGPUTransferBuffer(mikupan_render.device, s_transfer_buf);
        s_transfer_buf = NULL;
    }

    SDL_ReleaseWindowFromGPUDevice(mikupan_render.device, mikupan_render.window);
    SDL_DestroyGPUDevice(mikupan_render.device);
    SDL_DestroyWindow(mikupan_render.window);
}
