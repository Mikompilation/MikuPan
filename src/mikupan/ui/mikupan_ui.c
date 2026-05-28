#include "typedefs.h"

#define CIMGUI_DEFINE_ENUMS_AND_STRUCTS
#include "cimgui.h"

#include "typedefs.h"
#include "mikupan/ui/mikupan_ui.h"
#include "mikupan/mikupan_controller.h"
#include "mikupan/mikupan_screenshot.h"
#include "mikupan/gs/mikupan_texture_manager_c.h"
#include "mikupan/rendering/mikupan_shader.h"
#include "mikupan/rendering/mikupan_profiler.h"
#include "mikupan/rendering/mikupan_renderer.h"
#include "mikupan/rendering/mikupan_meshcache.h"
#include "glad/gl.h"
#include "graphics/graph2d/g2d_debug.h"
#include "graphics/graph2d/message.h"
#include "ingame/camera/camera.h"
#include "main/glob.h"

#include "mikupan/mikupan_config.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// -- Backend wrappers (defined in mikupan_ui.cpp) ----------------------------
void MikuPan_ImGui_ImplInit(SDL_Window *window, void *gl_context);
void MikuPan_ImGui_ImplShutdown(void);
void MikuPan_ImGui_ImplNewFrame(void);
void MikuPan_ImGui_ImplProcessEvent(SDL_Event *event);
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

// -- GS instrumentation (defined in mikupan_gs.cpp) --------------------------
int MikuPan_GsGetUploadCount(void);
int MikuPan_GsGetUploadBytes(void);
float MikuPan_GsGetUploadMs(void);
int MikuPan_GsGetDownloadCount(void);
int MikuPan_GsGetDownloadBytes(void);
float MikuPan_GsGetDownloadMs(void);

// -- State -------------------------------------------------------------------
const int msaa_list[] = {0, 2, 4, 8, 16, 32};
int show_fps = 0;
int show_menu_bar = 0;

static int show_frame_time_graph = 0;
static int render_wireframe = 0;
static int render_normals = 0;
static int msaa_samples = 0;

// Render resolution defaults to the primary display's current mode. The combo
// box below is populated from the primary display's supported fullscreen modes
// in MikuPan_InitUi; until that runs it falls back to a sensible 1080p.
static int render_resolution_width  = 1920;
static int render_resolution_height = 1080;

#define MIKUPAN_MAX_RESOLUTIONS 64

static MikuPan_Resolution resolution_list[MIKUPAN_MAX_RESOLUTIONS];
static char               resolution_labels[MIKUPAN_MAX_RESOLUTIONS][32];
static const char        *resolution_label_ptrs[MIKUPAN_MAX_RESOLUTIONS];
static int                resolution_count = 0;
static int                resolution_selected = 0;

static int show_texture_list = 0;
static int show_controller_remap = 0;
static int show_shader_reload = 0;
static int show_draw_inspector = 0;
static int show_camera_debug = 0;
static char config_save_status[128] = {0};

// Last reload error (per-shader: index 0..MAX-1; index MAX = "reload all")
// — kept around so the user can read why the last reload failed without it
// flying past in the log.
static char last_reload_error[1280] = {0};

static int is_fullscreen = 0;
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
static int mesh_lighting_mode = 1; // 0 = per-fragment, 1 = per-vertex
static float normal_length = 10.0f;

// Post-process tone controls — applied by the POSTPROCESS_SHADER on the
// final scene-to-window blit. Defaults are no-ops (1.0 brightness, 1.0
// gamma); the renderer reads them via the getters below each frame and
// pushes them as uniforms.
static float brightness = 1.0f;
static float gamma_value = 1.0f;

#define MIKUPAN_CRT_DEFAULTS \
    {                        \
        0,                   \
        1.0f,                \
        0.08f,               \
        0.02f,               \
        0.18f,               \
        1.0f,                \
        1.6f,                \
        0.22f,               \
        1.0f,                \
        0.25f,               \
        0.78f,               \
        0.75f,               \
        0.15f,               \
        0.75f,               \
        0.02f,               \
        0.02f,               \
        0.08f                \
    }

static const MikuPan_ConfigCrt crt_defaults = MIKUPAN_CRT_DEFAULTS;
static MikuPan_ConfigCrt crt_settings = MIKUPAN_CRT_DEFAULTS;

static void MikuPan_UiStoreRuntimeConfiguration(void);

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

static void MikuPan_ClampCrtSettings(MikuPan_ConfigCrt *crt)
{
    crt->enabled = crt->enabled ? 1 : 0;
    crt->strength = MikuPan_ClampFloat(crt->strength, 0.0f, 1.0f);
    crt->curvature = MikuPan_ClampFloat(crt->curvature, 0.0f, 0.30f);
    crt->overscan = MikuPan_ClampFloat(crt->overscan, 0.0f, 0.12f);
    crt->scanline_strength = MikuPan_ClampFloat(crt->scanline_strength, 0.0f, 1.0f);
    crt->scanline_scale = MikuPan_ClampFloat(crt->scanline_scale, 0.25f, 3.0f);
    crt->scanline_thickness = MikuPan_ClampFloat(crt->scanline_thickness, 0.5f, 4.0f);
    crt->mask_strength = MikuPan_ClampFloat(crt->mask_strength, 0.0f, 1.0f);
    crt->mask_scale = MikuPan_ClampFloat(crt->mask_scale, 0.5f, 4.0f);
    crt->vignette_strength = MikuPan_ClampFloat(crt->vignette_strength, 0.0f, 1.0f);
    crt->vignette_size = MikuPan_ClampFloat(crt->vignette_size, 0.25f, 1.25f);
    crt->chroma_offset = MikuPan_ClampFloat(crt->chroma_offset, 0.0f, 3.0f);
    crt->blend_strength = MikuPan_ClampFloat(crt->blend_strength, 0.0f, 1.0f);
    crt->blend_radius = MikuPan_ClampFloat(crt->blend_radius, 0.0f, 3.0f);
    crt->noise_strength = MikuPan_ClampFloat(crt->noise_strength, 0.0f, 0.15f);
    crt->flicker_strength = MikuPan_ClampFloat(crt->flicker_strength, 0.0f, 0.10f);
    crt->glow_strength = MikuPan_ClampFloat(crt->glow_strength, 0.0f, 0.50f);
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
    mikupan_configuration.renderer.is_fullscreen = is_fullscreen;
    mikupan_configuration.renderer.vsync = is_vsync;
    mikupan_configuration.renderer.lighting_mode = mesh_lighting_mode;
    mikupan_configuration.renderer.msaa_index = msaa_samples;
    mikupan_configuration.renderer.brightness = brightness;
    mikupan_configuration.renderer.gamma = gamma_value;
    mikupan_configuration.crt = crt_settings;
}

// -- FrameTimeGraph ----------------------------------------------------------
#define FRAME_GRAPH_CAPACITY 600

typedef struct
{
    float times[FRAME_GRAPH_CAPACITY];///< total wall-clock per frame (ms)
    float cpu[FRAME_GRAPH_CAPACITY];  ///< CPU command-submission time (ms)
    float gpu
        [FRAME_GRAPH_CAPACITY];///< GPU execution time via GL_TIME_ELAPSED (ms)
    int count;
    int max_samples;
    float ms_scale;
} FrameTimeGraph;

