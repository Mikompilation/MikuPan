#include "typedefs.h"

#define CIMGUI_DEFINE_ENUMS_AND_STRUCTS
#include "cimgui.h"

#include "glad/gl.h"
#include "graphics/graph2d/g2d_debug.h"
#include "ingame/camera/camera.h"
#include "main/glob.h"
#include "mikupan/gs/mikupan_texture_manager_c.h"
#include "mikupan/mikupan_controller.h"
#include "mikupan/mikupan_screenshot.h"
#include "mikupan/mikupan_utils.h"
#include "mikupan/rendering/mikupan_gpu.h"
#include "mikupan/rendering/mikupan_meshcache.h"
#include "mikupan/rendering/mikupan_profiler.h"
#include "mikupan/rendering/mikupan_renderer.h"
#include "mikupan/rendering/mikupan_shader.h"
#include "mikupan/ui/mikupan_ui.h"
#include "typedefs.h"

#include "mikupan/mikupan_config.h"

#include "SDL3/SDL_dialog.h"

#include "mikupan_version.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// -- Backend wrappers (defined in mikupan_ui.cpp) ----------------------------
void MikuPan_ImGui_ImplInit(SDL_Window* window);
void MikuPan_ImGui_ImplShutdown(void);
void MikuPan_ImGui_ImplNewFrame(void);
void MikuPan_ImGui_ImplProcessEvent(SDL_Event* event);
void MikuPan_ImGui_ImplRenderDrawData(void);

// -- Renderer-side timing accessors (defined in mikupan_renderer.c) ----------
float MikuPan_GetLastFrameCpuMs(void);
float MikuPan_GetLastFrameGpuMs(void);
float MikuPan_PerfGetSectionMs(int section);
int MikuPan_PerfGetTexL1Hits(void);
int MikuPan_PerfGetTexL1Misses(void);

/// CPU section IDs (must match MikuPan_PerfSection in mikupan_profiler.h)
#define MP_PERF_MESH_RENDER 0
#define MP_PERF_SPRITE_RENDER 1
#define MP_PERF_BATCH_FLUSH 2
#define MP_PERF_DRAWUI 3
#define MP_PERF_RENDERUI 4
#define MP_PERF_DRAW_SUBMIT 5
#define MP_PERF_BUFFER_UPLOAD 6
#define MP_PERF_STATE_CHANGE 7

/// Sub-sections decomposing MP_PERF_STATE_CHANGE (sum ≈ STATE_CHANGE).
#define MP_PERF_SC_SHADER 8
#define MP_PERF_SC_TEXTURE 9
#define MP_PERF_SC_RS3D 10
#define MP_PERF_SC_VAO 11

/// Per-mesh-type sub-sections decomposing MP_PERF_MESH_RENDER.
#define MP_PERF_MESH_0x2 12
#define MP_PERF_MESH_0xA 13
#define MP_PERF_MESH_0x10 14
#define MP_PERF_MESH_0x12 15
#define MP_PERF_MESH_0x32 16
#define MP_PERF_MESH_0x82 17

/// Sub-sections decomposing MP_PERF_SC_TEXTURE — fine-grained breakdown of
/// where MikuPan_SetTexture spends its time.
#define MP_PERF_TEX_L1_LOOKUP 18
#define MP_PERF_TEX_HASH 19
#define MP_PERF_TEX_L2_LOOKUP 20
#define MP_PERF_TEX_CREATE 21
#define MP_PERF_TEX_BIND 22

/// "Other / unknown" breakdown — uninstrumented PS2/SG scene-graph emulation.
#define MP_PERF_SKIN_PREP 23
#define MP_PERF_COORD_CALC 24
#define MP_PERF_LIGHT_SETUP 25

// -- GS instrumentation (defined in mikupan_gs.cpp) --------------------------
int MikuPan_GsGetUploadCount(void);
int MikuPan_GsGetUploadBytes(void);
float MikuPan_GsGetUploadMs(void);
int MikuPan_GsGetDownloadCount(void);
int MikuPan_GsGetDownloadBytes(void);
float MikuPan_GsGetDownloadMs(void);

// -- State -------------------------------------------------------------------
const int msaa_list[] = {0, 2, 4, 8};
int show_fps = 0;
int show_menu_bar = 0;

static int show_frame_time_graph = 0;
static int render_wireframe = 0;
static int render_normals = 0;
static int msaa_samples = 0;

// Render resolution defaults to the primary display's current mode. The combo
// box below is populated from the primary display's supported fullscreen modes
// in MikuPan_InitUi; until that runs it falls back to a sensible 1080p.
static int render_resolution_width = 1920;
static int render_resolution_height = 1080;

#define MIKUPAN_MAX_RESOLUTIONS 96
#define MIKUPAN_MAX_PS2_RESOLUTION_SCALE 6

static MikuPan_Resolution resolution_list[MIKUPAN_MAX_RESOLUTIONS];
static char resolution_labels[MIKUPAN_MAX_RESOLUTIONS][48];
static const char* resolution_label_ptrs[MIKUPAN_MAX_RESOLUTIONS];
static int resolution_count = 0;
static int resolution_selected = 0;

/* SDL_GPU backend selector. Index 0 is always "Auto" (empty driver name); the
 * rest mirror SDL_GetGPUDriver. Populated once in MikuPan_InitUi. Drivers that
 * consume none of the shipped shader formats (MIKUPAN_GPU_SHADER_FORMATS)
 * stay listed but disabled. The device is created once at startup, so a
 * change applies on the next launch. */
#define MIKUPAN_MAX_GPU_DRIVERS 8
static char gpu_driver_names[MIKUPAN_MAX_GPU_DRIVERS + 1][32];
static char gpu_driver_labels[MIKUPAN_MAX_GPU_DRIVERS + 1][64];
static int gpu_driver_supported[MIKUPAN_MAX_GPU_DRIVERS + 1];
static int gpu_driver_count = 0;
static int gpu_driver_selected = 0;

static int show_texture_list = 0;
static int show_controller_remap = 0;
static int show_shader_reload = 0;
static int show_draw_inspector = 0;
static int show_camera_debug = 0;
static int show_photo_debug_window = 0;
static int show_shadow_debug_window = 0;
static int cheat_tofu_mode = 0;
static float cheat_tofu_color[3] = {0.86f, 0.81f, 0.63f};
static int shadow_debug_auto_probe = 0;
static int shadow_debug_flip_y = 1;
static float shadow_debug_preview_size = 384.0f;
static float photo_debug_preview_size = 256.0f;
static char config_save_status[128] = {0};
static SDL_Window* ui_window = NULL;

// Last reload error (per-shader: index 0..MAX-1; index MAX = "reload all")
// — kept around so the user can read why the last reload failed without it
// flying past in the log.
static char last_reload_error[1280] = {0};

static int is_fullscreen = 0;
static int window_mode =
    0; /* MikuPan_WindowMode: 0=windowed, 1=fullscreen, 2=borderless */
static int is_vsync = 0;
static int disable_gs_uploads = 0;
static int show_bounding_boxes = 0;
static int show_mesh_0x82 = 1;
static int show_mesh_0x32 = 1;
static int show_mesh_0x10 = 1;
static int show_mesh_0x12 = 1;
static int show_mesh_0x2 = 1;
static int disable_lighting = 0;
static int show_static_lighting = 0;
static int mesh_lighting_mode = 1;// 0 = per-fragment, 1 = per-vertex
static float normal_length = 10.0f;

const int no_navigation_window =
    ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoDecoration
    | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_AlwaysAutoResize
    | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoInputs;

// The perf panel has interactive controls (collapsible trees) so it can't use
// no_navigation_window — NoInputs/NoNav would eat every click. But keep it fixed
// and chrome-free like before: no move, no decoration/title bar, transparent
// background, auto-sized. The only difference from no_navigation_window is that
// inputs/nav stay enabled so the sub-sections can actually expand.
const int perf_window_flags =
    ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoDecoration
    | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_AlwaysAutoResize;

// Post-process tone controls — applied by the POSTPROCESS_SHADER on the
// final scene-to-window blit. Defaults are no-ops (1.0 brightness, 1.0
// gamma); the renderer reads them via the getters below each frame and
// pushes them as uniforms.
static float brightness = 1.0f;
static float gamma_value = 1.0f;

#define MIKUPAN_UI_THEME_COUNT 6
#define MIKUPAN_UI_FONT_COUNT 2

static const char* theme_labels[MIKUPAN_UI_THEME_COUNT] = {
    "Moonlit Blue", "Ghost Cyan", "Crimson",
    "FF1 Ritual",   "Mist Teal",  "Sepia Photo",
};

static const char* font_labels[MIKUPAN_UI_FONT_COUNT] = {
    "ImGui Default Monospace",
    "Century Old Style",
};

static ImFont* ui_fonts[MIKUPAN_UI_FONT_COUNT] = {0};
static float ui_display_scale = 1.0f;
static float ui_font_default_size = 13.0f;
static float ui_font_regular_size = 14.0f;

#define MIKUPAN_CRT_DEFAULTS                                                   \
    {0,     1.0f,  0.08f, 0.02f, 0.18f, 1.0f,  1.6f,  0.22f, 1.0f,             \
     0.25f, 0.78f, 0.75f, 0.15f, 0.75f, 0.02f, 0.02f, 0.08f}

static const MikuPan_ConfigCrt crt_defaults = MIKUPAN_CRT_DEFAULTS;
static MikuPan_ConfigCrt crt_settings = MIKUPAN_CRT_DEFAULTS;

static void MikuPan_UiStoreRuntimeConfiguration(void);

static int MikuPan_ClampThemeIndex(int theme)
{
    if (theme < 0 || theme >= MIKUPAN_UI_THEME_COUNT)
    {
        return 0;
    }

    return theme;
}

static int MikuPan_ClampFontIndex(int font)
{
    if (font < 0 || font >= MIKUPAN_UI_FONT_COUNT)
    {
        return 1;
    }

    return font;
}

static float MikuPan_ClampFloat(float value, float min_value, float max_value)
{
    if (value < min_value)
    {
        return min_value;
    }

    if (value > max_value)
    {
        return max_value;
    }

    return value;
}

static float MikuPan_CalculateUiDisplayScale(SDL_Window* window)
{
    SDL_DisplayID display = 0;
    const SDL_DisplayMode* mode = NULL;
    float content_scale = 1.0f;
    float resolution_scale = 1.0f;
    float scale;

    if (window != NULL)
    {
        display = SDL_GetDisplayForWindow(window);
        content_scale = SDL_GetWindowDisplayScale(window);
    }

    if (display == 0)
    {
        display = SDL_GetPrimaryDisplay();
    }

    if (display != 0)
    {
        mode = SDL_GetCurrentDisplayMode(display);
        if (content_scale <= 0.0f)
        {
            content_scale = SDL_GetDisplayContentScale(display);
        }
    }

    if (mode != NULL && mode->w > 0 && mode->h > 0)
    {
        const float sx = (float) mode->w / 1920.0f;
        const float sy = (float) mode->h / 1080.0f;
        resolution_scale = sx < sy ? sx : sy;
    }

    if (content_scale <= 0.0f)
    {
        content_scale = 1.0f;
    }

    scale = content_scale > resolution_scale ? content_scale : resolution_scale;
    return MikuPan_ClampFloat(scale, 0.85f, 2.25f);
}

static void MikuPan_ClampCrtSettings(MikuPan_ConfigCrt* crt)
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

static void
MikuPan_ClampThirdPersonCameraSettings(MikuPan_ConfigThirdPersonCamera* tps)
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

static void MikuPan_ApplyThirdPersonCameraConfiguration(void)
{
    MikuPan_ConfigThirdPersonCamera* tps =
        &mikupan_configuration.third_person_camera;

    MikuPan_ClampThirdPersonCameraSettings(tps);
    camera_third_person_enabled = tps->enabled;
    camera_third_person_distance = tps->distance;
    camera_third_person_height = tps->height;
    camera_third_person_side = tps->side;
    camera_third_person_look_ahead = tps->look_ahead;
    camera_third_person_interest_height = tps->interest_height;
    camera_third_person_fov_deg = tps->fov_deg;
}

static void MikuPan_UiSaveConfiguration(void)
{
    MikuPan_UiStoreRuntimeConfiguration();
    if (MikuPan_SaveConfiguration(NULL))
    {
        snprintf(config_save_status, sizeof(config_save_status),
                 "Saved to mikupan.ini");
    }
    else
    {
        snprintf(config_save_status, sizeof(config_save_status),
                 "Failed to save mikupan.ini");
    }
}

static void MikuPan_UiStoreRuntimeConfiguration(void)
{
    mikupan_configuration.renderer.render.width = render_resolution_width;
    mikupan_configuration.renderer.render.height = render_resolution_height;
    mikupan_configuration.renderer.window_mode = window_mode;
    is_fullscreen = (window_mode != MIKUPAN_WINDOW_WINDOWED);
    mikupan_configuration.renderer.is_fullscreen = is_fullscreen;
    mikupan_configuration.renderer.vsync = is_vsync;
    mikupan_configuration.renderer.lighting_mode = mesh_lighting_mode;
    mikupan_configuration.renderer.msaa_index = msaa_samples;
    mikupan_configuration.renderer.shadow_resolution =
        MikuPan_GetShadowResolution();
    mikupan_configuration.renderer.brightness = brightness;
    mikupan_configuration.renderer.gamma = gamma_value;
    mikupan_configuration.selected_theme =
        MikuPan_ClampThemeIndex(mikupan_configuration.selected_theme);
    mikupan_configuration.selected_font =
        MikuPan_ClampFontIndex(mikupan_configuration.selected_font);
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
    MikuPan_ClampThirdPersonCameraSettings(
        &mikupan_configuration.third_person_camera);
    mikupan_configuration.input.selected_gamepad_index =
        MikuPan_ControllerGetPreferredGamepadIndex();
    MikuPan_ControllerStoreBindingsToConfig();
}

// -- FrameTimeGraph ----------------------------------------------------------
#define FRAME_GRAPH_CAPACITY 600

typedef struct
{
    float times[FRAME_GRAPH_CAPACITY];///< total wall-clock per frame (ms)
    float cpu[FRAME_GRAPH_CAPACITY];  ///< CPU command-submission time (ms)
    float gpu
        [FRAME_GRAPH_CAPACITY];///< SDL_GPU idle/present wait after submit (ms)
    int count;
    int max_samples;
    float ms_scale;
} FrameTimeGraph;

static FrameTimeGraph g_frame_graph = {.count = 0,
                                       .max_samples = 600,
                                       .ms_scale = -1.0f};

static void FrameTimeGraph_Update(FrameTimeGraph* g, float total_ms,
                                  float cpu_ms, float gpu_ms)
{
    if (g->count >= g->max_samples)
    {
        memmove(g->times, g->times + 1, (g->count - 1) * sizeof(float));
        memmove(g->cpu, g->cpu + 1, (g->count - 1) * sizeof(float));
        memmove(g->gpu, g->gpu + 1, (g->count - 1) * sizeof(float));
        g->count--;
    }
    g->times[g->count] = total_ms;
    g->cpu[g->count] = cpu_ms;
    g->gpu[g->count] = gpu_ms;
    g->count++;
}

/* ── Condensed perf-table helpers ───────────────────────────────────────────
 * A "perf row" is label | milliseconds | share-of-total, laid out as a borderless
 * 3-column table so each section lines up tightly instead of sprawling down a
 * flat igText list. `total` is the denominator for the share column (CPU frame
 * time for the top-level buckets, or a parent bucket for sub-rows). */
