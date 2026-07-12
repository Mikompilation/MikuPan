#include "mikupan_ui_settings.h"

#include "typedefs.h"

#define CIMGUI_DEFINE_ENUMS_AND_STRUCTS
#include "cimgui.h"
#include "SDL3/SDL_dialog.h"
#include "SDL3/SDL_hints.h"
#include "SDL3/SDL_platform.h"
#include "SDL3/SDL_stdinc.h"
#include "SDL3/SDL_timer.h"
#include "SDL3/SDL_video.h"
#include "ingame/camera/camera.h"
#include "mikupan/mikupan_config.h"
#include "mikupan/mikupan_audio_bus.h"
#include "mikupan/io/mikupan_controller.h"
#include "mikupan/gameplay/mikupan_first_person.h"
#include "mikupan/mikupan_utils.h"
#include "mikupan/rendering/mikupan_gpu.h"
#include "mikupan/rendering/mikupan_renderer.h"
#include "mikupan/ui/mikupan_ui.h"
#include "mikupan/ui/mikupan_rml_options.h"
#include "mikupan_ui_debug.h"
#include "mikupan_ui_theme.h"
#include "mikupan/gameplay/mikupan_title_scene.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define MIKUPAN_MAX_RESOLUTIONS 64
#define MIKUPAN_MAX_WINDOW_SIZES 128
#define MIKUPAN_MAX_PS2_RESOLUTION_SCALE 6
#define MIKUPAN_MAX_GPU_DRIVERS 8

static const int msaa_list[] = {0, 2, 4, 8};

static int show_controller_remap = 0;
static int msaa_samples = 0;
static int render_resolution_width = 1920;
static int render_resolution_height = 1080;

static MikuPan_Resolution resolution_list[MIKUPAN_MAX_RESOLUTIONS];
static int resolution_mode_list[MIKUPAN_MAX_RESOLUTIONS];
static int resolution_scale_percent_list[MIKUPAN_MAX_RESOLUTIONS];
static char resolution_labels[MIKUPAN_MAX_RESOLUTIONS][64];
static const char* resolution_label_ptrs[MIKUPAN_MAX_RESOLUTIONS];
static int resolution_count = 0;
static int resolution_selected = 0;

static MikuPan_Resolution window_size_list[MIKUPAN_MAX_WINDOW_SIZES];
static char window_size_labels[MIKUPAN_MAX_WINDOW_SIZES][48];
static const char* window_size_label_ptrs[MIKUPAN_MAX_WINDOW_SIZES];
static int window_size_count = 0;
static int window_size_selected = 0;

static char gpu_driver_names[MIKUPAN_MAX_GPU_DRIVERS + 1][32];
static char gpu_driver_labels[MIKUPAN_MAX_GPU_DRIVERS + 1][64];
static int gpu_driver_supported[MIKUPAN_MAX_GPU_DRIVERS + 1];
static int gpu_driver_count = 0;
static int gpu_driver_selected = 0;

static char config_save_status[128] = {0};
static int is_fullscreen = 0;
static int window_mode = 0; /* MikuPan_WindowMode: 0=windowed, 2=borderless fullscreen */
static float brightness = 1.0f;
static float gamma_value = 1.0f;
static float contrast = 1.0f;
static float shadow_depth = 1.0f;
static int hdr_enabled = 0;
static float hdr_paper_white = 203.0f;
static float hdr_peak_luminance = 1000.0f;

static void MikuPan_RefreshWindowBackedRenderResolution(void);
static int MikuPan_IsSuperSamplingEnabledInternal(void);


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

static int MikuPan_WaitForWindowPixelSize(SDL_Window* window,
                                          int desired_width,
                                          int desired_height)
{
    if (window == NULL || desired_width <= 0 || desired_height <= 0)
    {
        return 0;
    }

    for (int i = 0; i < 8; i++)
    {
        int width = 0;
        int height = 0;
        SDL_GetWindowSize(window, &width, &height);
        if (width == desired_width && height == desired_height)
        {
            return 1;
        }

        SDL_Delay(1);
    }

    return 0;
}

static void MikuPan_ApplyWindowedSizeNow(int width, int height)
{
#ifndef __ANDROID__
    SDL_Window* window = MikuPan_GetUiWindow();
    if (window == NULL || width <= 0 || height <= 0)
    {
        return;
    }

    SDL_SetWindowFullscreen(window, false);
    SDL_SetWindowBordered(window, true);
    SDL_RestoreWindow(window);
    SDL_SetWindowSize(window, width, height);
    MikuPan_WaitForWindowPixelSize(window, width, height);
    MikuPan_CenterWindowOnDisplay(window, width, height);
#else
    (void)width;
    (void)height;
#endif
}

#define MIKUPAN_CRT_DEFAULTS                                                   \
    {0,     1.0f,  0.08f, 0.02f, 0.18f, 1.0f,  1.6f,  0.22f, 1.0f,             \
     0.25f, 0.78f, 0.75f, 0.15f, 0.75f, 0.02f, 0.02f, 0.08f}

static const MikuPan_ConfigCrt crt_defaults = MIKUPAN_CRT_DEFAULTS;
static MikuPan_ConfigCrt crt_settings = MIKUPAN_CRT_DEFAULTS;

static void MikuPan_UiSyncSettingsFromConfiguration(void)
{
    MikuPan_ConfigurationValidate();
    render_resolution_width = mikupan_configuration.renderer.render.width;
    render_resolution_height = mikupan_configuration.renderer.render.height;
    msaa_samples = mikupan_configuration.renderer.msaa_index;
    brightness = mikupan_configuration.renderer.brightness;
    gamma_value = mikupan_configuration.renderer.gamma;
    contrast = mikupan_configuration.renderer.contrast;
    shadow_depth = mikupan_configuration.renderer.shadow_depth;
    hdr_enabled = mikupan_configuration.renderer.hdr_enabled;
    hdr_paper_white = mikupan_configuration.renderer.hdr_paper_white;
    hdr_peak_luminance = mikupan_configuration.renderer.hdr_peak_luminance;
    window_mode = mikupan_configuration.renderer.window_mode;
    is_fullscreen = mikupan_configuration.renderer.is_fullscreen;
    crt_settings = mikupan_configuration.crt;
}

static void MikuPan_ApplyThirdPersonCameraConfiguration(void)
{
    MikuPan_ConfigThirdPersonCamera* tps =
        &mikupan_configuration.third_person_camera;

    MikuPan_ConfigurationValidate();
    camera_third_person_enabled = tps->enabled;
    camera_third_person_distance = tps->distance;
    camera_third_person_height = tps->height;
    camera_third_person_side = tps->side;
    camera_third_person_look_ahead = tps->look_ahead;
    camera_third_person_interest_height = tps->interest_height;
    camera_third_person_fov_deg = tps->fov_deg;
}

static void MikuPan_ApplyFirstPersonCameraConfiguration(void)
{
    MikuPan_ConfigFirstPersonCamera* fps =
        &mikupan_configuration.first_person_camera;

    MikuPan_ConfigurationValidate();
    mikupan_first_person_r3_toggle_enabled = fps->r3_toggle_enabled;
    mikupan_first_person_auto_run_enabled = fps->auto_run_enabled;
    mikupan_first_person_eye_height = fps->eye_height;
    mikupan_first_person_eye_forward = fps->eye_forward;
    mikupan_first_person_look_distance = fps->look_distance;
    mikupan_first_person_fov_deg = fps->fov_deg;
    mikupan_first_person_stick_yaw_speed_deg = fps->stick_yaw_speed_deg;
    mikupan_first_person_stick_pitch_speed_deg = fps->stick_pitch_speed_deg;

    MikuPan_FirstPersonSetEnabled(fps->enabled);
}

