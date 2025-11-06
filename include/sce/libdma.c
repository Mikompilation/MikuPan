#include "libdma.h"

//#include <stdlib.h>

int sceDmaReset(int mode)
{
}

sceDmaEnv* sceDmaGetEnv(sceDmaEnv* env)
{
}

int sceDmaPutEnv(sceDmaEnv* env)
{
}

sceDmaChan* sceDmaGetChan(int id)
{
   return malloc(sizeof(sceDmaChan));
}

void sceDmaSend(sceDmaChan* d, void* tag)
{
}

int sceDmaSync(sceDmaChan* d, int mode, int timeout)
{
}