static int PerfTableBegin(const char* id)
{
    if (!igBeginTable(id, 3,
                      ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerH
                          | ImGuiTableFlags_SizingFixedFit
                          | ImGuiTableFlags_NoHostExtendX,
                      (ImVec2) {0.0f, 0.0f}, 0.0f))
    {
        return 0;
    }
    igTableSetupColumn("Section", ImGuiTableColumnFlags_WidthStretch, 0.0f, 0);
    igTableSetupColumn("ms", ImGuiTableColumnFlags_WidthFixed, 0.0f, 0);
    igTableSetupColumn("share", ImGuiTableColumnFlags_WidthFixed, 0.0f, 0);
    return 1;
}

static void PerfRow(const char* label, float ms, float total)
{
    igTableNextRow(0, 0.0f);
    igTableSetColumnIndex(0);
    igTextUnformatted(label, NULL);
    igTableSetColumnIndex(1);
    igText("%6.2f", ms);
    igTableSetColumnIndex(2);
    if (total > 0.001f)
    {
        igText("%3.0f%%", 100.0f * ms / total);
    }
    else
    {
        igTextUnformatted("--", NULL);
    }
}

static void FrameTimeGraph_Draw(FrameTimeGraph* g)
{
    if (g->count == 0)
    {
        igTextUnformatted("No frame data yet", NULL);
        return;
    }

    igBegin("Frame Time Graph", NULL, perf_window_flags);

    float total_sum = 0.0f, total_max = g->times[0];
    float cpu_sum = 0.0f, cpu_max = g->cpu[0];
    float gpu_sum = 0.0f, gpu_max = g->gpu[0];

    for (int i = 0; i < g->count; i++)
    {
        total_sum += g->times[i];
        if (g->times[i] > total_max)
        {
            total_max = g->times[i];
        }
        cpu_sum += g->cpu[i];
        if (g->cpu[i] > cpu_max)
        {
            cpu_max = g->cpu[i];
        }
        gpu_sum += g->gpu[i];
        if (g->gpu[i] > gpu_max)
        {
            gpu_max = g->gpu[i];
        }
    }

    float total_avg = total_sum / (float) g->count;
    float cpu_avg = cpu_sum / (float) g->count;
    float gpu_avg = gpu_sum / (float) g->count;
    float total_latest = g->times[g->count - 1];
    float cpu_latest = g->cpu[g->count - 1];
    float gpu_latest = g->gpu[g->count - 1];

    // One-line headline: current frame, FPS, and where this frame waited.
    // The SDL_GPU value is a post-submit idle wait, not a hardware GPU timer.
    igText("%.2f ms   %.0f FPS", total_latest,
           total_latest > 0.0f ? 1000.0f / total_latest : 0.0f);
    igSameLine(0.0f, -1.0f);
    if (gpu_latest < cpu_latest * 0.25f)
    {
        igTextColored((ImVec4) {1.0f, 0.6f, 0.3f, 1.0f}, "   - CPU submit");
    }
    else if (gpu_latest > cpu_latest)
    {
        igTextColored((ImVec4) {0.4f, 0.7f, 1.0f, 1.0f},
                      "   - GPU/present wait");
    }
    else
    {
        igTextColored((ImVec4) {0.6f, 0.9f, 0.6f, 1.0f}, "   - balanced");
    }

    // Frame / CPU / GPU now / avg / max in an aligned 4-column table.
    if (igBeginTable("##perf_summary", 4,
                     ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerH
                         | ImGuiTableFlags_SizingFixedFit
                         | ImGuiTableFlags_NoHostExtendX,
                     (ImVec2) {0.0f, 0.0f}, 0.0f))
    {
        igTableSetupColumn("", ImGuiTableColumnFlags_WidthStretch, 0.0f, 0);
        igTableSetupColumn("now", ImGuiTableColumnFlags_WidthFixed, 0.0f, 0);
        igTableSetupColumn("avg", ImGuiTableColumnFlags_WidthFixed, 0.0f, 0);
        igTableSetupColumn("max", ImGuiTableColumnFlags_WidthFixed, 0.0f, 0);

#define MP_SUMMARY_ROW(name, now, avg, mx)                                     \
    igTableNextRow(0, 0.0f);                                                   \
    igTableSetColumnIndex(0);                                                  \
    igTextUnformatted(name, NULL);                                             \
    igTableSetColumnIndex(1);                                                  \
    igText("%6.2f", now);                                                      \
    igTableSetColumnIndex(2);                                                  \
    igText("%6.2f", avg);                                                      \
    igTableSetColumnIndex(3);                                                  \
    igText("%6.2f", mx)

        MP_SUMMARY_ROW("Frame", total_latest, total_avg, total_max);
        MP_SUMMARY_ROW("CPU", cpu_latest, cpu_avg, cpu_max);
        MP_SUMMARY_ROW("GPU wait", gpu_latest, gpu_avg, gpu_max);
#undef MP_SUMMARY_ROW
        igEndTable();
    }
    igTextDisabled("CPU = submit; GPU wait = SDL_WaitForGPUIdle after submit");
    if (MikuPan_IsVsync())
    {
        igTextDisabled(
            "VSync is on: wait can converge to the display interval.");
    }

    igSpacing();

    // Shared y-axis scale so the three plots are directly comparable.
    float scale = total_max;
    if (cpu_max > scale)
    {
        scale = cpu_max;
    }
    if (gpu_max > scale)
    {
        scale = gpu_max;
    }
    if (g->ms_scale > 0.0f)
    {
        scale = g->ms_scale;
    }
    if (scale < 16.7f)
    {
        scale = 16.7f;// always show at least the 60-fps line
    }

    char total_overlay[64], cpu_overlay[64], gpu_overlay[64];
    snprintf(total_overlay, sizeof(total_overlay), "Total: %.2f ms",
             total_latest);
    snprintf(cpu_overlay, sizeof(cpu_overlay), "CPU:   %.2f ms", cpu_latest);
    snprintf(gpu_overlay, sizeof(gpu_overlay), "GPU wait: %.2f ms", gpu_latest);

    const ImVec2 plot_size = (ImVec2) {0.0f, 60.0f};
    igPlotLines_FloatPtr("##frame_total", g->times, g->count, 0, total_overlay,
                         0.0f, scale, plot_size, (int) sizeof(float));
    igPlotLines_FloatPtr("##frame_cpu", g->cpu, g->count, 0, cpu_overlay, 0.0f,
                         scale, plot_size, (int) sizeof(float));
    igPlotLines_FloatPtr("##frame_gpu", g->gpu, g->count, 0, gpu_overlay, 0.0f,
                         scale, plot_size, (int) sizeof(float));

    igSpacing();
    igSeparatorText("CPU breakdown (last frame)");

    float perf_mesh = MikuPan_PerfGetSectionMs(MP_PERF_MESH_RENDER);
    float perf_sprite = MikuPan_PerfGetSectionMs(MP_PERF_SPRITE_RENDER);
    float perf_flush = MikuPan_PerfGetSectionMs(MP_PERF_BATCH_FLUSH);
    float perf_drawui = MikuPan_PerfGetSectionMs(MP_PERF_DRAWUI);
    float perf_render = MikuPan_PerfGetSectionMs(MP_PERF_RENDERUI);
    float perf_gs_up = MikuPan_GsGetUploadMs();
    float perf_gs_dl = MikuPan_GsGetDownloadMs();

    /// "Other" breakdown — uninstrumented PS2/SG scene-graph emulation, now
    /// partially attributed to leaf timers (these are also subtracted from the
    /// residual "Other / unknown" so the columns still sum to the CPU total).
    float perf_skin = MikuPan_PerfGetSectionMs(MP_PERF_SKIN_PREP);
    float perf_coord = MikuPan_PerfGetSectionMs(MP_PERF_COORD_CALC);
    float perf_light = MikuPan_PerfGetSectionMs(MP_PERF_LIGHT_SETUP);

    float perf_known = perf_mesh + perf_sprite + perf_flush + perf_drawui
                       + perf_render + perf_gs_up + perf_gs_dl + perf_skin
                       + perf_coord + perf_light;
    float perf_other = cpu_latest - perf_known;
    if (perf_other < 0.0f)
    {
        perf_other = 0.0f;
    }

    if (PerfTableBegin("##cpu_breakdown"))
    {
        PerfRow("Mesh render", perf_mesh, cpu_latest);
        PerfRow("Sprite / UI render", perf_sprite, cpu_latest);
        PerfRow("Batch flushes", perf_flush, cpu_latest);
        PerfRow("DrawUi (game)", perf_drawui, cpu_latest);
        PerfRow("RenderUi (ImGui)", perf_render, cpu_latest);
        PerfRow("GS uploads", perf_gs_up, cpu_latest);
        PerfRow("GS downloads", perf_gs_dl, cpu_latest);
        PerfRow("Skin prep (SetVUVN)", perf_skin, cpu_latest);
        PerfRow("Coord calc (SetLWS)", perf_coord, cpu_latest);
        PerfRow("Light setup", perf_light, cpu_latest);
        PerfRow("Other / unknown", perf_other, cpu_latest);
        igEndTable();
    }

    // Per-mesh-type breakdown (sub-sections of MESH_RENDER) — collapsed by
    // default. Sum ≈ Mesh render minus the per-renderer dispatch overhead; the
    // share column here is relative to the Mesh render total.
    if (igTreeNode_Str("Mesh types"))
    {
        if (PerfTableBegin("##mesh_types"))
        {
            PerfRow("0x2", MikuPan_PerfGetSectionMs(MP_PERF_MESH_0x2),
                    perf_mesh);
            PerfRow("0xA", MikuPan_PerfGetSectionMs(MP_PERF_MESH_0xA),
                    perf_mesh);
            PerfRow("0x10", MikuPan_PerfGetSectionMs(MP_PERF_MESH_0x10),
                    perf_mesh);
            PerfRow("0x12", MikuPan_PerfGetSectionMs(MP_PERF_MESH_0x12),
                    perf_mesh);
            PerfRow("0x32", MikuPan_PerfGetSectionMs(MP_PERF_MESH_0x32),
                    perf_mesh);
            PerfRow("0x82", MikuPan_PerfGetSectionMs(MP_PERF_MESH_0x82),
                    perf_mesh);
            igEndTable();
        }
        igTextDisabled("share is relative to Mesh render total");
        igTreePop();
    }

    // Cross-cutting slices — these overlap the buckets above (they are NOT
    // additive frame time), so they live in a collapsed tree. Descriptions are
    // folded into the row labels to keep each one to a single line. Indented
    // labels show the State-changes → per-call → per-texture-step nesting.
    if (igTreeNode_Str("Cross-cutting & state changes"))
    {
        float perf_draw = MikuPan_PerfGetSectionMs(MP_PERF_DRAW_SUBMIT);
        float perf_upload = MikuPan_PerfGetSectionMs(MP_PERF_BUFFER_UPLOAD);
        float perf_state = MikuPan_PerfGetSectionMs(MP_PERF_STATE_CHANGE);
        float perf_sc_shader = MikuPan_PerfGetSectionMs(MP_PERF_SC_SHADER);
        float perf_sc_texture = MikuPan_PerfGetSectionMs(MP_PERF_SC_TEXTURE);
        float perf_sc_rs3d = MikuPan_PerfGetSectionMs(MP_PERF_SC_RS3D);
        float perf_sc_vao = MikuPan_PerfGetSectionMs(MP_PERF_SC_VAO);
        float perf_tex_l1 = MikuPan_PerfGetSectionMs(MP_PERF_TEX_L1_LOOKUP);
        float perf_tex_hash = MikuPan_PerfGetSectionMs(MP_PERF_TEX_HASH);
        float perf_tex_l2 = MikuPan_PerfGetSectionMs(MP_PERF_TEX_L2_LOOKUP);
        float perf_tex_create = MikuPan_PerfGetSectionMs(MP_PERF_TEX_CREATE);
        float perf_tex_bind = MikuPan_PerfGetSectionMs(MP_PERF_TEX_BIND);
        int l1_hits = MikuPan_PerfGetTexL1Hits();
        int l1_misses = MikuPan_PerfGetTexL1Misses();

        if (PerfTableBegin("##cross_cutting"))
        {
            PerfRow("glDraw* submit (driver/call)", perf_draw, cpu_latest);
            PerfRow("Buffer uploads (map+memcpy)", perf_upload, cpu_latest);
            PerfRow("State changes (cached path)", perf_state, cpu_latest);
            PerfRow("    Shader (glUseProgram)", perf_sc_shader, cpu_latest);
            PerfRow("    Texture (lookup+bind)", perf_sc_texture, cpu_latest);
            PerfRow("        L1 lookup (tex0->info)", perf_tex_l1, cpu_latest);
            PerfRow("        Hash (XXH3/GS mem)", perf_tex_hash, cpu_latest);
            PerfRow("        L2 lookup (hash->info)", perf_tex_l2, cpu_latest);
            PerfRow("        Create (glTexImage+mip)", perf_tex_create,
                    cpu_latest);
            PerfRow("        Bind (glBindTexture)", perf_tex_bind, cpu_latest);
            PerfRow("    RenderState (depth/cull/blend)", perf_sc_rs3d,
                    cpu_latest);
            PerfRow("    VAO (glBindVertexArray)", perf_sc_vao, cpu_latest);
            igEndTable();
        }
        igText("Texture L1: %d hits / %d misses  (miss -> XXH3 over GS memory)",
               l1_hits, l1_misses);
        igTextDisabled("subsets of the buckets above — not additional time");
        igTreePop();
    }

    igSpacing();
    igSeparatorText("GS texture traffic (last frame)");

    int ul_count = MikuPan_GsGetUploadCount();
    int ul_bytes = MikuPan_GsGetUploadBytes();
    int dl_count = MikuPan_GsGetDownloadCount();
    int dl_bytes = MikuPan_GsGetDownloadBytes();

    igText("Up %d calls / %.1f KB     Down %d calls / %.1f KB", ul_count,
           ul_bytes / 1024.0f, dl_count, dl_bytes / 1024.0f);

    igEnd();
}

// -- Texture list helpers ----------------------------------------------------
#define TEXTURE_LIST_MAX 4096

static MikuPan_TextureInfo* tex_list[TEXTURE_LIST_MAX];
static int tex_list_count = 0;

static void CollectTexture(MikuPan_TextureInfo* tex, void* ud)
{
    int* count = (int*) ud;
    if (*count < TEXTURE_LIST_MAX)
    {
        tex_list[(*count)++] = tex;
    }
}

static int CompareTextureById(const void* a, const void* b)
{
    const MikuPan_TextureInfo* ta = *(const MikuPan_TextureInfo**) a;
    const MikuPan_TextureInfo* tb = *(const MikuPan_TextureInfo**) b;
    return (int) ta->id - (int) tb->id;
}

// -- Hot shader reload window ------------------------------------------------

