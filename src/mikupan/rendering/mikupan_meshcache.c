#include "mikupan_meshcache.h"
#include "mikupan_pipeline.h"
#include "mikupan_profiler.h"
#include "mikupan_gpu.h"

#include <glad/gl.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define MIKUPAN_MESHCACHE_BUCKETS 1024

/// GPU-memory budget for cached mesh buffers. When exceeded, the least-recently
/// used entries are evicted. This bounds the cache regardless of whether stale
/// entries (PS2 memory overwritten/freed without a clean invalidation) are ever
/// explicitly dropped — they fall to the bottom of the LRU and get reclaimed.
#define MIKUPAN_MESHCACHE_BUDGET_BYTES (320ull * 1024 * 1024)

/// Mesh buffers are created small and grown to the actual upload size (most PS2
/// meshes are far under the old fixed 4 MiB), so the budget tracks real usage.
#define MIKUPAN_MESHCACHE_BUF_INITIAL (4 * 1024)

static MikuPan_MeshCacheEntry *g_buckets[MIKUPAN_MESHCACHE_BUCKETS] = {0};
static int g_initialised = 0;
static int g_enabled = 1;
static unsigned long long g_cache_tick = 0;
static unsigned long long g_cache_bytes = 0;

/// Record a buffer (slot 0..3 = vbo, 4 = ibo) growing to `size`, updating the
/// global byte total. MikuPan_GPUUploadBuffer only ever grows a buffer, so the
/// tracked size mirrors the real GPU allocation.
static void account_buffer(MikuPan_MeshCacheEntry *e, int slot, long long size)
{
    if (size > e->buf_bytes[slot])
    {
        g_cache_bytes += (unsigned long long)(size - e->buf_bytes[slot]);
        e->buf_bytes[slot] = size;
    }
}

static inline unsigned int hash_ptr(const void *p)
{
    /// Pointers are at least 4-byte aligned; shift out the low bits before
    /// folding so adjacent allocations don't all collide into one bucket.
    uintptr_t v = (uintptr_t)p;
    v ^= v >> 16;
    return (unsigned int)((v >> 4) % MIKUPAN_MESHCACHE_BUCKETS);
}

void MikuPan_MeshCache_Init(void)
{
    if (g_initialised) return;
    memset(g_buckets, 0, sizeof(g_buckets));
    g_initialised = 1;
}

static void destroy_entry(MikuPan_MeshCacheEntry *e)
{
    for (int i = 0; i < 5; i++)
    {
        g_cache_bytes -= (unsigned long long)e->buf_bytes[i];
    }
    if (e->vao) MikuPan_GPUReleaseVertexArray(e->vao);
    if (e->ibo) MikuPan_GPUReleaseBuffer(e->ibo);
    for (int i = 0; i < e->num_vbos; i++)
    {
        if (e->vbo[i]) MikuPan_GPUReleaseBuffer(e->vbo[i]);
    }
    free(e);
}

/// Evict least-recently-used entries until the cache is back under budget. The
/// just-inserted / just-hit entries carry the highest ticks, so they are never
/// the eviction target — only genuinely cold entries are dropped.
static void evict_to_budget(void)
{
    while (g_cache_bytes > MIKUPAN_MESHCACHE_BUDGET_BYTES)
    {
        MikuPan_MeshCacheEntry **victim_link = NULL;
        unsigned long long oldest = (unsigned long long)-1;
        for (int b = 0; b < MIKUPAN_MESHCACHE_BUCKETS; b++)
        {
            for (MikuPan_MeshCacheEntry **link = &g_buckets[b];
                 *link != NULL; link = &(*link)->next)
            {
                /// Never evict the entry the caller is mid-render on — it holds
                /// the newest tick (set by the lookup/insert that returned it),
                /// and these eviction passes run from inside its own uploads.
                if ((*link)->last_used != g_cache_tick &&
                    (*link)->last_used < oldest)
                {
                    oldest = (*link)->last_used;
                    victim_link = link;
                }
            }
        }
        if (victim_link == NULL)
        {
            break; /* nothing evictable left (only the in-use entry remains) */
        }
        MikuPan_MeshCacheEntry *victim = *victim_link;
        *victim_link = victim->next;
        destroy_entry(victim);
    }
}

