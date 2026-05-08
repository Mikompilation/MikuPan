#define CIMGUI_DEFINE_ENUMS_AND_STRUCTS
#include "cimgui.h"

#include "typedefs.h"
#include "mikupan/ui/mikupan_ui.h"
#include "mikupan/gs/mikupan_texture_manager_c.h"
#include "mikupan/rendering/mikupan_shader.h"
#include "mikupan/rendering/mikupan_profiler.h"
#include "mikupan/rendering/mikupan_renderer.h"
#include "glad/gl.h"
#include "main/glob.h"
#include "graphics/graph2d/g2d_debug.h"
#include "graphics/graph2d/message.h"
#include "sce/libpad.h"

#include <SDL3/SDL_gamepad.h>
#include <SDL3/SDL_keyboard.h>
#include <SDL3/SDL_scancode.h>

#include <math.h>
#include <string.h>
#include <stdio.h>

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
int   MikuPan_PerfGetTexL1Hits(void);
int   MikuPan_PerfGetTexL1Misses(void);

// CPU section IDs (must match MikuPan_PerfSection in mikupan_profiler.h)
#define MP_PERF_MESH_RENDER   0
#define MP_PERF_SPRITE_RENDER 1
#define MP_PERF_BATCH_FLUSH   2
#define MP_PERF_DRAWUI        3
#define MP_PERF_RENDERUI      4
#define MP_PERF_DRAW_SUBMIT   5
#define MP_PERF_BUFFER_UPLOAD 6
#define MP_PERF_STATE_CHANGE  7
// Sub-sections decomposing MP_PERF_STATE_CHANGE (sum ≈ STATE_CHANGE).
#define MP_PERF_SC_SHADER     8
#define MP_PERF_SC_TEXTURE    9
#define MP_PERF_SC_RS3D       10
#define MP_PERF_SC_VAO        11
// Per-mesh-type sub-sections decomposing MP_PERF_MESH_RENDER.
#define MP_PERF_MESH_0x2      12
#define MP_PERF_MESH_0xA      13
#define MP_PERF_MESH_0x10     14
#define MP_PERF_MESH_0x12     15
#define MP_PERF_MESH_0x32     16
#define MP_PERF_MESH_0x82     17
// Sub-sections decomposing MP_PERF_SC_TEXTURE — fine-grained breakdown of
// where MikuPan_SetTexture spends its time.
#define MP_PERF_TEX_L1_LOOKUP 18
#define MP_PERF_TEX_HASH      19
#define MP_PERF_TEX_L2_LOOKUP 20
#define MP_PERF_TEX_CREATE    21
#define MP_PERF_TEX_BIND      22

// -- GS instrumentation (defined in mikupan_gs.cpp) --------------------------
int   MikuPan_GsGetUploadCount(void);
int   MikuPan_GsGetUploadBytes(void);
float MikuPan_GsGetUploadMs(void);
int   MikuPan_GsGetDownloadCount(void);
int   MikuPan_GsGetDownloadBytes(void);
float MikuPan_GsGetDownloadMs(void);

// -- State -------------------------------------------------------------------
int show_fps = 1;
int show_menu_bar = 0;

static int show_frame_time_graph = 0;
static int render_wireframe = 0;
static int render_normals = 0;
static int msaa_samples = 0;

// Render resolution defaults to the primary display's current mode and the
// sliders cap there. MikuPan_InitUi queries SDL once at startup and fills in
// these values; until that runs they fall back to a sensible 1080p.
static int render_resolution_width  = 1920;
static int render_resolution_height = 1080;
static int screen_resolution_width  = 1920;
static int screen_resolution_height = 1080;
static int show_texture_list = 0;
static int show_controller_remap = 0;
static int show_shader_reload = 0;
static int show_draw_inspector = 0;

// Last reload error (per-shader: index 0..MAX-1; index MAX = "reload all")
// — kept around so the user can read why the last reload failed without it
// flying past in the log.
static char last_reload_error[1280] = {0};

// Remap target state — when >= 0 we're listening for the next physical input
// to bind to that PS2 button index. `remap_target_kb` selects which mapping
// table the rebind writes to (0 = sce_gp_map, 1 = sce_kb_map).
static int remap_target = -1;
static int remap_target_kb = 0;

static int show_bounding_boxes = 0;
static int show_mesh_0x82 = 1;
static int show_mesh_0x32 = 1;
static int show_mesh_0x12 = 1;
static int show_mesh_0x2 = 1;
static int disable_lighting = 0;