static void MikuPan_UiStoreRuntimeConfiguration(void)
{
    mikupan_configuration.renderer.render.width = render_resolution_width;
    mikupan_configuration.renderer.render.height = render_resolution_height;
    mikupan_configuration.renderer.window_mode = window_mode;
    is_fullscreen = (window_mode != MIKUPAN_WINDOW_WINDOWED);
    mikupan_configuration.renderer.is_fullscreen = is_fullscreen;
    mikupan_configuration.renderer.lighting_mode = MikuPan_GetMeshLightingMode();
    if (MikuPan_IsSuperSamplingEnabledInternal())
    {
        msaa_samples = 0;
    }
    mikupan_configuration.renderer.msaa_index = msaa_samples;
    mikupan_configuration.renderer.shadow_resolution =
        MikuPan_GetShadowResolution();
    mikupan_configuration.renderer.brightness = brightness;
    mikupan_configuration.renderer.gamma = gamma_value;
    mikupan_configuration.renderer.contrast = contrast;
    mikupan_configuration.renderer.shadow_depth = shadow_depth;
    mikupan_configuration.renderer.hdr_enabled = hdr_enabled;
    mikupan_configuration.renderer.hdr_paper_white = hdr_paper_white;
    mikupan_configuration.renderer.hdr_peak_luminance = hdr_peak_luminance;
    mikupan_configuration.crt = crt_settings;
    mikupan_configuration.third_person_camera.enabled =
        camera_third_person_enabled ? 1 : 0;
    mikupan_configuration.third_person_camera.distance =
        camera_third_person_distance;
    mikupan_configuration.third_person_camera.height =
        camera_third_person_height;
    mikupan_configuration.third_person_camera.side = camera_third_person_side;
    mikupan_configuration.third_person_camera.look_ahead =
        camera_third_person_look_ahead;
    mikupan_configuration.third_person_camera.interest_height =
        camera_third_person_interest_height;
    mikupan_configuration.third_person_camera.fov_deg =
        camera_third_person_fov_deg;
    mikupan_configuration.first_person_camera.enabled =
        MikuPan_FirstPersonEnabled() ? 1 : 0;
    mikupan_configuration.first_person_camera.r3_toggle_enabled =
        mikupan_first_person_r3_toggle_enabled ? 1 : 0;
    mikupan_configuration.first_person_camera.auto_run_enabled =
        mikupan_first_person_auto_run_enabled ? 1 : 0;
    mikupan_configuration.first_person_camera.eye_height =
        mikupan_first_person_eye_height;
    mikupan_configuration.first_person_camera.eye_forward =
        mikupan_first_person_eye_forward;
    mikupan_configuration.first_person_camera.look_distance =
        mikupan_first_person_look_distance;
    mikupan_configuration.first_person_camera.fov_deg =
        mikupan_first_person_fov_deg;
    mikupan_configuration.first_person_camera.stick_yaw_speed_deg =
        mikupan_first_person_stick_yaw_speed_deg;
    mikupan_configuration.first_person_camera.stick_pitch_speed_deg =
        mikupan_first_person_stick_pitch_speed_deg;
    mikupan_configuration.input.selected_gamepad_index =
        MikuPan_ControllerGetPreferredGamepadIndex();
    MikuPan_ControllerStoreBindingsToConfig();
    MikuPan_UiSyncSettingsFromConfiguration();
}

static int MikuPan_UiSaveConfiguration(void)
{
    MikuPan_UiStoreRuntimeConfiguration();
    if (MikuPan_SaveConfiguration(NULL))
    {
        snprintf(config_save_status, sizeof(config_save_status),
                 "Saved configuration");
        return 1;
    }

    snprintf(config_save_status, sizeof(config_save_status),
             "Failed to save configuration");
    return 0;
}

static int CompareResolutionAsc(const void* a, const void* b)
{
    const MikuPan_Resolution* ra = (const MikuPan_Resolution*) a;
    const MikuPan_Resolution* rb = (const MikuPan_Resolution*) b;
    int area_a = ra->width * ra->height;
    int area_b = rb->width * rb->height;
    return area_a - area_b;
}

static void MikuPan_AspectRatioStr(int w, int h, char* buf, int buf_size)
{
    int a = w, b = h;
    while (b)
    {
        int t = b;
        b = a % b;
        a = t;
    }
    /* a is now the GCD */
    snprintf(buf, buf_size, "%d:%d", w / a, h / a);
}

static void MikuPan_GetWindowPixelSize(int* out_width, int* out_height)
{
    int width = 0;
    int height = 0;

#ifndef __ANDROID__
    SDL_Window* window = MikuPan_GetUiWindow();
    if (window != NULL)
    {
        SDL_GetWindowSizeInPixels(window, &width, &height);
        if (width <= 0 || height <= 0)
        {
            SDL_GetWindowSize(window, &width, &height);
        }
    }
#endif

    if (width <= 0)
    {
        width = mikupan_configuration.renderer.window.width;
    }

    if (height <= 0)
    {
        height = mikupan_configuration.renderer.window.height;
    }

    if (width <= 0)
    {
        width = PS2_RESOLUTION_X_INT;
    }

    if (height <= 0)
    {
        height = PS2_RESOLUTION_Y_INT;
    }

    if (out_width != NULL)
    {
        *out_width = width;
    }

    if (out_height != NULL)
    {
        *out_height = height;
    }
}

static void MikuPan_RefreshWindowBackedRenderResolution(void)
{
    if (mikupan_configuration.renderer.render_mode
            == MIKUPAN_RENDER_RESOLUTION_MATCH_WINDOW
        || mikupan_configuration.renderer.render_mode
               == MIKUPAN_RENDER_RESOLUTION_WINDOW_SCALE)
    {
        render_resolution_width = MikuPan_GetRenderResolutionWidth();
        render_resolution_height = MikuPan_GetRenderResolutionHeight();
    }
}

static int MikuPan_IsPs2ScaleResolution(int width, int height)
{
    if (width <= 0 || height <= 0)
    {
        return 0;
    }

    for (int scale = 1; scale <= MIKUPAN_MAX_PS2_RESOLUTION_SCALE; scale++)
    {
        if (width == PS2_RESOLUTION_X_INT * scale
            && height == PS2_RESOLUTION_Y_INT * scale)
        {
            return 1;
        }
    }

    return 0;
}

static void MikuPan_AddRenderResolutionOption(int mode, int width, int height,
                                              int percent,
                                              const char* label)
{
    if (resolution_count >= MIKUPAN_MAX_RESOLUTIONS)
    {
        return;
    }

    if (mode == MIKUPAN_RENDER_RESOLUTION_FIXED
        && (width <= 0 || height <= 0))
    {
        return;
    }

    for (int i = 0; i < resolution_count; i++)
    {
        if (resolution_mode_list[i] == mode
            && resolution_list[i].width == width
            && resolution_list[i].height == height
            && resolution_scale_percent_list[i] == percent)
        {
            return;
        }
    }

    const int index = resolution_count++;
    resolution_list[index].width = width;
    resolution_list[index].height = height;
    resolution_mode_list[index] = mode;
    resolution_scale_percent_list[index] = percent;

    snprintf(resolution_labels[index], sizeof(resolution_labels[index]), "%s",
             label != NULL ? label : "Custom");
    resolution_label_ptrs[index] = resolution_labels[index];
}

static void MikuPan_AddWindowScaleResolution(int percent)
{
    char label[64];
    if (percent == 100)
    {
        snprintf(label, sizeof(label), "Match Window (100%%)");
    }
    else
    {
        snprintf(label, sizeof(label), "%d%% of Window", percent);
    }

    MikuPan_AddRenderResolutionOption(
        percent == 100 ? MIKUPAN_RENDER_RESOLUTION_MATCH_WINDOW
                       : MIKUPAN_RENDER_RESOLUTION_WINDOW_SCALE,
        0,
        0,
        percent,
        label);
}

static void MikuPan_AddPs2ResolutionScale(int scale)
{
    const int width = PS2_RESOLUTION_X_INT * scale;
    const int height = PS2_RESOLUTION_Y_INT * scale;
    char label[64];

    if (scale == 1)
    {
        snprintf(label, sizeof(label), "PS2 Native (%d x %d)", width, height);
    }
    else
    {
        snprintf(label, sizeof(label), "PS2 %dx (%d x %d)", scale, width, height);
    }

    MikuPan_AddRenderResolutionOption(
        MIKUPAN_RENDER_RESOLUTION_FIXED,
        width,
        height,
        100,
        label);
}

static void MikuPan_PopulateResolutionList(SDL_DisplayID display,
                                           const SDL_DisplayMode* current_mode)
{
    (void)display;
    (void)current_mode;

    resolution_count = 0;
    resolution_selected = 0;

    static const int window_scales[] = {
        50, 75, 90, 100, 110, 120, 125, 150, 175, 200,
    };

    for (int i = 0; i < (int)(sizeof(window_scales) / sizeof(window_scales[0])); i++)
    {
        MikuPan_AddWindowScaleResolution(window_scales[i]);
    }

    for (int scale = 1; scale <= MIKUPAN_MAX_PS2_RESOLUTION_SCALE; scale++)
    {
        MikuPan_AddPs2ResolutionScale(scale);
    }

    if (mikupan_configuration.renderer.render_mode == MIKUPAN_RENDER_RESOLUTION_FIXED
        && !MikuPan_IsPs2ScaleResolution(render_resolution_width,
                                         render_resolution_height))
    {
        char label[64];
        snprintf(label, sizeof(label), "%d x %d",
                 render_resolution_width, render_resolution_height);
        MikuPan_AddRenderResolutionOption(MIKUPAN_RENDER_RESOLUTION_FIXED,
                                          render_resolution_width,
                                          render_resolution_height,
                                          100,
                                          label);
    }

    for (int i = 0; i < resolution_count; i++)
    {
        const int mode = resolution_mode_list[i];
        int selected = 0;
        if (mode == MIKUPAN_RENDER_RESOLUTION_MATCH_WINDOW)
        {
            selected = mikupan_configuration.renderer.render_mode
                       == MIKUPAN_RENDER_RESOLUTION_MATCH_WINDOW;
        }
        else if (mode == MIKUPAN_RENDER_RESOLUTION_WINDOW_SCALE)
        {
            selected = mikupan_configuration.renderer.render_mode
                       == MIKUPAN_RENDER_RESOLUTION_WINDOW_SCALE
                       && mikupan_configuration.renderer.render_scale_percent
                              == resolution_scale_percent_list[i];
        }
        else
        {
            selected = mikupan_configuration.renderer.render_mode
                       == MIKUPAN_RENDER_RESOLUTION_FIXED
                       && resolution_list[i].width == render_resolution_width
                       && resolution_list[i].height == render_resolution_height;
        }

        if (selected)
        {
            resolution_selected = i;
        }
    }
}

