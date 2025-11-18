#include "common.h"
#include "ev_load.h"
#include "main/glob.h"
#include "enums.h"
#include "ev_spcl.h"
#include "ingame/map/map_area.h"
#include "ingame/entry/entry.h"
#include "common/memory_addresses.h"
#include "graphics/graph2d/effect_ene.h"
#include "graphics/graph2d/tim2.h"
#include "graphics/motion/mdlwork.h"
#include "ingame/entry/ap_dgost.h"
#include "ingame/entry/ap_fgost.h"
#include "ingame/entry/ap_ggost.h"
#include "ingame/map/door_ctl.h"
#include "ingame/map/furn_ctl.h"
#include "ingame/map/item_ctl.h"
#include "ingame/map/map_ctrl.h"
#include "ingame/map/furn_spe/fspe_acs.h"
#include "os/eeiop/eese.h"
#include "os/eeiop/adpcm/ea_cmd.h"
#include "os/eeiop/adpcm/ea_ctrl.h"
#include "os/eeiop/cdvd/eecdvd.h"
#include "outgame/btl_mode/btl_menu.h"

#include <string.h>

MSN_TITLE_WRK mttl_wrk;
EVENT_LOAD_WRK ev_load_wrk;
u_char msn_start_room[] = {0, 0, 40, 29, 35};
MSN_LOAD_DAT load_dat_wrk[] = {
    {
        .file_no = 0,
        .file_type = 0,
        .tmp_no = 0,
        .addr = 0,
    },
    {
        .file_no = 0,
        .file_type = 0,
        .tmp_no = 0,
        .addr = 0,
    },
    {
        .file_no = 0,
        .file_type = 0,
        .tmp_no = 0,
        .addr = 0,
    },
    {
        .file_no = 0,
        .file_type = 0,
        .tmp_no = 0,
        .addr = 0,
    },
    {
        .file_no = 0,
        .file_type = 0,
        .tmp_no = 0,
        .addr = 0,
    },
    {
        .file_no = 0,
        .file_type = 0,
        .tmp_no = 0,
        .addr = 0,
    },
    {
        .file_no = 0,
        .file_type = 0,
        .tmp_no = 0,
        .addr = 0,
    },
    {
        .file_no = 0,
        .file_type = 0,
        .tmp_no = 0,
        .addr = 0,
    },
    {
        .file_no = 0,
        .file_type = 0,
        .tmp_no = 0,
        .addr = 0,
    },
    {
        .file_no = 0,
        .file_type = 0,
        .tmp_no = 0,
        .addr = 0,
    },
    {
        .file_no = 0,
        .file_type = 0,
        .tmp_no = 0,
        .addr = 0,
    },
    {
        .file_no = 0,
        .file_type = 0,
        .tmp_no = 0,
        .addr = 0,
    },
    {
        .file_no = 0,
        .file_type = 0,
        .tmp_no = 0,
        .addr = 0,
    },
    {
        .file_no = 0,
        .file_type = 0,
        .tmp_no = 0,
        .addr = 0,
    },
    {
        .file_no = 0,
        .file_type = 0,
        .tmp_no = 0,
        .addr = 0,
    },
    {
        .file_no = 0,
        .file_type = 0,
        .tmp_no = 0,
        .addr = 0,
    },
    {
        .file_no = 0,
        .file_type = 0,
        .tmp_no = 0,
        .addr = 0,
    },
    {
        .file_no = 0,
        .file_type = 0,
        .tmp_no = 0,
        .addr = 0,
    },
    {
        .file_no = 0,
        .file_type = 0,
        .tmp_no = 0,
        .addr = 0,
    },
    {
        .file_no = 0,
        .file_type = 0,
        .tmp_no = 0,
        .addr = 0,
    },
    {
        .file_no = 0,
        .file_type = 0,
        .tmp_no = 0,
        .addr = 0,
    },
    {
        .file_no = 0,
        .file_type = 0,
        .tmp_no = 0,
        .addr = 0,
    },
    {
        .file_no = 0,
        .file_type = 0,
        .tmp_no = 0,
        .addr = 0,
    },
    {
        .file_no = 0,
        .file_type = 0,
        .tmp_no = 0,
        .addr = 0,
    },
    {
        .file_no = 0,
        .file_type = 0,
        .tmp_no = 0,
        .addr = 0,
    },
    {
        .file_no = 0,
        .file_type = 0,
        .tmp_no = 0,
        .addr = 0,
    },
    {
        .file_no = 0,
        .file_type = 0,
        .tmp_no = 0,
        .addr = 0,
    },
    {
        .file_no = 0,
        .file_type = 0,
        .tmp_no = 0,
        .addr = 0,
    },
    {
        .file_no = 0,
        .file_type = 0,
        .tmp_no = 0,
        .addr = 0,
    },
    {
        .file_no = 0,
        .file_type = 0,
        .tmp_no = 0,
        .addr = 0,
    },
    {
        .file_no = 0,
        .file_type = 0,
        .tmp_no = 0,
        .addr = 0,
    },
    {
        .file_no = 0,
        .file_type = 0,
        .tmp_no = 0,
        .addr = 0,
    },
    {
        .file_no = 0,
        .file_type = 0,
        .tmp_no = 0,
        .addr = 0,
    },
    {
        .file_no = 0,
        .file_type = 0,
        .tmp_no = 0,
        .addr = 0,
    },
    {
        .file_no = 0,
        .file_type = 0,
        .tmp_no = 0,
        .addr = 0,
    },
    {
        .file_no = 0,
        .file_type = 0,
        .tmp_no = 0,
        .addr = 0,
    },
    {
        .file_no = 0,
        .file_type = 0,
        .tmp_no = 0,
        .addr = 0,
    },
    {
        .file_no = 0,
        .file_type = 0,
        .tmp_no = 0,
        .addr = 0,
    },
    {
        .file_no = 0,
        .file_type = 0,
        .tmp_no = 0,
        .addr = 0,
    },
    {
        .file_no = 0,
        .file_type = 0,
        .tmp_no = 0,
        .addr = 0,
    },
};

