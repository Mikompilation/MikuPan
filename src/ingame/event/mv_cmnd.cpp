#include "common.h"
#include "typedefs.h"
#include "mv_cmnd.h"

#include <cstdio>

#include "graphics/graph2d/message.h"
#include "ingame/menu/ig_menu.h"
#include "main/glob.h"
#include "mikupan/mikupan_textoverride.h"
#include "mikupan/mikupan_memory.h"

MOVIE_CMD_WRK mvcmd_wrk = {0};

void EventMovieCommandInit()
{
    mvcmd_wrk = {};

    mvcmd_wrk.msg_no = -1;
}

void EventMovieCommandMain(int flame, int top_addr)
{
    int *dat_addr;
    u_char *cmd_addr;

    dat_addr = (int *)top_addr;

    while (dat_addr[0] <= flame && dat_addr[0] != -1)
    {
        if (flame == dat_addr[0])
        {
            cmd_addr = (u_char *)(top_addr + dat_addr[1]);

            switch(*cmd_addr)
            {
                case 0xf0:
                case 0xf1:
                case 0xf2:
                case 0xf3:
                case 0xf5:
                case 0xf6:
                case 0xf7:
                case 0xf8:
                case 0xf9:
                    // do nothing ...
                    break;
                case 0xf4:
                    mvcmd_wrk.msg_no = *(short int *)(cmd_addr + 2);
                    mvcmd_wrk.msg_wait = *(short int *)(cmd_addr + 4);
                    break;
            }
        }

        dat_addr += 2;
    }

    if (mvcmd_wrk.msg_no != -1)
    {
        if (mvcmd_wrk.msg_wait != 0)
        {
            SetMessageAddr(GetMovieMessageAddr(mvcmd_wrk.msg_no));

            mvcmd_wrk.msg_wait--;

            SetMessage();
        }
        else
        {
            mvcmd_wrk.msg_no = 0xffff;
        }
    }
}

int64_t GetMovieMessageAddr(short int msg_no)
{
    int addr;
    char mod_category[24];

    std::snprintf(mod_category, sizeof(mod_category), "m%d_event#movie",
                 ingame_wrk.msn_no);
    const int64_t mod_addr = MikuPan_GetTextModAddr(mod_category, msg_no);
    if (mod_addr != 0)
    {
        return mod_addr;
    }

    addr = Get4Byte((u_char *)MikuPan_GetHostPointer(BASE_ADDRESS + 3 * 4));
    addr = Get4Byte((u_char *)MikuPan_GetHostPointer(BASE_ADDRESS + addr + msg_no * 4));

    return MikuPan_GetHostAddress(BASE_ADDRESS + addr);
}