static void MikuPan_AddWindowSize(int w, int h)
{
    if (w <= 0 || h <= 0 || window_size_count >= MIKUPAN_MAX_WINDOW_SIZES)
    {
        return;
    }

    for (int i = 0; i < window_size_count; i++)
    {
        if (window_size_list[i].width == w && window_size_list[i].height == h)
        {
            return;
        }
    }

    window_size_list[window_size_count].width = w;
    window_size_list[window_size_count].height = h;
    window_size_count++;
}

static int MikuPan_IsStandardWindowSize(int width, int height)
{
    static const MikuPan_Resolution standard_sizes[] = {
        {640, 480},
        {800, 600},
        {960, 540},
        {1024, 576},
        {1024, 768},
        {1280, 720},
        {1280, 800},
        {1366, 768},
        {1600, 900},
        {1680, 1050},
        {1920, 1080},
        {1920, 1200},
        {2560, 1440},
        {2560, 1600},
        {3840, 2160},
    };

    for (int i = 0; i < (int)(sizeof(standard_sizes) / sizeof(standard_sizes[0]));
         i++)
    {
        if (standard_sizes[i].width == width
            && standard_sizes[i].height == height)
        {
            return 1;
        }
    }

    return 0;
}

static void MikuPan_AddStandardWindowSizes(int max_width, int max_height)
{
    static const MikuPan_Resolution standard_sizes[] = {
        {640, 480},
        {800, 600},
        {960, 540},
        {1024, 576},
        {1024, 768},
        {1280, 720},
        {1280, 800},
        {1366, 768},
        {1600, 900},
        {1680, 1050},
        {1920, 1080},
        {1920, 1200},
        {2560, 1440},
        {2560, 1600},
        {3840, 2160},
    };
    const int count = (int)(sizeof(standard_sizes) / sizeof(standard_sizes[0]));

    for (int i = 0; i < count; i++)
    {
        const int width = standard_sizes[i].width;
        const int height = standard_sizes[i].height;
        if ((max_width <= 0 || width <= max_width)
            && (max_height <= 0 || height <= max_height))
        {
            MikuPan_AddWindowSize(width, height);
        }
    }
}

static void MikuPan_AddDisplayModeWindowSizes(SDL_DisplayID display)
{
#ifndef __ANDROID__
    if (display == 0)
    {
        return;
    }

    int count = 0;
    SDL_DisplayMode** modes = SDL_GetFullscreenDisplayModes(display, &count);
    if (modes == NULL)
    {
        return;
    }

    for (int i = 0; i < count; i++)
    {
        if (modes[i] != NULL)
        {
            MikuPan_AddWindowSize(modes[i]->w, modes[i]->h);
        }
    }

    SDL_free(modes);
#else
    (void)display;
#endif
}

static void MikuPan_FormatWindowSizeLabel(char* buffer, int buffer_size,
                                          int width, int height,
                                          const char* suffix)
{
    snprintf(buffer, buffer_size, "%d x %d%s",
             width, height, suffix != NULL ? suffix : "");
}

static void MikuPan_PopulateWindowSizeList(SDL_DisplayID display,
                                           const SDL_DisplayMode* current_mode)
{
    (void)display;
    window_size_count = 0;
    window_size_selected = 0;

    int max_width = 0;
    int max_height = 0;
    if (current_mode != NULL)
    {
        max_width = current_mode->w;
        max_height = current_mode->h;
    }

    MikuPan_AddDisplayModeWindowSizes(display);
    MikuPan_AddStandardWindowSizes(max_width, max_height);
    MikuPan_AddWindowSize(mikupan_configuration.renderer.window.width,
                          mikupan_configuration.renderer.window.height);

    if (current_mode != NULL && current_mode->w > 0 && current_mode->h > 0)
    {
        MikuPan_AddWindowSize(current_mode->w, current_mode->h);
    }

    if (window_size_count > 1)
    {
        qsort(window_size_list, window_size_count,
              sizeof(MikuPan_Resolution), CompareResolutionAsc);
    }

    for (int i = 0; i < window_size_count; i++)
    {
        const int is_desktop = current_mode != NULL
            && window_size_list[i].width == current_mode->w
            && window_size_list[i].height == current_mode->h;
        const int is_standard = MikuPan_IsStandardWindowSize(
            window_size_list[i].width,
            window_size_list[i].height);

        const char* suffix = is_desktop ? " [Desktop]" : "";
        MikuPan_FormatWindowSizeLabel(window_size_labels[i],
                                      sizeof(window_size_labels[i]),
                                      window_size_list[i].width,
                                      window_size_list[i].height,
                                      suffix);
        window_size_label_ptrs[i] = window_size_labels[i];

        if (window_size_list[i].width == mikupan_configuration.renderer.window.width
            && window_size_list[i].height == mikupan_configuration.renderer.window.height)
        {
            window_size_selected = i;
        }
    }
}

static const char* MikuPan_GpuDriverDisplayName(const char* name)
{
    if (SDL_strcasecmp(name, "vulkan") == 0)
    {
        return "Vulkan";
    }
    if (SDL_strcasecmp(name, "direct3d12") == 0)
    {
        return "Direct3D 12";
    }
    if (SDL_strcasecmp(name, "metal") == 0)
    {
        return "Metal";
    }
    return name;
}

static void MikuPan_PopulateGpuDriverList(void)
{
    gpu_driver_names[0][0] = '\0';
    snprintf(gpu_driver_labels[0], sizeof(gpu_driver_labels[0]), "Auto");
    gpu_driver_supported[0] = 1;
    gpu_driver_count = 1;
    gpu_driver_selected = 0;

    const int n = SDL_GetNumGPUDrivers();
    for (int i = 0; i < n && gpu_driver_count <= MIKUPAN_MAX_GPU_DRIVERS; i++)
    {
        const char* name = SDL_GetGPUDriver(i);
        if (name == NULL)
        {
            continue;
        }

        const int idx = gpu_driver_count;
        snprintf(gpu_driver_names[idx], sizeof(gpu_driver_names[idx]), "%s",
                 name);
        gpu_driver_supported[idx] =
            SDL_GPUSupportsShaderFormats(MIKUPAN_GPU_SHADER_FORMATS, name) ? 1
                                                                           : 0;
        snprintf(gpu_driver_labels[idx], sizeof(gpu_driver_labels[idx]),
                 "%s%s", MikuPan_GpuDriverDisplayName(name),
                 gpu_driver_supported[idx] ? "" : " (unsupported)");

        if (SDL_strcasecmp(name, mikupan_configuration.renderer.gpu_driver)
            == 0)
        {
            gpu_driver_selected = idx;
        }

        gpu_driver_count++;
    }
}

static void MikuPan_UiGpuBackendCombo(void)
{
    if (igBeginCombo("GPU Backend", gpu_driver_labels[gpu_driver_selected], 0))
    {
        for (int i = 0; i < gpu_driver_count; i++)
        {
            bool is_selected = (gpu_driver_selected == i);

            if (!gpu_driver_supported[i])
            {
                igBeginDisabled(true);
            }

            if (igSelectable_Bool(gpu_driver_labels[i], is_selected, 0,
                                  ImVec2{0, 0}))
            {
                gpu_driver_selected = i;
                snprintf(mikupan_configuration.renderer.gpu_driver,
                         sizeof(mikupan_configuration.renderer.gpu_driver),
                         "%s", gpu_driver_names[i]);
            }

            if (!gpu_driver_supported[i])
            {
                igEndDisabled();
            }

            if (is_selected)
            {
                igSetItemDefaultFocus();
            }
        }

        igEndCombo();
    }

    SDL_GPUDevice* device = MikuPan_GPUGetDevice();
    const char* active =
        (device != NULL) ? SDL_GetGPUDeviceDriver(device) : NULL;
    igTextDisabled("Active: %s", (active != NULL)
                                     ? MikuPan_GpuDriverDisplayName(active)
                                     : "none");

    if (gpu_driver_names[gpu_driver_selected][0] != '\0'
        && (active == NULL
            || SDL_strcasecmp(gpu_driver_names[gpu_driver_selected], active)
                   != 0))
    {
        igTextDisabled("Save Configuration and restart to apply");
    }
}

