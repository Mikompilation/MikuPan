#include "common.h"
#include "ev_load.h"
#include "main/glob.h"
#include "enums.h"
#include "ev_spcl.h"
#include "ingame/map/map_area.h"
#include "ingame/entry/entry.h"
#include "mikupan/mikupan_memory.h"
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

#include <graphics/scene/scene.h>
#include <string.h>

MSN_TITLE_WRK mttl_wrk;
EVENT_LOAD_WRK ev_load_wrk;
u_char msn_start_room[] = {0, 0, 40, 29, 35};
MSN_LOAD_DAT load_dat_wrk[40] = {0};

u_char msn_title_sp_flr_no[] = {12, 12, 12, 12, 12};
u_char msn_title_flr_sp_num[] = {1, 2, 1, 1, 1};
u_char msn_title_sp_ttl_no[] = {11, 11, 11, 11, 11};
u_char msn_title_ttl_sp_num[] = {2, 1, 1, 1, 1};

SPRT_SDAT stg_title_sp_bak[];
SCN_LOAD_DAT *scn_load[] = {scn_load0, scn_load1, scn_load2, scn_load3, scn_load4};

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
        .addr = MSN_TITLE_DAT_ADDRESS,
    },
    {
        .file_no = 29,
        .file_type = 4,
        .tmp_no = 0,
        .addr = MSN_TITLE_DAT_ADDRESS
    },
    {
        .file_no = 32,
        .file_type = 5,
        .tmp_no = 0,
        .addr = MSN_TITLE_DAT_ADDRESS,
    },
    {
        .file_no = 35,
        .file_type = 6,
        .tmp_no = 0,
        .addr = MSN_TITLE_DAT_ADDRESS,
    },
    {
        .file_no = 15,
        .file_type = 1,
        .tmp_no = 0,
        .addr = MSN_TITLE_DAT_ADDRESS,
    },
    {
        .file_no = 21,
        .file_type = 1,
        .tmp_no = 0,
        .addr = MSN_TITLE_DAT_ADDRESS_1, // 0x7E0000
    },
    {
        .file_no = 10,
        .file_type = 7,
        .tmp_no = 0,
        .addr = MSN_TITLE_DAT_ADDRESS_2,
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
        .addr = M_SLCT_CMN_PK2_ADDRESS,
    },
    {
        .file_no = 901,
        .file_type = 9,
        .tmp_no = 58,
        .addr = SPRITE_ADDRESS,
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
        .addr = MSN_TITLE_DAT_ADDRESS_3,
    },
    {
        .file_no = 801,
        .file_type = 8,
        .tmp_no = 0,
        .addr = MSN_TITLE_DAT_ADDRESS_4,
    },
    {
        .file_no = 920,
        .file_type = 10,
        .tmp_no = 2,
        .addr = MSN_TITLE_DAT_ADDRESS_5,
    },
    {
        .file_no = 907,
        .file_type = 10,
        .tmp_no = 2,
        .addr = MSN_TITLE_DAT_ADDRESS_6,
    },
    {
        .file_no = 925,
        .file_type = 10,
        .tmp_no = 61,
        .addr = MSN_TITLE_DAT_ADDRESS_7,
    },
    {
        .file_no = 947,
        .file_type = 11,
        .tmp_no = 0,
        .addr = MSN_TITLE_DAT_ADDRESS_8,
    },
    {
        .file_no = 949,
        .file_type = 11,
        .tmp_no = 0,
        .addr = MSN_TITLE_DAT_ADDRESS_9,
    },
    {
        .file_no = 65535,
        .file_type = 0,
        .tmp_no = 0,
        .addr = 0,
    },
};

MSN_LOAD_DAT scn0022_load[] = {
    {
        .file_no = 2,
        .file_type = 12,
        .tmp_no = 0,
        .addr = NULL,
    },
    {
        .file_no = 0,
        .file_type = 0,
        .tmp_no = 0,
        .addr = NULL,
    },
};

u_short scn0022_del_dat[] = {65535};

SCN_LOAD_DAT scn_load0[] = {
    {
        .scn_no = 2,
        .load_dat = scn0022_load,
        .del_dat = scn0022_del_dat,
    },
    {
        .scn_no = 255,
        .load_dat = NULL,
        .del_dat = NULL,
    },
};

MSN_LOAD_DAT scn1010_load[] = {
    {
        .file_no = 802,
        .file_type = 8,
        .tmp_no = 0,
        .addr = 0xe00000,
    },
    {
        .file_no = 908,
        .file_type = 10,
        .tmp_no = 3,
        .addr = 0xc40000,
    },
    {
        .file_no = 1431,
        .file_type = 2,
        .tmp_no = 0,
        .addr = 0x16,
    },
    {
        .file_no = 947,
        .file_type = 11,
        .tmp_no = 0,
        .addr = 0x1000000,
    },
    {
        .file_no = 0,
        .file_type = 0,
        .tmp_no = 0,
        .addr = NULL,
    },
};

u_short scn1010_del_dat[] = {65535};

MSN_LOAD_DAT scn1030_load[] = {
    {
        .file_no = 976,
        .file_type = 11,
        .tmp_no = 0,
        .addr = 0x1020000,
    },
    {
        .file_no = 0,
        .file_type = 0,
        .tmp_no = 0,
        .addr = NULL,
    },
};

u_short scn1030_del_dat[] = {908, 65535};

MSN_LOAD_DAT scn1034_load[] = {
    {
        .file_no = 800,
        .file_type = 8,
        .tmp_no = 0,
        .addr = 0xf00000,
    },
    {
        .file_no = 910,
        .file_type = 10,
        .tmp_no = 3,
        .addr = 0xc40000,
    },
    {
        .file_no = 909,
        .file_type = 10,
        .tmp_no = 3,
        .addr = 0xc48000,
    },
    {
        .file_no = 924,
        .file_type = 10,
        .tmp_no = 1,
        .addr = 0xc58000,
    },
    {
        .file_no = 1430,
        .file_type = 2,
        .tmp_no = 0,
        .addr = 0x17,
    },
    {
        .file_no = 1434,
        .file_type = 2,
        .tmp_no = 0,
        .addr = 0x18,
    },
    {
        .file_no = 1442,
        .file_type = 2,
        .tmp_no = 0,
        .addr = 0x19,
    },
    {
        .file_no = 0,
        .file_type = 0,
        .tmp_no = 0,
        .addr = NULL,
    },
};

u_short scn1034_del_dat[] = {65535};

MSN_LOAD_DAT scn1160_load[] = {
    {
        .file_no = 0,
        .file_type = 0,
        .tmp_no = 0,
        .addr = NULL,
    },
};

u_short scn1160_del_dat[] = {976, 800, 924, 909, 1442, 1430, 65535};

MSN_LOAD_DAT scn1070_load[] = {
    {
        .file_no = 808,
        .file_type = 8,
        .tmp_no = 0,
        .addr = 0xe80000,
    },
    {
        .file_no = 913,
        .file_type = 10,
        .tmp_no = 3,
        .addr = 0xc48000,
    },
    {
        .file_no = 932,
        .file_type = 10,
        .tmp_no = 9,
        .addr = 0xc58000,
    },
    {
        .file_no = 948,
        .file_type = 11,
        .tmp_no = 0,
        .addr = 0x1000000,
    },
    {
        .file_no = 1441,
        .file_type = 2,
        .tmp_no = 0,
        .addr = 0x17,
    },
    {
        .file_no = 0,
        .file_type = 0,
        .tmp_no = 0,
        .addr = NULL,
    },
};

u_short scn1070_del_dat[] = {947, 805, 65535};

MSN_LOAD_DAT scn1090_load[] = {
    {
        .file_no = 848,
        .file_type = 8,
        .tmp_no = 0,
        .addr = 0xf00000,
    },
    {
        .file_no = 0,
        .file_type = 0,
        .tmp_no = 0,
        .addr = NULL,
    },
};

u_short scn1090_del_dat[] = {913, 65535};

MSN_LOAD_DAT scn1100_load[] = {
    {
        .file_no = 0,
        .file_type = 0,
        .tmp_no = 0,
        .addr = NULL,
    },
};