void MikuPan_UiShaderReloadWindow(void)
{
    if (!show_shader_reload)
    {
        return;
    }

    igSetNextWindowSize((ImVec2) {560.0f, 480.0f}, ImGuiCond_FirstUseEver);
    if (!igBegin("Shader Reload", (bool*) &show_shader_reload, 0))
    {
        igEnd();
        return;
    }

    igTextDisabled(
        "Edits to .vert/.frag/.geom files take effect after Reload.");
    igTextDisabled(
        "Failed reloads keep the live program — no need to restart.");
    igSpacing();

    if (igButton("Reload All  (F5)", (ImVec2) {160.0f, 0.0f}))
    {
        last_reload_error[0] = '\0';
        MikuPan_ReloadAllShaders(last_reload_error,
                                 (int) sizeof(last_reload_error));
    }
    igSameLine(0.0f, -1.0f);
    igTextDisabled("9 programs");

    igSpacing();
    igSeparator();
    igSpacing();

    for (int i = 0; i < MAX_SHADER_PROGRAMS; i++)
    {
        igPushID_Int(i);

        if (igButton("Reload", (ImVec2) {80.0f, 0.0f}))
        {
            last_reload_error[0] = '\0';
            MikuPan_ReloadShader(i, last_reload_error,
                                 (int) sizeof(last_reload_error));
        }
        igSameLine(0.0f, -1.0f);
        igText("%-26s id=%u", MikuPan_GetShaderName(i),
               (unsigned) shader_list[i]);

        // Source paths underneath, indented — handy when there's any doubt
        // about which file an edit has to land in for a given shader.
        const char* vp = MikuPan_GetShaderVertSource(i);
        const char* gp = MikuPan_GetShaderGeomSource(i);
        const char* fp = MikuPan_GetShaderFragSource(i);
        if (vp)
        {
            igText("    vert: %s", vp);
        }
        if (gp)
        {
            igText("    geom: %s", gp);
        }
        if (fp)
        {
            igText("    frag: %s", fp);
        }

        igSpacing();
        igPopID();
    }

    if (last_reload_error[0] != '\0')
    {
        igSpacing();
        igSeparator();
        igTextColored((ImVec4) {1.0f, 0.4f, 0.4f, 1.0f}, "Last reload error:");
        igTextWrapped("%s", last_reload_error);
        if (igButton("Clear", (ImVec2) {0, 0}))
        {
            last_reload_error[0] = '\0';
        }
    }

    igEnd();
}

// -- Per-draw-call inspector -------------------------------------------------

static const char* GlPrimName(GLenum mode)
{
    switch (mode)
    {
        case GL_POINTS:
            return "POINTS";
        case GL_LINES:
            return "LINES";
        case GL_LINE_LOOP:
            return "LINE_LOOP";
        case GL_LINE_STRIP:
            return "LINE_STRIP";
        case GL_TRIANGLES:
            return "TRIANGLES";
        case GL_TRIANGLE_STRIP:
            return "TRIANGLE_STRIP";
        case GL_TRIANGLE_FAN:
            return "TRIANGLE_FAN";
        default:
            return "?";
    }
}

void MikuPan_UiDrawCallInspector(void)
{
    if (!show_draw_inspector)
    {
        // Auto-disable capture when the panel closes — no point paying the
        // capture cost (small but nonzero) when nothing is reading it.
        if (MikuPan_DrawCapEnabled())
        {
            MikuPan_DrawCapEnable(0);
        }
        return;
    }

    igSetNextWindowSize((ImVec2) {780.0f, 540.0f}, ImGuiCond_FirstUseEver);
    if (!igBegin("Draw Call Inspector", (bool*) &show_draw_inspector, 0))
    {
        igEnd();
        return;
    }

    int enabled = MikuPan_DrawCapEnabled();
    if (igCheckbox("Capture", (bool*) &enabled))
    {
        MikuPan_DrawCapEnable(enabled);
    }

    igSameLine(0.0f, -1.0f);
    int isolate = MikuPan_DrawCapGetIsolate();
    if (isolate >= 0)
    {
        igTextColored((ImVec4) {1.0f, 0.7f, 0.2f, 1.0f}, "  ISOLATING #%d",
                      isolate);
        igSameLine(0.0f, -1.0f);
        if (igButton("Show All", (ImVec2) {0, 0}))
        {
            MikuPan_DrawCapSetIsolate(-1);
        }
    }
    else
    {
        igTextDisabled("  (click 'Solo' on a row to render only that draw)");
    }

    int count = MikuPan_DrawCapLastCount();
    igText("Last frame: %d draw calls captured", count);

    igSpacing();
    igSeparator();

    // Header row + scrollable child so long captures stay manageable.
    igText("%-4s %-6s %-16s %-26s %-7s %-7s %-9s %s", "#", "skip?", "primitive",
           "shader", "vao", "tex0", "ms", "");

    igBeginChild_Str("##draw_list", (ImVec2) {0.0f, 0.0f}, 0, 0);

    for (int i = 0; i < count; i++)
    {
        const MikuPan_DrawCapEntry* e = MikuPan_DrawCapLastEntryAt(i);
        if (e == NULL)
        {
            break;
        }

        igPushID_Int(i);

        ImVec4 col = e->skipped ? (ImVec4) {0.6f, 0.6f, 0.6f, 1.0f}
                                : (ImVec4) {0.9f, 0.9f, 0.9f, 1.0f};

        igTextColored(col, "%-4d %-6s %-16s %-26s %-7u %-7u %7.3f  count=%d%s",
                      i, e->skipped ? "skip" : "draw", GlPrimName(e->mode),
                      e->shader_idx >= 0 ? MikuPan_GetShaderName(e->shader_idx)
                                         : "<external>",
                      (unsigned) e->vao, (unsigned) e->texture0, e->ms,
                      e->count, e->is_elements ? " (elements)" : "");

        igSameLine(0.0f, -1.0f);

        if (isolate == i)
        {
            if (igButton("Unsolo", (ImVec2) {70.0f, 0.0f}))
            {
                MikuPan_DrawCapSetIsolate(-1);
            }
        }
        else
        {
            if (igButton("Solo", (ImVec2) {70.0f, 0.0f}))
            {
                MikuPan_DrawCapSetIsolate(i);
            }
        }

        igPopID();
    }

    igEndChild();
    igEnd();
}

static const char* MikuPan_CameraDebugKindName(int kind)
{
    switch (kind)
    {
        case 0:
            return "Normal";
        case 1:
            return "Battle";
        case 2:
            return "Drama";
        case 3:
            return "Door";
        default:
            return "Unknown";
    }
}

static void MikuPan_CameraDebugVec3(const char* label, const sceVu0FVECTOR v,
                                    ImVec4 color)
{
    igTextColored(color, "%-15s %8.1f %8.1f %8.1f", label, v[0], v[1], v[2]);
}

static void MikuPan_UiCameraDebugWindow(void)
{
    if (!show_camera_debug)
    {
        return;
    }

    const CAMERA_DEBUG_PATH* path = CameraGetDebugPath();
    sceVu0FVECTOR view_vec = {camera.i[0] - camera.p[0],
                              camera.i[1] - camera.p[1],
                              camera.i[2] - camera.p[2], 0.0f};
    float view_dist =
        sqrtf(view_vec[0] * view_vec[0] + view_vec[1] * view_vec[1]
              + view_vec[2] * view_vec[2]);

    igSetNextWindowSize((ImVec2) {360.0f, 0.0f}, ImGuiCond_FirstUseEver);
    if (!igBegin("Camera World Info", NULL, ImGuiWindowFlags_AlwaysAutoResize))
    {
        igEnd();
        return;
    }

    if (path->active)
    {
        igText("Map camera: %s  no=%u old=%u  type=%u",
               MikuPan_CameraDebugKindName(path->kind), path->no, path->no_old,
               path->type);
    }
    else
    {
        igText("Map camera: runtime/free camera");
    }

    igText("Distance camera -> interest: %.1f", view_dist);
    igSeparator();

    MikuPan_CameraDebugVec3("camera.p", camera.p,
                            (ImVec4) {1.0f, 0.78f, 0.10f, 1.0f});
    MikuPan_CameraDebugVec3("camera.i", camera.i,
                            (ImVec4) {0.15f, 1.0f, 0.35f, 1.0f});
    MikuPan_CameraDebugVec3("view line", view_vec,
                            (ImVec4) {1.0f, 1.0f, 1.0f, 1.0f});

    if (path->active)
    {
        igSeparator();
        igTextColored((ImVec4) {1.0f, 0.42f, 0.08f, 1.0f},
                      "authored camera path points: %u",
                      path->camera_path_points);
        igTextColored((ImVec4) {0.05f, 0.80f, 1.0f, 1.0f},
                      "authored interest path points: %u",
                      path->interest_path_points);

        if (path->change || path->no != path->no_old)
        {
            igTextColored((ImVec4) {1.0f, 0.10f, 0.30f, 1.0f},
                          "transition target active");
        }
    }

    igEnd();
}

// -- Public API --------------------------------------------------------------

static int CompareResolutionDesc(const void* a, const void* b)
{
    const MikuPan_Resolution* ra = (const MikuPan_Resolution*) a;
    const MikuPan_Resolution* rb = (const MikuPan_Resolution*) b;
    int area_a = ra->width * ra->height;
    int area_b = rb->width * rb->height;
    return area_b - area_a;
}

/* Write a reduced aspect-ratio string (e.g. "16:9") into buf. */
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

    qsort(resolution_list, resolution_count, sizeof(MikuPan_Resolution),
          CompareResolutionDesc);

    for (int i = 0; i < resolution_count; i++)
    {
        char aspect[12];
        int ps2_scale = MikuPan_GetPs2ResolutionScale(
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

/* GPU backend dropdown (Settings > Display > Graphics). Writes the choice
 * straight into the configuration so Save Configuration persists it; the
 * device cannot be swapped while running, so the new backend is only used on
 * the next launch. */
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
                                  (ImVec2) {0, 0}))
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

static void MikuPan_ApplyUiFont(int font)
{
    ImGuiIO* io = igGetIO_Nil();
    font = MikuPan_ClampFontIndex(font);

    if (ui_fonts[font] != NULL)
    {
        io->FontDefault = ui_fonts[font];
    }
}

static float MikuPan_ClampFontScale(float scale)
{
    if (scale < 0.5f)
    {
        return 0.5f;
    }
    if (scale > 3.0f)
    {
        return 3.0f;
    }
    return scale;
}

/* Apply the user font-size multiplier to the whole UI. FontScaleMain is the
 * ImGui 1.92 dynamic global font scale (in ImGuiStyle), so this stays crisp at
 * any size. */
static void MikuPan_ApplyUiFontScale(void)
{
    ImGuiStyle* style = igGetStyle();
    mikupan_configuration.font_scale =
        MikuPan_ClampFontScale(mikupan_configuration.font_scale);
    style->FontScaleMain = mikupan_configuration.font_scale;
}

static void MikuPan_LoadUiFonts(void)
{
    ImGuiIO* io = igGetIO_Nil();
    ImFontConfig* default_font_config = ImFontConfig_ImFontConfig();

    ui_font_default_size = 13.0f * ui_display_scale;
    ui_font_regular_size = 14.0f * ui_display_scale;

    if (default_font_config != NULL)
    {
        default_font_config->SizePixels = ui_font_default_size;
    }

    ui_fonts[0] = ImFontAtlas_AddFontDefault(io->Fonts, default_font_config);

    if (default_font_config != NULL)
    {
        ImFontConfig_destroy(default_font_config);
    }

    ui_fonts[1] = ImFontAtlas_AddFontFromFileTTF(
        io->Fonts, "./resources/fonts/CenturyOldStyle.ttf",
        ui_font_regular_size, NULL,
        ImFontAtlas_GetGlyphRangesDefault(io->Fonts));

    if (ui_fonts[1] == NULL)
    {
        ui_fonts[1] = ui_fonts[0];
    }

    mikupan_configuration.selected_font =
        MikuPan_ClampFontIndex(mikupan_configuration.selected_font);
    MikuPan_ApplyUiFont(mikupan_configuration.selected_font);
}

static void MikuPan_ApplyThemeBaseline(ImVec4* c)
{
    c[ImGuiCol_Separator] = (ImVec4) {0.22f, 0.24f, 0.28f, 0.55f};
    c[ImGuiCol_SeparatorHovered] = (ImVec4) {0.40f, 0.45f, 0.52f, 0.85f};
    c[ImGuiCol_SeparatorActive] = (ImVec4) {0.58f, 0.66f, 0.76f, 1.00f};
    c[ImGuiCol_ResizeGrip] = (ImVec4) {0.24f, 0.28f, 0.34f, 0.25f};
    c[ImGuiCol_ResizeGripHovered] = (ImVec4) {0.42f, 0.50f, 0.60f, 0.70f};
    c[ImGuiCol_ResizeGripActive] = (ImVec4) {0.58f, 0.68f, 0.80f, 0.95f};
    c[ImGuiCol_TabDimmed] = (ImVec4) {0.06f, 0.08f, 0.11f, 0.95f};
    c[ImGuiCol_TabDimmedSelected] = (ImVec4) {0.14f, 0.18f, 0.24f, 1.00f};
    c[ImGuiCol_TabDimmedSelectedOverline] =
        (ImVec4) {0.36f, 0.48f, 0.62f, 1.00f};
    c[ImGuiCol_PlotLines] = (ImVec4) {0.58f, 0.70f, 0.84f, 1.00f};
    c[ImGuiCol_PlotLinesHovered] = (ImVec4) {0.76f, 0.86f, 0.98f, 1.00f};
    c[ImGuiCol_PlotHistogram] = (ImVec4) {0.44f, 0.54f, 0.68f, 1.00f};
    c[ImGuiCol_PlotHistogramHovered] = (ImVec4) {0.64f, 0.76f, 0.92f, 1.00f};
    c[ImGuiCol_TableRowBg] = (ImVec4) {0.00f, 0.00f, 0.00f, 0.00f};
    c[ImGuiCol_TableRowBgAlt] = (ImVec4) {0.12f, 0.16f, 0.22f, 0.25f};
    c[ImGuiCol_DragDropTarget] = (ImVec4) {0.76f, 0.86f, 0.98f, 0.90f};
    c[ImGuiCol_NavCursor] = (ImVec4) {0.58f, 0.70f, 0.84f, 1.00f};
    c[ImGuiCol_NavWindowingHighlight] = (ImVec4) {0.76f, 0.86f, 0.98f, 0.70f};
    c[ImGuiCol_NavWindowingDimBg] = (ImVec4) {0.03f, 0.04f, 0.06f, 0.55f};
    c[ImGuiCol_ModalWindowDimBg] = (ImVec4) {0.02f, 0.03f, 0.05f, 0.72f};
}

