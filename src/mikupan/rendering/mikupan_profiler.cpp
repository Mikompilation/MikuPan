#include "mikupan_profiler.h"

#include "SDL3/SDL_timer.h"
#include "mikupan_shader.h"
#include "mikupan_pipeline.h"
#include "mikupan_gpu.h"
#include "mikupan/debug/mikupan_logging_c.h"
#include <stdlib.h>
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
static int    g_gpu_wait_enabled      = 0;

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

static MikuPan_PerfDiagSnapshot g_perf_diag = {0};
static MikuPan_PerfDiagSnapshot g_perf_diag_last = {0};
static int g_perf_diag_init = 0;
static int g_perf_diag_enabled = 0;
static int g_perf_diag_log_enabled = 0;
static int g_perf_diag_log_interval = 60;


#define DRAWSOURCE_STACK_MAX 64

static MikuPan_DrawSourceState g_draw_source_stack[DRAWSOURCE_STACK_MAX];
static int g_draw_source_depth = 0;

const char *MikuPan_DrawSourceName(int kind)
{
    switch (kind)
    {
        case MIKUPAN_DRAW_SOURCE_MAIN_SCENE: return "main_scene";
        case MIKUPAN_DRAW_SOURCE_ROOM_BACKGROUND: return "room_background";
        case MIKUPAN_DRAW_SOURCE_MIRROR_PASS: return "mirror_pass";
        case MIKUPAN_DRAW_SOURCE_ROOM_NEAR: return "room_near";
        case MIKUPAN_DRAW_SOURCE_ROOM_SS: return "room_ss";
        case MIKUPAN_DRAW_SOURCE_FURNITURE: return "furniture";
        case MIKUPAN_DRAW_SOURCE_DRAWGIRL_NORMAL: return "drawgirl_normal";
        case MIKUPAN_DRAW_SOURCE_DRAWGIRL_MIRROR: return "drawgirl_mirror";
        case MIKUPAN_DRAW_SOURCE_PLAYER_SHADOW: return "player_shadow";
        case MIKUPAN_DRAW_SOURCE_ROOM_SHADOW: return "room_shadow";
        case MIKUPAN_DRAW_SOURCE_EFFECT_2D: return "effect_2d";
        case MIKUPAN_DRAW_SOURCE_SPRITE: return "sprite";
        case MIKUPAN_DRAW_SOURCE_POSTPROCESS: return "postprocess";
        case MIKUPAN_DRAW_SOURCE_UI: return "ui";
        case MIKUPAN_DRAW_SOURCE_RML: return "rml";
        case MIKUPAN_DRAW_SOURCE_DEBUG_DRAW: return "debug_draw";
        case MIKUPAN_DRAW_SOURCE_TEXTURE_COPY: return "texture_copy";
        case MIKUPAN_DRAW_SOURCE_ENEMY: return "enemy";
        case MIKUPAN_DRAW_SOURCE_FLY_MOVE: return "fly_move";
        default: return "unknown";
    }
}

int MikuPan_DrawSourcePush(int kind, int detail0, int detail1)
{
    int token = g_draw_source_depth;
    if (kind < 0 || kind >= MIKUPAN_DRAW_SOURCE_COUNT)
    {
        kind = MIKUPAN_DRAW_SOURCE_UNKNOWN;
    }
    if (g_draw_source_depth < DRAWSOURCE_STACK_MAX)
    {
        g_draw_source_stack[g_draw_source_depth].kind = kind;
        g_draw_source_stack[g_draw_source_depth].detail0 = detail0;
        g_draw_source_stack[g_draw_source_depth].detail1 = detail1;
        g_draw_source_depth++;
    }
    else
    {
        g_draw_source_stack[DRAWSOURCE_STACK_MAX - 1].kind = kind;
        g_draw_source_stack[DRAWSOURCE_STACK_MAX - 1].detail0 = detail0;
        g_draw_source_stack[DRAWSOURCE_STACK_MAX - 1].detail1 = detail1;
    }
    return token;
}

