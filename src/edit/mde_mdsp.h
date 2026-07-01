#ifndef EDIT_MDE_MDSP_H
#define EDIT_MDE_MDSP_H

#include "typedefs.h"
#include "edit/mde_menu.h"

void MdeMdspLine(int line, char *str);
void MdeMdspSummary(const MDE_MENU_WRK *wrk);
void MdeMdspSectionTable(const MDE_MENU_WRK *wrk);
void MdeMdspRoomPage(const MDE_MENU_WRK *wrk);

#endif // EDIT_MDE_MDSP_H
