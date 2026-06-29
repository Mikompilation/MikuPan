#ifndef EDIT_MDE_FILE_H
#define EDIT_MDE_FILE_H

#include "typedefs.h"

void MdeFileInit(int mission_no, int floor_no);
void MdeFileSetFloor(int mission_no, int floor_no);
int MdeFileLoadReq(int mission_no);
int MdeFileLoadCtrl(void);
const char *MdeFileState(void);

#endif // EDIT_MDE_FILE_H
