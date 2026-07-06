#include "mikupan_renderer.h"
#include "../mikupan_types.h"
#include "SDL3/SDL_hints.h"
#include "SDL3/SDL_init.h"
#include "SDL3/SDL_timer.h"
#include "cglm/cglm.h"
#include "graphics/graph2d/message.h"
#include "graphics/graph3d/sgsu.h"
#include "mikupan/gs/mikupan_gs_c.h"
#include "mikupan/gs/mikupan_texture_manager_c.h"
#include "mikupan/io/mikupan_file_c.h"
#include "mikupan/debug/mikupan_logging_c.h"
#include "mikupan/mikupan_screenshot.h"
#include "mikupan/ui/mikupan_ui.h"
#include "mikupan/ui/mikupan_ui_debug.h"
#include "mikupan_gpu.h"
#include "mikupan_profiler.h"
#include "mikupan_renderer_internal.h"
#include "mikupan_shader.h"

#include <mikupan_version.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

static void MikuPan_ApplyWindowMode(int mode);
static int MikuPan_RenderSceneToPhotoFramebuffer(void);
static int MikuPan_RendererIsSuperSampling(SDL_Window* window);
static void MikuPan_GetPhotoFramebufferTargetSize(int *width, int *height);
static int MikuPan_EnsurePhotoFramebuffer(void);
static void MikuPan_ConvertPs2RectToRenderTextureUv(float *out,
                                                    float x, float y,
                                                    float w, float h);

#include "graphics/graph3d/sglib.h"
#include "mikupan/mikupan_config.h"
#include "mikupan/mikupan_utils.h"
#include "mikupan_meshcache.h"
#include "mikupan_pipeline.h"

#include <glad/gl.h>

MikuPan_RenderWindow mikupan_render = {0};
MikuPan_MsaaBufferObject render_back_msaa = {0};
static int g_mirror_scissor_enabled = 0;
static int g_mirror_reflection_pass = 0;
static unsigned int g_mirror_texture_id = 0;
static unsigned int g_mirror_depth_id = 0;
static int g_mirror_texture_width = 0;
static int g_mirror_texture_height = 0;
static int g_mirror_texture_valid = 0;
static unsigned int g_scene_history_id[2] = {0, 0};
static int g_scene_history_valid[2] = {0, 0};
static int g_scene_history_suppress_next_capture = 0;
static int g_photo_capture_suppress_screen_copy_effects = 0;
static unsigned int g_photo_framebuffer_id = 0;
static int g_photo_framebuffer_valid = 0;
static int g_photo_framebuffer_width = 0;
static int g_photo_framebuffer_height = 0;
static unsigned int g_photo_capture_readback_texture_id = 0;
static int g_photo_capture_readback_texture_width = 0;
static int g_photo_capture_readback_texture_height = 0;
static unsigned int g_screen_negative_scene_copy_id = 0;
static unsigned int g_pause_capture_texture_id = 0;
static int g_pause_capture_valid = 0;
static int g_pause_capture_addr = 0;
static int g_pause_capture_width = 0;
static int g_pause_capture_height = 0;
static unsigned int g_enemy_out_capture_texture_id[3] = {0, 0, 0};
static int g_enemy_out_capture_valid[3] = {0, 0, 0};
static int g_enemy_out_capture_width[3] = {0, 0, 0};
static int g_enemy_out_capture_height[3] = {0, 0, 0};

static int MikuPan_IsValidEnemyOutCaptureSlot(int slot)
{
    return slot >= 0 && slot < 3;
}

static void MikuPan_ReleaseEnemyOutCaptureSlot(int slot)
{
    if (!MikuPan_IsValidEnemyOutCaptureSlot(slot))
    {
        return;
    }

    if (g_enemy_out_capture_texture_id[slot] != 0)
    {
        MikuPan_GPUReleaseTexture(g_enemy_out_capture_texture_id[slot]);
        g_enemy_out_capture_texture_id[slot] = 0;
    }

    g_enemy_out_capture_valid[slot] = 0;
    g_enemy_out_capture_width[slot] = 0;
    g_enemy_out_capture_height[slot] = 0;
}

static void MikuPan_ReleaseEnemyOutCaptures(void)
{
    for (int i = 0; i < 3; i++)
    {
        MikuPan_ReleaseEnemyOutCaptureSlot(i);
    }
}

void MikuPan_StreamUploadFull(GLenum target, GLuint buffer,
                                            GLsizeiptr size, const void *data)
{
    (void)target;
    if (size <= 0)
    {
        return;
    }

    Uint64 t0 = MikuPan_PerfBegin();

    MikuPan_GPUUploadBuffer(buffer, (unsigned int)size, data);

    MikuPan_PerfEnd(PERF_SECT_BUFFER_UPLOAD, t0);
}

int MikuPan_IsBlackWhiteModeActive(void)
{
    return loadbw_flg != 0;
}

static void MikuPan_ConvertRGBA8ToBlackWhite(unsigned char *rgba,
                                             int width,
                                             int height)
{
    const int pixel_count = width * height;

    for (int i = 0; i < pixel_count; i++)
    {
        unsigned char *p = rgba + i * 4;
        const unsigned int gray =
            ((unsigned int)p[0] + (unsigned int)p[1] + (unsigned int)p[2]) / 3;
        p[0] = (unsigned char)gray;
        p[1] = (unsigned char)gray;
        p[2] = (unsigned char)gray;
    }
}

static int MikuPan_RendererIsSuperSampling(SDL_Window* window)
{
    int window_width = 0;
    int window_height = 0;

#ifndef __ANDROID__
    if (window != NULL)
    {
        SDL_GetWindowSizeInPixels(window, &window_width, &window_height);
        if (window_width <= 0 || window_height <= 0)
        {
            SDL_GetWindowSize(window, &window_width, &window_height);
        }
    }
#else
    (void)window;
#endif

    if (window_width <= 0)
    {
        window_width = mikupan_configuration.renderer.window.width;
    }

    if (window_height <= 0)
    {
        window_height = mikupan_configuration.renderer.window.height;
    }

    if (window_width <= 0 || window_height <= 0)
    {
        return 0;
    }

    int render_width = mikupan_configuration.renderer.render.width;
    int render_height = mikupan_configuration.renderer.render.height;

    if (mikupan_configuration.renderer.render_mode
        == MIKUPAN_RENDER_RESOLUTION_MATCH_WINDOW)
    {
        render_width = window_width;
        render_height = window_height;
    }
    else if (mikupan_configuration.renderer.render_mode
             == MIKUPAN_RENDER_RESOLUTION_WINDOW_SCALE)
    {
        render_width = (int)roundf((float)window_width
                                   * (float)mikupan_configuration.renderer.render_scale_percent
                                   / 100.0f);
        render_height = (int)roundf((float)window_height
                                    * (float)mikupan_configuration.renderer.render_scale_percent
                                    / 100.0f);
    }

    return render_width > window_width || render_height > window_height;
}