void MikuPan_MeshCache_Shutdown(void)
{
    MikuPan_MeshCache_Flush();
    g_initialised = 0;
}

MikuPan_MeshCacheEntry *MikuPan_MeshCache_Lookup(void *pPUHead, int kind)
{
    if (!g_initialised || pPUHead == NULL) return NULL;
    unsigned int h = hash_ptr(pPUHead);
    for (MikuPan_MeshCacheEntry *e = g_buckets[h]; e != NULL; e = e->next)
    {
        if (e->pPUHead == pPUHead && e->kind == kind)
        {
            e->last_used = ++g_cache_tick; /* keep hot entries out of the LRU */
            return e;
        }
    }
    return NULL;
}

MikuPan_MeshCacheEntry *MikuPan_MeshCache_Insert(
    void *pPUHead, void *sgd_top, int pipeline_type, int kind)
{
    if (!g_initialised) MikuPan_MeshCache_Init();

    MikuPan_PipelineInfo *p = MikuPan_GetPipelineInfo(
        (enum MikuPan_PipelineType)pipeline_type);
    if (p == NULL) return NULL;

    /// Evict any existing entry for this exact key first. Insert is only reached
    /// on a miss (no entry, or the entry's sgd_top went stale), so a stale entry
    /// must be destroyed here — otherwise re-inserting would orphan its VAO/VBOs
    /// and leak GPU memory every frame the key keeps missing.
    {
        unsigned int hk = hash_ptr(pPUHead);
        MikuPan_MeshCacheEntry **link = &g_buckets[hk];
        while (*link != NULL)
        {
            MikuPan_MeshCacheEntry *cur = *link;
            if (cur->pPUHead == pPUHead && cur->kind == kind)
            {
                *link = cur->next;
                destroy_entry(cur);
                break; /* keys are unique, so at most one match */
            }
            link = &cur->next;
        }
    }

    MikuPan_MeshCacheEntry *e = (MikuPan_MeshCacheEntry *)calloc(
        1, sizeof(MikuPan_MeshCacheEntry));
    e->pPUHead       = pPUHead;
    e->kind          = kind;
    e->sgd_top       = sgd_top;
    e->pipeline_type = pipeline_type;
    e->num_vbos      = (p->num_buffers > 4) ? 4 : (int)p->num_buffers;

    /// Mirror the template pipeline's vertex-buffer layout with private GPU
    /// buffers. Attribute metadata lives on the template pipeline and is read
    /// by the SDL_GPU pipeline creator. Buffers start small and grow to the
    /// actual upload size (MikuPan_GPUUploadBuffer grows on demand), so a small
    /// mesh doesn't pin the old fixed 4 MiB per buffer.
    for (int b = 0; b < e->num_vbos; b++)
    {
        e->vbo[b] = MikuPan_GPUCreateBuffer(
            MIKUPAN_MESHCACHE_BUF_INITIAL, MIKUPAN_GPU_BUFFER_VERTEX);
        account_buffer(e, b, MIKUPAN_MESHCACHE_BUF_INITIAL);
    }

    e->ibo = MikuPan_GPUCreateBuffer(
        MIKUPAN_MESHCACHE_BUF_INITIAL, MIKUPAN_GPU_BUFFER_INDEX);
    account_buffer(e, 4, MIKUPAN_MESHCACHE_BUF_INITIAL);
    e->vao = MikuPan_GPURegisterVertexArray(
        pipeline_type, (unsigned int)e->num_vbos, e->vbo, e->ibo);

    e->last_used = ++g_cache_tick;

    /// The cached bind shadows in mikupan_pipeline.c are now stale — anything
    /// the caller does next will go through MikuPan_BindVAO / BindBufferCached
    /// which will compare against shadowed state. Reset to force a re-issue.
    MikuPan_ResetGLBindCache();

    unsigned int h = hash_ptr(pPUHead);
    e->next = g_buckets[h];
    g_buckets[h] = e;

    evict_to_budget();

    return e;
}