u_char msn_title_sp_flr_no[] = {12, 12, 12, 12, 12};
u_char msn_title_flr_sp_num[] = {1, 2, 1, 1, 1};
u_char msn_title_sp_ttl_no[] = {11, 11, 11, 11, 11};
u_char msn_title_ttl_sp_num[] = {2, 1, 1, 1, 1};

SPRT_SDAT msn_title00_sp_flr[] = {
    {
        .u = 0,
        .v = 0,
        .w = 384,
        .h = 168,
        .x = 256,
        .y = 251,
        .pri = 20,
        .alp = 128,
    },
};

SPRT_SDAT msn_title00_sp_ttl[] = {
    {
        .u = 0,
        .v = 0,
        .w = 256,
        .h = 72,
        .x = 343,
        .y = 274,
        .pri = 10,
        .alp = 128,
    },
    {
        .u = 100,
        .v = 75,
        .w = 128,
        .h = 53,
        .x = 436,
        .y = 349,
        .pri = 10,
        .alp = 128,
    },
};


SPRT_SDAT msn_title01_sp_flr[] = {
    {
        .u = 1,
        .v = 2,
        .w = 510,
        .h = 194,
        .x = 96,
        .y = 233,
        .pri = 20,
        .alp = 128,
    },
    {
        .u = 1,
        .v = 198,
        .w = 34,
        .h = 194,
        .x = 606,
        .y = 233,
        .pri = 20,
        .alp = 128,
    },
};

SPRT_SDAT msn_title01_sp_ttl[] = {
    {
        .u = 0,
        .v = 0,
        .w = 456,
        .h = 154,
        .x = 160,
        .y = 248,
        .pri = 10,
        .alp = 128,
    },
};

SPRT_SDAT msn_title02_sp_flr[] = {
    {
        .u = 0,
        .v = 0,
        .w = 432,
        .h = 168,
        .x = 0,
        .y = 272,
        .pri = 20,
        .alp = 128,
    },
};

SPRT_SDAT msn_title02_sp_ttl[] = {
    {
        .u = 0,
        .v = 0,
        .w = 323,
        .h = 137,
        .x = 13,
        .y = 296,
        .pri = 10,
        .alp = 128,
    },
};

SPRT_SDAT msn_title03_sp_flr[] = {
    {
        .u = 0,
        .v = 0,
        .w = 408,
        .h = 200,
        .x = 232,
        .y = 22,
        .pri = 20,
        .alp = 128,
    },
};

SPRT_SDAT msn_title03_sp_ttl[] = {
    {
        .u = 0,
        .v = 0,
        .w = 340,
        .h = 136,
        .x = 288,
        .y = 64,
        .pri = 10,
        .alp = 128,
    },
};