static void MikuPan_ApplyFatalFrameStyle(int theme)
{
    theme = MikuPan_ClampThemeIndex(theme);

    ImGuiStyle* s = igGetStyle();
    ImVec4* c = s->Colors;

    s->Alpha = 0.96f;
    s->DisabledAlpha = 0.45f;
    s->WindowRounding = 0.0f;
    s->ChildRounding = 0.0f;
    s->PopupRounding = 0.0f;
    s->FrameRounding = 0.0f;
    s->ScrollbarRounding = 0.0f;
    s->GrabRounding = 0.0f;
    s->TabRounding = 0.0f;
    s->WindowBorderSize = 1.0f;
    s->ChildBorderSize = 1.0f;
    s->PopupBorderSize = 1.0f;
    s->FrameBorderSize = 1.0f;
    s->TabBorderSize = 1.0f;
    s->WindowPadding = (ImVec2) {10.0f, 10.0f};
    s->FramePadding = (ImVec2) {8.0f, 4.0f};
    s->ItemSpacing = (ImVec2) {8.0f, 5.0f};
    s->ItemInnerSpacing = (ImVec2) {6.0f, 4.0f};
    s->IndentSpacing = 18.0f;
    s->ScrollbarSize = 14.0f;
    s->GrabMinSize = 10.0f;

    if (theme != 2)
    {
        MikuPan_ApplyThemeBaseline(c);
    }

    switch (theme)
    {
        default:
        case 0:
            // Text: pale paper with a hint of purple-blue
            c[ImGuiCol_Text] = (ImVec4) {0.78f, 0.82f, 0.88f, 1.00f};
            c[ImGuiCol_TextDisabled] = (ImVec4) {0.32f, 0.36f, 0.42f, 1.00f};

            // Background: deep blue-black like moonlit mansion
            c[ImGuiCol_WindowBg] = (ImVec4) {0.04f, 0.06f, 0.10f, 0.96f};
            c[ImGuiCol_ChildBg] = (ImVec4) {0.06f, 0.08f, 0.12f, 0.85f};
            c[ImGuiCol_PopupBg] = (ImVec4) {0.03f, 0.05f, 0.08f, 0.98f};

            // Borders: slate with a cold purple lean
            c[ImGuiCol_Border] = (ImVec4) {0.24f, 0.28f, 0.35f, 0.65f};
            c[ImGuiCol_BorderShadow] = (ImVec4) {0, 0, 0, 0};

            // Frames: cool gray-blue
            c[ImGuiCol_FrameBg] = (ImVec4) {0.09f, 0.12f, 0.16f, 0.95f};
            c[ImGuiCol_FrameBgHovered] = (ImVec4) {0.16f, 0.21f, 0.28f, 0.95f};
            c[ImGuiCol_FrameBgActive] = (ImVec4) {0.23f, 0.30f, 0.40f, 1.00f};

            // Title bars: dark with cold highlights
            c[ImGuiCol_TitleBg] = (ImVec4) {0.05f, 0.07f, 0.10f, 1.00f};
            c[ImGuiCol_TitleBgActive] = (ImVec4) {0.10f, 0.14f, 0.18f, 1.00f};
            c[ImGuiCol_TitleBgCollapsed] =
                (ImVec4) {0.03f, 0.05f, 0.08f, 0.85f};

            c[ImGuiCol_MenuBarBg] = (ImVec4) {0.07f, 0.09f, 0.12f, 1.00f};

            // Scrollbars: soft azure
            c[ImGuiCol_ScrollbarBg] = (ImVec4) {0.03f, 0.04f, 0.06f, 0.85f};
            c[ImGuiCol_ScrollbarGrab] = (ImVec4) {0.18f, 0.24f, 0.30f, 1.00f};
            c[ImGuiCol_ScrollbarGrabHovered] =
                (ImVec4) {0.26f, 0.34f, 0.42f, 1.00f};
            c[ImGuiCol_ScrollbarGrabActive] =
                (ImVec4) {0.34f, 0.44f, 0.54f, 1.00f};

            // Accent: ghost-light cyan / pale purple
            c[ImGuiCol_CheckMark] = (ImVec4) {0.60f, 0.78f, 0.96f, 1.00f};
            c[ImGuiCol_SliderGrab] = (ImVec4) {0.42f, 0.64f, 0.88f, 1.00f};
            c[ImGuiCol_SliderGrabActive] =
                (ImVec4) {0.60f, 0.84f, 0.98f, 1.00f};

            // Buttons: twilight blue-gray
            c[ImGuiCol_Button] = (ImVec4) {0.08f, 0.10f, 0.14f, 0.95f};
            c[ImGuiCol_ButtonHovered] = (ImVec4) {0.18f, 0.24f, 0.32f, 1.00f};
            c[ImGuiCol_ButtonActive] = (ImVec4) {0.28f, 0.36f, 0.48f, 1.00f};

            // Headers
            c[ImGuiCol_Header] = (ImVec4) {0.12f, 0.16f, 0.22f, 0.85f};
            c[ImGuiCol_HeaderHovered] = (ImVec4) {0.20f, 0.28f, 0.38f, 0.95f};
            c[ImGuiCol_HeaderActive] = (ImVec4) {0.28f, 0.38f, 0.52f, 1.00f};

            // Tabs
            c[ImGuiCol_Tab] = (ImVec4) {0.08f, 0.10f, 0.14f, 0.95f};
            c[ImGuiCol_TabHovered] = (ImVec4) {0.20f, 0.28f, 0.38f, 0.95f};
            c[ImGuiCol_TabSelected] = (ImVec4) {0.26f, 0.34f, 0.46f, 1.00f};
            c[ImGuiCol_TabSelectedOverline] =
                (ImVec4) {0.60f, 0.78f, 0.96f, 1.00f};

            // Misc accents (plots, nav, tables, etc.)
            c[ImGuiCol_TextLink] = (ImVec4) {0.60f, 0.78f, 0.96f, 1.00f};
            c[ImGuiCol_TextSelectedBg] = (ImVec4) {0.25f, 0.32f, 0.42f, 0.65f};
            c[ImGuiCol_TableHeaderBg] = (ImVec4) {0.08f, 0.12f, 0.16f, 1.00f};
            c[ImGuiCol_TableBorderStrong] =
                (ImVec4) {0.22f, 0.28f, 0.34f, 1.00f};
            c[ImGuiCol_TableBorderLight] =
                (ImVec4) {0.14f, 0.18f, 0.24f, 1.00f};
            break;
        case 1:
            // Text: pale paper, slightly cold
            c[ImGuiCol_Text] = (ImVec4) {0.82f, 0.86f, 0.88f, 1.00f};
            c[ImGuiCol_TextDisabled] = (ImVec4) {0.38f, 0.42f, 0.45f, 1.00f};

            // Backgrounds: blue-black ink
            c[ImGuiCol_WindowBg] = (ImVec4) {0.05f, 0.07f, 0.09f, 0.96f};
            c[ImGuiCol_ChildBg] = (ImVec4) {0.07f, 0.09f, 0.11f, 0.85f};
            c[ImGuiCol_PopupBg] = (ImVec4) {0.04f, 0.06f, 0.08f, 0.98f};

            // Borders: cold slate
            c[ImGuiCol_Border] = (ImVec4) {0.22f, 0.28f, 0.32f, 0.65f};
            c[ImGuiCol_BorderShadow] = (ImVec4) {0, 0, 0, 0};

            // Frames: dusty wood in moonlight
            c[ImGuiCol_FrameBg] = (ImVec4) {0.10f, 0.13f, 0.16f, 0.95f};
            c[ImGuiCol_FrameBgHovered] = (ImVec4) {0.18f, 0.24f, 0.28f, 0.95f};
            c[ImGuiCol_FrameBgActive] = (ImVec4) {0.26f, 0.34f, 0.40f, 1.00f};

            // Titles: very dark blue-black
            c[ImGuiCol_TitleBg] = (ImVec4) {0.06f, 0.08f, 0.10f, 1.00f};
            c[ImGuiCol_TitleBgActive] = (ImVec4) {0.12f, 0.18f, 0.22f, 1.00f};
            c[ImGuiCol_TitleBgCollapsed] =
                (ImVec4) {0.05f, 0.07f, 0.09f, 0.85f};

            c[ImGuiCol_MenuBarBg] = (ImVec4) {0.08f, 0.10f, 0.12f, 1.00f};

            // Scrollbar: faint blue steel
            c[ImGuiCol_ScrollbarBg] = (ImVec4) {0.04f, 0.05f, 0.07f, 0.85f};
            c[ImGuiCol_ScrollbarGrab] = (ImVec4) {0.22f, 0.30f, 0.36f, 1.00f};
            c[ImGuiCol_ScrollbarGrabHovered] =
                (ImVec4) {0.30f, 0.40f, 0.46f, 1.00f};
            c[ImGuiCol_ScrollbarGrabActive] =
                (ImVec4) {0.38f, 0.50f, 0.58f, 1.00f};

            // Accent color: ghostly cyan (FF1 signature)
            c[ImGuiCol_CheckMark] = (ImVec4) {0.60f, 0.85f, 0.90f, 1.00f};
            c[ImGuiCol_SliderGrab] = (ImVec4) {0.50f, 0.78f, 0.84f, 1.00f};
            c[ImGuiCol_SliderGrabActive] =
                (ImVec4) {0.70f, 0.92f, 0.98f, 1.00f};

            // Buttons: foggy slate
            c[ImGuiCol_Button] = (ImVec4) {0.12f, 0.16f, 0.20f, 0.95f};
            c[ImGuiCol_ButtonHovered] = (ImVec4) {0.22f, 0.30f, 0.36f, 1.00f};
            c[ImGuiCol_ButtonActive] = (ImVec4) {0.32f, 0.42f, 0.50f, 1.00f};

            // Headers
            c[ImGuiCol_Header] = (ImVec4) {0.14f, 0.20f, 0.24f, 0.85f};
            c[ImGuiCol_HeaderHovered] = (ImVec4) {0.24f, 0.34f, 0.40f, 0.95f};
            c[ImGuiCol_HeaderActive] = (ImVec4) {0.34f, 0.46f, 0.54f, 1.00f};

            // Separators
            c[ImGuiCol_Separator] = (ImVec4) {0.22f, 0.28f, 0.32f, 0.55f};
            c[ImGuiCol_SeparatorHovered] =
                (ImVec4) {0.40f, 0.52f, 0.60f, 0.85f};
            c[ImGuiCol_SeparatorActive] = (ImVec4) {0.55f, 0.70f, 0.80f, 1.00f};

            // Tabs
            c[ImGuiCol_Tab] = (ImVec4) {0.10f, 0.14f, 0.18f, 0.95f};
            c[ImGuiCol_TabHovered] = (ImVec4) {0.24f, 0.34f, 0.40f, 0.95f};
            c[ImGuiCol_TabSelected] = (ImVec4) {0.30f, 0.42f, 0.50f, 1.00f};
            c[ImGuiCol_TabSelectedOverline] =
                (ImVec4) {0.65f, 0.90f, 0.95f, 1.00f};

            // Tables / misc accents
            c[ImGuiCol_TableHeaderBg] = (ImVec4) {0.12f, 0.18f, 0.22f, 1.00f};
            c[ImGuiCol_TableBorderStrong] =
                (ImVec4) {0.22f, 0.30f, 0.36f, 1.00f};
            c[ImGuiCol_TableBorderLight] =
                (ImVec4) {0.16f, 0.22f, 0.26f, 1.00f};
            c[ImGuiCol_TextLink] = (ImVec4) {0.65f, 0.90f, 0.95f, 1.00f};
            c[ImGuiCol_TextSelectedBg] = (ImVec4) {0.30f, 0.42f, 0.50f, 0.65f};

            // Nav / modal
            c[ImGuiCol_NavCursor] = (ImVec4) {0.65f, 0.90f, 0.95f, 1.00f};
            c[ImGuiCol_NavWindowingHighlight] =
                (ImVec4) {0.65f, 0.90f, 0.95f, 0.70f};
            c[ImGuiCol_NavWindowingDimBg] =
                (ImVec4) {0.04f, 0.05f, 0.07f, 0.50f};
            c[ImGuiCol_ModalWindowDimBg] =
                (ImVec4) {0.04f, 0.05f, 0.07f, 0.65f};
            break;
        case 2:
            c[ImGuiCol_Text] = (ImVec4) {0.86f, 0.78f, 0.62f, 1.00f};
            c[ImGuiCol_TextDisabled] = (ImVec4) {0.42f, 0.36f, 0.28f, 1.00f};
            c[ImGuiCol_WindowBg] = (ImVec4) {0.06f, 0.04f, 0.03f, 0.96f};
            c[ImGuiCol_ChildBg] = (ImVec4) {0.08f, 0.05f, 0.04f, 0.85f};
            c[ImGuiCol_PopupBg] = (ImVec4) {0.05f, 0.03f, 0.02f, 0.98f};
            c[ImGuiCol_Border] = (ImVec4) {0.40f, 0.10f, 0.08f, 0.65f};
            c[ImGuiCol_BorderShadow] = (ImVec4) {0.00f, 0.00f, 0.00f, 0.00f};
            c[ImGuiCol_FrameBg] = (ImVec4) {0.12f, 0.07f, 0.05f, 0.95f};
            c[ImGuiCol_FrameBgHovered] = (ImVec4) {0.32f, 0.10f, 0.08f, 0.85f};
            c[ImGuiCol_FrameBgActive] = (ImVec4) {0.52f, 0.12f, 0.10f, 0.95f};
            c[ImGuiCol_TitleBg] = (ImVec4) {0.08f, 0.04f, 0.03f, 1.00f};
            c[ImGuiCol_TitleBgActive] = (ImVec4) {0.35f, 0.05f, 0.05f, 1.00f};
            c[ImGuiCol_TitleBgCollapsed] =
                (ImVec4) {0.05f, 0.02f, 0.02f, 0.85f};
            c[ImGuiCol_MenuBarBg] = (ImVec4) {0.10f, 0.05f, 0.04f, 1.00f};
            c[ImGuiCol_ScrollbarBg] = (ImVec4) {0.04f, 0.02f, 0.02f, 0.85f};
            c[ImGuiCol_ScrollbarGrab] = (ImVec4) {0.28f, 0.16f, 0.10f, 1.00f};
            c[ImGuiCol_ScrollbarGrabHovered] =
                (ImVec4) {0.50f, 0.20f, 0.12f, 1.00f};
            c[ImGuiCol_ScrollbarGrabActive] =
                (ImVec4) {0.65f, 0.22f, 0.14f, 1.00f};
            c[ImGuiCol_CheckMark] = (ImVec4) {0.85f, 0.40f, 0.18f, 1.00f};
            c[ImGuiCol_SliderGrab] = (ImVec4) {0.60f, 0.20f, 0.14f, 1.00f};
            c[ImGuiCol_SliderGrabActive] =
                (ImVec4) {0.80f, 0.30f, 0.16f, 1.00f};
            c[ImGuiCol_Button] = (ImVec4) {0.18f, 0.08f, 0.06f, 0.95f};
            c[ImGuiCol_ButtonHovered] = (ImVec4) {0.45f, 0.12f, 0.10f, 1.00f};
            c[ImGuiCol_ButtonActive] = (ImVec4) {0.65f, 0.16f, 0.12f, 1.00f};
            c[ImGuiCol_Header] = (ImVec4) {0.22f, 0.08f, 0.06f, 0.85f};
            c[ImGuiCol_HeaderHovered] = (ImVec4) {0.45f, 0.12f, 0.10f, 0.95f};
            c[ImGuiCol_HeaderActive] = (ImVec4) {0.60f, 0.14f, 0.10f, 1.00f};
            c[ImGuiCol_Separator] = (ImVec4) {0.40f, 0.12f, 0.10f, 0.55f};
            c[ImGuiCol_SeparatorHovered] =
                (ImVec4) {0.65f, 0.18f, 0.12f, 0.85f};
            c[ImGuiCol_SeparatorActive] = (ImVec4) {0.85f, 0.24f, 0.14f, 1.00f};
            c[ImGuiCol_ResizeGrip] = (ImVec4) {0.28f, 0.12f, 0.08f, 0.50f};
            c[ImGuiCol_ResizeGripHovered] =
                (ImVec4) {0.55f, 0.16f, 0.10f, 0.85f};
            c[ImGuiCol_ResizeGripActive] =
                (ImVec4) {0.80f, 0.22f, 0.14f, 1.00f};
            c[ImGuiCol_Tab] = (ImVec4) {0.14f, 0.06f, 0.04f, 0.95f};
            c[ImGuiCol_TabHovered] = (ImVec4) {0.45f, 0.12f, 0.10f, 0.95f};
            c[ImGuiCol_TabSelected] = (ImVec4) {0.55f, 0.14f, 0.10f, 1.00f};
            c[ImGuiCol_TabSelectedOverline] =
                (ImVec4) {0.85f, 0.25f, 0.14f, 1.00f};
            c[ImGuiCol_TabDimmed] = (ImVec4) {0.10f, 0.05f, 0.03f, 0.95f};
            c[ImGuiCol_TabDimmedSelected] =
                (ImVec4) {0.30f, 0.08f, 0.06f, 1.00f};
            c[ImGuiCol_TabDimmedSelectedOverline] =
                (ImVec4) {0.55f, 0.14f, 0.10f, 1.00f};
            c[ImGuiCol_PlotLines] = (ImVec4) {0.85f, 0.50f, 0.18f, 1.00f};
            c[ImGuiCol_PlotLinesHovered] =
                (ImVec4) {1.00f, 0.55f, 0.20f, 1.00f};
            c[ImGuiCol_PlotHistogram] = (ImVec4) {0.65f, 0.20f, 0.14f, 1.00f};
            c[ImGuiCol_PlotHistogramHovered] =
                (ImVec4) {0.90f, 0.30f, 0.18f, 1.00f};
            c[ImGuiCol_TableHeaderBg] = (ImVec4) {0.18f, 0.08f, 0.06f, 1.00f};
            c[ImGuiCol_TableBorderStrong] =
                (ImVec4) {0.40f, 0.12f, 0.10f, 1.00f};
            c[ImGuiCol_TableBorderLight] =
                (ImVec4) {0.25f, 0.10f, 0.08f, 1.00f};
            c[ImGuiCol_TableRowBg] = (ImVec4) {0.00f, 0.00f, 0.00f, 0.00f};
            c[ImGuiCol_TableRowBgAlt] = (ImVec4) {0.95f, 0.50f, 0.18f, 0.04f};
            c[ImGuiCol_TextLink] = (ImVec4) {0.95f, 0.50f, 0.18f, 1.00f};
            c[ImGuiCol_TextSelectedBg] = (ImVec4) {0.55f, 0.14f, 0.10f, 0.65f};
            c[ImGuiCol_DragDropTarget] = (ImVec4) {0.95f, 0.50f, 0.18f, 0.90f};
            c[ImGuiCol_NavCursor] = (ImVec4) {0.85f, 0.25f, 0.14f, 1.00f};
            c[ImGuiCol_NavWindowingHighlight] =
                (ImVec4) {0.95f, 0.50f, 0.18f, 0.70f};
            c[ImGuiCol_NavWindowingDimBg] =
                (ImVec4) {0.04f, 0.02f, 0.02f, 0.50f};
            c[ImGuiCol_ModalWindowDimBg] =
                (ImVec4) {0.04f, 0.02f, 0.02f, 0.65f};
            break;

        case 3:
            /*
                Fatal Frame 1 PS2 inspired theme.

                Palette direction:
                - blackened wood / tatami shadows
                - dried blood red-browns
                - faded paper text
                - restrained bruised violet accents
                - sharp, square PS2-era UI shapes
            */
            // Text: faded paper / old photograph highlight
            c[ImGuiCol_Text] = (ImVec4) {0.86f, 0.80f, 0.70f, 1.00f};
            c[ImGuiCol_TextDisabled] = (ImVec4) {0.42f, 0.35f, 0.34f, 1.00f};

            // Backgrounds: blackened tatami / dark old wood
            c[ImGuiCol_WindowBg] = (ImVec4) {0.055f, 0.035f, 0.040f, 0.96f};
            c[ImGuiCol_ChildBg] = (ImVec4) {0.085f, 0.055f, 0.055f, 0.86f};
            c[ImGuiCol_PopupBg] = (ImVec4) {0.045f, 0.030f, 0.035f, 0.98f};

            // Borders: dusty red-brown lacquer
            c[ImGuiCol_Border] = (ImVec4) {0.31f, 0.19f, 0.18f, 0.68f};
            c[ImGuiCol_BorderShadow] = (ImVec4) {0.00f, 0.00f, 0.00f, 0.00f};

            // Frames: aged wood / stained cloth
            c[ImGuiCol_FrameBg] = (ImVec4) {0.13f, 0.075f, 0.080f, 0.95f};
            c[ImGuiCol_FrameBgHovered] = (ImVec4) {0.24f, 0.12f, 0.15f, 0.96f};
            c[ImGuiCol_FrameBgActive] = (ImVec4) {0.36f, 0.16f, 0.22f, 1.00f};

            // Titles: deep cover-shadow red-black
            c[ImGuiCol_TitleBg] = (ImVec4) {0.07f, 0.035f, 0.040f, 1.00f};
            c[ImGuiCol_TitleBgActive] = (ImVec4) {0.20f, 0.075f, 0.110f, 1.00f};
            c[ImGuiCol_TitleBgCollapsed] =
                (ImVec4) {0.055f, 0.030f, 0.035f, 0.86f};

            c[ImGuiCol_MenuBarBg] = (ImVec4) {0.10f, 0.060f, 0.060f, 1.00f};

            // Scrollbar: old cabinet / dark rust
            c[ImGuiCol_ScrollbarBg] = (ImVec4) {0.045f, 0.030f, 0.035f, 0.86f};
            c[ImGuiCol_ScrollbarGrab] = (ImVec4) {0.28f, 0.14f, 0.16f, 1.00f};
            c[ImGuiCol_ScrollbarGrabHovered] =
                (ImVec4) {0.40f, 0.18f, 0.23f, 1.00f};
            c[ImGuiCol_ScrollbarGrabActive] =
                (ImVec4) {0.54f, 0.22f, 0.31f, 1.00f};

            // Accent color: muted ritual violet / bruised crimson-purple
            c[ImGuiCol_CheckMark] = (ImVec4) {0.58f, 0.34f, 0.62f, 1.00f};
            c[ImGuiCol_SliderGrab] = (ImVec4) {0.42f, 0.23f, 0.48f, 1.00f};
            c[ImGuiCol_SliderGrabActive] =
                (ImVec4) {0.68f, 0.42f, 0.72f, 1.00f};

            // Buttons: muted blood-stained wood
            c[ImGuiCol_Button] = (ImVec4) {0.16f, 0.075f, 0.085f, 0.95f};
            c[ImGuiCol_ButtonHovered] = (ImVec4) {0.32f, 0.13f, 0.18f, 1.00f};
            c[ImGuiCol_ButtonActive] = (ImVec4) {0.40f, 0.15f, 0.22f, 1.00f};

            // Headers: carpet maroon / dried blood
            c[ImGuiCol_Header] = (ImVec4) {0.20f, 0.085f, 0.105f, 0.86f};
            c[ImGuiCol_HeaderHovered] = (ImVec4) {0.38f, 0.14f, 0.20f, 0.96f};
            c[ImGuiCol_HeaderActive] = (ImVec4) {0.42f, 0.15f, 0.24f, 1.00f};

            // Separators: worn red-brown edges with violet interaction highlight
            c[ImGuiCol_Separator] = (ImVec4) {0.31f, 0.19f, 0.18f, 0.58f};
            c[ImGuiCol_SeparatorHovered] =
                (ImVec4) {0.48f, 0.26f, 0.46f, 0.85f};
            c[ImGuiCol_SeparatorActive] = (ImVec4) {0.62f, 0.36f, 0.66f, 1.00f};

            // Resize grips: dim until interacted with
            c[ImGuiCol_ResizeGrip] = (ImVec4) {0.31f, 0.19f, 0.18f, 0.25f};
            c[ImGuiCol_ResizeGripHovered] =
                (ImVec4) {0.48f, 0.26f, 0.46f, 0.70f};
            c[ImGuiCol_ResizeGripActive] =
                (ImVec4) {0.62f, 0.36f, 0.66f, 0.95f};

            // Tabs: dark red-brown, selected with restrained shrine-violet
            c[ImGuiCol_Tab] = (ImVec4) {0.13f, 0.065f, 0.075f, 0.95f};
            c[ImGuiCol_TabHovered] = (ImVec4) {0.38f, 0.14f, 0.20f, 0.96f};
            c[ImGuiCol_TabSelected] = (ImVec4) {0.34f, 0.13f, 0.22f, 1.00f};
            c[ImGuiCol_TabSelectedOverline] =
                (ImVec4) {0.62f, 0.36f, 0.66f, 1.00f};
            c[ImGuiCol_TabDimmed] = (ImVec4) {0.09f, 0.045f, 0.050f, 0.95f};
            c[ImGuiCol_TabDimmedSelected] =
                (ImVec4) {0.18f, 0.075f, 0.105f, 1.00f};
            c[ImGuiCol_TabDimmedSelectedOverline] =
                (ImVec4) {0.38f, 0.22f, 0.42f, 1.00f};

            // Plot colors: subdued, readable, not too modern-looking
            c[ImGuiCol_PlotLines] = (ImVec4) {0.66f, 0.44f, 0.68f, 1.00f};
            c[ImGuiCol_PlotLinesHovered] =
                (ImVec4) {0.80f, 0.58f, 0.82f, 1.00f};
            c[ImGuiCol_PlotHistogram] = (ImVec4) {0.50f, 0.22f, 0.28f, 1.00f};
            c[ImGuiCol_PlotHistogramHovered] =
                (ImVec4) {0.68f, 0.30f, 0.40f, 1.00f};

            // Tables / misc accents
            c[ImGuiCol_TableHeaderBg] = (ImVec4) {0.16f, 0.075f, 0.090f, 1.00f};
            c[ImGuiCol_TableBorderStrong] =
                (ImVec4) {0.34f, 0.18f, 0.19f, 1.00f};
            c[ImGuiCol_TableBorderLight] =
                (ImVec4) {0.23f, 0.12f, 0.13f, 1.00f};
            c[ImGuiCol_TableRowBg] = (ImVec4) {0.00f, 0.00f, 0.00f, 0.00f};
            c[ImGuiCol_TableRowBgAlt] = (ImVec4) {0.12f, 0.065f, 0.070f, 0.32f};

            c[ImGuiCol_TextLink] = (ImVec4) {0.66f, 0.44f, 0.68f, 1.00f};
            c[ImGuiCol_TextSelectedBg] = (ImVec4) {0.34f, 0.18f, 0.36f, 0.65f};

            // Drag/drop and highlights
            c[ImGuiCol_DragDropTarget] = (ImVec4) {0.80f, 0.58f, 0.82f, 0.90f};

            // Nav / modal
            c[ImGuiCol_NavCursor] = (ImVec4) {0.66f, 0.44f, 0.68f, 1.00f};
            c[ImGuiCol_NavWindowingHighlight] =
                (ImVec4) {0.66f, 0.44f, 0.68f, 0.70f};
            c[ImGuiCol_NavWindowingDimBg] =
                (ImVec4) {0.045f, 0.030f, 0.035f, 0.55f};
            c[ImGuiCol_ModalWindowDimBg] =
                (ImVec4) {0.035f, 0.020f, 0.025f, 0.72f};
            break;
        case 4:
            // Text: pale moonlit cloth / ghost grey
            c[ImGuiCol_Text] = (ImVec4) {0.78f, 0.84f, 0.82f, 1.00f};
            c[ImGuiCol_TextDisabled] = (ImVec4) {0.36f, 0.43f, 0.42f, 1.00f};

            // Backgrounds: cold blue-green night fog
            c[ImGuiCol_WindowBg] = (ImVec4) {0.035f, 0.070f, 0.075f, 0.96f};
            c[ImGuiCol_ChildBg] = (ImVec4) {0.055f, 0.095f, 0.100f, 0.86f};
            c[ImGuiCol_PopupBg] = (ImVec4) {0.025f, 0.055f, 0.060f, 0.98f};

            // Borders: misty oxidized teal
            c[ImGuiCol_Border] = (ImVec4) {0.20f, 0.34f, 0.35f, 0.66f};
            c[ImGuiCol_BorderShadow] = (ImVec4) {0.00f, 0.00f, 0.00f, 0.00f};

            // Frames: dark wet wood under moonlight
            c[ImGuiCol_FrameBg] = (ImVec4) {0.070f, 0.120f, 0.125f, 0.95f};
            c[ImGuiCol_FrameBgHovered] =
                (ImVec4) {0.120f, 0.210f, 0.215f, 0.96f};
            c[ImGuiCol_FrameBgActive] =
                (ImVec4) {0.170f, 0.300f, 0.310f, 1.00f};

            // Titles: deep blue-green shadow
            c[ImGuiCol_TitleBg] = (ImVec4) {0.030f, 0.060f, 0.065f, 1.00f};
            c[ImGuiCol_TitleBgActive] =
                (ImVec4) {0.075f, 0.145f, 0.155f, 1.00f};
            c[ImGuiCol_TitleBgCollapsed] =
                (ImVec4) {0.030f, 0.055f, 0.060f, 0.86f};

            c[ImGuiCol_MenuBarBg] = (ImVec4) {0.050f, 0.095f, 0.100f, 1.00f};

            // Scrollbar: dull teal slate
            c[ImGuiCol_ScrollbarBg] = (ImVec4) {0.025f, 0.050f, 0.055f, 0.86f};
            c[ImGuiCol_ScrollbarGrab] =
                (ImVec4) {0.150f, 0.270f, 0.280f, 1.00f};
            c[ImGuiCol_ScrollbarGrabHovered] =
                (ImVec4) {0.210f, 0.370f, 0.380f, 1.00f};
            c[ImGuiCol_ScrollbarGrabActive] =
                (ImVec4) {0.290f, 0.500f, 0.510f, 1.00f};

            // Accent color: restrained spirit violet, less pink
            c[ImGuiCol_CheckMark] = (ImVec4) {0.56f, 0.45f, 0.72f, 1.00f};
            c[ImGuiCol_SliderGrab] = (ImVec4) {0.40f, 0.32f, 0.58f, 1.00f};
            c[ImGuiCol_SliderGrabActive] =
                (ImVec4) {0.66f, 0.55f, 0.82f, 1.00f};

            // Buttons: foggy teal-black
            c[ImGuiCol_Button] = (ImVec4) {0.075f, 0.130f, 0.135f, 0.95f};
            c[ImGuiCol_ButtonHovered] =
                (ImVec4) {0.145f, 0.255f, 0.265f, 1.00f};
            c[ImGuiCol_ButtonActive] = (ImVec4) {0.205f, 0.355f, 0.365f, 1.00f};

            // Headers: darker forest-fog teal
            c[ImGuiCol_Header] = (ImVec4) {0.095f, 0.175f, 0.180f, 0.86f};
            c[ImGuiCol_HeaderHovered] =
                (ImVec4) {0.165f, 0.300f, 0.310f, 0.96f};
            c[ImGuiCol_HeaderActive] = (ImVec4) {0.230f, 0.405f, 0.420f, 1.00f};

            // Separators: fogged teal edges with violet interaction highlight
            c[ImGuiCol_Separator] = (ImVec4) {0.20f, 0.34f, 0.35f, 0.55f};
            c[ImGuiCol_SeparatorHovered] =
                (ImVec4) {0.38f, 0.34f, 0.56f, 0.85f};
            c[ImGuiCol_SeparatorActive] = (ImVec4) {0.56f, 0.45f, 0.72f, 1.00f};

            // Resize grips: dim until interacted with
            c[ImGuiCol_ResizeGrip] = (ImVec4) {0.20f, 0.34f, 0.35f, 0.25f};
            c[ImGuiCol_ResizeGripHovered] =
                (ImVec4) {0.38f, 0.34f, 0.56f, 0.70f};
            c[ImGuiCol_ResizeGripActive] =
                (ImVec4) {0.56f, 0.45f, 0.72f, 0.95f};

            // Tabs: cold teal, selected with subtle spirit violet
            c[ImGuiCol_Tab] = (ImVec4) {0.065f, 0.120f, 0.125f, 0.95f};
            c[ImGuiCol_TabHovered] = (ImVec4) {0.165f, 0.300f, 0.310f, 0.96f};
            c[ImGuiCol_TabSelected] = (ImVec4) {0.145f, 0.240f, 0.270f, 1.00f};
            c[ImGuiCol_TabSelectedOverline] =
                (ImVec4) {0.56f, 0.45f, 0.72f, 1.00f};
            c[ImGuiCol_TabDimmed] = (ImVec4) {0.040f, 0.080f, 0.085f, 0.95f};
            c[ImGuiCol_TabDimmedSelected] =
                (ImVec4) {0.085f, 0.145f, 0.155f, 1.00f};
            c[ImGuiCol_TabDimmedSelectedOverline] =
                (ImVec4) {0.34f, 0.30f, 0.48f, 1.00f};

            // Plot colors: subdued ghost-violet and muted blood-red
            c[ImGuiCol_PlotLines] = (ImVec4) {0.56f, 0.45f, 0.72f, 1.00f};
            c[ImGuiCol_PlotLinesHovered] =
                (ImVec4) {0.70f, 0.62f, 0.86f, 1.00f};
            c[ImGuiCol_PlotHistogram] = (ImVec4) {0.42f, 0.18f, 0.22f, 1.00f};
            c[ImGuiCol_PlotHistogramHovered] =
                (ImVec4) {0.58f, 0.26f, 0.32f, 1.00f};

            // Tables / misc accents
            c[ImGuiCol_TableHeaderBg] =
                (ImVec4) {0.075f, 0.145f, 0.150f, 1.00f};
            c[ImGuiCol_TableBorderStrong] =
                (ImVec4) {0.20f, 0.34f, 0.35f, 1.00f};
            c[ImGuiCol_TableBorderLight] =
                (ImVec4) {0.120f, 0.230f, 0.235f, 1.00f};
            c[ImGuiCol_TableRowBg] = (ImVec4) {0.00f, 0.00f, 0.00f, 0.00f};
            c[ImGuiCol_TableRowBgAlt] =
                (ImVec4) {0.070f, 0.125f, 0.130f, 0.30f};

            c[ImGuiCol_TextLink] = (ImVec4) {0.62f, 0.56f, 0.78f, 1.00f};
            c[ImGuiCol_TextSelectedBg] = (ImVec4) {0.28f, 0.30f, 0.44f, 0.65f};

            // Drag/drop and highlights
            c[ImGuiCol_DragDropTarget] = (ImVec4) {0.70f, 0.62f, 0.86f, 0.90f};

            // Nav / modal
            c[ImGuiCol_NavCursor] = (ImVec4) {0.62f, 0.56f, 0.78f, 1.00f};
            c[ImGuiCol_NavWindowingHighlight] =
                (ImVec4) {0.62f, 0.56f, 0.78f, 0.70f};
            c[ImGuiCol_NavWindowingDimBg] =
                (ImVec4) {0.025f, 0.050f, 0.055f, 0.55f};
            c[ImGuiCol_ModalWindowDimBg] =
                (ImVec4) {0.020f, 0.040f, 0.045f, 0.72f};
            break;
        case 5:
            // Text: faded photographic paper
            c[ImGuiCol_Text] = (ImVec4) {0.86f, 0.80f, 0.68f, 1.00f};
            c[ImGuiCol_TextDisabled] = (ImVec4) {0.42f, 0.36f, 0.27f, 1.00f};

            // Backgrounds: dark sepia print / aged ink
            c[ImGuiCol_WindowBg] = (ImVec4) {0.055f, 0.045f, 0.030f, 0.96f};
            c[ImGuiCol_ChildBg] = (ImVec4) {0.080f, 0.065f, 0.045f, 0.86f};
            c[ImGuiCol_PopupBg] = (ImVec4) {0.045f, 0.035f, 0.024f, 0.98f};

            // Borders: old varnish / worn frame edges
            c[ImGuiCol_Border] = (ImVec4) {0.36f, 0.27f, 0.16f, 0.68f};
            c[ImGuiCol_BorderShadow] = (ImVec4) {0.00f, 0.00f, 0.00f, 0.00f};

            // Frames: cabinet wood and photo album paper
            c[ImGuiCol_FrameBg] = (ImVec4) {0.120f, 0.090f, 0.055f, 0.95f};
            c[ImGuiCol_FrameBgHovered] =
                (ImVec4) {0.230f, 0.170f, 0.095f, 0.96f};
            c[ImGuiCol_FrameBgActive] =
                (ImVec4) {0.330f, 0.240f, 0.130f, 1.00f};

            // Titles: dark exposed film
            c[ImGuiCol_TitleBg] = (ImVec4) {0.060f, 0.045f, 0.030f, 1.00f};
            c[ImGuiCol_TitleBgActive] =
                (ImVec4) {0.160f, 0.110f, 0.060f, 1.00f};
            c[ImGuiCol_TitleBgCollapsed] =
                (ImVec4) {0.045f, 0.035f, 0.024f, 0.86f};

            c[ImGuiCol_MenuBarBg] = (ImVec4) {0.090f, 0.065f, 0.040f, 1.00f};

            // Scrollbar: dark bronze
            c[ImGuiCol_ScrollbarBg] = (ImVec4) {0.040f, 0.032f, 0.022f, 0.86f};
            c[ImGuiCol_ScrollbarGrab] =
                (ImVec4) {0.230f, 0.165f, 0.090f, 1.00f};
            c[ImGuiCol_ScrollbarGrabHovered] =
                (ImVec4) {0.340f, 0.245f, 0.130f, 1.00f};
            c[ImGuiCol_ScrollbarGrabActive] =
                (ImVec4) {0.470f, 0.330f, 0.170f, 1.00f};

            // Accent color: amber candlelight on old paper
            c[ImGuiCol_CheckMark] = (ImVec4) {0.78f, 0.58f, 0.28f, 1.00f};
            c[ImGuiCol_SliderGrab] = (ImVec4) {0.58f, 0.40f, 0.18f, 1.00f};
            c[ImGuiCol_SliderGrabActive] =
                (ImVec4) {0.86f, 0.64f, 0.30f, 1.00f};

            // Buttons: aged wood
            c[ImGuiCol_Button] = (ImVec4) {0.135f, 0.095f, 0.055f, 0.95f};
            c[ImGuiCol_ButtonHovered] =
                (ImVec4) {0.280f, 0.190f, 0.095f, 1.00f};
            c[ImGuiCol_ButtonActive] = (ImVec4) {0.400f, 0.270f, 0.130f, 1.00f};

            // Headers: photo album separators
            c[ImGuiCol_Header] = (ImVec4) {0.170f, 0.120f, 0.065f, 0.86f};
            c[ImGuiCol_HeaderHovered] =
                (ImVec4) {0.320f, 0.220f, 0.110f, 0.96f};
            c[ImGuiCol_HeaderActive] = (ImVec4) {0.430f, 0.290f, 0.140f, 1.00f};

            // Separators and grips: warm paper edge highlights
            c[ImGuiCol_Separator] = (ImVec4) {0.36f, 0.27f, 0.16f, 0.58f};
            c[ImGuiCol_SeparatorHovered] =
                (ImVec4) {0.55f, 0.40f, 0.20f, 0.85f};
            c[ImGuiCol_SeparatorActive] = (ImVec4) {0.78f, 0.58f, 0.28f, 1.00f};
            c[ImGuiCol_ResizeGrip] = (ImVec4) {0.36f, 0.27f, 0.16f, 0.25f};
            c[ImGuiCol_ResizeGripHovered] =
                (ImVec4) {0.55f, 0.40f, 0.20f, 0.70f};
            c[ImGuiCol_ResizeGripActive] =
                (ImVec4) {0.78f, 0.58f, 0.28f, 0.95f};

            // Tabs: dark sepia with amber overline
            c[ImGuiCol_Tab] = (ImVec4) {0.115f, 0.080f, 0.045f, 0.95f};
            c[ImGuiCol_TabHovered] = (ImVec4) {0.320f, 0.220f, 0.110f, 0.96f};
            c[ImGuiCol_TabSelected] = (ImVec4) {0.300f, 0.205f, 0.105f, 1.00f};
            c[ImGuiCol_TabSelectedOverline] =
                (ImVec4) {0.78f, 0.58f, 0.28f, 1.00f};
            c[ImGuiCol_TabDimmed] = (ImVec4) {0.075f, 0.055f, 0.032f, 0.95f};
            c[ImGuiCol_TabDimmedSelected] =
                (ImVec4) {0.155f, 0.105f, 0.055f, 1.00f};
            c[ImGuiCol_TabDimmedSelectedOverline] =
                (ImVec4) {0.45f, 0.32f, 0.16f, 1.00f};

            // Plot colors: readable, warm, restrained
            c[ImGuiCol_PlotLines] = (ImVec4) {0.78f, 0.58f, 0.28f, 1.00f};
            c[ImGuiCol_PlotLinesHovered] =
                (ImVec4) {0.95f, 0.72f, 0.36f, 1.00f};
            c[ImGuiCol_PlotHistogram] = (ImVec4) {0.54f, 0.34f, 0.16f, 1.00f};
            c[ImGuiCol_PlotHistogramHovered] =
                (ImVec4) {0.80f, 0.50f, 0.22f, 1.00f};

            // Tables / misc accents
            c[ImGuiCol_TableHeaderBg] =
                (ImVec4) {0.150f, 0.105f, 0.060f, 1.00f};
            c[ImGuiCol_TableBorderStrong] =
                (ImVec4) {0.36f, 0.27f, 0.16f, 1.00f};
            c[ImGuiCol_TableBorderLight] =
                (ImVec4) {0.230f, 0.165f, 0.090f, 1.00f};
            c[ImGuiCol_TableRowBg] = (ImVec4) {0.00f, 0.00f, 0.00f, 0.00f};
            c[ImGuiCol_TableRowBgAlt] =
                (ImVec4) {0.120f, 0.085f, 0.048f, 0.30f};
            c[ImGuiCol_TextLink] = (ImVec4) {0.82f, 0.62f, 0.32f, 1.00f};
            c[ImGuiCol_TextSelectedBg] = (ImVec4) {0.42f, 0.29f, 0.14f, 0.65f};
            c[ImGuiCol_DragDropTarget] = (ImVec4) {0.95f, 0.72f, 0.36f, 0.90f};

            // Nav / modal
            c[ImGuiCol_NavCursor] = (ImVec4) {0.82f, 0.62f, 0.32f, 1.00f};
            c[ImGuiCol_NavWindowingHighlight] =
                (ImVec4) {0.95f, 0.72f, 0.36f, 0.70f};
            c[ImGuiCol_NavWindowingDimBg] =
                (ImVec4) {0.035f, 0.026f, 0.018f, 0.55f};
            c[ImGuiCol_ModalWindowDimBg] =
                (ImVec4) {0.030f, 0.022f, 0.014f, 0.72f};
            break;
    }

    if (ui_display_scale != 1.0f)
    {
        ImGuiStyle_ScaleAllSizes(s, ui_display_scale);
    }
}

