#include "mikupan/mikupan_region.h"
#ifndef INGAME_INGAME_H
#define INGAME_INGAME_H

#include "typedefs.h"


MIKUPAN_NAMESPACE_BEGIN

extern int start_msn;
extern int msn0_wait_timer;

void InGameCtrl();
int InGameFirstLoad();
void InGameInit();
void InGameInit2();
void InGameMain();
void TmpPadOff();


MIKUPAN_NAMESPACE_END
#endif // INGAME_INGAME_H