static void MikuPan_UiDitherModeCombo(void)
{
    enum { MIKUPAN_DITHER_OPTION_COUNT = 2 };
    const char* labels[MIKUPAN_DITHER_OPTION_COUNT];
    int selected = MikuPan_GetSelectedDitherModeOption();

    for (int i = 0; i < MIKUPAN_DITHER_OPTION_COUNT; i++)
    {
        const char* label = MikuPan_GetDitherModeOptionLabel(i);
        labels[i] = label != NULL ? label : "Unknown";
    }

    if (igCombo_Str_arr("Dither Filtering", &selected, labels,
                        MIKUPAN_DITHER_OPTION_COUNT, -1))
    {
        MikuPan_SelectDitherModeOption(selected);
        MikuPan_ConfigurationValidate();
    }
}

static void MikuPan_DataFolderSelected(void* userdata,
                                       const char* const * filelist, int filter)
{
    (void) userdata;
    (void) filter;

    if (filelist == NULL || filelist[0] == NULL)
    {
        return;
    }

    strncpy(mikupan_configuration.data_folder, filelist[0],
            sizeof(mikupan_configuration.data_folder) - 1);
    mikupan_configuration
        .data_folder[sizeof(mikupan_configuration.data_folder) - 1] = '\0';
}

void MikuPan_UiSettingsInit(void)
{
    MikuPan_UiSyncSettingsFromConfiguration();

    SDL_DisplayID primary = SDL_GetPrimaryDisplay();

    if (primary != 0)
    {
        const SDL_DisplayMode* mode = SDL_GetCurrentDisplayMode(primary);
        MikuPan_PopulateResolutionList(primary, mode);
        MikuPan_PopulateWindowSizeList(primary, mode);
    }

    MikuPan_PopulateGpuDriverList();

    MikuPan_SetShadowResolution(mikupan_configuration.renderer.shadow_resolution);
    MikuPan_ApplyThirdPersonCameraConfiguration();
    MikuPan_ApplyFirstPersonCameraConfiguration();
    MikuPan_ControllerSetPreferredGamepadIndex(mikupan_configuration.input.selected_gamepad_index);
    mikupan_configuration.input.selected_gamepad_index = MikuPan_ControllerGetPreferredGamepadIndex();
    MikuPan_ControllerLoadBindingsFromConfig();
}