SPRT_SDAT msn_title04_sp_flr[] = {
    {
        .u = 0,
        .v = 0,
        .w = 352,
        .h = 168,
        .x = 0,
        .y = 280,
        .pri = 20,
        .alp = 128,
    },
};

SPRT_SDAT msn_title04_sp_ttl[] = {
    {
        .u = 0,
        .v = 0,
        .w = 272,
        .h = 128,
        .x = 17,
        .y = 306,
        .pri = 10,
        .alp = 128,
    },
};

SPRT_SDAT *msn_title_sp_flr[] = { msn_title00_sp_flr, msn_title01_sp_flr, msn_title02_sp_flr, msn_title03_sp_flr, msn_title04_sp_flr };
SPRT_SDAT *msn_title_sp_ttl[] = { msn_title00_sp_ttl, msn_title01_sp_ttl, msn_title02_sp_ttl, msn_title03_sp_ttl, msn_title04_sp_ttl };
SPRT_SDAT msn_title_sp_bak[] = {
    {
        .u = 0,
        .v = 0,
        .w = 512,
        .h = 256,
        .x = 0,
        .y = 0,
        .pri = 30,
        .alp = 128,
    },
    {
        .u = 0,
        .v = 0,
        .w = 128,
        .h = 128,
        .x = 512,
        .y = 0,
        .pri = 30,
        .alp = 128,
    },
    {
        .u = 0,
        .v = 0,
        .w = 128,
        .h = 128,
        .x = 512,
        .y = 128,
        .pri = 30,
        .alp = 128,
    },
    {
        .u = 0,
        .v = 0,
        .w = 256,
        .h = 128,
        .x = 0,
        .y = 256,
        .pri = 30,
        .alp = 128,
    },
    {
        .u = 0,
        .v = 0,
        .w = 256,
        .h = 128,
        .x = 256,
        .y = 256,
        .pri = 30,
        .alp = 128,
    },
    {
        .u = 0,
        .v = 0,
        .w = 128,
        .h = 128,
        .x = 512,
        .y = 256,
        .pri = 30,
        .alp = 128,
    },
    {
        .u = 0,
        .v = 0,
        .w = 128,
        .h = 64,
        .x = 0,
        .y = 384,
        .pri = 30,
        .alp = 128,
    },
    {
        .u = 0,
        .v = 0,
        .w = 128,
        .h = 64,
        .x = 128,
        .y = 384,
        .pri = 30,
        .alp = 128,
    },
    {
        .u = 0,
        .v = 0,
        .w = 128,
        .h = 64,
        .x = 256,
        .y = 384,
        .pri = 30,
        .alp = 128,
    },
    {
        .u = 0,
        .v = 0,
        .w = 128,
        .h = 64,
        .x = 384,
        .y = 384,
        .pri = 30,
        .alp = 128,
    },
    {
        .u = 0,
        .v = 0,
        .w = 128,
        .h = 64,
        .x = 512,
        .y = 384,
        .pri = 30,
        .alp = 128,
    },
};

MSN_LOAD_DAT msn0_title_load_dat[] = {
    {
        .file_no = 26,
        .file_type = 3,
        .tmp_no = 0,
        .addr = 8323072,
    },
    {
        .file_no = 29,
        .file_type = 4,
        .tmp_no = 0,
        .addr = 8323072,
    },
    {
        .file_no = 32,
        .file_type = 5,
        .tmp_no = 0,
        .addr = 8323072,
    },
    {
        .file_no = 35,
        .file_type = 6,
        .tmp_no = 0,
        .addr = 8323072,
    },
    {
        .file_no = 15,
        .file_type = 1,
        .tmp_no = 0,
        .addr = 8323072,
    },
    {
        .file_no = 21,
        .file_type = 1,
        .tmp_no = 0,
        .addr = 8257536,
    },
    {
        .file_no = 10,
        .file_type = 7,
        .tmp_no = 0,
        .addr = 8355840,
    },
    {
        .file_no = 1430,
        .file_type = 2,
        .tmp_no = 0,
        .addr = 22,
    },
    {
        .file_no = 1431,
        .file_type = 2,
        .tmp_no = 0,
        .addr = 23,
    },
    {
        .file_no = 1434,
        .file_type = 2,
        .tmp_no = 0,
        .addr = 24,
    },
    {
        .file_no = 1435,
        .file_type = 2,
        .tmp_no = 0,
        .addr = 25,
    },
    {
        .file_no = 857,
        .file_type = 8,
        .tmp_no = 0,
        .addr = 13107200,
    },
    {
        .file_no = 901,
        .file_type = 9,
        .tmp_no = 58,
        .addr = 10682368,
    },
    {
        .file_no = 1385,
        .file_type = 2,
        .tmp_no = 0,
        .addr = 16,
    },
    {
        .file_no = 860,
        .file_type = 8,
        .tmp_no = 0,
        .addr = 14680064,
    },
    {
        .file_no = 801,
        .file_type = 8,
        .tmp_no = 0,
        .addr = 15204352,
    },
    {
        .file_no = 920,
        .file_type = 10,
        .tmp_no = 2,
        .addr = 12845056,
    },
    {
        .file_no = 907,
        .file_type = 10,
        .tmp_no = 2,
        .addr = 12894208,
    },
    {
        .file_no = 925,
        .file_type = 10,
        .tmp_no = 61,
        .addr = 12943360,
    },
    {
        .file_no = 947,
        .file_type = 11,
        .tmp_no = 0,
        .addr = 16777216,
    },
    {
        .file_no = 949,
        .file_type = 11,
        .tmp_no = 0,
        .addr = 16908288,
    },
    {
        .file_no = 65535,
        .file_type = 0,
        .tmp_no = 0,
        .addr = 0,
    },
};

