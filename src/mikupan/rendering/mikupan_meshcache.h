#ifndef MIKUPAN_MESHCACHE_H
#define MIKUPAN_MESHCACHE_H

#include "mikupan/mikupan_basictypes.h"

#ifdef __cplusplus
extern "C" {
#endif

/// Render-pass namespace for the cache key. The same pPUHead is drawn in
/// multiple passes with different pipelines / topology (lit color pass, shadow
/// caster silhouette pass, shadow receiver pass), so each pass keeps its own
/// cache entry — they must not share GPU buffers.
enum MikuPan_MeshCacheKind
{
    MIKUPAN_MESHCACHE_KIND_MAIN     = 0, ///< Lit / color main scene pass
    MIKUPAN_MESHCACHE_KIND_CASTER   = 1, ///< Shadow caster silhouette pass
    MIKUPAN_MESHCACHE_KIND_RECEIVER = 2, ///< Shadow receiver projection pass
};

/// Per-mesh GPU buffer cache keyed by the SGD procedure-unit header pointer
/// (pPUHead) plus the render-pass kind. The pointer is stable for the lifetime of a loaded SGD, so once
/// a static mesh (furniture, room geometry) has had its vertex / UV / color /
/// index streams uploaded, every subsequent draw can reuse the same VBOs.
///
/// Each entry owns a private VAO whose attribute layout is cloned from one of
/// the existing MikuPan pipelines, so the per-frame draw path collapses to
/// glBindVertexArray + glDrawElements — no buffer uploads, no attrib pointer
/// shuffling.
typedef struct MikuPan_MeshCacheEntry
{
    void *pPUHead;     ///< Hash key — the SGD procedure-unit header pointer
    int   kind;        ///< Hash key — render-pass namespace (MikuPan_MeshCacheKind)
    void *sgd_top;     ///< Owning SGD top, used for region-level invalidation
    int   pipeline_type;
    int   index_count;
    u_int vao;
    u_int vbo[4];
    int   num_vbos;
    u_int ibo;
    /// Content fingerprint of the last data streamed into each VBO via
    /// MikuPan_MeshCache_StreamVbo. Lets the per-frame stream skip the upload
    /// (and the render-pass break it forces under SDL_GPU) when the bytes are
    /// unchanged — the common case for static furniture/room colour streams.
    unsigned long long stream_hash[4];
    long long stream_size[4];
    int   stream_valid[4];
    /// LRU / memory-budget bookkeeping. `last_used` is a monotonic access tick
    /// bumped on every lookup hit and insert; the eviction pass drops the
    /// smallest-tick entries when the cache exceeds its GPU-memory budget. This
    /// reclaims entries whose PS2-memory source was overwritten/freed without a
    /// clean SgMapUnit invalidation (they're simply never looked up again).
    /// `buf_bytes` tracks each owned GPU buffer's allocated size (vbo[0..3],
    /// ibo at index 4) so the budget reflects real usage.
    unsigned long long last_used;
    long long buf_bytes[5];
    struct MikuPan_MeshCacheEntry *next; ///< chaining within hash bucket
} MikuPan_MeshCacheEntry;

void MikuPan_MeshCache_Init(void);
void MikuPan_MeshCache_Shutdown(void);

/// Returns the cached entry for (`pPUHead`, `kind`), or NULL on miss. Does not
/// allocate. `kind` is a MikuPan_MeshCacheKind value.
MikuPan_MeshCacheEntry *MikuPan_MeshCache_Lookup(void *pPUHead, int kind);

/// Creates a fresh cache entry: a VAO with the named pipeline's attribute
/// layout, one VBO per pipeline buffer (up to 4), and an IBO. The returned
/// entry's VAO is bound on return — the caller is expected to fill the VBOs
/// and IBO with glBufferData(GL_STATIC_DRAW) (helpers below) and then set
/// `index_count` before drawing.
MikuPan_MeshCacheEntry *MikuPan_MeshCache_Insert(
    void *pPUHead, void *sgd_top, int pipeline_type, int kind);

/// One-shot static upload into one of the entry's VBOs. Allocates+copies.
void MikuPan_MeshCache_UploadVbo(MikuPan_MeshCacheEntry *entry,
                                 int idx, long long size, const void *data);

/// One-shot static upload into the entry's IBO.
void MikuPan_MeshCache_UploadIbo(MikuPan_MeshCacheEntry *entry,
                                 long long size, const void *data);

/// Per-frame streaming upload into a cache-owned VBO — used for the partial
/// cache pattern (e.g. CPU-skinned 0x2/0xA meshes) where indices/UVs/VAO are
/// static but positions+normals change every frame. Mirrors the
/// glMapBufferRange(MAP_INVALIDATE_BUFFER) semantics of MikuPan_StreamUploadFull
/// so the GL driver gets the orphan-and-rewrite hint.
void MikuPan_MeshCache_StreamVbo(MikuPan_MeshCacheEntry *entry,
                                 int idx, long long size, const void *data);

/// Drop every entry whose owning sgd_top matches. Called from SgMapUnit when
/// fresh data is detected at an address that may have been cached before.
void MikuPan_MeshCache_InvalidateSgd(void *sgd_top);

/// Wipe the cache. Called on global resets (e.g. InitModelLoad).
void MikuPan_MeshCache_Flush(void);

/// Runtime enable/disable. When disabled, callers fall back to the original
/// per-frame streaming-buffer path so the user can A/B steady-state perf.
/// Disabling does NOT free existing entries — they sit dormant and become
/// reusable as soon as caching is re-enabled (provided the underlying SGD
/// data hasn't been reloaded in the meantime).
void MikuPan_MeshCache_SetEnabled(int enabled);
int  MikuPan_MeshCache_IsEnabled(void);
void MikuPan_MeshCache_GetStats(unsigned long long* bytes,
                                 unsigned int* buffers,
                                 unsigned int* entries);

#ifdef __cplusplus
}
#endif

#endif // MIKUPAN_MESHCACHE_H
