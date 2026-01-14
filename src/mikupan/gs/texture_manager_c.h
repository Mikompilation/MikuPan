#ifndef MIKUPAN_TEXTURE_MANAGER_C_H
#define MIKUPAN_TEXTURE_MANAGER_C_H

#include "ee/eestruct.h"
#include "mikupan/mikupan_basictypes.h"

void MikuPan_AddTexture(sceGsTex0 *tex0, MikuPan_TextureInfo* texture_info);
MikuPan_TextureInfo* MikuPan_GetTextureInfo(sceGsTex0* tex0);
u_char* MikuPan_GetTexturePixelBuffer(sceGsTex0* tex0);
void MikuPan_AddTexturePixelBuffer(sceGsTex0* tex0, unsigned char *img);
void FirstUploadDone();
int IsFirstUploadDone();
void MikuPan_FlushTextureCache();
void MikuPan_RequestFlushTextureCache();

#endif //MIKUPAN_TEXTURE_MANAGER_C_H