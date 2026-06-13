#include "mikupan_profiler.h"

#include "SDL3/SDL_timer.h"
#include "mikupan_shader.h"
#include "mikupan_pipeline.h"
#include "mikupan_gpu.h"
#include <string.h>

/// Per-section CPU timers — accumulate time spent in each high-level area of
/// rendering. Reset at frame start; sampled by the perf graph from the *last*
/// frame so the values don't change underneath the UI mid-draw.
static double g_perf_section_ms_curr[PERF_SECT_COUNT] = {0};
static double g_perf_section_ms_last[PERF_SECT_COUNT] = {0};

/// Per-frame CPU / SDL_GPU idle-wait timing for the perf-debug graph.
///
/// We measure two distinct, non-overlapping quantities:
///
///   CPU time = wall-clock from MikuPan_PerfBeginFrame up to the moment the
///              CPU has finished submitting the SDL_GPU command buffer.
///
///   GPU wait = wall-clock spent in SDL_WaitForGPUIdle after that submit.
///
/// This is useful as a queue-drain/back-pressure signal, but it is not a
/// hardware GPU-duration query. With SDL_GPU it can include queued work,
/// swapchain presentation, and vsync pacing, so it can converge to the display
/// interval even when per-frame rendering work gets cheaper.
///
/// Cost: waiting idle at end-of-frame serializes CPU and GPU, so the engine
/// loses some pipelining while the graph is collecting data.
static Uint64 g_frame_cpu_start_ticks = 0;
static float  g_last_frame_cpu_ms     = 0.0f;
static float  g_last_frame_gpu_ms     = 0.0f;

/// Coarse per-frame counters that feed the once-per-frame info_log line.
/// `state_changes` is incremented on real (non-deduped) state transitions;
/// `draw_calls` is incremented inside each batched draw submission.
static int g_state_changes_curr = 0;
static int g_draw_calls_curr    = 0;

/// Static-mesh-cache hit/miss instrumentation. The cache itself was removed
/// (each MikuPan_RenderMeshType* uploads + draws immediately), but the
/// counters and getters still exist so the UI binding compiles and so the
/// API is in place for the day a cache is reintroduced. Currently always 0.
static int g_mesh_cache_hits_curr        = 0;
static int g_mesh_cache_misses_new_curr  = 0;
static int g_mesh_cache_misses_full_curr = 0;
static int g_mesh_cache_hits_last        = 0;
static int g_mesh_cache_misses_new_last  = 0;
static int g_mesh_cache_misses_full_last = 0;

/// Texture L1 hit/miss instrumentation. An L1 miss falls through to
/// MikuPan_GetTextureHash, which XXH3-hashes potentially many KB of GS
/// memory — that's the suspected hot spot in PERF_SECT_SC_TEXTURE.
static int g_tex_l1_hits_curr   = 0;
static int g_tex_l1_misses_curr = 0;
static int g_tex_l1_hits_last   = 0;
static int g_tex_l1_misses_last = 0;

/* ── Section timing ─────────────────────────────────────────────────── */

Uint64 MikuPan_PerfBegin(void)
{
    return SDL_GetPerformanceCounter();
}

void MikuPan_PerfEnd(MikuPan_PerfSection sect, Uint64 t0)
{
    Uint64 t1 = SDL_GetPerformanceCounter();
    g_perf_section_ms_curr[sect] +=
        (double)(t1 - t0) * 1000.0 / (double)SDL_GetPerformanceFrequency();
}

void MikuPan_PerfScopeEnd(MikuPan_PerfScope *s)
{
    Uint64 t1 = SDL_GetPerformanceCounter();
    g_perf_section_ms_curr[s->section] +=
        (double)(t1 - s->t0) * 1000.0 / (double)SDL_GetPerformanceFrequency();
}

/* ── Per-draw-call capture ──────────────────────────────────────────── */
/// Fixed-cap ring of draw records. Two slots so the UI can read the *last*
/// frame's draws while the *current* frame is still being filled in — same
/// double-buffer pattern used for section timings above.
#define DRAWCAP_CAPACITY 2048

static int g_drawcap_enabled    = 0;
static int g_drawcap_isolate    = -1;

static MikuPan_DrawCapEntry g_drawcap_curr[DRAWCAP_CAPACITY];
static MikuPan_DrawCapEntry g_drawcap_last[DRAWCAP_CAPACITY];
static int g_drawcap_curr_count  = 0;
static int g_drawcap_curr_index  = 0; ///< draws issued this frame (whether captured or skipped)
static int g_drawcap_last_count  = 0;

