#include "mikupan/mikupan_region.h"
#ifndef EDIT_MDE_MDSP_H
#define EDIT_MDE_MDSP_H

#include "typedefs.h"
#include "edit/mde_menu.h"


MIKUPAN_NAMESPACE_BEGIN

void MdeMdspLine(int line, char *str);
void MdeMdspSummary(const MDE_MENU_WRK *wrk);
void MdeMdspSectionTable(const MDE_MENU_WRK *wrk);
void MdeMdspRoomPage(const MDE_MENU_WRK *wrk);


MIKUPAN_NAMESPACE_END
#endif // EDIT_MDE_MDSP_H
