#ifndef INGAME_PLYR_TIME_CTL_H
#define INGAME_PLYR_TIME_CTL_H

#include "typedefs.h"

#include "os/system.h"

#define SECONDS_249D_23H_59M_00S ((249*24*60*60)+(23*60*60)+(59*60)+0)

void TimeCtrlInit();
void GameTimeCtrl();
void SetNowClock(sceCdCLOCK *nc);
void PlyrTimerCtrl();
void EntryTimeCtrl();
void SetRealTime();

#endif // INGAME_PLYR_TIME_CTL_H