MSN_LOAD_DAT msn1_title_load_dat[] = {
    {
        .file_no = 26,
        .file_type = 3,
        .tmp_no = 0,
        .addr = 8323072,
    },
    {
        .file_no = 29,
        .file_type = 4,
        .tmp_no = 0,
        .addr = 8323072,
    },
    {
        .file_no = 32,
        .file_type = 5,
        .tmp_no = 0,
        .addr = 8323072,
    },
    {
        .file_no = 35,
        .file_type = 6,
        .tmp_no = 0,
        .addr = 8323072,
    },
    {
        .file_no = 16,
        .file_type = 1,
        .tmp_no = 0,
        .addr = 8323072,
    },
    {
        .file_no = 22,
        .file_type = 1,
        .tmp_no = 0,
        .addr = 8257536,
    },
    {
        .file_no = 11,
        .file_type = 7,
        .tmp_no = 0,
        .addr = 8355840,
    },
    {
        .file_no = 275,
        .file_type = 1,
        .tmp_no = 0,
        .addr = 30081024,
    },
    {
        .file_no = 805,
        .file_type = 8,
        .tmp_no = 0,
        .addr = 15204352,
    },
    {
        .file_no = 65535,
        .file_type = 0,
        .tmp_no = 0,
        .addr = 0,
    },
};

MSN_LOAD_DAT msn2_title_load_dat[] = {
    {
        .file_no = 27,
        .file_type = 3,
        .tmp_no = 0,
        .addr = 8323072,
    },
    {
        .file_no = 30,
        .file_type = 4,
        .tmp_no = 0,
        .addr = 8323072,
    },
    {
        .file_no = 33,
        .file_type = 5,
        .tmp_no = 0,
        .addr = 8323072,
    },
    {
        .file_no = 36,
        .file_type = 6,
        .tmp_no = 0,
        .addr = 8323072,
    },
    {
        .file_no = 17,
        .file_type = 1,
        .tmp_no = 0,
        .addr = 8323072,
    },
    {
        .file_no = 23,
        .file_type = 1,
        .tmp_no = 0,
        .addr = 8257536,
    },
    {
        .file_no = 12,
        .file_type = 7,
        .tmp_no = 0,
        .addr = 8355840,
    },
    {
        .file_no = 828,
        .file_type = 8,
        .tmp_no = 0,
        .addr = 14680064,
    },
    {
        .file_no = 954,
        .file_type = 11,
        .tmp_no = 0,
        .addr = 16777216,
    },
    {
        .file_no = 65535,
        .file_type = 0,
        .tmp_no = 0,
        .addr = 0,
    },
};