void MikuPan_InitUi(SDL_Window* window)
{
    ui_window = window;
    igCreateContext(NULL);
    ImGuiIO* io = igGetIO_Nil();
    io->ConfigFlags |=
        ImGuiConfigFlags_NavEnableKeyboard | ImGuiConfigFlags_NavEnableGamepad;

    mikupan_configuration.selected_theme =
        MikuPan_ClampThemeIndex(mikupan_configuration.selected_theme);
    mikupan_configuration.selected_font =
        MikuPan_ClampFontIndex(mikupan_configuration.selected_font);
    ui_display_scale = MikuPan_CalculateUiDisplayScale(window);
    MikuPan_LoadUiFonts();
    MikuPan_ApplyUiFontScale();
    MikuPan_ApplyFatalFrameStyle(mikupan_configuration.selected_theme);

    SDL_DisplayID primary = SDL_GetPrimaryDisplay();
    if (primary != 0)
    {
        const SDL_DisplayMode* mode = SDL_GetCurrentDisplayMode(primary);
        render_resolution_width = mikupan_configuration.renderer.render.width;
        render_resolution_height = mikupan_configuration.renderer.render.height;

        MikuPan_PopulateResolutionList(primary, mode);
    }

    msaa_samples = mikupan_configuration.renderer.msaa_index;
    brightness = mikupan_configuration.renderer.brightness;
    gamma_value = mikupan_configuration.renderer.gamma;
    window_mode = mikupan_configuration.renderer.window_mode;
    show_fps = mikupan_configuration.show_fps;

    if (window_mode == MIKUPAN_WINDOW_WINDOWED
        && mikupan_configuration.renderer.is_fullscreen)
    {
        window_mode = MIKUPAN_WINDOW_FULLSCREEN;
    }

    if (window_mode < MIKUPAN_WINDOW_WINDOWED
        || window_mode > MIKUPAN_WINDOW_BORDERLESS)
    {
        window_mode = MIKUPAN_WINDOW_WINDOWED;
    }
    mikupan_configuration.renderer.window_mode = window_mode;
    is_fullscreen = (window_mode != MIKUPAN_WINDOW_WINDOWED);
    mikupan_configuration.renderer.is_fullscreen = is_fullscreen;
    mesh_lighting_mode = mikupan_configuration.renderer.lighting_mode;
    is_vsync = mikupan_configuration.renderer.vsync;
    MikuPan_PopulateGpuDriverList();

    if (mikupan_configuration.renderer.shadow_resolution <= 0)
    {
        mikupan_configuration.renderer.shadow_resolution = 256;
    }
    MikuPan_SetShadowResolution(
        mikupan_configuration.renderer.shadow_resolution);
    crt_settings = mikupan_configuration.crt;
    MikuPan_ClampCrtSettings(&crt_settings);
    mikupan_configuration.crt = crt_settings;
    MikuPan_ApplyThirdPersonCameraConfiguration();
    MikuPan_ControllerSetPreferredGamepadIndex(
        mikupan_configuration.input.selected_gamepad_index);
    mikupan_configuration.input.selected_gamepad_index =
        MikuPan_ControllerGetPreferredGamepadIndex();
    MikuPan_ControllerLoadBindingsFromConfig();

    if (mesh_lighting_mode > 1 || mesh_lighting_mode < 0)
    {
        mesh_lighting_mode = 0;
        mikupan_configuration.renderer.lighting_mode = 0;
    }

    if (gamma_value < 0.0f || gamma_value > 3.0f)
    {
        gamma_value = 1.0f;
    }

    if (brightness < 0.0f || brightness > 2.0f)
    {
        brightness = 1.0f;
    }

    MikuPan_ImGui_ImplInit(window);
}

