#include "libdma.h"

#include "gs/gs_packet_handler.h"

#include <stdlib.h>

sceDmaChan* dma_chan = NULL;

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
}
