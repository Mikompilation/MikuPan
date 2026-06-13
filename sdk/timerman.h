#ifndef MIKUPAN_IOP_TIMERMAN_H
#define MIKUPAN_IOP_TIMERMAN_H

#include "typedefs.h"

struct SysClock {
    u_int low;
    u_int hi;
};

typedef u_int (*TimerHandler)(void* userdata);

void USec2SysClock(u_int usec, struct SysClock* clock);
int AllocHardTimer(int source, int size, int mode);
int SetTimerHandler(int timer_id, u_int compare, TimerHandler handler, void* userdata);
int SetupHardTimer(int timer_id, int source, int mode, int prescale);
int StartHardTimer(int timer_id);
int StopHardTimer(int timer_id);

#endif