void MikuPan_RenderUi(void)
{
    igRender();
    ImGuiIO* io = igGetIO_Nil();
    MikuPan_GPUSetViewport(0, 0, (int) io->DisplaySize.x,
                           (int) io->DisplaySize.y);
    MikuPan_ImGui_ImplRenderDrawData();
}

void MikuPan_StartFrameUi(void)
{
    MikuPan_ImGui_ImplNewFrame();
    igNewFrame();
}

static void MikuPan_UiShadowDebugWindow(void)
{
    if (!show_shadow_debug_window)
    {
        return;
    }

    igSetNextWindowSize((ImVec2) {560.0f, 680.0f}, ImGuiCond_FirstUseEver);
    if (!igBegin("Shadow Debug", (bool*) &show_shadow_debug_window, 0))
    {
        igEnd();
        return;
    }

    int shadows_on = MikuPan_IsShadowEnabled();
    if (igCheckbox("Enable Shadows", (bool*) &shadows_on))
    {
        MikuPan_SetShadowEnabled(shadows_on);
    }

    int receiver_debug_view = MikuPan_IsShadowReceiverDebugViewEnabled();
    if (igCheckbox("Receiver Debug Colors", (bool*) &receiver_debug_view))
    {
        MikuPan_SetShadowReceiverDebugViewEnabled(receiver_debug_view);
    }

    int inspect = MikuPan_IsShadowInspectEnabled();
    if (igCheckbox("Inspect Caster (orbit camera)", (bool*) &inspect))
    {
        MikuPan_SetShadowInspectEnabled(inspect);
    }
    if (inspect)
    {
        float yaw = 0.0f, pitch = 0.0f;
        MikuPan_GetShadowInspectAngles(&yaw, &pitch);
        int changed = 0;
        changed |= igSliderFloat("Yaw", &yaw, -3.14159f, 3.14159f, "%.2f", 0);
        changed |= igSliderFloat("Pitch", &pitch, -1.55f, 1.55f, "%.2f", 0);
        if (changed)
        {
            MikuPan_SetShadowInspectAngles(yaw, pitch);
        }

        int wireframe = MikuPan_IsShadowInspectWireframe();
        if (igCheckbox("Wireframe", (bool*) &wireframe))
        {
            MikuPan_SetShadowInspectWireframe(wireframe);
        }

        igTextDisabled("Orbits the shadow camera; the preview shows the");
        igTextDisabled("caster model from this angle (cast shadow is");
        igTextDisabled("overridden while this is on).");
    }

    if (shadow_debug_auto_probe)
    {
        MikuPan_ShadowDebugProbeTexture();
    }

    const MikuPan_ShadowDebugInfo* shadow_debug = MikuPan_GetShadowDebugInfo();

    igText("FBO: %s  status: 0x%04x",
           shadow_debug->fbo_initialized
               ? (shadow_debug->fbo_complete ? "complete" : "incomplete")
               : "not created",
           shadow_debug->fbo_status);
    igText("Matrix: %s  texture: %u (%dx%d)",
           shadow_debug->matrix_valid ? "valid" : "missing",
           shadow_debug->texture_id, shadow_debug->texture_size,
           shadow_debug->texture_size);

    igSeparator();
    igText("Passes: caster %d  receiver %d", shadow_debug->caster_passes,
           shadow_debug->receiver_passes);
    igText("Draws:  caster %d (%d indices)  receiver %d (%d indices)",
           shadow_debug->caster_draws, shadow_debug->caster_indices,
           shadow_debug->receiver_draws, shadow_debug->receiver_indices);
    igText(
        "Caster mesh types: 0x00=%d 0x02=%d 0x40=%d 0x42=%d 0x80=%d 0x82=%d "
        "other=%d",
        shadow_debug->caster_type_0, shadow_debug->caster_type_2,
        shadow_debug->caster_type_80, shadow_debug->caster_type_82,
        shadow_debug->caster_type_other);
    igText(
        "Caster draw types: 0x00=%d 0x02=%d 0x40=%d 0x42=%d 0x80=%d 0x82=%d "
        "other=%d",
        shadow_debug->caster_draw_type_0, shadow_debug->caster_draw_type_2,
        shadow_debug->caster_draw_type_80, shadow_debug->caster_draw_type_82,
        shadow_debug->caster_draw_type_other);
    igText("Receiver mesh types: 0x00=%d 0x10=%d 0x12=%d 0x32=%d other=%d",
           shadow_debug->receiver_type_0, shadow_debug->receiver_type_10,
           shadow_debug->receiver_type_12, shadow_debug->receiver_type_32,
           shadow_debug->receiver_type_other);

    if (!shadow_debug->fbo_complete && shadow_debug->fbo_initialized)
    {
        igTextColored(
            (ImVec4) {1.0f, 0.4f, 0.3f, 1.0f},
            "Shadow target is incomplete; caster rendering cannot write.");
    }
    else if (shadow_debug->caster_passes == 0)
    {
        igTextColored((ImVec4) {1.0f, 0.7f, 0.2f, 1.0f},
                      "No caster pass this frame.");
    }
    else if (shadow_debug->caster_draws == 0
             && (shadow_debug->caster_type_0 != 0
                 || shadow_debug->caster_type_80 != 0
                 || shadow_debug->caster_type_other != 0))
    {
        igTextColored(
            (ImVec4) {1.0f, 0.7f, 0.2f, 1.0f},
            "Caster meshes were found, but none reached a GL shadow draw.");
    }
    else if (shadow_debug->receiver_passes != 0
             && shadow_debug->receiver_draws == 0)
    {
        igTextColored(
            (ImVec4) {1.0f, 0.7f, 0.2f, 1.0f},
            "Receiver traversal ran, but no receiver meshes were drawn.");
    }

    igSeparator();
    if (igButton("Probe Shadow Map", (ImVec2) {0.0f, 0.0f}))
    {
        MikuPan_ShadowDebugProbeTexture();
        shadow_debug = MikuPan_GetShadowDebugInfo();
    }
    igSameLine(0.0f, -1.0f);
    igCheckbox("Auto Probe", (bool*) &shadow_debug_auto_probe);

    if (shadow_debug->probe_valid)
    {
        igText("Map probe: nonzero %d / %d (%.2f%%), max %d, avg %.3f",
               shadow_debug->probe_nonzero_pixels,
               shadow_debug->texture_size * shadow_debug->texture_size,
               shadow_debug->probe_coverage * 100.0f,
               shadow_debug->probe_max_value, shadow_debug->probe_average);

        if (shadow_debug->caster_draws != 0
            && shadow_debug->probe_nonzero_pixels == 0)
        {
            igTextColored(
                (ImVec4) {1.0f, 0.7f, 0.2f, 1.0f},
                "Caster draws happened, but the shadow map is empty.");
        }
    }
    else
    {
        igTextDisabled("Map probe: not sampled yet");
    }

    igSeparator();
    igText("Shadow Map (R8 occlusion, white = caster)");
    igSliderFloat("Texture Preview Size", &shadow_debug_preview_size, 64.0f,
                  768.0f, "%.0f", 0);
    igCheckbox("Flip Y", (bool*) &shadow_debug_flip_y);

    if (shadow_debug->texture_id != 0)
    {
        ImVec2 uv0 =
            shadow_debug_flip_y ? (ImVec2) {0.0f, 1.0f} : (ImVec2) {0.0f, 0.0f};
        ImVec2 uv1 =
            shadow_debug_flip_y ? (ImVec2) {1.0f, 0.0f} : (ImVec2) {1.0f, 1.0f};

        // Draw on a mid-grey background with a visible frame so the 256x256
        // extent is obvious even when the map is empty (all-black). The R8
        // channel is swizzled to RGB at texture creation, so this shows the
        // occlusion silhouette as proper grayscale rather than dark red.
        ImVec2 img_min = igGetCursorScreenPos();
        ImVec2 img_size =
            (ImVec2) {shadow_debug_preview_size, shadow_debug_preview_size};

        igImageWithBg(
            (ImTextureRef_c) {
                ._TexID = (ImTextureID) (uintptr_t) MikuPan_GPUGetTextureHandle(
                    shadow_debug->texture_id)},
            img_size, uv0, uv1,
            (ImVec4) {0.15f, 0.15f, 0.18f,
                      1.0f}, /* bg behind transparent/black */
            (ImVec4) {1.0f, 1.0f, 1.0f, 1.0f}); /* tint */

        ImDrawList* draw_list = igGetWindowDrawList();
        ImVec2 img_max =
            (ImVec2) {img_min.x + img_size.x, img_min.y + img_size.y};
        ImDrawList_AddRect(
            draw_list, img_min, img_max,
            igGetColorU32_Vec4((ImVec4) {0.55f, 0.85f, 1.0f, 0.9f}), 0.0f, 0,
            0);
    }
    else
    {
        igTextDisabled("Shadow texture has not been created yet.");
    }

    igEnd();
}