MSN_LOAD_DAT msn3_title_load_dat[] = {
    {
        .file_no = 28,
        .file_type = 3,
        .tmp_no = 0,
        .addr = 8323072,
    },
    {
        .file_no = 31,
        .file_type = 4,
        .tmp_no = 0,
        .addr = 8323072,
    },
    {
        .file_no = 34,
        .file_type = 5,
        .tmp_no = 0,
        .addr = 8323072,
    },
    {
        .file_no = 37,
        .file_type = 6,
        .tmp_no = 0,
        .addr = 8323072,
    },
    {
        .file_no = 18,
        .file_type = 1,
        .tmp_no = 0,
        .addr = 8323072,
    },
    {
        .file_no = 24,
        .file_type = 1,
        .tmp_no = 0,
        .addr = 8257536,
    },
    {
        .file_no = 13,
        .file_type = 7,
        .tmp_no = 0,
        .addr = 8355840,
    },
    {
        .file_no = 854,
        .file_type = 8,
        .tmp_no = 0,
        .addr = 13107200,
    },
    {
        .file_no = 897,
        .file_type = 9,
        .tmp_no = 55,
        .addr = 10682368,
    },
    {
        .file_no = 1379,
        .file_type = 2,
        .tmp_no = 1,
        .addr = 16,
    },
    {
        .file_no = 1438,
        .file_type = 2,
        .tmp_no = 0,
        .addr = 22,
    },
    {
        .file_no = 800,
        .file_type = 8,
        .tmp_no = 0,
        .addr = 14680064,
    },
    {
        .file_no = 65535,
        .file_type = 0,
        .tmp_no = 0,
        .addr = 0,
    },
};

MSN_LOAD_DAT msn4_title_load_dat[] = {
    {
        .file_no = 28,
        .file_type = 3,
        .tmp_no = 0,
        .addr = 8323072,
    },
    {
        .file_no = 31,
        .file_type = 4,
        .tmp_no = 0,
        .addr = 8323072,
    },
    {
        .file_no = 34,
        .file_type = 5,
        .tmp_no = 0,
        .addr = 8323072,
    },
    {
        .file_no = 37,
        .file_type = 6,
        .tmp_no = 0,
        .addr = 8323072,
    },
    {
        .file_no = 19,
        .file_type = 1,
        .tmp_no = 0,
        .addr = 8323072,
    },
    {
        .file_no = 25,
        .file_type = 1,
        .tmp_no = 0,
        .addr = 8257536,
    },
    {
        .file_no = 14,
        .file_type = 7,
        .tmp_no = 0,
        .addr = 8355840,
    },
    {
        .file_no = 826,
        .file_type = 8,
        .tmp_no = 0,
        .addr = 14155776,
    },
    {
        .file_no = 884,
        .file_type = 9,
        .tmp_no = 27,
        .addr = 12124160,
    },
    {
        .file_no = 1389,
        .file_type = 2,
        .tmp_no = 0,
        .addr = 18,
    },
    {
        .file_no = 827,
        .file_type = 8,
        .tmp_no = 0,
        .addr = 20840448,
    },
    {
        .file_no = 885,
        .file_type = 9,
        .tmp_no = 28,
        .addr = 20119552,
    },
    {
        .file_no = 1386,
        .file_type = 2,
        .tmp_no = 0,
        .addr = 17,
    },
    {
        .file_no = 1439,
        .file_type = 2,
        .tmp_no = 0,
        .addr = 22,
    },
    {
        .file_no = 863,
        .file_type = 8,
        .tmp_no = 0,
        .addr = 13107200,
    },
    {
        .file_no = 904,
        .file_type = 9,
        .tmp_no = 64,
        .addr = 10682368,
    },
    {
        .file_no = 1384,
        .file_type = 2,
        .tmp_no = 0,
        .addr = 16,
    },
    {
        .file_no = 805,
        .file_type = 8,
        .tmp_no = 0,
        .addr = 14680064,
    },
    {
        .file_no = 856,
        .file_type = 8,
        .tmp_no = 0,
        .addr = 15204352,
    },
    {
        .file_no = 65535,
        .file_type = 0,
        .tmp_no = 0,
        .addr = 0,
    },
};

MSN_LOAD_DAT *msn_title_load_dat[] = {
    msn0_title_load_dat, msn1_title_load_dat, msn2_title_load_dat, msn3_title_load_dat, msn4_title_load_dat,
};

static u_char msn_start_floor[] = {1, 1, 1, 0, 2};

void MissionTitleInit(int msn_no)
{
    DataLoadWrkInit();
    memset(&mttl_wrk, 0, sizeof(mttl_wrk));
    mttl_wrk.mode = 0;
    mttl_wrk.load_mode = 0;
    ingame_wrk.stts |= 0x28;
  
    if (ingame_wrk.game == 0)
    {
        map_wrk.floor = msn_start_floor[msn_no];
        mttl_wrk.load_id = LoadReq(msn_no + 0x26, &MISSION_TITLE_CARD_ADDRESS);
    }
    else if (ingame_wrk.game == 1)
    {
        mttl_wrk.load_id = StageTitleInit();
    }
    MissionStartMapItemInit(msn_no);
    AdpcmMapNoUse();
}