static float light_color[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
static float normal_length = 10.0f;

// Post-process tone controls — applied by the POSTPROCESS_SHADER on the
// final scene-to-window blit. Defaults are no-ops (1.0 brightness, 1.0
// gamma); the renderer reads them via the getters below each frame and
// pushes them as uniforms.
static float brightness  = 1.0f;
static float gamma_value = 1.0f;

// -- FrameTimeGraph ----------------------------------------------------------
#define FRAME_GRAPH_CAPACITY 600

typedef struct
{
    float times[FRAME_GRAPH_CAPACITY];  ///< total wall-clock per frame (ms)
    float cpu  [FRAME_GRAPH_CAPACITY];  ///< CPU command-submission time (ms)
    float gpu  [FRAME_GRAPH_CAPACITY];  ///< GPU execution time via GL_TIME_ELAPSED (ms)
    int count;
    int max_samples;
    float ms_scale;
} FrameTimeGraph;

static FrameTimeGraph g_frame_graph = {
    .count = 0,
    .max_samples = 600,
    .ms_scale = -1.0f
};

static void FrameTimeGraph_Update(FrameTimeGraph *g, float total_ms, float cpu_ms, float gpu_ms)
{
    if (g->count >= g->max_samples)
    {
        memmove(g->times, g->times + 1, (g->count - 1) * sizeof(float));
        memmove(g->cpu,   g->cpu   + 1, (g->count - 1) * sizeof(float));
        memmove(g->gpu,   g->gpu   + 1, (g->count - 1) * sizeof(float));
        g->count--;
    }
    g->times[g->count] = total_ms;
    g->cpu  [g->count] = cpu_ms;
    g->gpu  [g->count] = gpu_ms;
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
    float cpu_sum   = 0.0f, cpu_max   = g->cpu[0];
    float gpu_sum   = 0.0f, gpu_max   = g->gpu[0];

    for (int i = 0; i < g->count; i++)
    {
        total_sum += g->times[i]; if (g->times[i] > total_max) total_max = g->times[i];
        cpu_sum   += g->cpu[i];   if (g->cpu[i]   > cpu_max)   cpu_max   = g->cpu[i];
        gpu_sum   += g->gpu[i];   if (g->gpu[i]   > gpu_max)   gpu_max   = g->gpu[i];
    }

    float total_avg = total_sum / (float)g->count;
    float cpu_avg   = cpu_sum   / (float)g->count;
    float gpu_avg   = gpu_sum   / (float)g->count;
    float total_latest = g->times[g->count - 1];
    float cpu_latest   = g->cpu  [g->count - 1];
    float gpu_latest   = g->gpu  [g->count - 1];

    igText("Frame:  %5.2f ms  (%4.1f FPS)  avg %5.2f  max %5.2f",
           total_latest, total_latest > 0.0f ? 1000.0f / total_latest : 0.0f,
           total_avg, total_max);
    igText("CPU work:        %5.2f ms      avg %5.2f  max %5.2f", cpu_latest, cpu_avg, cpu_max);
    igText("GPU finish wait: %5.2f ms      avg %5.2f  max %5.2f", gpu_latest, gpu_avg, gpu_max);
    igTextDisabled("(measured via fence sync - GPU value is the time CPU spent waiting after submitting)");

    // Heuristic: tag the dominant cost. With the new fence-sync measurement,
    // a low GPU value means the GPU drained the queue as fast as the CPU
    // could feed it (= CPU-bound), and a high GPU value means CPU finished
    // first and waited (= GPU-bound).
    if (gpu_latest < cpu_latest * 0.25f)
    {
        igTextColored((ImVec4){1.0f, 0.6f, 0.3f, 1.0f}, "CPU-bound (GPU drained queue fast)");
    }
    else if (gpu_latest > cpu_latest)
    {
        igTextColored((ImVec4){0.4f, 0.7f, 1.0f, 1.0f}, "GPU-bound (CPU waited for GPU)");
    }
    else
    {
        igTextColored((ImVec4){0.6f, 0.9f, 0.6f, 1.0f}, "CPU/GPU balanced");
    }

    igSpacing();

    // Shared y-axis scale so the three plots are directly comparable.
    float scale = total_max;
    if (cpu_max > scale) scale = cpu_max;
    if (gpu_max > scale) scale = gpu_max;
    if (g->ms_scale > 0.0f) scale = g->ms_scale;
    if (scale < 16.7f) scale = 16.7f; // always show at least the 60-fps line

    char total_overlay[64], cpu_overlay[64], gpu_overlay[64];
    snprintf(total_overlay, sizeof(total_overlay), "Total: %.2f ms",   total_latest);
    snprintf(cpu_overlay,   sizeof(cpu_overlay),   "CPU:   %.2f ms",   cpu_latest);
    snprintf(gpu_overlay,   sizeof(gpu_overlay),   "GPU:   %.2f ms",   gpu_latest);

    const ImVec2 plot_size = (ImVec2){0.0f, 60.0f};
    igPlotLines_FloatPtr("##frame_total", g->times, g->count, 0, total_overlay,
                         0.0f, scale, plot_size, (int)sizeof(float));
    igPlotLines_FloatPtr("##frame_cpu",   g->cpu,   g->count, 0, cpu_overlay,
                         0.0f, scale, plot_size, (int)sizeof(float));
    igPlotLines_FloatPtr("##frame_gpu",   g->gpu,   g->count, 0, gpu_overlay,
                         0.0f, scale, plot_size, (int)sizeof(float));

    igSpacing();
    igTextUnformatted("CPU breakdown (last frame, ms)", NULL);

    float perf_mesh   = MikuPan_PerfGetSectionMs(MP_PERF_MESH_RENDER);
    float perf_sprite = MikuPan_PerfGetSectionMs(MP_PERF_SPRITE_RENDER);
    float perf_flush  = MikuPan_PerfGetSectionMs(MP_PERF_BATCH_FLUSH);
    float perf_drawui = MikuPan_PerfGetSectionMs(MP_PERF_DRAWUI);
    float perf_render = MikuPan_PerfGetSectionMs(MP_PERF_RENDERUI);
    float perf_gs_up  = MikuPan_GsGetUploadMs();
    float perf_gs_dl  = MikuPan_GsGetDownloadMs();
    float perf_known  = perf_mesh + perf_sprite + perf_flush
                      + perf_drawui + perf_render + perf_gs_up + perf_gs_dl;
    float perf_other  = cpu_latest - perf_known;
    if (perf_other < 0.0f) perf_other = 0.0f;

    igText("Mesh render:      %5.2f ms", perf_mesh);

    // Per-mesh-type breakdown (sub-sections of MESH_RENDER). Sum ≈ perf_mesh
    // minus the dispatch / early-return overhead at the top of each renderer.
    {
        float perf_mesh_0x2  = MikuPan_PerfGetSectionMs(MP_PERF_MESH_0x2);
        float perf_mesh_0xA  = MikuPan_PerfGetSectionMs(MP_PERF_MESH_0xA);
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
    float perf_draw   = MikuPan_PerfGetSectionMs(MP_PERF_DRAW_SUBMIT);
    float perf_upload = MikuPan_PerfGetSectionMs(MP_PERF_BUFFER_UPLOAD);
    float perf_state  = MikuPan_PerfGetSectionMs(MP_PERF_STATE_CHANGE);
    igText("  glDraw* submit: %5.2f ms (driver overhead per draw call)", perf_draw);
    igText("  Buffer uploads: %5.2f ms (map+memcpy+unmap)", perf_upload);
    igText("  State changes:  %5.2f ms (shader/texture/render-state in cached path)", perf_state);

    // Per-call decomposition of "State changes" — tells you which of the four
    // calls (shader / texture / render-state / VAO) is the actual hot one.
    // Sum ≈ perf_state (a few hundred ns/frame of timer overhead aside).
    float perf_sc_shader  = MikuPan_PerfGetSectionMs(MP_PERF_SC_SHADER);
    float perf_sc_texture = MikuPan_PerfGetSectionMs(MP_PERF_SC_TEXTURE);
    float perf_sc_rs3d    = MikuPan_PerfGetSectionMs(MP_PERF_SC_RS3D);
    float perf_sc_vao     = MikuPan_PerfGetSectionMs(MP_PERF_SC_VAO);
    igText("      Shader:     %5.2f ms (glUseProgram / SetCurrentShaderProgram)", perf_sc_shader);
    igText("      Texture:    %5.2f ms (lookup + glBindTexture)",                 perf_sc_texture);
    {
        // L1 (tex0 → info) hit/miss. Misses fall through to MikuPan_GetTextureHash
        // which XXH3-hashes GS memory — usually the heaviest part of SC_TEXTURE.
        // L1 is wiped on every MikuPan_GsUpload, so post-upload frames re-hash.
        int l1_hits   = MikuPan_PerfGetTexL1Hits();
        int l1_misses = MikuPan_PerfGetTexL1Misses();
        igText("        L1 hits=%d misses=%d (misses → XXH3 over GS memory)",
               l1_hits, l1_misses);

        // Per-step decomposition of SC_TEXTURE — tells you exactly which step
        // inside MikuPan_SetTexture is dominant. Sum ≈ perf_sc_texture minus a
        // small amount of dispatch overhead between the inner timers.
        float perf_tex_l1     = MikuPan_PerfGetSectionMs(MP_PERF_TEX_L1_LOOKUP);
        float perf_tex_hash   = MikuPan_PerfGetSectionMs(MP_PERF_TEX_HASH);
        float perf_tex_l2     = MikuPan_PerfGetSectionMs(MP_PERF_TEX_L2_LOOKUP);
        float perf_tex_create = MikuPan_PerfGetSectionMs(MP_PERF_TEX_CREATE);
        float perf_tex_bind   = MikuPan_PerfGetSectionMs(MP_PERF_TEX_BIND);
        igText("        L1 lookup:  %5.2f ms (tex0 → info hash table probe)",     perf_tex_l1);
        igText("        Hash:       %5.2f ms (XXH3 over GS memory — L1-miss only)", perf_tex_hash);
        igText("        L2 lookup:  %5.2f ms (hash → info)",                       perf_tex_l2);
        igText("        Create:     %5.2f ms (glTexImage2D + glGenerateMipmap)",   perf_tex_create);
        igText("        Bind:       %5.2f ms (glBindTexture under cache)",         perf_tex_bind);
    }
    igText("      RenderState:%5.2f ms (depth / cull / blend mode)",              perf_sc_rs3d);
    igText("      VAO:        %5.2f ms (glBindVertexArray)",                      perf_sc_vao);

    igTextDisabled("These are subsets of the buckets above, not additional time.");

    igSpacing();
    igTextUnformatted("GS texture traffic (last frame)", NULL);

    int   ul_count = MikuPan_GsGetUploadCount();
    int   ul_bytes = MikuPan_GsGetUploadBytes();
    int   dl_count = MikuPan_GsGetDownloadCount();
    int   dl_bytes = MikuPan_GsGetDownloadBytes();

    igText("Uploads:   %4d calls   %6.1f KB",
           ul_count, ul_bytes / 1024.0f);
    igText("Downloads: %4d calls   %6.1f KB",
           dl_count, dl_bytes / 1024.0f);

    igSpacing();
    igTextUnformatted("Frame-time histogram (ms)", NULL);

    #define HIST_BUCKETS 16
    float histf[HIST_BUCKETS] = {0};
    float step = scale / (float)HIST_BUCKETS;
    int maxcount = 1;

    for (int i = 0; i < g->count; i++)
    {
        int b = (int)floorf(g->times[i] / step);
        if (b < 0) b = 0;
        if (b >= HIST_BUCKETS) b = HIST_BUCKETS - 1;
        histf[b] += 1.0f;
        if ((int)histf[b] > maxcount) maxcount = (int)histf[b];
    }

    igPlotHistogram_FloatPtr("##hist", histf, HIST_BUCKETS, 0, NULL,
                             0.0f, (float)maxcount, (ImVec2){0.0f, 60.0f}, (int)sizeof(float));

    igEnd();
}

// -- Texture list helpers ----------------------------------------------------
#define TEXTURE_LIST_MAX 4096

static MikuPan_TextureInfo *tex_list[TEXTURE_LIST_MAX];
static int tex_list_count = 0;

static void CollectTexture(MikuPan_TextureInfo *tex, void *ud)
{
    int *count = (int *)ud;
    if (*count < TEXTURE_LIST_MAX)
    {
        tex_list[(*count)++] = tex;
    }
}

static int CompareTextureById(const void *a, const void *b)
{
    const MikuPan_TextureInfo *ta = *(const MikuPan_TextureInfo **)a;
    const MikuPan_TextureInfo *tb = *(const MikuPan_TextureInfo **)b;
    return (int)ta->id - (int)tb->id;
}

// -- Hot shader reload window ------------------------------------------------

void MikuPan_UiShaderReloadWindow(void)
{
    if (!show_shader_reload)
    {
        return;
    }

    igSetNextWindowSize((ImVec2){560.0f, 480.0f}, ImGuiCond_FirstUseEver);
    if (!igBegin("Shader Reload", (bool *)&show_shader_reload, 0))
    {
        igEnd();
        return;
    }

    igTextDisabled("Edits to .vert/.frag/.geom files take effect after Reload.");
    igTextDisabled("Failed reloads keep the live program — no need to restart.");
    igSpacing();

    if (igButton("Reload All  (F5)", (ImVec2){160.0f, 0.0f}))
    {
        last_reload_error[0] = '\0';
        MikuPan_ReloadAllShaders(last_reload_error, (int)sizeof(last_reload_error));
    }
    igSameLine(0.0f, -1.0f);
    igTextDisabled("9 programs");

    igSpacing();
    igSeparator();
    igSpacing();

    for (int i = 0; i < MAX_SHADER_PROGRAMS; i++)
    {
        igPushID_Int(i);

        if (igButton("Reload", (ImVec2){80.0f, 0.0f}))
        {
            last_reload_error[0] = '\0';
            MikuPan_ReloadShader(i, last_reload_error, (int)sizeof(last_reload_error));
        }
        igSameLine(0.0f, -1.0f);
        igText("%-26s id=%u",
               MikuPan_GetShaderName(i),
               (unsigned)shader_list[i]);

        // Source paths underneath, indented — handy when there's any doubt
        // about which file an edit has to land in for a given shader.
        const char *vp = MikuPan_GetShaderVertSource(i);
        const char *gp = MikuPan_GetShaderGeomSource(i);
        const char *fp = MikuPan_GetShaderFragSource(i);
        if (vp) igText("    vert: %s", vp);
        if (gp) igText("    geom: %s", gp);
        if (fp) igText("    frag: %s", fp);

        igSpacing();
        igPopID();
    }

    if (last_reload_error[0] != '\0')
    {
        igSpacing();
        igSeparator();
        igTextColored((ImVec4){1.0f, 0.4f, 0.4f, 1.0f}, "Last reload error:");
        igTextWrapped("%s", last_reload_error);
        if (igButton("Clear", (ImVec2){0, 0}))
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
        case GL_POINTS:                   return "POINTS";
        case GL_LINES:                    return "LINES";
        case GL_LINE_LOOP:                return "LINE_LOOP";
        case GL_LINE_STRIP:               return "LINE_STRIP";
        case GL_TRIANGLES:                return "TRIANGLES";
        case GL_TRIANGLE_STRIP:           return "TRIANGLE_STRIP";
        case GL_TRIANGLE_FAN:             return "TRIANGLE_FAN";
        default:                          return "?";
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

    igSetNextWindowSize((ImVec2){780.0f, 540.0f}, ImGuiCond_FirstUseEver);
    if (!igBegin("Draw Call Inspector", (bool *)&show_draw_inspector, 0))
    {
        igEnd();
        return;
    }

    int enabled = MikuPan_DrawCapEnabled();
    if (igCheckbox("Capture", (bool *)&enabled))
    {
        MikuPan_DrawCapEnable(enabled);
    }

    igSameLine(0.0f, -1.0f);
    int isolate = MikuPan_DrawCapGetIsolate();
    if (isolate >= 0)
    {
        igTextColored((ImVec4){1.0f, 0.7f, 0.2f, 1.0f}, "  ISOLATING #%d", isolate);
        igSameLine(0.0f, -1.0f);
        if (igButton("Show All", (ImVec2){0, 0}))
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
    igText("%-4s %-6s %-16s %-26s %-7s %-7s %-9s %s",
           "#", "skip?", "primitive", "shader", "vao", "tex0", "ms", "");

    igBeginChild_Str("##draw_list", (ImVec2){0.0f, 0.0f}, 0, 0);

    for (int i = 0; i < count; i++)
    {
        const MikuPan_DrawCapEntry *e = MikuPan_DrawCapLastEntryAt(i);
        if (e == NULL) break;

        igPushID_Int(i);

        ImVec4 col = e->skipped ? (ImVec4){0.6f, 0.6f, 0.6f, 1.0f}
                                : (ImVec4){0.9f, 0.9f, 0.9f, 1.0f};

        igTextColored(col, "%-4d %-6s %-16s %-26s %-7u %-7u %7.3f  count=%d%s",
                      i,
                      e->skipped ? "skip" : "draw",
                      GlPrimName(e->mode),
                      e->shader_idx >= 0 ? MikuPan_GetShaderName(e->shader_idx) : "<external>",
                      (unsigned)e->vao,
                      (unsigned)e->texture0,
                      e->ms,
                      e->count,
                      e->is_elements ? " (elements)" : "");

        igSameLine(0.0f, -1.0f);

        if (isolate == i)
        {
            if (igButton("Unsolo", (ImVec2){70.0f, 0.0f}))
            {
                MikuPan_DrawCapSetIsolate(-1);
            }
        }
        else
        {
            if (igButton("Solo", (ImVec2){70.0f, 0.0f}))
            {
                MikuPan_DrawCapSetIsolate(i);
            }
        }

        igPopID();
    }

    igEndChild();
    igEnd();
}

// -- Controller remap window -------------------------------------------------

// Logical-PS2-button labels — one per index of sce_pad[]. Order must match
// the defaults in sdk/sce/libpad.c (and the comment there).
static const char *kPadLabels[SCE_PAD_LOGICAL_COUNT] = {
    "Triangle",
    "Cross",
    "Square",
    "Circle",
    "DPad Up",
    "DPad Down",
    "DPad Left",
    "DPad Right",
    "R3 (Right Stick)",
    "Select",
    "Start",
    "L3 (Left Stick)",
    "R1 / RB",
    "L2 / LT",
    "R2 / RT",
    "L1 / LB",
};

// pressed ? active : normal — keeps the per-shape callsites tight.
static ImU32 col_if(int pressed, ImU32 normal, ImU32 active)
{
    return pressed ? active : normal;
}

// Stable-frame check: only commit a rebind when the listened-for input has
// actually been *just* pressed. We use SDL gamepad button state directly so
// it works whether or not libpad has read this frame yet.
static int FindPressedGamepadInput(SDL_Gamepad *gp, int *out_kind, int *out_code)
{
    if (gp == NULL)
    {
        return 0;
    }

    for (int b = 0; b < SDL_GAMEPAD_BUTTON_COUNT; b++)
    {
        if (SDL_GetGamepadButton(gp, (SDL_GamepadButton)b))
        {
            *out_kind = SCE_BIND_BUTTON;
            *out_code = b;
            return 1;
        }
    }

    // Triggers only — sticks have rest values that drift and would constantly
    // commit a rebind. Use a generous half-press threshold.
    if (SDL_GetGamepadAxis(gp, SDL_GAMEPAD_AXIS_LEFT_TRIGGER) > 16384)
    {
        *out_kind = SCE_BIND_AXIS;
        *out_code = SDL_GAMEPAD_AXIS_LEFT_TRIGGER;
        return 1;
    }
    if (SDL_GetGamepadAxis(gp, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER) > 16384)
    {
        *out_kind = SCE_BIND_AXIS;
        *out_code = SDL_GAMEPAD_AXIS_RIGHT_TRIGGER;
        return 1;
    }

    return 0;
}

static int FindPressedScancode(void)
{
    const bool *keys = SDL_GetKeyboardState(NULL);
    if (keys == NULL)
    {
        return -1;
    }

    // Skip ENTER/SPACE so the user can confirm UI buttons without
    // immediately rebinding to the same key.
    for (int i = 4; i < SDL_SCANCODE_COUNT; i++)
    {
        if (i == SDL_SCANCODE_RETURN || i == SDL_SCANCODE_KP_ENTER ||
            i == SDL_SCANCODE_SPACE  || i == SDL_SCANCODE_ESCAPE)
        {
            continue;
        }
        if (keys[i])
        {
            return i;
        }
    }
    return -1;
}

static void DrawGamepadImage(SDL_Gamepad *gp)
{
    const float W = 540.0f;
    const float H = 270.0f;

    ImVec2 origin = igGetCursorScreenPos();
    igInvisibleButton("##gp_canvas", (ImVec2){W, H}, 0);

    ImDrawList *dl = igGetWindowDrawList();

    const ImU32 body_col     = 0xFF2A2A30;
    const ImU32 body_outline = 0xFF606070;
    const ImU32 plate_col    = 0xFF505058;
    const ImU32 active_col   = 0xFF50C8FF;

    // -- Body & grips --
    ImVec2 b_min = (ImVec2){origin.x + 70.0f,        origin.y + 50.0f};
    ImVec2 b_max = (ImVec2){origin.x + W - 70.0f,    origin.y + H - 30.0f};
    ImDrawList_AddRectFilled(dl, b_min, b_max, body_col, 30.0f, 0);
    ImDrawList_AddRect      (dl, b_min, b_max, body_outline, 30.0f, 0, 2.0f);

    ImDrawList_AddCircleFilled(dl, (ImVec2){origin.x + 80,     origin.y + H - 60}, 50, body_col,     24);
    ImDrawList_AddCircle      (dl, (ImVec2){origin.x + 80,     origin.y + H - 60}, 50, body_outline, 24, 2.0f);
    ImDrawList_AddCircleFilled(dl, (ImVec2){origin.x + W - 80, origin.y + H - 60}, 50, body_col,     24);
    ImDrawList_AddCircle      (dl, (ImVec2){origin.x + W - 80, origin.y + H - 60}, 50, body_outline, 24, 2.0f);

    // -- Shoulders & triggers --
    int lb = gp ? SDL_GetGamepadButton(gp, SDL_GAMEPAD_BUTTON_LEFT_SHOULDER)         : 0;
    int rb = gp ? SDL_GetGamepadButton(gp, SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER)        : 0;
    int lt = gp ? (SDL_GetGamepadAxis(gp, SDL_GAMEPAD_AXIS_LEFT_TRIGGER)  > 8000)    : 0;
    int rt = gp ? (SDL_GetGamepadAxis(gp, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER) > 8000)    : 0;

    ImDrawList_AddRectFilled(dl, (ImVec2){origin.x + 60,  origin.y + 28},
                                  (ImVec2){origin.x + 140, origin.y + 48},
                                  col_if(lb, plate_col, active_col), 6.0f, 0);
    ImDrawList_AddRectFilled(dl, (ImVec2){origin.x + 70,  origin.y + 8},
                                  (ImVec2){origin.x + 130, origin.y + 26},
                                  col_if(lt, plate_col, active_col), 6.0f, 0);
    ImDrawList_AddRectFilled(dl, (ImVec2){origin.x + W - 140, origin.y + 28},
                                  (ImVec2){origin.x + W - 60,  origin.y + 48},
                                  col_if(rb, plate_col, active_col), 6.0f, 0);
    ImDrawList_AddRectFilled(dl, (ImVec2){origin.x + W - 130, origin.y + 8},
                                  (ImVec2){origin.x + W - 70,  origin.y + 26},
                                  col_if(rt, plate_col, active_col), 6.0f, 0);

    ImDrawList_AddText_Vec2(dl, (ImVec2){origin.x + 90,        origin.y + 31}, 0xFFFFFFFF, "L1", NULL);
    ImDrawList_AddText_Vec2(dl, (ImVec2){origin.x + 90,        origin.y + 11}, 0xFFFFFFFF, "L2", NULL);
    ImDrawList_AddText_Vec2(dl, (ImVec2){origin.x + W - 110,   origin.y + 31}, 0xFFFFFFFF, "R1", NULL);
    ImDrawList_AddText_Vec2(dl, (ImVec2){origin.x + W - 110,   origin.y + 11}, 0xFFFFFFFF, "R2", NULL);

    // -- D-Pad (cross of 4 rectangles) --
    {
        float dx = origin.x + 145.0f;
        float dy = origin.y + 110.0f;
        float a  = 18.0f;
        float t  = 18.0f;

        int up    = gp ? SDL_GetGamepadButton(gp, SDL_GAMEPAD_BUTTON_DPAD_UP)    : 0;
        int down  = gp ? SDL_GetGamepadButton(gp, SDL_GAMEPAD_BUTTON_DPAD_DOWN)  : 0;
        int left  = gp ? SDL_GetGamepadButton(gp, SDL_GAMEPAD_BUTTON_DPAD_LEFT)  : 0;
        int right = gp ? SDL_GetGamepadButton(gp, SDL_GAMEPAD_BUTTON_DPAD_RIGHT) : 0;

        ImDrawList_AddRectFilled(dl, (ImVec2){dx - t/2, dy - a},
                                      (ImVec2){dx + t/2, dy - t/2},
                                      col_if(up, plate_col, active_col), 3, 0);
        ImDrawList_AddRectFilled(dl, (ImVec2){dx - t/2, dy + t/2},
                                      (ImVec2){dx + t/2, dy + a},
                                      col_if(down, plate_col, active_col), 3, 0);
        ImDrawList_AddRectFilled(dl, (ImVec2){dx - a,   dy - t/2},
                                      (ImVec2){dx - t/2, dy + t/2},
                                      col_if(left, plate_col, active_col), 3, 0);
        ImDrawList_AddRectFilled(dl, (ImVec2){dx + t/2, dy - t/2},
                                      (ImVec2){dx + a,   dy + t/2},
                                      col_if(right, plate_col, active_col), 3, 0);
        ImDrawList_AddRectFilled(dl, (ImVec2){dx - t/2, dy - t/2},
                                      (ImVec2){dx + t/2, dy + t/2},
                                      plate_col, 0, 0);
    }

    // -- Face buttons (4 colored circles in a diamond) --
    {
        float fx = origin.x + W - 145.0f;
        float fy = origin.y + 110.0f;
        float r  = 14.0f;
        float d  = 24.0f;

        int north = gp ? SDL_GetGamepadButton(gp, SDL_GAMEPAD_BUTTON_NORTH) : 0;
        int south = gp ? SDL_GetGamepadButton(gp, SDL_GAMEPAD_BUTTON_SOUTH) : 0;
        int west  = gp ? SDL_GetGamepadButton(gp, SDL_GAMEPAD_BUTTON_WEST)  : 0;
        int east  = gp ? SDL_GetGamepadButton(gp, SDL_GAMEPAD_BUTTON_EAST)  : 0;

        // North = Triangle (green); South = Cross (blue);
        // West  = Square   (pink ); East  = Circle (red ).
        ImDrawList_AddCircleFilled(dl, (ImVec2){fx,     fy - d}, r, col_if(north, 0xFF306030, 0xFF60FF60), 0);
        ImDrawList_AddCircleFilled(dl, (ImVec2){fx,     fy + d}, r, col_if(south, 0xFF303060, 0xFF6080FF), 0);
        ImDrawList_AddCircleFilled(dl, (ImVec2){fx - d, fy    }, r, col_if(west,  0xFF603060, 0xFFFF60FF), 0);
        ImDrawList_AddCircleFilled(dl, (ImVec2){fx + d, fy    }, r, col_if(east,  0xFF603030, 0xFFFF6060), 0);

        ImDrawList_AddCircle(dl, (ImVec2){fx,     fy - d}, r, 0xFFA0A0A0, 0, 1.5f);
        ImDrawList_AddCircle(dl, (ImVec2){fx,     fy + d}, r, 0xFFA0A0A0, 0, 1.5f);
        ImDrawList_AddCircle(dl, (ImVec2){fx - d, fy    }, r, 0xFFA0A0A0, 0, 1.5f);
        ImDrawList_AddCircle(dl, (ImVec2){fx + d, fy    }, r, 0xFFA0A0A0, 0, 1.5f);

        ImDrawList_AddText_Vec2(dl, (ImVec2){fx - 4,     fy - d - 7}, 0xFFFFFFFF, "Y", NULL);
        ImDrawList_AddText_Vec2(dl, (ImVec2){fx - 4,     fy + d - 7}, 0xFFFFFFFF, "A", NULL);
        ImDrawList_AddText_Vec2(dl, (ImVec2){fx - d - 4, fy - 7    }, 0xFFFFFFFF, "X", NULL);
        ImDrawList_AddText_Vec2(dl, (ImVec2){fx + d - 4, fy - 7    }, 0xFFFFFFFF, "B", NULL);
    }

    // -- Select / Start --
    {
        int back  = gp ? SDL_GetGamepadButton(gp, SDL_GAMEPAD_BUTTON_BACK)  : 0;
        int start = gp ? SDL_GetGamepadButton(gp, SDL_GAMEPAD_BUTTON_START) : 0;

        ImDrawList_AddRectFilled(dl, (ImVec2){origin.x + W/2 - 38, origin.y + 105},
                                      (ImVec2){origin.x + W/2 - 12, origin.y + 117},
                                      col_if(back, plate_col, active_col), 4, 0);
        ImDrawList_AddRectFilled(dl, (ImVec2){origin.x + W/2 + 12, origin.y + 105},
                                      (ImVec2){origin.x + W/2 + 38, origin.y + 117},
                                      col_if(start, plate_col, active_col), 4, 0);
        ImDrawList_AddText_Vec2(dl, (ImVec2){origin.x + W/2 - 42, origin.y + 92}, 0xFFC0C0C0, "Select", NULL);
        ImDrawList_AddText_Vec2(dl, (ImVec2){origin.x + W/2 + 14, origin.y + 92}, 0xFFC0C0C0, "Start",  NULL);
    }

    // -- Analog sticks --
    {
        int lpress = gp ? SDL_GetGamepadButton(gp, SDL_GAMEPAD_BUTTON_LEFT_STICK)  : 0;
        int rpress = gp ? SDL_GetGamepadButton(gp, SDL_GAMEPAD_BUTTON_RIGHT_STICK) : 0;

        float lsx = gp ? SDL_GetGamepadAxis(gp, SDL_GAMEPAD_AXIS_LEFTX)  / 32767.0f : 0.0f;
        float lsy = gp ? SDL_GetGamepadAxis(gp, SDL_GAMEPAD_AXIS_LEFTY)  / 32767.0f : 0.0f;
        float rsx = gp ? SDL_GetGamepadAxis(gp, SDL_GAMEPAD_AXIS_RIGHTX) / 32767.0f : 0.0f;
        float rsy = gp ? SDL_GetGamepadAxis(gp, SDL_GAMEPAD_AXIS_RIGHTY) / 32767.0f : 0.0f;

        float lcx = origin.x + 215.0f;
        float lcy = origin.y + 190.0f;
        float rcx = origin.x + W - 215.0f;
        float rcy = origin.y + 190.0f;

        ImDrawList_AddCircleFilled(dl, (ImVec2){lcx, lcy}, 26, 0xFF202028, 0);
        ImDrawList_AddCircle      (dl, (ImVec2){lcx, lcy}, 26, body_outline, 0, 1.5f);
        ImDrawList_AddCircleFilled(dl, (ImVec2){lcx + lsx*16, lcy + lsy*16}, 16,
                                       col_if(lpress, plate_col, active_col), 0);

        ImDrawList_AddCircleFilled(dl, (ImVec2){rcx, rcy}, 26, 0xFF202028, 0);
        ImDrawList_AddCircle      (dl, (ImVec2){rcx, rcy}, 26, body_outline, 0, 1.5f);
        ImDrawList_AddCircleFilled(dl, (ImVec2){rcx + rsx*16, rcy + rsy*16}, 16,
                                       col_if(rpress, plate_col, active_col), 0);
    }
}

static void DrawGamepadBindingList(SDL_Gamepad *gp)
{
    igTextDisabled(gp != NULL ? "Click 'Edit' on a row, then press a button on your controller."
                              : "No controller connected. Connect one to remap gamepad bindings.");

    for (int i = 0; i < SCE_PAD_LOGICAL_COUNT; i++)
    {
        igPushID_Int(i);

        igText("%-18s", kPadLabels[i]);
        igSameLine(180.0f, -1.0f);
        igText("%-22s", scePadBindingLabel(sce_gp_map[i]));
        igSameLine(390.0f, -1.0f);

        if (remap_target == i && remap_target_kb == 0)
        {
            igTextColored((ImVec4){1.0f, 0.7f, 0.2f, 1.0f}, "Press a button...");
            igSameLine(0.0f, -1.0f);
            if (igButton("Cancel", (ImVec2){0, 0}))
            {
                remap_target = -1;
            }
        }
        else
        {
            if (igButton("Edit", (ImVec2){60, 0}))
            {
                remap_target = i;
                remap_target_kb = 0;
            }
            igSameLine(0.0f, -1.0f);
            if (igButton("Clear", (ImVec2){60, 0}))
            {
                sce_gp_map[i].kind = SCE_BIND_NONE;
                sce_gp_map[i].code = 0;
            }
        }

        igPopID();
    }

    if (remap_target >= 0 && remap_target_kb == 0)
    {
        int kind = 0, code = 0;
        if (FindPressedGamepadInput(gp, &kind, &code))
        {
            sce_gp_map[remap_target].kind = kind;
            sce_gp_map[remap_target].code = code;
            remap_target = -1;
        }
        if (igIsKeyPressed_Bool(ImGuiKey_Escape, 0))
        {
            remap_target = -1;
        }
    }
}

static void DrawKeyboardBindingList(void)
{
    igTextDisabled("Active when no controller is connected. Click 'Edit' then press a key.");

    for (int i = 0; i < SCE_PAD_LOGICAL_COUNT; i++)
    {
        igPushID_Int(0x100 + i);

        igText("%-18s", kPadLabels[i]);
        igSameLine(180.0f, -1.0f);
        igText("%-22s", scePadScancodeLabel(sce_kb_map[i]));
        igSameLine(390.0f, -1.0f);

        if (remap_target == i && remap_target_kb == 1)
        {
            igTextColored((ImVec4){1.0f, 0.7f, 0.2f, 1.0f}, "Press a key...");
            igSameLine(0.0f, -1.0f);
            if (igButton("Cancel", (ImVec2){0, 0}))
            {
                remap_target = -1;
            }
        }
        else
        {
            if (igButton("Edit", (ImVec2){60, 0}))
            {
                remap_target = i;
                remap_target_kb = 1;
            }
            igSameLine(0.0f, -1.0f);
            if (igButton("Clear", (ImVec2){60, 0}))
            {
                sce_kb_map[i] = 0;
            }
        }

        igPopID();
    }

    if (remap_target >= 0 && remap_target_kb == 1)
    {
        int sc = FindPressedScancode();
        if (sc >= 0)
        {
            sce_kb_map[remap_target] = sc;
            remap_target = -1;
        }
        if (igIsKeyPressed_Bool(ImGuiKey_Escape, 0))
        {
            remap_target = -1;
        }
    }
}

void MikuPan_UiControllerRemapWindow(void)
{
    if (!show_controller_remap)
    {
        return;
    }

    SDL_Gamepad *gp = scePadGetSdlGamepad();

    igSetNextWindowSize((ImVec2){600.0f, 720.0f}, ImGuiCond_FirstUseEver);
    if (!igBegin("Controller Mapping", (bool *)&show_controller_remap, 0))
    {
        igEnd();
        return;
    }

    if (gp != NULL)
    {
        igText("Connected: %s", SDL_GetGamepadName(gp));
    }
    else
    {
        igTextDisabled("No controller connected (keyboard fallback active).");
    }

    DrawGamepadImage(gp);

    if (igButton("Reset to defaults", (ImVec2){0, 0}))
    {
        scePadResetBindings();
        remap_target = -1;
    }
    igSameLine(0.0f, -1.0f);
    igTextDisabled("(restores both gamepad and keyboard mappings)");

    igSpacing();

    if (igBeginTabBar("##remap_tabs", 0))
    {
        if (igBeginTabItem("Gamepad", NULL, 0))
        {
            DrawGamepadBindingList(gp);
            igEndTabItem();
        }
        if (igBeginTabItem("Keyboard", NULL, 0))
        {
            DrawKeyboardBindingList();
            igEndTabItem();
        }
        igEndTabBar();
    }

    igEnd();
}

// -- Public API --------------------------------------------------------------

void MikuPan_InitUi(SDL_Window *window, SDL_GLContext renderer)
{
    igCreateContext(NULL);
    ImGuiIO *io = igGetIO_Nil();
    io->ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    // Pull the primary display's current mode so the resolution sliders cap
    // at the user's actual screen size, and seed the default render resolution
    // to match — both were previously hardcoded at 5120/1440 (max) and 640/448
    // (default), which were wrong on every machine that wasn't exactly that.
    SDL_DisplayID primary = SDL_GetPrimaryDisplay();
    if (primary != 0)
    {
        const SDL_DisplayMode *mode = SDL_GetCurrentDisplayMode(primary);
        if (mode != NULL && mode->w > 0 && mode->h > 0)
        {
            screen_resolution_width  = mode->w;
            screen_resolution_height = mode->h;
            render_resolution_width  = mode->w;
            render_resolution_height = mode->h;
        }
    }

    MikuPan_ImGui_ImplInit(window, renderer);
}

void MikuPan_RenderUi(void)
{
    igRender();
    ImGuiIO *io = igGetIO_Nil();
    glad_glViewport(0, 0, (int)io->DisplaySize.x, (int)io->DisplaySize.y);
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
    FrameTimeGraph_Update(&g_frame_graph,
                          1000.0f / io->Framerate,
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

    MikuPan_UiControllerRemapWindow();
    MikuPan_UiShaderReloadWindow();
    MikuPan_UiDrawCallInspector();

    if (show_fps)
    {
        SetString2(0x10, 0.0f, 420.0f, 1, 0x80, 0x80, 0x80, (char *)"FPS %d", (int)MikuPan_GetFrameRate());
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

    qsort(tex_list, tex_list_count, sizeof(MikuPan_TextureInfo *), CompareTextureById);

    igBegin("OpenGL Texture", NULL, 0);

    for (int i = 0; i < tex_list_count; i++)
    {
        char label[64];
        snprintf(label, sizeof(label), "Texture ID %u", tex_list[i]->id);

        if (igCollapsingHeader_TreeNodeFlags(label, 0))
        {
            igText("%u: %d x %d", tex_list[i]->id, tex_list[i]->width, tex_list[i]->height);
            igImage(
                (ImTextureRef_c){ ._TexID = (ImTextureID)(uintptr_t)tex_list[i]->id },
                (ImVec2){(float)tex_list[i]->width, (float)tex_list[i]->height},
                (ImVec2){0.0f, 0.0f},
                (ImVec2){1.0f, 1.0f});
        }
    }

    igEnd();
}

void MikuPan_UiHandleShortcuts(void)
{
    if (igIsKeyPressed_Bool(ImGuiKey_F1, 0))
    {
        show_menu_bar = !show_menu_bar;
    }

    if (igIsKeyPressed_Bool(ImGuiKey_F2, 0))
    {
        dbg_wrk.mode_on = !dbg_wrk.mode_on;
    }

    // F5: reload all shaders. Skill-floor shortcut to make the edit-test loop
    // for shader work feel instant — no need to even open the panel.
    if (igIsKeyPressed_Bool(ImGuiKey_F5, 0))
    {
        last_reload_error[0] = '\0';
        MikuPan_ReloadAllShaders(last_reload_error, (int)sizeof(last_reload_error));
        // Surface the panel automatically when a reload errored, so the user
        // sees what failed instead of silently rendering against the old code.
        if (last_reload_error[0] != '\0')
        {
            show_shader_reload = 1;
        }
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
        if (igBeginMenu("Rendering", 1))
        {
            igCheckbox("Wireframe", (bool *)&render_wireframe);
            igCheckbox("Normals", (bool *)&render_normals);

            if (render_normals)
            {
                igSliderFloat("Normal Length", &normal_length, 0.1f, 100.0f, "%.1f", 0);
            }

            igCheckbox("Disable Lighting", (bool *)&disable_lighting);

            int shadows_on = MikuPan_IsShadowEnabled();
            if (igCheckbox("Shadows", (bool *)&shadows_on))
            {
                MikuPan_SetShadowEnabled(shadows_on);
            }

            igCheckbox("Textures", (bool *)&show_texture_list);
            igCheckbox("BoundingBox", (bool *)&show_bounding_boxes);

            if (igBeginMenu("Meshes", 1))
            {
                igCheckbox("Mesh 0x82", (bool *)&show_mesh_0x82);
                igCheckbox("Mesh 0x32", (bool *)&show_mesh_0x32);
                igCheckbox("Mesh 0x12", (bool *)&show_mesh_0x12);
                igCheckbox("Mesh 0x2",  (bool *)&show_mesh_0x2);
                igEndMenu();
            }

            if (igButton("Clear Texture Cache", (ImVec2){0.0f, 0.0f}))
            {
                MikuPan_RequestFlushTextureCache();
            }

            igSliderInt("MSAA", &msaa_samples, 0, 5, "", 0);
            igSameLine(0.0f, -1.0f);
            igText("%dx", msaa_samples << 1);
            // Cap slider max at the primary display's current mode (queried
            // once in MikuPan_InitUi). Lower bound stays at the minimum useful
            // PS2 internal resolution; if the user's screen is smaller than
            // that for some reason, hold at the minimum to keep the slider
            // consistent.
            int max_w = screen_resolution_width  < 640 ? 640 : screen_resolution_width;
            int max_h = screen_resolution_height < 448 ? 448 : screen_resolution_height;
            igSliderInt("Width",  &render_resolution_width,  640, max_w, "%d", 0);
            igSliderInt("Height", &render_resolution_height, 448, max_h, "%d", 0);
            igSliderFloat4("Light Color", light_color, 0.0f, 3.0f, "%.3f", 0);

            // Brightness / gamma — read by the renderer each frame and
            // pushed as uniforms on POSTPROCESS_SHADER for the final
            // scene-to-window blit (see MikuPan_GetBrightness/Gamma below
            // and the EndFrame post-process bind in mikupan_renderer.c).
            igSliderFloat("Brightness", &brightness,  0.0f, 2.0f, "%.2f", 0);
            igSliderFloat("Gamma",      &gamma_value, 0.1f, 3.0f, "%.2f", 0);

            igEndMenu();
        }

        igCheckbox("Ingame Debug Menu", (bool *)&dbg_wrk.mode_on);
        igCheckbox("Shader Reload",     (bool *)&show_shader_reload);
        igCheckbox("Draw Call Inspector", (bool *)&show_draw_inspector);

        igEndMenu();
    }

    if (igBeginMenu("Performance", 1))
    {
        igCheckbox("FPS Counter",       (bool *)&show_fps);
        igCheckbox("Frame Time Graph",  (bool *)&show_frame_time_graph);
        igEndMenu();
    }

    if (igBeginMenu("Input", 1))
    {
        igCheckbox("Controller Mapping", (bool *)&show_controller_remap);
        igEndMenu();
    }

    igEndMainMenuBar();
}

int MikuPan_IsBoundingBoxRendering(void)
{
    return show_bounding_boxes;
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

int MikuPan_IsMesh0x2Rendering(void)
{
    return show_mesh_0x2;
}

int MikuPan_IsLightingDisabled(void)
{
    return disable_lighting;
}

float *MikuPan_GetLightColor(void)
{
    return light_color;
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
    return msaa_samples << 1;
}

float MikuPan_GetBrightness(void)
{
    return brightness;
}

float MikuPan_GetGamma(void)
{
    return gamma_value;
}
