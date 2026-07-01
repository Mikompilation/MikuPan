#include "common.h"
#include "typedefs.h"
#include "enums.h"
#include "edit/mde_disp.h"

#include "edit/map_data.h"
#include "edit/mde_mdsp.h"
#include "graphics/graph2d/effect_sub.h"
#include "graphics/graph2d/message.h"

#include <stdio.h>

static void MdeDispSetSquare(int pri, float x, float y, float w, float h, u_char r, u_char g, u_char b, u_char a)
{
    SetSquare(pri, x - 320.0f, y - 224.0f, (x - 320.0f) + w, y - 224.0f,
              x - 320.0f, (y - 224.0f) + h, (x - 320.0f) + w, (y - 224.0f) + h,
              r, g, b, a);
}

static void MdeDispLine(int line, char *str)
{
    SetASCIIString2(1, 40.0f, line * 14 + 48, 1, 0x80, 0x80, 0x80, str);
}

static void MdeDispMenuRows(const MDE_MENU_WRK *wrk)
{
    char tmp_str[256];
    int room_id;
    int data_room;
    int data_num;

    room_id = MdeMenuCurrentRoomId();
    data_room = MdeMapDataRoom(wrk->section_no, room_id);
    data_num = MdeMapDataNum(wrk->section_no, room_id);

    sprintf(tmp_str, "PAGE    %s", MdeMenuPageName(wrk->page));
    MdeDispLine(0, tmp_str);

    sprintf(tmp_str, "MISSION %d", wrk->mission_no);
    MdeDispLine(1, tmp_str);

    sprintf(tmp_str, "FLOOR   %d", wrk->floor_no);
    MdeDispLine(2, tmp_str);

    sprintf(tmp_str, "SECTION %02d %s", wrk->section_no, MdeMapSectionName(wrk->section_no));
    MdeDispLine(3, tmp_str);

    sprintf(tmp_str, "ROOM    %02d ID %02X", wrk->room_no, room_id);
    MdeDispLine(4, tmp_str);

    sprintf(tmp_str, "DATA    %02d / %02d  DROOM %02X", wrk->data_no, data_num, data_room);
    MdeDispLine(5, tmp_str);

    sprintf(tmp_str, "POS X   %04X", wrk->pos_x);
    MdeDispLine(6, tmp_str);

    sprintf(tmp_str, "POS Z   %04X", wrk->pos_z);
    MdeDispLine(7, tmp_str);

    sprintf(tmp_str, "LOAD MAP DATA");
    MdeDispLine(8, tmp_str);

    sprintf(tmp_str, "MARK ROOM PASS");
    MdeDispLine(9, tmp_str);

    sprintf(tmp_str, "EXIT");
    MdeDispLine(10, tmp_str);
}

void MdeDispMain(const MDE_MENU_WRK *wrk)
{
    char tmp_str[256];

    MdeDispSetSquare(2, 28.0f, 28.0f, 580.0f, 392.0f, 0x20, 0x30, 0x38, 0x70);
    SetASCIIString2(1, 40.0f, 28.0f, 0, 0x40, 0x90, 0x90, "MAP DATA EDIT");
    MdeDispMenuRows(wrk);

    switch (wrk->page)
    {
    case MDE_MENU_PAGE_SECTION:
        MdeMdspSectionTable(wrk);
        break;
    case MDE_MENU_PAGE_ROOM:
        MdeMdspRoomPage(wrk);
        break;
    case MDE_MENU_PAGE_SUMMARY:
    default:
        MdeMdspSummary(wrk);
        break;
    }

    sprintf(tmp_str, "LEFT/RIGHT CHANGE  X DECIDE  START LOAD  O EXIT");
    SetASCIIString2(1, 40.0f, 406.0f, 1, 0x80, 0x80, 0x80, tmp_str);

    MdeDispSetSquare(2, 36.0f, wrk->csr * 14 + 46, 270.0f, 12.0f, 0x50, 0x50, 0x64, 0x50);
}