int MissionTitleMain(int msn_no)
{
    switch(mttl_wrk.mode) 
    {
        case MSN_TITLE_MODE_READY:
            if (IsLoadEnd(mttl_wrk.load_id) != 0) 
            {
                mttl_wrk.mode = MSN_TITLE_MODE_IN;
                mttl_wrk.time = 30;
                EAdpcmCmdPlay(0, 0, AO002_SHOU_TITLE_STR, 0, 0x3fff, 0x280, 0xfff, 0);
            }
        break;
        case MSN_TITLE_MODE_IN:
            if (mttl_wrk.time == 0)
            {
                mttl_wrk.mode = MSN_TITLE_MODE_LOAD;
                mttl_wrk.time = 180;
            }
            else
            {
                mttl_wrk.time -= 1;
            }
        break;
        case MSN_TITLE_MODE_LOAD:
            if (mttl_wrk.time != 0) 
            {
                mttl_wrk.time -= 1;
            }

            if (ingame_wrk.game == 0)
            {
                if (MissionTitleLoad(msn_no) != 0 && mttl_wrk.time == 0)
                {
                    mttl_wrk.mode = MSN_TITLE_MODE_OUT;
                    mttl_wrk.time = 30;
                }
            }
            else if (ingame_wrk.game == 1) 
            {
                if (StageTitleLoad() != 0 && mttl_wrk.time == 0)
                {
                    mttl_wrk.mode = MSN_TITLE_MODE_OUT;
                    mttl_wrk.time = 30;
                }
            }
        break;
        case MSN_TITLE_MODE_OUT:
            if (mttl_wrk.time != 0)
            {
                mttl_wrk.time -= 1;
            }
            else
            {
                mttl_wrk.mode = MSN_TITLE_MODE_END_PRE;
                EAdpcmFadeOut(30);
            }
        break;        
        case MSN_TITLE_MODE_END_PRE:
            if (IsEndAdpcmFadeOut() != 0) 
            {
                mttl_wrk.mode = MSN_TITLE_MODE_END;
            }
        break;
  
        case MSN_TITLE_MODE_END:
            ingame_wrk.stts &= 0x80 | 0x40 | 0x10 | 0x4 | 0x2 | 0x1;
            return 1;
        break;
    }
  
    if (mttl_wrk.mode != MSN_TITLE_MODE_READY && mttl_wrk.mode != MSN_TITLE_MODE_END) 
    {
        if (ingame_wrk.game == 0) 
        {
            MissionTitleDisp(msn_no);
        }
        else if (ingame_wrk.game == 1) 
        {
            StageTitleDisp(msn_no);
        }
    }

    return 0;
}

int MissionTitleLoad(int msn_no)
{
    if (mttl_wrk.load_mode == 9)
    {
        return 1;
    }
    
    if (mttl_wrk.load_mode == 0)
    {
        mttl_wrk.load_mode = 1;
        
        ReqMsnInitPlyr(msn_no);
        return 0;
    }
    else if (mttl_wrk.load_mode == 1)
    {
        if (MsnInitPlyr())
        {
            mttl_wrk.load_mode = 2;
        }

        return 0;
    }
    
    if (msn_title_load_dat[msn_no][mttl_wrk.load_count].file_no == 0xFFFF)
    {
        mttl_wrk.load_mode = 4;
        mttl_wrk.load_count = 0;
    }
    else if (mttl_wrk.load_mode == 3)
    {
        if (IsLoadEnd(mttl_wrk.load_id))
        {
            MissionDataLoadAfterInit(msn_title_load_dat[msn_no] + mttl_wrk.load_count);
            SetDataLoadWrk(msn_title_load_dat[msn_no] + mttl_wrk.load_count);
            mttl_wrk.load_mode = 2;
            mttl_wrk.load_count++;
        }
    }
    else if (mttl_wrk.load_mode == 2)
    {
        mttl_wrk.load_id = MissionDataLoadReq(&msn_title_load_dat[msn_no][mttl_wrk.load_count]);
        mttl_wrk.load_mode = 3;
    }
    else if (mttl_wrk.load_mode == 4)
    {
        if (mttl_wrk.load_count != 0)
        {
            FloatGhostLoadReq();
            mttl_wrk.load_mode = 6;
            return 0;
        }
        
        RoomMdlLoadReq(NULL, mttl_wrk.load_count, msn_no, msn_start_room[msn_no], 1);
        area_wrk.room[mttl_wrk.load_count] = msn_start_room[msn_no];
        mttl_wrk.load_mode = 5;
    }
    else if (mttl_wrk.load_mode == 5)
    {
        if (RoomMdlLoadWait() == 0)
        {
            return 0;
        }
        
        mttl_wrk.load_mode = 4;
        mttl_wrk.load_count++;
    } 
    else if (mttl_wrk.load_mode == 6)
    {
        if (FloatGhostLoadMain() == 0)
        {
            return 0;
        }
        
        FloatGhostAppearTypeSet(ap_wrk.fgst_no, 0, msn_start_room[msn_no]);
        if (GuardGhostAppearSet() == 0)
        {
            mttl_wrk.load_mode = 9;
        }
        else
        {
            mttl_wrk.load_mode = 8;
        }
    }
    else if (mttl_wrk.load_mode == 7)
    {
        if (DeadGhostLoad() == 0)
        {
            return 0;
        }
        
        if (GuardGhostAppearSet() == 0)
        {
            mttl_wrk.load_mode = 9;
        }
        else
        {
            mttl_wrk.load_mode = 8;
        }
    }
    else
    {
        if (mttl_wrk.load_mode != 8)
        {
            return 0;
        }
        
        if (GuardGhostLoad() != 0)
        {
            mttl_wrk.load_mode = 9;
        }
    }
    
    return 0;
}

