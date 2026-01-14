#ifndef MIKUPAN_TEXTURE_MANAGER_H
#define MIKUPAN_TEXTURE_MANAGER_H
#include "ee/eestruct.h"
#include "mikupan/mikupan_basictypes.h"

#include <unordered_map>

extern std::unordered_map<unsigned long long, unsigned char*> texture_atlas;
extern std::unordered_map<unsigned long long, MikuPan_TextureInfo*> mikupan_render_texture_atlas;
extern bool first_upload_done;

extern "C"
{
    void MikuPan_AddTexture(sceGsTex0 *tex0, MikuPan_TextureInfo* texture_info);
    MikuPan_TextureInfo* MikuPan_GetTextureInfo(sceGsTex0* tex0);

    u_char* MikuPan_GetTexturePixelBuffer(sceGsTex0* tex0);
    void MikuPan_AddTexturePixelBuffer(sceGsTex0* tex0, unsigned char *img);

    void FirstUploadDone();
    bool IsFirstUploadDone();
    void MikuPan_FlushTextureCache();
    void MikuPan_RequestFlushTextureCache();
}


#endif //MIKUPAN_TEXTURE_MANAGER_H