u_short scn1100_del_dat[] = {808, 932, 65535};

MSN_LOAD_DAT scn1102_load[] = {
    {
        .file_no = 809,
        .file_type = 8,
        .tmp_no = 0,
        .addr = 0xc80000,
    },
    {
        .file_no = 869,
        .file_type = 9,
        .tmp_no = 10,
        .addr = 0xa30000,
    },
    {
        .file_no = 1363,
        .file_type = 2,
        .tmp_no = 0,
        .addr = 0x10,
    },
    {
        .file_no = 11,
        .file_type = 12,
        .tmp_no = 0,
        .addr = NULL,
    },
    {
        .file_no = 0,
        .file_type = 0,
        .tmp_no = 0,
        .addr = NULL,
    },
};

u_short scn1102_del_dat[] = {848, 65535};

MSN_LOAD_DAT scn1120_load[] = {
    {
        .file_no = 911,
        .file_type = 10,
        .tmp_no = 10,
        .addr = 0xc40000,
    },
    {
        .file_no = 950,
        .file_type = 11,
        .tmp_no = 0,
        .addr = 0x1000000,
    },
    {
        .file_no = 955,
        .file_type = 11,
        .tmp_no = 0,
        .addr = 0x1010000,
    },
    {
        .file_no = 980,
        .file_type = 11,
        .tmp_no = 0,
        .addr = 0x1030000,
    },
    {
        .file_no = 1433,
        .file_type = 2,
        .tmp_no = 0,
        .addr = 0x19,
    },
    {
        .file_no = 0,
        .file_type = 0,
        .tmp_no = 0,
        .addr = NULL,
    },
};

u_short scn1120_del_dat[] = {802, 910, 948, 65535};

MSN_LOAD_DAT scn1170_load[] = {
    {
        .file_no = 803,
        .file_type = 8,
        .tmp_no = 0,
        .addr = 0xe00000,
    },
    {
        .file_no = 0,
        .file_type = 0,
        .tmp_no = 0,
        .addr = NULL,
    },
};

u_short scn1170_del_dat[] = {950, 955, 980, 65535};

MSN_LOAD_DAT scn1150_load[] = {
    {
        .file_no = 812,
        .file_type = 8,
        .tmp_no = 0,
        .addr = 0xc80000,
    },
    {
        .file_no = 872,
        .file_type = 9,
        .tmp_no = 13,
        .addr = 0xa30000,
    },
    {
        .file_no = 1381,
        .file_type = 2,
        .tmp_no = 0,
        .addr = 0x10,
    },
    {
        .file_no = 800,
        .file_type = 8,
        .tmp_no = 0,
        .addr = 0xe80000,
    },
    {
        .file_no = 859,
        .file_type = 8,
        .tmp_no = 0,
        .addr = 0xf80000,
    },
    {
        .file_no = 925,
        .file_type = 10,
        .tmp_no = 4,
        .addr = 0xc40000,
    },
    {
        .file_no = 917,
        .file_type = 10,
        .tmp_no = 4,
        .addr = 0xc50000,
    },
    {
        .file_no = 0,
        .file_type = 0,
        .tmp_no = 0,
        .addr = NULL,
    },
};

u_short scn1150_del_dat[] = {809, 869, 1363, 911, 1433, 65535};

MSN_LOAD_DAT scn1180_load[] = {
    {
        .file_no = 864,
        .file_type = 8,
        .tmp_no = 0,
        .addr = 0xd00000,
    },
    {
        .file_no = 905,
        .file_type = 9,
        .tmp_no = 65,
        .addr = 0xae0000,
    },
    {
        .file_no = 1389,
        .file_type = 2,
        .tmp_no = 0,
        .addr = 0x11,
    },
    {
        .file_no = 805,
        .file_type = 8,
        .tmp_no = 0,
        .addr = 0xf00000,
    },
    {
        .file_no = 926,
        .file_type = 10,
        .tmp_no = 6,
        .addr = 0xc50000,
    },
    {
        .file_no = 946,
        .file_type = 11,
        .tmp_no = 0,
        .addr = 0x1000000,
    },
    {
        .file_no = 952,
        .file_type = 11,
        .tmp_no = 0,
        .addr = 0x1020000,
    },
    {
        .file_no = 1440,
        .file_type = 2,
        .tmp_no = 0,
        .addr = 0x19,
    },
    {
        .file_no = 0,
        .file_type = 0,
        .tmp_no = 0,
        .addr = NULL,
    },
};

u_short scn1180_del_dat[] = {917, 65535};

MSN_LOAD_DAT scn1230_load[] = {
    {
        .file_no = 832,
        .file_type = 8,
        .tmp_no = 0,
        .addr = 0xf80000,
    },
    {
        .file_no = 927,
        .file_type = 10,
        .tmp_no = 4,
        .addr = 0xc50000,
    },
    {
        .file_no = 918,
        .file_type = 10,
        .tmp_no = 33,
        .addr = 0xc70000,
    },
    {
        .file_no = 823,
        .file_type = 8,
        .tmp_no = 0,
        .addr = 0xf00000,
    },
    {
        .file_no = 915,
        .file_type = 10,
        .tmp_no = 24,
        .addr = 0xc60000,
    },
    {
        .file_no = 982,
        .file_type = 11,
        .tmp_no = 0,
        .addr = 0x1040000,
    },
    {
        .file_no = 961,
        .file_type = 11,
        .tmp_no = 0,
        .addr = 0x1070000,
    },
    {
        .file_no = 1433,
        .file_type = 2,
        .tmp_no = 0,
        .addr = 0x16,
    },
    {
        .file_no = 1435,
        .file_type = 2,
        .tmp_no = 0,
        .addr = 0x18,
    },
    {
        .file_no = 0,
        .file_type = 0,
        .tmp_no = 0,
        .addr = NULL,
    },
};

u_short scn1230_del_dat[] = {872, 812, 1381, 905, 864, 1389, 800, 805, 859, 926, 1431, 1434, 65535};

MSN_LOAD_DAT scn1220_load[] = {
    {
        .file_no = 817,
        .file_type = 8,
        .tmp_no = 0,
        .addr = 0xc80000,
    },
    {
        .file_no = 876,
        .file_type = 9,
        .tmp_no = 18,
        .addr = 0xa30000,
    },
    {
        .file_no = 1367,
        .file_type = 2,
        .tmp_no = 0,
        .addr = 0x10,
    },
    {
        .file_no = 862,
        .file_type = 8,
        .tmp_no = 0,
        .addr = 0xd00000,
    },
    {
        .file_no = 903,
        .file_type = 9,
        .tmp_no = 63,
        .addr = 0xae0000,
    },
    {
        .file_no = 1382,
        .file_type = 2,
        .tmp_no = 0,
        .addr = 0x11,
    },
    {
        .file_no = 810,
        .file_type = 8,
        .tmp_no = 0,
        .addr = 0xd80000,
    },
    {
        .file_no = 870,
        .file_type = 9,
        .tmp_no = 11,
        .addr = 0xb90000,
    },
    {
        .file_no = 1364,
        .file_type = 2,
        .tmp_no = 0,
        .addr = 0x12,
    },
    {
        .file_no = 804,
        .file_type = 8,
        .tmp_no = 0,
        .addr = 0xe80000,
    },
    {
        .file_no = 928,
        .file_type = 10,
        .tmp_no = 5,
        .addr = 0xc50000,
    },
    {
        .file_no = 929,
        .file_type = 10,
        .tmp_no = 63,
        .addr = 0xc58000,
    },
    {
        .file_no = 1437,
        .file_type = 2,
        .tmp_no = 0,
        .addr = 0x19,
    },
    {
        .file_no = 0,
        .file_type = 0,
        .tmp_no = 0,
        .addr = NULL,
    },
};

u_short scn1220_del_dat[] = {952, 927, 1440, 65535};