int MissionDataLoadReq(MSN_LOAD_DAT* dat)
{
    int ret;

    if (dat->file_type == 2)
    {
        if (dat->addr == 16)
        {
            ap_wrk.fg_se_empty[0] = 1;
        }
        else if (dat->addr == 17)
        {
            ap_wrk.fg_se_empty[1] = 1;
        }
        else if (dat->addr == 18)
        {
            ap_wrk.fg_se_empty[2] = 1;
        }

        if (dat->tmp_no == 0)
        {
            ret = SeFileLoadAndSet(dat->file_no, dat->addr);
        }
        else
        {
            ret = SeFileLoadAndSetFGhost(dat->file_no, dat->addr);
        }
    }
    else
    {
        if (dat->file_type == 9)
        {
            LoadEneDmgTex(dat->tmp_no, (u_int *)(dat->addr + 0x98000));

            ret = LoadReq(dat->file_no, dat->addr);
        }
        else
        {
            ret = LoadReq(dat->file_no, dat->addr);
        }
    }

    return ret;
}

void MissionDataLoadAfterInit(MSN_LOAD_DAT* dat)
{
    switch (dat->file_type)
    {
        case 3:
            memcpy(&map_cam_dat, (void *)dat->addr, sizeof(map_cam_dat));
            break;

        
        case 4:
            memcpy(&map_cam_dat2, (void *)dat->addr, sizeof(map_cam_dat2));
            break;

        
        case 5:
            memcpy(&map_cam_dat3, (void *)dat->addr, sizeof(map_cam_dat3));
            break;

        case 6:
            memcpy(&map_cam_dat4, (void *)dat->addr, sizeof(map_cam_dat4));
            break;

        case 7:    
            FSpeMapDataMapping();
            map_wrk.dat_adr = GetFloorTopAddr(map_wrk.floor);
            break;

        case 8:
            motInitEnemyMdl((void *)dat->addr, dat->file_no - 799);
            break;

        case 9:
        case 10:
            motInitEnemyAnm((void *)dat->addr, dat->tmp_no, dat->file_no - 867);
            break;
        
        case 11:
            ItemLoadAfterInit(dat->file_no - 944, dat->addr);
            break;

        default:
            return;
        break;
    }

    return;
}

void DataLoadWrkInit()
{	
    int i;

    memset(&load_dat_wrk, 0, sizeof(load_dat_wrk));

    for (i = 0; i < 40; i++)
    {
        load_dat_wrk[i].file_no = -1;
    }
}

void SetDataLoadWrk(MSN_LOAD_DAT* dat)
{
    int i;

    for (i = 0; i < 40; i++)
    {
        if (load_dat_wrk[i].file_no == 0xFFFF)
        {
            load_dat_wrk[i].file_no = dat->file_no;
            load_dat_wrk[i].file_type = dat->file_type;
            load_dat_wrk[i].tmp_no = dat->tmp_no;
            load_dat_wrk[i].addr = dat->addr;
            return;
        }
    }
}

