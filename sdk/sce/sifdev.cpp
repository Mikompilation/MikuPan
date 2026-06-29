#include "sifdev.h"

#include <stdlib.h>

int sceOpen(const char* filename, int flag, ...)
{
    return 1;
}

int sceClose(int fd)
{
    return 1;
}

int sceRead(int fd, void* buf, int nbyte)
{
    return 1;
}

int sceWrite(int fd, const void* buf, int nbyte)
{
    return 1;
}

int sceLseek(int fd, int offset, int where)
{
    return 1;
}

int sceFsReset()
{
    return 1;
}

int sceSifInitIopHeap()
{
    return 1;
}

int sceSifFreeIopHeap(void* p)
{
    free(p);
    return 1;
}

void* sceSifAllocIopHeap(unsigned int size)
{
    return malloc(size);
}

int sceSifRebootIop(const char* img)
{
    return 1;
}

int sceSifSyncIop()
{
    return 1;
}

int sceSifLoadModule(const char* filename, int args, const char* argp)
{
    return 1;
}