MSN_LOAD_DAT scn1260_load[] = {
    {
        .file_no = 863,
        .file_type = 8,
        .tmp_no = 0,
        .addr = 0xc80000,
    },
    {
        .file_no = 904,
        .file_type = 9,
        .tmp_no = 64,
        .addr = 0xa30000,
    },
    {
        .file_no = 1384,
        .file_type = 2,
        .tmp_no = 0,
        .addr = 0x10,
    },
    {
        .file_no = 919,
        .file_type = 10,
        .tmp_no = 64,
        .addr = 0xc6c000,
    },
    {
        .file_no = 805,
        .file_type = 8,
        .tmp_no = 0,
        .addr = 0xe00000,
    },
    {
        .file_no = 921,
        .file_type = 10,
        .tmp_no = 6,
        .addr = 0xc4c000,
    },
    {
        .file_no = 920,
        .file_type = 10,
        .tmp_no = 5,
        .addr = 0xc40000,
    },
    {
        .file_no = 1432,
        .file_type = 2,
        .tmp_no = 0,
        .addr = 0x17,
    },
    {
        .file_no = 1440,
        .file_type = 2,
        .tmp_no = 0,
        .addr = 0x19,
    },
    {
        .file_no = 0,
        .file_type = 0,
        .tmp_no = 0,
        .addr = NULL,
    },
};

u_short scn1260_del_dat[] = {817, 876, 1367, 862, 903, 1382, 803, 925, 928, 929, 982, 1441, 1437, 65535};

MSN_LOAD_DAT scn1250_load[] = {
    {
        .file_no = 816,
        .file_type = 8,
        .tmp_no = 0,
        .addr = 0xe00000,
    },
    {
        .file_no = 0,
        .file_type = 0,
        .tmp_no = 0,
        .addr = NULL,
    },
};

u_short scn1250_del_dat[] = {805, 920, 921, 1440, 65535};

MSN_LOAD_DAT scn1280_load[] = {
    {
        .file_no = 811,
        .file_type = 8,
        .tmp_no = 0,
        .addr = 0xc80000,
    },
    {
        .file_no = 871,
        .file_type = 9,
        .tmp_no = 12,
        .addr = 0xa30000,
    },
    {
        .file_no = 1365,
        .file_type = 2,
        .tmp_no = 0,
        .addr = 0x10,
    },
    {
        .file_no = 829,
        .file_type = 8,
        .tmp_no = 0,
        .addr = 0xd00000,
    },
    {
        .file_no = 846,
        .file_type = 8,
        .tmp_no = 0,
        .addr = 0xf00000,
    },
    {
        .file_no = 923,
        .file_type = 10,
        .tmp_no = 47,
        .addr = 0xc48000,
    },
    {
        .file_no = 922,
        .file_type = 10,
        .tmp_no = 5,
        .addr = 0xc40000,
    },
    {
        .file_no = 0,
        .file_type = 0,
        .tmp_no = 0,
        .addr = NULL,
    },
};


u_short scn1280_del_dat[] = {810, 870, 1364, 863, 904, 1384, 919, 823, 915, 65535};

MSN_LOAD_DAT scn1301_load[] = {
    {
        .file_no = 953,
        .file_type = 11,
        .tmp_no = 0,
        .addr = 0x1000000,
    },
    {
        .file_no = 25,
        .file_type = 12,
        .tmp_no = 0,
        .addr = NULL,
    },
    {
        .file_no = 0,
        .file_type = 0,
        .tmp_no = 0,
        .addr = NULL,
    },
};

u_short scn1301_del_dat[] = {846, 923, 804, 922, 832, 918, 946, 961, 65535};

MSN_LOAD_DAT scn1320_load[] = {
    {
        .file_no = 954,
        .file_type = 11,
        .tmp_no = 0,
        .addr = 0x1010000,
    },
    {
        .file_no = 0,
        .file_type = 0,
        .tmp_no = 0,
        .addr = NULL,
    },
};

u_short scn1320_del_dat[] = {811, 871, 1365, 829, 65535};

MSN_LOAD_DAT scn1330_load[] = {
    {
        .file_no = 839,
        .file_type = 8,
        .tmp_no = 0,
        .addr = 0xc80000,
    },
    {
        .file_no = 895,
        .file_type = 9,
        .tmp_no = 40,
        .addr = 0xa30000,
    },
    {
        .file_no = 1380,
        .file_type = 2,
        .tmp_no = 0,
        .addr = 0x10,
    },
    {
        .file_no = 838,
        .file_type = 8,
        .tmp_no = 0,
        .addr = 0xe80000,
    },
    {
        .file_no = 0,
        .file_type = 0,
        .tmp_no = 0,
        .addr = NULL,
    },
};

u_short scn1330_del_dat[] = {953, 65535};

MSN_LOAD_DAT scn1410_load[] = {
    {
        .file_no = 0,
        .file_type = 0,
        .tmp_no = 0,
        .addr = NULL,
    },
};

u_short scn1410_del_dat[] = {954, 65535};

SCN_LOAD_DAT scn_load1[] = {
    {
        .scn_no = 6,
        .load_dat = scn1010_load,
        .del_dat = scn1010_del_dat,
    },
    {
        .scn_no = 7,
        .load_dat = scn1030_load,
        .del_dat = scn1030_del_dat,
    },
    {
        .scn_no = 9,
        .load_dat = scn1034_load,
        .del_dat = scn1034_del_dat,
    },
    {
        .scn_no = 18,
        .load_dat = scn1160_load,
        .del_dat = scn1160_del_dat,
    },
    {
        .scn_no = 10,
        .load_dat = scn1070_load,
        .del_dat = scn1070_del_dat,
    },
    {
        .scn_no = 12,
        .load_dat = scn1090_load,
        .del_dat = scn1090_del_dat,
    },
    {
        .scn_no = 13,
        .load_dat = scn1100_load,
        .del_dat = scn1100_del_dat,
    },
    {
        .scn_no = 15,
        .load_dat = scn1102_load,
        .del_dat = scn1102_del_dat,
    },
    {
        .scn_no = 16,
        .load_dat = scn1120_load,
        .del_dat = scn1120_del_dat,
    },
    {
        .scn_no = 19,
        .load_dat = scn1170_load,
        .del_dat = scn1170_del_dat,
    },
    {
        .scn_no = 17,
        .load_dat = scn1150_load,
        .del_dat = scn1150_del_dat,
    },
    {
        .scn_no = 20,
        .load_dat = scn1180_load,
        .del_dat = scn1180_del_dat,
    },
    {
        .scn_no = 22,
        .load_dat = scn1230_load,
        .del_dat = scn1230_del_dat,
    },
    {
        .scn_no = 21,
        .load_dat = scn1220_load,
        .del_dat = scn1220_del_dat,
    },
    {
        .scn_no = 25,
        .load_dat = scn1260_load,
        .del_dat = scn1260_del_dat,
    },
    {
        .scn_no = 24,
        .load_dat = scn1250_load,
        .del_dat = scn1250_del_dat,
    },
    {
        .scn_no = 26,
        .load_dat = scn1280_load,
        .del_dat = scn1280_del_dat,
    },
    {
        .scn_no = 28,
        .load_dat = scn1301_load,
        .del_dat = scn1301_del_dat,
    },
    {
        .scn_no = 29,
        .load_dat = scn1320_load,
        .del_dat = scn1320_del_dat,
    },
    {
        .scn_no = 30,
        .load_dat = scn1330_load,
        .del_dat = scn1330_del_dat,
    },
    {
        .scn_no = 34,
        .load_dat = scn1410_load,
        .del_dat = scn1410_del_dat,
    },
    {
        .scn_no = 255,
        .load_dat = NULL,
        .del_dat = NULL,
    },
};

MSN_LOAD_DAT scn2013_load[] = {
    {
        .file_no = 823,
        .file_type = 8,
        .tmp_no = 0,
        .addr = 0xc80000,
    },
    {
        .file_no = 882,
        .file_type = 9,
        .tmp_no = 24,
        .addr = 0xa30000,
    },
    {
        .file_no = 1366,
        .file_type = 2,
        .tmp_no = 0,
        .addr = 0x10,
    },
    {
        .file_no = 933,
        .file_type = 10,
        .tmp_no = 29,
        .addr = 0xc40000,
    },
    {
        .file_no = 1435,
        .file_type = 2,
        .tmp_no = 0,
        .addr = 0x16,
    },
    {
        .file_no = 0,
        .file_type = 0,
        .tmp_no = 0,
        .addr = NULL,
    },
};


