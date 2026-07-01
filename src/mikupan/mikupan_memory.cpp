#include "typedefs.h"
#include "mikupan_memory.h"
#include "mikupan/debug/mikupan_logging_c.h"
#include <stdint.h>
#include <string.h>

#define PS2_VIRTUAL_RAM_SIZE (1024 * 1024 * 32)
#define PS2_SCRATCHPAD_MEM_SIZE (1024 * 16)

/// Allocate double the amount of PS2 RAM to avoid overflow issues.
ATTRIBUTE_ALIGNED(64, unsigned char ps2_virtual_ram[PS2_VIRTUAL_RAM_SIZE]);

/// 16kb of scratchpad memory
ATTRIBUTE_ALIGNED(64, unsigned char ps2_virtual_scratchpad[PS2_SCRATCHPAD_MEM_SIZE]);

int64_t MikuPan_GetHostAddress(int offset)
{
    int64_t host_address;

    if (!MikuPan_TryGetHostAddressFromPs2Address((uint64_t)(uint32_t)offset,
                                                 &host_address))
    {
        info_log("0x%x PS2 address was requested but falls outside the range", offset);
        return -1;
    }

    return host_address;
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

int MikuPan_IsPs2AddressMainMemoryRange64(uint64_t address)
{
    if ((address & UINT64_C(0xffffffff00000000)) != 0)
    {
        return 0;
    }

    return MikuPan_IsPs2AddressMainMemoryRange((int)address);
}

int MikuPan_TryGetHostAddressFromPs2Address(uint64_t address, int64_t *host_address)
{
    if (host_address == NULL)
    {
        return 0;
    }

    if (address == 0)
    {
        *host_address = 0;
        return 1;
    }

    if (!MikuPan_IsPs2AddressMainMemoryRange64(address))
    {
        return 0;
    }

    int offset = MikuPan_SanitizePs2Address((int) address);
    *host_address = (int64_t)(uintptr_t)(ps2_virtual_ram + offset);

    return 1;
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
