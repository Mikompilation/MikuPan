#include "sif.h"

#include "mikupan/mikupan_file_c.h"
#include "mikupan/mikupan_memory.h"

#include <string.h>

int sceSifDmaStat(unsigned int id)
{
    (void)id;
    return -1;
}

/* DMA descriptors carry a mix of raw host pointers and PS2 bus addresses
   (the EE-side game code still passes addresses like FLOAT_GHOST_SE_LOAD_ADDR
   straight from the original program). Translate PS2 main-memory addresses
   into the emulated-RAM mirror; anything else is assumed to already be a
   host pointer. The range test uses the full 64-bit value so a host pointer
   can never be mistaken for a PS2 address by truncation. */
static void* ResolveDmaPointer(int64_t address, int* is_ps2_memory)
{
    *is_ps2_memory = 0;

    if (address == 0 || address == -1)
    {
        return NULL;
    }

    if (MikuPan_IsPs2MemoryPointer(address))
    {
        *is_ps2_memory = 1;
        return (void*)address;
    }

    if (address > 0 && address < 0x80000000LL
        && MikuPan_IsPs2AddressMainMemoryRange((int)address))
    {
        *is_ps2_memory = 1;
        return MikuPan_GetHostPointer((int)address);
    }

    return (void*)address;
}

unsigned int sceSifSetDma(sceSifDmaData* sdd, int len)
{
    if (sdd == NULL || len <= 0)
    {
        return 0;
    }

    for (int i = 0; i < len; i++)
    {
        int dst_is_ps2_memory;
        int src_is_ps2_memory;
        void* dst = ResolveDmaPointer(sdd[i].addr, &dst_is_ps2_memory);
        const void* src = ResolveDmaPointer(sdd[i].data, &src_is_ps2_memory);

        if (dst == NULL || src == NULL || sdd[i].size == 0)
        {
            continue;
        }

        memcpy(dst, src, sdd[i].size);

        if (dst_is_ps2_memory)
        {
            MikuPan_NotifyPs2MemoryLoad(MikuPan_GetPs2OffsetFromHostPointer(dst));
        }
    }

    return 1;
}
