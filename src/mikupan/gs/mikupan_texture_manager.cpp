#include "mikupan_texture_manager.h"

extern "C"
{
#include "mikupan/rendering/mikupan_renderer.h"
#include "mikupan/gs/mikupan_gs_c.h"
#include <stdlib.h>
}

std::unordered_map<uint64_t, MikuPan_TextureInfo*> mikupan_render_texture_atlas;

/// L1 cache keyed on the raw 64-bit tex0 register value. Lets MikuPan_SetTexture
/// skip the (expensive) per-call XXH3 over GS memory whenever the same tex0
/// register has been resolved before. Each entry remembers the GS-memory range
/// it samples; invalidation is selective — only entries whose range overlaps
/// a recent MikuPan_GsUpload are dropped, instead of wiping the entire cache.
struct L1Entry
{
    MikuPan_TextureInfo *info;
    int gs_addr;
    int gs_size;
};
std::unordered_map<uint64_t, L1Entry> mikupan_tex0_to_info_cache;

bool first_upload_done = false;
bool request_texture_cache_flush = false;

void MikuPan_FirstUploadDone()
{
    first_upload_done = true;
}

bool MikuPan_IsFirstUploadDone()
{
    return first_upload_done;
}

/**
 * Flushes the texture cache by freeing all stored textures.
 * SHOULD ONLY BE CALLED FROM THE MAIN THREAD!
 */
void MikuPan_FlushTextureCache()
{
    if (!request_texture_cache_flush)
    {
        return;
    }

    for (auto texture : mikupan_render_texture_atlas)
    {
        MikuPan_DeleteTexture(texture.second);
    }

    mikupan_render_texture_atlas.clear();
    mikupan_tex0_to_info_cache.clear();

    request_texture_cache_flush = false;
}

int MikuPan_IsTextureCacheFlushRequested()
{
    return request_texture_cache_flush;
}

void MikuPan_RequestFlushTextureCache()
{
    request_texture_cache_flush = true;
}

MikuPan_TextureInfo* MikuPan_GetTex0TextureInfoAndCount(uint64_t tex0, int* count)
{
    MikuPan_TextureInfo* texture = nullptr;
    *count = 0;

    for (const auto& [key, info] : mikupan_render_texture_atlas)
    {
        if (info->tex0 == tex0)
        {
            texture = info;
            (*count)++;
        }
    }

    return texture;
}

MikuPan_TextureInfo* MikuPan_GetTextureInfo(uint64_t hash)
{
    if (!first_upload_done)
    {
        return nullptr;
    }

    if (hash == 0)
    {
        return nullptr;
    }

    if (auto el = mikupan_render_texture_atlas.find(hash);
        el != mikupan_render_texture_atlas.end())
    {
        return mikupan_render_texture_atlas[hash];
    }

    return nullptr;
}

void MikuPan_AddTexture(uint64_t hash, MikuPan_TextureInfo* texture_info)
{
    mikupan_render_texture_atlas[hash] = texture_info;
}

void MikuPan_ForEachTexture(void (*callback)(MikuPan_TextureInfo* tex, void* userdata), void* userdata)
{
    for (const auto& pair : mikupan_render_texture_atlas)
    {
        callback(pair.second, userdata);
    }
}

/// Drain callback: the C-API hands us each pending upload region in turn.
/// We scan the L1 once per region and erase entries whose stored GS range
/// overlaps. With ~10 uploads × ~100 entries that's ~1000 cheap integer
/// comparisons per frame — negligible compared to the XXH3 it replaces.
static void InvalidateOverlapping(int up_addr, int up_size, void *ud)
{
    auto &cache = *static_cast<std::unordered_map<uint64_t, L1Entry>*>(ud);
    const int up_end = up_addr + up_size;
    for (auto it = cache.begin(); it != cache.end(); )
    {
        const int e_end = it->second.gs_addr + it->second.gs_size;
        // Half-open interval overlap test.
        const bool overlaps =
            (it->second.gs_addr < up_end) && (up_addr < e_end);
        if (overlaps)
        {
            it = cache.erase(it);
        }
        else
        {
            ++it;
        }
    }
}

MikuPan_TextureInfo* MikuPan_LookupTextureByTex0(uint64_t tex0_value)
{
    // Lazy, selective invalidation: drain the GS-side pending-upload list and
    // drop only L1 entries whose region overlaps one of those uploads. The
    // remaining entries stay valid across frames, so post-upload lookups of
    // unrelated textures hit the L1 (avoiding the XXH3-over-GS-memory path).
    if (MikuPan_GsHasPendingUploads())
    {
        MikuPan_GsConsumePendingUploads(InvalidateOverlapping,
                                        &mikupan_tex0_to_info_cache);
    }

    auto it = mikupan_tex0_to_info_cache.find(tex0_value);
    return it != mikupan_tex0_to_info_cache.end() ? it->second.info : nullptr;
}

void MikuPan_RegisterTextureForTex0(uint64_t tex0_value, sceGsTex0 *tex0,
                                    MikuPan_TextureInfo* info)
{
    if (info == nullptr) return;

    L1Entry entry;
    entry.info = info;
    MikuPan_GetTextureGsRegion(tex0, &entry.gs_addr, &entry.gs_size);
    mikupan_tex0_to_info_cache[tex0_value] = entry;
}

void MikuPan_InvalidateTex0Cache(void)
{
    mikupan_tex0_to_info_cache.clear();
}