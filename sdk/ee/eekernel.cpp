#include "eekernel.h"

void* _gp;

//int CreateThread(ThreadParam*)
//{
//}

int StartThread(int thread_id, void* arg)
{
    (void)thread_id;
    (void)arg;
    return 0;
}

int GetThreadId(void)
{
    return 0;
}

int ChangeThreadPriority(int thread_id, int priority)
{
    (void)thread_id;
    (void)priority;
    return 0;
}

int RotateThreadReadyQueue(int priority)
{
    (void)priority;
    return 0;
}

int CreateSema(SemaParam* param)
{
    static int next_sema_id = 1;

    (void)param;
    return next_sema_id++;
}

int SignalSema(int sema_id)
{
    (void)sema_id;
    return 0;
}

int WaitSema(int sema_id)
{
    (void)sema_id;
    return 0;
}

int DeleteSema(int sema_id)
{
    (void)sema_id;
    return 0;
}
