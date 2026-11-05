#include "mikupan/mikupan_region.h"
#ifndef OUTGAME_OUTGAME_H
#define OUTGAME_OUTGAME_H

#include "typedefs.h"


MIKUPAN_NAMESPACE_BEGIN

void OutGameCtrl();
void OutGameModeChange(u_char mode);
void OrgSetSquare(int pri, float x, float y, float w, float h, u_char r, u_char g, u_char b, u_char a);
void RoomSizeDisp();
void RoomSizeCheckCtrl();
void RoomSizeCheckInit();


MIKUPAN_NAMESPACE_END
#endif // OUTGAME_OUTGAME_H
