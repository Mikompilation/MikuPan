#include "common.h"
#include "typedefs.h"
#include "enums.h"
#include "edit/map_data.h"

#include "ingame/map/map_ctrl.h"
#include "main/glob.h"
#include "mikupan/mikupan_memory.h"
#include "os/eeiop/cdvd/eecdvd.h"

#include <string.h>

typedef struct {
    int mission_no;
    int floor_no;
    int load_id;
    int ready;
    const char *state_str;
} MDE_MAP_DATA_WRK;

static MDE_MAP_DATA_WRK mde_map_data_wrk = {
    0,
    0,
    -1,
    0,
    "NO DATA",
};

static const char *mde_map_section_name[MDE_MAP_SECTION_NUM] = {
    "ROOM_DAT",
    "CAMERA",
    "CAMERA_B",
    "CAMERA_D",
    "CAMERA_T",
    "HEIGHT",
    "HIT_CHECK",
    "REQ_EVENT",
    "REQ_SE",
    "FIND_DAT",
    "DOOR_DAT",
    "FURNITUR",
    "MOVE_MOT",
};

static int MdeMapGetSectionOffset(int floor_no, int section_no, int *section_offset);

static int MdeMapClamp(int value, int min, int max)
{
    if (value < min)
    {
        return min;
    }

    if (value > max)
    {
        return max;
    }

    return value;
}

static int MdeMapPs2ToHost(uint64_t ps2_addr, void **host)
{
    int64_t host_addr;

    if (host == NULL || ps2_addr == 0)
    {
        return 0;
    }

    if (MikuPan_TryGetHostAddressFromPs2Address(ps2_addr, &host_addr) == 0 || host_addr == 0)
    {
        return 0;
    }

    *host = (void *)host_addr;

    return 1;
}

static int MdeMapOffsetToHost(int offset, void **host)
{
    if (host == NULL)
    {
        return 0;
    }

    return MdeMapPs2ToHost((uint64_t)(uint32_t)offset + MAP_DATA_ADDRESS, host);
}

static int *MdeMapSectionTable(int floor_no, int section_no)
{
    int section_offset;
    void *host;

    if (
        MdeMapGetSectionOffset(floor_no, section_no, &section_offset) == 0 ||
        MdeMapOffsetToHost(section_offset, &host) == 0
    )
    {
        return NULL;
    }

    return (int *)host;
}

static u_char *MdeMapSectionRoomIds(int floor_no, int section_no)
{
    int *section_table;
    void *host;

    if (section_no == MAP_DOOR_DAT)
    {
        return NULL;
    }

    section_table = MdeMapSectionTable(floor_no, section_no);

    if (section_table == NULL || MdeMapOffsetToHost(section_table[0], &host) == 0)
    {
        return NULL;
    }

    return (u_char *)host;
}

static int MdeMapGetSectionOffset(int floor_no, int section_no, int *section_offset)
{
    int *addr;

    if (section_offset == NULL || section_no < 0 || section_no >= MDE_MAP_SECTION_NUM)
    {
        return 0;
    }

    addr = (int *)MdeMapFloorTopAddr(floor_no);

    if (addr == NULL)
    {
        return 0;
    }

    *section_offset = addr[section_no];

    return 1;
}

int MdeMapDataLoadReq(int mission_no)
{
    mde_map_data_wrk.mission_no = MdeMapClamp(mission_no, 0, MDE_MAP_MISSION_NUM - 1);
    mde_map_data_wrk.ready = 0;
    mde_map_data_wrk.load_id = LoadReq(MSN00MAP_OBJ + mde_map_data_wrk.mission_no, MAP_DATA_ADDRESS);

    if (mde_map_data_wrk.load_id >= 0)
    {
        mde_map_data_wrk.state_str = "LOAD REQ";
    }
    else
    {
        mde_map_data_wrk.state_str = "LOAD ERR";
    }

    return mde_map_data_wrk.load_id;
}