static void MikuPan_UiPhotoDebugWindow(void)
{
    if (!show_photo_debug_window)
    {
        return;
    }

    igSetNextWindowSize((ImVec2) {460.0f, 560.0f}, ImGuiCond_FirstUseEver);
    if (!igBegin("Photo Debug", (bool*) &show_photo_debug_window, 0))
    {
        igEnd();
        return;
    }

    int force_preview = MikuPan_IsPhotoDebugForceOpaquePreviewEnabled();
    if (igCheckbox("Force Opaque Preview Overlay", (bool*) &force_preview))
    {
        MikuPan_SetPhotoDebugForceOpaquePreviewEnabled(force_preview);
    }

    int force_negative = MikuPan_IsPhotoDebugForceNegativePreviewEnabled();
    if (igCheckbox("Force Negative Final Pass", (bool*) &force_negative))
    {
        MikuPan_SetPhotoDebugForceNegativePreviewEnabled(force_negative);
    }

    float force_negative_strength =
        MikuPan_GetPhotoDebugForceNegativePreviewStrength();
    if (igSliderFloat("Forced Negative Strength", &force_negative_strength,
                      0.0f, 1.0f, "%.2f", 0))
    {
        MikuPan_SetPhotoDebugForceNegativePreviewStrength(
            force_negative_strength);
    }

    int target_rect = MikuPan_IsPhotoDebugTargetRectEnabled();
    if (igCheckbox("Show Target Rect", (bool*) &target_rect))
    {
        MikuPan_SetPhotoDebugTargetRectEnabled(target_rect);
    }

    int negative_layer = MikuPan_IsPhotoDebugNegativeLayerEnabled();
    if (igCheckbox("Show Negative Debug Layer", (bool*) &negative_layer))
    {
        MikuPan_SetPhotoDebugNegativeLayerEnabled(negative_layer);
    }

    igSeparator();

    const MikuPan_PhotoDebugInfo* photo_debug = MikuPan_GetPhotoDebugInfo();

    igText("Texture: %s  id: %u  size: %dx%d",
           photo_debug->texture_valid ? "valid" : "missing",
           photo_debug->texture_id, photo_debug->texture_width,
           photo_debug->texture_height);
    igText("Texture updates: %d", photo_debug->texture_update_count);
    igText("Negative source: %s  id: %u  size: %dx%d",
           photo_debug->negative_source_texture_valid ? "valid" : "missing",
           photo_debug->negative_source_texture_id,
           photo_debug->negative_source_texture_width,
           photo_debug->negative_source_texture_height);
    igText("Negative source updates: %d",
           photo_debug->negative_source_texture_update_count);

    igSeparator();
    igText("Queued: %s  count: %d", photo_debug->queued ? "yes" : "no",
           photo_debug->queue_count);
    igText("Effect overlay active: %s  count: %d",
           photo_debug->effect_overlay_active ? "yes" : "no",
           photo_debug->effect_overlay_count);
    igText("Effect overlay drawn in game: %s  count: %d",
           photo_debug->effect_overlay_drawn_in_game ? "yes" : "no",
           photo_debug->effect_overlay_in_game_count);
    igText("Negative final-pass active: %s  count: %d",
           photo_debug->negative_overlay_active ? "yes" : "no",
           photo_debug->negative_overlay_count);
    igText("Legacy negative sprite drawn: %s  count: %d",
           photo_debug->negative_overlay_drawn_in_game ? "yes" : "no",
           photo_debug->negative_overlay_in_game_count);
    igText("Force negative preview: %s  strength: %.2f",
           photo_debug->force_negative_preview_enabled ? "yes" : "no",
           photo_debug->force_negative_preview_strength);
    igText("Negative rect: %d,%d %dx%d  strength: %.2f",
           photo_debug->negative_x, photo_debug->negative_y,
           photo_debug->negative_w, photo_debug->negative_h,
           photo_debug->negative_strength);
    igText("Queue rect: %d,%d %dx%d  alpha: %d", photo_debug->queue_x,
           photo_debug->queue_y, photo_debug->queue_w, photo_debug->queue_h,
           photo_debug->queue_alpha);

    igSeparator();
    igText("Last draw: %s  count: %d",
           photo_debug->last_draw_valid ? "yes" : "no",
           photo_debug->draw_count);
    igText("Draw rect: %d,%d %dx%d  alpha: %d", photo_debug->draw_x,
           photo_debug->draw_y, photo_debug->draw_w, photo_debug->draw_h,
           photo_debug->draw_alpha);
    igText("Target rect drawn this frame: %s",
           photo_debug->target_rect_drawn ? "yes" : "no");
    igText("Negative debug layer drawn this frame: %s  count: %d",
           photo_debug->negative_debug_layer_drawn ? "yes" : "no",
           photo_debug->negative_debug_layer_count);

    igSeparator();
    igTextDisabled("Expected photo rect is usually 128,80 384x256.");
    igTextDisabled("Cyan negative debug layer marks the outside region.");
    igTextDisabled("If this preview is correct but the overlay is wrong,");
    igTextDisabled("the failure is compositing/order/state, not capture.");

    igSeparator();
    igText("Captured Photo Texture");
    igSliderFloat("Preview Size", &photo_debug_preview_size, 64.0f, 512.0f,
                  "%.0f", 0);

    if (photo_debug->texture_id != 0)
    {
        float aspect = 1.0f;
        if (photo_debug->texture_width > 0 && photo_debug->texture_height > 0)
        {
            aspect = (float) photo_debug->texture_width
                     / (float) photo_debug->texture_height;
        }

        ImVec2 img_size = (ImVec2) {photo_debug_preview_size,
                                    photo_debug_preview_size / aspect};

        igImageWithBg(
            (ImTextureRef_c) {
                ._TexID = (ImTextureID) (uintptr_t) MikuPan_GPUGetTextureHandle(
                    photo_debug->texture_id)},
            img_size, (ImVec2) {0.0f, 0.0f}, (ImVec2) {1.0f, 1.0f},
            (ImVec4) {0.12f, 0.12f, 0.14f, 1.0f},
            (ImVec4) {1.0f, 1.0f, 1.0f, 1.0f});
    }
    else
    {
        igTextDisabled("No photo preview texture has been uploaded yet.");
    }

    igEnd();
}

void MikuPan_DrawUi(void)
{
    ImGuiIO* io = igGetIO_Nil();
    FrameTimeGraph_Update(&g_frame_graph, 1000.0f / io->Framerate,
                          MikuPan_GetLastFrameCpuMs(),
                          MikuPan_GetLastFrameGpuMs());

    MikuPan_UiHandleShortcuts();
    MikuPan_UiMenuBar();

    if (dbg_wrk.mode_on == 1)
    {
        gra2dDrawDbgMenu();
    }

    if (show_frame_time_graph)
    {
        FrameTimeGraph_Draw(&g_frame_graph);
    }

    if (show_texture_list)
    {
        MikuPan_ShowTextureList();
    }

    MikuPan_UiShaderReloadWindow();
    MikuPan_UiDrawCallInspector();
    MikuPan_UiCameraDebugWindow();
    MikuPan_UiPhotoDebugWindow();
    MikuPan_UiShadowDebugWindow();

    if (show_fps)
    {
        igBegin("fps", (bool*) &show_fps, no_navigation_window);
        igPushFont(igGetFont(), 18.0f * ui_display_scale);
        igText("FPS %.2f", MikuPan_GetFrameRate());
        igPopFont();
        igEnd();
    }
}

void MikuPan_ShutDownUi(void)
{
    MikuPan_ImGui_ImplShutdown();
    igDestroyContext(NULL);
}

void MikuPan_ProcessEventUi(SDL_Event* event)
{
    MikuPan_ImGui_ImplProcessEvent(event);
}

