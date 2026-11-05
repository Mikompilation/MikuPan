#include "mikupan/mikupan_region.h"
#ifndef EDIT_MDE_FILE_H
#define EDIT_MDE_FILE_H

#include "typedefs.h"


MIKUPAN_NAMESPACE_BEGIN

void MdeFileInit(int mission_no, int floor_no);
void MdeFileSetFloor(int mission_no, int floor_no);
int MdeFileLoadReq(int mission_no);
int MdeFileLoadCtrl(void);
const char *MdeFileState(void);


MIKUPAN_NAMESPACE_END
#endif // EDIT_MDE_FILE_H
