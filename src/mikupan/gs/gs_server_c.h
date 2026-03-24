#ifndef MIKUPAN_GS_SERVER_C_H
#define MIKUPAN_GS_SERVER_C_H

#include "ee/eestruct.h"
#include "sce/libgraph.h"

void GsUpload(sceGsLoadImage* image_load, unsigned char* image);
unsigned char* DownloadGsTexture(sceGsTex0* tex0);

#endif //MIKUPAN_GS_SERVER_C_H