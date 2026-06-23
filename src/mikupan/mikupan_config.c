#include "mikupan_config.h"
#include "mikupan_utils.h"

#define MIKUPAN_CONFIG_THEME_COUNT 6
#define MIKUPAN_CONFIG_FONT_COUNT 2
#define MIKUPAN_CONFIG_DEFAULT_WINDOW_WIDTH 1280
#define MIKUPAN_CONFIG_DEFAULT_WINDOW_HEIGHT 720
#define MIKUPAN_CONFIG_MSAA_COUNT 6
#define MIKUPAN_CONFIG_DEFAULT_MSAA_INDEX 4

MikuPan_Config mikupan_configuration = {
    {
        {
            1280,
            720
        },
        {
            640,
            448
        },
        0,
        0,
        1,
        0,
        2,
        256,
        1.0f,
        1.0f,
        "",
        0
    },
    {
        0,
        1.0f,
        0.08f,
        0.02f,
        0.18f,
        1.0f,
        1.6f,
        0.22f,
        1.0f,
        0.25f,
        0.78f,
        0.75f,
        0.15f,
        0.75f,
        0.02f,
        0.02f,
        0.08f
    },
    2,
    1,
    1.0f,
    0,
    {
        0,
        900.0f,
        520.0f,
        120.0f,
        1100.0f,
        360.0f,
        51.0f
    },
    {
        0,
        1,
        1,
        650.0f,
        -100.0f,
        1000.0f,
        60.0f,
        2.4f,
        2.0f
    },
    {
        -1
    },
    ""
};

static int MikuPan_ConfigClampIndex(int value, int count, int fallback)
{
    if (value < 0 || value >= count)
    {
        return fallback;
    }

    return value;
}

static int MikuPan_ConfigIsFitWindowResolution(
    const MikuPan_Resolution* resolution)
{
    return resolution->width == MIKUPAN_RENDER_RESOLUTION_FIT_WINDOW
           && resolution->height == MIKUPAN_RENDER_RESOLUTION_FIT_WINDOW;
}

static void MikuPan_ConfigurationValidateRenderer(
    MikuPan_ConfigRenderer* renderer)
{
    if (renderer->window.width <= 0)
    {
        renderer->window.width = MIKUPAN_CONFIG_DEFAULT_WINDOW_WIDTH;
    }

    if (renderer->window.height <= 0)
    {
        renderer->window.height = MIKUPAN_CONFIG_DEFAULT_WINDOW_HEIGHT;
    }

    if (!MikuPan_ConfigIsFitWindowResolution(&renderer->render)
        && renderer->render.width <= 0)
    {
        renderer->render.width = PS2_RESOLUTION_X_INT;
    }

    if (!MikuPan_ConfigIsFitWindowResolution(&renderer->render)
        && renderer->render.height <= 0)
    {
        renderer->render.height = PS2_RESOLUTION_Y_INT;
    }

    if (renderer->window_mode == MIKUPAN_WINDOW_WINDOWED
        && renderer->is_fullscreen)
    {
        renderer->window_mode = MIKUPAN_WINDOW_FULLSCREEN;
    }

    if (renderer->window_mode < MIKUPAN_WINDOW_WINDOWED
        || renderer->window_mode > MIKUPAN_WINDOW_BORDERLESS)
    {
        renderer->window_mode = MIKUPAN_WINDOW_WINDOWED;
    }

    renderer->is_fullscreen =
        (renderer->window_mode != MIKUPAN_WINDOW_WINDOWED);
    renderer->vsync = renderer->vsync ? 1 : 0;

    if (renderer->lighting_mode < 0 || renderer->lighting_mode > 1)
    {
        renderer->lighting_mode = 0;
    }

    if (renderer->msaa_index < 0
        || renderer->msaa_index >= MIKUPAN_CONFIG_MSAA_COUNT)
    {
        renderer->msaa_index = MIKUPAN_CONFIG_DEFAULT_MSAA_INDEX;
    }

    if (renderer->shadow_resolution <= 0)
    {
        renderer->shadow_resolution = 256;
    }

    if (renderer->brightness < 0.0f || renderer->brightness > 2.0f)
    {
        renderer->brightness = 1.0f;
    }

    if (renderer->gamma < 0.0f || renderer->gamma > 3.0f)
    {
        renderer->gamma = 1.0f;
    }

    renderer->gpu_debug = renderer->gpu_debug ? 1 : 0;
}