void MikuPan_DrawSourcePop(int token)
{
    if (token < 0)
    {
        token = 0;
    }
    if (token <= g_draw_source_depth)
    {
        g_draw_source_depth = token;
    }
}

void MikuPan_DrawSourceGetCurrent(MikuPan_DrawSourceState *out)
{
    if (out == NULL)
    {
        return;
    }
    if (g_draw_source_depth <= 0)
    {
        out->kind = MIKUPAN_DRAW_SOURCE_UNKNOWN;
        out->detail0 = 0;
        out->detail1 = 0;
        return;
    }
    *out = g_draw_source_stack[g_draw_source_depth - 1];
}

static void DrawSourceGetParent(MikuPan_DrawSourceState *out)
{
    if (out == NULL)
    {
        return;
    }
    if (g_draw_source_depth <= 1)
    {
        out->kind = MIKUPAN_DRAW_SOURCE_UNKNOWN;
        out->detail0 = 0;
        out->detail1 = 0;
        return;
    }
    *out = g_draw_source_stack[g_draw_source_depth - 2];
}

static int DrawSourceInferFromShader(int shader_idx)
{
    const char *name = NULL;
    if (shader_idx >= 0)
    {
        name = MikuPan_GetShaderName(shader_idx);
    }
    if (name == NULL)
    {
        return MIKUPAN_DRAW_SOURCE_UNKNOWN;
    }
    if (strncmp(name, "SPRITE", 6) == 0 || strstr(name, "SPRITE") != NULL)
    {
        return MIKUPAN_DRAW_SOURCE_SPRITE;
    }
    if (strstr(name, "POSTPROCESS") != NULL)
    {
        return MIKUPAN_DRAW_SOURCE_POSTPROCESS;
    }
    if (strstr(name, "RML") != NULL)
    {
        return MIKUPAN_DRAW_SOURCE_RML;
    }
    return MIKUPAN_DRAW_SOURCE_UNKNOWN;
}

static void DrawCapFillSource(MikuPan_DrawCapEntry *e)
{
    MikuPan_DrawSourceState src;
    MikuPan_DrawSourceState parent;
    MikuPan_DrawSourceGetCurrent(&src);
    DrawSourceGetParent(&parent);
    if (src.kind == MIKUPAN_DRAW_SOURCE_UNKNOWN)
    {
        src.kind = DrawSourceInferFromShader(e->shader_idx);
    }
    e->source_kind = src.kind;
    e->source_detail0 = src.detail0;
    e->source_detail1 = src.detail1;
    e->source_parent_kind = parent.kind;
    e->source_parent_detail0 = parent.detail0;
    e->source_parent_detail1 = parent.detail1;
}

static int PerfDiagEnvInt(const char *name, int fallback)
{
    const char *v = getenv(name);
    if (v == NULL || v[0] == '\0')
    {
        return fallback;
    }
    return atoi(v);
}

static void PerfDiagInitialize(void)
{
    if (g_perf_diag_init)
    {
        return;
    }

    g_perf_diag_enabled = PerfDiagEnvInt("MIKUPAN_PERF_DIAG", 0) != 0;
    g_perf_diag_log_enabled = g_perf_diag_enabled;
    g_perf_diag_log_interval = PerfDiagEnvInt("MIKUPAN_PERF_DIAG_INTERVAL", 60);
    if (g_perf_diag_log_interval <= 0)
    {
        g_perf_diag_log_interval = 1;
    }
    g_perf_diag_init = 1;
}

int MikuPan_PerfDiagIsEnabled(void)
{
    PerfDiagInitialize();
    return g_perf_diag_enabled;
}

void MikuPan_PerfDiagSetEnabled(int enabled)
{
    PerfDiagInitialize();
    g_perf_diag_enabled = enabled ? 1 : 0;
}

int MikuPan_PerfDiagLogEnabled(void)
{
    PerfDiagInitialize();
    return g_perf_diag_log_enabled;
}

void MikuPan_PerfDiagSetLogEnabled(int enabled)
{
    PerfDiagInitialize();
    g_perf_diag_log_enabled = enabled ? 1 : 0;
}

