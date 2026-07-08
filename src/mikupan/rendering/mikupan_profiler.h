#ifndef MIKUPAN_PROFILER_H
#define MIKUPAN_PROFILER_H

#include "SDL3/SDL_stdinc.h"  /* Uint64 */
#include "SDL3/SDL_timer.h"   /* SDL_GetPerformanceCounter — used by MIKUPAN_PERF_SCOPE */
#include "mikupan_gl_compat.h"

#ifdef __cplusplus
extern "C" {
#endif

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

/// CPU section IDs for the per-section perf graph. Stable integers — the UI
/// panel feeds matching values into MikuPan_PerfGetSectionMs(). Append new
/// IDs at the end so existing IDs keep their values.
typedef enum
{
    PERF_SECT_MESH_RENDER = 0,    ///< total time in MikuPan_RenderMeshType* calls (outer)
    PERF_SECT_SPRITE_RENDER,      ///< total time in 2D sprite/UI draws
    PERF_SECT_BATCH_FLUSH,        ///< MikuPan_FlushMeshBatch / FlushTexturedSpriteBatch
    PERF_SECT_DRAWUI,             ///< MikuPan_DrawUi (game UI text + ImGui submission)
    PERF_SECT_RENDERUI,           ///< MikuPan_RenderUi (ImGui actual render)
    PERF_SECT_DRAW_SUBMIT,        ///< wall-clock spent inside glDrawElements/glDrawArrays
    PERF_SECT_BUFFER_UPLOAD,      ///< wall-clock in stream-upload helper (map+memcpy+unmap)
    PERF_SECT_STATE_CHANGE,       ///< wall-clock in shader/texture/render-state setters (outer)
    PERF_SECT_SC_SHADER,          ///< sub: glUseProgram / MikuPan_SetCurrentShaderProgram
    PERF_SECT_SC_TEXTURE,         ///< sub: MikuPan_SetTexture (incl. hash lookup + bind)
    PERF_SECT_SC_RS3D,            ///< sub: MikuPan_SetRenderState3D (depth/cull/blend)
    PERF_SECT_SC_VAO,             ///< sub: MikuPan_BindVAO (glBindVertexArray)
    /// Per-mesh-type sub-sections of MESH_RENDER. Their sum is approximately
    /// MESH_RENDER minus the dispatch/early-return overhead.
    PERF_SECT_MESH_0x2,           ///< sub: MikuPan_RenderMeshType0x2 with mesh_type 0x2
    PERF_SECT_MESH_0xA,           ///< sub: MikuPan_RenderMeshType0x2 with mesh_type 0xA
    PERF_SECT_MESH_0x10,          ///< sub: MikuPan_RenderMeshType0x32 with mesh_type 0x10
    PERF_SECT_MESH_0x12,          ///< sub: MikuPan_RenderMeshType0x32 with mesh_type 0x12
    PERF_SECT_MESH_0x32,          ///< sub: MikuPan_RenderMeshType0x32 with mesh_type 0x32
    PERF_SECT_MESH_0x82,          ///< sub: MikuPan_RenderMeshType0x82
    /// Sub-sections of SC_TEXTURE — answer "where exactly does the texture
    /// state change spend its time?". Their sum is approximately SC_TEXTURE
    /// minus the dispatch overhead between the inner timers.
    PERF_SECT_TEX_L1_LOOKUP,      ///< sub: MikuPan_LookupTextureByTex0 (raw tex0 → info hash table)
    PERF_SECT_TEX_HASH,           ///< sub: MikuPan_GetTextureHash (XXH3 over GS memory — hot on L1 miss)
    PERF_SECT_TEX_L2_LOOKUP,      ///< sub: MikuPan_GetTextureInfo (by GS-memory hash)
    PERF_SECT_TEX_CREATE,         ///< sub: MikuPan_CreateGLTexture (glTexImage2D + glGenerateMipmap)
    PERF_SECT_TEX_BIND,           ///< sub: MikuPan_BindTexture2DCached (glBindTexture under the cache)
    /// "Other / unknown" breakdown — uninstrumented PS2/SG scene-graph emulation
    /// that runs between frame-start and the timed render calls. These are
    /// mutually-exclusive leaf timers, subtracted from "Other" in the UI.
    PERF_SECT_SKIN_PREP,          ///< SetVUVNData/Post: GPU-skin gather + palette / CPU skin
    PERF_SECT_COORD_CALC,         ///< CalcCoordinate → SetLWS (per-bone world matrices)
    PERF_SECT_LIGHT_SETUP,        ///< SetLightData (per-coord light transform)
    PERF_SECT_COUNT
} MikuPan_PerfSection;

/* ── Section timing ─────────────────────────────────────────────────── */

Uint64 MikuPan_PerfBegin(void);
void   MikuPan_PerfEnd(MikuPan_PerfSection sect, Uint64 t0);

/// Scope helper state. C++ users wrap this in MikuPan_PerfScopeGuard below;
/// C/GNU users can still use __attribute__((cleanup)).
typedef struct { Uint64 t0; int section; } MikuPan_PerfScope;
void MikuPan_PerfScopeEnd(MikuPan_PerfScope *s);

#ifdef __cplusplus
}