void MikuPan_UiSettingsRender(void)
{
    if (igBeginMenu("Settings", 1))
    {
        if (igBeginMenu("Display", 1))
        {
            const char* window_modes[] = {"Windowed", "Fullscreen"};
            int display_mode =
                window_mode == MIKUPAN_WINDOW_BORDERLESS ? 1 : 0;
            if (igCombo_Str_arr("Display Mode", &display_mode, window_modes, 2,
                                -1))
            {
                window_mode = display_mode == 1 ? MIKUPAN_WINDOW_BORDERLESS
                                                : MIKUPAN_WINDOW_WINDOWED;
                is_fullscreen = (window_mode != MIKUPAN_WINDOW_WINDOWED);
            }

            if (window_size_count > 0)
            {
                if (igCombo_Str_arr("Windowed Resolution", &window_size_selected,
                                    window_size_label_ptrs, window_size_count,
                                    -1))
                {
                    MikuPan_SelectWindowSizeOption(window_size_selected);
                }
            }
            else
            {
                igTextDisabled("Windowed Resolution: no modes available");
            }

            if (resolution_count > 0)
            {
                if (igCombo_Str_arr("Render Resolution", &resolution_selected,
                                    resolution_label_ptrs, resolution_count,
                                    -1))
                {
                    MikuPan_SelectRenderResolutionOption(resolution_selected);
                }
            }
            else
            {
                igTextDisabled("Render Resolution: no modes available");
            }

            igSliderFloat("Brightness", &brightness, 0.0f, 2.0f, "%.2f", 0);
            igSliderFloat("Gamma", &gamma_value, 0.1f, 2.0f, "%.2f", 0);
            igSliderFloat("Contrast", &contrast, 0.0f, 2.0f, "%.2f", 0);
            igSliderFloat("Shadow Depth", &shadow_depth, 0.0f, 2.0f, "%.2f", 0);
            igCheckbox("VSync", (bool*) &mikupan_configuration.renderer.vsync);

            igSeparatorText("Graphics");

            MikuPan_UiGpuBackendCombo();

            if (strcmp(SDL_GetPlatform(), "Android") == 0)
            {
                igTextDisabled("MSAA: disabled on Android");
            }
            else if (MikuPan_IsSuperSamplingEnabledInternal())
            {
                msaa_samples = 0;
                mikupan_configuration.renderer.msaa_index = 0;
                igTextDisabled("MSAA: disabled while supersampling");
            }
            else
            {
                char msaa_dropdown_list[32];
                snprintf(msaa_dropdown_list, sizeof(msaa_dropdown_list), "%s",
                         msaa_list[msaa_samples] == 0 ? "Off" : "");
                if (msaa_list[msaa_samples] != 0)
                {
                    snprintf(msaa_dropdown_list, sizeof(msaa_dropdown_list), "%dx",
                             msaa_list[msaa_samples]);
                }

                if (igBeginCombo("MSAA", msaa_dropdown_list, 0))
                {
                    for (int i = 0; i < sizeof(msaa_list)/sizeof(msaa_list[0]); i++)
                    {
                        bool is_selected = (msaa_samples == i);
                        if (msaa_list[i] == 0)
                        {
                            snprintf(msaa_dropdown_list, sizeof(msaa_dropdown_list),
                                     "Off");
                        }
                        else
                        {
                            snprintf(msaa_dropdown_list, sizeof(msaa_dropdown_list),
                                     "%dx", msaa_list[i]);
                        }

                        if (igSelectable_Bool(msaa_dropdown_list, is_selected, 0,
                                              ImVec2{0, 0}))
                        {
                            msaa_samples = i;
                        }

                        if (is_selected)
                        {
                            igSetItemDefaultFocus();
                        }
                    }

                    igEndCombo();
                }

                igTextDisabled("Scene samples: %dx", MikuPan_GPUGetSceneMSAA());
            }

            {
                bool ssao_enabled = MikuPan_IsSsaoEnabled() != 0;
                if (igCheckbox("SSAO", &ssao_enabled))
                {
                    MikuPan_SetSsaoEnabled(ssao_enabled ? 1 : 0);
                }

                if (MikuPan_GPUGetSceneMSAA() > 1)
                {
                    igTextDisabled("SSAO requires MSAA Off; scene depth is multisampled.");
                }
                else if (!MikuPan_GPUIsSceneDepthTextureSampleable())
                {
                    igTextDisabled("SSAO unavailable: backend cannot sample the scene depth format.");
                }

                if (MikuPan_IsSsaoEnabled())
                {
                    float ssao_strength = MikuPan_GetSsaoStrength();
                    float ssao_radius = MikuPan_GetSsaoRadius();
                    float ssao_bias = MikuPan_GetSsaoBias();
                    if (igSliderFloat("SSAO Strength", &ssao_strength, 0.0f,
                                      1.5f, "%.2f", 0))
                    {
                        MikuPan_SetSsaoStrength(ssao_strength);
                    }
                    if (igSliderFloat("SSAO Radius", &ssao_radius, 1.0f,
                                      24.0f, "%.1f", 0))
                    {
                        MikuPan_SetSsaoRadius(ssao_radius);
                    }
                    if (igSliderFloat("SSAO Bias", &ssao_bias, 0.0f,
                                      0.05f, "%.4f", 0))
                    {
                        MikuPan_SetSsaoBias(ssao_bias);
                    }
                }
            }

            {
                bool shafts_enabled = MikuPan_IsVolumetricShaftsEnabled() != 0;
                if (igCheckbox("Volumetric Shafts", &shafts_enabled))
                {
                    MikuPan_SetVolumetricShaftsEnabled(shafts_enabled ? 1 : 0);
                }

                if (MikuPan_GPUGetSceneMSAA() > 1)
                {
                    igTextDisabled("Volumetric shafts require MSAA Off; scene depth is multisampled.");
                }
                else if (!MikuPan_GPUIsSceneDepthTextureSampleable())
                {
                    igTextDisabled("Volumetric shafts unavailable: backend cannot sample the scene depth format.");
                }

                if (MikuPan_IsVolumetricShaftsEnabled())
                {
                    float shaft_strength = MikuPan_GetVolumetricShaftsStrength();
                    float shaft_radius = MikuPan_GetVolumetricShaftsRadius();
                    float shaft_density = MikuPan_GetVolumetricShaftsDensity();
                    if (igSliderFloat("Shaft Strength", &shaft_strength, 0.0f,
                                      1.5f, "%.2f", 0))
                    {
                        MikuPan_SetVolumetricShaftsStrength(shaft_strength);
                    }
                    if (igSliderFloat("Shaft Radius", &shaft_radius, 0.15f,
                                      1.5f, "%.2f", 0))
                    {
                        MikuPan_SetVolumetricShaftsRadius(shaft_radius);
                    }
                    if (igSliderFloat("Shaft Density", &shaft_density, 0.0f,
                                      1.5f, "%.2f", 0))
                    {
                        MikuPan_SetVolumetricShaftsDensity(shaft_density);
                    }
                }
            }

            {
                bool bloom_enabled = MikuPan_IsBloomEnabled() != 0;
                if (igCheckbox("Bloom", &bloom_enabled))
                {
                    MikuPan_SetBloomEnabled(bloom_enabled ? 1 : 0);
                }

                if (MikuPan_IsBloomEnabled())
                {
                    float bloom_strength = MikuPan_GetBloomStrength();
                    float bloom_threshold = MikuPan_GetBloomThreshold();
                    float bloom_radius = MikuPan_GetBloomRadius();
                    if (igSliderFloat("Bloom Strength", &bloom_strength, 0.0f,
                                      1.5f, "%.2f", 0))
                    {
                        MikuPan_SetBloomStrength(bloom_strength);
                    }
                    if (igSliderFloat("Bloom Threshold", &bloom_threshold, 0.0f,
                                      1.0f, "%.2f", 0))
                    {
                        MikuPan_SetBloomThreshold(bloom_threshold);
                    }
                    if (igSliderFloat("Bloom Radius", &bloom_radius, 0.5f,
                                      16.0f, "%.1f", 0))
                    {
                        MikuPan_SetBloomRadius(bloom_radius);
                    }
                }
            }

            MikuPan_UiShadowResolutionCombo("Shadow Resolution");

            {
                bool soft_shadows = MikuPan_IsSoftShadowsEnabled() != 0;
                if (igCheckbox("Soft Shadows", &soft_shadows))
                {
                    MikuPan_SetSoftShadowsEnabled(soft_shadows ? 1 : 0);
                }

                if (MikuPan_IsSoftShadowsEnabled())
                {
                    float shadow_radius = MikuPan_GetSoftShadowRadius();
                    if (igSliderFloat("Soft Shadow Radius", &shadow_radius,
                                      0.0f, 8.0f, "%.2f", 0))
                    {
                        MikuPan_SetSoftShadowRadius(shadow_radius);
                    }
                }
            }

            const char* display_lighting_modes[] = {"Pixel (Modern)",
                                                    "Vertex (PS2)"};
            igCombo_Str_arr("Lighting Mode", &mikupan_configuration.renderer.lighting_mode,
                            display_lighting_modes, 2, -1);

            MikuPan_UiDitherModeCombo();
            igTextDisabled("Save Configuration to keep Dither Filtering after restart.");

            const char* finder_mask_modes[] = {"Black", "Blur"};
            int finder_mask_mode =
                mikupan_configuration.renderer.finder_viewport_mask_mode;
            if (igCombo_Str_arr("Finder Surround", &finder_mask_mode,
                                finder_mask_modes, 2, -1))
            {
                mikupan_configuration.renderer.finder_viewport_mask_mode =
                    finder_mask_mode;
            }

            igEndMenu();
        }

        if (igBeginMenu("Theme", 1))
        {
            MikuPan_ConfigurationValidate();
            int selected_theme = mikupan_configuration.selected_theme;

            if (igCombo_Str_arr("Theme", &selected_theme, MikuPan_UiThemeLabels,
                                MIKUPAN_UI_THEME_COUNT, -1))
            {
                mikupan_configuration.selected_theme = selected_theme;
                MikuPan_ApplyFatalFrameStyle(selected_theme);
            }

            int selected_font = mikupan_configuration.selected_font;

            if (igCombo_Str_arr("Font", &selected_font, MikuPan_UiFontLabels,
                                MIKUPAN_UI_FONT_COUNT, -1))
            {
                MikuPan_SelectFontOption(selected_font);
            }

            float font_scale = mikupan_configuration.font_scale;
            if (igSliderFloat("Font Size", &font_scale, 0.5f, 3.0f, "%.2fx", 0))
            {
                mikupan_configuration.font_scale = font_scale;
                MikuPan_ApplyUiFontScale();
            }

            igSeparator();
            int title_room_background = MikuPan_TitleUseRoomBackground();
            if (igCheckbox("Room title background",
                           (bool*) &title_room_background))
            {
                MikuPan_TitleSetUseRoomBackground(title_room_background);
            }

            int title_dither = MikuPan_TitleUseDither();
            if (igCheckbox("Title dithering", (bool*) &title_dither))
            {
                MikuPan_TitleSetUseDither(title_dither);
            }

            igEndMenu();
        }

        if (igBeginMenu("CRT", 1))
        {
            igCheckbox("Enabled", (bool*) &crt_settings.enabled);
            if (crt_settings.enabled)
            {
                igSliderFloat("Strength", &crt_settings.strength, 0.0f, 1.0f,
                          "%.2f", 0);
                igSliderFloat("Curvature", &crt_settings.curvature, 0.0f, 0.30f,
                              "%.3f", 0);
                igSliderFloat("Overscan", &crt_settings.overscan, 0.0f, 0.12f,
                              "%.3f", 0);

                igSeparator();
                igSliderFloat("Scanline Strength", &crt_settings.scanline_strength,
                              0.0f, 1.0f, "%.2f", 0);
                igSliderFloat("Scanline Scale", &crt_settings.scanline_scale, 0.25f,
                              3.0f, "%.2f", 0);
                igSliderFloat("Scanline Thickness",
                              &crt_settings.scanline_thickness, 0.5f, 4.0f, "%.2f",
                              0);

                igSeparator();
                igSliderFloat("Mask Strength", &crt_settings.mask_strength, 0.0f,
                              1.0f, "%.2f", 0);
                igSliderFloat("Mask Scale", &crt_settings.mask_scale, 0.5f, 4.0f,
                              "%.2f", 0);

                igSeparator();
                igSliderFloat("Vignette Strength", &crt_settings.vignette_strength,
                              0.0f, 1.0f, "%.2f", 0);
                igSliderFloat("Vignette Size", &crt_settings.vignette_size, 0.25f,
                              1.25f, "%.2f", 0);
                igSliderFloat("Chroma Offset", &crt_settings.chroma_offset, 0.0f,
                              3.0f, "%.2f", 0);
                igSliderFloat("Blend Strength", &crt_settings.blend_strength, 0.0f,
                              1.0f, "%.2f", 0);
                igSliderFloat("Blend Radius", &crt_settings.blend_radius, 0.0f,
                              3.0f, "%.2f", 0);
                igSliderFloat("Noise", &crt_settings.noise_strength, 0.0f, 0.15f,
                              "%.3f", 0);
                igSliderFloat("Flicker", &crt_settings.flicker_strength, 0.0f,
                              0.10f, "%.3f", 0);
                igSliderFloat("Glow", &crt_settings.glow_strength, 0.0f, 0.50f,
                              "%.2f", 0);

                mikupan_configuration.crt = crt_settings;
                MikuPan_ConfigurationValidate();
                crt_settings = mikupan_configuration.crt;

                if (igButton("Reset CRT", ImVec2{0.0f, 0.0f}))
                {
                    crt_settings = crt_defaults;
                    mikupan_configuration.crt = crt_settings;
                }
            }


            igEndMenu();
        }

        if (igBeginMenu("Input", 1))
        {
            MikuPan_ControllerDrawDeviceSelectorUi();
            igSeparator();

            igCheckbox("Controller / Joystick Mapping",
                       (bool*) &show_controller_remap);

            if (igMenuItem_Bool("Reset Bindings to Defaults", NULL, false,
                                true))
            {
                MikuPan_ControllerResetBindings();
            }

            igEndMenu();
        }

        igSeparatorText("Game Data Folder");
        igInputText("Folder", mikupan_configuration.data_folder, sizeof(mikupan_configuration.data_folder), 0, NULL, NULL);

        if (igButton("Browse...", ImVec2{0.0f, 0.0f}))
        {
            const char *start = mikupan_configuration.data_folder[0] != '\0'
                                    ? mikupan_configuration.data_folder
                                    : NULL;
#ifdef __ANDROID__
            SDL_SetHint("SDL_ANDROID_ALLOW_PERSISTENT_FOLDER_ACCESS", "1");
#endif
            SDL_ShowOpenFolderDialog(MikuPan_DataFolderSelected, NULL,
                                     MikuPan_GetUiWindow(), start, false);
        }

        igSeparator();

        if (igMenuItem_Bool("Save Configuration", NULL, false, true))
        {
            MikuPan_UiSaveConfiguration();
        }

        if (config_save_status[0] != '\0')
        {
            igTextDisabled("%s", config_save_status);
        }

        igEndMenu();
    }
}