int MikuPan_PerfDiagGetLogInterval(void)
{
    PerfDiagInitialize();
    return g_perf_diag_log_interval > 0 ? g_perf_diag_log_interval : 1;
}

void MikuPan_PerfDiagSetLogInterval(int interval)
{
    PerfDiagInitialize();
    g_perf_diag_log_interval = interval > 0 ? interval : 1;
}

void MikuPan_PerfDiagGetLastSnapshot(MikuPan_PerfDiagSnapshot *out)
{
    if (out == NULL)
    {
        return;
    }
    *out = g_perf_diag_last;
}

const char *MikuPan_PerfDiagUploadContextName(int context)
{
    switch (context)
    {
        case MIKUPAN_PERF_UPLOAD_MESH0X2_CPU_STREAM: return "mesh0x2_cpu_stream";
        case MIKUPAN_PERF_UPLOAD_MESH0X2_GPU_BIND: return "mesh0x2_gpu_bind";
        case MIKUPAN_PERF_UPLOAD_MESH0X2_UV: return "mesh0x2_uv";
        case MIKUPAN_PERF_UPLOAD_MESH0X2_IBO: return "mesh0x2_ibo";
        case MIKUPAN_PERF_UPLOAD_SKIN_PALETTE: return "skin_palette";
        case MIKUPAN_PERF_UPLOAD_OTHER:
        default: return "other";
    }
}

const char *MikuPan_PerfDiagSkinRejectName(int reason)
{
    switch (reason)
    {
        case MIKUPAN_PERF_SKIN_REJECT_UNSUPPORTED_VTYPE: return "unsupported_vtype";
        case MIKUPAN_PERF_SKIN_REJECT_MESH_TYPE: return "mesh_type";
        case MIKUPAN_PERF_SKIN_REJECT_GPU_DISABLED: return "gpu_disabled";
        case MIKUPAN_PERF_SKIN_REJECT_SHADOW_PASS: return "shadow_pass";
        case MIKUPAN_PERF_SKIN_REJECT_RECEIVER_PASS: return "receiver_pass";
        case MIKUPAN_PERF_SKIN_REJECT_WEIGHTED_DISABLED: return "weighted_disabled";
        case MIKUPAN_PERF_SKIN_REJECT_WEIGHTED_PREP_FAILED: return "weighted_prep_failed";
        case MIKUPAN_PERF_SKIN_REJECT_WEIGHTED_INVALID_ARGS: return "weighted_invalid_args";
        case MIKUPAN_PERF_SKIN_REJECT_WEIGHTED_NO_LIST: return "weighted_no_list";
        case MIKUPAN_PERF_SKIN_REJECT_WEIGHTED_NO_VERTEX_SOURCE: return "weighted_no_vertex_source";
        case MIKUPAN_PERF_SKIN_REJECT_WEIGHTED_NO_NORMAL_SOURCE: return "weighted_no_normal_source";
        case MIKUPAN_PERF_SKIN_REJECT_WEIGHTED_LIST_PTR_NULL: return "weighted_list_ptr_null";
        case MIKUPAN_PERF_SKIN_REJECT_WEIGHTED_VERTEX_PTR_NULL: return "weighted_vertex_ptr_null";
        case MIKUPAN_PERF_SKIN_REJECT_WEIGHTED_NORMAL_PTR_NULL: return "weighted_normal_ptr_null";
        case MIKUPAN_PERF_SKIN_REJECT_WEIGHTED_POS_INDEX_INVALID: return "weighted_pos_index_invalid";
        case MIKUPAN_PERF_SKIN_REJECT_WEIGHTED_NRM_INDEX_INVALID: return "weighted_nrm_index_invalid";
        case MIKUPAN_PERF_SKIN_REJECT_WEIGHTED_POS_LIST_MISS: return "weighted_pos_list_miss";
        case MIKUPAN_PERF_SKIN_REJECT_WEIGHTED_NRM_LIST_MISS: return "weighted_nrm_list_miss";
        case MIKUPAN_PERF_SKIN_REJECT_LEGACY_PREPASS: return "legacy_prepass";
        default: return "unknown";
    }
}