u_short scn2013_del_dat[] = {65535};

MSN_LOAD_DAT scn2160_load[] = {
    {
        .file_no = 812,
        .file_type = 8,
        .tmp_no = 0,
        .addr = 0xd00000,
    },
    {
        .file_no = 872,
        .file_type = 9,
        .tmp_no = 13,
        .addr = 0xae0000,
    },
    {
        .file_no = 1381,
        .file_type = 2,
        .tmp_no = 0,
        .addr = 0x11,
    },
    {
        .file_no = 825,
        .file_type = 8,
        .tmp_no = 0,
        .addr = 0xe80000,
    },
    {
        .file_no = 817,
        .file_type = 8,
        .tmp_no = 0,
        .addr = 0xf00000,
    },
    {
        .file_no = 832,
        .file_type = 8,
        .tmp_no = 0,
        .addr = 0xf80000,
    },
    {
        .file_no = 935,
        .file_type = 10,
        .tmp_no = 18,
        .addr = 0xc48000,
    },
    {
        .file_no = 936,
        .file_type = 10,
        .tmp_no = 26,
        .addr = 0xc50000,
    },
    {
        .file_no = 918,
        .file_type = 10,
        .tmp_no = 33,
        .addr = 0xc5c000,
    },
    {
        .file_no = 1432,
        .file_type = 2,
        .tmp_no = 0,
        .addr = 0x17,
    },
    {
        .file_no = 1442,
        .file_type = 2,
        .tmp_no = 0,
        .addr = 0x18,
    },
    {
        .file_no = 1431,
        .file_type = 2,
        .tmp_no = 0,
        .addr = 0x19,
    },
    {
        .file_no = 0,
        .file_type = 0,
        .tmp_no = 0,
        .addr = NULL,
    },
};

u_short scn2160_del_dat[] = {954, 65535};

MSN_LOAD_DAT scn2020_load[] = {
    {
        .file_no = 822,
        .file_type = 8,
        .tmp_no = 0,
        .addr = 0xc80000,
    },
    {
        .file_no = 881,
        .file_type = 9,
        .tmp_no = 23,
        .addr = 0xa30000,
    },
    {
        .file_no = 1368,
        .file_type = 2,
        .tmp_no = 0,
        .addr = 0x10,
    },
    {
        .file_no = 0,
        .file_type = 0,
        .tmp_no = 0,
        .addr = NULL,
    },
};

u_short scn2020_del_dat[] = {823, 882, 1366, 65535};

MSN_LOAD_DAT scn2030_load[] = {
    {
        .file_no = 823,
        .file_type = 8,
        .tmp_no = 0,
        .addr = 0xd00000,
    },
    {
        .file_no = 882,
        .file_type = 9,
        .tmp_no = 24,
        .addr = 0xae0000,
    },
    {
        .file_no = 1366,
        .file_type = 2,
        .tmp_no = 0,
        .addr = 0x11,
    },
    {
        .file_no = 800,
        .file_type = 8,
        .tmp_no = 0,
        .addr = 0xe00000,
    },
    {
        .file_no = 934,
        .file_type = 10,
        .tmp_no = 1,
        .addr = 0xc6c000,
    },
    {
        .file_no = 0,
        .file_type = 0,
        .tmp_no = 0,
        .addr = NULL,
    },
};

u_short scn2030_del_dat[] = {812, 872, 1381, 828, 933, 65535};

MSN_LOAD_DAT scn2100_load[] = {
    {
        .file_no = 817,
        .file_type = 8,
        .tmp_no = 0,
        .addr = 0xc80000,
    },
    {
        .file_no = 876,
        .file_type = 9,
        .tmp_no = 18,
        .addr = 0xa30000,
    },
    {
        .file_no = 1367,
        .file_type = 2,
        .tmp_no = 0,
        .addr = 0x10,
    },
    {
        .file_no = 828,
        .file_type = 8,
        .tmp_no = 0,
        .addr = 0x1000000,
    },
    {
        .file_no = 805,
        .file_type = 8,
        .tmp_no = 0,
        .addr = 0xf00000,
    },
    {
        .file_no = 821,
        .file_type = 8,
        .tmp_no = 0,
        .addr = 0xf80000,
    },
    {
        .file_no = 933,
        .file_type = 10,
        .tmp_no = 29,
        .addr = 0xc40000,
    },
    {
        .file_no = 940,
        .file_type = 10,
        .tmp_no = 26,
        .addr = 0xc48000,
    },
    {
        .file_no = 915,
        .file_type = 10,
        .tmp_no = 24,
        .addr = 0xc50000,
    },
    {
        .file_no = 937,
        .file_type = 10,
        .tmp_no = 6,
        .addr = 0xc58000,
    },
    {
        .file_no = 1435,
        .file_type = 2,
        .tmp_no = 0,
        .addr = 0x16,
    },
    {
        .file_no = 1438,
        .file_type = 2,
        .tmp_no = 0,
        .addr = 0x17,
    },
    {
        .file_no = 1440,
        .file_type = 2,
        .tmp_no = 0,
        .addr = 0x18,
    },
    {
        .file_no = 0,
        .file_type = 0,
        .tmp_no = 0,
        .addr = NULL,
    },
};

u_short scn2100_del_dat[] = {822, 881, 1368, 934, 817, 935, 832, 918, 936, 1432, 1442, 1431, 65535};

MSN_LOAD_DAT scn2040_load[] = {
    {
        .file_no = 821,
        .file_type = 8,
        .tmp_no = 0,
        .addr = 0xc80000,
    },
    {
        .file_no = 880,
        .file_type = 9,
        .tmp_no = 22,
        .addr = 0xa30000,
    },
    {
        .file_no = 1369,
        .file_type = 2,
        .tmp_no = 0,
        .addr = 0x10,
    },
    {
        .file_no = 813,
        .file_type = 8,
        .tmp_no = 0,
        .addr = 0xd80000,
    },
    {
        .file_no = 873,
        .file_type = 9,
        .tmp_no = 14,
        .addr = 0xb90000,
    },
    {
        .file_no = 1383,
        .file_type = 2,
        .tmp_no = 0,
        .addr = 0x12,
    },
    {
        .file_no = 939,
        .file_type = 10,
        .tmp_no = 26,
        .addr = 0xc40000,
    },
    {
        .file_no = 1434,
        .file_type = 2,
        .tmp_no = 0,
        .addr = 0x19,
    },
    {
        .file_no = 0,
        .file_type = 0,
        .tmp_no = 0,
        .addr = NULL,
    },
};

u_short scn2040_del_dat[] = {817, 876, 1367, 800, 828, 933, 940, 915, 805, 937, 1435, 1438, 1440, 65535};

MSN_LOAD_DAT scn2170_load[] = {
    {
        .file_no = 832,
        .file_type = 8,
        .tmp_no = 0,
        .addr = 0xc80000,
    },
    {
        .file_no = 888,
        .file_type = 9,
        .tmp_no = 33,
        .addr = 0xa30000,
    },
    {
        .file_no = 1371,
        .file_type = 2,
        .tmp_no = 0,
        .addr = 0x10,
    },
    {
        .file_no = 967,
        .file_type = 11,
        .tmp_no = 0,
        .addr = 0x1000000,
    },
    {
        .file_no = 0,
        .file_type = 0,
        .tmp_no = 0,
        .addr = NULL,
    },
};

u_short scn2170_del_dat[] = {821, 821, 880, 1369, 823, 882, 1366, 65535};

MSN_LOAD_DAT scn2060_load[] = {
    {
        .file_no = 0,
        .file_type = 0,
        .tmp_no = 0,
        .addr = NULL,
    },
};

u_short scn2060_del_dat[] = {825, 939, 1434, 65535};