float MikuPan_GetFrameRate(void)
{
    ImGuiIO* io = igGetIO_Nil();
    return io->Framerate;
}

int MikuPan_IsWireframeRendering(void)
{
    return render_wireframe;
}

int MikuPan_IsNormalsRendering(void)
{
    return render_normals;
}

void MikuPan_ShowTextureList(void)
{
    const MikuPan_TextureInfo* screen_copy;

    tex_list_count = 0;
    MikuPan_ForEachTexture(CollectTexture, &tex_list_count);

    qsort(tex_list, tex_list_count, sizeof(MikuPan_TextureInfo*),
          CompareTextureById);

    igBegin("Texture List", NULL, 0);

    screen_copy = MikuPan_GetScreenCopyTextureInfo();
    if (screen_copy != NULL)
    {
        if (igCollapsingHeader_TreeNodeFlags(
                "Generated: distortion screen copy (0x1a40)",
                ImGuiTreeNodeFlags_DefaultOpen))
        {
            const MikuPan_ScreenCopyDebugInfo* debug =
                MikuPan_GetScreenCopyDebugInfo();
            float preview_w = (float) screen_copy->width;
            float preview_h = (float) screen_copy->height;

            if (preview_w > 512.0f)
            {
                preview_h *= 512.0f / preview_w;
                preview_w = 512.0f;
            }

            igText("%u: %d x %d", screen_copy->id, screen_copy->width,
                   screen_copy->height);
            if (debug != NULL)
            {
                const char* depth =
                    debug->depth_mode == MIKUPAN_DEPTH_ALWAYS   ? "always"
                    : debug->depth_mode == MIKUPAN_DEPTH_GEQUAL ? "gequal"
                                                                : "lequal";

                igText("last draw: %d vertices, depth %s, blend %s",
                       debug->vertex_count, depth,
                       debug->additive_blend ? "add" : "alpha");
                igText("uv:  %.3f %.3f  ->  %.3f %.3f", debug->uv_min[0],
                       debug->uv_min[1], debug->uv_max[0], debug->uv_max[1]);
                igText("ndc: %.3f %.3f  ->  %.3f %.3f", debug->ndc_min[0],
                       debug->ndc_min[1], debug->ndc_max[0], debug->ndc_max[1]);
            }
            igImage((ImTextureRef_c) {._TexID = (ImTextureID) (uintptr_t)
                                          MikuPan_GPUGetTextureHandle(
                                              screen_copy->id)},
                    (ImVec2) {preview_w, preview_h}, (ImVec2) {0.0f, 0.0f},
                    (ImVec2) {1.0f, 1.0f});
        }
    }
    else
    {
        igText("Generated distortion screen copy: not created yet");
    }

    for (int i = 0; i < tex_list_count; i++)
    {
        char label[64];
        snprintf(label, sizeof(label), "Texture ID %u", tex_list[i]->id);

        if (igCollapsingHeader_TreeNodeFlags(label, 0))
        {
            igText("%u: %d x %d", tex_list[i]->id, tex_list[i]->width,
                   tex_list[i]->height);
            igImage((ImTextureRef_c) {._TexID = (ImTextureID) (uintptr_t)
                                          MikuPan_GPUGetTextureHandle(
                                              tex_list[i]->id)},
                    (ImVec2) {(float) tex_list[i]->width,
                              (float) tex_list[i]->height},
                    (ImVec2) {0.0f, 0.0f}, (ImVec2) {1.0f, 1.0f});
        }
    }

    igEnd();
}

void MikuPan_UiHandleShortcuts(void)
{
    if (igIsKeyPressed_Bool(ImGuiKey_F1, 0)
        || (igIsKeyPressed_Bool(ImGuiKey_GamepadL3, 1)
            && igIsKeyPressed_Bool(ImGuiKey_GamepadStart, 1)))
    {
        show_menu_bar = !show_menu_bar;
    }

    if (igIsKeyPressed_Bool(ImGuiKey_F2, 0))
    {
        dbg_wrk.mode_on = !dbg_wrk.mode_on;
    }

    if (igIsKeyPressed_Bool(ImGuiKey_F5, 0))
    {
        last_reload_error[0] = '\0';
        MikuPan_ReloadAllShaders(last_reload_error,
                                 (int) sizeof(last_reload_error));
        if (last_reload_error[0] != '\0')
        {
            show_shader_reload = 1;
        }
    }

    if (igIsKeyPressed_Bool(ImGuiKey_F12, 0))
    {
        MikuPan_ScreenshotRequest();
    }
}

/* Shared shadow-map resolution dropdown (used in both Display and
 * Rendering > Shadows). Reads/writes the runtime size, which is persisted to
 * mikupan_configuration.renderer.shadow_resolution on Save Configuration. */
static void MikuPan_UiShadowResolutionCombo(const char* label)
{
    static const int res_values[] = {128, 256, 512, 1024, 2048};
    static const char* res_labels[] = {"128", "256", "512", "1024", "2048"};
    const int count = (int) (sizeof(res_values) / sizeof(res_values[0]));

    int cur = MikuPan_GetShadowResolution();
    int idx = 1; /* default 256 */
    for (int i = 0; i < count; i++)
    {
        if (res_values[i] == cur)
        {
            idx = i;
            break;
        }
    }

    if (igCombo_Str_arr(label, &idx, res_labels, count, -1))
    {
        MikuPan_SetShadowResolution(res_values[idx]);
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

void MikuPan_UiMenuBar(void)
{
    if (!show_menu_bar || !igBeginMainMenuBar())
    {
        return;
    }

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
            igCheckbox("VSync", (bool*) &is_vsync);

            igSeparatorText("Graphics");

            MikuPan_UiGpuBackendCombo();

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
                                          (ImVec2) {0, 0}))
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

            MikuPan_UiShadowResolutionCombo("Shadow Resolution");

            const char* display_lighting_modes[] = {"Pixel (Mordern)",
                                                    "Vertex (PS2)"};
            igCombo_Str_arr("Lighting Mode", &mesh_lighting_mode,
                            display_lighting_modes, 2, -1);

            igEndMenu();
        }

        if (igBeginMenu("Theme", 1))
        {
            int selected_theme =
                MikuPan_ClampThemeIndex(mikupan_configuration.selected_theme);
            if (selected_theme != mikupan_configuration.selected_theme)
            {
                mikupan_configuration.selected_theme = selected_theme;
            }

            if (igCombo_Str_arr("Theme", &selected_theme, theme_labels,
                                MIKUPAN_UI_THEME_COUNT, -1))
            {
                mikupan_configuration.selected_theme = selected_theme;
                MikuPan_ApplyFatalFrameStyle(selected_theme);
            }

            int selected_font =
                MikuPan_ClampFontIndex(mikupan_configuration.selected_font);
            if (selected_font != mikupan_configuration.selected_font)
            {
                mikupan_configuration.selected_font = selected_font;
            }

            if (igCombo_Str_arr("Font", &selected_font, font_labels,
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

                MikuPan_ClampCrtSettings(&crt_settings);

                if (igButton("Reset CRT", (ImVec2) {0.0f, 0.0f}))
                {
                    crt_settings = crt_defaults;
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

        igSeparatorText("Save / Game Data Folder");
        igInputText("Folder", mikupan_configuration.data_folder, sizeof(mikupan_configuration.data_folder), 0, NULL, NULL);

        if (igButton("Browse...", (ImVec2) {0.0f, 0.0f}))
        {
            const char *start = mikupan_configuration.data_folder[0] != '\0'
                                    ? mikupan_configuration.data_folder
                                    : NULL;
            SDL_ShowOpenFolderDialog(MikuPan_DataFolderSelected, NULL,
                                     ui_window, start, false);
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

    if (igBeginMenu("Debug", 1))
    {
        igCheckbox("Zero's Menu", (bool*) &dbg_wrk.mode_on);

        igSeparator();

        if (igBeginMenu("Rendering", 1))
        {
            igCheckbox("Draw Call Inspector", (bool*) &show_draw_inspector);
            igCheckbox("Shader Reload", (bool*) &show_shader_reload);

            if (igBeginMenu("Visualization", 1))
            {
                igCheckbox("Wireframe", (bool*) &render_wireframe);
                igCheckbox("Bounding Boxes", (bool*) &show_bounding_boxes);
                igCheckbox("Normals", (bool*) &render_normals);

                if (render_normals)
                {
                    igSliderFloat("Normal Length", &normal_length, 0.1f, 100.0f,
                                  "%.1f", 0);
                }

                igCheckbox("Photo", (bool*) &show_photo_debug_window);

                igEndMenu();
            }

            if (igBeginMenu("Lighting", 1))
            {
                igCheckbox("Disable Lighting", (bool*) &disable_lighting);
                igCheckbox("Static Lighting", (bool*) &show_static_lighting);
                const char* lighting_modes[] = {"Per-Fragment", "Per-Vertex"};
                igCombo_Str_arr("Lighting Mode", &mesh_lighting_mode,
                                lighting_modes, 2, -1);
                igEndMenu();
            }

            if (igBeginMenu("Shadows", 1))
            {
                int shadows_on = MikuPan_IsShadowEnabled();
                if (igCheckbox("Enable Shadows", (bool*) &shadows_on))
                {
                    MikuPan_SetShadowEnabled(shadows_on);
                }

                MikuPan_UiShadowResolutionCombo("Resolution");

                igCheckbox("Shadow Debug Window",
                           (bool*) &show_shadow_debug_window);
                igEndMenu();
            }

            if (igBeginMenu("Meshes", 1))
            {
                igCheckbox("Mesh 0x82", (bool*) &show_mesh_0x82);
                igCheckbox("Mesh 0x32", (bool*) &show_mesh_0x32);
                igCheckbox("Mesh 0x12", (bool*) &show_mesh_0x12);
                igCheckbox("Mesh 0x10", (bool*) &show_mesh_0x10);
                igCheckbox("Mesh 0x2", (bool*) &show_mesh_0x2);

                igSeparator();
                int mesh_cache_on = MikuPan_MeshCache_IsEnabled();
                if (igCheckbox("Mesh Cache", (bool*) &mesh_cache_on))
                {
                    MikuPan_MeshCache_SetEnabled(mesh_cache_on);
                }
                if (igButton("Clear Mesh Cache", (ImVec2) {0.0f, 0.0f}))
                {
                    MikuPan_MeshCache_Flush();
                }

                extern int MikuPan_GpuSkinningEnabled(void);
                extern void MikuPan_SetGpuSkinningEnabled(int);
                int gpu_skin_on = MikuPan_GpuSkinningEnabled();
                if (igCheckbox("GPU Skinning (0x2/0xA)", (bool*) &gpu_skin_on))
                {
                    MikuPan_SetGpuSkinningEnabled(gpu_skin_on);
                }
                igEndMenu();
            }

            if (igBeginMenu("Textures", 1))
            {
                igCheckbox("Disable GS Uploads", (bool*) &disable_gs_uploads);

                if (igIsItemHovered(0))
                {
                    igBeginTooltip();
                    igText("THIS *WILL* BREAK TEXTURES, ONLY USE FOR TESTING PERFORMANCE IMPACT OF GS UPLOADS");
                    igEndTooltip();
                }

                igCheckbox("Texture List", (bool*) &show_texture_list);
                if (igButton("Clear Texture Cache", (ImVec2) {0.0f, 0.0f}))
                {
                    MikuPan_RequestFlushTextureCache();
                }

                igEndMenu();
            }

            igEndMenu();
        }

        if (igBeginMenu("Camera", 1))
        {
            igCheckbox("Camera World Info", (bool*) &show_camera_debug);
            igCheckbox("Third-Person Camera",
                       (bool*) &camera_third_person_enabled);
            if (camera_third_person_enabled)
            {
                igSeparator();
                igSliderFloat("Distance", &camera_third_person_distance, 100.0f,
                              2500.0f, "%.0f", 0);
                igSliderFloat("Height", &camera_third_person_height, 0.0f,
                              1400.0f, "%.0f", 0);
                igSliderFloat("Side", &camera_third_person_side, -600.0f,
                              600.0f, "%.0f", 0);
                igSliderFloat("Look Ahead", &camera_third_person_look_ahead,
                              100.0f, 2500.0f, "%.0f", 0);
                igSliderFloat("Interest Height",
                              &camera_third_person_interest_height, -400.0f,
                              1200.0f, "%.0f", 0);
                igSliderFloat("FOV", &camera_third_person_fov_deg, 20.0f, 90.0f,
                              "%.0f", 0);
            }

            igEndMenu();
        }

        if (igBeginMenu("Profiler", 1))
        {
            if (igCheckbox("FPS Counter", (bool*) &show_fps))
            {
                mikupan_configuration.show_fps = show_fps;
            }

            igCheckbox("Frame Time Graph", (bool*) &show_frame_time_graph);
            igEndMenu();
        }

        igEndMenu();
    }

    if (igBeginMenu("Cheats", 1))
    {
        if (igMenuItem_Bool("Take Screenshot", "F12", false, true))
        {
            MikuPan_ScreenshotRequest();
        }

        igCheckbox("Tofu Mode", (bool*) &cheat_tofu_mode);
        igColorEdit3("Tofu Color", cheat_tofu_color, 0);

        igEndMenu();
    }

    if (igBeginMenu("Info", 1))
    {
        igText("MikuPan");
        igSeparator();
        igText("Tag:      %s", MIKUPAN_GIT_TAG);
        igText("Commit:   %s", MIKUPAN_GIT_COMMIT);
        igText("Version:  %s", MIKUPAN_GIT_DESCRIBE);
        igText("Branch:   %s", MIKUPAN_GIT_BRANCH);
        igText("Built:    %s", MIKUPAN_BUILD_DATE);

        igSeparator();
        char version_line[256];
        snprintf(version_line, sizeof(version_line), "%s (%s)",
                 MIKUPAN_GIT_DESCRIBE, MIKUPAN_GIT_COMMIT);
        if (igButton("Copy version to clipboard", (ImVec2) {0.0f, 0.0f}))
        {
            igSetClipboardText(version_line);
        }

        igEndMenu();
    }

    igEndMainMenuBar();
}

int MikuPan_IsBoundingBoxRendering(void)
{
    return show_bounding_boxes;
}

int MikuPan_IsTofuModeEnabled(void)
{
    return cheat_tofu_mode;
}

const float* MikuPan_GetTofuColor(void)
{
    return cheat_tofu_color;
}

int MikuPan_ShowCameraDebug(void)
{
    return show_camera_debug;
}

int MikuPan_IsMesh0x82Rendering(void)
{
    return show_mesh_0x82;
}

int MikuPan_IsMesh0x32Rendering(void)
{
    return show_mesh_0x32;
}

int MikuPan_IsMesh0x12Rendering(void)
{
    return show_mesh_0x12;
}

int MikuPan_IsMesh0x10Rendering(void)
{
    return show_mesh_0x10;
}

int MikuPan_IsMesh0x2Rendering(void)
{
    return show_mesh_0x2;
}

int MikuPan_IsLightingDisabled(void)
{
    return disable_lighting;
}

int MikuPan_IsGsUploadsDisabled(void)
{
    return disable_gs_uploads;
}

int MikuPan_ShowStaticLighting(void)
{
    return show_static_lighting;
}

int MikuPan_GetMeshLightingMode(void)
{
    return mesh_lighting_mode;
}

float MikuPan_GetNormalLength(void)
{
    return normal_length;
}

int MikuPan_GetRenderResolutionWidth(void)
{
    return render_resolution_width;
}

int MikuPan_GetRenderResolutionHeight(void)
{
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
    return is_vsync;
}
