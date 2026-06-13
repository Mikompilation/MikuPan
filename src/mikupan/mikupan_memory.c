#include "mikupan_memory.h"

#include "mikupan_logging_c.h"

#include <stdint.h>
#include <string.h>

#define PS2_VIRTUAL_RAM_SIZE (1024 * 1024 * 32)

/// Allocate double the amount of PS2 RAM to avoid overflow issues
unsigned char ps2_virtual_ram[PS2_VIRTUAL_RAM_SIZE];

/// 16kb of scratchpad memory
unsigned char ps2_virtual_scratchpad[1024*16];

int64_t MikuPan_GetHostAddress(int offset)
{
    if (offset == 0)
    {
        return 0;
    }

    offset = MikuPan_SanitizePs2Address(offset);
    if (!MikuPan_IsPs2AddressMainMemoryRange(offset))
    {
        info_log("0x%x pointer was requested but falls outside the range", offset);
        return -1;
    }

    return (int64_t)(uintptr_t)(ps2_virtual_ram + offset);
}

void *MikuPan_GetHostPointer(int offset)
{
    return (void*)MikuPan_GetHostAddress(offset);
}

void MikuPan_InitPs2Memory()
{
    memset(ps2_virtual_ram, 0, sizeof(ps2_virtual_ram));
}

int MikuPan_IsPs2MemoryPointer(int64_t address)
{
    uintptr_t ptr = (uintptr_t)address;
    uintptr_t ps2_ram_start = (uintptr_t)ps2_virtual_ram;
    uintptr_t ps2_ram_end = ps2_ram_start + sizeof(ps2_virtual_ram);

    return (ptr >= ps2_ram_start && ptr < ps2_ram_end);
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
    return (address >= 0x00000000 && address < PS2_VIRTUAL_RAM_SIZE);
}

int MikuPan_GetPs2OffsetFromHostPointer(void *ptr)
{
    if (!MikuPan_IsPs2MemoryPointer((int64_t) ptr))
    {
        return -1;
    }

    /// Conversion to int is safe because it is within the PS2 RAM size
    return (int)((uintptr_t)ptr - (uintptr_t)ps2_virtual_ram);
}
