#include "libsg.h"

u32 g_vu0_r = 0;

u32 rnext(void)
{
    u32 r = g_vu0_r;
    s32 x = (r>>4)&1;
    s32 y = (r>>22)&1;
    r <<= 1;
    r ^= (x^y);
    r =  0x3f800000|(r&0x7fffff); /*23bit*/
    g_vu0_r = r;
    return r;
}