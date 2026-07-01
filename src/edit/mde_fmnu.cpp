#include "common.h"
#include "typedefs.h"
#include "enums.h"
#include "edit/mde_fmnu.h"

#include "edit/mde_file.h"
#include "ingame/map/map_ctrl.h"
#include "outgame/outgame.h"

int MdeFmnuExec(int act)
{
    int room_id;

    switch (act)
    {
    case MDE_MENU_ACT_LOAD:
        MdeFileLoadReq(mde_menu_wrk.mission_no);
        mde_menu_wrk.msg = "LOAD REQ";
        break;
    case MDE_MENU_ACT_MARK_PASS:
        room_id = MdeMenuCurrentRoomId();

        if (room_id != 0xff)
        {
            RoomPass((u_char)room_id);
            mde_menu_wrk.msg = "ROOM PASS";
        }
        else
        {
            mde_menu_wrk.msg = "NO ROOM";
        }
        break;
    case MDE_MENU_ACT_EXIT:
        OutGameModeChange(OUTGAME_MODE_TITLE);
        return 1;
    default:
        break;
    }

    return 0;
}