static FrameTimeGraph g_frame_graph = {.count = 0,
                                       .max_samples = 600,
                                       .ms_scale = -1.0f};

static void FrameTimeGraph_Update(FrameTimeGraph *g, float total_ms,
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

static void FrameTimeGraph_Draw(FrameTimeGraph *g)
{
    if (g->count == 0)
    {
        igTextUnformatted("No frame data yet", NULL);
        return;
    }

    igBegin("Frame Time Graph", NULL, 0);

    float total_sum = 0.0f, total_max = g->times[0];
    float cpu_sum = 0.0f, cpu_max = g->cpu[0];
    float gpu_sum = 0.0f, gpu_max = g->gpu[0];

    for (int i = 0; i < g->count; i++)
    {
        total_sum += g->times[i];
        if (g->times[i] > total_max)
            total_max = g->times[i];
        cpu_sum += g->cpu[i];
        if (g->cpu[i] > cpu_max)
            cpu_max = g->cpu[i];
        gpu_sum += g->gpu[i];
        if (g->gpu[i] > gpu_max)
            gpu_max = g->gpu[i];
    }

    float total_avg = total_sum / (float) g->count;
    float cpu_avg = cpu_sum / (float) g->count;
    float gpu_avg = gpu_sum / (float) g->count;
    float total_latest = g->times[g->count - 1];
    float cpu_latest = g->cpu[g->count - 1];
    float gpu_latest = g->gpu[g->count - 1];

    igText("Frame:  %5.2f ms  (%4.1f FPS)  avg %5.2f  max %5.2f", total_latest,
           total_latest > 0.0f ? 1000.0f / total_latest : 0.0f, total_avg,
           total_max);
    igText("CPU work:        %5.2f ms      avg %5.2f  max %5.2f", cpu_latest,
           cpu_avg, cpu_max);
    igText("GPU finish wait: %5.2f ms      avg %5.2f  max %5.2f", gpu_latest,
           gpu_avg, gpu_max);
    igTextDisabled(
        "(measured via fence sync - GPU value is the time CPU spent waiting "
        "after submitting)");

    // Heuristic: tag the dominant cost. With the new fence-sync measurement,
    // a low GPU value means the GPU drained the queue as fast as the CPU
    // could feed it (= CPU-bound), and a high GPU value means CPU finished
    // first and waited (= GPU-bound).
    if (gpu_latest < cpu_latest * 0.25f)
    {
        igTextColored((ImVec4) {1.0f, 0.6f, 0.3f, 1.0f},
                      "CPU-bound (GPU drained queue fast)");
    }
    else if (gpu_latest > cpu_latest)
    {
        igTextColored((ImVec4) {0.4f, 0.7f, 1.0f, 1.0f},
                      "GPU-bound (CPU waited for GPU)");
    }
    else
    {
        igTextColored((ImVec4) {0.6f, 0.9f, 0.6f, 1.0f}, "CPU/GPU balanced");
    }

    igSpacing();

    // Shared y-axis scale so the three plots are directly comparable.
    float scale = total_max;
    if (cpu_max > scale)
        scale = cpu_max;
    if (gpu_max > scale)
        scale = gpu_max;
    if (g->ms_scale > 0.0f)
        scale = g->ms_scale;
    if (scale < 16.7f)
        scale = 16.7f;// always show at least the 60-fps line

    char total_overlay[64], cpu_overlay[64], gpu_overlay[64];
    snprintf(total_overlay, sizeof(total_overlay), "Total: %.2f ms",
             total_latest);
    snprintf(cpu_overlay, sizeof(cpu_overlay), "CPU:   %.2f ms", cpu_latest);
    snprintf(gpu_overlay, sizeof(gpu_overlay), "GPU:   %.2f ms", gpu_latest);

    const ImVec2 plot_size = (ImVec2) {0.0f, 60.0f};
    igPlotLines_FloatPtr("##frame_total", g->times, g->count, 0, total_overlay,
                         0.0f, scale, plot_size, (int) sizeof(float));
    igPlotLines_FloatPtr("##frame_cpu", g->cpu, g->count, 0, cpu_overlay, 0.0f,
                         scale, plot_size, (int) sizeof(float));
    igPlotLines_FloatPtr("##frame_gpu", g->gpu, g->count, 0, gpu_overlay, 0.0f,
                         scale, plot_size, (int) sizeof(float));

    igSpacing();
    igTextUnformatted("CPU breakdown (last frame, ms)", NULL);

    float perf_mesh = MikuPan_PerfGetSectionMs(MP_PERF_MESH_RENDER);
    float perf_sprite = MikuPan_PerfGetSectionMs(MP_PERF_SPRITE_RENDER);
    float perf_flush = MikuPan_PerfGetSectionMs(MP_PERF_BATCH_FLUSH);
    float perf_drawui = MikuPan_PerfGetSectionMs(MP_PERF_DRAWUI);
    float perf_render = MikuPan_PerfGetSectionMs(MP_PERF_RENDERUI);
    float perf_gs_up = MikuPan_GsGetUploadMs();
    float perf_gs_dl = MikuPan_GsGetDownloadMs();
    float perf_known = perf_mesh + perf_sprite + perf_flush + perf_drawui
                       + perf_render + perf_gs_up + perf_gs_dl;
    float perf_other = cpu_latest - perf_known;
    if (perf_other < 0.0f)
        perf_other = 0.0f;

    igText("Mesh render:      %5.2f ms", perf_mesh);

    // Per-mesh-type breakdown (sub-sections of MESH_RENDER). Sum ≈ perf_mesh
    // minus the dispatch / early-return overhead at the top of each renderer.
    {
        float perf_mesh_0x2 = MikuPan_PerfGetSectionMs(MP_PERF_MESH_0x2);
        float perf_mesh_0xA = MikuPan_PerfGetSectionMs(MP_PERF_MESH_0xA);
        float perf_mesh_0x10 = MikuPan_PerfGetSectionMs(MP_PERF_MESH_0x10);
        float perf_mesh_0x12 = MikuPan_PerfGetSectionMs(MP_PERF_MESH_0x12);
        float perf_mesh_0x32 = MikuPan_PerfGetSectionMs(MP_PERF_MESH_0x32);
        float perf_mesh_0x82 = MikuPan_PerfGetSectionMs(MP_PERF_MESH_0x82);
        igText("    0x2:          %5.2f ms", perf_mesh_0x2);
        igText("    0xA:          %5.2f ms", perf_mesh_0xA);
        igText("    0x10:         %5.2f ms", perf_mesh_0x10);
        igText("    0x12:         %5.2f ms", perf_mesh_0x12);
        igText("    0x32:         %5.2f ms", perf_mesh_0x32);
        igText("    0x82:         %5.2f ms", perf_mesh_0x82);
    }

    igText("Sprite/UI render: %5.2f ms", perf_sprite);
    igText("Batch flushes:    %5.2f ms", perf_flush);
    igText("DrawUi (game):    %5.2f ms", perf_drawui);
    igText("RenderUi (ImGui): %5.2f ms", perf_render);
    igText("GS uploads:       %5.2f ms", perf_gs_up);
    igText("GS downloads:     %5.2f ms", perf_gs_dl);
    igText("Other / unknown:  %5.2f ms", perf_other);

    igSpacing();
    igTextUnformatted("Cross-cutting (sliced through everything above):", NULL);
    float perf_draw = MikuPan_PerfGetSectionMs(MP_PERF_DRAW_SUBMIT);
    float perf_upload = MikuPan_PerfGetSectionMs(MP_PERF_BUFFER_UPLOAD);
    float perf_state = MikuPan_PerfGetSectionMs(MP_PERF_STATE_CHANGE);
    igText("  glDraw* submit: %5.2f ms (driver overhead per draw call)",
           perf_draw);
    igText("  Buffer uploads: %5.2f ms (map+memcpy+unmap)", perf_upload);
    igText(
        "  State changes:  %5.2f ms (shader/texture/render-state in cached "
        "path)",
        perf_state);

    // Per-call decomposition of "State changes" — tells you which of the four
    // calls (shader / texture / render-state / VAO) is the actual hot one.
    // Sum ≈ perf_state (a few hundred ns/frame of timer overhead aside).
    float perf_sc_shader = MikuPan_PerfGetSectionMs(MP_PERF_SC_SHADER);
    float perf_sc_texture = MikuPan_PerfGetSectionMs(MP_PERF_SC_TEXTURE);
    float perf_sc_rs3d = MikuPan_PerfGetSectionMs(MP_PERF_SC_RS3D);
    float perf_sc_vao = MikuPan_PerfGetSectionMs(MP_PERF_SC_VAO);
    igText(
        "      Shader:     %5.2f ms (glUseProgram / SetCurrentShaderProgram)",
        perf_sc_shader);
    igText("      Texture:    %5.2f ms (lookup + glBindTexture)",
           perf_sc_texture);
    {
        // L1 (tex0 → info) hit/miss. Misses fall through to MikuPan_GetTextureHash
        // which XXH3-hashes GS memory — usually the heaviest part of SC_TEXTURE.
        // L1 is wiped on every MikuPan_GsUpload, so post-upload frames re-hash.
        int l1_hits = MikuPan_PerfGetTexL1Hits();
        int l1_misses = MikuPan_PerfGetTexL1Misses();
        igText("        L1 hits=%d misses=%d (misses → XXH3 over GS memory)",
               l1_hits, l1_misses);

        // Per-step decomposition of SC_TEXTURE — tells you exactly which step
        // inside MikuPan_SetTexture is dominant. Sum ≈ perf_sc_texture minus a
        // small amount of dispatch overhead between the inner timers.
        float perf_tex_l1 = MikuPan_PerfGetSectionMs(MP_PERF_TEX_L1_LOOKUP);
        float perf_tex_hash = MikuPan_PerfGetSectionMs(MP_PERF_TEX_HASH);
        float perf_tex_l2 = MikuPan_PerfGetSectionMs(MP_PERF_TEX_L2_LOOKUP);
        float perf_tex_create = MikuPan_PerfGetSectionMs(MP_PERF_TEX_CREATE);
        float perf_tex_bind = MikuPan_PerfGetSectionMs(MP_PERF_TEX_BIND);
        igText("        L1 lookup:  %5.2f ms (tex0 → info hash table probe)",
               perf_tex_l1);
        igText(
            "        Hash:       %5.2f ms (XXH3 over GS memory — L1-miss only)",
            perf_tex_hash);
        igText("        L2 lookup:  %5.2f ms (hash → info)", perf_tex_l2);
        igText("        Create:     %5.2f ms (glTexImage2D + glGenerateMipmap)",
               perf_tex_create);
        igText("        Bind:       %5.2f ms (glBindTexture under cache)",
               perf_tex_bind);
    }
    igText("      RenderState:%5.2f ms (depth / cull / blend mode)",
           perf_sc_rs3d);
    igText("      VAO:        %5.2f ms (glBindVertexArray)", perf_sc_vao);

    igTextDisabled(
        "These are subsets of the buckets above, not additional time.");

    igSpacing();
    igTextUnformatted("GS texture traffic (last frame)", NULL);

    int ul_count = MikuPan_GsGetUploadCount();
    int ul_bytes = MikuPan_GsGetUploadBytes();
    int dl_count = MikuPan_GsGetDownloadCount();
    int dl_bytes = MikuPan_GsGetDownloadBytes();

    igText("Uploads:   %4d calls   %6.1f KB", ul_count, ul_bytes / 1024.0f);
    igText("Downloads: %4d calls   %6.1f KB", dl_count, dl_bytes / 1024.0f);

    igSpacing();
    igTextUnformatted("Frame-time histogram (ms)", NULL);

#define HIST_BUCKETS 16
    float histf[HIST_BUCKETS] = {0};
    float step = scale / (float) HIST_BUCKETS;
    int maxcount = 1;

    for (int i = 0; i < g->count; i++)
    {
        int b = (int) floorf(g->times[i] / step);
        if (b < 0)
            b = 0;
        if (b >= HIST_BUCKETS)
            b = HIST_BUCKETS - 1;
        histf[b] += 1.0f;
        if ((int) histf[b] > maxcount)
            maxcount = (int) histf[b];
    }

    igPlotHistogram_FloatPtr("##hist", histf, HIST_BUCKETS, 0, NULL, 0.0f,
                             (float) maxcount, (ImVec2) {0.0f, 60.0f},
                             (int) sizeof(float));

    igEnd();
}

// -- Texture list helpers ----------------------------------------------------
#define TEXTURE_LIST_MAX 4096

static MikuPan_TextureInfo *tex_list[TEXTURE_LIST_MAX];
static int tex_list_count = 0;

static void CollectTexture(MikuPan_TextureInfo *tex, void *ud)
{
    int *count = (int *) ud;
    if (*count < TEXTURE_LIST_MAX)
    {
        tex_list[(*count)++] = tex;
    }
}

static int CompareTextureById(const void *a, const void *b)
{
    const MikuPan_TextureInfo *ta = *(const MikuPan_TextureInfo **) a;
    const MikuPan_TextureInfo *tb = *(const MikuPan_TextureInfo **) b;
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
    if (!igBegin("Shader Reload", (bool *) &show_shader_reload, 0))
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
        const char *vp = MikuPan_GetShaderVertSource(i);
        const char *gp = MikuPan_GetShaderGeomSource(i);
        const char *fp = MikuPan_GetShaderFragSource(i);
        if (vp)
            igText("    vert: %s", vp);
        if (gp)
            igText("    geom: %s", gp);
        if (fp)
            igText("    frag: %s", fp);

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

static const char *GlPrimName(GLenum mode)
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
    if (!igBegin("Draw Call Inspector", (bool *) &show_draw_inspector, 0))
    {
        igEnd();
        return;
    }

    int enabled = MikuPan_DrawCapEnabled();
    if (igCheckbox("Capture", (bool *) &enabled))
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
        const MikuPan_DrawCapEntry *e = MikuPan_DrawCapLastEntryAt(i);
        if (e == NULL)
            break;

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

static const char *MikuPan_CameraDebugKindName(int kind)
{
    switch (kind)
    {
        case 0: return "Normal";
        case 1: return "Battle";
        case 2: return "Drama";
        case 3: return "Door";
        default: return "Unknown";
    }
}

static void MikuPan_CameraDebugVec3(const char *label,
                                    const sceVu0FVECTOR v,
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

    const CAMERA_DEBUG_PATH *path = CameraGetDebugPath();
    sceVu0FVECTOR view_vec = {
        camera.i[0] - camera.p[0],
        camera.i[1] - camera.p[1],
        camera.i[2] - camera.p[2],
        0.0f
    };
    float view_dist = sqrtf(view_vec[0] * view_vec[0] +
                            view_vec[1] * view_vec[1] +
                            view_vec[2] * view_vec[2]);

    igSetNextWindowSize((ImVec2) {360.0f, 0.0f}, ImGuiCond_FirstUseEver);
    if (!igBegin("Camera World Info", NULL, ImGuiWindowFlags_AlwaysAutoResize))
    {
        igEnd();
        return;
    }

    if (path->active)
    {
        igText("Map camera: %s  no=%u old=%u  type=%u",
               MikuPan_CameraDebugKindName(path->kind),
               path->no, path->no_old, path->type);
    }
    else
    {
        igText("Map camera: runtime/free camera");
    }

    igText("Distance camera -> interest: %.1f", view_dist);
    igSeparator();

    MikuPan_CameraDebugVec3("camera.p", camera.p,
                            (ImVec4){1.0f, 0.78f, 0.10f, 1.0f});
    MikuPan_CameraDebugVec3("camera.i", camera.i,
                            (ImVec4){0.15f, 1.0f, 0.35f, 1.0f});
    MikuPan_CameraDebugVec3("view line", view_vec,
                            (ImVec4){1.0f, 1.0f, 1.0f, 1.0f});

    if (path->active)
    {
        igSeparator();
        igTextColored((ImVec4){1.0f, 0.42f, 0.08f, 1.0f},
                      "authored camera path points: %u",
                      path->camera_path_points);
        igTextColored((ImVec4){0.05f, 0.80f, 1.0f, 1.0f},
                      "authored interest path points: %u",
                      path->interest_path_points);

        if (path->change || path->no != path->no_old)
        {
            igTextColored((ImVec4){1.0f, 0.10f, 0.30f, 1.0f},
                          "transition target active");
        }
    }

    igEnd();
}

// -- Public API --------------------------------------------------------------

static int CompareResolutionDesc(const void *a, const void *b)
{
    const MikuPan_Resolution *ra = (const MikuPan_Resolution *)a;
    const MikuPan_Resolution *rb = (const MikuPan_Resolution *)b;
    int area_a = ra->width * ra->height;
    int area_b = rb->width * rb->height;
    return area_b - area_a;
}

static void MikuPan_PopulateResolutionList(SDL_DisplayID display, const SDL_DisplayMode *current_mode)
{
    resolution_count = 0;

    int n = 0;
    SDL_DisplayMode **modes = SDL_GetFullscreenDisplayModes(display, &n);

    if (modes != NULL)
    {
        for (int i = 0; i < n && resolution_count < MIKUPAN_MAX_RESOLUTIONS; i++)
        {
            int w = modes[i]->w;
            int h = modes[i]->h;

            if (w <= 0 || h <= 0)
            {
                continue;
            }

            int dup = 0;

            for (int j = 0; j < resolution_count; j++)
            {
                if (resolution_list[j].width == w && resolution_list[j].height == h)
                {
                    dup = 1;
                    break;
                }
            }

            if (dup)
            {
                continue;
            }

            resolution_list[resolution_count].width = w;
            resolution_list[resolution_count].height = h;
            resolution_count++;
        }

        /// Add the normal render resolution of the game
        resolution_list[resolution_count].width = 640;
        resolution_list[resolution_count].height = 448;
        resolution_count++;

        SDL_free(modes);
    }

    // Always include the current desktop mode — older SDL backends sometimes
    // omit it from the fullscreen list.
    if (current_mode != NULL && current_mode->w > 0 && current_mode->h > 0)
    {
        int present = 0;
        for (int i = 0; i < resolution_count; i++)
        {
            if (resolution_list[i].width == current_mode->w &&
                resolution_list[i].height == current_mode->h)
            {
                present = 1;
                break;
            }
        }
        if (!present && resolution_count < MIKUPAN_MAX_RESOLUTIONS)
        {
            resolution_list[resolution_count].width = current_mode->w;
            resolution_list[resolution_count].height = current_mode->h;
            resolution_count++;
        }
    }

    qsort(resolution_list, resolution_count, sizeof(MikuPan_Resolution), CompareResolutionDesc);

    for (int i = 0; i < resolution_count; i++)
    {
        snprintf(resolution_labels[i], sizeof(resolution_labels[i]),
                 "%d x %d", resolution_list[i].width, resolution_list[i].height);

        resolution_label_ptrs[i] = resolution_labels[i];

        if (resolution_list[i].width == render_resolution_width &&
            resolution_list[i].height == render_resolution_height)
        {
            resolution_selected = i;
        }
    }
}

static void MikuPan_ApplyFatalFrameStyle(int theme)
{
    ImGuiStyle *s = igGetStyle();
    ImVec4 *c = s->Colors;

    s->Alpha             = 0.96f;
    s->DisabledAlpha     = 0.45f;
    s->WindowRounding    = 0.0f;
    s->ChildRounding     = 0.0f;
    s->PopupRounding     = 0.0f;
    s->FrameRounding     = 0.0f;
    s->ScrollbarRounding = 0.0f;
    s->GrabRounding      = 0.0f;
    s->TabRounding       = 0.0f;
    s->WindowBorderSize  = 1.0f;
    s->ChildBorderSize   = 1.0f;
    s->PopupBorderSize   = 1.0f;
    s->FrameBorderSize   = 1.0f;
    s->TabBorderSize     = 1.0f;
    s->WindowPadding     = (ImVec2){10.0f, 10.0f};
    s->FramePadding      = (ImVec2){ 8.0f,  4.0f};
    s->ItemSpacing       = (ImVec2){ 8.0f,  5.0f};
    s->ItemInnerSpacing  = (ImVec2){ 6.0f,  4.0f};
    s->IndentSpacing     = 18.0f;
    s->ScrollbarSize     = 14.0f;
    s->GrabMinSize       = 10.0f;

    switch (theme)
    {
        default:
        case 0:
            // Text: pale paper with a hint of purple-blue
            c[ImGuiCol_Text]                  = (ImVec4){0.78f, 0.82f, 0.88f, 1.00f};
            c[ImGuiCol_TextDisabled]          = (ImVec4){0.32f, 0.36f, 0.42f, 1.00f};

            // Background: deep blue-black like moonlit mansion
            c[ImGuiCol_WindowBg]              = (ImVec4){0.04f, 0.06f, 0.10f, 0.96f};
            c[ImGuiCol_ChildBg]               = (ImVec4){0.06f, 0.08f, 0.12f, 0.85f};
            c[ImGuiCol_PopupBg]               = (ImVec4){0.03f, 0.05f, 0.08f, 0.98f};

            // Borders: slate with a cold purple lean
            c[ImGuiCol_Border]                = (ImVec4){0.24f, 0.28f, 0.35f, 0.65f};
            c[ImGuiCol_BorderShadow]          = (ImVec4){0, 0, 0, 0};

            // Frames: cool gray-blue
            c[ImGuiCol_FrameBg]               = (ImVec4){0.09f, 0.12f, 0.16f, 0.95f};
            c[ImGuiCol_FrameBgHovered]        = (ImVec4){0.16f, 0.21f, 0.28f, 0.95f};
            c[ImGuiCol_FrameBgActive]         = (ImVec4){0.23f, 0.30f, 0.40f, 1.00f};

            // Title bars: dark with cold highlights
            c[ImGuiCol_TitleBg]               = (ImVec4){0.05f, 0.07f, 0.10f, 1.00f};
            c[ImGuiCol_TitleBgActive]         = (ImVec4){0.10f, 0.14f, 0.18f, 1.00f};
            c[ImGuiCol_TitleBgCollapsed]      = (ImVec4){0.03f, 0.05f, 0.08f, 0.85f};

            c[ImGuiCol_MenuBarBg]             = (ImVec4){0.07f, 0.09f, 0.12f, 1.00f};

            // Scrollbars: soft azure
            c[ImGuiCol_ScrollbarBg]           = (ImVec4){0.03f, 0.04f, 0.06f, 0.85f};
            c[ImGuiCol_ScrollbarGrab]         = (ImVec4){0.18f, 0.24f, 0.30f, 1.00f};
            c[ImGuiCol_ScrollbarGrabHovered]  = (ImVec4){0.26f, 0.34f, 0.42f, 1.00f};
            c[ImGuiCol_ScrollbarGrabActive]   = (ImVec4){0.34f, 0.44f, 0.54f, 1.00f};

            // Accent: ghost-light cyan / pale purple
            c[ImGuiCol_CheckMark]             = (ImVec4){0.60f, 0.78f, 0.96f, 1.00f};
            c[ImGuiCol_SliderGrab]            = (ImVec4){0.42f, 0.64f, 0.88f, 1.00f};
            c[ImGuiCol_SliderGrabActive]      = (ImVec4){0.60f, 0.84f, 0.98f, 1.00f};

            // Buttons: twilight blue-gray
            c[ImGuiCol_Button]                = (ImVec4){0.08f, 0.10f, 0.14f, 0.95f};
            c[ImGuiCol_ButtonHovered]         = (ImVec4){0.18f, 0.24f, 0.32f, 1.00f};
            c[ImGuiCol_ButtonActive]          = (ImVec4){0.28f, 0.36f, 0.48f, 1.00f};

            // Headers
            c[ImGuiCol_Header]                = (ImVec4){0.12f, 0.16f, 0.22f, 0.85f};
            c[ImGuiCol_HeaderHovered]         = (ImVec4){0.20f, 0.28f, 0.38f, 0.95f};
            c[ImGuiCol_HeaderActive]          = (ImVec4){0.28f, 0.38f, 0.52f, 1.00f};

            // Tabs
            c[ImGuiCol_Tab]                   = (ImVec4){0.08f, 0.10f, 0.14f, 0.95f};
            c[ImGuiCol_TabHovered]            = (ImVec4){0.20f, 0.28f, 0.38f, 0.95f};
            c[ImGuiCol_TabSelected]           = (ImVec4){0.26f, 0.34f, 0.46f, 1.00f};
            c[ImGuiCol_TabSelectedOverline]   = (ImVec4){0.60f, 0.78f, 0.96f, 1.00f};

            // Misc accents (plots, nav, tables, etc.)
            c[ImGuiCol_TextLink]              = (ImVec4){0.60f, 0.78f, 0.96f, 1.00f};
            c[ImGuiCol_TextSelectedBg]        = (ImVec4){0.25f, 0.32f, 0.42f, 0.65f};
            c[ImGuiCol_TableHeaderBg]         = (ImVec4){0.08f, 0.12f, 0.16f, 1.00f};
            c[ImGuiCol_TableBorderStrong]     = (ImVec4){0.22f, 0.28f, 0.34f, 1.00f};
            c[ImGuiCol_TableBorderLight]      = (ImVec4){0.14f, 0.18f, 0.24f, 1.00f};
            break;
        case 1:
            // Text: pale paper, slightly cold
            c[ImGuiCol_Text]                  = (ImVec4){0.82f, 0.86f, 0.88f, 1.00f};
            c[ImGuiCol_TextDisabled]          = (ImVec4){0.38f, 0.42f, 0.45f, 1.00f};

            // Backgrounds: blue-black ink
            c[ImGuiCol_WindowBg]              = (ImVec4){0.05f, 0.07f, 0.09f, 0.96f};
            c[ImGuiCol_ChildBg]               = (ImVec4){0.07f, 0.09f, 0.11f, 0.85f};
            c[ImGuiCol_PopupBg]               = (ImVec4){0.04f, 0.06f, 0.08f, 0.98f};

            // Borders: cold slate
            c[ImGuiCol_Border]                = (ImVec4){0.22f, 0.28f, 0.32f, 0.65f};
            c[ImGuiCol_BorderShadow]          = (ImVec4){0,0,0,0};

            // Frames: dusty wood in moonlight
            c[ImGuiCol_FrameBg]               = (ImVec4){0.10f, 0.13f, 0.16f, 0.95f};
            c[ImGuiCol_FrameBgHovered]        = (ImVec4){0.18f, 0.24f, 0.28f, 0.95f};
            c[ImGuiCol_FrameBgActive]         = (ImVec4){0.26f, 0.34f, 0.40f, 1.00f};

            // Titles: very dark blue-black
            c[ImGuiCol_TitleBg]               = (ImVec4){0.06f, 0.08f, 0.10f, 1.00f};
            c[ImGuiCol_TitleBgActive]         = (ImVec4){0.12f, 0.18f, 0.22f, 1.00f};
            c[ImGuiCol_TitleBgCollapsed]      = (ImVec4){0.05f, 0.07f, 0.09f, 0.85f};

            c[ImGuiCol_MenuBarBg]             = (ImVec4){0.08f, 0.10f, 0.12f, 1.00f};

            // Scrollbar: faint blue steel
            c[ImGuiCol_ScrollbarBg]           = (ImVec4){0.04f, 0.05f, 0.07f, 0.85f};
            c[ImGuiCol_ScrollbarGrab]         = (ImVec4){0.22f, 0.30f, 0.36f, 1.00f};
            c[ImGuiCol_ScrollbarGrabHovered]  = (ImVec4){0.30f, 0.40f, 0.46f, 1.00f};
            c[ImGuiCol_ScrollbarGrabActive]   = (ImVec4){0.38f, 0.50f, 0.58f, 1.00f};

            // Accent color: ghostly cyan (FF1 signature)
            c[ImGuiCol_CheckMark]             = (ImVec4){0.60f, 0.85f, 0.90f, 1.00f};
            c[ImGuiCol_SliderGrab]            = (ImVec4){0.50f, 0.78f, 0.84f, 1.00f};
            c[ImGuiCol_SliderGrabActive]      = (ImVec4){0.70f, 0.92f, 0.98f, 1.00f};

            // Buttons: foggy slate
            c[ImGuiCol_Button]                = (ImVec4){0.12f, 0.16f, 0.20f, 0.95f};
            c[ImGuiCol_ButtonHovered]         = (ImVec4){0.22f, 0.30f, 0.36f, 1.00f};
            c[ImGuiCol_ButtonActive]          = (ImVec4){0.32f, 0.42f, 0.50f, 1.00f};

            // Headers
            c[ImGuiCol_Header]                = (ImVec4){0.14f, 0.20f, 0.24f, 0.85f};
            c[ImGuiCol_HeaderHovered]         = (ImVec4){0.24f, 0.34f, 0.40f, 0.95f};
            c[ImGuiCol_HeaderActive]          = (ImVec4){0.34f, 0.46f, 0.54f, 1.00f};

            // Separators
            c[ImGuiCol_Separator]             = (ImVec4){0.22f, 0.28f, 0.32f, 0.55f};
            c[ImGuiCol_SeparatorHovered]      = (ImVec4){0.40f, 0.52f, 0.60f, 0.85f};
            c[ImGuiCol_SeparatorActive]       = (ImVec4){0.55f, 0.70f, 0.80f, 1.00f};

            // Tabs
            c[ImGuiCol_Tab]                   = (ImVec4){0.10f, 0.14f, 0.18f, 0.95f};
            c[ImGuiCol_TabHovered]            = (ImVec4){0.24f, 0.34f, 0.40f, 0.95f};
            c[ImGuiCol_TabSelected]           = (ImVec4){0.30f, 0.42f, 0.50f, 1.00f};
            c[ImGuiCol_TabSelectedOverline]   = (ImVec4){0.65f, 0.90f, 0.95f, 1.00f};

            // Tables / misc accents
            c[ImGuiCol_TableHeaderBg]         = (ImVec4){0.12f, 0.18f, 0.22f, 1.00f};
            c[ImGuiCol_TableBorderStrong]     = (ImVec4){0.22f, 0.30f, 0.36f, 1.00f};
            c[ImGuiCol_TableBorderLight]      = (ImVec4){0.16f, 0.22f, 0.26f, 1.00f};
            c[ImGuiCol_TextLink]              = (ImVec4){0.65f, 0.90f, 0.95f, 1.00f};
            c[ImGuiCol_TextSelectedBg]        = (ImVec4){0.30f, 0.42f, 0.50f, 0.65f};

            // Nav / modal
            c[ImGuiCol_NavCursor]             = (ImVec4){0.65f, 0.90f, 0.95f, 1.00f};
            c[ImGuiCol_NavWindowingHighlight] = (ImVec4){0.65f, 0.90f, 0.95f, 0.70f};
            c[ImGuiCol_NavWindowingDimBg]     = (ImVec4){0.04f, 0.05f, 0.07f, 0.50f};
            c[ImGuiCol_ModalWindowDimBg]      = (ImVec4){0.04f, 0.05f, 0.07f, 0.65f};
            break;
        case 2:
            c[ImGuiCol_Text]                  = (ImVec4){0.86f, 0.78f, 0.62f, 1.00f};
            c[ImGuiCol_TextDisabled]          = (ImVec4){0.42f, 0.36f, 0.28f, 1.00f};
            c[ImGuiCol_WindowBg]              = (ImVec4){0.06f, 0.04f, 0.03f, 0.96f};
            c[ImGuiCol_ChildBg]               = (ImVec4){0.08f, 0.05f, 0.04f, 0.85f};
            c[ImGuiCol_PopupBg]               = (ImVec4){0.05f, 0.03f, 0.02f, 0.98f};
            c[ImGuiCol_Border]                = (ImVec4){0.40f, 0.10f, 0.08f, 0.65f};
            c[ImGuiCol_BorderShadow]          = (ImVec4){0.00f, 0.00f, 0.00f, 0.00f};
            c[ImGuiCol_FrameBg]               = (ImVec4){0.12f, 0.07f, 0.05f, 0.95f};
            c[ImGuiCol_FrameBgHovered]        = (ImVec4){0.32f, 0.10f, 0.08f, 0.85f};
            c[ImGuiCol_FrameBgActive]         = (ImVec4){0.52f, 0.12f, 0.10f, 0.95f};
            c[ImGuiCol_TitleBg]               = (ImVec4){0.08f, 0.04f, 0.03f, 1.00f};
            c[ImGuiCol_TitleBgActive]         = (ImVec4){0.35f, 0.05f, 0.05f, 1.00f};
            c[ImGuiCol_TitleBgCollapsed]      = (ImVec4){0.05f, 0.02f, 0.02f, 0.85f};
            c[ImGuiCol_MenuBarBg]             = (ImVec4){0.10f, 0.05f, 0.04f, 1.00f};
            c[ImGuiCol_ScrollbarBg]           = (ImVec4){0.04f, 0.02f, 0.02f, 0.85f};
            c[ImGuiCol_ScrollbarGrab]         = (ImVec4){0.28f, 0.16f, 0.10f, 1.00f};
            c[ImGuiCol_ScrollbarGrabHovered]  = (ImVec4){0.50f, 0.20f, 0.12f, 1.00f};
            c[ImGuiCol_ScrollbarGrabActive]   = (ImVec4){0.65f, 0.22f, 0.14f, 1.00f};
            c[ImGuiCol_CheckMark]             = (ImVec4){0.85f, 0.40f, 0.18f, 1.00f};
            c[ImGuiCol_SliderGrab]            = (ImVec4){0.60f, 0.20f, 0.14f, 1.00f};
            c[ImGuiCol_SliderGrabActive]      = (ImVec4){0.80f, 0.30f, 0.16f, 1.00f};
            c[ImGuiCol_Button]                = (ImVec4){0.18f, 0.08f, 0.06f, 0.95f};
            c[ImGuiCol_ButtonHovered]         = (ImVec4){0.45f, 0.12f, 0.10f, 1.00f};
            c[ImGuiCol_ButtonActive]          = (ImVec4){0.65f, 0.16f, 0.12f, 1.00f};
            c[ImGuiCol_Header]                = (ImVec4){0.22f, 0.08f, 0.06f, 0.85f};
            c[ImGuiCol_HeaderHovered]         = (ImVec4){0.45f, 0.12f, 0.10f, 0.95f};
            c[ImGuiCol_HeaderActive]          = (ImVec4){0.60f, 0.14f, 0.10f, 1.00f};
            c[ImGuiCol_Separator]             = (ImVec4){0.40f, 0.12f, 0.10f, 0.55f};
            c[ImGuiCol_SeparatorHovered]      = (ImVec4){0.65f, 0.18f, 0.12f, 0.85f};
            c[ImGuiCol_SeparatorActive]       = (ImVec4){0.85f, 0.24f, 0.14f, 1.00f};
            c[ImGuiCol_ResizeGrip]            = (ImVec4){0.28f, 0.12f, 0.08f, 0.50f};
            c[ImGuiCol_ResizeGripHovered]     = (ImVec4){0.55f, 0.16f, 0.10f, 0.85f};
            c[ImGuiCol_ResizeGripActive]      = (ImVec4){0.80f, 0.22f, 0.14f, 1.00f};
            c[ImGuiCol_Tab]                   = (ImVec4){0.14f, 0.06f, 0.04f, 0.95f};
            c[ImGuiCol_TabHovered]            = (ImVec4){0.45f, 0.12f, 0.10f, 0.95f};
            c[ImGuiCol_TabSelected]           = (ImVec4){0.55f, 0.14f, 0.10f, 1.00f};
            c[ImGuiCol_TabSelectedOverline]   = (ImVec4){0.85f, 0.25f, 0.14f, 1.00f};
            c[ImGuiCol_TabDimmed]             = (ImVec4){0.10f, 0.05f, 0.03f, 0.95f};
            c[ImGuiCol_TabDimmedSelected]     = (ImVec4){0.30f, 0.08f, 0.06f, 1.00f};
            c[ImGuiCol_TabDimmedSelectedOverline] = (ImVec4){0.55f, 0.14f, 0.10f, 1.00f};
            c[ImGuiCol_PlotLines]             = (ImVec4){0.85f, 0.50f, 0.18f, 1.00f};
            c[ImGuiCol_PlotLinesHovered]      = (ImVec4){1.00f, 0.55f, 0.20f, 1.00f};
            c[ImGuiCol_PlotHistogram]         = (ImVec4){0.65f, 0.20f, 0.14f, 1.00f};
            c[ImGuiCol_PlotHistogramHovered]  = (ImVec4){0.90f, 0.30f, 0.18f, 1.00f};
            c[ImGuiCol_TableHeaderBg]         = (ImVec4){0.18f, 0.08f, 0.06f, 1.00f};
            c[ImGuiCol_TableBorderStrong]     = (ImVec4){0.40f, 0.12f, 0.10f, 1.00f};
            c[ImGuiCol_TableBorderLight]      = (ImVec4){0.25f, 0.10f, 0.08f, 1.00f};
            c[ImGuiCol_TableRowBg]            = (ImVec4){0.00f, 0.00f, 0.00f, 0.00f};
            c[ImGuiCol_TableRowBgAlt]         = (ImVec4){0.95f, 0.50f, 0.18f, 0.04f};
            c[ImGuiCol_TextLink]              = (ImVec4){0.95f, 0.50f, 0.18f, 1.00f};
            c[ImGuiCol_TextSelectedBg]        = (ImVec4){0.55f, 0.14f, 0.10f, 0.65f};
            c[ImGuiCol_DragDropTarget]        = (ImVec4){0.95f, 0.50f, 0.18f, 0.90f};
            c[ImGuiCol_NavCursor]             = (ImVec4){0.85f, 0.25f, 0.14f, 1.00f};
            c[ImGuiCol_NavWindowingHighlight] = (ImVec4){0.95f, 0.50f, 0.18f, 0.70f};
            c[ImGuiCol_NavWindowingDimBg]     = (ImVec4){0.04f, 0.02f, 0.02f, 0.50f};
            c[ImGuiCol_ModalWindowDimBg]      = (ImVec4){0.04f, 0.02f, 0.02f, 0.65f};
            break;
    }
}

void MikuPan_InitUi(SDL_Window *window, SDL_GLContext renderer)
{
    igCreateContext(NULL);
    ImGuiIO *io = igGetIO_Nil();
    io->ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard | ImGuiConfigFlags_NavEnableGamepad;

    MikuPan_ApplyFatalFrameStyle(mikupan_configuration.selected_theme);

    SDL_DisplayID primary = SDL_GetPrimaryDisplay();
    if (primary != 0)
    {
        const SDL_DisplayMode *mode = SDL_GetCurrentDisplayMode(primary);
        render_resolution_width = mikupan_configuration.renderer.render.width;
        render_resolution_height = mikupan_configuration.renderer.render.height;

        MikuPan_PopulateResolutionList(primary, mode);
    }

    msaa_samples = mikupan_configuration.renderer.msaa_index;
    brightness = mikupan_configuration.renderer.brightness;
    gamma_value = mikupan_configuration.renderer.gamma;
    is_fullscreen = mikupan_configuration.renderer.is_fullscreen;
    mesh_lighting_mode = mikupan_configuration.renderer.lighting_mode;
    is_vsync = mikupan_configuration.renderer.vsync;
    crt_settings = mikupan_configuration.crt;
    MikuPan_ClampCrtSettings(&crt_settings);
    mikupan_configuration.crt = crt_settings;

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

    MikuPan_ImGui_ImplInit(window, renderer);
}

void MikuPan_RenderUi(void)
{
    igRender();
    ImGuiIO *io = igGetIO_Nil();
    glad_glViewport(0, 0, (int) io->DisplaySize.x, (int) io->DisplaySize.y);
    MikuPan_ImGui_ImplRenderDrawData();
}

void MikuPan_StartFrameUi(void)
{
    MikuPan_ImGui_ImplNewFrame();
    igNewFrame();
}

void MikuPan_DrawUi(void)
{
    ImGuiIO *io = igGetIO_Nil();
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

    if (show_fps)
    {
        SetString2(0x10, 0.0f, 420.0f, 1, 0x80, 0x80, 0x80, (char *) "FPS %d",
                   (int) MikuPan_GetFrameRate());
    }
}

void MikuPan_ShutDownUi(void)
{
    MikuPan_ImGui_ImplShutdown();
    igDestroyContext(NULL);
}

void MikuPan_ProcessEventUi(SDL_Event *event)
{
    MikuPan_ImGui_ImplProcessEvent(event);
}

float MikuPan_GetFrameRate(void)
{
    ImGuiIO *io = igGetIO_Nil();
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
    tex_list_count = 0;
    MikuPan_ForEachTexture(CollectTexture, &tex_list_count);

    qsort(tex_list, tex_list_count, sizeof(MikuPan_TextureInfo *),
          CompareTextureById);

    igBegin("OpenGL Texture", NULL, 0);

    for (int i = 0; i < tex_list_count; i++)
    {
        char label[64];
        snprintf(label, sizeof(label), "Texture ID %u", tex_list[i]->id);

        if (igCollapsingHeader_TreeNodeFlags(label, 0))
        {
            igText("%u: %d x %d", tex_list[i]->id, tex_list[i]->width,
                   tex_list[i]->height);
            igImage(
                (ImTextureRef_c) {
                    ._TexID = (ImTextureID) (uintptr_t) tex_list[i]->id},
                (ImVec2) {(float) tex_list[i]->width,
                          (float) tex_list[i]->height},
                (ImVec2) {0.0f, 0.0f}, (ImVec2) {1.0f, 1.0f});
        }
    }

    igEnd();
}

void MikuPan_UiHandleShortcuts(void)
{
    if (igIsKeyPressed_Bool(ImGuiKey_F1, 0) ||
        (igIsKeyPressed_Bool(ImGuiKey_GamepadL3, 1) && igIsKeyPressed_Bool(ImGuiKey_GamepadStart, 1)))
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
        MikuPan_ReloadAllShaders(last_reload_error, (int) sizeof(last_reload_error));
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

void MikuPan_UiMenuBar(void)
{
    if (!show_menu_bar || !igBeginMainMenuBar())
    {
        return;
    }

    if (igBeginMenu("Debug", 1))
    {
        igCheckbox("Ingame Debug Menu", (bool *) &dbg_wrk.mode_on);
        igCheckbox("Shader Reload", (bool *) &show_shader_reload);
        igCheckbox("Draw Call Inspector", (bool *) &show_draw_inspector);
        igCheckbox("Camera World Info", (bool *) &show_camera_debug);

        igEndMenu();
    }

    if (igBeginMenu("Rendering", 1))
    {
        igCheckbox("Wireframe", (bool *) &render_wireframe);
        igCheckbox("Disable Lighting", (bool *) &disable_lighting);
        igCheckbox("Static Lighting", (bool *) &show_static_lighting);
        const char *lighting_modes[] = {"Per-Fragment", "Per-Vertex"};
        igCombo_Str_arr("Lighting Mode", &mesh_lighting_mode, lighting_modes, 2, -1);

        igCheckbox("Normals", (bool *) &render_normals);
        if (render_normals)
        {
            igSliderFloat("Normal Length", &normal_length, 0.1f, 100.0f, "%.1f", 0);
        }

        igCheckbox("GS Uploads", (bool *) &disable_gs_uploads);

        int shadows_on = MikuPan_IsShadowEnabled();
        if (igCheckbox("Shadows", (bool *) &shadows_on))
        {
            MikuPan_SetShadowEnabled(shadows_on);
        }

        igCheckbox("Textures", (bool *) &show_texture_list);
        igCheckbox("BoundingBox", (bool *) &show_bounding_boxes);

        if (igBeginMenu("Meshes", 1))
        {
            igCheckbox("Mesh 0x82", (bool *) &show_mesh_0x82);
            igCheckbox("Mesh 0x32", (bool *) &show_mesh_0x32);
            igCheckbox("Mesh 0x12", (bool *) &show_mesh_0x12);
            igCheckbox("Mesh 0x10", (bool *) &show_mesh_0x10);
            igCheckbox("Mesh 0x2", (bool *) &show_mesh_0x2);
            igEndMenu();
        }

        if (igButton("Clear Texture Cache", (ImVec2) {0.0f, 0.0f}))
        {
            MikuPan_RequestFlushTextureCache();
        }

        int mesh_cache_on = MikuPan_MeshCache_IsEnabled();
        if (igCheckbox("Mesh Cache", (bool *) &mesh_cache_on))
        {
            MikuPan_MeshCache_SetEnabled(mesh_cache_on);
        }

        if (igButton("Clear Mesh Cache", (ImVec2) {0.0f, 0.0f}))
        {
            MikuPan_MeshCache_Flush();
        }

        if (igMenuItem_Bool("Take Screenshot (F12)", "F12", false, true))
        {
            MikuPan_ScreenshotRequest();
        }

        igEndMenu();
    }

    if (igBeginMenu("Display", 1))
    {
        igCheckbox("Fullscreen", (bool*)&is_fullscreen);
        igCheckbox("VSync", (bool*)&is_vsync);

        char msaa_dropdown_list[32];
        snprintf(msaa_dropdown_list, sizeof(msaa_dropdown_list), "%d", msaa_list[msaa_samples]);

        if (resolution_count > 0)
        {
            if (igCombo_Str_arr("Resolution", &resolution_selected,
                                resolution_label_ptrs, resolution_count, -1))
            {
                render_resolution_width  = resolution_list[resolution_selected].width;
                render_resolution_height = resolution_list[resolution_selected].height;
            }
        }
        else
        {
            igTextDisabled("Resolution: no display modes available");
        }

        if (igBeginCombo("MSAA", msaa_dropdown_list, 0))
        {
            for (int i = 0; i < 6; i++)
            {
                bool is_selected = (msaa_samples == i);
                snprintf(msaa_dropdown_list, sizeof(msaa_dropdown_list), "%d", msaa_list[i]);

                if (igSelectable_Bool(msaa_dropdown_list, is_selected, 0, (ImVec2){0,0}))
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

        igSliderFloat("Brightness", &brightness,  0.0f, 2.0f, "%.2f", 0);
        igSliderFloat("Gamma",      &gamma_value, 0.1f, 3.0f, "%.2f", 0);

        if (igMenuItem_Bool("Save Configuration", NULL, false, true))
        {
            MikuPan_UiSaveConfiguration();
        }

        if (config_save_status[0] != '\0')
        {
            igTextDisabled("%s", config_save_status);
        }

        if (igMenuItem_Bool("Take Screenshot (F12)", NULL, false, true))
        {
            MikuPan_ScreenshotRequest();
        }

        igEndMenu();
    }

    if (igBeginMenu("CRT", 1))
    {
        igCheckbox("Enabled", (bool *) &crt_settings.enabled);
        igSliderFloat("Strength", &crt_settings.strength, 0.0f, 1.0f, "%.2f", 0);
        igSliderFloat("Curvature", &crt_settings.curvature, 0.0f, 0.30f, "%.3f", 0);
        igSliderFloat("Overscan", &crt_settings.overscan, 0.0f, 0.12f, "%.3f", 0);

        igSeparator();
        igSliderFloat("Scanline Strength", &crt_settings.scanline_strength,
                      0.0f, 1.0f, "%.2f", 0);
        igSliderFloat("Scanline Scale", &crt_settings.scanline_scale,
                      0.25f, 3.0f, "%.2f", 0);
        igSliderFloat("Scanline Thickness", &crt_settings.scanline_thickness,
                      0.5f, 4.0f, "%.2f", 0);

        igSeparator();
        igSliderFloat("Mask Strength", &crt_settings.mask_strength,
                      0.0f, 1.0f, "%.2f", 0);
        igSliderFloat("Mask Scale", &crt_settings.mask_scale,
                      0.5f, 4.0f, "%.2f", 0);

        igSeparator();
        igSliderFloat("Vignette Strength", &crt_settings.vignette_strength,
                      0.0f, 1.0f, "%.2f", 0);
        igSliderFloat("Vignette Size", &crt_settings.vignette_size,
                      0.25f, 1.25f, "%.2f", 0);
        igSliderFloat("Chroma Offset", &crt_settings.chroma_offset,
                      0.0f, 3.0f, "%.2f", 0);
        igSliderFloat("Blend Strength", &crt_settings.blend_strength,
                      0.0f, 1.0f, "%.2f", 0);
        igSliderFloat("Blend Radius", &crt_settings.blend_radius,
                      0.0f, 3.0f, "%.2f", 0);
        igSliderFloat("Noise", &crt_settings.noise_strength,
                      0.0f, 0.15f, "%.3f", 0);
        igSliderFloat("Flicker", &crt_settings.flicker_strength,
                      0.0f, 0.10f, "%.3f", 0);
        igSliderFloat("Glow", &crt_settings.glow_strength,
                      0.0f, 0.50f, "%.2f", 0);

        MikuPan_ClampCrtSettings(&crt_settings);

        if (igButton("Reset CRT", (ImVec2) {0.0f, 0.0f}))
        {
            crt_settings = crt_defaults;
        }

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

    if (igBeginMenu("Performance", 1))
    {
        igCheckbox("FPS Counter", (bool *) &show_fps);
        igCheckbox("Frame Time Graph", (bool *) &show_frame_time_graph);
        igEndMenu();
    }

    if (igBeginMenu("Input", 1))
    {
        igCheckbox("Controller / Joystick Mapping", (bool *)&show_controller_remap);

        if (igMenuItem_Bool("Reset Bindings to Defaults", NULL, false, true))
        {
            MikuPan_ControllerResetBindings();
        }

        igEndMenu();
    }

    igEndMainMenuBar();
}

int MikuPan_IsBoundingBoxRendering(void)
{
    return show_bounding_boxes;
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

const MikuPan_ConfigCrt *MikuPan_GetCrtSettings(void)
{
    return &crt_settings;
}

int MikuPan_IsFullScreen(void)
{
    return is_fullscreen;
}

int MikuPan_IsVsync(void)
{
    return is_vsync;
}
