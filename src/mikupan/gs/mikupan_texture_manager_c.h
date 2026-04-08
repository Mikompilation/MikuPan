#ifndef MIKUPAN_TEXTURE_MANAGER_C_H
#define MIKUPAN_TEXTURE_MANAGER_C_H

#include "mikupan/mikupan_basictypes.h"

void MikuPan_AddTexture(uint64_t hash, MikuPan_TextureInfo* texture_info);
MikuPan_TextureInfo* MikuPan_GetTex0TextureInfoAndCount(uint64_t tex0, int* count);
MikuPan_TextureInfo* MikuPan_GetTextureInfo(uint64_t hash);
int MikuPan_IsTextureCacheFlushRequested();
void MikuPan_FirstUploadDone();
int MikuPan_IsFirstUploadDone();
void MikuPan_FlushTextureCache();
void MikuPan_RequestFlushTextureCache();

#endif //MIKUPAN_TEXTURE_MANAGER_C_H