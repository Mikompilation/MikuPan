#include "common.h"
#include "typedefs.h"
#include "enums.h"
#include "edit/mde_menu.h"

#include "edit/map_data.h"
#include "edit/mde_file.h"
#include "main/glob.h"

#include <string.h>

#define MDE_MENU_ROW_NUM 11

MDE_MENU_WRK mde_menu_wrk;

static const char *mde_menu_page_name[MDE_MENU_PAGE_NUM] = {
    "SUMMARY",
    "SECTION",
    "ROOM",
};

static void MdeMenuWrapInt(int *value, int min, int max)
{
    if (*value < min)
    {
        *value = max;
    }
    else if (*value > max)
    {
        *value = min;
    }
}

static void MdeMenuClampInt(int *value, int min, int max)
{
    if (*value < min)
    {
        *value = min;
    }
    else if (*value > max)
    {
        *value = max;
    }
}

int MdeMenuCurrentRoomId(void)
{
    return MdeMapRoomIdFromRoomNo(MAP_ROOM_DAT, mde_menu_wrk.room_no, mde_menu_wrk.floor_no);
}

void MdeMenuClamp(void)
{
    int room_num;
    int room_id;
    int data_num;

    room_num = MdeMapRoomNum(mde_menu_wrk.floor_no);

    if (room_num <= 0)
    {
        room_num = MDE_MAP_ROOM_MAX;
    }

    MdeMenuClampInt(&mde_menu_wrk.room_no, 0, room_num - 1);

    room_id = MdeMenuCurrentRoomId();
    data_num = MdeMapDataNum(mde_menu_wrk.section_no, room_id);

    if (data_num <= 0)
    {
        data_num = 1;
    }

    MdeMenuClampInt(&mde_menu_wrk.data_no, 0, data_num - 1);
}

void MdeMenuInit(void)
{
    memset(&mde_menu_wrk, 0, sizeof(mde_menu_wrk));

    mde_menu_wrk.floor_no = 1;
    mde_menu_wrk.section_no = MAP_ROOM_DAT;
    mde_menu_wrk.msg = "READY";
}

static void MdeMenuApplyChange(int changed)
{
    switch (mde_menu_wrk.csr)
    {
    case 0:
        mde_menu_wrk.page += changed;
        MdeMenuWrapInt(&mde_menu_wrk.page, 0, MDE_MENU_PAGE_NUM - 1);
        break;
    case 1:
        mde_menu_wrk.mission_no += changed;
        MdeMenuWrapInt(&mde_menu_wrk.mission_no, 0, MDE_MAP_MISSION_NUM - 1);
        mde_menu_wrk.msg = "PRESS LOAD";
        break;
    case 2:
        mde_menu_wrk.floor_no += changed;
        MdeMenuWrapInt(&mde_menu_wrk.floor_no, 0, MDE_MAP_FLOOR_NUM - 1);

        if (MdeMapDataIsReady() != 0)
        {
            MdeFileSetFloor(mde_menu_wrk.mission_no, mde_menu_wrk.floor_no);
        }

        MdeMenuClamp();
        break;
    case 3:
        mde_menu_wrk.section_no += changed;
        MdeMenuWrapInt(&mde_menu_wrk.section_no, 0, MDE_MAP_SECTION_NUM - 1);
        MdeMenuClamp();
        break;
    case 4:
        mde_menu_wrk.room_no += changed;
        MdeMenuClamp();
        break;
    case 5:
        mde_menu_wrk.data_no += changed;
        MdeMenuClamp();
        break;
    case 6:
        mde_menu_wrk.pos_x += changed * 0x100;
        MdeMenuClampInt(&mde_menu_wrk.pos_x, 0, 0xffff);
        break;
    case 7:
        mde_menu_wrk.pos_z += changed * 0x100;
        MdeMenuClampInt(&mde_menu_wrk.pos_z, 0, 0xffff);
        break;
    default:
        break;
    }
}

int MdeMenuCtrl(void)
{
    int changed;

    if (pad[0].one & 0x40)
    {
        return MDE_MENU_ACT_EXIT;
    }

    if (pad[0].rpt & 0x4000)
    {
        mde_menu_wrk.csr++;
        MdeMenuWrapInt(&mde_menu_wrk.csr, 0, MDE_MENU_ROW_NUM - 1);
    }
    else if (pad[0].rpt & 0x1000)
    {
        mde_menu_wrk.csr--;
        MdeMenuWrapInt(&mde_menu_wrk.csr, 0, MDE_MENU_ROW_NUM - 1);
    }

    changed = 0;

    if (pad[0].rpt & 0x2000)
    {
        changed = 1;
    }
    else if (pad[0].rpt & 0x8000)
    {
        changed = -1;
    }

    if (changed != 0)
    {
        MdeMenuApplyChange(changed);
    }

    if (pad[0].one & 0x20)
    {
        switch (mde_menu_wrk.csr)
        {
        case 8:
            return MDE_MENU_ACT_LOAD;
        case 9:
            return MDE_MENU_ACT_MARK_PASS;
        case 10:
            return MDE_MENU_ACT_EXIT;
        default:
            break;
        }
    }

    if (START_PRESSED() == 1)
    {
        return MDE_MENU_ACT_LOAD;
    }

    return MDE_MENU_ACT_NONE;
}

const char *MdeMenuPageName(int page)
{
    if (page < 0 || page >= MDE_MENU_PAGE_NUM)
    {
        return "INVALID";
    }

    return mde_menu_page_name[page];
}
