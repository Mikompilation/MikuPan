#include "common.h"
#include "typedefs.h"
#include "enums.h"
#include "edit/mde_mdsp.h"

#include "edit/map_data.h"
#include "edit/mde_file.h"
#include "graphics/graph2d/message.h"
#include "ingame/map/map_ctrl.h"
#include "mikupan/mikupan_memory.h"

#include <stdio.h>

#define MDE_MDSP_X 320.0f

void MdeMdspLine(int line, char *str)
{
    SetASCIIString2(1, MDE_MDSP_X, line * 13 + 58, 1, 0x80, 0x80, 0x80, str);
}

void MdeMdspSummary(const MDE_MENU_WRK *wrk)
{
    char tmp_str[256];
    MDE_MAP_ROOM_POS room_pos;
    int room_id;
    int data_room;
    int data_num;
    int point_room;

    room_id = MdeMenuCurrentRoomId();
    data_room = MdeMapDataRoom(wrk->section_no, room_id);
    data_num = MdeMapDataNum(wrk->section_no, room_id);
    point_room = MdeMapPointRoomNo(wrk->pos_x, wrk->pos_z, wrk->floor_no);

    sprintf(tmp_str, "STATE %s/%s", MdeFileState(), wrk->msg);
    MdeMdspLine(0, tmp_str);

    sprintf(tmp_str, "BASE  %08X", MAP_DATA_ADDRESS);
    MdeMdspLine(1, tmp_str);

    sprintf(tmp_str, "FLOOR %llX", (long long)MdeMapFloorTopAddr(wrk->floor_no));
    MdeMdspLine(2, tmp_str);

    sprintf(tmp_str, "SEC%02d O%08X", wrk->section_no, MdeMapSectionOffset(wrk->floor_no, wrk->section_no));
    MdeMdspLine(3, tmp_str);

    sprintf(tmp_str, "HOST  %llX", (long long)MdeMapSectionAddr(wrk->floor_no, wrk->section_no));
    MdeMdspLine(4, tmp_str);

    sprintf(tmp_str, "ROOMS %02d", MdeMapRoomNum(wrk->floor_no));
    MdeMdspLine(5, tmp_str);

    sprintf(tmp_str, "RID %02X PASS %d", room_id, room_id == 0xff ? 0 : IsRoomPass(room_id));
    MdeMdspLine(6, tmp_str);

    sprintf(tmp_str, "DROOM %02X NUM %02d", data_room, data_num);
    MdeMdspLine(7, tmp_str);

    if (MdeMapRoomPos(wrk->room_no, wrk->floor_no, &room_pos) != 0)
    {
        sprintf(tmp_str, "RPOS X%04X Y%04X", room_pos.x, room_pos.y & 0xffff);
        MdeMdspLine(8, tmp_str);

        sprintf(tmp_str, "     Z%04X H%04X", room_pos.z, room_pos.h & 0xffff);
        MdeMdspLine(9, tmp_str);
    }
    else
    {
        sprintf(tmp_str, "RPOS -");
        MdeMdspLine(8, tmp_str);
    }

    sprintf(tmp_str, "PNT ROOM %02X", point_room);
    MdeMdspLine(10, tmp_str);
}

void MdeMdspSectionTable(const MDE_MENU_WRK *wrk)
{
    char tmp_str[256];
    int i;
    int room_id;

    room_id = MdeMenuCurrentRoomId();

    for (i = 0; i < MDE_MAP_SECTION_NUM; i++)
    {
        sprintf(tmp_str, "%02d %-8.8s O%08X N%02d",
                i,
                MdeMapSectionName(i),
                MdeMapSectionOffset(wrk->floor_no, i),
                MdeMapDataNum(i, room_id));
        MdeMdspLine(i, tmp_str);
    }
}

void MdeMdspRoomPage(const MDE_MENU_WRK *wrk)
{
    char tmp_str[256];
    int i;
    int room_id;
    int data_room;

    room_id = MdeMenuCurrentRoomId();

    sprintf(tmp_str, "ROOM %02d ID %02X FL%d", wrk->room_no, room_id, wrk->floor_no);
    MdeMdspLine(0, tmp_str);

    sprintf(tmp_str, "PNT X%04X Z%04X R%02X", wrk->pos_x, wrk->pos_z,
            MdeMapPointRoomNo(wrk->pos_x, wrk->pos_z, wrk->floor_no));
    MdeMdspLine(1, tmp_str);

    for (i = 0; i < MDE_MAP_SECTION_NUM; i++)
    {
        data_room = MdeMapDataRoom(i, room_id);
        sprintf(tmp_str, "%02d %-8.8s D%02X N%02d",
                i, MdeMapSectionName(i), data_room, MdeMapDataNum(i, room_id));
        MdeMdspLine(i + 3, tmp_str);
    }
}
