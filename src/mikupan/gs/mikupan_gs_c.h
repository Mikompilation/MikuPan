#ifndef MIKUPAN_GS_SERVER_C_H
#define MIKUPAN_GS_SERVER_C_H

#include "typedefs.h"
#include "ee/eestruct.h"
#include "sce/libgraph.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void MikuPan_GsUpload(sceGsLoadImage* image_load, unsigned char* image);
unsigned char* MikuPan_GsDownloadTexture(sceGsTex0* tex0, uint64_t* hash);
uint64_t MikuPan_GetTextureHash(sceGsTex0 *tex0);

/// Per-frame GS instrumentation. Reset in MikuPan_Clear; sampled by the
/// FrameTimeGraph window for upload/download monitoring.
void  MikuPan_GsResetFrameMetrics(void);
int   MikuPan_GsGetUploadCount(void);
int   MikuPan_GsGetUploadBytes(void);
float MikuPan_GsGetUploadMs(void);
int   MikuPan_GsGetDownloadCount(void);
int   MikuPan_GsGetDownloadBytes(void);
float MikuPan_GsGetDownloadMs(void);
void MikuPan_GsStore(sceGsStoreImage *sp, unsigned char *out);
int  MikuPan_GsHasPendingUploads(void);
void MikuPan_GsConsumePendingUploads(
    void (*cb)(int addr, int size, void *ud), void *ud);
void MikuPan_GetTextureGsRegion(sceGsTex0 *tex0, int *out_addr, int *out_size);

#ifdef __cplusplus
}
#endif

#endif //MIKUPAN_GS_SERVER_C_H