int MdeMapDataLoadCtrl(void)
{
    if (mde_map_data_wrk.load_id < 0)
    {
        return 0;
    }

    if (IsLoadEnd(mde_map_data_wrk.load_id) == 0)
    {
        return 0;
    }

    mde_map_data_wrk.load_id = -1;
    mde_map_data_wrk.ready = 1;
    MdeMapDataSetFloor(mde_map_data_wrk.mission_no, mde_map_data_wrk.floor_no);
    mde_map_data_wrk.state_str = "READY";

    return 1;
}

void MdeMapDataSetFloor(int mission_no, int floor_no)
{
    int i;

    mde_map_data_wrk.mission_no = MdeMapClamp(mission_no, 0, MDE_MAP_MISSION_NUM - 1);
    mde_map_data_wrk.floor_no = MdeMapClamp(floor_no, 0, MDE_MAP_FLOOR_NUM - 1);

    ingame_wrk.msn_no = mde_map_data_wrk.mission_no;
    map_wrk.floor = mde_map_data_wrk.floor_no;

    for (i = 0; i < MDE_MAP_FLOOR_NUM; i++)
    {
        map_wrk.flr_exist[i] = floor_exist[mde_map_data_wrk.mission_no][i];
    }

    if (mde_map_data_wrk.ready != 0)
    {
        map_wrk.dat_adr = MdeMapFloorTopAddr(mde_map_data_wrk.floor_no);
    }
    else
    {
        map_wrk.dat_adr = 0;
    }
}

int MdeMapDataIsReady(void)
{
    return mde_map_data_wrk.ready;
}

const char *MdeMapDataState(void)
{
    return mde_map_data_wrk.state_str;
}

int MdeMapDataMission(void)
{
    return mde_map_data_wrk.mission_no;
}

int MdeMapDataFloor(void)
{
    return mde_map_data_wrk.floor_no;
}

const char *MdeMapSectionName(int section_no)
{
    if (section_no < 0 || section_no >= MDE_MAP_SECTION_NUM)
    {
        return "INVALID";
    }

    return mde_map_section_name[section_no];
}

int64_t MdeMapFloorTopAddr(int floor_no)
{
    int *addr;
    void *host;

    if (mde_map_data_wrk.ready == 0)
    {
        return 0;
    }

    floor_no = MdeMapClamp(floor_no, 0, MDE_MAP_FLOOR_NUM - 1);

    if (floor_exist[mde_map_data_wrk.mission_no][floor_no] == 0)
    {
        return 0;
    }

    if (MdeMapPs2ToHost(MAP_DATA_ADDRESS, &host) == 0)
    {
        return 0;
    }

    addr = (int *)host;

    if (MdeMapOffsetToHost(addr[floor_no], &host) == 0)
    {
        return 0;
    }

    return (int64_t)host;
}

int MdeMapSectionOffset(int floor_no, int section_no)
{
    int section_offset;

    if (MdeMapGetSectionOffset(floor_no, section_no, &section_offset) == 0)
    {
        return 0;
    }

    return section_offset;
}

int64_t MdeMapSectionAddr(int floor_no, int section_no)
{
    int section_offset;
    void *host;

    if (
        MdeMapGetSectionOffset(floor_no, section_no, &section_offset) == 0 ||
        MdeMapOffsetToHost(section_offset, &host) == 0
    )
    {
        return 0;
    }

    return (int64_t)host;
}

int MdeMapRoomNum(int floor_no)
{
    u_char *room_ids;
    int room_num;

    room_ids = MdeMapSectionRoomIds(floor_no, MAP_ROOM_DAT);

    if (room_ids == NULL)
    {
        return 0;
    }

    room_num = room_ids[0];

    if (room_num < 0 || room_num > MDE_MAP_ROOM_MAX)
    {
        return 0;
    }

    return room_num;
}

