#include "mikupan_meshcache.h"
#include "mikupan_pipeline.h"
#include "mikupan_profiler.h"

#include <glad/gl.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define MIKUPAN_MESHCACHE_BUCKETS 1024

static MikuPan_MeshCacheEntry *g_buckets[MIKUPAN_MESHCACHE_BUCKETS] = {0};
static int g_initialised = 0;
static int g_enabled = 1;

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
    if (e->vao) glad_glDeleteVertexArrays(1, &e->vao);
    if (e->ibo) glad_glDeleteBuffers(1, &e->ibo);
    for (int i = 0; i < e->num_vbos; i++)
    {
        if (e->vbo[i]) glad_glDeleteBuffers(1, &e->vbo[i]);
    }
    free(e);
}

void MikuPan_MeshCache_Shutdown(void)
{
    MikuPan_MeshCache_Flush();
    g_initialised = 0;
}

MikuPan_MeshCacheEntry *MikuPan_MeshCache_Lookup(void *pPUHead)
{
    if (!g_initialised || pPUHead == NULL) return NULL;
    unsigned int h = hash_ptr(pPUHead);
    for (MikuPan_MeshCacheEntry *e = g_buckets[h]; e != NULL; e = e->next)
    {
        if (e->pPUHead == pPUHead) return e;
    }
    return NULL;
}

MikuPan_MeshCacheEntry *MikuPan_MeshCache_Insert(
    void *pPUHead, void *sgd_top, int pipeline_type)
{
    if (!g_initialised) MikuPan_MeshCache_Init();

    MikuPan_PipelineInfo *p = MikuPan_GetPipelineInfo(
        (enum MikuPan_PipelineType)pipeline_type);
    if (p == NULL) return NULL;

    MikuPan_MeshCacheEntry *e = (MikuPan_MeshCacheEntry *)calloc(
        1, sizeof(MikuPan_MeshCacheEntry));
    e->pPUHead       = pPUHead;
    e->sgd_top       = sgd_top;
    e->pipeline_type = pipeline_type;
    e->num_vbos      = (p->num_buffers > 4) ? 4 : (int)p->num_buffers;

    glad_glGenVertexArrays(1, &e->vao);
    glad_glBindVertexArray(e->vao);

    /// Mirror the template pipeline's attribute layout, but redirect every
    /// glVertexAttribPointer call at our private VBOs instead of the shared
    /// streaming buffers. Whatever VBO is bound at glVertexAttribPointer time
    /// is what gets baked into the VAO.
    for (int b = 0; b < e->num_vbos; b++)
    {
        glad_glGenBuffers(1, &e->vbo[b]);
        glad_glBindBuffer(GL_ARRAY_BUFFER, e->vbo[b]);

        MikuPan_BufferObjectInfo *bi = &p->buffers[b];
        for (unsigned int a = 0; a < bi->num_attributes; a++)
        {
            MikuPan_AttributeInfo *ai = &bi->attributes[a];
            glad_glEnableVertexAttribArray(ai->index);
            glad_glVertexAttribPointer(
                ai->index, ai->size, GL_FLOAT, GL_FALSE,
                ai->stride, (void *)(uintptr_t)ai->offset);
        }
    }

    glad_glGenBuffers(1, &e->ibo);
    glad_glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, e->ibo);

    glad_glBindVertexArray(0);

    /// The cached bind shadows in mikupan_pipeline.c are now stale — anything
    /// the caller does next will go through MikuPan_BindVAO / BindBufferCached
    /// which will compare against shadowed state. Reset to force a re-issue.
    MikuPan_ResetGLBindCache();

    unsigned int h = hash_ptr(pPUHead);
    e->next = g_buckets[h];
    g_buckets[h] = e;

    return e;
}

void MikuPan_MeshCache_UploadVbo(MikuPan_MeshCacheEntry *entry,
                                 int idx, long size, const void *data)
{
    if (entry == NULL || idx < 0 || idx >= entry->num_vbos || size <= 0) return;
    glad_glBindBuffer(GL_ARRAY_BUFFER, entry->vbo[idx]);
    glad_glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)size, data, GL_STATIC_DRAW);
    MikuPan_ResetGLBindCache();
}

void MikuPan_MeshCache_UploadIbo(MikuPan_MeshCacheEntry *entry,
                                 long size, const void *data)
{
    if (entry == NULL || size <= 0) return;
    glad_glBindVertexArray(entry->vao);
    glad_glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, entry->ibo);
    glad_glBufferData(GL_ELEMENT_ARRAY_BUFFER, (GLsizeiptr)size, data, GL_STATIC_DRAW);
    MikuPan_ResetGLBindCache();
}

void MikuPan_MeshCache_StreamVbo(MikuPan_MeshCacheEntry *entry,
                                 int idx, long size, const void *data)
{
    if (entry == NULL || idx < 0 || idx >= entry->num_vbos || size <= 0) return;

    glad_glBindBuffer(GL_ARRAY_BUFFER, entry->vbo[idx]);

    void *ptr = glad_glMapBufferRange(GL_ARRAY_BUFFER, 0, (GLsizeiptr)size,
        GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT);

    if (ptr != NULL)
    {
        memcpy(ptr, data, (size_t)size);
        glad_glUnmapBuffer(GL_ARRAY_BUFFER);
    }
    else
    {
        /// First-call path: the VBO was created with no data store, so
        /// glMapBufferRange has nothing to map. glBufferData allocates AND
        /// uploads, then future frames hit the map path.
        glad_glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)size, data, GL_DYNAMIC_DRAW);
    }

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
