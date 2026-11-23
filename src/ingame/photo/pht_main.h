#include "mikupan/mikupan_region.h"
#ifndef INGAME_PHOTO_PHT_MAIN_H
#define INGAME_PHOTO_PHT_MAIN_H

#include "typedefs.h"

// extern int after_timer;

MIKUPAN_NAMESPACE_BEGIN

extern int pazene_load_buf[2];

void PhotoCtrlInit();
void PhotoCtrl();
void InitPazEneTexLoad();


MIKUPAN_NAMESPACE_END
#endif// INGAME_PHOTO_PHT_MAIN_H