void MikuPan_DrawCapEnable(int enabled)
{
    g_drawcap_enabled = enabled ? 1 : 0;
    if (!enabled)
    {
        // Releasing isolation when disabling avoids accidentally leaving
        // the renderer rendering only one draw after the panel is closed.
        g_drawcap_isolate = -1;
    }
}

int MikuPan_DrawCapEnabled(void)              { return g_drawcap_enabled; }
int MikuPan_DrawCapLastCount(void)            { return g_drawcap_last_count; }
void MikuPan_DrawCapSetIsolate(int idx)       { g_drawcap_isolate = idx; }
int MikuPan_DrawCapGetIsolate(void)           { return g_drawcap_isolate; }

const MikuPan_DrawCapEntry *MikuPan_DrawCapLastEntryAt(int idx)
{
    if (idx < 0 || idx >= g_drawcap_last_count) return NULL;
    return &g_drawcap_last[idx];
}

void MikuPan_DrawCapBeginFrame(void)
{
    // Snapshot the just-finished frame for the UI, then reset for the new one.
    if (g_drawcap_curr_count > 0)
    {
        memcpy(g_drawcap_last, g_drawcap_curr,
               (size_t)g_drawcap_curr_count * sizeof(MikuPan_DrawCapEntry));
    }
    g_drawcap_last_count = g_drawcap_curr_count;
    g_drawcap_curr_count = 0;
    g_drawcap_curr_index = 0;
}

/// Look up which entry in shader_list[] holds `program`, returning -1 if the
/// draw was issued via a program we don't manage (currently nothing — but
/// guards against external GL clients in the future).
static int FindShaderIndexForProgram(unsigned int program)
{
    for (int i = 0; i < MAX_SHADER_PROGRAMS; i++)
    {
        if (shader_list[i] == program) return i;
    }
    return -1;
}

/* ── Timed glDraw wrappers ──────────────────────────────────────────── */

void MikuPan_TimedDrawElements(GLenum mode, GLsizei count, GLenum type, const void *indices)
{
    int draw_index = g_drawcap_curr_index++;
    int skip = (g_drawcap_isolate >= 0 && draw_index != g_drawcap_isolate);

    Uint64 t0 = SDL_GetPerformanceCounter();
    if (!skip)
    {
        MikuPan_GPUDrawElements(mode, count, type, indices);
    }
    Uint64 t1 = SDL_GetPerformanceCounter();
    double ms = (double)(t1 - t0) * 1000.0 / (double)SDL_GetPerformanceFrequency();
    if (!skip)
    {
        g_perf_section_ms_curr[PERF_SECT_DRAW_SUBMIT] += ms;
    }

    if (g_drawcap_enabled && g_drawcap_curr_count < DRAWCAP_CAPACITY)
    {
        MikuPan_DrawCapEntry *e = &g_drawcap_curr[g_drawcap_curr_count++];
        e->mode        = mode;
        e->count       = (int)count;
        e->first       = 0;
        e->is_elements = 1;
        e->index_type  = type;
        e->program     = MikuPan_GetCurrentShaderProgram();
        e->shader_idx  = FindShaderIndexForProgram(e->program);
        e->vao         = MikuPan_GetBoundVAO();
        e->texture0    = MikuPan_GetBoundTexture2D();
        e->skipped     = skip;
        e->ms          = skip ? 0.0f : (float)ms;
    }
}

void MikuPan_TimedDrawArrays(GLenum mode, GLint first, GLsizei count)
{
    int draw_index = g_drawcap_curr_index++;
    int skip = (g_drawcap_isolate >= 0 && draw_index != g_drawcap_isolate);

    Uint64 t0 = SDL_GetPerformanceCounter();
    if (!skip)
    {
        MikuPan_GPUDrawArrays(mode, first, count);
    }
    Uint64 t1 = SDL_GetPerformanceCounter();
    double ms = (double)(t1 - t0) * 1000.0 / (double)SDL_GetPerformanceFrequency();
    if (!skip)
    {
        g_perf_section_ms_curr[PERF_SECT_DRAW_SUBMIT] += ms;
    }

    if (g_drawcap_enabled && g_drawcap_curr_count < DRAWCAP_CAPACITY)
    {
        MikuPan_DrawCapEntry *e = &g_drawcap_curr[g_drawcap_curr_count++];
        e->mode        = mode;
        e->count       = (int)count;
        e->first       = (int)first;
        e->is_elements = 0;
        e->index_type  = 0;
        e->program     = MikuPan_GetCurrentShaderProgram();
        e->shader_idx  = FindShaderIndexForProgram(e->program);
        e->vao         = MikuPan_GetBoundVAO();
        e->texture0    = MikuPan_GetBoundTexture2D();
        e->skipped     = skip;
        e->ms          = skip ? 0.0f : (float)ms;
    }
}