static void PerfDiagMaybeLog(void)
{
    if (!MikuPan_PerfDiagIsEnabled() || !MikuPan_PerfDiagLogEnabled() || g_perf_diag.frame == 0)
    {
        return;
    }

    int interval = MikuPan_PerfDiagGetLogInterval();
    if ((g_perf_diag.frame % (unsigned int)interval) != 0)
    {
        return;
    }

    info_log("[PERFDIAG] frame=%u cpu=%.3fms gpuwait=%.3fms draws=%d state=%d uploads=%d bytes=%llu grows=%d upload_flushes=%d renderpass_flushes=%d meshcache=%d/%d/%d tex_l1=%d/%d",
             g_perf_diag.frame,
             g_last_frame_cpu_ms,
             g_last_frame_gpu_ms,
             g_draw_calls_curr,
             g_state_changes_curr,
             g_perf_diag.gpu_uploads,
             g_perf_diag.gpu_upload_bytes,
             g_perf_diag.gpu_upload_grows,
             g_perf_diag.gpu_upload_flushes,
             g_perf_diag.render_pass_flushes,
             g_mesh_cache_hits_curr,
             g_mesh_cache_misses_new_curr,
             g_mesh_cache_misses_full_curr,
             g_tex_l1_hits_curr,
             g_tex_l1_misses_curr);

    info_log("[PERFDIAG] mesh0x2 calls=%d mesh0xA=%d cpu=%d gpu=%d shadow=%d receiver=%d cache_on=%d cache_hit=%d static_miss=%d submeshes=%d vertices=%d indices=%d max_sub=%d max_vtx=%d max_idx=%d stream_skip=%d stream_skip_bytes=%llu",
             g_perf_diag.mesh0x2_calls,
             g_perf_diag.mesh0xA_calls,
             g_perf_diag.mesh0x2_cpu,
             g_perf_diag.mesh0x2_gpu,
             g_perf_diag.mesh0x2_shadow,
             g_perf_diag.mesh0x2_receiver,
             g_perf_diag.mesh0x2_cache_on,
             g_perf_diag.mesh0x2_cache_hit,
             g_perf_diag.mesh0x2_static_miss,
             g_perf_diag.mesh0x2_submeshes,
             g_perf_diag.mesh0x2_vertices,
             g_perf_diag.mesh0x2_indices,
             g_perf_diag.mesh0x2_max_submeshes,
             g_perf_diag.mesh0x2_max_vertices,
             g_perf_diag.mesh0x2_max_indices,
             g_perf_diag.mesh0x2_stream_skips,
             g_perf_diag.mesh0x2_stream_skip_bytes);

    info_log("[PERFDIAG] skin vtype2=%d vtype3=%d other=%d allowed=%d blocked=%d copy=%d/%d weighted=%d/%d cpu_calc=%d/%d gpu=%d/%d",
             g_perf_diag.skin_vtype2,
             g_perf_diag.skin_vtype3,
             g_perf_diag.skin_other,
             g_perf_diag.skin_allowed,
             g_perf_diag.skin_blocked,
             g_perf_diag.skin_paths[MIKUPAN_PERF_SKIN_COPY],
             g_perf_diag.skin_vertices[MIKUPAN_PERF_SKIN_COPY],
             g_perf_diag.skin_paths[MIKUPAN_PERF_SKIN_WEIGHTED_COPY],
             g_perf_diag.skin_vertices[MIKUPAN_PERF_SKIN_WEIGHTED_COPY],
             g_perf_diag.skin_paths[MIKUPAN_PERF_SKIN_CPU_CALC],
             g_perf_diag.skin_vertices[MIKUPAN_PERF_SKIN_CPU_CALC],
             g_perf_diag.skin_paths[MIKUPAN_PERF_SKIN_GPU],
             g_perf_diag.skin_vertices[MIKUPAN_PERF_SKIN_GPU]);

    for (int i = 0; i < MIKUPAN_PERF_SKIN_VTYPE_COUNT; i++)
    {
        if (g_perf_diag.skin_vtype_counts[i] == 0)
        {
            continue;
        }
        info_log("[PERFDIAG] skin_vtype 0x%02x count=%d vertices=%d copy=%d/%d weighted=%d/%d cpu_calc=%d/%d gpu=%d/%d",
                 i,
                 g_perf_diag.skin_vtype_counts[i],
                 g_perf_diag.skin_vtype_vertices[i],
                 g_perf_diag.skin_vtype_path_counts[MIKUPAN_PERF_SKIN_COPY][i],
                 g_perf_diag.skin_vtype_path_vertices[MIKUPAN_PERF_SKIN_COPY][i],
                 g_perf_diag.skin_vtype_path_counts[MIKUPAN_PERF_SKIN_WEIGHTED_COPY][i],
                 g_perf_diag.skin_vtype_path_vertices[MIKUPAN_PERF_SKIN_WEIGHTED_COPY][i],
                 g_perf_diag.skin_vtype_path_counts[MIKUPAN_PERF_SKIN_CPU_CALC][i],
                 g_perf_diag.skin_vtype_path_vertices[MIKUPAN_PERF_SKIN_CPU_CALC][i],
                 g_perf_diag.skin_vtype_path_counts[MIKUPAN_PERF_SKIN_GPU][i],
                 g_perf_diag.skin_vtype_path_vertices[MIKUPAN_PERF_SKIN_GPU][i]);
    }

    for (int i = 0; i < MIKUPAN_PERF_SKIN_REJECT_COUNT; i++)
    {
        if (g_perf_diag.skin_reject_counts[i] == 0)
        {
            continue;
        }
        info_log("[PERFDIAG] skin_reject %s count=%d vertices=%d",
                 MikuPan_PerfDiagSkinRejectName(i),
                 g_perf_diag.skin_reject_counts[i],
                 g_perf_diag.skin_reject_vertices[i]);
        for (int j = 0; j < MIKUPAN_PERF_SKIN_VTYPE_COUNT; j++)
        {
            if (g_perf_diag.skin_reject_vtype_counts[i][j] == 0)
            {
                continue;
            }
            info_log("[PERFDIAG] skin_reject_vtype %s vtype=0x%02x count=%d vertices=%d",
                     MikuPan_PerfDiagSkinRejectName(i),
                     j,
                     g_perf_diag.skin_reject_vtype_counts[i][j],
                     g_perf_diag.skin_reject_vtype_vertices[i][j]);
        }
    }

    info_log("[PERFDIAG] drawgirl normal=%d mirror=%d rendered=%d hidden=%d tofu=%d subobj_calls=%d subobj_total=%d sorted_items=%d",
             g_perf_diag.drawgirl_normal,
             g_perf_diag.drawgirl_mirror,
             g_perf_diag.drawgirl_rendered,
             g_perf_diag.drawgirl_hidden,
             g_perf_diag.drawgirl_tofu,
             g_perf_diag.drawgirl_subobj_calls,
             g_perf_diag.drawgirl_subobj_total,
             g_perf_diag.drawgirl_sort_items);

    for (int i = 0; i < MIKUPAN_PERF_UPLOAD_COUNT; i++)
    {
        if (g_perf_diag.upload_counts[i] == 0)
        {
            continue;
        }
        info_log("[PERFDIAG] upload_context %s count=%d bytes=%llu",
                 MikuPan_PerfDiagUploadContextName(i),
                 g_perf_diag.upload_counts[i],
                 g_perf_diag.upload_bytes[i]);
    }
}

