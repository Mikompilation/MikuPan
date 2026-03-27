#include "mikupan_texture_manager.h"

extern "C"
{
#include "mikupan/rendering/mikupan_renderer.h"
#include <stdlib.h>
}

std::unordered_map<uint64_t, MikuPan_TextureInfo*> mikupan_render_texture_atlas;
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