static void MikuPan_ConfigurationValidateCrt(MikuPan_ConfigCrt* crt)
{
    crt->enabled = crt->enabled ? 1 : 0;
    crt->strength = MikuPan_ClampFloat(crt->strength, 0.0f, 1.0f);
    crt->curvature = MikuPan_ClampFloat(crt->curvature, 0.0f, 0.30f);
    crt->overscan = MikuPan_ClampFloat(crt->overscan, 0.0f, 0.12f);
    crt->scanline_strength =
        MikuPan_ClampFloat(crt->scanline_strength, 0.0f, 1.0f);
    crt->scanline_scale = MikuPan_ClampFloat(crt->scanline_scale, 0.25f, 3.0f);
    crt->scanline_thickness =
        MikuPan_ClampFloat(crt->scanline_thickness, 0.5f, 4.0f);
    crt->mask_strength = MikuPan_ClampFloat(crt->mask_strength, 0.0f, 1.0f);
    crt->mask_scale = MikuPan_ClampFloat(crt->mask_scale, 0.5f, 4.0f);
    crt->vignette_strength =
        MikuPan_ClampFloat(crt->vignette_strength, 0.0f, 1.0f);
    crt->vignette_size = MikuPan_ClampFloat(crt->vignette_size, 0.25f, 1.25f);
    crt->chroma_offset = MikuPan_ClampFloat(crt->chroma_offset, 0.0f, 3.0f);
    crt->blend_strength = MikuPan_ClampFloat(crt->blend_strength, 0.0f, 1.0f);
    crt->blend_radius = MikuPan_ClampFloat(crt->blend_radius, 0.0f, 3.0f);
    crt->noise_strength = MikuPan_ClampFloat(crt->noise_strength, 0.0f, 0.15f);
    crt->flicker_strength =
        MikuPan_ClampFloat(crt->flicker_strength, 0.0f, 0.10f);
    crt->glow_strength = MikuPan_ClampFloat(crt->glow_strength, 0.0f, 0.50f);
}

static void MikuPan_ConfigurationValidateThirdPersonCamera(
    MikuPan_ConfigThirdPersonCamera* tps)
{
    tps->enabled = tps->enabled ? 1 : 0;
    tps->distance = MikuPan_ClampFloat(tps->distance, 100.0f, 2500.0f);
    tps->height = MikuPan_ClampFloat(tps->height, 0.0f, 1400.0f);
    tps->side = MikuPan_ClampFloat(tps->side, -600.0f, 600.0f);
    tps->look_ahead = MikuPan_ClampFloat(tps->look_ahead, 100.0f, 2500.0f);
    tps->interest_height =
        MikuPan_ClampFloat(tps->interest_height, -400.0f, 1200.0f);
    tps->fov_deg = MikuPan_ClampFloat(tps->fov_deg, 20.0f, 90.0f);
}

static void MikuPan_ConfigurationValidateFirstPersonCamera(
    MikuPan_ConfigFirstPersonCamera* fps)
{
    fps->enabled = fps->enabled ? 1 : 0;
    fps->r3_toggle_enabled = fps->r3_toggle_enabled ? 1 : 0;
    fps->auto_run_enabled = fps->auto_run_enabled ? 1 : 0;
    fps->eye_height = MikuPan_ClampFloat(fps->eye_height, 350.0f, 900.0f);
    fps->eye_forward = MikuPan_ClampFloat(fps->eye_forward, -100.0f, 200.0f);
    fps->look_distance =
        MikuPan_ClampFloat(fps->look_distance, 250.0f, 2500.0f);
    fps->fov_deg = MikuPan_ClampFloat(fps->fov_deg, 20.0f, 90.0f);
    fps->stick_yaw_speed_deg =
        MikuPan_ClampFloat(fps->stick_yaw_speed_deg, 0.1f, 8.0f);
    fps->stick_pitch_speed_deg =
        MikuPan_ClampFloat(fps->stick_pitch_speed_deg, 0.1f, 8.0f);
}

static void MikuPan_ConfigurationValidateInput(MikuPan_ConfigInput* input)
{
    input->bindings_saved = input->bindings_saved ? 1 : 0;
    input->action_profile_saved = input->action_profile_saved ? 1 : 0;
    input->action_profile_enabled = input->action_profile_enabled ? 1 : 0;
    input->action_profile_subjective_move =
        input->action_profile_subjective_move ? 1 : 0;
    input->action_profile_dpad_subjective_move =
        input->action_profile_dpad_subjective_move ? 1 : 0;
    input->action_profile_stick_subjective_move =
        input->action_profile_stick_subjective_move ? 1 : 0;
    input->action_profile_finder_reverse_y =
        input->action_profile_finder_reverse_y ? 1 : 0;
    input->action_profile_finder_swap_sticks =
        input->action_profile_finder_swap_sticks ? 1 : 0;
    input->finder_mouse_enabled = input->finder_mouse_enabled ? 1 : 0;

    if (input->finder_mouse_sensitivity < 0.0f)
    {
        input->finder_mouse_sensitivity = 0.0f;
    }
}

void MikuPan_ConfigurationValidate(void)
{
    MikuPan_ConfigurationValidateRenderer(&mikupan_configuration.renderer);
    MikuPan_ConfigurationValidateCrt(&mikupan_configuration.crt);
    MikuPan_ConfigurationValidateThirdPersonCamera(
        &mikupan_configuration.third_person_camera);
    MikuPan_ConfigurationValidateFirstPersonCamera(
        &mikupan_configuration.first_person_camera);
    MikuPan_ConfigurationValidateInput(&mikupan_configuration.input);

    mikupan_configuration.selected_theme =
        MikuPan_ConfigClampIndex(mikupan_configuration.selected_theme,
                                 MIKUPAN_CONFIG_THEME_COUNT, 0);
    mikupan_configuration.selected_font =
        MikuPan_ConfigClampIndex(mikupan_configuration.selected_font,
                                 MIKUPAN_CONFIG_FONT_COUNT, 1);
    mikupan_configuration.font_scale =
        MikuPan_ClampFloat(mikupan_configuration.font_scale, 0.5f, 3.0f);
    mikupan_configuration.show_fps = mikupan_configuration.show_fps ? 1 : 0;
}
