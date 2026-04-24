#ifndef MIKUPAN_TEXTURE_MANAGER_H
#define MIKUPAN_TEXTURE_MANAGER_H

#include "mikupan/mikupan_basictypes.h"
#include <unordered_map>

extern std::unordered_map<uint64_t, MikuPan_TextureInfo*> mikupan_render_texture_atlas;
extern bool first_upload_done;

extern "C"
{
    void MikuPan_AddTexture(uint64_t hash, MikuPan_TextureInfo* texture_info);
    MikuPan_TextureInfo* MikuPan_GetTex0TextureInfoAndCount(uint64_t tex0, int* count);
    MikuPan_TextureInfo* MikuPan_GetTextureInfo(uint64_t hash);
    int MikuPan_IsTextureCacheFlushRequested();
    void MikuPan_FirstUploadDone();
    bool MikuPan_IsFirstUploadDone();
    void MikuPan_FlushTextureCache();
    void MikuPan_RequestFlushTextureCache();
    void MikuPan_ShutdownTextureCache();
}


#endif //MIKUPAN_TEXTURE_MANAGER_H