int MikuPan_GetWindowSizeOptionCount(void)
{
    return window_size_count;
}

int MikuPan_GetSelectedWindowSizeOption(void)
{
    for (int i = 0; i < window_size_count; i++)
    {
        if (window_size_list[i].width == mikupan_configuration.renderer.window.width
            && window_size_list[i].height == mikupan_configuration.renderer.window.height)
        {
            window_size_selected = i;
            break;
        }
    }
    return window_size_selected;
}

const char* MikuPan_GetWindowSizeOptionLabel(int index)
{
    if (index < 0 || index >= window_size_count)
    {
        return NULL;
    }

    return window_size_label_ptrs[index];
}

int MikuPan_SelectWindowSizeOption(int index)
{
    if (index < 0 || index >= window_size_count)
    {
        return 0;
    }

    window_size_selected = index;
    const int width = window_size_list[index].width;
    const int height = window_size_list[index].height;

    mikupan_configuration.renderer.window.width = width;
    mikupan_configuration.renderer.window.height = height;

    if (window_mode == MIKUPAN_WINDOW_WINDOWED)
    {
        MikuPan_ApplyWindowedSizeNow(width, height);
    }

    MikuPan_RefreshWindowBackedRenderResolution();

    return 1;
}

int MikuPan_GetRenderResolutionWidth(void)
{
    if (mikupan_configuration.renderer.render_mode
        == MIKUPAN_RENDER_RESOLUTION_MATCH_WINDOW
        || mikupan_configuration.renderer.render_mode
               == MIKUPAN_RENDER_RESOLUTION_WINDOW_SCALE)
    {
        int width = 0;
        int height = 0;
        MikuPan_GetWindowPixelSize(&width, &height);

        if (mikupan_configuration.renderer.render_mode
            == MIKUPAN_RENDER_RESOLUTION_WINDOW_SCALE)
        {
            width = (int)roundf((float)width
                                * (float)mikupan_configuration.renderer.render_scale_percent
                                / 100.0f);
        }

        return MikuPan_ClampInt(width, PS2_RESOLUTION_X_INT / 2, 16384);
    }

    return render_resolution_width > 0 ? render_resolution_width
                                       : PS2_RESOLUTION_X_INT;
}

int MikuPan_GetRenderResolutionHeight(void)
{
    if (mikupan_configuration.renderer.render_mode
        == MIKUPAN_RENDER_RESOLUTION_MATCH_WINDOW
        || mikupan_configuration.renderer.render_mode
               == MIKUPAN_RENDER_RESOLUTION_WINDOW_SCALE)
    {
        int width = 0;
        int height = 0;
        MikuPan_GetWindowPixelSize(&width, &height);

        if (mikupan_configuration.renderer.render_mode
            == MIKUPAN_RENDER_RESOLUTION_WINDOW_SCALE)
        {
            height = (int)roundf((float)height
                                 * (float)mikupan_configuration.renderer.render_scale_percent
                                 / 100.0f);
        }

        return MikuPan_ClampInt(height, PS2_RESOLUTION_Y_INT / 2, 16384);
    }

    return render_resolution_height > 0 ? render_resolution_height
                                        : PS2_RESOLUTION_Y_INT;
}

static int MikuPan_IsSuperSamplingEnabledInternal(void)
{
    int window_width = 0;
    int window_height = 0;
    MikuPan_GetWindowPixelSize(&window_width, &window_height);

    if (window_width <= 0 || window_height <= 0)
    {
        return 0;
    }

    const int render_width = MikuPan_GetRenderResolutionWidth();
    const int render_height = MikuPan_GetRenderResolutionHeight();
    return render_width > window_width || render_height > window_height;
}

int MikuPan_GetRenderResolutionOptionCount(void)
{
    return resolution_count;
}

int MikuPan_GetSelectedRenderResolutionOption(void)
{
    return resolution_selected;
}

const char* MikuPan_GetRenderResolutionOptionLabel(int index)
{
    if (index < 0 || index >= resolution_count)
    {
        return NULL;
    }

    return resolution_label_ptrs[index];
}

int MikuPan_SelectRenderResolutionOption(int index)
{
    if (index < 0 || index >= resolution_count)
    {
        return 0;
    }

    resolution_selected = index;
    const int mode = resolution_mode_list[index];
    mikupan_configuration.renderer.render_mode = mode;

    if (mode == MIKUPAN_RENDER_RESOLUTION_WINDOW_SCALE)
    {
        mikupan_configuration.renderer.render_scale_percent =
            resolution_scale_percent_list[index];
        render_resolution_width = MikuPan_GetRenderResolutionWidth();
        render_resolution_height = MikuPan_GetRenderResolutionHeight();
    }
    else if (mode == MIKUPAN_RENDER_RESOLUTION_MATCH_WINDOW)
    {
        mikupan_configuration.renderer.render_scale_percent = 100;
        render_resolution_width = MikuPan_GetRenderResolutionWidth();
        render_resolution_height = MikuPan_GetRenderResolutionHeight();
    }
    else
    {
        render_resolution_width = resolution_list[index].width;
        render_resolution_height = resolution_list[index].height;
        mikupan_configuration.renderer.render.width = render_resolution_width;
        mikupan_configuration.renderer.render.height = render_resolution_height;
        mikupan_configuration.renderer.render_scale_percent = 100;
    }

    MikuPan_ConfigurationValidate();
    if (MikuPan_IsSuperSamplingEnabledInternal())
    {
        msaa_samples = 0;
        mikupan_configuration.renderer.msaa_index = 0;
    }
    return 1;
}

int MikuPan_GetMSAA(void)
{
    if (MikuPan_IsSuperSamplingEnabledInternal())
    {
        return 0;
    }

    return msaa_list[msaa_samples];
}

int MikuPan_GetMSAAOptionCount(void)
{
    return (int) (sizeof(msaa_list) / sizeof(msaa_list[0]));
}

int MikuPan_GetSelectedMSAAOption(void)
{
    if (MikuPan_IsSuperSamplingEnabledInternal())
    {
        return 0;
    }

    return msaa_samples;
}

int MikuPan_IsSuperSamplingEnabled(void)
{
    return MikuPan_IsSuperSamplingEnabledInternal();
}

const char* MikuPan_GetMSAAOptionLabel(int index)
{
    static char labels[sizeof(msaa_list) / sizeof(msaa_list[0])][8];
    if (index < 0 || index >= MikuPan_GetMSAAOptionCount())
    {
        return NULL;
    }

    if (msaa_list[index] == 0)
    {
        snprintf(labels[index], sizeof(labels[index]), "Off");
    }
    else
    {
        snprintf(labels[index], sizeof(labels[index]), "%dx", msaa_list[index]);
    }
    return labels[index];
}

int MikuPan_SelectMSAAOption(int index)
{
    if (index < 0 || index >= MikuPan_GetMSAAOptionCount())
    {
        return 0;
    }

    if (MikuPan_IsSuperSamplingEnabledInternal())
    {
        msaa_samples = 0;
        mikupan_configuration.renderer.msaa_index = 0;
        return 1;
    }

    msaa_samples = index;
    return 1;
}

int MikuPan_ShowControllerRemapWindow(void)
{
    return show_controller_remap;
}

void MikuPan_SetControllerRemapWindowVisible(int visible)
{
    show_controller_remap = visible ? 1 : 0;
}

void MikuPan_ResetControllerBindingsFromUi(void)
{
    MikuPan_ControllerResetBindings();
}

float MikuPan_GetBrightness(void)
{
    return brightness;
}

void MikuPan_SetBrightness(float value)
{
    brightness = MikuPan_ClampFloat(value, 0.0f, 2.0f);
}

float MikuPan_GetGamma(void)
{
    return gamma_value;
}

void MikuPan_SetGamma(float value)
{
    gamma_value = MikuPan_ClampFloat(value, 0.1f, 2.0f);
}

float MikuPan_GetContrast(void)
{
    return contrast;
}

void MikuPan_SetContrast(float value)
{
    contrast = MikuPan_ClampFloat(value, 0.0f, 2.0f);
}

float MikuPan_GetShadowDepth(void)
{
    return shadow_depth;
}

void MikuPan_SetShadowDepth(float value)
{
    shadow_depth = MikuPan_ClampFloat(value, 0.0f, 2.0f);
}

int MikuPan_IsHdrEnabled(void)
{
    return hdr_enabled;
}

void MikuPan_SetHdrEnabled(int enabled)
{
    hdr_enabled = enabled ? 1 : 0;
}

float MikuPan_GetHdrPaperWhite(void)
{
    return hdr_paper_white;
}

void MikuPan_SetHdrPaperWhite(float value)
{
    hdr_paper_white = MikuPan_ClampFloat(value, 80.0f, 400.0f);
    if (hdr_peak_luminance < hdr_paper_white)
    {
        hdr_peak_luminance = hdr_paper_white;
    }
}

