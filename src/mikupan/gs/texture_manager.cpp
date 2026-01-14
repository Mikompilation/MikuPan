#include "texture_manager.h"

extern "C"
{
#include "mikupan/rendering/mikupan_renderer.h"
#include <stdlib.h>
}

std::unordered_map<unsigned long long, unsigned char*> texture_atlas;
std::unordered_map<unsigned long long, MikuPan_TextureInfo*> mikupan_render_texture_atlas;
bool first_upload_done = false;
bool request_texture_cache_flush = false;

#define CONVERT_TEX0_TO_ULONG(tex0) *(u_long*)tex0

void MikuPan_AddTexturePixelBuffer(sceGsTex0* tex0, unsigned char *img)
{
    texture_atlas[CONVERT_TEX0_TO_ULONG(tex0)] = img;
}

u_char* MikuPan_GetTexturePixelBuffer(sceGsTex0* tex0)
{
    if (!first_upload_done)
    {
        return nullptr;
    }

    if (auto el = texture_atlas.find(CONVERT_TEX0_TO_ULONG(tex0)); el != texture_atlas.end())
    {
        return texture_atlas[CONVERT_TEX0_TO_ULONG(tex0)];
    }

    return nullptr;
}

void FirstUploadDone()
{
    first_upload_done = true;
}

bool IsFirstUploadDone()
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

    for (auto texture : texture_atlas)
    {
        free(texture.second);
    }

    texture_atlas.clear();

    for (auto texture : mikupan_render_texture_atlas)
    {
        MikuPan_DeleteTexture(texture.second);
    }

    mikupan_render_texture_atlas.clear();

    request_texture_cache_flush = false;
}

void MikuPan_RequestFlushTextureCache()
{
    request_texture_cache_flush = true;
}

MikuPan_TextureInfo* MikuPan_GetTextureInfo(sceGsTex0 *tex0)
{
    if (!first_upload_done)
    {
        return nullptr;
    }

    if (auto el = mikupan_render_texture_atlas.find(CONVERT_TEX0_TO_ULONG(tex0));
        el != mikupan_render_texture_atlas.end())
    {
        return mikupan_render_texture_atlas[CONVERT_TEX0_TO_ULONG(tex0)];
    }

    return nullptr;
}

void MikuPan_AddTexture(sceGsTex0 *tex0, MikuPan_TextureInfo* texture_info)
{
    mikupan_render_texture_atlas[CONVERT_TEX0_TO_ULONG(tex0)] = texture_info;
}