static void PerfDiagSnapshotCurrent(void)
{
    g_perf_diag.cpu_ms = g_last_frame_cpu_ms;
    g_perf_diag.gpu_wait_ms = g_last_frame_gpu_ms;
    g_perf_diag.draw_calls = g_draw_calls_curr;
    g_perf_diag.state_changes = g_state_changes_curr;
    g_perf_diag.mesh_cache_hits = g_mesh_cache_hits_curr;
    g_perf_diag.mesh_cache_misses_new = g_mesh_cache_misses_new_curr;
    g_perf_diag.mesh_cache_misses_full = g_mesh_cache_misses_full_curr;
    g_perf_diag.tex_l1_hits = g_tex_l1_hits_curr;
    g_perf_diag.tex_l1_misses = g_tex_l1_misses_curr;
    g_perf_diag_last = g_perf_diag;
}

static void PerfDiagReset(void)
{
    unsigned int next_frame = g_perf_diag.frame + 1;
    memset(&g_perf_diag, 0, sizeof(g_perf_diag));
    g_perf_diag.frame = next_frame;
}

int MikuPan_PerfSetUploadContext(int context)
{
    int old = g_perf_diag.upload_context;
    if (context < 0 || context >= MIKUPAN_PERF_UPLOAD_COUNT)
    {
        context = MIKUPAN_PERF_UPLOAD_OTHER;
    }
    g_perf_diag.upload_context = context;
    return old;
}