float MikuPan_GetHdrPeakLuminance(void)
{
    return hdr_peak_luminance;
}

void MikuPan_SetHdrPeakLuminance(float value)
{
    hdr_peak_luminance = MikuPan_ClampFloat(value, 100.0f, 4000.0f);
    if (hdr_peak_luminance < hdr_paper_white)
    {
        hdr_peak_luminance = hdr_paper_white;
    }
}

const MikuPan_ConfigCrt* MikuPan_GetCrtSettings(void)
{
    return &crt_settings;
}

void MikuPan_SetCrtSettings(const MikuPan_ConfigCrt* settings)
{
    if (settings == NULL)
    {
        return;
    }

    crt_settings = *settings;
    mikupan_configuration.crt = crt_settings;
    MikuPan_ConfigurationValidate();
    crt_settings = mikupan_configuration.crt;
}

void MikuPan_SetCrtEnabled(int enabled)
{
    crt_settings.enabled = enabled ? 1 : 0;
}

static float* MikuPan_GetCrtFieldPointer(int index)
{
    switch (index)
    {
        case 0:
            return &crt_settings.strength;
        case 1:
            return &crt_settings.curvature;
        case 2:
            return &crt_settings.overscan;
        case 3:
            return &crt_settings.scanline_strength;
        case 4:
            return &crt_settings.scanline_scale;
        case 5:
            return &crt_settings.scanline_thickness;
        case 6:
            return &crt_settings.mask_strength;
        case 7:
            return &crt_settings.mask_scale;
        case 8:
            return &crt_settings.vignette_strength;
        case 9:
            return &crt_settings.vignette_size;
        case 10:
            return &crt_settings.chroma_offset;
        case 11:
            return &crt_settings.blend_strength;
        case 12:
            return &crt_settings.blend_radius;
        case 13:
            return &crt_settings.noise_strength;
        case 14:
            return &crt_settings.flicker_strength;
        case 15:
            return &crt_settings.glow_strength;
        default:
            return NULL;
    }
}

int MikuPan_GetCrtFieldCount(void)
{
    return 16;
}

const char* MikuPan_GetCrtFieldLabel(int index)
{
    static const char* labels[] = {
        "Strength",
        "Curvature",
        "Overscan",
        "Scanline Strength",
        "Scanline Scale",
        "Scanline Thickness",
        "Mask Strength",
        "Mask Scale",
        "Vignette Strength",
        "Vignette Size",
        "Chroma Offset",
        "Blend Strength",
        "Blend Radius",
        "Noise",
        "Flicker",
        "Glow",
    };

    if (index < 0 || index >= MikuPan_GetCrtFieldCount())
    {
        return NULL;
    }

    return labels[index];
}

float MikuPan_GetCrtFieldValue(int index)
{
    float* value = MikuPan_GetCrtFieldPointer(index);
    return value != NULL ? *value : 0.0f;
}

float MikuPan_GetCrtFieldMin(int index)
{
    switch (index)
    {
        case 1:
        case 2:
        case 10:
        case 12:
            return 0.0f;
        case 4:
        case 9:
            return 0.25f;
        case 5:
        case 7:
            return 0.5f;
        default:
            return 0.0f;
    }
}

float MikuPan_GetCrtFieldMax(int index)
{
    switch (index)
    {
        case 1:
            return 0.30f;
        case 2:
            return 0.12f;
        case 4:
            return 3.0f;
        case 5:
        case 7:
            return 4.0f;
        case 9:
            return 1.25f;
        case 10:
        case 12:
            return 3.0f;
        case 13:
            return 0.15f;
        case 14:
            return 0.10f;
        case 15:
            return 0.50f;
        default:
            return 1.0f;
    }
}

float MikuPan_GetCrtFieldStep(int index)
{
    switch (index)
    {
        case 1:
        case 2:
        case 13:
        case 14:
            return 0.001f;
        default:
            return 0.01f;
    }
}

void MikuPan_SetCrtFieldValue(int index, float value)
{
    float* field = MikuPan_GetCrtFieldPointer(index);
    if (field == NULL)
    {
        return;
    }

    *field = value;
    mikupan_configuration.crt = crt_settings;
    MikuPan_ConfigurationValidate();
    crt_settings = mikupan_configuration.crt;
}

void MikuPan_ResetCrtSettings(void)
{
    crt_settings = crt_defaults;
    mikupan_configuration.crt = crt_settings;
}


static float MikuPan_ClampAudioVolume(float value)
{
    return MikuPan_ClampFloat(value, 0.0f, 1.0f);
}

float MikuPan_GetAudioMasterVolume(void)
{
    return MikuPan_ClampAudioVolume(mikupan_configuration.audio.master);
}

void MikuPan_SetAudioMasterVolume(float value)
{
    value = MikuPan_ClampAudioVolume(value);

    if (mikupan_configuration.audio.master == value)
    {
        return;
    }

    mikupan_configuration.audio.master = value;
    MikuPan_AudioBumpRevision();
}

int MikuPan_IsFullScreen(void)
{
    return is_fullscreen;
}

int MikuPan_GetWindowMode(void)
{
    return window_mode;
}

void MikuPan_SetWindowMode(int mode)
{
    if (mode == MIKUPAN_WINDOW_FULLSCREEN)
    {
        mode = MIKUPAN_WINDOW_BORDERLESS;
    }

    if (mode != MIKUPAN_WINDOW_WINDOWED && mode != MIKUPAN_WINDOW_BORDERLESS)
    {
        mode = MIKUPAN_WINDOW_WINDOWED;
    }

    window_mode = mode;
    is_fullscreen = (window_mode != MIKUPAN_WINDOW_WINDOWED);
    MikuPan_RefreshWindowBackedRenderResolution();
}

int MikuPan_IsVsync(void)
{
    return mikupan_configuration.renderer.vsync;
}

void MikuPan_SetVsync(int enabled)
{
    mikupan_configuration.renderer.vsync = enabled ? 1 : 0;
}

int MikuPan_GetGpuDriverOptionCount(void)
{
    return gpu_driver_count;
}

int MikuPan_GetSelectedGpuDriverOption(void)
{
    return gpu_driver_selected;
}

const char* MikuPan_GetGpuDriverOptionLabel(int index)
{
    if (index < 0 || index >= gpu_driver_count)
    {
        return NULL;
    }

    return gpu_driver_labels[index];
}

int MikuPan_IsGpuDriverOptionSupported(int index)
{
    if (index < 0 || index >= gpu_driver_count)
    {
        return 0;
    }

    return gpu_driver_supported[index];
}

int MikuPan_SelectGpuDriverOption(int index)
{
    if (index < 0 || index >= gpu_driver_count || !gpu_driver_supported[index])
    {
        return 0;
    }

    gpu_driver_selected = index;
    snprintf(mikupan_configuration.renderer.gpu_driver,
             sizeof(mikupan_configuration.renderer.gpu_driver), "%s",
             gpu_driver_names[index]);
    return 1;
}

const char* MikuPan_GetActiveGpuDriverLabel(void)
{
    SDL_GPUDevice* device = MikuPan_GPUGetDevice();
    const char* active = (device != NULL) ? SDL_GetGPUDeviceDriver(device) : NULL;
    return active != NULL ? MikuPan_GpuDriverDisplayName(active) : "none";
}

int MikuPan_IsGpuDriverRestartPending(void)
{
    SDL_GPUDevice* device = MikuPan_GPUGetDevice();
    const char* active = (device != NULL) ? SDL_GetGPUDeviceDriver(device) : NULL;

    return gpu_driver_names[gpu_driver_selected][0] != '\0'
           && (active == NULL
               || SDL_strcasecmp(gpu_driver_names[gpu_driver_selected], active)
                      != 0);
}

int MikuPan_GetShadowResolutionOptionCount(void)
{
    return 5;
}

int MikuPan_GetSelectedShadowResolutionOption(void)
{
    static const int res_values[] = {128, 256, 512, 1024, 2048};
    const int current = MikuPan_GetShadowResolution();
    for (int i = 0; i < MikuPan_GetShadowResolutionOptionCount(); i++)
    {
        if (res_values[i] == current)
        {
            return i;
        }
    }

    return 1;
}

const char* MikuPan_GetShadowResolutionOptionLabel(int index)
{
    static const char* labels[] = {"128", "256", "512", "1024", "2048"};
    if (index < 0 || index >= MikuPan_GetShadowResolutionOptionCount())
    {
        return NULL;
    }

    return labels[index];
}

int MikuPan_SelectShadowResolutionOption(int index)
{
    static const int res_values[] = {128, 256, 512, 1024, 2048};
    if (index < 0 || index >= MikuPan_GetShadowResolutionOptionCount())
    {
        return 0;
    }

    MikuPan_SetShadowResolution(res_values[index]);
    return 1;
}

