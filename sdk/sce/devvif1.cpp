#include "devvif1.h"

void sceDevVif1Reset(void)
{
}

int sceDevVif1Pause(int mode)
{
    return 0;
}

int sceDevVif1Continue(void)
{
    return 0;
}

u_int sceDevVif1PutErr(int interrupt, int miss1, int miss2)
{
    return 0;
}

u_int sceDevVif1GetErr(void)
{
    return 0;
}

int sceDevVif1GetCnd(sceDevVif1Cnd* cnd)
{
    return 0;
}

int sceDevVif1PutFifo(u_long128* addr, int n)
{
    return 0;
}

int sceDevVif1GetFifo(u_long128* addr, int n)
{
    return 0;
}
