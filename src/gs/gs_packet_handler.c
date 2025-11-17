#include "gs_packet_handler.h"

#include "gs_server_c.h"
#include "common/logging_c.h"
#include "common/memory_addresses.h"
#include "ee/eestruct.h"
#include "sce/libgraph.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define u8DMAend 0x70
#define u8DMAnext 0x20



void ImageLoadPacket(Q_WORDDATA *packet)
{
    sceGsLoadImage *load = (sceGsLoadImage*)&packet[-1];
    GsUpload(load, (unsigned char*)*(int64_t*)&(((int*)&load[1])[1]));
    info_log("SCE_GS_BITBLTBUF Detected");
}

void ReadPacket(Q_WORDDATA *packet)
{
    //printf("packet of type type %#x\n", packet->ui32[2]);

    if (packet == NULL)
    {
        return;
    }

    if (packet->uc8[3] == u8DMAend)
    {
        return;
    }

    if (packet->uc8[3] == u8DMAnext)
    {
        info_log("Found packet chain!");

        //Q_WORDDATA *packet_chain = (Q_WORDDATA*)*(int64_t*)&(((int*)&packet->ui32[1])[0]);
        //if (packet->ui32[2] == VU0_ADDRESS || packet->ui32[2] == VU0_ADDRESS || packet_chain == NULL)
        //{
        //    return;
        //}

        /// Until I understand better, not worth the risk
        //ReadAllPackets(packet_chain);
    }

    switch (packet->ui32[2])
    {
        case SCE_GS_TEX0_1: info_log("SCE_GS_TEX0_1 Detected"); break;
        case SCE_GS_BITBLTBUF: ImageLoadPacket(packet); break;
        //case SCE_GS_PRIM_SPRITE: info_log("SPRITE DETECTED!"); break;
        default: /* info_log("Unknown packet {:x}" , packet->ui32[2]); */ break;
    }
}

void ReadAllPackets(Q_WORDDATA *packets)
{
    while (packets != NULL && packets->uc8[3] != u8DMAend && packets->ul128 != 0)
    {
        ReadPacket(packets);
        packets++;
    }
}