int MikuPan_IsSoftShadowsEnabled(void)
{
    return mikupan_configuration.renderer.shadow_soft_enabled;
}

void MikuPan_SetSoftShadowsEnabled(int enabled)
{
    mikupan_configuration.renderer.shadow_soft_enabled = enabled ? 1 : 0;
}

float MikuPan_GetSoftShadowRadius(void)
{
    return mikupan_configuration.renderer.shadow_soft_radius;
}

void MikuPan_SetSoftShadowRadius(float value)
{
    mikupan_configuration.renderer.shadow_soft_radius =
        MikuPan_ClampFloat(value, 0.0f, 8.0f);
}

int MikuPan_GetLightingMode(void)
{
    return mikupan_configuration.renderer.lighting_mode;
}

void MikuPan_SetLightingMode(int mode)
{
    mikupan_configuration.renderer.lighting_mode = mode == 1 ? 1 : 0;
}

int MikuPan_GetDitherModeOptionCount(void)
{
    return 2;
}

int MikuPan_GetSelectedDitherModeOption(void)
{
    mikupan_configuration.renderer.dither_mode =
        MikuPan_ClampInt(mikupan_configuration.renderer.dither_mode, 0, 1);
    return mikupan_configuration.renderer.dither_mode;
}

const char* MikuPan_GetDitherModeOptionLabel(int index)
{
    static const char* labels[] = {"Native", "Soft"};
    if (index < 0 || index >= MikuPan_GetDitherModeOptionCount())
    {
        return NULL;
    }

    return labels[index];
}

int MikuPan_SelectDitherModeOption(int index)
{
    if (index < 0 || index >= MikuPan_GetDitherModeOptionCount())
    {
        return 0;
    }

    mikupan_configuration.renderer.dither_mode = index;
    return 1;
}

int MikuPan_IsSsaoEnabled(void)
{
    return mikupan_configuration.renderer.ssao_enabled;
}

void MikuPan_SetSsaoEnabled(int enabled)
{
    mikupan_configuration.renderer.ssao_enabled = enabled ? 1 : 0;
}

float MikuPan_GetSsaoStrength(void)
{
    return mikupan_configuration.renderer.ssao_strength;
}

void MikuPan_SetSsaoStrength(float value)
{
    mikupan_configuration.renderer.ssao_strength =
        MikuPan_ClampFloat(value, 0.0f, 1.5f);
}

float MikuPan_GetSsaoRadius(void)
{
    return mikupan_configuration.renderer.ssao_radius;
}

void MikuPan_SetSsaoRadius(float value)
{
    mikupan_configuration.renderer.ssao_radius =
        MikuPan_ClampFloat(value, 1.0f, 24.0f);
}

float MikuPan_GetSsaoBias(void)
{
    return mikupan_configuration.renderer.ssao_bias;
}

void MikuPan_SetSsaoBias(float value)
{
    mikupan_configuration.renderer.ssao_bias =
        MikuPan_ClampFloat(value, 0.0f, 0.05f);
}

int MikuPan_IsVolumetricShaftsEnabled(void)
{
    return mikupan_configuration.renderer.volumetric_shafts_enabled;
}

void MikuPan_SetVolumetricShaftsEnabled(int enabled)
{
    mikupan_configuration.renderer.volumetric_shafts_enabled = enabled ? 1 : 0;
}

float MikuPan_GetVolumetricShaftsStrength(void)
{
    return mikupan_configuration.renderer.volumetric_shafts_strength;
}

void MikuPan_SetVolumetricShaftsStrength(float value)
{
    mikupan_configuration.renderer.volumetric_shafts_strength =
        MikuPan_ClampFloat(value, 0.0f, 1.5f);
}

float MikuPan_GetVolumetricShaftsRadius(void)
{
    return mikupan_configuration.renderer.volumetric_shafts_radius;
}

void MikuPan_SetVolumetricShaftsRadius(float value)
{
    mikupan_configuration.renderer.volumetric_shafts_radius =
        MikuPan_ClampFloat(value, 0.15f, 1.5f);
}

float MikuPan_GetVolumetricShaftsDensity(void)
{
    return mikupan_configuration.renderer.volumetric_shafts_density;
}

void MikuPan_SetVolumetricShaftsDensity(float value)
{
    mikupan_configuration.renderer.volumetric_shafts_density =
        MikuPan_ClampFloat(value, 0.0f, 1.5f);
}

int MikuPan_IsBloomEnabled(void)
{
    return mikupan_configuration.renderer.bloom_enabled;
}

void MikuPan_SetBloomEnabled(int enabled)
{
    mikupan_configuration.renderer.bloom_enabled = enabled ? 1 : 0;
}

float MikuPan_GetBloomStrength(void)
{
    return mikupan_configuration.renderer.bloom_strength;
}

void MikuPan_SetBloomStrength(float value)
{
    mikupan_configuration.renderer.bloom_strength =
        MikuPan_ClampFloat(value, 0.0f, 1.5f);
}

float MikuPan_GetBloomThreshold(void)
{
    return mikupan_configuration.renderer.bloom_threshold;
}

void MikuPan_SetBloomThreshold(float value)
{
    mikupan_configuration.renderer.bloom_threshold =
        MikuPan_ClampFloat(value, 0.0f, 1.0f);
}

float MikuPan_GetBloomRadius(void)
{
    return mikupan_configuration.renderer.bloom_radius;
}

void MikuPan_SetBloomRadius(float value)
{
    mikupan_configuration.renderer.bloom_radius =
        MikuPan_ClampFloat(value, 0.5f, 16.0f);
}

int MikuPan_GetThemeOptionCount(void)
{
    return MIKUPAN_UI_THEME_COUNT;
}

int MikuPan_GetSelectedThemeOption(void)
{
    mikupan_configuration.selected_theme =
        MikuPan_ClampThemeIndex(mikupan_configuration.selected_theme);
    return mikupan_configuration.selected_theme;
}

const char* MikuPan_GetThemeOptionLabel(int index)
{
    if (index < 0 || index >= MIKUPAN_UI_THEME_COUNT)
    {
        return NULL;
    }

    return MikuPan_UiThemeLabels[index];
}

int MikuPan_SelectThemeOption(int index)
{
    if (index < 0 || index >= MIKUPAN_UI_THEME_COUNT)
    {
        return 0;
    }

    mikupan_configuration.selected_theme = index;
    MikuPan_ApplyFatalFrameStyle(index);
    MikuPan_RmlOptionsApplyTheme(index);
    return 1;
}

int MikuPan_GetFontOptionCount(void)
{
    return MIKUPAN_UI_FONT_COUNT;
}

int MikuPan_GetSelectedFontOption(void)
{
    mikupan_configuration.selected_font =
        MikuPan_ClampFontIndex(mikupan_configuration.selected_font);
    return mikupan_configuration.selected_font;
}

const char* MikuPan_GetFontOptionLabel(int index)
{
    if (index < 0 || index >= MIKUPAN_UI_FONT_COUNT)
    {
        return NULL;
    }

    return MikuPan_UiFontLabels[index];
}

int MikuPan_SelectFontOption(int index)
{
    if (index < 0 || index >= MIKUPAN_UI_FONT_COUNT)
    {
        return 0;
    }

    mikupan_configuration.selected_font = index;
    MikuPan_ApplyUiFont(index);
    MikuPan_RmlOptionsApplyFont(index);
    return 1;
}

float MikuPan_GetUiFontScale(void)
{
    return mikupan_configuration.font_scale;
}

void MikuPan_SetUiFontScale(float scale)
{
    mikupan_configuration.font_scale = MikuPan_ClampFloat(scale, 0.5f, 3.0f);
    MikuPan_ApplyUiFontScale();
}

const char* MikuPan_GetDataFolder(void)
{
    return mikupan_configuration.data_folder;
}

void MikuPan_SetDataFolder(const char* folder)
{
    if (folder == NULL)
    {
        folder = "";
    }

    strncpy(mikupan_configuration.data_folder, folder,
            sizeof(mikupan_configuration.data_folder) - 1);
    mikupan_configuration
        .data_folder[sizeof(mikupan_configuration.data_folder) - 1] = '\0';
}

void MikuPan_BrowseDataFolderFromUi(void)
{
    const char* start = mikupan_configuration.data_folder[0] != '\0'
                            ? mikupan_configuration.data_folder
                            : NULL;
#ifdef __ANDROID__
    SDL_SetHint("SDL_ANDROID_ALLOW_PERSISTENT_FOLDER_ACCESS", "1");
#endif
    SDL_ShowOpenFolderDialog(MikuPan_DataFolderSelected, NULL,
                             MikuPan_GetUiWindow(), start, false);
}

int MikuPan_SaveConfigurationFromUi(void)
{
    return MikuPan_UiSaveConfiguration();
}

const char* MikuPan_GetConfigSaveStatus(void)
{
    return config_save_status;
}
