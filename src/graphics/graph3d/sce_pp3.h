#include "mikupan/mikupan_region.h"
#ifndef GRAPHICS_GRAPH3D_SCE_PP3_H
#define GRAPHICS_GRAPH3D_SCE_PP3_H

#include "typedefs.h"



MIKUPAN_NAMESPACE_BEGIN

void scePP13_Sync();
void scePP1_Kick(void *addr);
void scePP3_Kick(u_int *addr, int qwc);
int scePP1_Disp(u_int mode);
int scePP1_Init(u_int mode);
int scePP1_End(u_int mode);


MIKUPAN_NAMESPACE_END
#endif // GRAPHICS_GRAPH3D_SCE_PP3_H
