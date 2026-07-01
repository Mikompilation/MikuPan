#include "common.h"
#include "typedefs.h"
#include "edit/mde_file.h"

#include "edit/map_data.h"

void MdeFileInit(int mission_no, int floor_no)
{
    MdeFileSetFloor(mission_no, floor_no);
}

void MdeFileSetFloor(int mission_no, int floor_no)
{
    MdeMapDataSetFloor(mission_no, floor_no);
}

int MdeFileLoadReq(int mission_no)
{
    return MdeMapDataLoadReq(mission_no);
}

int MdeFileLoadCtrl(void)
{
    return MdeMapDataLoadCtrl();
}

const char *MdeFileState(void)
{
    return MdeMapDataState();
}