MSN_LOAD_DAT scn2062_load[] = {
    {
        .file_no = 0,
        .file_type = 0,
        .tmp_no = 0,
        .addr = NULL,
    },
};

u_short scn2062_del_dat[] = {65535};

MSN_LOAD_DAT scn2070_load[] = {
    {
        .file_no = 0,
        .file_type = 0,
        .tmp_no = 0,
        .addr = NULL,
    },
};

u_short scn2070_del_dat[] = {65535};

MSN_LOAD_DAT scn2072_load[] = {
    {
        .file_no = 825,
        .file_type = 8,
        .tmp_no = 0,
        .addr = 0xe80000,
    },
    {
        .file_no = 938,
        .file_type = 10,
        .tmp_no = 26,
        .addr = 0xc40000,
    },
    {
        .file_no = 985,
        .file_type = 11,
        .tmp_no = 0,
        .addr = 0x1020000,
    },
    {
        .file_no = 1441,
        .file_type = 2,
        .tmp_no = 0,
        .addr = 0x16,
    },
    {
        .file_no = 0,
        .file_type = 0,
        .tmp_no = 0,
        .addr = NULL,
    },
};

u_short scn2072_del_dat[] = {65535};

MSN_LOAD_DAT scn2090_load[] = {
    {
        .file_no = 823,
        .file_type = 8,
        .tmp_no = 0,
        .addr = 0xc80000,
    },
    {
        .file_no = 882,
        .file_type = 9,
        .tmp_no = 24,
        .addr = 0xa30000,
    },
    {
        .file_no = 1366,
        .file_type = 2,
        .tmp_no = 0,
        .addr = 0x10,
    },
    {
        .file_no = 0,
        .file_type = 0,
        .tmp_no = 0,
        .addr = NULL,
    },
};

u_short scn2090_del_dat[] = {832, 888, 1371, 825, 938, 967, 1441, 65535};

MSN_LOAD_DAT scn2092_load[] = {
    {
        .file_no = 815,
        .file_type = 8,
        .tmp_no = 0,
        .addr = 0xd00000,
    },
    {
        .file_no = 875,
        .file_type = 9,
        .tmp_no = 16,
        .addr = 0xae0000,
    },
    {
        .file_no = 1370,
        .file_type = 2,
        .tmp_no = 0,
        .addr = 0x11,
    },
    {
        .file_no = 825,
        .file_type = 8,
        .tmp_no = 0,
        .addr = 0xe80000,
    },
    {
        .file_no = 939,
        .file_type = 10,
        .tmp_no = 26,
        .addr = 0xc40000,
    },
    {
        .file_no = 0,
        .file_type = 0,
        .tmp_no = 0,
        .addr = NULL,
    },
};

u_short scn2092_del_dat[] = {813, 873, 1383, 65535};

MSN_LOAD_DAT scn2130_load[] = {
    {
        .file_no = 0,
        .file_type = 0,
        .tmp_no = 0,
        .addr = 0,
    },
};

u_short scn2130_del_dat[] = {823, 882, 1366, 815, 875, 1370, 825, 939, 985, 65535};

MSN_LOAD_DAT scn2132_load[] = {
    {
        .file_no = 855,
        .file_type = 8,
        .tmp_no = 0,
        .addr = 0xf00000,
    },
    {
        .file_no = 957,
        .file_type = 11,
        .tmp_no = 0,
        .addr = 0x1030000,
    },
    {
        .file_no = 28,
        .file_type = 12,
        .tmp_no = 0,
        .addr = NULL,
    },
    {
        .file_no = 0,
        .file_type = 0,
        .tmp_no = 0,
        .addr = NULL,
    },
};

u_short scn2132_del_dat[] = {65535};

MSN_LOAD_DAT scn2140_load[] = {
    {
        .file_no = 839,
        .file_type = 8,
        .tmp_no = 0,
        .addr = 0xd00000,
    },
    {
        .file_no = 895,
        .file_type = 9,
        .tmp_no = 40,
        .addr = 0xae0000,
    },
    {
        .file_no = 1380,
        .file_type = 2,
        .tmp_no = 0,
        .addr = 0x11,
    },
    {
        .file_no = 0,
        .file_type = 0,
        .tmp_no = 0,
        .addr = NULL,
    },
};

u_short scn2140_del_dat[] = {65535};

MSN_LOAD_DAT scn2141_load[] = {
    {
        .file_no = 0,
        .file_type = 0,
        .tmp_no = 0,
        .addr = NULL,
    },
};

u_short scn2141_del_dat[] = {855, 957, 65535};

MSN_LOAD_DAT scn2150_load[] = {
    {
        .file_no = 0,
        .file_type = 0,
        .tmp_no = 0,
        .addr = NULL,
    },
};

u_short scn2150_del_dat[] = {65535};

SCN_LOAD_DAT scn_load2[] = {
    {
        .scn_no = 36,
        .load_dat = scn2013_load,
        .del_dat = scn2013_del_dat,
    },
    {
        .scn_no = 60,
        .load_dat = scn2160_load,
        .del_dat = scn2160_del_dat,
    },
    {
        .scn_no = 37,
        .load_dat = scn2020_load,
        .del_dat = scn2020_del_dat,
    },
    {
        .scn_no = 38,
        .load_dat = scn2030_load,
        .del_dat = scn2030_del_dat,
    },
    {
        .scn_no = 50,
        .load_dat = scn2100_load,
        .del_dat = scn2100_del_dat,
    },
    {
        .scn_no = 39,
        .load_dat = scn2040_load,
        .del_dat = scn2040_del_dat,
    },
    {
        .scn_no = 61,
        .load_dat = scn2170_load,
        .del_dat = scn2170_del_dat,
    },
    {
        .scn_no = 41,
        .load_dat = scn2060_load,
        .del_dat = scn2060_del_dat,
    },
    {
        .scn_no = 43,
        .load_dat = scn2062_load,
        .del_dat = scn2062_del_dat,
    },
    {
        .scn_no = 44,
        .load_dat = scn2070_load,
        .del_dat = scn2070_del_dat,
    },
    {
        .scn_no = 46,
        .load_dat = scn2072_load,
        .del_dat = scn2072_del_dat,
    },
    {
        .scn_no = 47,
        .load_dat = scn2090_load,
        .del_dat = scn2090_del_dat,
    },
    {
        .scn_no = 49,
        .load_dat = scn2092_load,
        .del_dat = scn2092_del_dat,
    },
    {
        .scn_no = 52,
        .load_dat = scn2130_load,
        .del_dat = scn2130_del_dat,
    },
    {
        .scn_no = 54,
        .load_dat = scn2132_load,
        .del_dat = scn2132_del_dat,
    },
    {
        .scn_no = 55,
        .load_dat = scn2140_load,
        .del_dat = scn2140_del_dat,
    },
    {
        .scn_no = 56,
        .load_dat = scn2141_load,
        .del_dat = scn2141_del_dat,
    },
    {
        .scn_no = 59,
        .load_dat = scn2150_load,
        .del_dat = scn2150_del_dat,
    },
    {
        .scn_no = 255,
        .load_dat = NULL,
        .del_dat = NULL,
    },
};

MSN_LOAD_DAT scn3020_load[] = {
    {
        .file_no = 805,
        .file_type = 8,
        .tmp_no = 0,
        .addr = 0xe80000,
    },
    {
        .file_no = 979,
        .file_type = 11,
        .tmp_no = 0,
        .addr = 0x1000000,
    },
    {
        .file_no = 0,
        .file_type = 0,
        .tmp_no = 0,
        .addr = NULL,
    },
};

u_short scn3020_del_dat[] = {65535};

MSN_LOAD_DAT scn3030_load[] = {
    {
        .file_no = 858,
        .file_type = 8,
        .tmp_no = 0,
        .addr = 0xe00000,
    },
    {
        .file_no = 941,
        .file_type = 10,
        .tmp_no = 59,
        .addr = 0xc40000,
    },
    {
        .file_no = 942,
        .file_type = 10,
        .tmp_no = 59,
        .addr = 0xc50000,
    },
    {
        .file_no = 0,
        .file_type = 0,
        .tmp_no = 0,
        .addr = NULL,
    },
};

