#ifndef MIKUPAN_MESHCACHE_H
#define MIKUPAN_MESHCACHE_H

#include "mikupan/mikupan_basictypes.h"

#ifdef __cplusplus
extern "C" {
#endif

/// Per-mesh GPU buffer cache keyed by the SGD procedure-unit header pointer
/// (pPUHead). The pointer is stable for the lifetime of a loaded SGD, so once
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
    void *sgd_top;     ///< Owning SGD top, used for region-level invalidation
    int   pipeline_type;
    int   index_count;
    u_int vao;
    u_int vbo[4];
    int   num_vbos;
    u_int ibo;
    struct MikuPan_MeshCacheEntry *next; ///< chaining within hash bucket
} MikuPan_MeshCacheEntry;

void MikuPan_MeshCache_Init(void);
void MikuPan_MeshCache_Shutdown(void);

/// Returns the cached entry for `pPUHead`, or NULL on miss. Does not allocate.
MikuPan_MeshCacheEntry *MikuPan_MeshCache_Lookup(void *pPUHead);

/// Creates a fresh cache entry: a VAO with the named pipeline's attribute
/// layout, one VBO per pipeline buffer (up to 4), and an IBO. The returned
/// entry's VAO is bound on return — the caller is expected to fill the VBOs
/// and IBO with glBufferData(GL_STATIC_DRAW) (helpers below) and then set
/// `index_count` before drawing.
MikuPan_MeshCacheEntry *MikuPan_MeshCache_Insert(
    void *pPUHead, void *sgd_top, int pipeline_type);

/// One-shot static upload into one of the entry's VBOs. Allocates+copies.
void MikuPan_MeshCache_UploadVbo(MikuPan_MeshCacheEntry *entry,
                                 int idx, long size, const void *data);

/// One-shot static upload into the entry's IBO.
void MikuPan_MeshCache_UploadIbo(MikuPan_MeshCacheEntry *entry,
                                 long size, const void *data);

/// Per-frame streaming upload into a cache-owned VBO — used for the partial
/// cache pattern (e.g. CPU-skinned 0x2/0xA meshes) where indices/UVs/VAO are
/// static but positions+normals change every frame. Mirrors the
/// glMapBufferRange(MAP_INVALIDATE_BUFFER) semantics of MikuPan_StreamUploadFull
/// so the GL driver gets the orphan-and-rewrite hint.
void MikuPan_MeshCache_StreamVbo(MikuPan_MeshCacheEntry *entry,
                                 int idx, long size, const void *data);

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

#ifdef __cplusplus
}
#endif

#endif // MIKUPAN_MESHCACHE_H
