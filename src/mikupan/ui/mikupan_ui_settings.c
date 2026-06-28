#include "mikupan_ui_settings.h"

#include "typedefs.h"

#define CIMGUI_DEFINE_ENUMS_AND_STRUCTS
#include "cimgui.h"
#include "SDL3/SDL_dialog.h"
#include "SDL3/SDL_hints.h"
#include "SDL3/SDL_platform.h"
#include "ingame/camera/camera.h"
#include "mikupan/mikupan_config.h"
#include "mikupan/mikupan_controller.h"
#include "mikupan/mikupan_first_person.h"
#include "mikupan/mikupan_utils.h"
#include "mikupan/rendering/mikupan_gpu.h"
#include "mikupan/rendering/mikupan_renderer.h"
#include "mikupan/ui/mikupan_ui.h"
#include "mikupan_ui_debug.h"
#include "mikupan_ui_theme.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MIKUPAN_MAX_RESOLUTIONS 96
#define MIKUPAN_MAX_PS2_RESOLUTION_SCALE 6
#define MIKUPAN_MAX_GPU_DRIVERS 8

static const int msaa_list[] = {0, 2, 4, 8, 16, 32};

static int show_controller_remap = 0;
static int msaa_samples = 0;
static int render_resolution_width = 1920;
static int render_resolution_height = 1080;

static MikuPan_Resolution resolution_list[MIKUPAN_MAX_RESOLUTIONS];
static char resolution_labels[MIKUPAN_MAX_RESOLUTIONS][48];
static const char* resolution_label_ptrs[MIKUPAN_MAX_RESOLUTIONS];
static int resolution_count = 0;
static int resolution_selected = 0;

static char gpu_driver_names[MIKUPAN_MAX_GPU_DRIVERS + 1][32];
static char gpu_driver_labels[MIKUPAN_MAX_GPU_DRIVERS + 1][64];
static int gpu_driver_supported[MIKUPAN_MAX_GPU_DRIVERS + 1];
static int gpu_driver_count = 0;
static int gpu_driver_selected = 0;

static char config_save_status[128] = {0};
static int is_fullscreen = 0;
static int window_mode = 0; /* MikuPan_WindowMode: 0=windowed, 1=fullscreen, 2=borderless */
static float brightness = 1.0f;
static float gamma_value = 1.0f;

#define MIKUPAN_CRT_DEFAULTS                                                   \
    {0,     1.0f,  0.08f, 0.02f, 0.18f, 1.0f,  1.6f,  0.22f, 1.0f,             \
     0.25f, 0.78f, 0.75f, 0.15f, 0.75f, 0.02f, 0.02f, 0.08f}

static const MikuPan_ConfigCrt crt_defaults = MIKUPAN_CRT_DEFAULTS;
static MikuPan_ConfigCrt crt_settings = MIKUPAN_CRT_DEFAULTS;

static int MikuPan_IsFitWindowResolution(int width, int height)
{
    return width == MIKUPAN_RENDER_RESOLUTION_FIT_WINDOW
           && height == MIKUPAN_RENDER_RESOLUTION_FIT_WINDOW;
}

static void MikuPan_GetFitWindowResolution(int* width, int* height)
{
    SDL_Window* window = MikuPan_GetUiWindow();
    *width = 0;
    *height = 0;

    if (window != NULL)
    {
        SDL_GetWindowSize(window, width, height);
    }

    if (*width <= 0 || *height <= 0)
    {
        *width = mikupan_configuration.renderer.window.width;
        *height = mikupan_configuration.renderer.window.height;
    }

    if (*width <= 0 || *height <= 0)
    {
        *width = PS2_RESOLUTION_X_INT;
        *height = PS2_RESOLUTION_Y_INT;
    }
}

