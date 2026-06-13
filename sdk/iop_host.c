#include "intrman.h"
#include "introld.h"
#include "sysmem.h"
#define MIKUPAN_IOP_THREAD_NO_COMPAT_MACROS
#include "thread.h"
#include "timerman.h"

#include <SDL3/SDL_mutex.h>
#include <SDL3/SDL_thread.h>
#include <SDL3/SDL_timer.h>
#include <stdlib.h>
#include <string.h>

#define MIKUPAN_IOP_MAX_THREADS 64
#define MIKUPAN_IOP_MAX_TIMERS 8

typedef int (*IopThreadEntry)(void);

typedef struct {
    int id;
    void* entry;
    SDL_Thread* thread;
    SDL_Semaphore* wake;
    int started;
} MikuPanIopThread;

typedef struct {
    int id;
    u_int compare;
    TimerHandler handler;
    void* userdata;
    SDL_Thread* thread;
    int running;
} MikuPanIopTimer;

static MikuPanIopThread iop_threads[MIKUPAN_IOP_MAX_THREADS];
static MikuPanIopTimer iop_timers[MIKUPAN_IOP_MAX_TIMERS];
static SDL_TLSID iop_thread_tls;
static volatile int iop_host_shutdown;

static MikuPanIopThread* FindThread(int thread_id)
{
    for (int i = 0; i < MIKUPAN_IOP_MAX_THREADS; i++) {
        if (iop_threads[i].id == thread_id) {
            return &iop_threads[i];
        }
    }

    return NULL;
}

static MikuPanIopTimer* FindTimer(int timer_id)
{
    for (int i = 0; i < MIKUPAN_IOP_MAX_TIMERS; i++) {
        if (iop_timers[i].id == timer_id) {
            return &iop_timers[i];
        }
    }

    return NULL;
}

static int SDLCALL IopThreadMain(void* data)
{
    MikuPanIopThread* thread = data;
    IopThreadEntry entry = (IopThreadEntry)thread->entry;

    SDL_SetTLS(&iop_thread_tls, thread, NULL);

    if (entry != NULL) {
        return entry();
    }

    return 0;
}

static int SDLCALL IopTimerMain(void* data)
{
    MikuPanIopTimer* timer = data;
    Uint64 interval = (Uint64)(timer->compare ? timer->compare : 1) * 1000ULL;
    Uint64 next = SDL_GetTicksNS() + interval;

    while (timer->running && !iop_host_shutdown) {
        Uint64 now = SDL_GetTicksNS();
        if (now < next) {
            SDL_DelayNS(next - now);
        }

        if (!timer->running || iop_host_shutdown) {
            break;
        }

        if (timer->handler != NULL) {
            u_int next_compare = timer->handler(timer->userdata);
            if (next_compare != 0) {
                interval = (Uint64)next_compare * 1000ULL;
            }
        }

        next += interval;
    }

    return 0;
}

int MikuPan_IopCreateThread(struct ThreadParam* param)
{
    if (param == NULL || param->entry == NULL) {
        return -1;
    }

    for (int i = 0; i < MIKUPAN_IOP_MAX_THREADS; i++) {
        if (iop_threads[i].id == 0) {
            iop_threads[i].id = i + 1;
            iop_threads[i].entry = param->entry;
            iop_threads[i].thread = NULL;
            iop_threads[i].wake = SDL_CreateSemaphore(0);
            iop_threads[i].started = 0;
            return iop_threads[i].id;
        }
    }

    return -1;
}

int MikuPan_IopStartThread(int thread_id, void* arg)
{
    (void)arg;

    MikuPanIopThread* thread = FindThread(thread_id);
    if (thread == NULL || thread->started) {
        return -1;
    }

    thread->thread = SDL_CreateThread(IopThreadMain, "iop-thread", thread);
    if (thread->thread == NULL) {
        return -1;
    }

    thread->started = 1;
    return 0;
}

int MikuPan_IopSleepThread(void)
{
    MikuPanIopThread* thread = SDL_GetTLS(&iop_thread_tls);
    if (iop_host_shutdown) {
        return -1;
    }

    if (thread == NULL || thread->wake == NULL) {
        SDL_Delay(1);
        return iop_host_shutdown ? -1 : 0;
    }

    if (!iop_host_shutdown) {
        SDL_WaitSemaphore(thread->wake);
    }

    return iop_host_shutdown ? -1 : 0;
}

int MikuPan_IopWakeupThread(int thread_id)
{
    MikuPanIopThread* thread = FindThread(thread_id);
    if (thread == NULL || thread->wake == NULL) {
        return -1;
    }

    SDL_SignalSemaphore(thread->wake);
    return 0;
}

