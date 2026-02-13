#include "mikupan_memory.h"

#include <string.h>

/// Allocate double the amount of PS2 RAM to avoid overflow issues
unsigned char ps2_virtual_ram[1024 * 1024 * 32];

/// 16kb of scratchpad memory
unsigned char ps2_virtual_scratchpad[1024*16];

int64_t MikuPan_GetHostAddress(int offset)
{
    offset = MikuPan_SanitizePs2Address(offset);
    int64_t ptr = (int64_t) (ps2_virtual_ram + offset);

    if (!MikuPan_IsPs2MemoryPointer((int64_t) ptr))
    {
        return -1;
    }

    return ptr;
}

void *MikuPan_GetHostPointer(int offset)
{
    offset = MikuPan_SanitizePs2Address(offset);
    return (void*)(ps2_virtual_ram + offset);
}

void MikuPan_InitPs2Memory()
{
    memset(ps2_virtual_ram, 0, sizeof(ps2_virtual_ram));
}

int MikuPan_IsPs2MemoryPointer(int64_t address)
{
    return (address >= (int64_t) ps2_virtual_ram
            && address < (int64_t) (ps2_virtual_ram + sizeof(ps2_virtual_ram)));
}

/**
 *
 * @param address PS2 memory address
 * @return Clean memory address with removed configuration bits such as accelerated ram
 */
int MikuPan_SanitizePs2Address(int address)
{
    /// Remove accelerated ram bits
    address = (address & ~0x30000000);

    return address;
}

int MikuPan_IsPs2AddressMainMemoryRange(int address)
{
    address = MikuPan_SanitizePs2Address(address);
    return (address >= 0x00000000 && address < (1024 * 1024 * 32));
}

int MikuPan_GetPs2OffsetFromHostPointer(void *ptr)
{
    if (!MikuPan_IsPs2MemoryPointer((int64_t) ptr))
    {
        return -1;
    }

    /// Conversion to int is safe because it is within the PS2 RAM size
    return (int)((int64_t)ptr - (int64_t)ps2_virtual_ram);
}