void DelDataLoadWrk(u_short file_no)
{        
    int i;

    for (i = 0; i < 40; i++)
    {
        if (load_dat_wrk[i].file_no == file_no)
        {
            if (file_no >= M000_MIKU_ANM && file_no <= I000_PLAY_CAMERA1_SGD)
            {
                motReleaseAniMdlBuf((u_short)file_no - M000_MIKU_ANM, (u_int *)load_dat_wrk[i].addr);
            }
            else if (load_dat_wrk[i].file_type == 2)
            {
                if (load_dat_wrk[i].addr == 16)
                {
                    ap_wrk.fg_se_empty[0] = 0;
                }
                else if (load_dat_wrk[i].addr == 17)
                {
                    ap_wrk.fg_se_empty[1] = 0;
                }
                else if (load_dat_wrk[i].addr == 18)
                {
                    ap_wrk.fg_se_empty[2] = 0;
                }
            }
            
            load_dat_wrk[i].file_no = -1;

            break;
        }
    }
}

u_int GetLoadDataAddr(u_short file_no)
{
    int i;

    for (i = 0; i < 40; i++)
    {
        if (load_dat_wrk[i].file_no == file_no)
        {
            return load_dat_wrk[i].addr;
        }
    }

    return 0;
}

void SortLoadDataAddr()
{
    info_log("SORT LOAD DATA ADDR \n");
}

void MissionTitleDisp(int msn_no)
{	
    /* s0 16 */ int i;
	/* s3 19 */ u_char alp_rate;
	/* 0x0(sp) */ SPRT_SDAT ssd;
    SPRT_SDAT *p;

    SetSprFile(MISSION_TITLE_CARD_ADDRESS);
    
    if (mttl_wrk.mode == 1)
    {
        alp_rate = ((0x1e - mttl_wrk.time) * 100) / 0x1e & 0xff;
    }
    else if (mttl_wrk.mode == 3 || mttl_wrk.mode == 4)
    {
        alp_rate = (mttl_wrk.time * 100) / 0x1e & 0xff;
    }
    else
    {
        alp_rate = 100;
    }

    for (i = 0; i < 11; i++)
    {
        SimpleDispSprt(&msn_title_sp_bak[i], MISSION_TITLE_CARD_ADDRESS, i, NULL, NULL, alp_rate);
    }

    for (i = 0; i < msn_title_flr_sp_num[msn_no]; i++) {
        SimpleDispAlphaSprt(&msn_title_sp_flr[msn_no][i], MISSION_TITLE_CARD_ADDRESS,
                            msn_title_sp_flr_no[msn_no], ((alp_rate * 0x46) / 100), 0);
    }

    for (i = 0; i < msn_title_ttl_sp_num[msn_no]; i++) {    
        SimpleDispSprt(&msn_title_sp_ttl[msn_no][i], MISSION_TITLE_CARD_ADDRESS, 
                       msn_title_sp_ttl_no[msn_no], NULL, NULL, alp_rate);    
    }
}

void StageTitleDisp(int msn_no)
{
}

void EventLoadDataInit()
{
    memset(&ev_load_wrk, 0, 8);
    return;
}

int EventLoadData(u_char load_no)
{
}

int GetLoadGhostInfo(u_char* load_inf)
{
}

void MikuCGDisp()
{
}

void RoomLoadReq(int load_room)
{
	int i;

    for (i = 0; i < 2; i++)
    {
        if (load_room == area_wrk.room[i])
        {
            return;
        }
    }
    
    for (i = 0; i < 60; i++) // Line 897
    {
        if (furn_wrk[i].room_id != room_wrk.disp_no[0]) // Line 898
        {
            if(furn_wrk[i].use == 0 || furn_wrk[i].use == 2) // Line 899 
            {
                FurnSetWrkNoUse(&furn_wrk[i], i); // Line 901
            }
        }
    } 
  
    DoorFreeFurnWrk(0); // Line 907
    FurnSortFurnWrk(0); // Line 910

    for (i = 0; i < 2; i++) // Line 914
    {
        if (plyr_wrk.pr_info.room_no != area_wrk.room[i]) // Line 915
        {
            RoomMdlLoadReq(0, i, ingame_wrk.msn_no, load_room, 2); // Line 917
            FloatGhostAppearTypeSet(ap_wrk.fgst_no, i, load_room); // Line 919
            area_wrk.room[i] = load_room;
            break;
        }
    }
}
