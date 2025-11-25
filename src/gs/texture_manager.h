#ifndef MIKUPAN_TEXTURE_MANAGER_H
#define MIKUPAN_TEXTURE_MANAGER_H
#include "ee/eestruct.h"

#include <unordered_map>

extern std::unordered_map<unsigned long long, unsigned char*> texture_atlas;
extern std::unordered_map<unsigned long long, void*> sdl_texture_atlas;
extern bool first_upload_done;

extern "C"
{
    void AddTexture(sceGsTex0* tex0, unsigned char* img);
    void AddSDLTexture(sceGsTex0* tex0, void* img);
    unsigned char* GetTexture(sceGsTex0* tex0);
    void* GetSDLTexture(sceGsTex0* tex0);
    void FirstUploadDone();
    bool IsFirstUploadDone();
    void MikuPan_FlushTextureCache();
    void MikuPan_RequestFlushTextureCache();
}


#endif //MIKUPAN_TEXTURE_MANAGER_H