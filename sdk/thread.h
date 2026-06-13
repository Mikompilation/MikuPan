#ifndef MIKUPAN_IOP_THREAD_H
#define MIKUPAN_IOP_THREAD_H

struct ThreadParam {
    int attr;
    void* entry;
    int initPriority;
    int stackSize;
    int option;
};

int MikuPan_IopCreateThread(struct ThreadParam* param);
int MikuPan_IopStartThread(int thread_id, void* arg);
int MikuPan_IopSleepThread(void);
int MikuPan_IopWakeupThread(int thread_id);
int MikuPan_IopWakeupThreadFromInterrupt(int thread_id);
int MikuPan_IopGetThreadId(void);
int MikuPan_IopHostShouldShutdown(void);
void MikuPan_IopHostShutdown(void);

#ifndef MIKUPAN_IOP_THREAD_NO_COMPAT_MACROS
#define CreateThread MikuPan_IopCreateThread
#define StartThread MikuPan_IopStartThread
#define SleepThread MikuPan_IopSleepThread
#define WakeupThread MikuPan_IopWakeupThread
#define iWakeupThread MikuPan_IopWakeupThreadFromInterrupt
#define GetThreadId MikuPan_IopGetThreadId
#endif

#endif
