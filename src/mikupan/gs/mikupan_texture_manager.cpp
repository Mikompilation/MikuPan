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
 * Font textures (held by s_fnt_texture[]) are preserved in the atlas so
 * they remain accessible after the flush without re-creation.
 * SHOULD ONLY BE CALLED FROM THE MAIN THREAD!
 */
void MikuPan_FlushTextureCache()
{
    if (!request_texture_cache_flush)
    {
        return;
    }

    auto it = mikupan_render_texture_atlas.begin();
    while (it != mikupan_render_texture_atlas.end())
    {
        if (MikuPan_IsFontTexture(it->second))
        {
            ++it;
        }
        else
        {
            MikuPan_DeleteTexture(it->second);
            it = mikupan_render_texture_atlas.erase(it);
        }
    }

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

void MikuPan_ShutdownTextureCache()
{
    auto it = mikupan_render_texture_atlas.begin();
    while (it != mikupan_render_texture_atlas.end())
    {
        if (!MikuPan_IsFontTexture(it->second))
        {
            MikuPan_DeleteTexture(it->second);
        }
        it = mikupan_render_texture_atlas.erase(it);
    }
}