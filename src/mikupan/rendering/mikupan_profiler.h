#ifndef MIKUPAN_PROFILER_H
#define MIKUPAN_PROFILER_H

#include "SDL3/SDL_stdinc.h"  /* Uint64 */
#include "SDL3/SDL_timer.h"   /* SDL_GetPerformanceCounter — used by MIKUPAN_PERF_SCOPE */
#include <glad/gl.h>          /* GLenum / GLsizei / GLint — used by timed-draw wrappers */

#ifdef __cplusplus
extern "C" {
#endif

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

/// RAII-style scope helper. Declare at function entry; any return path
/// (including early returns) automatically accumulates the elapsed time
/// into the right bucket via __attribute__((cleanup)).
typedef struct { Uint64 t0; int section; } MikuPan_PerfScope;
void MikuPan_PerfScopeEnd(MikuPan_PerfScope *s);

/// `__LINE__`-suffixed variable name lets the same function declare multiple
/// MIKUPAN_PERF_SCOPE blocks (e.g. an outer aggregate plus a per-dispatch
/// inner section) without redeclaration errors.
#define MIKUPAN_PERF_SCOPE_PASTE_(a, b) a##b
#define MIKUPAN_PERF_SCOPE_PASTE(a, b)  MIKUPAN_PERF_SCOPE_PASTE_(a, b)

#define MIKUPAN_PERF_SCOPE(sect) \
    MikuPan_PerfScope MIKUPAN_PERF_SCOPE_PASTE(_perf_scope_, __LINE__) \
        __attribute__((cleanup(MikuPan_PerfScopeEnd))) = \
        { SDL_GetPerformanceCounter(), (sect) }

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