int MdeMapRoomIdFromRoomNo(int section_no, int room_no, int floor_no)
{
    u_char *room_ids;
    int room_num;

    if (
        mde_map_data_wrk.ready == 0 ||
        section_no < 0 ||
        section_no >= MDE_MAP_SECTION_NUM
    )
    {
        return 0xff;
    }

    room_ids = MdeMapSectionRoomIds(floor_no, section_no);

    if (room_ids == NULL)
    {
        return 0xff;
    }

    room_num = room_ids[0];

    if (room_num > MDE_MAP_ROOM_MAX || room_no < 0 || room_no >= room_num)
    {
        return 0xff;
    }

    return room_ids[room_no + 1];
}

int MdeMapDataRoom(int section_no, int room_id)
{
    u_char *room_ids;
    int i;
    int room_num;

    if (
        mde_map_data_wrk.ready == 0 ||
        section_no < 0 ||
        section_no >= MDE_MAP_SECTION_NUM ||
        room_id == 0xff
    )
    {
        return 0xff;
    }

    room_ids = MdeMapSectionRoomIds(mde_map_data_wrk.floor_no, section_no);

    if (room_ids == NULL)
    {
        return 0xff;
    }

    room_num = room_ids[0];

    if (room_num > MDE_MAP_ROOM_MAX)
    {
        return 0xff;
    }

    for (i = 0; i < room_num; i++)
    {
        if (room_id == room_ids[i + 1])
        {
            return i;
        }
    }

    return 0xff;
}

int MdeMapDataNum(int section_no, int room_id)
{
    int *addr;
    int data_room;
    int section_offset;
    void *host;

    if (
        mde_map_data_wrk.ready == 0 ||
        section_no < 0 ||
        section_no >= MDE_MAP_SECTION_NUM ||
        room_id == 0xff
    )
    {
        return 0;
    }

    if (section_no == MAP_ROOM_DAT)
    {
        return 1;
    }

    if (section_no == MAP_DOOR_DAT)
    {
        return 0;
    }

    data_room = MdeMapDataRoom(section_no, room_id);

    if (data_room == 0xff)
    {
        return 0;
    }

    if (
        MdeMapGetSectionOffset(mde_map_data_wrk.floor_no, section_no, &section_offset) == 0 ||
        MdeMapOffsetToHost(section_offset + data_room * 4, &host) == 0
    )
    {
        return 0;
    }

    addr = (int *)host;

    if (MdeMapOffsetToHost(addr[1], &host) == 0)
    {
        return 0;
    }

    addr = (int *)host;

    if (MdeMapOffsetToHost(addr[0], &host) == 0)
    {
        return 0;
    }

    return *(u_char *)host;
}

int MdeMapRoomPos(int room_no, int floor_no, MDE_MAP_ROOM_POS *pos)
{
    int *addr;
    short *room_pos;
    u_char *room_ids;
    void *host;

    if (pos == NULL || room_no < 0 || room_no >= MdeMapRoomNum(floor_no))
    {
        return 0;
    }

    memset(pos, 0, sizeof(*pos));

    room_ids = MdeMapSectionRoomIds(floor_no, MAP_ROOM_DAT);
    addr = MdeMapSectionTable(floor_no, MAP_ROOM_DAT);

    if (room_ids == NULL || addr == NULL)
    {
        return 0;
    }

    pos->room_id = room_ids[room_no + 1];

    addr = &addr[room_no] + 1;

    if (MdeMapOffsetToHost(*addr, &host) == 0)
    {
        return 0;
    }

    addr = (int *)host;

    if (MdeMapOffsetToHost(*addr, &host) == 0)
    {
        return 0;
    }

    room_pos = (short *)host;

    pos->x = ((u_short *)room_pos)[0];
    pos->y = room_pos[1];
    pos->z = ((u_short *)room_pos)[2];
    pos->h = room_pos[3];

    return 1;
}

int MdeMapPointRoomNo(int pos_x, int pos_z, int floor_no)
{
    if (mde_map_data_wrk.ready == 0 || MdeMapRoomNum(floor_no) == 0)
    {
        return 0xff;
    }

    return 0xff;
}