u_short scn3030_del_dat[] = {800, 65535};

MSN_LOAD_DAT scn3100_load[] = {
    {
        .file_no = 836,
        .file_type = 8,
        .tmp_no = 0,
        .addr = 0xc80000,
    },
    {
        .file_no = 866,
        .file_type = 8,
        .tmp_no = 0,
        .addr = 0xd00000,
    },
    {
        .file_no = 892,
        .file_type = 9,
        .tmp_no = 37,
        .addr = 0xa30000,
    },
    {
        .file_no = 1377,
        .file_type = 2,
        .tmp_no = 0,
        .addr = 0x10,
    },
    {
        .file_no = 960,
        .file_type = 11,
        .tmp_no = 0,
        .addr = 0x1000000,
    },
    {
        .file_no = 1352,
        .file_type = 2,
        .tmp_no = 0,
        .addr = 0x1,
    },
    {
        .file_no = 0,
        .file_type = 0,
        .tmp_no = 0,
        .addr = NULL,
    },
};

u_short scn3100_del_dat[] = {854, 897, 1379, 805, 979, 942, 65535};

MSN_LOAD_DAT scn3050_load[] = {
    {
        .file_no = 0,
        .file_type = 0,
        .tmp_no = 0,
        .addr = NULL,
    },
};

u_short scn3050_del_dat[] = {65535};

MSN_LOAD_DAT scn3110_load[] = {
    {
        .file_no = 0,
        .file_type = 0,
        .tmp_no = 0,
        .addr = NULL,
    },
};

u_short scn3110_del_dat[] = {960, 65535};

MSN_LOAD_DAT scn3060_load[] = {
    {
        .file_no = 957,
        .file_type = 11,
        .tmp_no = 0,
        .addr = 0x1000000,
    },
    {
        .file_no = 31,
        .file_type = 12,
        .tmp_no = 0,
        .addr = NULL,
    },
    {
        .file_no = 0,
        .file_type = 0,
        .tmp_no = 0,
        .addr = NULL,
    },
};

u_short scn3060_del_dat[] = {858, 941, 65535};

MSN_LOAD_DAT scn3070_load[] = {
    {
        .file_no = 0,
        .file_type = 0,
        .tmp_no = 0,
        .addr = NULL,
    },
};

u_short scn3070_del_dat[] = {65535};

MSN_LOAD_DAT scn4900_load[] = {
    {
        .file_no = 19,
        .file_type = 12,
        .tmp_no = 0,
        .addr = NULL,
    },
    {
        .file_no = 0,
        .file_type = 0,
        .tmp_no = 0,
        .addr = NULL,
    },
};

u_short scn4900_del_dat[] = {65535};

MSN_LOAD_DAT scn4901_load[] = {
    {
        .file_no = 31,
        .file_type = 12,
        .tmp_no = 0,
        .addr = NULL,
    },
    {
        .file_no = 0,
        .file_type = 0,
        .tmp_no = 0,
        .addr = NULL,
    },
};

u_short scn4901_del_dat[] = {65535};

SCN_LOAD_DAT scn_load3[] = {
    {
        .scn_no = 64,
        .load_dat = scn3020_load,
        .del_dat = scn3020_del_dat,
    },
    {
        .scn_no = 65,
        .load_dat = scn3030_load,
        .del_dat = scn3030_del_dat,
    },
    {
        .scn_no = 73,
        .load_dat = scn3100_load,
        .del_dat = scn3100_del_dat,
    },
    {
        .scn_no = 67,
        .load_dat = scn3050_load,
        .del_dat = scn3050_del_dat,
    },
    {
        .scn_no = 74,
        .load_dat = scn3110_load,
        .del_dat = scn3110_del_dat,
    },
    {
        .scn_no = 68,
        .load_dat = scn3060_load,
        .del_dat = scn3060_del_dat,
    },
    {
        .scn_no = 69,
        .load_dat = scn3070_load,
        .del_dat = scn3070_del_dat,
    },
    {
        .scn_no = 93,
        .load_dat = scn4900_load,
        .del_dat = scn4900_del_dat,
    },
    {
        .scn_no = 94,
        .load_dat = scn4901_load,
        .del_dat = scn4901_del_dat,
    },
    {
        .scn_no = 255,
        .load_dat = NULL,
        .del_dat = NULL,
    },
};

SCN_LOAD_DAT scn_load4[] = {
    {
        .scn_no = 76,
        .load_dat = scn4011_load,
        .del_dat = scn4011_del_dat,
    },
    {
        .scn_no = 77,
        .load_dat = scn4020_load,
        .del_dat = scn4020_del_dat,
    },
    {
        .scn_no = 78,
        .load_dat = scn4030_load,
        .del_dat = scn4030_del_dat,
    },
    {
        .scn_no = 80,
        .load_dat = scn4040_load,
        .del_dat = scn4040_del_dat,
    },
    {
        .scn_no = 82,
        .load_dat = scn4042_load,
        .del_dat = scn4042_del_dat,
    },
    {
        .scn_no = 83,
        .load_dat = scn4050_load,
        .del_dat = scn4050_del_dat,
    },
    {
        .scn_no = 85,
        .load_dat = scn4070_load,
        .del_dat = scn4070_del_dat,
    },
    {
        .scn_no = 93,
        .load_dat = scn4900_load,
        .del_dat = scn4900_del_dat,
    },
    {
        .scn_no = 94,
        .load_dat = scn4901_load,
        .del_dat = scn4901_del_dat,
    },
    {
        .scn_no = 255,
        .load_dat = NULL,
        .del_dat = NULL,
    },
};

MSN_LOAD_DAT scn4011_load[] = {
    {
        .file_no = 0,
        .file_type = 0,
        .tmp_no = 0,
        .addr = NULL,
    },
};

u_short scn4011_del_dat[] = {65535};

MSN_LOAD_DAT scn4020_load[] = {
    {
        .file_no = 0,
        .file_type = 0,
        .tmp_no = 0,
        .addr = NULL,
    },
};

u_short scn4020_del_dat[] = {65535};

MSN_LOAD_DAT scn4040_load[] = {
    {
        .file_no = 0,
        .file_type = 0,
        .tmp_no = 0,
        .addr = NULL,
    },
};

u_short scn4040_del_dat[] = {65535};

MSN_LOAD_DAT scn4042_load[] = {
    {
        .file_no = 0,
        .file_type = 0,
        .tmp_no = 0,
        .addr = NULL,
    },
};

u_short scn4042_del_dat[] = {65535};

MSN_LOAD_DAT scn4030_load[] = {
    {
        .file_no = 0,
        .file_type = 0,
        .tmp_no = 0,
        .addr = NULL,
    },
};

u_short scn4030_del_dat[] = {65535};

MSN_LOAD_DAT scn4050_load[] = {
    {
        .file_no = 816,
        .file_type = 8,
        .tmp_no = 0,
        .addr = 0xf00000,
    },
    {
        .file_no = 943,
        .file_type = 10,
        .tmp_no = 6,
        .addr = 0xc40000,
    },
    {
        .file_no = 0,
        .file_type = 0,
        .tmp_no = 0,
        .addr = NULL,
    },
};

u_short scn4050_del_dat[] = {863, 904, 1384, 65535};

MSN_LOAD_DAT scn4070_load[] = {
    {
        .file_no = 839,
        .file_type = 8,
        .tmp_no = 0,
        .addr = 0xc80000,
    },
    {
        .file_no = 895,
        .file_type = 9,
        .tmp_no = 40,
        .addr = 0xa30000,
    },
    {
        .file_no = 1380,
        .file_type = 2,
        .tmp_no = 0,
        .addr = 0x10,
    },
    {
        .file_no = 17,
        .file_type = 12,
        .tmp_no = 0,
        .addr = NULL,
    },
    {
        .file_no = 0,
        .file_type = 0,
        .tmp_no = 0,
        .addr = NULL,
    },
};

u_short scn4070_del_dat[] = {805, 856, 937, 943, 65535};