void MikuPan_MeshCache_UploadVbo(MikuPan_MeshCacheEntry *entry,
                                 int idx, long long size, const void *data)
{
    if (entry == NULL || idx < 0 || idx >= entry->num_vbos || size <= 0) return;
    MikuPan_GPUUploadBuffer(entry->vbo[idx], (unsigned int)size, data);
    account_buffer(entry, idx, size);
    evict_to_budget();
    MikuPan_ResetGLBindCache();
}

void MikuPan_MeshCache_UploadIbo(MikuPan_MeshCacheEntry *entry,
                                 long long size, const void *data)
{
    if (entry == NULL || size <= 0) return;
    MikuPan_GPUUploadBuffer(entry->ibo, (unsigned int)size, data);
    account_buffer(entry, 4, size);
    evict_to_budget();
    MikuPan_ResetGLBindCache();
}

static unsigned long long meshcache_hash(const void *data, long long size)
{
    /// FNV-1a over the raw bytes. Cheap (memory-bandwidth bound) relative to the
    /// GPU upload + render-pass teardown it lets us skip.
    const unsigned char *p = (const unsigned char *)data;
    unsigned long long h = 1469598103934665603ULL;
    for (long long i = 0; i < size; i++)
    {
        h ^= (unsigned long long)p[i];
        h *= 1099511628211ULL;
    }
    return h;
}

void MikuPan_MeshCache_StreamVbo(MikuPan_MeshCacheEntry *entry,
                                 int idx, long long size, const void *data)
{
    if (entry == NULL || idx < 0 || idx >= entry->num_vbos || size <= 0) return;

    /// Skip the upload when this VBO already holds identical bytes. Under the
    /// SDL_GPU backend MikuPan_GPUUploadBuffer ends the active render pass (copy
    /// passes cannot run inside one), so an unconditional per-mesh stream tore
    /// the scene pass down and rebuilt it for every cached mesh. Skipping the
    /// no-op uploads keeps all unchanged geometry inside a single render pass.
    unsigned long long h = meshcache_hash(data, size);
    if (entry->stream_valid[idx] &&
        entry->stream_size[idx] == size &&
        entry->stream_hash[idx] == h)
    {
        return;
    }

    MikuPan_GPUUploadBuffer(entry->vbo[idx], (unsigned int)size, data);
    account_buffer(entry, idx, size);
    entry->stream_hash[idx]  = h;
    entry->stream_size[idx]  = size;
    entry->stream_valid[idx] = 1;
    evict_to_budget();
    MikuPan_ResetGLBindCache();
}

void MikuPan_MeshCache_InvalidateSgd(void *sgd_top)
{
    if (!g_initialised || sgd_top == NULL) return;
    for (int b = 0; b < MIKUPAN_MESHCACHE_BUCKETS; b++)
    {
        MikuPan_MeshCacheEntry **link = &g_buckets[b];
        while (*link != NULL)
        {
            MikuPan_MeshCacheEntry *cur = *link;
            if (cur->sgd_top == sgd_top)
            {
                *link = cur->next;
                destroy_entry(cur);
            }
            else
            {
                link = &cur->next;
            }
        }
    }
    MikuPan_ResetGLBindCache();
}

void MikuPan_MeshCache_Flush(void)
{
    for (int b = 0; b < MIKUPAN_MESHCACHE_BUCKETS; b++)
    {
        MikuPan_MeshCacheEntry *e = g_buckets[b];
        while (e != NULL)
        {
            MikuPan_MeshCacheEntry *next = e->next;
            destroy_entry(e);
            e = next;
        }
        g_buckets[b] = NULL;
    }
    MikuPan_ResetGLBindCache();
}

void MikuPan_MeshCache_SetEnabled(int enabled)
{
    g_enabled = enabled ? 1 : 0;
}

int MikuPan_MeshCache_IsEnabled(void)
{
    return g_enabled;
}