int MikuPan_IopWakeupThreadFromInterrupt(int thread_id)
{
    return MikuPan_IopWakeupThread(thread_id);
}

int MikuPan_IopGetThreadId(void)
{
    MikuPanIopThread* thread = SDL_GetTLS(&iop_thread_tls);
    return thread != NULL ? thread->id : 0;
}

void USec2SysClock(u_int usec, struct SysClock* clock)
{
    if (clock != NULL) {
        clock->low = usec;
        clock->hi = 0;
    }
}

int AllocHardTimer(int source, int size, int mode)
{
    (void)source;
    (void)size;
    (void)mode;

    for (int i = 0; i < MIKUPAN_IOP_MAX_TIMERS; i++) {
        if (iop_timers[i].id == 0) {
            iop_timers[i].id = i + 1;
            return iop_timers[i].id;
        }
    }

    return -1;
}

int SetTimerHandler(int timer_id, u_int compare, TimerHandler handler, void* userdata)
{
    MikuPanIopTimer* timer = FindTimer(timer_id);
    if (timer == NULL) {
        return -1;
    }

    timer->compare = compare;
    timer->handler = handler;
    timer->userdata = userdata;
    return 0;
}

int SetupHardTimer(int timer_id, int source, int mode, int prescale)
{
    (void)timer_id;
    (void)source;
    (void)mode;
    (void)prescale;
    return 0;
}

int StartHardTimer(int timer_id)
{
    MikuPanIopTimer* timer = FindTimer(timer_id);
    if (timer == NULL || timer->running) {
        return -1;
    }

    timer->running = 1;
    timer->thread = SDL_CreateThread(IopTimerMain, "iop-timer", timer);
    return timer->thread == NULL ? -1 : 0;
}

int StopHardTimer(int timer_id)
{
    MikuPanIopTimer* timer = FindTimer(timer_id);
    if (timer == NULL) {
        return -1;
    }

    timer->running = 0;
    return 0;
}

int CpuEnableIntr(void)
{
    return 0;
}

int CpuSuspendIntr(int* oldstat)
{
    if (oldstat != NULL) {
        *oldstat = 0;
    }
    return 0;
}

int CpuResumeIntr(int oldstat)
{
    (void)oldstat;
    return 0;
}

int EnableIntr(int intr)
{
    (void)intr;
    return 0;
}

void FlushDcache(void)
{
}

void* AllocSysMemory(int type, unsigned int size, void* addr)
{
    (void)type;
    (void)addr;
    return calloc(1, size);
}

int FreeSysMemory(void* ptr)
{
    free(ptr);
    return 0;
}

int QueryMemSize(void)
{
    return 32 * 1024 * 1024;
}

int QueryTotalFreeMemSize(void)
{
    return 16 * 1024 * 1024;
}

int QueryMaxFreeMemSize(void)
{
    return 16 * 1024 * 1024;
}

int MikuPan_IopHostShouldShutdown(void)
{
    return iop_host_shutdown;
}

void MikuPan_IopHostShutdown(void)
{
    if (iop_host_shutdown) {
        return;
    }

    iop_host_shutdown = 1;

    for (int i = 0; i < MIKUPAN_IOP_MAX_TIMERS; i++) {
        iop_timers[i].running = 0;
    }

    for (int i = 0; i < MIKUPAN_IOP_MAX_THREADS; i++) {
        if (iop_threads[i].wake != NULL) {
            SDL_SignalSemaphore(iop_threads[i].wake);
        }
    }

    for (int i = 0; i < MIKUPAN_IOP_MAX_TIMERS; i++) {
        if (iop_timers[i].thread != NULL) {
            SDL_WaitThread(iop_timers[i].thread, NULL);
            iop_timers[i].thread = NULL;
        }
        iop_timers[i].id = 0;
        iop_timers[i].handler = NULL;
        iop_timers[i].userdata = NULL;
        iop_timers[i].compare = 0;
    }

    for (int i = 0; i < MIKUPAN_IOP_MAX_THREADS; i++) {
        if (iop_threads[i].thread != NULL) {
            SDL_WaitThread(iop_threads[i].thread, NULL);
            iop_threads[i].thread = NULL;
        }
        if (iop_threads[i].wake != NULL) {
            SDL_DestroySemaphore(iop_threads[i].wake);
            iop_threads[i].wake = NULL;
        }
        iop_threads[i].id = 0;
        iop_threads[i].entry = NULL;
        iop_threads[i].started = 0;
    }
}
