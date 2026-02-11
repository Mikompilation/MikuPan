#include "common.h"
#include "furn_spe.h"
#include "typedefs.h"

#define BASE_ADDRESS 0x7f8000

void FSpeMapDataMapping()
{
    u_int *addr_top;
    u_int *addr_data;
    int i;
    u_short data_num;

    addr_top = (u_int *) MikuPan_GetHostPointer(BASE_ADDRESS + 4 * 4);
    addr_top = (u_int *) MikuPan_GetHostPointer(*addr_top + BASE_ADDRESS);

    addr_data = (u_int *) MikuPan_GetHostPointer(*addr_top + BASE_ADDRESS);

    data_num = ((int64_t) addr_data - (int64_t) addr_top) / 4;

    addr_data = addr_top;

    for (i = 0; i < data_num; i++)
    {
        *addr_data += BASE_ADDRESS;

        addr_data++;
    }
}

u_char *FSpeGetTopAddr(u_short fact_no)
{
    u_int *addr;

    if (fact_no == 0xffff)
    {
        return 0;
    }

    addr = (u_int *) MikuPan_GetHostPointer(BASE_ADDRESS + 4 * 4);
    addr = (u_int *) MikuPan_GetHostPointer(*addr + BASE_ADDRESS);

    return (u_char *) MikuPan_GetHostPointer(addr[fact_no]);
}