/* ── Frame timing ───────────────────────────────────────────────────── */

void MikuPan_PerfBeginFrame(void)
{
    g_frame_cpu_start_ticks = SDL_GetPerformanceCounter();
    MikuPan_DrawCapBeginFrame();
}

void MikuPan_PerfEndFrame(void)
{
    Uint64 cpu_done_ticks_gpu = SDL_GetPerformanceCounter();
    double freq_gpu = (double)SDL_GetPerformanceFrequency();
    g_last_frame_cpu_ms =
        (float)((double)(cpu_done_ticks_gpu - g_frame_cpu_start_ticks) *
                1000.0 / freq_gpu);

    MikuPan_GPUWaitIdle();
    Uint64 gpu_done_ticks = SDL_GetPerformanceCounter();
    double gpu_ms =
        (double)(gpu_done_ticks - cpu_done_ticks_gpu) * 1000.0 / freq_gpu;
    g_last_frame_gpu_ms = gpu_ms > 0.0 ? (float)gpu_ms : 0.0f;
}

void MikuPan_PerfResetFrame(void)
{
    for (int i = 0; i < PERF_SECT_COUNT; i++)
    {
        g_perf_section_ms_last[i] = g_perf_section_ms_curr[i];
        g_perf_section_ms_curr[i] = 0.0;
    }

    g_state_changes_curr = 0;
    g_draw_calls_curr    = 0;

    g_mesh_cache_hits_last        = g_mesh_cache_hits_curr;
    g_mesh_cache_misses_new_last  = g_mesh_cache_misses_new_curr;
    g_mesh_cache_misses_full_last = g_mesh_cache_misses_full_curr;
    g_mesh_cache_hits_curr        = 0;
    g_mesh_cache_misses_new_curr  = 0;
    g_mesh_cache_misses_full_curr = 0;

    g_tex_l1_hits_last   = g_tex_l1_hits_curr;
    g_tex_l1_misses_last = g_tex_l1_misses_curr;
    g_tex_l1_hits_curr   = 0;
    g_tex_l1_misses_curr = 0;
}

float MikuPan_GetLastFrameCpuMs(void) { return g_last_frame_cpu_ms; }
float MikuPan_GetLastFrameGpuMs(void) { return g_last_frame_gpu_ms; }

/* ── Counter increments ─────────────────────────────────────────────── */

void MikuPan_PerfStateChange(void)       { g_state_changes_curr++; }
void MikuPan_PerfDrawCall(void)          { g_draw_calls_curr++; }
void MikuPan_PerfMeshCacheHit(void)      { g_mesh_cache_hits_curr++; }
void MikuPan_PerfMeshCacheMissNew(void)  { g_mesh_cache_misses_new_curr++; }
void MikuPan_PerfMeshCacheMissFull(void) { g_mesh_cache_misses_full_curr++; }
void MikuPan_PerfTexL1Hit(void)          { g_tex_l1_hits_curr++; }
void MikuPan_PerfTexL1Miss(void)         { g_tex_l1_misses_curr++; }

/* ── Last-frame queries ─────────────────────────────────────────────── */

float MikuPan_PerfGetSectionMs(int section)
{
    if (section < 0 || section >= PERF_SECT_COUNT) return 0.0f;
    return (float)g_perf_section_ms_last[section];
}

int MikuPan_PerfGetMeshCacheHits(void)        { return g_mesh_cache_hits_last; }
int MikuPan_PerfGetMeshCacheMissesNew(void)   { return g_mesh_cache_misses_new_last; }
int MikuPan_PerfGetMeshCacheMissesFull(void)  { return g_mesh_cache_misses_full_last; }
int MikuPan_PerfGetTexL1Hits(void)            { return g_tex_l1_hits_last; }
int MikuPan_PerfGetTexL1Misses(void)          { return g_tex_l1_misses_last; }

/* ── Current-frame snapshots ────────────────────────────────────────── */

int MikuPan_PerfGetStateChangesCurrent(void)         { return g_state_changes_curr; }
int MikuPan_PerfGetDrawCallsCurrent(void)            { return g_draw_calls_curr; }
int MikuPan_PerfGetMeshCacheHitsCurrent(void)        { return g_mesh_cache_hits_curr; }
int MikuPan_PerfGetMeshCacheMissesNewCurrent(void)   { return g_mesh_cache_misses_new_curr; }
int MikuPan_PerfGetMeshCacheMissesFullCurrent(void)  { return g_mesh_cache_misses_full_curr; }