u_char msn_start_floor[] = {1, 1, 1, 0, 2};

SPRT_SDAT stg_title_sp_flr[] = {
    {
        .u = 0,
        .v = 0,
        .w = 156,
        .h = 426,
        .x = 458,
        .y = 0,
        .pri = 20,
        .alp = 128,
    },
};

SPRT_SDAT stg_title_sp_ttl[] = {
    {
        .u = 80,
        .v = 0,
        .w = 49,
        .h = 133,
        .x = 552,
        .y = 35,
        .pri = 10,
        .alp = 128,
    },
    {
        .u = 0,
        .v = 0,
        .w = 78,
        .h = 212,
        .x = 471,
        .y = 98,
        .pri = 10,
        .alp = 128,
    },
};

MSN_LOAD_DAT msn1_title_load_dat[] = {
    {
        .file_no = 26,
        .file_type = 3,
        .tmp_no = 0,
        .addr = 0x7f0000,
    },
    {
        .file_no = 29,
        .file_type = 4,
        .tmp_no = 0,
        .addr = 0x7f0000,
    },
    {
        .file_no = 32,
        .file_type = 5,
        .tmp_no = 0,
        .addr = 0x7f0000,
    },
    {
        .file_no = 35,
        .file_type = 6,
        .tmp_no = 0,
        .addr = 0x7f0000,
    },
    {
        .file_no = 16,
        .file_type = 1,
        .tmp_no = 0,
        .addr = 0x7f0000,
    },
    {
        .file_no = 22,
        .file_type = 1,
        .tmp_no = 0,
        .addr = MSN_TITLE_DAT_ADDRESS_1,
    },
    {
        .file_no = 11,
        .file_type = 7,
        .tmp_no = 0,
        .addr = MSN_TITLE_DAT_ADDRESS_2,
    },
    {
        .file_no = 275,
        .file_type = 1,
        .tmp_no = 0,
        .addr = 0x1cb0000,
    },
    {
        .file_no = 805,
        .file_type = 8,
        .tmp_no = 0,
        .addr = MSN_TITLE_DAT_ADDRESS_4,
    },
    {
        .file_no = 65535,
        .file_type = 0,
        .tmp_no = 0,
        .addr = NULL,
    },
};

MSN_LOAD_DAT msn2_title_load_dat[] = {
    {
        .file_no = 27,
        .file_type = 3,
        .tmp_no = 0,
        .addr = 0x7f0000,
    },
    {
        .file_no = 30,
        .file_type = 4,
        .tmp_no = 0,
        .addr = 0x7f0000,
    },
    {
        .file_no = 33,
        .file_type = 5,
        .tmp_no = 0,
        .addr = 0x7f0000,
    },
    {
        .file_no = 36,
        .file_type = 6,
        .tmp_no = 0,
        .addr = 0x7f0000,
    },
    {
        .file_no = 17,
        .file_type = 1,
        .tmp_no = 0,
        .addr = 0x7f0000,
    },
    {
        .file_no = 23,
        .file_type = 1,
        .tmp_no = 0,
        .addr = MSN_TITLE_DAT_ADDRESS_1,
    },
    {
        .file_no = 12,
        .file_type = 7,
        .tmp_no = 0,
        .addr = MSN_TITLE_DAT_ADDRESS_2,
    },
    {
        .file_no = 828,
        .file_type = 8,
        .tmp_no = 0,
        .addr = MSN_TITLE_DAT_ADDRESS_3,
    },
    {
        .file_no = 954,
        .file_type = 11,
        .tmp_no = 0,
        .addr = MSN_TITLE_DAT_ADDRESS_8,
    },
    {
        .file_no = 65535,
        .file_type = 0,
        .tmp_no = 0,
        .addr = NULL,
    },
};

MSN_LOAD_DAT msn3_title_load_dat[] = {
    {
        .file_no = 28,
        .file_type = 3,
        .tmp_no = 0,
        .addr = 0x7f0000,
    },
    {
        .file_no = 31,
        .file_type = 4,
        .tmp_no = 0,
        .addr = 0x7f0000,
    },
    {
        .file_no = 34,
        .file_type = 5,
        .tmp_no = 0,
        .addr = 0x7f0000,
    },
    {
        .file_no = 37,
        .file_type = 6,
        .tmp_no = 0,
        .addr = 0x7f0000,
    },
    {
        .file_no = 18,
        .file_type = 1,
        .tmp_no = 0,
        .addr = 0x7f0000,
    },
    {
        .file_no = 24,
        .file_type = 1,
        .tmp_no = 0,
        .addr = MSN_TITLE_DAT_ADDRESS_1,
    },
    {
        .file_no = 13,
        .file_type = 7,
        .tmp_no = 0,
        .addr = MSN_TITLE_DAT_ADDRESS_2,
    },
    {
        .file_no = 854,
        .file_type = 8,
        .tmp_no = 0,
        .addr = 0xc80000,
    },
    {
        .file_no = 897,
        .file_type = 9,
        .tmp_no = 55,
        .addr = SPRITE_ADDRESS,
    },
    {
        .file_no = 1379,
        .file_type = 2,
        .tmp_no = 1,
        .addr = 0x10,
    },
    {
        .file_no = 1438,
        .file_type = 2,
        .tmp_no = 0,
        .addr = 0x16,
    },
    {
        .file_no = 800,
        .file_type = 8,
        .tmp_no = 0,
        .addr = MSN_TITLE_DAT_ADDRESS_3,
    },
    {
        .file_no = 65535,
        .file_type = 0,
        .tmp_no = 0,
        .addr = NULL,
    },
};

MSN_LOAD_DAT msn4_title_load_dat[] = {
    {
        .file_no = 28,
        .file_type = 3,
        .tmp_no = 0,
        .addr = 0x7f0000,
    },
    {
        .file_no = 31,
        .file_type = 4,
        .tmp_no = 0,
        .addr = 0x7f0000,
    },
    {
        .file_no = 34,
        .file_type = 5,
        .tmp_no = 0,
        .addr = 0x7f0000,
    },
    {
        .file_no = 37,
        .file_type = 6,
        .tmp_no = 0,
        .addr = 0x7f0000,
    },
    {
        .file_no = 19,
        .file_type = 1,
        .tmp_no = 0,
        .addr = 0x7f0000,
    },
    {
        .file_no = 25,
        .file_type = 1,
        .tmp_no = 0,
        .addr = MSN_TITLE_DAT_ADDRESS_1,
    },
    {
        .file_no = 14,
        .file_type = 7,
        .tmp_no = 0,
        .addr = MSN_TITLE_DAT_ADDRESS_2,
    },
    {
        .file_no = 826,
        .file_type = 8,
        .tmp_no = 0,
        .addr = 0xd80000,
    },
    {
        .file_no = 884,
        .file_type = 9,
        .tmp_no = 27,
        .addr = 0xb90000,
    },
    {
        .file_no = 1389,
        .file_type = 2,
        .tmp_no = 0,
        .addr = 0x12,
    },
    {
        .file_no = 827,
        .file_type = 8,
        .tmp_no = 0,
        .addr = 0x13e0000,
    },
    {
        .file_no = 885,
        .file_type = 9,
        .tmp_no = 28,
        .addr = 0x1330000,
    },
    {
        .file_no = 1386,
        .file_type = 2,
        .tmp_no = 0,
        .addr = 0x11,
    },
    {
        .file_no = 1439,
        .file_type = 2,
        .tmp_no = 0,
        .addr = 0x16,
    },
    {
        .file_no = 863,
        .file_type = 8,
        .tmp_no = 0,
        .addr = 0xc80000,
    },
    {
        .file_no = 904,
        .file_type = 9,
        .tmp_no = 64,
        .addr = SPRITE_ADDRESS,
    },
    {
        .file_no = 1384,
        .file_type = 2,
        .tmp_no = 0,
        .addr = 0x10,
    },
    {
        .file_no = 805,
        .file_type = 8,
        .tmp_no = 0,
        .addr = MSN_TITLE_DAT_ADDRESS_3,
    },
    {
        .file_no = 856,
        .file_type = 8,
        .tmp_no = 0,
        .addr = MSN_TITLE_DAT_ADDRESS_4,
    },
    {
        .file_no = 65535,
        .file_type = 0,
        .tmp_no = 0,
        .addr = NULL,
    },
};

