#ifndef MIKUPAN_TEXTURE_MANAGER_C_H
#define MIKUPAN_TEXTURE_MANAGER_C_H

#include "ee/eestruct.h"

void AddTexture(sceGsTex0* tex0, unsigned char* img);
void AddSDLTexture(sceGsTex0* tex0, void* img);
unsigned char* GetTexture(sceGsTex0* tex0);
void* GetSDLTexture(sceGsTex0* tex0);
void FirstUploadDone();
int IsFirstUploadDone();
void MikuPan_FlushTextureCache();
void MikuPan_RequestFlushTextureCache();

#endif //MIKUPAN_TEXTURE_MANAGER_C_H