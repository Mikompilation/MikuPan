#include "texture_manager.h"

extern "C"
{
#include "rendering/mikupan_renderer.h"
#include <stdlib.h>
}


std::unordered_map<unsigned long long, unsigned char*> texture_atlas;
std::unordered_map<unsigned long long, void*> sdl_texture_atlas;
bool first_upload_done = false;
bool request_texture_cache_flush = false;

#define CONVERT_TEX0_TO_ULONG(tex0) *(u_long*)tex0

void AddTexture(sceGsTex0* tex0, unsigned char *img)
{
    texture_atlas[CONVERT_TEX0_TO_ULONG(tex0)] = img;
}

unsigned char * GetTexture(sceGsTex0* tex0)
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

void AddSDLTexture(sceGsTex0* tex0, void *img)
{
    sdl_texture_atlas[CONVERT_TEX0_TO_ULONG(tex0)] = img;
}

void *GetSDLTexture(sceGsTex0 *tex0)
{
    if (!first_upload_done)
    {
        return nullptr;
    }

    if (auto el = sdl_texture_atlas.find(CONVERT_TEX0_TO_ULONG(tex0));
        el != sdl_texture_atlas.end())
    {
        return sdl_texture_atlas[CONVERT_TEX0_TO_ULONG(tex0)];
    }

    return nullptr;
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

    for (auto t : texture_atlas)
    {
        free(t.second);
    }

    texture_atlas.clear();

    for (auto t : sdl_texture_atlas)
    {
        MikuPan_DeleteTexture(t.second);
    }

    sdl_texture_atlas.clear();

    request_texture_cache_flush = false;
}

void MikuPan_RequestFlushTextureCache()
{
    request_texture_cache_flush = true;
}