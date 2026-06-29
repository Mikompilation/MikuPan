#include "libdma.h"

#include <stdlib.h>

sceDmaChan* dma_chan = NULL;

int sceDmaReset(int mode)
{
    return 1;
}

sceDmaEnv* sceDmaGetEnv(sceDmaEnv* env)
{
    return env;
}

int sceDmaPutEnv(sceDmaEnv* env)
{
    return 1;
}

sceDmaChan* sceDmaGetChan(int id)
{
    if (dma_chan == NULL)
    {
        dma_chan = (sceDmaChan*)malloc(sizeof(sceDmaChan));
    }

   return dma_chan;
}

void sceDmaSend(sceDmaChan* d, void* tag)
{
    //ReadPacket((Q_WORDDATA*)tag);
}

int sceDmaSync(sceDmaChan* d, int mode, int timeout)
{
    return 1;
}
