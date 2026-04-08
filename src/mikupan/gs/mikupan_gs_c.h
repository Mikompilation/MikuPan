#ifndef MIKUPAN_GS_SERVER_C_H
#define MIKUPAN_GS_SERVER_C_H

#include "typedefs.h"
#include "ee/eestruct.h"
#include "sce/libgraph.h"
#include <stdint.h>

void MikuPan_GsUpload(sceGsLoadImage* image_load, unsigned char* image);
unsigned char* MikuPan_GsDownloadTexture(sceGsTex0* tex0, uint64_t* hash);
uint64_t MikuPan_GetTextureHash(sceGsTex0 *tex0);

#endif //MIKUPAN_GS_SERVER_C_H