MSN_LOAD_DAT *msn_title_load_dat[] = {
    msn0_title_load_dat, msn1_title_load_dat, msn2_title_load_dat, msn3_title_load_dat, msn4_title_load_dat,
};

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
        mttl_wrk.load_id = LoadReq(msn_no + MSN00TTL_PK2, MISSION_TITLE_CARD_ADDRESS);
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
            ret = LoadReq(dat->file_no, (int64_t)dat->addr);
        }
        else
        {
            ret = LoadReq(dat->file_no, (int64_t)dat->addr);
        }
    }

    return ret;
}

void MissionDataLoadAfterInit(MSN_LOAD_DAT* dat)
{
    switch (dat->file_type)
    {
        case 3:
            memcpy(&map_cam_dat, (void *)MikuPan_GetHostPointer(dat->addr), sizeof(map_cam_dat));
            break;

        
        case 4:
            memcpy(&map_cam_dat2, (void *)MikuPan_GetHostPointer(dat->addr), sizeof(map_cam_dat2));
            break;

        
        case 5:
            memcpy(&map_cam_dat3, (void *)MikuPan_GetHostPointer(dat->addr), sizeof(map_cam_dat3));
            break;

        case 6:
            memcpy(&map_cam_dat4, (void *)MikuPan_GetHostPointer(dat->addr), sizeof(map_cam_dat4));
            break;

        case 7:    
            FSpeMapDataMapping();
            map_wrk.dat_adr = GetFloorTopAddr(map_wrk.floor);
            break;

        case 8:
            motInitEnemyMdl((void *)MikuPan_GetHostPointer(dat->addr), dat->file_no - 799);
            break;

        case 9:
        case 10:
            motInitEnemyAnm((void *)MikuPan_GetHostPointer(dat->addr), dat->tmp_no, dat->file_no - 867);
            break;
        
        case 11:
            ItemLoadAfterInit(dat->file_no - 944, MikuPan_GetHostAddress(dat->addr));
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

    SetSprFile((int64_t)MISSION_TITLE_CARD_ADDRESS);
    
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
        SimpleDispSprt(&msn_title_sp_bak[i], MikuPan_GetHostAddress(MISSION_TITLE_CARD_ADDRESS), i, NULL, NULL, alp_rate);
    }

    for (i = 0; i < msn_title_flr_sp_num[msn_no]; i++) {
        SimpleDispAlphaSprt(&msn_title_sp_flr[msn_no][i], MikuPan_GetHostAddress(MISSION_TITLE_CARD_ADDRESS),
                            msn_title_sp_flr_no[msn_no], ((alp_rate * 0x46) / 100), 0);
    }

    for (i = 0; i < msn_title_ttl_sp_num[msn_no]; i++) {    
        SimpleDispSprt(&msn_title_sp_ttl[msn_no][i], MikuPan_GetHostAddress(MISSION_TITLE_CARD_ADDRESS),
                       msn_title_sp_ttl_no[msn_no], NULL, NULL, alp_rate);    
    }
}

void StageTitleDisp(int msn_no)
{
	int i;
	u_char alp_rate;
	SPRT_SDAT ssd;

    SetSprFile(MISSION_TITLE_CARD_ADDRESS);

    if (mttl_wrk.mode == 1)
    {
        alp_rate = (30 - mttl_wrk.time) * 100 / 30;
    }

    else if (mttl_wrk.mode == 3 || mttl_wrk.mode == 4)
    {
        alp_rate = mttl_wrk.time * 100 / 30;
    }
    else
    {
        alp_rate = 100;
    }
  
    for (i = 0; i < 11; i++)
    {
        SimpleDispSprt(&stg_title_sp_bak[i], MikuPan_GetHostAddress(MISSION_TITLE_CARD_ADDRESS), i, NULL, NULL, alp_rate);
    }
}

void EventLoadDataInit()
{
    memset(&ev_load_wrk, 0, 8);
}

int EventLoadData(u_char load_no)
{	
    SCN_LOAD_DAT *sld;
	MSN_LOAD_DAT *mld;
	int i;
  
    if (ev_load_wrk.mode == EV_LOAD_MODE_END)
    {
        return 1;
    }

    sld = scn_load[ingame_wrk.msn_no];

    i = 0;

    while (1)
    {
        if (sld[i].scn_no == 0xff)
        {
            ev_load_wrk.mode = EV_LOAD_MODE_END;
            return 1;
        }

        if (load_no == sld[i].scn_no)
        {
            load_no = i;
            break;
        }

        i++;
    }

    mld = &sld[load_no].load_dat[ev_load_wrk.count];

    if (mld->file_type == 0 && ev_load_wrk.mode != 0)
    {
        ev_load_wrk.mode = EV_LOAD_MODE_END;
        return 1;
    }

    if (ev_load_wrk.mode == EV_LOAD_MODE_REQ)
    {
        if (mld->file_type == 13)
        {
            SceneDataLoadReq(mld->file_no, (u_int *)mld->addr);

            ev_load_wrk.mode = EV_LOAD_MODE_SCENE;

            return 0;
        }
        else if (mld->file_type == 12)
        {
            RoomLoadReq(mld->file_no);

            ev_load_wrk.mode = EV_LOAD_MODE_ROOM;

            return 0;
        }
    }
  
    switch(ev_load_wrk.mode)
    {
    case EV_LOAD_MODE_READY:
        for (i = 0; sld[load_no].del_dat[i] != 0xffff; i++) 
        {
            DelDataLoadWrk(sld[load_no].del_dat[i]);
        }

        ev_load_wrk.mode = EV_LOAD_MODE_REQ;
    break;
    case EV_LOAD_MODE_SCENE:
        if (SceneDataLoadWait() != 0)
        {
            SetDataLoadWrk(mld);

            ev_load_wrk.mode = EV_LOAD_MODE_REQ;
            ev_load_wrk.count++;
        }
    break;
    case EV_LOAD_MODE_ROOM:
        if (RoomMdlLoadWait() != 0)
        {
            ev_load_wrk.mode = EV_LOAD_MODE_REQ;
            ev_load_wrk.count++;
        }
    break;
    case EV_LOAD_MODE_REQ:
        ev_load_wrk.load_id = MissionDataLoadReq(mld);
        ev_load_wrk.mode = EV_LOAD_MODE_LOAD;
    break;
    case EV_LOAD_MODE_LOAD:
        if (IsLoadEnd(ev_load_wrk.load_id) != 0)
        {
            MissionDataLoadAfterInit(mld);
            SetDataLoadWrk(mld);

            ev_load_wrk.mode = EV_LOAD_MODE_REQ;
            ev_load_wrk.count++;
        }
    break;
    }

    return 0;
}

int GetLoadGhostInfo(u_char* load_inf)
{	
    int i;
	int count;

    count = 0;

    for (i = 0; i < 40; i++)
    {
        if (load_dat_wrk[i].file_type == 8)
        {
            switch (load_dat_wrk[i].addr)
            {
            case 0xc80000:
                load_inf[0] = 1;
                count++;
            break;
            case 0xd00000:
                load_inf[1] = 1;
                count++;
            break;
            case 0xd80000:
                load_inf[2] = 1;
                count++;
            break;
            }
        }
    }

    return count;
}

void MikuCGDisp()
{	
    int i;
	SPRT_SDAT ssd;

    for (i = 0; i < 11; i ++)
    {
        SimpleDispSprt(&msn_title_sp_bak[i], MikuPan_GetHostAddress(MISSION_TITLE_CARD_ADDRESS), i, NULL, NULL, 0x64);
    }
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
            RoomMdlLoadReq(NULL, i, ingame_wrk.msn_no, load_room, 2); // Line 917
            FloatGhostAppearTypeSet(ap_wrk.fgst_no, i, load_room); // Line 919
            area_wrk.room[i] = load_room;
            break;
        }
    }
}
