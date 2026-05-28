#ifndef MIKUPAN_TEXTURE_MANAGER_C_H
#define MIKUPAN_TEXTURE_MANAGER_C_H

#include "mikupan/mikupan_basictypes.h"
#include "ee/eestruct.h"
#include "sce/libgraph.h"

void MikuPan_AddTexture(uint64_t hash, MikuPan_TextureInfo* texture_info);
MikuPan_TextureInfo* MikuPan_GetTex0TextureInfoAndCount(uint64_t tex0, int* count);
MikuPan_TextureInfo* MikuPan_GetTextureInfo(uint64_t hash);
int MikuPan_IsTextureCacheFlushRequested();
void MikuPan_FirstUploadDone();
int MikuPan_IsFirstUploadDone();
void MikuPan_FlushTextureCache();
void MikuPan_RequestFlushTextureCache();
void MikuPan_ForEachTexture(void (*callback)(MikuPan_TextureInfo* tex, void* userdata), void* userdata);
MikuPan_TextureInfo* MikuPan_LookupTextureByTex0(uint64_t tex0_value);
/// Register a tex0_value → info mapping in the L1. Takes the original sceGsTex0
/// pointer so it can record the GS-memory range the texture samples; that range
/// is what selective invalidation overlap-tests against pending uploads.
void MikuPan_RegisterTextureForTex0(uint64_t tex0_value, sceGsTex0 *tex0,
                                    MikuPan_TextureInfo* info);
void MikuPan_InvalidateTex0Cache(void);

#endif //MIKUPAN_TEXTURE_MANAGER_C_H