int MikuPan_PerfGetUploadContext(void)
{
    return g_perf_diag.upload_context;
}

void MikuPan_PerfRecordGpuUpload(unsigned int buffer_id, unsigned int size, int grew, int flushed_render_pass)
{
    if (!MikuPan_PerfDiagIsEnabled())
    {
        return;
    }
    (void)buffer_id;
    int context = g_perf_diag.upload_context;
    if (context < 0 || context >= MIKUPAN_PERF_UPLOAD_COUNT)
    {
        context = MIKUPAN_PERF_UPLOAD_OTHER;
    }
    g_perf_diag.gpu_uploads++;
    g_perf_diag.gpu_upload_bytes += size;
    g_perf_diag.gpu_upload_grows += grew ? 1 : 0;
    g_perf_diag.gpu_upload_flushes += flushed_render_pass ? 1 : 0;
    g_perf_diag.upload_counts[context]++;
    g_perf_diag.upload_bytes[context] += size;
}

void MikuPan_PerfRecordRenderPassFlush(void)
{
    if (!MikuPan_PerfDiagIsEnabled())
    {
        return;
    }
    g_perf_diag.render_pass_flushes++;
}

void MikuPan_PerfRecordMesh0x2Packet(int mesh_type, int is_skin, int cache_on, int cache_hit, int need_static_upload, int num_mesh, int vnum, int index_count, int shadow_pass, int receiver_pass)
{
    if (!MikuPan_PerfDiagIsEnabled())
    {
        return;
    }
    if ((mesh_type & 0xff) == 0x2)
    {
        g_perf_diag.mesh0x2_calls++;
    }
    else if ((mesh_type & 0xff) == 0xA)
    {
        g_perf_diag.mesh0xA_calls++;
    }
    else
    {
        return;
    }

    if (is_skin) g_perf_diag.mesh0x2_gpu++;
    else g_perf_diag.mesh0x2_cpu++;
    if (shadow_pass) g_perf_diag.mesh0x2_shadow++;
    if (receiver_pass) g_perf_diag.mesh0x2_receiver++;
    if (cache_on) g_perf_diag.mesh0x2_cache_on++;
    if (cache_hit) g_perf_diag.mesh0x2_cache_hit++;
    if (need_static_upload) g_perf_diag.mesh0x2_static_miss++;
    g_perf_diag.mesh0x2_submeshes += num_mesh;
    g_perf_diag.mesh0x2_vertices += vnum;
    g_perf_diag.mesh0x2_indices += index_count;
    if (num_mesh > g_perf_diag.mesh0x2_max_submeshes) g_perf_diag.mesh0x2_max_submeshes = num_mesh;
    if (vnum > g_perf_diag.mesh0x2_max_vertices) g_perf_diag.mesh0x2_max_vertices = vnum;
    if (index_count > g_perf_diag.mesh0x2_max_indices) g_perf_diag.mesh0x2_max_indices = index_count;
}

void MikuPan_PerfRecordMesh0x2StreamSkip(long long size)
{
    if (!MikuPan_PerfDiagIsEnabled())
    {
        return;
    }
    if (size <= 0)
    {
        return;
    }
    g_perf_diag.mesh0x2_stream_skips++;
    g_perf_diag.mesh0x2_stream_skip_bytes += (unsigned long long)size;
}