SDL_AppResult MikuPan_Init()
{
    MikuPan_InitLogging();

    SDL_SetAppMetadata("MikuPan", MIKUPAN_GIT_DESCRIBE, "MikuPan");

    info_log("Initializing SDL");

    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD | SDL_INIT_JOYSTICK | SDL_INIT_HAPTIC))
    {
        info_log("Couldn't initialize SDL: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    MikuPan_LoadConfiguration(NULL);

    SDL_SetHint(SDL_HINT_MAIN_CALLBACK_RATE, "60");

    info_log("Loading SDL_GameControllerDB");

    char controller_db_path[1024];
    if (MikuPan_ResolveBasePath("resources/gamecontrollerdb.txt",
                                controller_db_path,
                                sizeof(controller_db_path))
        && !SDL_AddGamepadMappingsFromFile(controller_db_path))
    {
        info_log("Error adding the controller mapping, bindings might be wrong %s", SDL_GetError());
    }

    info_log("Creating SDL Window");

    SDL_DisplayID primary = SDL_GetPrimaryDisplay();
    const SDL_DisplayMode *mode = SDL_GetCurrentDisplayMode(primary);

    int desired_window_width = mikupan_configuration.renderer.window.width;
    int desired_window_height = mikupan_configuration.renderer.window.height;

    if (desired_window_width > mode->w)
    {
        desired_window_width = mode->w;
        mikupan_configuration.renderer.window.width = desired_window_width;
    }

    if (desired_window_height > mode->h)
    {
        desired_window_height = mode->h;
        mikupan_configuration.renderer.window.height = desired_window_height;
    }

    int config_window_flags =
#ifdef __ANDROID__
        SDL_WINDOW_FULLSCREEN;
#else
        SDL_WINDOW_RESIZABLE;
#endif

    int startup_window_mode = mikupan_configuration.renderer.window_mode;
#ifndef __ANDROID__
    if (startup_window_mode == MIKUPAN_WINDOW_FULLSCREEN)
    {
        startup_window_mode = MIKUPAN_WINDOW_BORDERLESS;
    }
#endif
#ifdef __ANDROID__
    startup_window_mode = MIKUPAN_WINDOW_FULLSCREEN;
#endif

    mikupan_configuration.renderer.window_mode = startup_window_mode;
    mikupan_configuration.renderer.is_fullscreen = (startup_window_mode != MIKUPAN_WINDOW_WINDOWED);

    if (startup_window_mode == MIKUPAN_WINDOW_FULLSCREEN)
    {
        config_window_flags |= SDL_WINDOW_FULLSCREEN;
    }
    else if (startup_window_mode == MIKUPAN_WINDOW_BORDERLESS)
    {
        config_window_flags |= SDL_WINDOW_BORDERLESS;
    }

    mikupan_render.window = SDL_CreateWindow(
        "MikuPan",
        desired_window_width,
        desired_window_height,
        config_window_flags
        );

    if (mikupan_render.window == NULL)
    {
        info_log("Error creating the SDL window %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

#ifndef __ANDROID__
    MikuPan_ApplyWindowMode(startup_window_mode);
#endif

    SDL_GetWindowSize(mikupan_render.window, &mikupan_render.width, &mikupan_render.height);
    if (startup_window_mode == MIKUPAN_WINDOW_WINDOWED)
    {
        mikupan_configuration.renderer.window.width = mikupan_render.width;
        mikupan_configuration.renderer.window.height = mikupan_render.height;
    }

#ifndef __ANDROID__
    char icon_path[1024];
    SDL_Surface* iconSurface = NULL;
    if (MikuPan_ResolveBasePath("resources/mikupan.png",
                                icon_path,
                                sizeof(icon_path)))
    {
        iconSurface = SDL_LoadPNG(icon_path);
    }

    if (iconSurface == NULL)
    {
        info_log("Error loading the icon %s", SDL_GetError());
    }
    else if (!SDL_SetWindowIcon(mikupan_render.window, iconSurface))
    {
        info_log("Error creating the icon %s", SDL_GetError());
    }

    if (iconSurface != NULL)
    {
        SDL_DestroySurface(iconSurface);
    }
#endif

    int desired_msaa = mikupan_configuration.renderer.msaa_index;
    int desired_vsync = mikupan_configuration.renderer.vsync;

    const int msaa_list[] = {0, 2, 4, 8};
    if (desired_msaa < 0
        || desired_msaa >= (int)(sizeof(msaa_list) / sizeof(msaa_list[0])))
    {
        desired_msaa = 0;
        mikupan_configuration.renderer.msaa_index = desired_msaa;
    }

    if (MikuPan_RendererIsSuperSampling(mikupan_render.window))
    {
        desired_msaa = 0;
        mikupan_configuration.renderer.msaa_index = desired_msaa;
    }

#ifdef __ANDROID__
    desired_msaa = 0;
    mikupan_configuration.renderer.msaa_index = desired_msaa;
#endif

    if (!MikuPan_GPUInit(mikupan_render.window, desired_vsync,
                         mikupan_configuration.renderer.gpu_driver,
                         mikupan_configuration.renderer.gpu_debug))
    {
        return SDL_APP_FAILURE;
    }

    MikuPan_InitUi(mikupan_render.window);
    MikuPan_CreateInternalBuffer(MikuPan_GetRenderResolutionWidth(),
                                 MikuPan_GetRenderResolutionHeight(),
                                 msaa_list[desired_msaa]);
    if (MikuPan_InitShaders() != 0)
    {
        info_log("Error initializing shaders");
        return SDL_APP_FAILURE;
    }
    MikuPan_InitPipeline();
    MikuPan_MeshCache_Init();

    MikuPan_Setup3D();

    info_log("Loaded video device: %s", SDL_GetGPUDeviceDriver(MikuPan_GPUGetDevice()));

    return SDL_APP_CONTINUE;
}

void MikuPan_DestroyInternalBuffer()
{
    for (int i = 0; i < 2; i++)
    {
        if (g_scene_history_id[i] != 0)
        {
            MikuPan_GPUReleaseTexture(g_scene_history_id[i]);
            g_scene_history_id[i] = 0;
        }
        g_scene_history_valid[i] = 0;
    }

    if (g_photo_framebuffer_id != 0)
    {
        MikuPan_GPUReleaseTexture(g_photo_framebuffer_id);
        g_photo_framebuffer_id = 0;
    }
    g_photo_framebuffer_valid = 0;
    g_photo_framebuffer_width = 0;
    g_photo_framebuffer_height = 0;
    MikuPan_ClearPhotoNegativeSourceTextureReference();

    if (g_photo_capture_readback_texture_id != 0)
    {
        MikuPan_GPUReleaseTexture(g_photo_capture_readback_texture_id);
        g_photo_capture_readback_texture_id = 0;
    }
    g_photo_capture_readback_texture_width = 0;
    g_photo_capture_readback_texture_height = 0;

    if (g_screen_negative_scene_copy_id != 0)
    {
        MikuPan_GPUReleaseTexture(g_screen_negative_scene_copy_id);
        g_screen_negative_scene_copy_id = 0;
    }

    MikuPan_ReleaseEnemyOutCaptures();

    MikuPan_GPUDestroyInternalBuffer();
    memset(&render_back_msaa, 0, sizeof(render_back_msaa));
}

static void MikuPan_CopyTextureAspectCropRGBA8(const unsigned char *src_rgba,
                                                int src_w,
                                                int src_h,
                                                int dst_w,
                                                int dst_h,
                                                unsigned char *dst_rgba)
{
    if (src_rgba == NULL || dst_rgba == NULL ||
        src_w <= 0 || src_h <= 0 || dst_w <= 0 || dst_h <= 0)
    {
        return;
    }

    const float src_aspect = (float)src_w / (float)src_h;
    const float dst_aspect = (float)dst_w / (float)dst_h;
    int crop_x = 0;
    int crop_y = 0;
    int crop_w = src_w;
    int crop_h = src_h;

    if (src_aspect > dst_aspect)
    {
        crop_w = (int)((float)src_h * dst_aspect + 0.5f);
        if (crop_w < 1) crop_w = 1;
        if (crop_w > src_w) crop_w = src_w;
        crop_x = (src_w - crop_w) / 2;
    }
    else
    {
        crop_h = (int)((float)src_w / dst_aspect + 0.5f);
        if (crop_h < 1) crop_h = 1;
        if (crop_h > src_h) crop_h = src_h;
        crop_y = (src_h - crop_h) / 2;
    }

    for (int y = 0; y < dst_h; y++)
    {
        int sy = crop_y + (int)(((long long)y * crop_h) / dst_h);
        if (sy < 0) sy = 0;
        if (sy >= src_h) sy = src_h - 1;

        for (int x = 0; x < dst_w; x++)
        {
            int sx = crop_x + (int)(((long long)x * crop_w) / dst_w);
            if (sx < 0) sx = 0;
            if (sx >= src_w) sx = src_w - 1;

            memcpy(dst_rgba + ((size_t)y * dst_w + x) * 4u,
                   src_rgba + ((size_t)sy * src_w + sx) * 4u,
                   4);
        }
    }
}

static int MikuPan_ReadTextureRGBA8TopLeft(unsigned int texture_id,
                                           int src_w,
                                           int src_h,
                                           int width,
                                           int height,
                                           unsigned char *out_rgba)
{
    if (width <= 0 || height <= 0 || out_rgba == NULL)
    {
        return 0;
    }

    if (texture_id == 0 || src_w <= 0 || src_h <= 0)
    {
        return 0;
    }

    if (width == src_w && height == src_h)
    {
        if (!MikuPan_GPUReadTextureRGBA8(texture_id, width, height, out_rgba))
        {
            return 0;
        }
    }
    else
    {
        const size_t src_bytes = (size_t)src_w * (size_t)src_h * 4u;
        unsigned char *src_rgba = (unsigned char *)malloc(src_bytes);
        if (src_rgba == NULL)
        {
            return 0;
        }
        if (!MikuPan_GPUReadTextureRGBA8(texture_id, src_w, src_h, src_rgba))
        {
            free(src_rgba);
            return 0;
        }

        MikuPan_CopyTextureAspectCropRGBA8(src_rgba, src_w, src_h,
                                           width, height, out_rgba);
        free(src_rgba);
    }

    if (MikuPan_IsBlackWhiteModeActive())
    {
        MikuPan_ConvertRGBA8ToBlackWhite(out_rgba, width, height);
    }

    return 1;
}

static int MikuPan_ReadSceneTextureRGBA8TopLeft(int width,
                                                int height,
                                                unsigned char *out_rgba)
{
    return MikuPan_ReadTextureRGBA8TopLeft(render_back_msaa.texture.id,
                                           render_back_msaa.texture.width,
                                           render_back_msaa.texture.height,
                                           width, height, out_rgba);
}

static int MikuPan_ReadFramebufferRGBA8TopLeftFromFbo(GLuint src_fbo,
                                                      int width,
                                                      int height,
                                                      unsigned char *out_rgba)
{
    (void)src_fbo;
    return MikuPan_ReadSceneTextureRGBA8TopLeft(width, height, out_rgba);
}

static void MikuPan_SetPostprocessPassthroughUniforms(int source_width,
                                                     int source_height,
                                                     int output_width,
                                                     int output_height)
{
    MikuPan_SetUniform1iToCurrentShader(0, "uTexture");
    MikuPan_SetUniform1iToCurrentShader(1, "uPhotoNegativeSourceTexture");
    MikuPan_SetUniform1iToCurrentShader(0, "uBlackWhiteMode");
    MikuPan_SetUniform1fToCurrentShader(1.0f, "uBrightness");
    MikuPan_SetUniform1fToCurrentShader(1.0f, "uGamma");
    MikuPan_SetUniform1fToCurrentShader(1.0f, "uContrast");
    MikuPan_SetUniform1iToCurrentShader(0, "uCrtEnabled");
    MikuPan_SetUniform1fToCurrentShader(0.0f, "uCrtStrength");
    MikuPan_SetUniform1fToCurrentShader(0.0f, "uCrtCurvature");
    MikuPan_SetUniform1fToCurrentShader(0.0f, "uCrtOverscan");
    MikuPan_SetUniform1fToCurrentShader(0.0f, "uCrtScanlineStrength");
    MikuPan_SetUniform1fToCurrentShader(1.0f, "uCrtScanlineScale");
    MikuPan_SetUniform1fToCurrentShader(1.0f, "uCrtScanlineThickness");
    MikuPan_SetUniform1fToCurrentShader(0.0f, "uCrtMaskStrength");
    MikuPan_SetUniform1fToCurrentShader(1.0f, "uCrtMaskScale");
    MikuPan_SetUniform1fToCurrentShader(0.0f, "uCrtVignetteStrength");
    MikuPan_SetUniform1fToCurrentShader(1.0f, "uCrtVignetteSize");
    MikuPan_SetUniform1fToCurrentShader(0.0f, "uCrtChromaOffset");
    MikuPan_SetUniform1fToCurrentShader(0.0f, "uCrtBlendStrength");
    MikuPan_SetUniform1fToCurrentShader(1.0f, "uCrtBlendRadius");
    MikuPan_SetUniform1fToCurrentShader(0.0f, "uCrtNoiseStrength");
    MikuPan_SetUniform1fToCurrentShader(0.0f, "uCrtFlickerStrength");
    MikuPan_SetUniform1fToCurrentShader(0.0f, "uCrtGlowStrength");
    MikuPan_SetUniform2fToCurrentShader((float)source_width,
                                        (float)source_height,
                                        "uTextureSize");
    MikuPan_SetUniform2fToCurrentShader((float)output_width,
                                        (float)output_height,
                                        "uOutputSize");
    MikuPan_SetUniform1fToCurrentShader(0.0f, "uTime");
    MikuPan_SetUniform1iToCurrentShader(0, "uPhotoNegativeEnabled");
    MikuPan_SetUniform1iToCurrentShader(0, "uPhotoNegativeSourceEnabled");
    {
        float full_rect[4] = {0.0f, 0.0f, 1.0f, 1.0f};
        MikuPan_SetUniform4fvToCurrentShader(full_rect,
                                             "uPhotoNegativeContentRect");
        MikuPan_SetUniform4fvToCurrentShader(full_rect,
                                             "uPhotoNegativeRect");
    }
    MikuPan_SetUniform1fToCurrentShader(0.0f, "uPhotoNegativeStrength");
    {
        float screen_negative[4] = {0.0f, 0.0f, 0.0f, 0.0f};
        MikuPan_SetUniform4fvToCurrentShader(screen_negative,
                                             "uScreenNegative");
    }
}

static void MikuPan_GetPhotoFramebufferTargetSize(int *width, int *height)
{
    int w = 0;
    int h = 0;

    if (mikupan_render.window != NULL)
    {
        SDL_GetWindowSizeInPixels(mikupan_render.window, &w, &h);
    }

    if (w <= 0 || h <= 0)
    {
        w = render_back_msaa.texture.width;
        h = render_back_msaa.texture.height;
    }

    if (w <= 0) w = 640;
    if (h <= 0) h = 448;

    if (width != NULL) *width = w;
    if (height != NULL) *height = h;
}

static int MikuPan_EnsurePhotoFramebuffer(void)
{
    int width = 0;
    int height = 0;
    MikuPan_GetPhotoFramebufferTargetSize(&width, &height);

    if (width <= 0 || height <= 0)
    {
        return 0;
    }

    if (g_photo_framebuffer_id != 0 &&
        g_photo_framebuffer_width == width &&
        g_photo_framebuffer_height == height)
    {
        return 1;
    }

    if (g_photo_framebuffer_id != 0)
    {
        MikuPan_GPUReleaseTexture(g_photo_framebuffer_id);
        g_photo_framebuffer_id = 0;
        MikuPan_ClearPhotoNegativeSourceTextureReference();
    }

    g_photo_framebuffer_width = width;
    g_photo_framebuffer_height = height;
    g_photo_framebuffer_id = MikuPan_GPUCreateRenderTextureRGBA8(width, height);
    g_photo_framebuffer_valid = 0;

    return g_photo_framebuffer_id != 0;
}


static int MikuPan_EnsurePhotoCaptureReadbackTexture(int width, int height)
{
    if (width <= 0 || height <= 0)
    {
        return 0;
    }

    if (g_photo_capture_readback_texture_id != 0 &&
        g_photo_capture_readback_texture_width == width &&
        g_photo_capture_readback_texture_height == height)
    {
        return 1;
    }

    if (g_photo_capture_readback_texture_id != 0)
    {
        MikuPan_GPUReleaseTexture(g_photo_capture_readback_texture_id);
        g_photo_capture_readback_texture_id = 0;
    }

    g_photo_capture_readback_texture_width = width;
    g_photo_capture_readback_texture_height = height;
    g_photo_capture_readback_texture_id =
        MikuPan_GPUCreateRenderTextureRGBA8(width, height);

    if (g_photo_capture_readback_texture_id == 0)
    {
        g_photo_capture_readback_texture_width = 0;
        g_photo_capture_readback_texture_height = 0;
    }

    return g_photo_capture_readback_texture_id != 0;
}

static void MikuPan_ConvertPs2RectToTextureUv(float *out,
                                              int texture_width,
                                              int texture_height,
                                              float x, float y,
                                              float w, float h)
{
    float viewport_x = 0.0f;
    float viewport_y = 0.0f;
    float viewport_w = 0.0f;
    float viewport_h = 0.0f;
    float viewport_scale = 1.0f;

    MikuPan_GetPS2Viewport(texture_width, texture_height,
                           &viewport_x, &viewport_y,
                           &viewport_w, &viewport_h,
                           &viewport_scale);

    float x0 = viewport_x + x * viewport_scale;
    float y0 = viewport_y + y * viewport_scale;
    float x1 = viewport_x + (x + w) * viewport_scale;
    float y1 = viewport_y + (y + h) * viewport_scale;

    if (x0 < 0.0f) x0 = 0.0f;
    if (y0 < 0.0f) y0 = 0.0f;
    if (x1 > (float)texture_width) x1 = (float)texture_width;
    if (y1 > (float)texture_height) y1 = (float)texture_height;
    if (x1 < x0) x1 = x0;
    if (y1 < y0) y1 = y0;

    out[0] = texture_width > 0 ? x0 / (float)texture_width : 0.0f;
    out[1] = texture_height > 0 ? y0 / (float)texture_height : 0.0f;
    out[2] = texture_width > 0 ? x1 / (float)texture_width : 1.0f;
    out[3] = texture_height > 0 ? y1 / (float)texture_height : 1.0f;
}

static int MikuPan_RenderSceneToPhotoFramebuffer(void)
{
    if (render_back_msaa.texture.id == 0 ||
        render_back_msaa.texture.width <= 0 ||
        render_back_msaa.texture.height <= 0 ||
        !MikuPan_EnsurePhotoFramebuffer())
    {
        return 0;
    }

    MikuPan_GPUResolveSceneForSampling();

    float quad[] = {
        0,1,0,0,   1,1,1,1,   -1,-1,0,1,
        1,1,0,0,   1,1,1,1,    1,-1,0,1,
        0,0,0,0,   1,1,1,1,   -1, 1,0,1,
        1,0,0,0,   1,1,1,1,    1, 1,0,1
    };

    MikuPan_SetViewportCached(0, 0,
                              g_photo_framebuffer_width,
                              g_photo_framebuffer_height);
    MikuPan_GPUSetScreenCopyTarget(g_photo_framebuffer_id,
                                   g_photo_framebuffer_width,
                                   g_photo_framebuffer_height,
                                   1);
    MikuPan_BindTexture2DCached(render_back_msaa.texture.id);
    MikuPan_SetRenderState2D();
    MikuPan_SetCurrentShaderProgram(POSTPROCESS_SHADER);
    MikuPan_SetPostprocessPassthroughUniforms(
        render_back_msaa.texture.width,
        render_back_msaa.texture.height,
        g_photo_framebuffer_width,
        g_photo_framebuffer_height);

    MikuPan_PipelineInfo *pipeline = MikuPan_GetPipelineInfo(UV4_COLOUR4_POSITION4);
    if (pipeline == NULL)
    {
        MikuPan_GPUSetTarget(MIKUPAN_GPU_TARGET_SCENE, 0);
        MikuPan_SetViewportCached(0, 0,
                                  render_back_msaa.texture.width,
                                  render_back_msaa.texture.height);
        return 0;
    }

    MikuPan_BindVAO(pipeline->vao);
    MikuPan_StreamUploadFull(GL_ARRAY_BUFFER, pipeline->buffers[0].id,
                             (GLsizeiptr)sizeof(quad), quad);
    MikuPan_TimedDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    MikuPan_GPUFlushRenderPass();

    MikuPan_GPUSetTarget(MIKUPAN_GPU_TARGET_SCENE, 0);
    MikuPan_SetViewportCached(0, 0,
                              render_back_msaa.texture.width,
                              render_back_msaa.texture.height);
    return 1;
}

static void MikuPan_CopyCurrentSceneToTexture(unsigned int dst_texture_id,
                                                int *valid_flag)
{
    if (valid_flag != NULL)
    {
        *valid_flag = 0;
    }

    if (render_back_msaa.texture.id == 0 || dst_texture_id == 0 ||
        render_back_msaa.texture.width <= 0 || render_back_msaa.texture.height <= 0)
    {
        return;
    }

    MikuPan_GPUCopyTexture(render_back_msaa.texture.id,
                           dst_texture_id,
                           render_back_msaa.texture.width,
                           render_back_msaa.texture.height);

    if (valid_flag != NULL)
    {
        *valid_flag = 1;
    }
}

static int MikuPan_EnsurePauseCaptureTexture(void)
{
    const int width = render_back_msaa.texture.width;
    const int height = render_back_msaa.texture.height;

    if (width <= 0 || height <= 0)
    {
        return 0;
    }

    if (g_pause_capture_texture_id != 0 &&
        g_pause_capture_width == width &&
        g_pause_capture_height == height)
    {
        return 1;
    }

    if (g_pause_capture_texture_id != 0)
    {
        MikuPan_GPUReleaseTexture(g_pause_capture_texture_id);
        g_pause_capture_texture_id = 0;
    }

    g_pause_capture_texture_id = MikuPan_GPUCreateRenderTextureRGBA8(width, height);
    g_pause_capture_width = g_pause_capture_texture_id != 0 ? width : 0;
    g_pause_capture_height = g_pause_capture_texture_id != 0 ? height : 0;
    g_pause_capture_valid = 0;

    return g_pause_capture_texture_id != 0;
}

int MikuPan_CapturePauseScreen(u_int addr)
{
    g_pause_capture_valid = 0;
    g_pause_capture_addr = (int)addr;

    if (render_back_msaa.texture.id == 0 || !MikuPan_EnsurePauseCaptureTexture())
    {
        return 0;
    }

    MikuPan_GPUCopyTexture(render_back_msaa.texture.id,
                           g_pause_capture_texture_id,
                           render_back_msaa.texture.width,
                           render_back_msaa.texture.height);

    g_pause_capture_valid = 1;
    return 1;
}

void MikuPan_ClearPauseScreenCapture(u_int addr)
{
    if ((int)addr == g_pause_capture_addr)
    {
        g_pause_capture_valid = 0;
    }
}

int MikuPan_GetPauseScreenCaptureTexture(u_int addr, unsigned int *texture_id, int *width, int *height)
{
    if (!g_pause_capture_valid ||
        (int)addr != g_pause_capture_addr ||
        g_pause_capture_texture_id == 0)
    {
        return 0;
    }

    if (texture_id != NULL)
    {
        *texture_id = g_pause_capture_texture_id;
    }

    if (width != NULL)
    {
        *width = g_pause_capture_width;
    }

    if (height != NULL)
    {
        *height = g_pause_capture_height;
    }

    return 1;
}

static int MikuPan_EnsureEnemyOutCaptureTexture(int slot)
{
    const int width = render_back_msaa.texture.width;
    const int height = render_back_msaa.texture.height;

    if (!MikuPan_IsValidEnemyOutCaptureSlot(slot) || width <= 0 || height <= 0)
    {
        return 0;
    }

    if (g_enemy_out_capture_texture_id[slot] != 0 &&
        g_enemy_out_capture_width[slot] == width &&
        g_enemy_out_capture_height[slot] == height)
    {
        return 1;
    }

    MikuPan_ReleaseEnemyOutCaptureSlot(slot);

    g_enemy_out_capture_texture_id[slot] = MikuPan_GPUCreateRenderTextureRGBA8(width, height);
    g_enemy_out_capture_width[slot] = g_enemy_out_capture_texture_id[slot] != 0 ? width : 0;
    g_enemy_out_capture_height[slot] = g_enemy_out_capture_texture_id[slot] != 0 ? height : 0;
    g_enemy_out_capture_valid[slot] = 0;

    return g_enemy_out_capture_texture_id[slot] != 0;
}

int MikuPan_CaptureEnemyOutScreen(int slot)
{
    if (!MikuPan_IsValidEnemyOutCaptureSlot(slot))
    {
        return 0;
    }

    g_enemy_out_capture_valid[slot] = 0;

    if (render_back_msaa.texture.id == 0 ||
        !MikuPan_EnsureEnemyOutCaptureTexture(slot))
    {
        return 0;
    }

    MikuPan_GPUCopyTexture(render_back_msaa.texture.id,
                           g_enemy_out_capture_texture_id[slot],
                           render_back_msaa.texture.width,
                           render_back_msaa.texture.height);

    g_enemy_out_capture_valid[slot] = 1;
    return 1;
}

void MikuPan_ClearEnemyOutScreenCapture(int slot)
{
    if (MikuPan_IsValidEnemyOutCaptureSlot(slot))
    {
        g_enemy_out_capture_valid[slot] = 0;
    }
}

int MikuPan_GetEnemyOutScreenCaptureTexture(int slot, unsigned int *texture_id,
                                            int *width, int *height)
{
    if (!MikuPan_IsValidEnemyOutCaptureSlot(slot) ||
        !g_enemy_out_capture_valid[slot] ||
        g_enemy_out_capture_texture_id[slot] == 0)
    {
        return 0;
    }

    if (texture_id != NULL)
    {
        *texture_id = g_enemy_out_capture_texture_id[slot];
    }

    if (width != NULL)
    {
        *width = g_enemy_out_capture_width[slot];
    }

    if (height != NULL)
    {
        *height = g_enemy_out_capture_height[slot];
    }

    return 1;
}

int MikuPan_RenderPauseScreen(u_int addr, u_char r, u_char g, u_char b, u_char a)
{
    if (!g_pause_capture_valid ||
        (int)addr != g_pause_capture_addr ||
        g_pause_capture_texture_id == 0)
    {
        return 0;
    }

    MikuPan_FlushTexturedSpriteBatch();

    const float cr = MikuPan_ConvertScaleColor(r);
    const float cg = MikuPan_ConvertScaleColor(g);
    const float cb = MikuPan_ConvertScaleColor(b);
    const float ca = MikuPan_ConvertScaleColor(a);

    const float verts[6][12] = {
        {0.0f, 1.0f, 0.0f, 0.0f, cr, cg, cb, ca, -1.0f, -1.0f, 0.0f, 1.0f},
        {1.0f, 1.0f, 0.0f, 0.0f, cr, cg, cb, ca,  1.0f, -1.0f, 0.0f, 1.0f},
        {0.0f, 0.0f, 0.0f, 0.0f, cr, cg, cb, ca, -1.0f,  1.0f, 0.0f, 1.0f},
        {1.0f, 1.0f, 0.0f, 0.0f, cr, cg, cb, ca,  1.0f, -1.0f, 0.0f, 1.0f},
        {1.0f, 0.0f, 0.0f, 0.0f, cr, cg, cb, ca,  1.0f,  1.0f, 0.0f, 1.0f},
        {0.0f, 0.0f, 0.0f, 0.0f, cr, cg, cb, ca, -1.0f,  1.0f, 0.0f, 1.0f},
    };

    MikuPan_SetCurrentShaderProgram(SPRITE_SHADER);
    MikuPan_SetUniform1iToCurrentShader(0, "uTexture");
    MikuPan_SetUniform1fToCurrentShader(0.0f, "uPhotoNegativeStrength");
    MikuPan_SetRenderState2D();

    MikuPan_PipelineInfo *pipeline = MikuPan_GetPipelineInfo(UV4_COLOUR4_POSITION4);
    if (pipeline == NULL)
    {
        return 0;
    }

    MikuPan_BindVAO(pipeline->vao);
    MikuPan_BindTexture2DCached(g_pause_capture_texture_id);
    MikuPan_StreamUploadFull(GL_ARRAY_BUFFER, pipeline->buffers[0].id,
                             (GLsizeiptr)sizeof(verts), verts);
    MikuPan_TimedDrawArrays(GL_TRIANGLES, 0, 6);
    MikuPan_PerfDrawCall();
    return 1;
}

void MikuPan_SetPhotoCaptureScreenCopyEffectsSuppressed(int suppressed)
{
    g_photo_capture_suppress_screen_copy_effects = suppressed ? 1 : 0;
}

int MikuPan_ArePhotoCaptureScreenCopyEffectsSuppressed(void)
{
    return g_photo_capture_suppress_screen_copy_effects;
}

int MikuPan_ScreenCopyConsoleLogEnabled(void)
{
    return 0;
}

int MikuPan_DisableScreenDeformEnabled(void)
{
    return 0;
}

int MikuPan_DisablePartsDeformEnabled(void)
{
    return 0;
}

void MikuPan_ClearSceneFeedbackFrameHistory(void)
{
    g_scene_history_valid[0] = 0;
    g_scene_history_valid[1] = 0;
}

void MikuPan_SuppressNextSceneFeedbackFrameCapture(void)
{
    g_scene_history_suppress_next_capture = 1;
}


void MikuPan_ApplyScreenNegative(void)
{
    const MikuPan_PhotoDebugInfo *photo_debug = MikuPan_GetPhotoDebugInfo();

    if (photo_debug == NULL || !photo_debug->screen_negative_overlay_active ||
        photo_debug->screen_negative_strength <= 0.0f)
    {
        return;
    }

    if (render_back_msaa.texture.id == 0 ||
        render_back_msaa.texture.width <= 0 ||
        render_back_msaa.texture.height <= 0 ||
        g_screen_negative_scene_copy_id == 0)
    {
        MikuPan_ClearScreenNegativeOverlay();
        return;
    }

    MikuPan_GPUCopyTexture(render_back_msaa.texture.id,
                           g_screen_negative_scene_copy_id,
                           render_back_msaa.texture.width,
                           render_back_msaa.texture.height);

    float quad[] = {
        0,1,0,0,   1,1,1,1,   -1,-1,0,1,
        1,1,0,0,   1,1,1,1,    1,-1,0,1,
        0,0,0,0,   1,1,1,1,   -1, 1,0,1,
        1,0,0,0,   1,1,1,1,    1, 1,0,1
    };

    MikuPan_SetViewportCached(0, 0,
                              render_back_msaa.texture.width,
                              render_back_msaa.texture.height);
    MikuPan_GPUSetTarget(MIKUPAN_GPU_TARGET_SCENE, 0);

    MikuPan_BindTexture2DCached(g_screen_negative_scene_copy_id);
    MikuPan_SetRenderState2D();
    MikuPan_SetCurrentShaderProgram(POSTPROCESS_SHADER);
    MikuPan_SetUniform1iToCurrentShader(0, "uTexture");
    MikuPan_SetUniform1iToCurrentShader(1, "uPhotoNegativeSourceTexture");
    MikuPan_SetUniform1iToCurrentShader(0, "uBlackWhiteMode");
    MikuPan_SetUniform1fToCurrentShader(1.0f, "uBrightness");
    MikuPan_SetUniform1fToCurrentShader(1.0f, "uGamma");
    MikuPan_SetUniform1fToCurrentShader(1.0f, "uContrast");

    MikuPan_SetUniform1iToCurrentShader(0, "uCrtEnabled");
    MikuPan_SetUniform1fToCurrentShader(0.0f, "uCrtStrength");
    MikuPan_SetUniform1fToCurrentShader(0.0f, "uCrtCurvature");
    MikuPan_SetUniform1fToCurrentShader(0.0f, "uCrtOverscan");
    MikuPan_SetUniform1fToCurrentShader(0.0f, "uCrtScanlineStrength");
    MikuPan_SetUniform1fToCurrentShader(1.0f, "uCrtScanlineScale");
    MikuPan_SetUniform1fToCurrentShader(1.0f, "uCrtScanlineThickness");
    MikuPan_SetUniform1fToCurrentShader(0.0f, "uCrtMaskStrength");
    MikuPan_SetUniform1fToCurrentShader(1.0f, "uCrtMaskScale");
    MikuPan_SetUniform1fToCurrentShader(0.0f, "uCrtVignetteStrength");
    MikuPan_SetUniform1fToCurrentShader(1.0f, "uCrtVignetteSize");
    MikuPan_SetUniform1fToCurrentShader(0.0f, "uCrtChromaOffset");
    MikuPan_SetUniform1fToCurrentShader(0.0f, "uCrtBlendStrength");
    MikuPan_SetUniform1fToCurrentShader(1.0f, "uCrtBlendRadius");
    MikuPan_SetUniform1fToCurrentShader(0.0f, "uCrtNoiseStrength");
    MikuPan_SetUniform1fToCurrentShader(0.0f, "uCrtFlickerStrength");
    MikuPan_SetUniform1fToCurrentShader(0.0f, "uCrtGlowStrength");
    MikuPan_SetUniform2fToCurrentShader((float) render_back_msaa.texture.width,
                                        (float) render_back_msaa.texture.height,
                                        "uTextureSize");
    MikuPan_SetUniform2fToCurrentShader((float) render_back_msaa.texture.width,
                                        (float) render_back_msaa.texture.height,
                                        "uOutputSize");
    MikuPan_SetUniform1fToCurrentShader((float) ((double) SDL_GetTicks() / 1000.0),
                                        "uTime");

    MikuPan_SetUniform1iToCurrentShader(0, "uPhotoNegativeEnabled");
    MikuPan_SetUniform1iToCurrentShader(0, "uPhotoNegativeSourceEnabled");
    {
        float full_rect[4] = {0.0f, 0.0f, 1.0f, 1.0f};
        MikuPan_SetUniform4fvToCurrentShader(full_rect,
                                             "uPhotoNegativeContentRect");
        MikuPan_SetUniform4fvToCurrentShader(full_rect,
                                             "uPhotoNegativeRect");
    }
    MikuPan_SetUniform1fToCurrentShader(0.0f, "uPhotoNegativeStrength");

    {
        float screen_negative[4] = {
            photo_debug->screen_negative_r,
            photo_debug->screen_negative_g,
            photo_debug->screen_negative_b,
            photo_debug->screen_negative_strength,
        };
        MikuPan_SetUniform4fvToCurrentShader(screen_negative,
                                             "uScreenNegative");
    }

    MikuPan_PipelineInfo *pipeline = MikuPan_GetPipelineInfo(UV4_COLOUR4_POSITION4);
    MikuPan_BindVAO(pipeline->vao);
    MikuPan_StreamUploadFull(GL_ARRAY_BUFFER, pipeline->buffers[0].id,
                             (GLsizeiptr)sizeof(quad), quad);
    MikuPan_TimedDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    MikuPan_ClearScreenNegativeOverlay();
}

void MikuPan_CaptureSceneFeedbackFrame(void)
{
    if (g_scene_history_suppress_next_capture)
    {
        g_scene_history_suppress_next_capture = 0;
        MikuPan_ClearSceneFeedbackFrameHistory();
        return;
    }

    if (g_scene_history_id[0] == 0 || g_scene_history_id[1] == 0)
    {
        return;
    }

    if (g_scene_history_valid[0])
    {
        MikuPan_GPUCopyTexture(g_scene_history_id[0],
                               g_scene_history_id[1],
                               render_back_msaa.texture.width,
                               render_back_msaa.texture.height);
        g_scene_history_valid[1] = 1;
    }
    else
    {
        g_scene_history_valid[1] = 0;
    }

    MikuPan_CopyCurrentSceneToTexture(g_scene_history_id[0],
                                      &g_scene_history_valid[0]);
}

unsigned int MikuPan_GetSceneFeedbackTextureId(int age)
{
    if (age < 0)
    {
        age = 0;
    }
    if (age > 1)
    {
        age = 1;
    }

    return g_scene_history_valid[age] ? g_scene_history_id[age] : 0;
}

int MikuPan_IsSceneFeedbackTextureValid(int age)
{
    if (age < 0 || age > 1)
    {
        return 0;
    }

    return g_scene_history_valid[age] && g_scene_history_id[age] != 0;
}

void MikuPan_CapturePhotoFramebuffer(void)
{
    g_photo_framebuffer_valid = MikuPan_RenderSceneToPhotoFramebuffer();
}

unsigned int MikuPan_GetPhotoFramebufferTextureId(void)
{
    return g_photo_framebuffer_valid ? g_photo_framebuffer_id : 0;
}

int MikuPan_GetPhotoFramebufferWidth(void)
{
    return g_photo_framebuffer_valid ? g_photo_framebuffer_width : 0;
}

int MikuPan_GetPhotoFramebufferHeight(void)
{
    return g_photo_framebuffer_valid ? g_photo_framebuffer_height : 0;
}

static void MikuPan_UpdateLastResolvedFrameCache(void)
{
}

int MikuPan_ReadFramebufferRGBA8TopLeft(int width, int height, unsigned char *out_rgba)
{
    return MikuPan_ReadSceneTextureRGBA8TopLeft(width, height, out_rgba);
}

int MikuPan_ReadResolvedFramebufferRGBA8TopLeft(int width, int height,
                                               unsigned char *out_rgba)
{
    return MikuPan_ReadFramebufferRGBA8TopLeft(width, height, out_rgba);
}

int MikuPan_ReadSceneFeedbackFramebufferRGBA8TopLeft(int width, int height,
                                                     unsigned char *out_rgba)
{
    if (out_rgba != NULL && g_scene_history_valid[0] && g_scene_history_id[0] != 0)
    {
        return MikuPan_ReadTextureRGBA8TopLeft(g_scene_history_id[0],
                                              render_back_msaa.texture.width,
                                              render_back_msaa.texture.height,
                                              width, height, out_rgba);
    }

    return 0;
}

int MikuPan_ReadPhotoFramebufferRGBA8TopLeft(int width, int height,
                                             unsigned char *out_rgba)
{
    if (out_rgba != NULL && g_photo_framebuffer_valid && g_photo_framebuffer_id != 0)
    {
        return MikuPan_ReadTextureRGBA8TopLeft(g_photo_framebuffer_id,
                                              g_photo_framebuffer_width,
                                              g_photo_framebuffer_height,
                                              width, height, out_rgba);
    }

    return MikuPan_ReadFramebufferRGBA8TopLeft(width, height, out_rgba);
}

static int MikuPan_ReadCaptureRectFromTextureRGBA8TopLeft(unsigned int source_texture_id,
                                                         int source_w,
                                                         int source_h,
                                                         int ps2_x,
                                                         int ps2_y,
                                                         int ps2_w,
                                                         int ps2_h,
                                                         int width,
                                                         int height,
                                                         int source_is_render_scene,
                                                         unsigned char *out_rgba)
{
    if (source_texture_id == 0 || source_w <= 0 || source_h <= 0 ||
        out_rgba == NULL || width <= 0 || height <= 0 ||
        ps2_w <= 0 || ps2_h <= 0)
    {
        return 0;
    }

    if (!MikuPan_EnsurePhotoCaptureReadbackTexture(width, height))
    {
        return 0;
    }

    float uv[4] = {0.0f, 0.0f, 1.0f, 1.0f};
    if (source_is_render_scene)
    {
        MikuPan_ConvertPs2RectToRenderTextureUv(uv,
                                                (float)ps2_x,
                                                (float)ps2_y,
                                                (float)ps2_w,
                                                (float)ps2_h);
    }
    else
    {
        MikuPan_ConvertPs2RectToTextureUv(uv,
                                          source_w,
                                          source_h,
                                          (float)ps2_x, (float)ps2_y,
                                          (float)ps2_w, (float)ps2_h);
    }

    const float verts[4][12] = {
        {uv[0], uv[3], 0.0f, 0.0f, 1.0f, 1.0f, 1.0f, 1.0f, -1.0f, -1.0f, 0.0f, 1.0f},
        {uv[2], uv[3], 0.0f, 0.0f, 1.0f, 1.0f, 1.0f, 1.0f,  1.0f, -1.0f, 0.0f, 1.0f},
        {uv[0], uv[1], 0.0f, 0.0f, 1.0f, 1.0f, 1.0f, 1.0f, -1.0f,  1.0f, 0.0f, 1.0f},
        {uv[2], uv[1], 0.0f, 0.0f, 1.0f, 1.0f, 1.0f, 1.0f,  1.0f,  1.0f, 0.0f, 1.0f},
    };

    MikuPan_SetViewportCached(0, 0, width, height);
    MikuPan_GPUSetScreenCopyTarget(g_photo_capture_readback_texture_id,
                                   width, height, 1);
    MikuPan_BindTexture2DCached(source_texture_id);
    MikuPan_SetRenderState2D();
    MikuPan_SetCurrentShaderProgram(SPRITE_SHADER);
    MikuPan_SetUniform1iToCurrentShader(0, "uTexture");
    MikuPan_SetUniform1fToCurrentShader(0.0f, "uPhotoNegativeStrength");

    MikuPan_PipelineInfo *pipeline = MikuPan_GetPipelineInfo(UV4_COLOUR4_POSITION4);
    if (pipeline == NULL)
    {
        MikuPan_GPUSetTarget(MIKUPAN_GPU_TARGET_SCENE, 0);
        MikuPan_SetViewportCached(0, 0,
                                  render_back_msaa.texture.width,
                                  render_back_msaa.texture.height);
        return 0;
    }

    MikuPan_BindVAO(pipeline->vao);
    MikuPan_StreamUploadFull(GL_ARRAY_BUFFER, pipeline->buffers[0].id,
                             (GLsizeiptr)sizeof(verts), verts);
    MikuPan_TimedDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    MikuPan_PerfDrawCall();
    MikuPan_GPUFlushRenderPass();

    MikuPan_GPUSetTarget(MIKUPAN_GPU_TARGET_SCENE, 0);
    MikuPan_SetViewportCached(0, 0,
                              render_back_msaa.texture.width,
                              render_back_msaa.texture.height);

    int ok = MikuPan_GPUReadTextureRGBA8(g_photo_capture_readback_texture_id,
                                         width, height, out_rgba);
    if (ok && MikuPan_IsBlackWhiteModeActive())
    {
        MikuPan_ConvertRGBA8ToBlackWhite(out_rgba, width, height);
    }

    return ok;
}

int MikuPan_ReadCurrentSceneCaptureRectRGBA8TopLeft(int ps2_x, int ps2_y,
                                                   int ps2_w, int ps2_h,
                                                   int width, int height,
                                                   unsigned char *out_rgba)
{
    if (render_back_msaa.texture.id == 0 ||
        render_back_msaa.texture.width <= 0 ||
        render_back_msaa.texture.height <= 0)
    {
        return 0;
    }

    MikuPan_GPUResolveSceneForSampling();
    return MikuPan_ReadCaptureRectFromTextureRGBA8TopLeft(
        render_back_msaa.texture.id,
        render_back_msaa.texture.width,
        render_back_msaa.texture.height,
        ps2_x, ps2_y, ps2_w, ps2_h,
        width, height, 1, out_rgba);
}

int MikuPan_ReadPhotoCaptureRectRGBA8TopLeft(int ps2_x, int ps2_y,
                                             int ps2_w, int ps2_h,
                                             int width, int height,
                                             unsigned char *out_rgba)
{
    if (!g_photo_framebuffer_valid || g_photo_framebuffer_id == 0 ||
        g_photo_framebuffer_width <= 0 || g_photo_framebuffer_height <= 0)
    {
        if (!MikuPan_RenderSceneToPhotoFramebuffer())
        {
            return 0;
        }
    }

    return MikuPan_ReadCaptureRectFromTextureRGBA8TopLeft(
        g_photo_framebuffer_id,
        g_photo_framebuffer_width,
        g_photo_framebuffer_height,
        ps2_x, ps2_y, ps2_w, ps2_h,
        width, height, 0, out_rgba);
}

void MikuPan_SetupOpenGLContext()
{
}

void MikuPan_CreateInternalBuffer(int w, int h, int msaa)
{
    MikuPan_GPUCreateInternalBuffer(w, h, msaa);
    render_back_msaa.texture.id = MikuPan_GPUGetSceneTextureId();
    render_back_msaa.texture.width = w;
    render_back_msaa.texture.height = h;
    render_back_msaa.msaa = msaa;

    for (int i = 0; i < 2; i++)
    {
        if (g_scene_history_id[i] != 0)
        {
            MikuPan_GPUReleaseTexture(g_scene_history_id[i]);
        }
        g_scene_history_id[i] = MikuPan_GPUCreateRenderTextureRGBA8(w, h);
        g_scene_history_valid[i] = 0;
    }

    if (g_photo_framebuffer_id != 0)
    {
        MikuPan_GPUReleaseTexture(g_photo_framebuffer_id);
        g_photo_framebuffer_id = 0;
    }
    g_photo_framebuffer_width = 0;
    g_photo_framebuffer_height = 0;
    g_photo_framebuffer_valid = 0;
    MikuPan_ClearPhotoNegativeSourceTextureReference();
    MikuPan_EnsurePhotoFramebuffer();

    if (g_screen_negative_scene_copy_id != 0)
    {
        MikuPan_GPUReleaseTexture(g_screen_negative_scene_copy_id);
    }
    g_screen_negative_scene_copy_id = MikuPan_GPUCreateRenderTextureRGBA8(w, h);

    MikuPan_ReleaseEnemyOutCaptures();

    MikuPan_SetViewportCached(0, 0, render_back_msaa.texture.width, render_back_msaa.texture.height);
}

static int g_applied_window_mode = MIKUPAN_WINDOW_WINDOWED;
static int g_saved_win_w = 0, g_saved_win_h = 0, g_saved_win_x = 0, g_saved_win_y = 0;

static int MikuPan_GetWindowDisplayBounds(SDL_Window* window, SDL_Rect* bounds)
{
    if (window == NULL || bounds == NULL)
    {
        return 0;
    }

    SDL_DisplayID display = SDL_GetDisplayForWindow(window);
    if (display == 0)
    {
        display = SDL_GetPrimaryDisplay();
    }

    if (display != 0 && SDL_GetDisplayUsableBounds(display, bounds))
    {
        return 1;
    }

    if (display != 0 && SDL_GetDisplayBounds(display, bounds))
    {
        return 1;
    }

    return 0;
}

static void MikuPan_CenterWindowOnDisplay(SDL_Window* window, int width, int height)
{
    if (window == NULL || width <= 0 || height <= 0)
    {
        return;
    }

    SDL_Rect bounds;
    if (!MikuPan_GetWindowDisplayBounds(window, &bounds))
    {
        return;
    }

    int x = bounds.x + (bounds.w - width) / 2;
    int y = bounds.y + (bounds.h - height) / 2;

    if (x < bounds.x)
    {
        x = bounds.x;
    }

    if (y < bounds.y)
    {
        y = bounds.y;
    }

    SDL_SetWindowPosition(window, x, y);
}

static void MikuPan_ApplyWindowMode(int mode)
{
#ifndef __ANDROID__
    if (mode == MIKUPAN_WINDOW_FULLSCREEN)
    {
        mode = MIKUPAN_WINDOW_BORDERLESS;
    }
#endif

    SDL_Window *win = mikupan_render.window;
    if (win == NULL)
    {
        return;
    }

    if (g_applied_window_mode == MIKUPAN_WINDOW_WINDOWED &&
        mode != MIKUPAN_WINDOW_WINDOWED)
    {
        SDL_GetWindowSize(win, &g_saved_win_w, &g_saved_win_h);
        SDL_GetWindowPosition(win, &g_saved_win_x, &g_saved_win_y);
    }

    switch (mode)
    {
        case MIKUPAN_WINDOW_FULLSCREEN:
        {
            SDL_DisplayID disp = SDL_GetDisplayForWindow(win);
            const SDL_DisplayMode *dm = SDL_GetDesktopDisplayMode(disp);
            SDL_SetWindowFullscreenMode(win, dm);
            SDL_SetWindowFullscreen(win, true);
            break;
        }
        case MIKUPAN_WINDOW_BORDERLESS:
        {
            SDL_SetWindowFullscreenMode(win, NULL);
            SDL_SetWindowFullscreen(win, true);
            break;
        }

        case MIKUPAN_WINDOW_WINDOWED:
        default:
        {
            SDL_SetWindowFullscreen(win, false);
            SDL_SetWindowBordered(win, true);

            int restore_w = mikupan_configuration.renderer.window.width;
            int restore_h = mikupan_configuration.renderer.window.height;
            if (restore_w <= 0 || restore_h <= 0)
            {
                restore_w = g_saved_win_w;
                restore_h = g_saved_win_h;
            }

            const int restore_size_changed =
                (restore_w != g_saved_win_w || restore_h != g_saved_win_h);

            if (restore_w > 0 && restore_h > 0)
            {
                SDL_SetWindowSize(win, restore_w, restore_h);
            }

            if (!restore_size_changed && (g_saved_win_x != 0 || g_saved_win_y != 0))
            {
                SDL_SetWindowPosition(win, g_saved_win_x, g_saved_win_y);
            }
            else
            {
                MikuPan_CenterWindowOnDisplay(win, restore_w, restore_h);
            }
            break;
        }
    }

    g_applied_window_mode = mode;
}

void MikuPan_Clear()
{
    MikuPan_GPUBeginFrame();
    MikuPan_PerfBeginFrame();
    MikuPan_ShadowDebugBeginFrame();

    MikuPan_GsResetFrameMetrics();
    MikuPan_PerfResetFrame();

    MikuPan_ResetShaderCache();
    MikuPan_ResetRenderStateCache();
    MikuPan_ResetGLBindCache();

    /* Reset the per-frame GPU-skinning palette dedupe so each model's first
     * skinned mesh re-reads its animated bone pose this frame. */
    extern void MikuPan_SkinFrameReset(void);
    MikuPan_SkinFrameReset();

    MikuPan_RenderSetDebugValues();

    int curr_render_width = MikuPan_GetRenderResolutionWidth();
    int curr_render_height = MikuPan_GetRenderResolutionHeight();
    int curr_msaa = MikuPan_GetMSAA();

    if (render_back_msaa.texture.width != curr_render_width || render_back_msaa.texture.height != curr_render_height || render_back_msaa.msaa != curr_msaa)
    {
        MikuPan_DestroyInternalBuffer();
        MikuPan_CreateInternalBuffer(curr_render_width, curr_render_height, curr_msaa);
    }

    MikuPan_SetViewportCached(0, 0, render_back_msaa.texture.width, render_back_msaa.texture.height);
    MikuPan_GPUSetTarget(MIKUPAN_GPU_TARGET_SCENE, 1);
    MikuPan_GPUBeginRenderPass();

    if (g_applied_window_mode != MikuPan_GetWindowMode())
    {
        MikuPan_ApplyWindowMode(MikuPan_GetWindowMode());
        mikupan_configuration.renderer.window_mode = MikuPan_GetWindowMode();
        mikupan_configuration.renderer.is_fullscreen =
            (MikuPan_GetWindowMode() != MIKUPAN_WINDOW_WINDOWED);
    }

    if (mikupan_configuration.renderer.vsync != MikuPan_IsVsync())
    {
        mikupan_configuration.renderer.vsync = MikuPan_IsVsync();
        MikuPan_GPUSetVsync(mikupan_configuration.renderer.vsync);
    }
}

static void MikuPan_ConvertPs2RectToRenderTextureUv(float *out,
                                                    float x, float y,
                                                    float w, float h)
{
    float tl[2] = {0};
    float br[2] = {0};

    MikuPan_ConvertPs2ScreenCoordToNDCMaintainAspectRatio(
        tl,
        (float)render_back_msaa.texture.width,
        (float)render_back_msaa.texture.height,
        x,
        y);
    MikuPan_ConvertPs2ScreenCoordToNDCMaintainAspectRatio(
        br,
        (float)render_back_msaa.texture.width,
        (float)render_back_msaa.texture.height,
        x + w,
        y + h);

    out[0] = (tl[0] + 1.0f) * 0.5f;
    out[1] = (1.0f - tl[1]) * 0.5f;
    out[2] = (br[0] + 1.0f) * 0.5f;
    out[3] = (1.0f - br[1]) * 0.5f;
}

void MikuPan_EndFrame()
{
    MikuPan_GPUFlushRenderPass();
    MikuPan_GPUResolveSceneForPresent();
    MikuPan_SetViewportCached(
        0,
        0,
        render_back_msaa.texture.width,
        render_back_msaa.texture.height);

    MikuPan_UpdateLastResolvedFrameCache();

    SDL_GetWindowSize(mikupan_render.window, &mikupan_render.width, &mikupan_render.height);
    if (mikupan_render.width <= 0 || mikupan_render.height <= 0)
    {
        MikuPan_GPUEndFrame();
        MikuPan_PerfEndFrame();
        return;
    }

    float quad[] = {
        0,1,0,0,   1,1,1,1,   -1,-1,0,1,
        1,1,0,0,   1,1,1,1,    1,-1,0,1,
        0,0,0,0,   1,1,1,1,   -1, 1,0,1,
        1,0,0,0,   1,1,1,1,    1, 1,0,1
    };

    int offset_output[4];
    MikuPan_ConvertScreenToNDCCoord(
        offset_output,
        (float)mikupan_render.width, (float)mikupan_render.height,
        (float)render_back_msaa.texture.width, (float)render_back_msaa.texture.height
        );

    int screenshot_width = 0, screenshot_height = 0;
    SDL_GetWindowSizeInPixels(mikupan_render.window, &screenshot_width, &screenshot_height);
    MikuPan_ScreenshotCaptureIfRequested(screenshot_width, screenshot_height);

    MikuPan_SetViewportCached(
        offset_output[0], offset_output[1],
        offset_output[2], offset_output[3]
        );
    MikuPan_GPUSetTarget(MIKUPAN_GPU_TARGET_WINDOW, 1);

    MikuPan_BindTexture2DCached(render_back_msaa.texture.id);

    MikuPan_SetRenderState2D();

    MikuPan_SetCurrentShaderProgram(POSTPROCESS_SHADER);
    MikuPan_SetUniform1iToCurrentShader(0, "uTexture");
    MikuPan_SetUniform1iToCurrentShader(1, "uPhotoNegativeSourceTexture");
    /*
     * Black/white mode is applied through the original model CLUT/light paths
     * and to explicit framebuffer copies. Do not apply it here: the final
     * postprocess pass contains already-composited 2D HUD/menu sprites too.
     */
    MikuPan_SetUniform1iToCurrentShader(0, "uBlackWhiteMode");
    MikuPan_SetUniform1fToCurrentShader(MikuPan_GetBrightness(), "uBrightness");
    MikuPan_SetUniform1fToCurrentShader(MikuPan_GetGamma(),      "uGamma");
    MikuPan_SetUniform1fToCurrentShader(MikuPan_GetContrast(),   "uContrast");
    {
        const MikuPan_ConfigCrt *crt = MikuPan_GetCrtSettings();
        MikuPan_SetUniform1iToCurrentShader(crt->enabled, "uCrtEnabled");
        MikuPan_SetUniform1fToCurrentShader(crt->strength, "uCrtStrength");
        MikuPan_SetUniform1fToCurrentShader(crt->curvature, "uCrtCurvature");
        MikuPan_SetUniform1fToCurrentShader(crt->overscan, "uCrtOverscan");
        MikuPan_SetUniform1fToCurrentShader(crt->scanline_strength, "uCrtScanlineStrength");
        MikuPan_SetUniform1fToCurrentShader(crt->scanline_scale, "uCrtScanlineScale");
        MikuPan_SetUniform1fToCurrentShader(crt->scanline_thickness, "uCrtScanlineThickness");
        MikuPan_SetUniform1fToCurrentShader(crt->mask_strength, "uCrtMaskStrength");
        MikuPan_SetUniform1fToCurrentShader(crt->mask_scale, "uCrtMaskScale");
        MikuPan_SetUniform1fToCurrentShader(crt->vignette_strength, "uCrtVignetteStrength");
        MikuPan_SetUniform1fToCurrentShader(crt->vignette_size, "uCrtVignetteSize");
        MikuPan_SetUniform1fToCurrentShader(crt->chroma_offset, "uCrtChromaOffset");
        MikuPan_SetUniform1fToCurrentShader(crt->blend_strength, "uCrtBlendStrength");
        MikuPan_SetUniform1fToCurrentShader(crt->blend_radius, "uCrtBlendRadius");
        MikuPan_SetUniform1fToCurrentShader(crt->noise_strength, "uCrtNoiseStrength");
        MikuPan_SetUniform1fToCurrentShader(crt->flicker_strength, "uCrtFlickerStrength");
        MikuPan_SetUniform1fToCurrentShader(crt->glow_strength, "uCrtGlowStrength");
        MikuPan_SetUniform2fToCurrentShader((float) render_back_msaa.texture.width,
                                            (float) render_back_msaa.texture.height,
                                            "uTextureSize");
        MikuPan_SetUniform2fToCurrentShader((float) offset_output[2],
                                            (float) offset_output[3],
                                            "uOutputSize");
        MikuPan_SetUniform1fToCurrentShader((float) ((double) SDL_GetTicks() / 1000.0),
                                            "uTime");
    }
    {
        const MikuPan_PhotoDebugInfo *photo_debug = MikuPan_GetPhotoDebugInfo();
        float photo_negative_content_rect[4] = {0.0f, 0.0f, 1.0f, 1.0f};
        float photo_negative_rect[4] = {0.0f, 0.0f, 1.0f, 1.0f};
        const int negative_w =
            photo_debug->negative_w > 0 ? photo_debug->negative_w : 384;
        const int negative_h =
            photo_debug->negative_h > 0 ? photo_debug->negative_h : 256;
        const int negative_x =
            photo_debug->negative_w > 0 ? photo_debug->negative_x : 128;
        const int negative_y =
            photo_debug->negative_h > 0 ? photo_debug->negative_y : 80;
        const int photo_negative_enabled =
            photo_debug->negative_overlay_active ||
            photo_debug->force_negative_preview_enabled;
        const float photo_negative_strength =
            photo_debug->force_negative_preview_enabled ?
                photo_debug->force_negative_preview_strength :
                photo_debug->negative_strength;
        const int photo_negative_source_enabled =
            photo_debug->negative_source_texture_id != 0;

        MikuPan_ConvertPs2RectToRenderTextureUv(photo_negative_content_rect,
                                                0.0f, 0.0f,
                                                640.0f, 448.0f);
        MikuPan_ConvertPs2RectToRenderTextureUv(
            photo_negative_rect,
            (float)negative_x,
            (float)negative_y,
            (float)negative_w,
            (float)negative_h);

        MikuPan_SetUniform1iToCurrentShader(
            photo_negative_enabled && photo_negative_strength > 0.0f,
            "uPhotoNegativeEnabled");
        MikuPan_SetUniform1iToCurrentShader(
            photo_negative_source_enabled,
            "uPhotoNegativeSourceEnabled");
        MikuPan_SetUniform4fvToCurrentShader(photo_negative_content_rect,
                                             "uPhotoNegativeContentRect");
        MikuPan_SetUniform4fvToCurrentShader(photo_negative_rect,
                                             "uPhotoNegativeRect");
        MikuPan_SetUniform1fToCurrentShader(photo_negative_strength,
                                            "uPhotoNegativeStrength");

        if (photo_negative_source_enabled)
        {
            MikuPan_ActiveTextureCached(GL_TEXTURE1);
            MikuPan_BindTexture2DCached(photo_debug->negative_source_texture_id);
            MikuPan_ActiveTextureCached(GL_TEXTURE0);
        }

        {
            float screen_negative[4] = {
                photo_debug->screen_negative_r,
                photo_debug->screen_negative_g,
                photo_debug->screen_negative_b,
                photo_debug->screen_negative_overlay_active ?
                    photo_debug->screen_negative_strength : 0.0f,
            };

            MikuPan_SetUniform4fvToCurrentShader(screen_negative,
                                                 "uScreenNegative");
        }
    }

    MikuPan_PipelineInfo* pipeline = MikuPan_GetPipelineInfo(UV4_COLOUR4_POSITION4);
    MikuPan_BindVAO(pipeline->vao);

    MikuPan_StreamUploadFull(GL_ARRAY_BUFFER, pipeline->buffers[0].id,
                             (GLsizeiptr)sizeof(quad), quad);

    MikuPan_TimedDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    MikuPan_SetViewportCached(offset_output[0], offset_output[1],
                              offset_output[2], offset_output[3]);
    MikuPan_RenderQueuedPhotoPreviewTexture();
    MikuPan_FlushLate2DOverlayQueue();
    MikuPan_Flush2DMessageQueue();
    MikuPan_SetViewportCached(0, 0, mikupan_render.width, mikupan_render.height);

    //info_log("Frame: state_changes=%d, draw_calls=%d, mesh_cache hits=%d misses_new=%d misses_full=%d",
    //    MikuPan_PerfGetStateChangesCurrent(),
    //    MikuPan_PerfGetDrawCallsCurrent(),
    //    MikuPan_PerfGetMeshCacheHitsCurrent(),
    //    MikuPan_PerfGetMeshCacheMissesNewCurrent(),
    //    MikuPan_PerfGetMeshCacheMissesFullCurrent());

    {
        Uint64 _t0 = MikuPan_PerfBegin();
        MikuPan_DrawUi();

        MikuPan_PerfEnd(PERF_SECT_DRAWUI, _t0);
    }

    MikuPan_FlushTexturedSpriteBatch();

    {
        Uint64 _t0 = MikuPan_PerfBegin();
        MikuPan_RenderUi();
        MikuPan_PerfEnd(PERF_SECT_RENDERUI, _t0);
    }

    MikuPan_GPUEndFrame();

    MikuPan_PerfEndFrame();
}

void MikuPan_UpdateWindowSize(int width, int height)
{
    mikupan_render.width = width;
    mikupan_render.height = height;

    /* Store only real windowed sizes. Fullscreen and borderless resize events
     * report desktop-sized viewports, and saving those here makes the Window
     * Size option drift into whatever mode was last used. */
    if (MikuPan_GetWindowMode() == MIKUPAN_WINDOW_WINDOWED)
    {
        if (width > 0)
        {
            mikupan_configuration.renderer.window.width = width;
        }

        if (height > 0)
        {
            mikupan_configuration.renderer.window.height = height;
        }
    }
}

int MikuPan_GetWindowWidth()
{
    return render_back_msaa.texture.width;
}

int MikuPan_GetWindowHeight()
{
    return render_back_msaa.texture.height;
}


static void MikuPan_GetMirrorTextureSize(int *width, int *height)
{
    int window_w = mikupan_render.width;
    int window_h = mikupan_render.height;

    if (mikupan_render.window != NULL)
    {
        SDL_GetWindowSizeInPixels(mikupan_render.window, &window_w, &window_h);
    }

    if (window_w <= 0)
    {
        window_w = render_back_msaa.texture.width;
    }

    if (window_h <= 0)
    {
        window_h = render_back_msaa.texture.height;
    }

    *width = MikuPan_ClampInt(window_w, 1, render_back_msaa.texture.width);
    *height = MikuPan_ClampInt(window_h, 1, render_back_msaa.texture.height);
}

static int MikuPan_EnsureMirrorTexture(void)
{
    int width = 0;
    int height = 0;

    if (render_back_msaa.texture.width <= 0 || render_back_msaa.texture.height <= 0)
    {
        return 0;
    }

    MikuPan_GetMirrorTextureSize(&width, &height);

    if (g_mirror_texture_id != 0 && g_mirror_depth_id != 0 &&
        g_mirror_texture_width == width && g_mirror_texture_height == height)
    {
        return 1;
    }

    if (g_mirror_texture_id != 0)
    {
        MikuPan_GPUReleaseTexture(g_mirror_texture_id);
        g_mirror_texture_id = 0;
    }

    if (g_mirror_depth_id != 0)
    {
        MikuPan_GPUReleaseTexture(g_mirror_depth_id);
        g_mirror_depth_id = 0;
    }

    g_mirror_texture_id = MikuPan_GPUCreateRenderTextureRGBA8(width, height);
    g_mirror_depth_id = MikuPan_GPUCreateDepthTexture(width, height);
    g_mirror_texture_width = width;
    g_mirror_texture_height = height;
    g_mirror_texture_valid = 0;

    if (g_mirror_texture_id == 0 || g_mirror_depth_id == 0)
    {
        if (g_mirror_texture_id != 0)
        {
            MikuPan_GPUReleaseTexture(g_mirror_texture_id);
        }

        if (g_mirror_depth_id != 0)
        {
            MikuPan_GPUReleaseTexture(g_mirror_depth_id);
        }

        g_mirror_texture_id = 0;
        g_mirror_depth_id = 0;
        g_mirror_texture_width = 0;
        g_mirror_texture_height = 0;
        g_mirror_texture_valid = 0;
        return 0;
    }

    return 1;
}

static int MikuPan_EnableMirrorScissorFromGsBoundsForSize(int xmin, int ymin,
                                                           int xmax, int ymax,
                                                           int render_w,
                                                           int render_h)
{
    if (render_w <= 0 || render_h <= 0)
    {
        MikuPan_GPUDisableScissor();
        g_mirror_scissor_enabled = 0;
        return 0;
    }

    if (xmax < xmin)
    {
        int tmp = xmin;
        xmin = xmax;
        xmax = tmp;
    }

    if (ymax < ymin)
    {
        int tmp = ymin;
        ymin = ymax;
        ymax = tmp;
    }

    const float scale_x = (float)render_w / PS2_RESOLUTION_X_FLOAT;
    const float scale_y = (float)render_h / PS2_RESOLUTION_Y_FLOAT;

    const float x0 = ((float)xmin / 16.0f) - 1728.0f;
    const float x1 = ((float)xmax / 16.0f) - 1728.0f;
    const float y0 = (((float)ymin / 16.0f) - 1936.0f) * 2.0f;
    const float y1 = (((float)ymax / 16.0f) - 1936.0f) * 2.0f;

    int sx0 = (int)(x0 * scale_x);
    int sx1 = (int)(x1 * scale_x + 0.999f);
    int sy0 = (int)(y0 * scale_y);
    int sy1 = (int)(y1 * scale_y + 0.999f);

    sx0 = MikuPan_ClampInt(sx0, 0, render_w);
    sx1 = MikuPan_ClampInt(sx1, 0, render_w);
    sy0 = MikuPan_ClampInt(sy0, 0, render_h);
    sy1 = MikuPan_ClampInt(sy1, 0, render_h);

    const int width = sx1 - sx0;
    const int height = sy1 - sy0;

    if (width <= 0 || height <= 0)
    {
        MikuPan_GPUDisableScissor();
        g_mirror_scissor_enabled = 0;
        return 0;
    }

    MikuPan_GPUSetScissor(sx0, sy0, width, height);
    g_mirror_scissor_enabled = 1;
    return 1;
}

int MikuPan_ShouldUpdateMirrorTexture(int frame_counter)
{
    (void) frame_counter;
    return 1;
}

int MikuPan_BeginMirrorTextureUpdate(int xmin, int ymin, int xmax, int ymax)
{
    if (!MikuPan_EnsureMirrorTexture())
    {
        return 0;
    }

    MikuPan_GPUSetMirrorTarget(g_mirror_texture_id, g_mirror_depth_id, 1);
    MikuPan_SetViewportCached(0, 0, g_mirror_texture_width, g_mirror_texture_height);

    if (!MikuPan_EnableMirrorScissorFromGsBoundsForSize(xmin, ymin, xmax, ymax,
                                                        g_mirror_texture_width,
                                                        g_mirror_texture_height))
    {
        MikuPan_GPUSetTarget(MIKUPAN_GPU_TARGET_SCENE, 0);
        MikuPan_SetViewportCached(0, 0, render_back_msaa.texture.width,
                                  render_back_msaa.texture.height);
        return 0;
    }

    return 1;
}

void MikuPan_EndMirrorTextureUpdate(void)
{
    MikuPan_GPUFlushRenderPass();
    MikuPan_GPUDisableScissor();
    g_mirror_scissor_enabled = 0;
    g_mirror_texture_valid = 1;
    MikuPan_GPUSetTarget(MIKUPAN_GPU_TARGET_SCENE, 0);
    MikuPan_SetViewportCached(0, 0, render_back_msaa.texture.width,
                              render_back_msaa.texture.height);
    MikuPan_ResetRenderStateCache();
    MikuPan_ResetGLBindCache();
}

int MikuPan_HasMirrorTexture(void)
{
    return g_mirror_texture_valid && g_mirror_texture_id != 0;
}

void MikuPan_BindMirrorTextureToAuxSlot(void)
{
    MikuPan_ActiveTextureCached(GL_TEXTURE1);
    MikuPan_BindTexture2DCached(g_mirror_texture_id);
    MikuPan_ActiveTextureCached(GL_TEXTURE0);
    MikuPan_SetUniform2fToAllShaders((float) render_back_msaa.texture.width,
                                     (float) render_back_msaa.texture.height,
                                     "uRenderSize");
}

void MikuPan_UnbindMirrorTextureFromAuxSlot(void)
{
    MikuPan_ActiveTextureCached(GL_TEXTURE1);
    MikuPan_BindTexture2DCached(0);
    MikuPan_ActiveTextureCached(GL_TEXTURE0);
}

void MikuPan_EnableMirrorScissorFromGsBounds(int xmin, int ymin, int xmax, int ymax)
{
    MikuPan_EnableMirrorScissorFromGsBoundsForSize(xmin, ymin, xmax, ymax,
                                                   render_back_msaa.texture.width,
                                                   render_back_msaa.texture.height);
}

void MikuPan_EnableMirrorScissorFromNdcBounds(float minx, float miny, float maxx, float maxy)
{
    const int render_w = render_back_msaa.texture.width;
    const int render_h = render_back_msaa.texture.height;

    if (render_w <= 0 || render_h <= 0)
    {
        MikuPan_GPUDisableScissor();
        g_mirror_scissor_enabled = 0;
        return;
    }

    minx = MikuPan_ClampFloat(minx, -1.0f, 1.0f);
    maxx = MikuPan_ClampFloat(maxx, -1.0f, 1.0f);
    miny = MikuPan_ClampFloat(miny, -1.0f, 1.0f);
    maxy = MikuPan_ClampFloat(maxy, -1.0f, 1.0f);

    if (maxx < minx)
    {
        float tmp = minx;
        minx = maxx;
        maxx = tmp;
    }

    if (maxy < miny)
    {
        float tmp = miny;
        miny = maxy;
        maxy = tmp;
    }

    int sx0 = (int)(((minx * 0.5f) + 0.5f) * (float)render_w);
    int sx1 = (int)((((maxx * 0.5f) + 0.5f) * (float)render_w) + 0.999f);
    int sy0 = (int)(((1.0f - maxy) * 0.5f) * (float)render_h);
    int sy1 = (int)((((1.0f - miny) * 0.5f) * (float)render_h) + 0.999f);

    sx0 = MikuPan_ClampInt(sx0, 0, render_w);
    sx1 = MikuPan_ClampInt(sx1, 0, render_w);
    sy0 = MikuPan_ClampInt(sy0, 0, render_h);
    sy1 = MikuPan_ClampInt(sy1, 0, render_h);

    const int width = sx1 - sx0;
    const int height = sy1 - sy0;

    if (width <= 0 || height <= 0)
    {
        MikuPan_GPUDisableScissor();
        g_mirror_scissor_enabled = 0;
        return;
    }

    MikuPan_GPUSetScissor(sx0, sy0, width, height);
    g_mirror_scissor_enabled = 1;
}

void MikuPan_ClearMirrorScissorDepth(void)
{
    if (!g_mirror_scissor_enabled)
    {
        return;
    }

    MikuPan_GPUSetDepthWrite(1);
    MikuPan_GPUClearDepth();
    MikuPan_GPUBeginRenderPass();
    MikuPan_GPUFlushRenderPass();
    MikuPan_ResetRenderStateCache();
}

void MikuPan_DisableMirrorScissor(void)
{
    MikuPan_GPUDisableScissor();
    g_mirror_scissor_enabled = 0;
}

void MikuPan_SetMirrorReflectionPass(int active)
{
    g_mirror_reflection_pass = active ? 1 : 0;
}

int MikuPan_IsMirrorReflectionPass(void)
{
    return g_mirror_reflection_pass;
}

void MikuPan_GetFullScreenHalfExtent(float *half_w, float *half_h)
{
    float vx, vy, vw, vh, scale;
    MikuPan_GetPS2Viewport(MikuPan_GetWindowWidth(), MikuPan_GetWindowHeight(),
                           &vx, &vy, &vw, &vh, &scale);
    if (scale <= 0.0f)
    {
        *half_w = PS2_CENTER_X;
        *half_h = PS2_CENTER_Y;
        return;
    }
    *half_w = (float) MikuPan_GetWindowWidth() / (2.0f * scale);
    *half_h = (float) MikuPan_GetWindowHeight() / (2.0f * scale);
}

int MikuPan_GetRenderMode()
{
    return MikuPan_IsWireframeRendering() ? GL_LINE_STRIP : GL_TRIANGLE_STRIP;
}

void MikuPan_RenderSetDebugValues()
{
    static int   last_render_normals   = -1;
    static float last_normal_length    = -1.0f;
    static int   last_disable_lighting = -1;
    static int   last_static_lighting  = -1;
    static int   last_mesh_lighting_mode = -1;
    static int   last_black_white_mode = -1;
    static float last_shadow_depth = -1.0f;
    static unsigned int last_shader_generation = 0;

    int   render_normals   = MikuPan_IsNormalsRendering();
    float normal_length    = MikuPan_GetNormalLength();
    int   disable_lighting = MikuPan_IsLightingDisabled();
    int   static_lighting  = MikuPan_ShowStaticLighting();
    int   mesh_lighting_mode = MikuPan_GetMeshLightingMode();
    int   black_white_mode = MikuPan_IsBlackWhiteModeActive();
    float shadow_depth = MikuPan_GetShadowDepth();
    unsigned int shader_generation = MikuPan_GetShaderGeneration();

    if (shader_generation != last_shader_generation)
    {
        last_render_normals = -1;
        last_normal_length = -1.0f;
        last_disable_lighting = -1;
        last_static_lighting = -1;
        last_mesh_lighting_mode = -1;
        last_black_white_mode = -1;
        last_shadow_depth = -1.0f;
        last_shader_generation = shader_generation;
    }

    if (render_normals != last_render_normals)
    {
        MikuPan_SetUniform1iToAllShaders(render_normals, "renderNormals");
        last_render_normals = render_normals;
    }

    if (normal_length != last_normal_length)
    {
        MikuPan_SetUniform1fToAllShaders(normal_length, "uNormalLength");
        last_normal_length = normal_length;
    }

    if (disable_lighting != last_disable_lighting)
    {
        MikuPan_SetUniform1iToAllShaders(disable_lighting, "disableLighting");
        last_disable_lighting = disable_lighting;
    }

    if (static_lighting != last_static_lighting)
    {
        MikuPan_SetUniform1iToAllShaders(static_lighting, "staticLighting");
        last_static_lighting = static_lighting;
    }

    if (mesh_lighting_mode != last_mesh_lighting_mode)
    {
        MikuPan_SetUniform1iToAllShaders(mesh_lighting_mode, "uMeshLightingMode");
        last_mesh_lighting_mode = mesh_lighting_mode;
    }

    if (black_white_mode != last_black_white_mode)
    {
        MikuPan_SetUniform1iToAllShaders(black_white_mode, "uBlackWhiteMode");
        last_black_white_mode = black_white_mode;
    }

    if (shadow_depth != last_shadow_depth)
    {
        MikuPan_SetUniform1fToAllShaders(shadow_depth, "uShadowDepth");
        last_shadow_depth = shadow_depth;
    }
}

void MikuPan_Shutdown()
{
    MikuPan_TextureShutdown();
    MikuPan_GPUShutdown();
    SDL_DestroyWindow(mikupan_render.window);
    MikuPan_ShutdownLogging();
}