struct MikuPan_PerfScopeGuard
{
    MikuPan_PerfScope scope;

    explicit MikuPan_PerfScopeGuard(int section)
        : scope{SDL_GetPerformanceCounter(), section}
    {
    }

    ~MikuPan_PerfScopeGuard()
    {
        MikuPan_PerfScopeEnd(&scope);
    }

    MikuPan_PerfScopeGuard(const MikuPan_PerfScopeGuard&) = delete;
    MikuPan_PerfScopeGuard& operator=(const MikuPan_PerfScopeGuard&) = delete;
};

extern "C" {
#endif

/// `__LINE__`-suffixed variable name lets the same function declare multiple
/// MIKUPAN_PERF_SCOPE blocks (e.g. an outer aggregate plus a per-dispatch
/// inner section) without redeclaration errors.
#define MIKUPAN_PERF_SCOPE_PASTE_(a, b) a##b
#define MIKUPAN_PERF_SCOPE_PASTE(a, b)  MIKUPAN_PERF_SCOPE_PASTE_(a, b)

#ifdef __cplusplus
#define MIKUPAN_PERF_SCOPE(sect)                                               \
    MikuPan_PerfScopeGuard MIKUPAN_PERF_SCOPE_PASTE(_perf_scope_, __LINE__)(sect)
#elif defined(__GNUC__) || defined(__clang__)
#define MIKUPAN_PERF_SCOPE(sect) \
    MikuPan_PerfScope MIKUPAN_PERF_SCOPE_PASTE(_perf_scope_, __LINE__) \
        __attribute__((cleanup(MikuPan_PerfScopeEnd))) = \
        { SDL_GetPerformanceCounter(), (sect) }
#else
#define MIKUPAN_PERF_SCOPE(sect) \
    MikuPan_PerfScope MIKUPAN_PERF_SCOPE_PASTE(_perf_scope_, __LINE__) = \
        { SDL_GetPerformanceCounter(), (sect) }; \
    (void)MIKUPAN_PERF_SCOPE_PASTE(_perf_scope_, __LINE__)
#endif

/* ── Timed glDraw wrappers ──────────────────────────────────────────── */

/// Each wrapper contributes its CPU-side wall-clock to PERF_SECT_DRAW_SUBMIT
/// so the perf graph can show whether the bottleneck is real driver overhead
/// per-draw vs surrounding code.
void MikuPan_TimedDrawElements(GLenum mode, GLsizei count, GLenum type, const void *indices);
void MikuPan_TimedDrawArrays(GLenum mode, GLint first, GLsizei count);

/* Frame timing (CPU / SDL_GPU idle-wait split) */

/// Split CPU submission and post-submit SDL_GPU idle wait for the perf-debug
/// graph. The wait value is a back-pressure/presentation signal, not a
/// hardware GPU-duration query, and can include vsync pacing.
void  MikuPan_PerfBeginFrame(void);
void  MikuPan_PerfEndFrame(void);
void  MikuPan_PerfResetFrame(void);
void  MikuPan_PerfSetGpuWaitEnabled(int enabled);
int   MikuPan_PerfIsGpuWaitEnabled(void);
float MikuPan_GetLastFrameCpuMs(void);
float MikuPan_GetLastFrameGpuMs(void);

/* ── Counter increments ─────────────────────────────────────────────── */

void MikuPan_PerfStateChange(void);
void MikuPan_PerfDrawCall(void);
void MikuPan_PerfMeshCacheHit(void);
void MikuPan_PerfMeshCacheMissNew(void);
void MikuPan_PerfMeshCacheMissFull(void);
void MikuPan_PerfTexL1Hit(void);
void MikuPan_PerfTexL1Miss(void);


typedef enum
{
    MIKUPAN_PERF_UPLOAD_OTHER = 0,
    MIKUPAN_PERF_UPLOAD_MESH0X2_CPU_STREAM,
    MIKUPAN_PERF_UPLOAD_MESH0X2_GPU_BIND,
    MIKUPAN_PERF_UPLOAD_MESH0X2_UV,
    MIKUPAN_PERF_UPLOAD_MESH0X2_IBO,
    MIKUPAN_PERF_UPLOAD_SKIN_PALETTE,
    MIKUPAN_PERF_UPLOAD_COUNT
} MikuPan_PerfUploadContext;

typedef enum
{
    MIKUPAN_PERF_SKIN_COPY = 0,
    MIKUPAN_PERF_SKIN_WEIGHTED_COPY,
    MIKUPAN_PERF_SKIN_CPU_CALC,
    MIKUPAN_PERF_SKIN_GPU,
    MIKUPAN_PERF_SKIN_COUNT
} MikuPan_PerfSkinPath;

typedef enum
{
    MIKUPAN_PERF_SKIN_REJECT_UNSUPPORTED_VTYPE = 0,
    MIKUPAN_PERF_SKIN_REJECT_MESH_TYPE,
    MIKUPAN_PERF_SKIN_REJECT_GPU_DISABLED,
    MIKUPAN_PERF_SKIN_REJECT_SHADOW_PASS,
    MIKUPAN_PERF_SKIN_REJECT_RECEIVER_PASS,
    MIKUPAN_PERF_SKIN_REJECT_WEIGHTED_DISABLED,
    MIKUPAN_PERF_SKIN_REJECT_WEIGHTED_PREP_FAILED,
    MIKUPAN_PERF_SKIN_REJECT_WEIGHTED_INVALID_ARGS,
    MIKUPAN_PERF_SKIN_REJECT_WEIGHTED_NO_LIST,
    MIKUPAN_PERF_SKIN_REJECT_WEIGHTED_NO_VERTEX_SOURCE,
    MIKUPAN_PERF_SKIN_REJECT_WEIGHTED_NO_NORMAL_SOURCE,
    MIKUPAN_PERF_SKIN_REJECT_WEIGHTED_LIST_PTR_NULL,
    MIKUPAN_PERF_SKIN_REJECT_WEIGHTED_VERTEX_PTR_NULL,
    MIKUPAN_PERF_SKIN_REJECT_WEIGHTED_NORMAL_PTR_NULL,
    MIKUPAN_PERF_SKIN_REJECT_WEIGHTED_POS_INDEX_INVALID,
    MIKUPAN_PERF_SKIN_REJECT_WEIGHTED_NRM_INDEX_INVALID,
    MIKUPAN_PERF_SKIN_REJECT_WEIGHTED_POS_LIST_MISS,
    MIKUPAN_PERF_SKIN_REJECT_WEIGHTED_NRM_LIST_MISS,
    MIKUPAN_PERF_SKIN_REJECT_LEGACY_PREPASS,
    MIKUPAN_PERF_SKIN_REJECT_COUNT
} MikuPan_PerfSkinRejectReason;

#define MIKUPAN_PERF_SKIN_VTYPE_COUNT 256

typedef struct
{
    unsigned int frame;
    float cpu_ms;
    float gpu_wait_ms;
    int draw_calls;
    int state_changes;
    int mesh_cache_hits;
    int mesh_cache_misses_new;
    int mesh_cache_misses_full;
    int tex_l1_hits;
    int tex_l1_misses;
    int upload_context;
    int gpu_uploads;
    unsigned long long gpu_upload_bytes;
    int gpu_upload_grows;
    int gpu_upload_flushes;
    int render_pass_flushes;
    int upload_counts[MIKUPAN_PERF_UPLOAD_COUNT];
    unsigned long long upload_bytes[MIKUPAN_PERF_UPLOAD_COUNT];
    int mesh0x2_calls;
    int mesh0xA_calls;
    int mesh0x2_cpu;
    int mesh0x2_gpu;
    int mesh0x2_shadow;
    int mesh0x2_receiver;
    int mesh0x2_cache_on;
    int mesh0x2_cache_hit;
    int mesh0x2_static_miss;
    int mesh0x2_submeshes;
    int mesh0x2_vertices;
    int mesh0x2_indices;
    int mesh0x2_max_submeshes;
    int mesh0x2_max_vertices;
    int mesh0x2_max_indices;
    int mesh0x2_stream_skips;
    unsigned long long mesh0x2_stream_skip_bytes;
    int skin_vtype2;
    int skin_vtype3;
    int skin_other;
    int skin_allowed;
    int skin_blocked;
    int skin_paths[MIKUPAN_PERF_SKIN_COUNT];
    int skin_vertices[MIKUPAN_PERF_SKIN_COUNT];
    int skin_vtype_counts[MIKUPAN_PERF_SKIN_VTYPE_COUNT];
    int skin_vtype_vertices[MIKUPAN_PERF_SKIN_VTYPE_COUNT];
    int skin_vtype_path_counts[MIKUPAN_PERF_SKIN_COUNT][MIKUPAN_PERF_SKIN_VTYPE_COUNT];
    int skin_vtype_path_vertices[MIKUPAN_PERF_SKIN_COUNT][MIKUPAN_PERF_SKIN_VTYPE_COUNT];
    int skin_reject_counts[MIKUPAN_PERF_SKIN_REJECT_COUNT];
    int skin_reject_vertices[MIKUPAN_PERF_SKIN_REJECT_COUNT];
    int skin_reject_vtype_counts[MIKUPAN_PERF_SKIN_REJECT_COUNT][MIKUPAN_PERF_SKIN_VTYPE_COUNT];
    int skin_reject_vtype_vertices[MIKUPAN_PERF_SKIN_REJECT_COUNT][MIKUPAN_PERF_SKIN_VTYPE_COUNT];
    int drawgirl_normal;
    int drawgirl_mirror;
    int drawgirl_rendered;
    int drawgirl_hidden;
    int drawgirl_tofu;
    int drawgirl_subobj_calls;
    int drawgirl_subobj_total;
    int drawgirl_sort_items;
} MikuPan_PerfDiagSnapshot;

int  MikuPan_PerfDiagIsEnabled(void);
void MikuPan_PerfDiagSetEnabled(int enabled);
int  MikuPan_PerfDiagLogEnabled(void);
void MikuPan_PerfDiagSetLogEnabled(int enabled);
int  MikuPan_PerfDiagGetLogInterval(void);
void MikuPan_PerfDiagSetLogInterval(int interval);
void MikuPan_PerfDiagGetLastSnapshot(MikuPan_PerfDiagSnapshot *out);
const char *MikuPan_PerfDiagUploadContextName(int context);
const char *MikuPan_PerfDiagSkinRejectName(int reason);

int  MikuPan_PerfSetUploadContext(int context);
int  MikuPan_PerfGetUploadContext(void);
void MikuPan_PerfRecordGpuUpload(unsigned int buffer_id, unsigned int size, int grew, int flushed_render_pass);
void MikuPan_PerfRecordRenderPassFlush(void);
void MikuPan_PerfRecordMesh0x2Packet(int mesh_type, int is_skin, int cache_on, int cache_hit, int need_static_upload, int num_mesh, int vnum, int index_count, int shadow_pass, int receiver_pass);
void MikuPan_PerfRecordMesh0x2StreamSkip(long long size);
void MikuPan_PerfRecordSkinPath(int vtype, int path, int allow_gpu_skin, int vertices);
void MikuPan_PerfRecordSkinReject(int vtype, int reason, int vertices);
void MikuPan_PerfRecordDrawGirl(int in_mirror, int rendered, int hidden, int tofu);
void MikuPan_PerfRecordDrawGirlSubObj(int obj_num);
void MikuPan_PerfRecordDrawGirlSubObjItem(int index);

/* ── Last-frame queries (consumed by the perf-debug UI panel) ───────── */

float MikuPan_PerfGetSectionMs(int section);
int   MikuPan_PerfGetMeshCacheHits(void);
int   MikuPan_PerfGetMeshCacheMissesNew(void);
int   MikuPan_PerfGetMeshCacheMissesFull(void);
int   MikuPan_PerfGetTexL1Hits(void);
int   MikuPan_PerfGetTexL1Misses(void);

/* ── Current-frame snapshots (read mid-frame, e.g. for debug logging) ─ */

int MikuPan_PerfGetStateChangesCurrent(void);
int MikuPan_PerfGetDrawCallsCurrent(void);
int MikuPan_PerfGetMeshCacheHitsCurrent(void);
int MikuPan_PerfGetMeshCacheMissesNewCurrent(void);
int MikuPan_PerfGetMeshCacheMissesFullCurrent(void);

typedef enum
{
    MIKUPAN_DRAW_SOURCE_UNKNOWN = 0,
    MIKUPAN_DRAW_SOURCE_MAIN_SCENE,
    MIKUPAN_DRAW_SOURCE_ROOM_BACKGROUND,
    MIKUPAN_DRAW_SOURCE_MIRROR_PASS,
    MIKUPAN_DRAW_SOURCE_ROOM_NEAR,
    MIKUPAN_DRAW_SOURCE_ROOM_SS,
    MIKUPAN_DRAW_SOURCE_FURNITURE,
    MIKUPAN_DRAW_SOURCE_DRAWGIRL_NORMAL,
    MIKUPAN_DRAW_SOURCE_DRAWGIRL_MIRROR,
    MIKUPAN_DRAW_SOURCE_PLAYER_SHADOW,
    MIKUPAN_DRAW_SOURCE_ROOM_SHADOW,
    MIKUPAN_DRAW_SOURCE_EFFECT_2D,
    MIKUPAN_DRAW_SOURCE_SPRITE,
    MIKUPAN_DRAW_SOURCE_POSTPROCESS,
    MIKUPAN_DRAW_SOURCE_UI,
    MIKUPAN_DRAW_SOURCE_RML,
    MIKUPAN_DRAW_SOURCE_DEBUG_DRAW,
    MIKUPAN_DRAW_SOURCE_TEXTURE_COPY,
    MIKUPAN_DRAW_SOURCE_ENEMY,
    MIKUPAN_DRAW_SOURCE_FLY_MOVE,
    MIKUPAN_DRAW_SOURCE_COUNT
} MikuPan_DrawSourceKind;

typedef struct
{
    int kind;
    int detail0;
    int detail1;
} MikuPan_DrawSourceState;

int MikuPan_DrawSourcePush(int kind, int detail0, int detail1);
void MikuPan_DrawSourcePop(int token);
void MikuPan_DrawSourceGetCurrent(MikuPan_DrawSourceState *out);
const char *MikuPan_DrawSourceName(int kind);

/* ── Per-draw-call capture (debug inspector) ───────────────────────── */
/// One captured glDraw* invocation. Populated from inside the timed-draw
/// wrappers when capture is enabled; consumed by the ImGui inspector.
typedef struct
{
    GLenum  mode;          ///< GL primitive (GL_TRIANGLES, GL_TRIANGLE_STRIP, …)
    int     count;         ///< vertex / index count
    int     first;         ///< glDrawArrays.first (0 for elements)
    int     is_elements;   ///< 1 = glDrawElements, 0 = glDrawArrays
    GLenum  index_type;    ///< GL_UNSIGNED_INT/SHORT for elements (0 for arrays)
    unsigned int program;  ///< raw GL program id at draw time
    int     shader_idx;    ///< index in shader_list[] for the program (-1 if unknown)
    unsigned int vao;      ///< bound VAO at draw time (cached)
    unsigned int texture0; ///< bound 2D texture on unit 0 (cached)
    int     source_kind;
    int     source_detail0;
    int     source_detail1;
    int     source_parent_kind;
    int     source_parent_detail0;
    int     source_parent_detail1;
    int     skipped;       ///< 1 if isolation suppressed this draw
    float   ms;            ///< CPU submit wall-clock (0 when skipped)
} MikuPan_DrawCapEntry;

/// Master switch — when off, the timed-draw wrappers run their fast path
/// and contribute nothing to the inspector. When on, every draw is logged
/// (capped at the ring's capacity) until the next BeginFrame clears it.
void MikuPan_DrawCapEnable(int enabled);
int  MikuPan_DrawCapEnabled(void);

/// Cleared at frame start (called from MikuPan_PerfBeginFrame).
void MikuPan_DrawCapBeginFrame(void);

/// `MikuPan_DrawCapCount` returns how many entries were captured in the
/// last completed frame; `EntryAt` returns NULL when idx is out of range.
int                                 MikuPan_DrawCapLastCount(void);
const MikuPan_DrawCapEntry *        MikuPan_DrawCapLastEntryAt(int idx);

/// Isolation: when set to a non-negative value, the timed-draw wrappers
/// skip every draw whose 0-based index in the current frame is NOT equal
/// to `idx`. Pass -1 to draw everything. Useful for "what does just this
/// one draw paint?" — the inspector wires a Solo button to it per row.
void MikuPan_DrawCapSetIsolate(int idx);
int  MikuPan_DrawCapGetIsolate(void);

#ifdef __cplusplus
}
#endif

#endif /* MIKUPAN_PROFILER_H */