void MikuPan_PerfRecordSkinPath(int vtype, int path, int allow_gpu_skin, int vertices)
{
    if (!MikuPan_PerfDiagIsEnabled())
    {
        return;
    }

    int vtype_index = (int)(unsigned char)vtype;
    if (vtype == 2) g_perf_diag.skin_vtype2++;
    else if (vtype == 3) g_perf_diag.skin_vtype3++;
    else g_perf_diag.skin_other++;

    if (allow_gpu_skin) g_perf_diag.skin_allowed++;
    else g_perf_diag.skin_blocked++;

    g_perf_diag.skin_vtype_counts[vtype_index]++;
    if (vertices > 0)
    {
        g_perf_diag.skin_vtype_vertices[vtype_index] += vertices;
    }

    if (path < 0 || path >= MIKUPAN_PERF_SKIN_COUNT)
    {
        return;
    }

    g_perf_diag.skin_paths[path]++;
    g_perf_diag.skin_vtype_path_counts[path][vtype_index]++;
    if (vertices > 0)
    {
        g_perf_diag.skin_vertices[path] += vertices;
        g_perf_diag.skin_vtype_path_vertices[path][vtype_index] += vertices;
    }
}

void MikuPan_PerfRecordSkinReject(int vtype, int reason, int vertices)
{
    if (!MikuPan_PerfDiagIsEnabled())
    {
        return;
    }
    if (reason < 0 || reason >= MIKUPAN_PERF_SKIN_REJECT_COUNT)
    {
        return;
    }

    int vtype_index = (int)(unsigned char)vtype;
    g_perf_diag.skin_reject_counts[reason]++;
    g_perf_diag.skin_reject_vtype_counts[reason][vtype_index]++;
    if (vertices > 0)
    {
        g_perf_diag.skin_reject_vertices[reason] += vertices;
        g_perf_diag.skin_reject_vtype_vertices[reason][vtype_index] += vertices;
    }
}


void MikuPan_PerfRecordDrawGirl(int in_mirror, int rendered, int hidden, int tofu)
{
    if (!MikuPan_PerfDiagIsEnabled())
    {
        return;
    }
    if (in_mirror) g_perf_diag.drawgirl_mirror++;
    else g_perf_diag.drawgirl_normal++;
    if (rendered) g_perf_diag.drawgirl_rendered++;
    if (hidden) g_perf_diag.drawgirl_hidden++;
    if (tofu) g_perf_diag.drawgirl_tofu++;
}

void MikuPan_PerfRecordDrawGirlSubObj(int obj_num)
{
    if (!MikuPan_PerfDiagIsEnabled())
    {
        return;
    }
    g_perf_diag.drawgirl_subobj_calls++;
    g_perf_diag.drawgirl_subobj_total += obj_num;
}

void MikuPan_PerfRecordDrawGirlSubObjItem(int index)
{
    if (!MikuPan_PerfDiagIsEnabled())
    {
        return;
    }
    (void)index;
    g_perf_diag.drawgirl_sort_items++;
}

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
    g_draw_source_depth = 0;
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
        DrawCapFillSource(e);
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
        DrawCapFillSource(e);
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

    if (g_gpu_wait_enabled)
    {
        MikuPan_GPUWaitIdle();
        Uint64 gpu_done_ticks = SDL_GetPerformanceCounter();
        double gpu_ms =
            (double)(gpu_done_ticks - cpu_done_ticks_gpu) * 1000.0 / freq_gpu;
        g_last_frame_gpu_ms = gpu_ms > 0.0 ? (float)gpu_ms : 0.0f;
    }
    else
    {
        g_last_frame_gpu_ms = 0.0f;
    }
}

void MikuPan_PerfSetGpuWaitEnabled(int enabled)
{
    g_gpu_wait_enabled = enabled ? 1 : 0;
}

int MikuPan_PerfIsGpuWaitEnabled(void)
{
    return g_gpu_wait_enabled;
}

void MikuPan_PerfResetFrame(void)
{
    PerfDiagSnapshotCurrent();
    PerfDiagMaybeLog();

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

    PerfDiagReset();
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
