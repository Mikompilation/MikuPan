#include "common.h"
#include "typedefs.h"
#include "enums.h"
#include "edit/mde_main.h"

#include "edit/mde_disp.h"
#include "edit/mde_file.h"
#include "edit/mde_fmnu.h"
#include "edit/mde_menu.h"
#include "graphics/graph2d/g2d_main.h"

void MdeInit(void)
{
    gra2dInitST();
    MdeMenuInit();
    MdeFileInit(mde_menu_wrk.mission_no, mde_menu_wrk.floor_no);
    MdeFileLoadReq(mde_menu_wrk.mission_no);
}

void MdeCtrl(void)
{
    int act;

    if (MdeFileLoadCtrl() != 0)
    {
        MdeMenuClamp();
        mde_menu_wrk.msg = "LOAD END";
    }

    act = MdeMenuCtrl();

    if (MdeFmnuExec(act) != 0)
    {
        return;
    }

    MdeDispMain(&mde_menu_wrk);
    gra2dDraw(GRA2D_CALL_OG);
}
