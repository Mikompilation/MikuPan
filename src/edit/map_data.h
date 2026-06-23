#ifndef EDIT_MAP_DATA_H
#define EDIT_MAP_DATA_H

#include "typedefs.h"

#include <stdint.h>

#define MDE_MAP_MISSION_NUM 5
#define MDE_MAP_FLOOR_NUM 4
#define MDE_MAP_SECTION_NUM 13
#define MDE_MAP_ROOM_MAX 42

typedef struct {
    int room_id;
    int x;
    int y;
    int z;
    int h;
} MDE_MAP_ROOM_POS;

int MdeMapDataLoadReq(int mission_no);
int MdeMapDataLoadCtrl(void);
void MdeMapDataSetFloor(int mission_no, int floor_no);
int MdeMapDataIsReady(void);
const char *MdeMapDataState(void);
int MdeMapDataMission(void);
int MdeMapDataFloor(void);

const char *MdeMapSectionName(int section_no);
int64_t MdeMapFloorTopAddr(int floor_no);
int MdeMapSectionOffset(int floor_no, int section_no);
int64_t MdeMapSectionAddr(int floor_no, int section_no);
int MdeMapRoomNum(int floor_no);
int MdeMapRoomIdFromRoomNo(int section_no, int room_no, int floor_no);
int MdeMapDataRoom(int section_no, int room_id);
int MdeMapDataNum(int section_no, int room_id);
int MdeMapRoomPos(int room_no, int floor_no, MDE_MAP_ROOM_POS *pos);
int MdeMapPointRoomNo(int pos_x, int pos_z, int floor_no);

#endif // EDIT_MAP_DATA_H
