#include "common.h"
#include "typedefs.h"
#include "enums.h"
#include "test_2d.h"

#include "common/memory_addresses.h"
#include "os/fileload.h"
#include "main/glob.h"
#include "outgame/outgame.h"
#include "graphics/graph2d/sprt.h"
#include "graphics/graph2d/number.h"
#include "graphics/graph2d/tim2.h"

#include "data/test_tex0.h" // extern SPRT_DAT test_tex0[];
#include "data/test_tex1.h" // extern SPRT_DAT test_tex1[];

void LayoutTestInit()
{
    FileLoadB(TEST2D_PK2, &TEST2D_PK2_ADDRESS);
}

void LayoutTestMain()
{
    SetSprFile(TEST2D_PK2_ADDRESS);
    LayoutTestDisp();

    if (*key_now[5] != 0)
    {
        OutGameModeChange(OUTGAME_MODE_TITLE);
    }
}

void LayoutTestDisp()
{
    DISP_SPRT ds;

    CopySprDToSpr(&ds, &test_tex0[0]);
    DispSprD(&ds);

    CopySprDToSpr(&ds, &test_tex0[1]);
    DispSprD(&ds);

    CopySprDToSpr(&ds, &test_tex0[2]);
    ds.att |= 1;
    DispSprD(&ds);

    CopySprDToSpr(&ds, &test_tex0[3]);
    ds.att |= 2;
    DispSprD(&ds);

    CopySprDToSpr(&ds, &test_tex0[4]);
    ds.att |= 3;
    DispSprD(&ds);

    for (int i = 0; i < 13; i++)
    {
        CopySprDToSpr(&ds, &test_tex1[i]);
        DispSprD(&ds);
    }

    NumberDisp(0xeb,0x140,100,0,0xff,0xff,0xff,0x80,0,5,0);
    NumberDisp(0xeb,0x140,0x96,0,0xff,0xff,0xff,0x80,0,5,1);
    NumberDisp(0xeb,0x140,200,0,0xff,0xff,0xff,0x80,0,5,2);
}
