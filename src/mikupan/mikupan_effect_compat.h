#ifndef MIKUPAN_EFFECT_COMPAT_H
#define MIKUPAN_EFFECT_COMPAT_H

#include "common.h"
#include "typedefs.h"
#include "sce/libgraph.h"

#ifdef __cplusplus
extern "C" {
#endif

int MikuPan_TryScreenNegativeOverlay(u_char r, u_char g, u_char b, u_char alp, u_char alp2);
void MikuPan_QueueGusTriangles(u_long tex0, const float *vertices, int vertex_count);
void MikuPan_DrawQueuedGusObjects(void);
void MikuPan_ApplyScreenDitherPresentationScale(const sceGsTex0 *tex, float *vertices, int vertex_count);

#ifdef __cplusplus
}
#endif

#endif
