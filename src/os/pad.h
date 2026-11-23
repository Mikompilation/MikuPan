#include "mikupan/mikupan_region.h"
#ifndef OS_PAD_H
#define OS_PAD_H

#include "typedefs.h"
#include "pad_types.h"


MIKUPAN_NAMESPACE_BEGIN

extern u_short sce_pad[16];

int InitPad();
int PadSyncCallback();
int PadReadFunc(PAD_STRUCT *psp, int p_id);
void SetAnlgInfo(PAD_STRUCT *psp, int p_id);
u_short VibrateRequest(u_short p_id, u_short act1, u_short act2); /// Initiall badly imported
u_short VibrateRequest1(u_short p_id, u_short act_1); /// Initiall badly imported
u_short VibrateRequest2(u_short p_id, u_short act_2);
void PadClearCount(int p_id);


MIKUPAN_NAMESPACE_END
#endif // OS_PAD_H