static void MikuPan_UiSyncSettingsFromConfiguration(void)
{
    MikuPan_ConfigurationValidate();
    render_resolution_width = mikupan_configuration.renderer.render.width;
    render_resolution_height = mikupan_configuration.renderer.render.height;
    msaa_samples = mikupan_configuration.renderer.msaa_index;
    brightness = mikupan_configuration.renderer.brightness;
    gamma_value = mikupan_configuration.renderer.gamma;
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
    mikupan_configuration.renderer.msaa_index = msaa_samples;
    mikupan_configuration.renderer.shadow_resolution =
        MikuPan_GetShadowResolution();
    mikupan_configuration.renderer.brightness = brightness;
    mikupan_configuration.renderer.gamma = gamma_value;
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

static void MikuPan_UiSaveConfiguration(void)
{
    MikuPan_UiStoreRuntimeConfiguration();
    if (MikuPan_SaveConfiguration(NULL))
    {
        snprintf(config_save_status, sizeof(config_save_status),
                 "Saved configuration");
    }
    else
    {
        snprintf(config_save_status, sizeof(config_save_status),
                 "Failed to save configuration");
    }
}

static int CompareResolutionDesc(const void* a, const void* b)
{
    const MikuPan_Resolution* ra = (const MikuPan_Resolution*) a;
    const MikuPan_Resolution* rb = (const MikuPan_Resolution*) b;
    int area_a = ra->width * ra->height;
    int area_b = rb->width * rb->height;
    return area_b - area_a;
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

static int MikuPan_GetPs2ResolutionScale(int w, int h)
{
    if (w <= 0 || h <= 0)
    {
        return 0;
    }

    if (w % PS2_RESOLUTION_X_INT != 0 || h % PS2_RESOLUTION_Y_INT != 0)
    {
        return 0;
    }

    int scale_x = w / PS2_RESOLUTION_X_INT;
    int scale_y = h / PS2_RESOLUTION_Y_INT;

    return scale_x == scale_y ? scale_x : 0;
}

static void MikuPan_AddResolution(int w, int h)
{
    if (w <= 0 || h <= 0 || resolution_count >= MIKUPAN_MAX_RESOLUTIONS)
    {
        return;
    }

    for (int i = 0; i < resolution_count; i++)
    {
        if (resolution_list[i].width == w && resolution_list[i].height == h)
        {
            return;
        }
    }

    resolution_list[resolution_count].width = w;
    resolution_list[resolution_count].height = h;
    resolution_count++;
}

static void MikuPan_AddFitWindowResolution(void)
{
    if (resolution_count >= MIKUPAN_MAX_RESOLUTIONS)
    {
        return;
    }

    resolution_list[resolution_count].width =
        MIKUPAN_RENDER_RESOLUTION_FIT_WINDOW;
    resolution_list[resolution_count].height =
        MIKUPAN_RENDER_RESOLUTION_FIT_WINDOW;
    resolution_count++;
}

static void MikuPan_AddPs2ResolutionMultiples(void)
{
    for (int scale = 1; scale <= MIKUPAN_MAX_PS2_RESOLUTION_SCALE; scale++)
    {
        MikuPan_AddResolution(PS2_RESOLUTION_X_INT * scale,
                              PS2_RESOLUTION_Y_INT * scale);
    }
}

static void MikuPan_PopulateResolutionList(SDL_DisplayID display,
                                           const SDL_DisplayMode* current_mode)
{
    resolution_count = 0;
    resolution_selected = 0;

    MikuPan_AddFitWindowResolution();
    MikuPan_AddPs2ResolutionMultiples();
    MikuPan_AddResolution(render_resolution_width, render_resolution_height);

    int n = 0;
    SDL_DisplayMode** modes = SDL_GetFullscreenDisplayModes(display, &n);

    if (modes != NULL)
    {
        for (int i = 0; i < n; i++)
        {
            MikuPan_AddResolution(modes[i]->w, modes[i]->h);
        }

        SDL_free(modes);
    }

    // Always include the current desktop mode; older SDL backends sometimes
    // omit it from the fullscreen list.
    if (current_mode != NULL && current_mode->w > 0 && current_mode->h > 0)
    {
        MikuPan_AddResolution(current_mode->w, current_mode->h);
    }

    if (resolution_count > 1)
    {
        qsort(&resolution_list[1], resolution_count - 1,
              sizeof(MikuPan_Resolution), CompareResolutionDesc);
    }

    for (int i = 0; i < resolution_count; i++)
    {
        char aspect[12];
        int ps2_scale;

        if (MikuPan_IsFitWindowResolution(resolution_list[i].width,
                                          resolution_list[i].height))
        {
            snprintf(resolution_labels[i], sizeof(resolution_labels[i]),
                     "Fit Window");
            resolution_label_ptrs[i] = resolution_labels[i];

            if (MikuPan_IsFitWindowResolution(render_resolution_width,
                                              render_resolution_height))
            {
                resolution_selected = i;
            }

            continue;
        }

        ps2_scale = MikuPan_GetPs2ResolutionScale(
            resolution_list[i].width, resolution_list[i].height);

        MikuPan_AspectRatioStr(resolution_list[i].width,
                               resolution_list[i].height, aspect,
                               sizeof(aspect));

        if (ps2_scale > 0)
        {
            snprintf(resolution_labels[i], sizeof(resolution_labels[i]),
                     "%d x %d (%s) [PS2 %dx]", resolution_list[i].width,
                     resolution_list[i].height, aspect, ps2_scale);
        }
        else
        {
            snprintf(resolution_labels[i], sizeof(resolution_labels[i]),
                     "%d x %d (%s)", resolution_list[i].width,
                     resolution_list[i].height, aspect);
        }

        resolution_label_ptrs[i] = resolution_labels[i];

        if (resolution_list[i].width == render_resolution_width
            && resolution_list[i].height == render_resolution_height)
        {
            resolution_selected = i;
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
            const char* window_modes[] = {"Windowed", "Fullscreen",
                                          "Borderless Fullscreen"};
            if (igCombo_Str_arr("Display Mode", &window_mode, window_modes, 3,
                                -1))
            {
                if (window_mode < MIKUPAN_WINDOW_WINDOWED
                    || window_mode > MIKUPAN_WINDOW_BORDERLESS)
                {
                    window_mode = MIKUPAN_WINDOW_WINDOWED;
                }
                is_fullscreen = (window_mode != MIKUPAN_WINDOW_WINDOWED);
            }

            if (resolution_count > 0)
            {
                if (igCombo_Str_arr("Resolution", &resolution_selected,
                                    resolution_label_ptrs, resolution_count,
                                    -1))
                {
                    render_resolution_width =
                        resolution_list[resolution_selected].width;
                    render_resolution_height =
                        resolution_list[resolution_selected].height;
                }
            }
            else
            {
                igTextDisabled("Resolution: no display modes available");
            }

            igSliderFloat("Brightness", &brightness, 0.0f, 2.0f, "%.2f", 0);
            igSliderFloat("Gamma", &gamma_value, 0.1f, 3.0f, "%.2f", 0);
            igCheckbox("VSync", (bool*) &mikupan_configuration.renderer.vsync);

            igSeparatorText("Graphics");

            MikuPan_UiGpuBackendCombo();

            if (strcmp(SDL_GetPlatform(), "Android") == 0)
            {
                igTextDisabled("MSAA: disabled on Android");
            }
            else
            {
                char msaa_dropdown_list[32];
                snprintf(msaa_dropdown_list, sizeof(msaa_dropdown_list), "%d",
                         msaa_list[msaa_samples]);

                if (igBeginCombo("MSAA", msaa_dropdown_list, 0))
                {
                    for (int i = 0; i < sizeof(msaa_list)/sizeof(msaa_list[0]); i++)
                    {
                        bool is_selected = (msaa_samples == i);
                        snprintf(msaa_dropdown_list, sizeof(msaa_dropdown_list),
                                 "%d", msaa_list[i]);

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

            MikuPan_UiShadowResolutionCombo("Shadow Resolution");

            const char* display_lighting_modes[] = {"Pixel (Modern)",
                                                    "Vertex (PS2)"};
            igCombo_Str_arr("Lighting Mode", &mikupan_configuration.renderer.lighting_mode,
                            display_lighting_modes, 2, -1);

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
                mikupan_configuration.selected_font = selected_font;
                MikuPan_ApplyUiFont(selected_font);
            }

            float font_scale = mikupan_configuration.font_scale;
            if (igSliderFloat("Font Size", &font_scale, 0.5f, 3.0f, "%.2fx", 0))
            {
                mikupan_configuration.font_scale = font_scale;
                MikuPan_ApplyUiFontScale();
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

int MikuPan_GetRenderResolutionWidth(void)
{
    if (MikuPan_IsFitWindowResolution(render_resolution_width,
                                      render_resolution_height))
    {
        int width = 0;
        int height = 0;
        MikuPan_GetFitWindowResolution(&width, &height);
        return width;
    }

    return render_resolution_width;
}

int MikuPan_GetRenderResolutionHeight(void)
{
    if (MikuPan_IsFitWindowResolution(render_resolution_width,
                                      render_resolution_height))
    {
        int width = 0;
        int height = 0;
        MikuPan_GetFitWindowResolution(&width, &height);
        return height;
    }

    return render_resolution_height;
}

int MikuPan_GetMSAA(void)
{
    return msaa_list[msaa_samples];
}

int MikuPan_ShowControllerRemapWindow(void)
{
    return show_controller_remap;
}

float MikuPan_GetBrightness(void)
{
    return brightness;
}

float MikuPan_GetGamma(void)
{
    return gamma_value;
}

const MikuPan_ConfigCrt* MikuPan_GetCrtSettings(void)
{
    return &crt_settings;
}

int MikuPan_IsFullScreen(void)
{
    return is_fullscreen;
}

int MikuPan_GetWindowMode(void)
{
    return window_mode;
}

int MikuPan_IsVsync(void)
{
    return mikupan_configuration.renderer.vsync;
}
