#include "common.h"
#include "typedefs.h"
#include "enums.h"
#include "ap_fgost.h"
#include "fgst_dat.h"

#include "main/glob.h"
#include "common/ul_math.h"
#include "os/eeiop/se_trans.h"
#include "os/eeiop/cdvd/eecdvd.h"
#include "ingame/plyr/unit_ctl.h"
#include "ingame/entry/entry.h"
#include "ingame/enemy/ene_ctl.h"
#include "ingame/entry/fgst_dat.h"
#include "ingame/entry/ap_dgost.h"
#include "ingame/event/ev_main.h"
#include "ingame/map/map_area.h"
#include "graphics/motion/motion.h"
#include "graphics/graph2d/effect_ene.h"

int load_mdl_addr[] = { 0xC80000, 0xD00000, 0xD80000, 0 };
int load_mot_addr[] = { 0xA30000, 0xAE0000, 0xB90000, 0 };
int load_se_addr[] = { 0x10, 0x11, 0x12, 0 };

u_char fgst_ap_no[][20] = {
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 1, 2, 5, 6, 0, 2, 3, 0, 4, 0, 0, 2, 7, 3, 1, 5, 2, 0, 1},
    {1, 0, 7, 6, 4, 1, 3, 0, 2, 5, 3, 0, 7, 1, 3, 1, 5, 7, 6, 2},
    {5, 1, 3, 10, 18, 14, 7, 18, 17, 1, 2, 6, 18, 1, 15, 8, 5, 18, 1, 6},
    {14, 17, 18, 8, 1, 5, 12, 14, 12, 15, 8, 15, 5, 17, 13, 8, 10, 11, 14, 5},
};

FGST_AP_DAT
    fg_ap_dat1[] =
        {
            {
                .room_no = 0,
                .ene_type = 0,
                .pos_num = 1,
                .cmr_no = 6,
                .rot = {180, 0, 0},
                .pos =
                    {
                        {2336, 100, 3600},
                        {0, 0, 0},
                        {0, 0, 0},
                    },
            },
            {
                .room_no = 0,
                .ene_type = 0,
                .pos_num = 1,
                .cmr_no = 2,
                .rot = {180, 0, 0},
                .pos =
                    {
                        {3853, -400, 5237},
                        {0, 0, 0},
                        {0, 0, 0},
                    },
            },
            {
                .room_no = 2,
                .ene_type = 0,
                .pos_num = 1,
                .cmr_no = 20,
                .rot = {70, 0, 0},
                .pos =
                    {
                        {176, 0, 218},
                        {0, 0, 0},
                        {0, 0, 0},
                    },
            },
            {
                .room_no = 2,
                .ene_type = 7,
                .pos_num = 1,
                .cmr_no = 23,
                .rot = {0, 0, 0},
                .pos =
                    {
                        {444, 0, 5974},
                        {0, 0, 0},
                        {0, 0, 0},
                    },
            },
            {
                .room_no = 2,
                .ene_type = 10,
                .pos_num = 1,
                .cmr_no = 27,
                .rot = {90, 0, 0},
                .pos =
                    {
                        {165, 0, 3308},
                        {0, 0, 0},
                        {0, 0, 0},
                    },
            },
            {
                .room_no = 3,
                .ene_type = 0,
                .pos_num = 1,
                .cmr_no = 39,
                .rot = {180, 0, 0},
                .pos =
                    {
                        {4807, 0, 3187},
                        {0, 0, 0},
                        {0, 0, 0},
                    },
            },
            {
                .room_no = 3,
                .ene_type = 0,
                .pos_num = 1,
                .cmr_no = 53,
                .rot = {90, 0, 0},
                .pos =
                    {
                        {280, 0, 2070},
                        {0, 0, 0},
                        {0, 0, 0},
                    },
            },
            {
                .room_no = 3,
                .ene_type = 0,
                .pos_num = 1,
                .cmr_no = 47,
                .rot = {90, 0, 0},
                .pos =
                    {
                        {2885, 0, 491},
                        {0, 0, 0},
                        {0, 0, 0},
                    },
            },
            {
                .room_no = 13,
                .ene_type = 0,
                .pos_num = 1,
                .cmr_no = 137,
                .rot = {220, 0, 0},
                .pos =
                    {
                        {4477, 0, 5118},
                        {0, 0, 0},
                        {0, 0, 0},
                    },
            },
            {
                .room_no = 13,
                .ene_type = 0,
                .pos_num = 1,
                .cmr_no = 133,
                .rot = {180, 0, 0},
                .pos =
                    {
                        {3369, 0, 4465},
                        {0, 0, 0},
                        {0, 0, 0},
                    },
            },
            {
                .room_no = 12,
                .ene_type = 0,
                .pos_num = 1,
                .cmr_no = 123,
                .rot = {0, 0, 0},
                .pos =
                    {
                        {1888, 0, 2665},
                        {0, 0, 0},
                        {0, 0, 0},
                    },
            },
            {
                .room_no = 12,
                .ene_type = 0,
                .pos_num = 1,
                .cmr_no = 119,
                .rot = {180, 0, 0},
                .pos =
                    {
                        {813, -225, 4000},
                        {0, 0, 0},
                        {0, 0, 0},
                    },
            },
            {
                .room_no = 12,
                .ene_type = 5,
                .pos_num = 1,
                .cmr_no = 121,
                .rot = {90, 0, 0},
                .pos =
                    {
                        {180, -700, 150},
                        {0, 0, 0},
                        {0, 0, 0},
                    },
            },
            {
                .room_no = 11,
                .ene_type = 0,
                .pos_num = 1,
                .cmr_no = 117,
                .rot = {0, 0, 0},
                .pos =
                    {
                        {2209, -200, 328},
                        {0, 0, 0},
                        {0, 0, 0},
                    },
            },
            {
                .room_no = 11,
                .ene_type = 0,
                .pos_num = 2,
                .cmr_no = 114,
                .rot = {180, 180, 0},
                .pos =
                    {
                        {893, 0, 3137},
                        {2000, 0, 3344},
                        {0, 0, 0},
                    },
            },
            {
                .room_no = 38,
                .ene_type = 0,
                .pos_num = 1,
                .cmr_no = 386,
                .rot = {0, 0, 0},
                .pos =
                    {
                        {260, 0, 201},
                        {0, 0, 0},
                        {0, 0, 0},
                    },
            },
            {
                .room_no = 38,
                .ene_type = 0,
                .pos_num = 1,
                .cmr_no = 384,
                .rot = {270, 0, 0},
                .pos =
                    {
                        {2500, 0, 3049},
                        {0, 0, 0},
                        {0, 0, 0},
                    },
            },
            {
                .room_no = 38,
                .ene_type = 7,
                .pos_num = 1,
                .cmr_no = 380,
                .rot = {180, 0, 0},
                .pos =
                    {
                        {-660, 0, 2800},
                        {0, 0, 0},
                        {0, 0, 0},
                    },
            },
            {
                .room_no = 41,
                .ene_type = 5,
                .pos_num = 1,
                .cmr_no = 378,
                .rot = {90, 0, 0},
                .pos =
                    {
                        {250, -400, 1707},
                        {0, 0, 0},
                        {0, 0, 0},
                    },
            },
            {
                .room_no = 41,
                .ene_type = 0,
                .pos_num = 1,
                .cmr_no = 387,
                .rot = {202, 0, 0},
                .pos =
                    {
                        {1365, 0, 3019},
                        {0, 0, 0},
                        {0, 0, 0},
                    },
            },
            {
                .room_no = 24,
                .ene_type = 0,
                .pos_num = 1,
                .cmr_no = 240,
                .rot = {0, 0, 0},
                .pos =
                    {
                        {6642, -1725, 1195},
                        {0, 0, 0},
                        {0, 0, 0},
                    },
            },
            {
                .room_no = 24,
                .ene_type = 7,
                .pos_num = 1,
                .cmr_no = 240,
                .rot = {180, 0, 0},
                .pos =
                    {
                        {5370, -1725, 2000},
                        {0, 0, 0},
                        {0, 0, 0},
                    },
            },
            {
                .room_no = 8,
                .ene_type = 0,
                .pos_num = 2,
                .cmr_no = 75,
                .rot = {270, 270, 0},
                .pos =
                    {
                        {5800, -1400, 1330},
                        {5800, -1400, 300},
                        {0, 0, 0},
                    },
            },
            {
                .room_no = 8,
                .ene_type = 0,
                .pos_num = 2,
                .cmr_no = 80,
                .rot = {0, 0, 0},
                .pos =
                    {
                        {3601, -635, 1873},
                        {5545, -1400, 2182},
                        {0, 0, 0},
                    },
            },
            {
                .room_no = 8,
                .ene_type = 7,
                .pos_num = 1,
                .cmr_no = 41,
                .rot = {90, 0, 0},
                .pos =
                    {
                        {5200, 0, 1793},
                        {0, 0, 0},
                        {0, 0, 0},
                    },
            },
            {
                .room_no = 1,
                .ene_type = 0,
                .pos_num = 1,
                .cmr_no = 10,
                .rot = {180, 0, 0},
                .pos =
                    {
                        {6831, 0, 3432},
                        {0, 0, 0},
                        {0, 0, 0},
                    },
            },
            {
                .room_no = 1,
                .ene_type = 0,
                .pos_num = 1,
                .cmr_no = 13,
                .rot = {0, 0, 0},
                .pos =
                    {
                        {5620, -50, 240},
                        {0, 0, 0},
                        {0, 0, 0},
                    },
            },
            {
                .room_no = 1,
                .ene_type = 0,
                .pos_num = 1,
                .cmr_no = 19,
                .rot = {0, 0, 0},
                .pos =
                    {
                        {1720, -50, 236},
                        {0, 0, 0},
                        {0, 0, 0},
                    },
            },
            {
                .room_no = 9,
                .ene_type = 0,
                .pos_num = 1,
                .cmr_no = 91,
                .rot = {270, 0, 0},
                .pos =
                    {
                        {3019, 0, 3167},
                        {0, 0, 0},
                        {0, 0, 0},
                    },
            },
            {
                .room_no = 9,
                .ene_type = 8,
                .pos_num = 1,
                .cmr_no = 96,
                .rot = {225, 0, 0},
                .pos =
                    {
                        {2000, 0, 160},
                        {0, 0, 0},
                        {0, 0, 0},
                    },
            },
            {
                .room_no = 9,
                .ene_type = 0,
                .pos_num = 1,
                .cmr_no = 99,
                .rot = {270, 0, 0},
                .pos =
                    {
                        {4497, -1170, 3184},
                        {0, 0, 0},
                        {0, 0, 0},
                    },
            },
            {
                .room_no = 7,
                .ene_type = 0,
                .pos_num = 1,
                .cmr_no = 63,
                .rot = {90, 0, 0},
                .pos =
                    {
                        {577, 0, 329},
                        {0, 0, 0},
                        {0, 0, 0},
                    },
            },
            {
                .room_no = 7,
                .ene_type = 0,
                .pos_num = 1,
                .cmr_no = 59,
                .rot = {180, 0, 0},
                .pos =
                    {
                        {4374, 0, 2500},
                        {0, 0, 0},
                        {0, 0, 0},
                    },
            },
            {
                .room_no = 15,
                .ene_type = 0,
                .pos_num = 1,
                .cmr_no = 145,
                .rot = {180, 0, 0},
                .pos =
                    {
                        {2324, -300, 2199},
                        {0, 0, 0},
                        {0, 0, 0},
                    },
            },
            {
                .room_no = 15,
                .ene_type = 10,
                .pos_num = 1,
                .cmr_no = 145,
                .rot = {180, 0, 0},
                .pos =
                    {
                        {1909, 0, 1563},
                        {0, 0, 0},
                        {0, 0, 0},
                    },
            },
            {
                .room_no = 16,
                .ene_type = 0,
                .pos_num = 1,
                .cmr_no = 153,
                .rot = {180, 0, 0},
                .pos =
                    {
                        {2122, -400, 3023},
                        {0, 0, 0},
                        {0, 0, 0},
                    },
            },
            {
                .room_no = 16,
                .ene_type = 0,
                .pos_num = 1,
                .cmr_no = 151,
                .rot = {180, 0, 0},
                .pos =
                    {
                        {6000, -600, 7599},
                        {0, 0, 0},
                        {0, 0, 0},
                    },
            },
            {
                .room_no = 16,
                .ene_type = 0,
                .pos_num = 1,
                .cmr_no = 157,
                .rot = {180, 0, 0},
                .pos =
                    {
                        {2321, -500, 6174},
                        {0, 0, 0},
                        {0, 0, 0},
                    },
            },
            {
                .room_no = 10,
                .ene_type = 10,
                .pos_num = 1,
                .cmr_no = 106,
                .rot = {90, 0, 0},
                .pos =
                    {
                        {1964, 0, 2309},
                        {0, 0, 0},
                        {0, 0, 0},
                    },
            },
            {
                .room_no = 10,
                .ene_type = 0,
                .pos_num = 1,
                .cmr_no = 109,
                .rot = {180, 0, 0},
                .pos =
                    {
                        {2395, -300, 3206},
                        {0, 0, 0},
                        {0, 0, 0},
                    },
            },
            {
                .room_no = 10,
                .ene_type = 5,
                .pos_num = 1,
                .cmr_no = 105,
                .rot = {90, 0, 0},
                .pos =
                    {
                        {1200, -750, 2332},
                        {0, 0, 0},
                        {0, 0, 0},
                    },
            },
            {
                .room_no = 22,
                .ene_type = 0,
                .pos_num = 1,
                .cmr_no = 228,
                .rot = {100, 0, 0},
                .pos =
                    {
                        {3500, -25, 7000},
                        {0, 0, 0},
                        {0, 0, 0},
                    },
            },
            {
                .room_no = 22,
                .ene_type = 0,
                .pos_num = 1,
                .cmr_no = 223,
                .rot = {0, 0, 0},
                .pos =
                    {
                        {2650, 0, 191},
                        {0, 0, 0},
                        {0, 0, 0},
                    },
            },
            {
                .room_no = 17,
                .ene_type = 0,
                .pos_num = 2,
                .cmr_no = 181,
                .rot = {0, 0, 0},
                .pos =
                    {
                        {1331, 0, 1127},
                        {251, -1400, 1221},
                        {0, 0, 0},
                    },
            },
            {
                .room_no = 17,
                .ene_type = 0,
                .pos_num = 1,
                .cmr_no = 173,
                .rot = {270, 0, 0},
                .pos =
                    {
                        {1903, -1400, 3157},
                        {0, 0, 0},
                        {0, 0, 0},
                    },
            },
            {
                .room_no = 17,
                .ene_type = 0,
                .pos_num = 2,
                .cmr_no = 178,
                .rot = {60, 0, 0},
                .pos =
                    {
                        {269, 0, 4165},
                        {2001, 0, 3790},
                        {0, 0, 0},
                    },
            },
            {
                .room_no = 23,
                .ene_type = 0,
                .pos_num = 1,
                .cmr_no = 235,
                .rot = {100, 0, 0},
                .pos =
                    {
                        {176, 800, 5150},
                        {0, 0, 0},
                        {0, 0, 0},
                    },
            },
            {
                .room_no = 23,
                .ene_type = 0,
                .pos_num = 1,
                .cmr_no = 232,
                .rot = {100, 0, 0},
                .pos =
                    {
                        {2326, 1000, 2596},
                        {0, 0, 0},
                        {0, 0, 0},
                    },
            },
            {
                .room_no = 21,
                .ene_type = 0,
                .pos_num = 1,
                .cmr_no = 210,
                .rot = {90, 0, 0},
                .pos =
                    {
                        {550, 0, 5000},
                        {0, 0, 0},
                        {0, 0, 0},
                    },
            },
            {
                .room_no = 21,
                .ene_type = 0,
                .pos_num = 1,
                .cmr_no = 219,
                .rot = {90, 0, 0},
                .pos =
                    {
                        {2919, 0, 5450},
                        {0, 0, 0},
                        {0, 0, 0},
                    },
            },
            {
                .room_no = 25,
                .ene_type = 0,
                .pos_num = 1,
                .cmr_no = 253,
                .rot = {270, 0, 0},
                .pos =
                    {
                        {4214, -1300, 3600},
                        {0, 0, 0},
                        {0, 0, 0},
                    },
            },
            {
                .room_no = 25,
                .ene_type = 0,
                .pos_num = 2,
                .cmr_no = 256,
                .rot = {180, 180, 0},
                .pos =
                    {
                        {5772, -2025, 9169},
                        {4499, -2025, 9170},
                        {0, 0, 0},
                    },
            },
            {
                .room_no = 0,
                .ene_type = 11,
                .pos_num = 0,
                .cmr_no = 0,
                .rot = {0, 0, 0},
                .pos =
                    {
                        {0, 0, 0},
                        {0, 0, 0},
                        {0, 0, 0},
                    },
            },
};

FGST_AP_DAT
    fg_ap_dat2[] =
        {
            {
                .room_no = 0,
                .ene_type = 0,
                .pos_num = 1,
                .cmr_no = 7,
                .rot = {270, 0, 0},
                .pos =
                    {
                        {3668, 0, 300},
                        {0, 0, 0},
                        {0, 0, 0},
                    },
            },
            {
                .room_no = 0,
                .ene_type = 0,
                .pos_num = 1,
                .cmr_no = 6,
                .rot = {180, 0, 0},
                .pos =
                    {
                        {2168, -400, 3459},
                        {0, 0, 0},
                        {0, 0, 0},
                    },
            },
            {
                .room_no = 0,
                .ene_type = 0,
                .pos_num = 1,
                .cmr_no = 2,
                .rot = {180, 0, 0},
                .pos =
                    {
                        {3853, -400, 5237},
                        {0, 0, 0},
                        {0, 0, 0},
                    },
            },
            {
                .room_no = 2,
                .ene_type = 8,
                .pos_num = 1,
                .cmr_no = 23,
                .rot = {180, 0, 0},
                .pos =
                    {
                        {400, 0, 5763},
                        {0, 0, 0},
                        {0, 0, 0},
                    },
            },
            {
                .room_no = 2,
                .ene_type = 0,
                .pos_num = 1,
                .cmr_no = 24,
                .rot = {130, 0, 0},
                .pos =
                    {
                        {-745, 0, 6824},
                        {0, 0, 0},
                        {0, 0, 0},
                    },
            },
            {
                .room_no = 2,
                .ene_type = 0,
                .pos_num = 1,
                .cmr_no = 458,
                .rot = {180, 0, 0},
                .pos =
                    {
                        {400, 0, 4952},
                        {0, 0, 0},
                        {0, 0, 0},
                    },
            },
            {
                .room_no = 3,
                .ene_type = 0,
                .pos_num = 1,
                .cmr_no = 38,
                .rot = {180, 0, 0},
                .pos =
                    {
                        {4807, 0, 3187},
                        {0, 0, 0},
                        {0, 0, 0},
                    },
            },
            {
                .room_no = 3,
                .ene_type = 0,
                .pos_num = 1,
                .cmr_no = 53,
                .rot = {90, 0, 0},
                .pos =
                    {
                        {280, 0, 2070},
                        {0, 0, 0},
                        {0, 0, 0},
                    },
            },
            {
                .room_no = 3,
                .ene_type = 0,
                .pos_num = 1,
                .cmr_no = 44,
                .rot = {180, 0, 0},
                .pos =
                    {
                        {2983, -1400, 4971},
                        {0, 0, 0},
                        {0, 0, 0},
                    },
            },
            {
                .room_no = 13,
                .ene_type = 0,
                .pos_num = 1,
                .cmr_no = 137,
                .rot = {180, 0, 0},
                .pos =
                    {
                        {4483, 0, 5104},
                        {0, 0, 0},
                        {0, 0, 0},
                    },
            },
            {
                .room_no = 13,
                .ene_type = 0,
                .pos_num = 1,
                .cmr_no = 136,
                .rot = {180, 0, 0},
                .pos =
                    {
                        {3369, 0, 4465},
                        {0, 0, 0},
                        {0, 0, 0},
                    },
            },
            {
                .room_no = 13,
                .ene_type = 6,
                .pos_num = 1,
                .cmr_no = 136,
                .rot = {0, 180, 0},
                .pos =
                    {
                        {3515, 0, 2268},
                        {3392, 0, 3923},
                        {0, 0, 0},
                    },
            },
            {
                .room_no = 12,
                .ene_type = 6,
                .pos_num = 1,
                .cmr_no = 120,
                .rot = {270, 0, 0},
                .pos =
                    {
                        {2503, 0, 229},
                        {0, 0, 0},
                        {0, 0, 0},
                    },
            },
            {
                .room_no = 12,
                .ene_type = 0,
                .pos_num = 1,
                .cmr_no = 127,
                .rot = {180, 0, 0},
                .pos =
                    {
                        {1092, -80, 3695},
                        {0, 0, 0},
                        {0, 0, 0},
                    },
            },
            {
                .room_no = 12,
                .ene_type = 5,
                .pos_num = 1,
                .cmr_no = 121,
                .rot = {90, 0, 0},
                .pos =
                    {
                        {180, 700, 150},
                        {0, 0, 0},
                        {0, 0, 0},
                    },
            },
            {
                .room_no = 11,
                .ene_type = 0,
                .pos_num = 1,
                .cmr_no = 110,
                .rot = {159, 0, 0},
                .pos =
                    {
                        {1728, 0, 3848},
                        {0, 0, 0},
                        {0, 0, 0},
                    },
            },
            {
                .room_no = 11,
                .ene_type = 0,
                .pos_num = 1,
                .cmr_no = 114,
                .rot = {214, 0, 0},
                .pos =
                    {
                        {1955, 0, 2901},
                        {0, 0, 0},
                        {0, 0, 0},
                    },
            },
            {
                .room_no = 38,
                .ene_type = 0,
                .pos_num = 2,
                .cmr_no = 376,
                .rot = {0, 0, 0},
                .pos =
                    {
                        {260, 0, 201},
                        {-272, 0, 1701},
                        {0, 0, 0},
                    },
            },
            {
                .room_no = 41,
                .ene_type = 5,
                .pos_num = 1,
                .cmr_no = 377,
                .rot = {90, 0, 0},
                .pos =
                    {
                        {250, -400, 1707},
                        {0, 0, 0},
                        {0, 0, 0},
                    },
            },
            {
                .room_no = 38,
                .ene_type = 0,
                .pos_num = 1,
                .cmr_no = 381,
                .rot = {270, 0, 0},
                .pos =
                    {
                        {-225, -50, 2724},
                        {0, 0, 0},
                        {0, 0, 0},
                    },
            },
            {
                .room_no = 24,
                .ene_type = 0,
                .pos_num = 1,
                .cmr_no = 242,
                .rot = {0, 0, 0},
                .pos =
                    {
                        {7200, -1725, 1900},
                        {0, 0, 0},
                        {0, 0, 0},
                    },
            },
            {
                .room_no = 8,
                .ene_type = 0,
                .pos_num = 2,
                .cmr_no = 75,
                .rot = {270, 270, 0},
                .pos =
                    {
                        {5800, -1400, 1330},
                        {5800, -1400, 300},
                        {0, 0, 0},
                    },
            },
            {
                .room_no = 8,
                .ene_type = 0,
                .pos_num = 2,
                .cmr_no = 82,
                .rot = {320, 180, 0},
                .pos =
                    {
                        {6628, 0, 300},
                        {3729, -635, 1925},
                        {0, 0, 0},
                    },
            },
            {
                .room_no = 8,
                .ene_type = 0,
                .pos_num = 1,
                .cmr_no = 41,
                .rot = {90, 0, 0},
                .pos =
                    {
                        {5200, 0, 1793},
                        {0, 0, 0},
                        {0, 0, 0},
                    },
            },
            {
                .room_no = 1,
                .ene_type = 0,
                .pos_num = 1,
                .cmr_no = 11,
                .rot = {0, 0, 0},
                .pos =
                    {
                        {6831, -50, 383},
                        {0, 0, 0},
                        {0, 0, 0},
                    },
            },
            {
                .room_no = 1,
                .ene_type = 0,
                .pos_num = 2,
                .cmr_no = 9,
                .rot = {180, 180, 0},
                .pos =
                    {
                        {176, 0, 3796},
                        {1849, 0, 3821},
                        {0, 0, 0},
                    },
            },
            {
                .room_no = 1,
                .ene_type = 0,
                .pos_num = 1,
                .cmr_no = 14,
                .rot = {180, 0, 0},
                .pos =
                    {
                        {5625, 0, 3676},
                        {0, 0, 0},
                        {0, 0, 0},
                    },
            },
            {
                .room_no = 9,
                .ene_type = 0,
                .pos_num = 2,
                .cmr_no = 99,
                .rot = {270, 90, 0},
                .pos =
                    {
                        {4504, -1170, 3174},
                        {498, 0, 2968},
                        {0, 0, 0},
                    },
            },
            {
                .room_no = 9,
                .ene_type = 0,
                .pos_num = 1,
                .cmr_no = 96,
                .rot = {225, 0, 0},
                .pos =
                    {
                        {2000, 0, 160},
                        {0, 0, 0},
                        {0, 0, 0},
                    },
            },
            {
                .room_no = 7,
                .ene_type = 0,
                .pos_num = 2,
                .cmr_no = 65,
                .rot = {270, 0, 0},
                .pos =
                    {
                        {4366, 0, 208},
                        {0, 0, 0},
                        {0, 0, 0},
                    },
            },
            {
                .room_no = 7,
                .ene_type = 0,
                .pos_num = 1,
                .cmr_no = 61,
                .rot = {180, 180, 0},
                .pos =
                    {
                        {271, 0, 2392},
                        {1300, -700, 2549},
                        {0, 0, 0},
                    },
            },
            {
                .room_no = 15,
                .ene_type = 0,
                .pos_num = 1,
                .cmr_no = 455,
                .rot = {270, 0, 0},
                .pos =
                    {
                        {2860, 0, 603},
                        {0, 0, 0},
                        {0, 0, 0},
                    },
            },
            {
                .room_no = 15,
                .ene_type = 0,
                .pos_num = 1,
                .cmr_no = 145,
                .rot = {180, 0, 0},
                .pos =
                    {
                        {2324, -300, 2199},
                        {0, 0, 0},
                        {0, 0, 0},
                    },
            },
            {
                .room_no = 16,
                .ene_type = 10,
                .pos_num = 1,
                .cmr_no = 165,
                .rot = {0, 0, 0},
                .pos =
                    {
                        {4326, -400, 177},
                        {0, 0, 0},
                        {0, 0, 0},
                    },
            },
            {
                .room_no = 16,
                .ene_type = 0,
                .pos_num = 2,
                .cmr_no = 151,
                .rot = {180, 180, 0},
                .pos =
                    {
                        {4500, 0, 8281},
                        {5045, 0, 7592},
                        {0, 0, 0},
                    },
            },
            {
                .room_no = 16,
                .ene_type = 0,
                .pos_num = 1,
                .cmr_no = 157,
                .rot = {180, 0, 0},
                .pos =
                    {
                        {2321, -500, 6174},
                        {0, 0, 0},
                        {0, 0, 0},
                    },
            },
            {
                .room_no = 22,
                .ene_type = 0,
                .pos_num = 1,
                .cmr_no = 229,
                .rot = {90, 0, 0},
                .pos =
                    {
                        {1800, 800, 8700},
                        {0, 0, 0},
                        {0, 0, 0},
                    },
            },
            {
                .room_no = 22,
                .ene_type = 0,
                .pos_num = 1,
                .cmr_no = 223,
                .rot = {0, 0, 0},
                .pos =
                    {
                        {2650, 0, 191},
                        {0, 0, 0},
                        {0, 0, 0},
                    },
            },
            {
                .room_no = 29,
                .ene_type = 0,
                .pos_num = 2,
                .cmr_no = 342,
                .rot = {90, 270, 0},
                .pos =
                    {
                        {1716, 1500, 5400},
                        {4467, 1500, 5327},
                        {0, 0, 0},
                    },
            },
            {
                .room_no = 29,
                .ene_type = 0,
                .pos_num = 1,
                .cmr_no = 356,
                .rot = {90, 0, 0},
                .pos =
                    {
                        {1329, 2000, 4697},
                        {0, 0, 0},
                        {0, 0, 0},
                    },
            },
            {
                .room_no = 29,
                .ene_type = 7,
                .pos_num = 1,
                .cmr_no = 340,
                .rot = {180, 0, 0},
                .pos =
                    {
                        {3220, 1000, 3171},
                        {0, 0, 0},
                        {0, 0, 0},
                    },
            },
            {
                .room_no = 40,
                .ene_type = 0,
                .pos_num = 1,
                .cmr_no = 246,
                .rot = {160, 0, 0},
                .pos =
                    {
                        {500, 0, 3224},
                        {0, 0, 0},
                        {0, 0, 0},
                    },
            },
            {
                .room_no = 40,
                .ene_type = 0,
                .pos_num = 1,
                .cmr_no = 245,
                .rot = {180, 0, 0},
                .pos =
                    {
                        {2415, 0, 3199},
                        {0, 0, 0},
                        {0, 0, 0},
                    },
            },
            {
                .room_no = 17,
                .ene_type = 0,
                .pos_num = 2,
                .cmr_no = 172,
                .rot = {180, 270, 0},
                .pos =
                    {
                        {1290, -500, 2644},
                        {2000, 0, 1905},
                        {0, 0, 0},
                    },
            },
            {
                .room_no = 17,
                .ene_type = 0,
                .pos_num = 1,
                .cmr_no = 173,
                .rot = {180, 0, 0},
                .pos =
                    {
                        {1018, -1700, 3681},
                        {0, 0, 0},
                        {0, 0, 0},
                    },
            },
            {
                .room_no = 17,
                .ene_type = 0,
                .pos_num = 1,
                .cmr_no = 179,
                .rot = {180, 0, 0},
                .pos =
                    {
                        {704, 0, 7059},
                        {0, 0, 0},
                        {0, 0, 0},
                    },
            },
            {
                .room_no = 23,
                .ene_type = 0,
                .pos_num = 1,
                .cmr_no = 230,
                .rot = {90, 0, 0},
                .pos =
                    {
                        {3095, 0, 424},
                        {0, 0, 0},
                        {0, 0, 0},
                    },
            },
            {
                .room_no = 23,
                .ene_type = 0,
                .pos_num = 1,
                .cmr_no = 235,
                .rot = {100, 0, 0},
                .pos =
                    {
                        {176, 50, 5150},
                        {0, 0, 0},
                        {0, 0, 0},
                    },
            },
            {
                .room_no = 21,
                .ene_type = 0,
                .pos_num = 2,
                .cmr_no = 212,
                .rot = {0, 180, 0},
                .pos =
                    {
                        {2151, 0, 1441},
                        {1615, 0, 3322},
                        {0, 0, 0},
                    },
            },
            {
                .room_no = 21,
                .ene_type = 0,
                .pos_num = 1,
                .cmr_no = 219,
                .rot = {180, 0, 0},
                .pos =
                    {
                        {2657, 0, 6700},
                        {0, 0, 0},
                        {0, 0, 0},
                    },
            },
            {
                .room_no = 25,
                .ene_type = 0,
                .pos_num = 2,
                .cmr_no = 253,
                .rot = {270, 90, 0},
                .pos =
                    {
                        {4214, -1300, 3600},
                        {4200, -800, 3837},
                        {0, 0, 0},
                    },
            },
            {
                .room_no = 25,
                .ene_type = 0,
                .pos_num = 1,
                .cmr_no = 255,
                .rot = {270, 0, 0},
                .pos =
                    {
                        {6000, -1500, 7000},
                        {0, 0, 0},
                        {0, 0, 0},
                    },
            },
            {
                .room_no = 25,
                .ene_type = 0,
                .pos_num = 2,
                .cmr_no = 256,
                .rot = {180, 180, 0},
                .pos =
                    {
                        {5772, -2025, 9169},
                        {4499, -2025, 9170},
                        {0, 0, 0},
                    },
            },
            {
                .room_no = 4,
                .ene_type = 0,
                .pos_num = 1,
                .cmr_no = 287,
                .rot = {270, 0, 0},
                .pos =
                    {
                        {6607, 0, 2010},
                        {0, 0, 0},
                        {0, 0, 0},
                    },
            },
            {
                .room_no = 4,
                .ene_type = 0,
                .pos_num = 1,
                .cmr_no = 285,
                .rot = {180, 0, 0},
                .pos =
                    {
                        {1463, 0, 5659},
                        {0, 0, 0},
                        {0, 0, 0},
                    },
            },
            {
                .room_no = 36,
                .ene_type = 7,
                .pos_num = 1,
                .cmr_no = 312,
                .rot = {180, 0, 0},
                .pos =
                    {
                        {1074, -375, 3064},
                        {0, 0, 0},
                        {0, 0, 0},
                    },
            },
            {
                .room_no = 36,
                .ene_type = 0,
                .pos_num = 1,
                .cmr_no = 311,
                .rot = {350, 0, 0},
                .pos =
                    {
                        {1845, -100, 152},
                        {0, 0, 0},
                        {0, 0, 0},
                    },
            },
            {
                .room_no = 5,
                .ene_type = 0,
                .pos_num = 1,
                .cmr_no = 296,
                .rot = {90, 0, 0},
                .pos =
                    {
                        {190, 0, 3050},
                        {0, 0, 0},
                        {0, 0, 0},
                    },
            },
            {
                .room_no = 6,
                .ene_type = 0,
                .pos_num = 2,
                .cmr_no = 373,
                .rot = {357, 90, 0},
                .pos =
                    {
                        {6153, 0, 1653},
                        {4044, 400, 1628},
                        {0, 0, 0},
                    },
            },
            {
                .room_no = 6,
                .ene_type = 0,
                .pos_num = 1,
                .cmr_no = 359,
                .rot = {0, 0, 0},
                .pos =
                    {
                        {1500, 20, 2200},
                        {0, 0, 0},
                        {0, 0, 0},
                    },
            },
            {
                .room_no = 6,
                .ene_type = 8,
                .pos_num = 2,
                .cmr_no = 365,
                .rot = {90, 0, 0},
                .pos =
                    {
                        {1230, 0, 5035},
                        {6153, 0, 1653},
                        {0, 0, 0},
                    },
            },
            {
                .room_no = 14,
                .ene_type = 0,
                .pos_num = 1,
                .cmr_no = 321,
                .rot = {125, 0, 0},
                .pos =
                    {
                        {176, 400, 4100},
                        {0, 0, 0},
                        {0, 0, 0},
                    },
            },
            {
                .room_no = 14,
                .ene_type = 0,
                .pos_num = 1,
                .cmr_no = 327,
                .rot = {180, 0, 0},
                .pos =
                    {
                        {3051, 400, 4100},
                        {0, 0, 0},
                        {0, 0, 0},
                    },
            },
            {
                .room_no = 27,
                .ene_type = 0,
                .pos_num = 2,
                .cmr_no = 273,
                .rot = {140, 310, 0},
                .pos =
                    {
                        {-1990, 0, 3181},
                        {-5, 0, 1314},
                        {0, 0, 0},
                    },
            },
            {
                .room_no = 27,
                .ene_type = 0,
                .pos_num = 1,
                .cmr_no = 270,
                .rot = {150, 0, 0},
                .pos =
                    {
                        {-5173, 0, 8353},
                        {0, 0, 0},
                        {0, 0, 0},
                    },
            },
            {
                .room_no = 0,
                .ene_type = 11,
                .pos_num = 0,
                .cmr_no = 0,
                .rot = {0, 0, 0},
                .pos =
                    {
                        {0, 0, 0},
                        {0, 0, 0},
                        {0, 0, 0},
                    },
            },
};

FGST_AP_DAT
    fg_ap_dat3[] =
        {
            {
                .room_no = 26,
                .ene_type = 9,
                .pos_num = 1,
                .cmr_no = 263,
                .rot = {312, 0, 0},
                .pos =
                    {
                        {791, 0, 1548},
                        {0, 0, 0},
                        {0, 0, 0},
                    },
            },
            {
                .room_no = 23,
                .ene_type = 0,
                .pos_num = 1,
                .cmr_no = 230,
                .rot = {90, 0, 0},
                .pos =
                    {
                        {3095, 0, 424},
                        {0, 0, 0},
                        {0, 0, 0},
                    },
            },
            {
                .room_no = 23,
                .ene_type = 0,
                .pos_num = 1,
                .cmr_no = 235,
                .rot = {100, 0, 0},
                .pos =
                    {
                        {176, 1000, 5150},
                        {0, 0, 0},
                        {0, 0, 0},
                    },
            },
            {
                .room_no = 21,
                .ene_type = 0,
                .pos_num = 2,
                .cmr_no = 212,
                .rot = {0, 180, 0},
                .pos =
                    {
                        {2151, 0, 1441},
                        {1615, 0, 3322},
                        {0, 0, 0},
                    },
            },
            {
                .room_no = 21,
                .ene_type = 8,
                .pos_num = 1,
                .cmr_no = 218,
                .rot = {90, 0, 0},
                .pos =
                    {
                        {4000, 0, 5000},
                        {0, 0, 0},
                        {0, 0, 0},
                    },
            },
            {
                .room_no = 25,
                .ene_type = 0,
                .pos_num = 2,
                .cmr_no = 253,
                .rot = {270, 90, 0},
                .pos =
                    {
                        {4214, -1300, 3600},
                        {4200, -800, 3837},
                        {0, 0, 0},
                    },
            },
            {
                .room_no = 25,
                .ene_type = 0,
                .pos_num = 1,
                .cmr_no = 255,
                .rot = {270, 0, 0},
                .pos =
                    {
                        {6000, -1500, 7000},
                        {0, 0, 0},
                        {0, 0, 0},
                    },
            },
            {
                .room_no = 25,
                .ene_type = 0,
                .pos_num = 2,
                .cmr_no = 256,
                .rot = {180, 180, 0},
                .pos =
                    {
                        {5772, -2025, 9169},
                        {4499, -2025, 9170},
                        {0, 0, 0},
                    },
            },
            {
                .room_no = 40,
                .ene_type = 0,
                .pos_num = 1,
                .cmr_no = 246,
                .rot = {180, 0, 0},
                .pos =
                    {
                        {300, -800, 1826},
                        {0, 0, 0},
                        {0, 0, 0},
                    },
            },
            {
                .room_no = 40,
                .ene_type = 8,
                .pos_num = 1,
                .cmr_no = 244,
                .rot = {0, 0, 0},
                .pos =
                    {
                        {1410, -50, 400},
                        {0, 0, 0},
                        {0, 0, 0},
                    },
            },
            {
                .room_no = 40,
                .ene_type = 5,
                .pos_num = 1,
                .cmr_no = 245,
                .rot = {0, 0, 0},
                .pos =
                    {
                        {2480, -700, 400},
                        {0, 0, 0},
                        {0, 0, 0},
                    },
            },
            {
                .room_no = 3,
                .ene_type = 7,
                .pos_num = 1,
                .cmr_no = 32,
                .rot = {0, 0, 0},
                .pos =
                    {
                        {2040, 0, 1100},
                        {0, 0, 0},
                        {0, 0, 0},
                    },
            },
            {
                .room_no = 3,
                .ene_type = 0,
                .pos_num = 1,
                .cmr_no = 38,
                .rot = {180, 0, 0},
                .pos =
                    {
                        {4807, 0, 3187},
                        {0, 0, 0},
                        {0, 0, 0},
                    },
            },
            {
                .room_no = 3,
                .ene_type = 0,
                .pos_num = 1,
                .cmr_no = 35,
                .rot = {90, 0, 0},
                .pos =
                    {
                        {1626, 0, 487},
                        {0, 0, 0},
                        {0, 0, 0},
                    },
            },
            {
                .room_no = 11,
                .ene_type = 5,
                .pos_num = 1,
                .cmr_no = 117,
                .rot = {0, 0, 0},
                .pos =
                    {
                        {2160, 0, 316},
                        {0, 0, 0},
                        {0, 0, 0},
                    },
            },
            {
                .room_no = 11,
                .ene_type = 0,
                .pos_num = 1,
                .cmr_no = 114,
                .rot = {214, 0, 0},
                .pos =
                    {
                        {1955, 0, 2901},
                        {0, 0, 0},
                        {0, 0, 0},
                    },
            },
            {
                .room_no = 12,
                .ene_type = 0,
                .pos_num = 1,
                .cmr_no = 123,
                .rot = {0, 0, 0},
                .pos =
                    {
                        {1969, 0, 2350},
                        {0, 0, 0},
                        {0, 0, 0},
                    },
            },
            {
                .room_no = 12,
                .ene_type = 5,
                .pos_num = 1,
                .cmr_no = 121,
                .rot = {90, 0, 0},
                .pos =
                    {
                        {180, 700, 150},
                        {0, 0, 0},
                        {0, 0, 0},
                    },
            },
            {
                .room_no = 13,
                .ene_type = 5,
                .pos_num = 1,
                .cmr_no = 137,
                .rot = {180, 0, 0},
                .pos =
                    {
                        {6344, 430, 5158},
                        {0, 0, 0},
                        {0, 0, 0},
                    },
            },
            {
                .room_no = 13,
                .ene_type = 0,
                .pos_num = 2,
                .cmr_no = 131,
                .rot = {270, 270, 0},
                .pos =
                    {
                        {4896, 0, 4084},
                        {3749, 0, 3860},
                        {0, 0, 0},
                    },
            },
            {
                .room_no = 14,
                .ene_type = 0,
                .pos_num = 1,
                .cmr_no = 324,
                .rot = {270, 0, 0},
                .pos =
                    {
                        {176, 400, 4100},
                        {0, 0, 0},
                        {0, 0, 0},
                    },
            },
            {
                .room_no = 14,
                .ene_type = 8,
                .pos_num = 1,
                .cmr_no = 327,
                .rot = {180, 0, 0},
                .pos =
                    {
                        {2300, -400, 4100},
                        {0, 0, 0},
                        {0, 0, 0},
                    },
            },
            {
                .room_no = 6,
                .ene_type = 0,
                .pos_num = 1,
                .cmr_no = 359,
                .rot = {0, 0, 0},
                .pos =
                    {
                        {1500, 20, 2700},
                        {0, 0, 0},
                        {0, 0, 0},
                    },
            },
            {
                .room_no = 6,
                .ene_type = 9,
                .pos_num = 1,
                .cmr_no = 370,
                .rot = {90, 0, 0},
                .pos =
                    {
                        {2904, -400, 2500},
                        {0, 0, 0},
                        {0, 0, 0},
                    },
            },
            {
                .room_no = 6,
                .ene_type = 0,
                .pos_num = 2,
                .cmr_no = 373,
                .rot = {0, 270, 0},
                .pos =
                    {
                        {4636, 0, 397},
                        {6051, 0, 1500},
                        {0, 0, 0},
                    },
            },
            {
                .room_no = 36,
                .ene_type = 9,
                .pos_num = 1,
                .cmr_no = 312,
                .rot = {180, 0, 0},
                .pos =
                    {
                        {1000, -370, 3124},
                        {0, 0, 0},
                        {0, 0, 0},
                    },
            },
            {
                .room_no = 36,
                .ene_type = 0,
                .pos_num = 1,
                .cmr_no = 313,
                .rot = {141, 0, 0},
                .pos =
                    {
                        {176, -450, 3100},
                        {0, 0, 0},
                        {0, 0, 0},
                    },
            },
            {
                .room_no = 36,
                .ene_type = 0,
                .pos_num = 1,
                .cmr_no = 312,
                .rot = {210, 0, 0},
                .pos =
                    {
                        {1900, -375, 3166},
                        {0, 0, 0},
                        {0, 0, 0},
                    },
            },
            {
                .room_no = 5,
                .ene_type = 0,
                .pos_num = 2,
                .cmr_no = 296,
                .rot = {90, 0, 0},
                .pos =
                    {
                        {296, 0, 9090},
                        {0, 0, 0},
                        {0, 0, 0},
                    },
            },
            {
                .room_no = 5,
                .ene_type = 0,
                .pos_num = 1,
                .cmr_no = 295,
                .rot = {90, 0, 0},
                .pos =
                    {
                        {193, 0, 426},
                        {0, 0, 0},
                        {0, 0, 0},
                    },
            },
            {
                .room_no = 4,
                .ene_type = 0,
                .pos_num = 1,
                .cmr_no = 287,
                .rot = {270, 0, 0},
                .pos =
                    {
                        {6607, 0, 2010},
                        {0, 0, 0},
                        {0, 0, 0},
                    },
            },
            {
                .room_no = 4,
                .ene_type = 6,
                .pos_num = 1,
                .cmr_no = 285,
                .rot = {180, 0, 0},
                .pos =
                    {
                        {1539, 0, 4687},
                        {0, 0, 0},
                        {0, 0, 0},
                    },
            },
            {
                .room_no = 0,
                .ene_type = 0,
                .pos_num = 1,
                .cmr_no = 49,
                .rot = {180, 0, 0},
                .pos =
                    {
                        {2144, -400, 5236},
                        {0, 0, 0},
                        {0, 0, 0},
                    },
            },
            {
                .room_no = 0,
                .ene_type = 0,
                .pos_num = 1,
                .cmr_no = 6,
                .rot = {180, 0, 0},
                .pos =
                    {
                        {2336, 0, 3600},
                        {0, 0, 0},
                        {0, 0, 0},
                    },
            },
            {
                .room_no = 2,
                .ene_type = 0,
                .pos_num = 1,
                .cmr_no = 20,
                .rot = {31, 0, 0},
                .pos =
                    {
                        {176, 0, 205},
                        {0, 0, 0},
                        {0, 0, 0},
                    },
            },
            {
                .room_no = 2,
                .ene_type = 6,
                .pos_num = 1,
                .cmr_no = 21,
                .rot = {180, 0, 0},
                .pos =
                    {
                        {545, 0, 2435},
                        {0, 0, 0},
                        {0, 0, 0},
                    },
            },
            {
                .room_no = 2,
                .ene_type = 9,
                .pos_num = 1,
                .cmr_no = 23,
                .rot = {0, 0, 0},
                .pos =
                    {
                        {280, -400, 6096},
                        {0, 0, 0},
                        {0, 0, 0},
                    },
            },
            {
                .room_no = 1,
                .ene_type = 6,
                .pos_num = 1,
                .cmr_no = 12,
                .rot = {0, 0, 0},
                .pos =
                    {
                        {4123, 0, 2039},
                        {0, 0, 0},
                        {0, 0, 0},
                    },
            },
            {
                .room_no = 1,
                .ene_type = 0,
                .pos_num = 1,
                .cmr_no = 14,
                .rot = {180, 0, 0},
                .pos =
                    {
                        {5638, 0, 3857},
                        {0, 0, 0},
                        {0, 0, 0},
                    },
            },
            {
                .room_no = 1,
                .ene_type = 0,
                .pos_num = 1,
                .cmr_no = 12,
                .rot = {270, 0, 0},
                .pos =
                    {
                        {3358, 0, 2543},
                        {0, 0, 0},
                        {0, 0, 0},
                    },
            },
            {
                .room_no = 9,
                .ene_type = 0,
                .pos_num = 2,
                .cmr_no = 99,
                .rot = {270, 0, 0},
                .pos =
                    {
                        {4504, -1170, 3174},
                        {0, 0, 0},
                        {0, 0, 0},
                    },
            },
            {
                .room_no = 9,
                .ene_type = 0,
                .pos_num = 1,
                .cmr_no = 92,
                .rot = {180, 0, 0},
                .pos =
                    {
                        {670, -1150, 3276},
                        {0, 0, 0},
                        {0, 0, 0},
                    },
            },
            {
                .room_no = 9,
                .ene_type = 5,
                .pos_num = 1,
                .cmr_no = 92,
                .rot = {180, 0, 0},
                .pos =
                    {
                        {800, 750, 3150},
                        {0, 0, 0},
                        {0, 0, 0},
                    },
            },
            {
                .room_no = 35,
                .ene_type = 0,
                .pos_num = 1,
                .cmr_no = 336,
                .rot = {0, 0, 0},
                .pos =
                    {
                        {2592, 0, 300},
                        {0, 0, 0},
                        {0, 0, 0},
                    },
            },
            {
                .room_no = 35,
                .ene_type = 0,
                .pos_num = 1,
                .cmr_no = 337,
                .rot = {180, 0, 0},
                .pos =
                    {
                        {2649, -100, 2221},
                        {0, 0, 0},
                        {0, 0, 0},
                    },
            },
            {
                .room_no = 8,
                .ene_type = 0,
                .pos_num = 1,
                .cmr_no = 41,
                .rot = {90, 0, 0},
                .pos =
                    {
                        {5200, 0, 1793},
                        {0, 0, 0},
                        {0, 0, 0},
                    },
            },
            {
                .room_no = 8,
                .ene_type = 0,
                .pos_num = 2,
                .cmr_no = 89,
                .rot = {90, 90, 0},
                .pos =
                    {
                        {3745, -760, 2570},
                        {6065, -1400, 1924},
                        {0, 0, 0},
                    },
            },
            {
                .room_no = 8,
                .ene_type = 0,
                .pos_num = 1,
                .cmr_no = 85,
                .rot = {320, 0, 0},
                .pos =
                    {
                        {9260, -1400, 2301},
                        {0, 0, 0},
                        {0, 0, 0},
                    },
            },
            {
                .room_no = 38,
                .ene_type = 5,
                .pos_num = 1,
                .cmr_no = 382,
                .rot = {180, 0, 0},
                .pos =
                    {
                        {-743, -250, 3703},
                        {0, 0, 0},
                        {0, 0, 0},
                    },
            },
            {
                .room_no = 24,
                .ene_type = 7,
                .pos_num = 1,
                .cmr_no = 240,
                .rot = {180, 0, 0},
                .pos =
                    {
                        {5500, -1725, 3000},
                        {0, 0, 0},
                        {0, 0, 0},
                    },
            },
            {
                .room_no = 38,
                .ene_type = 0,
                .pos_num = 1,
                .cmr_no = 385,
                .rot = {180, 0, 0},
                .pos =
                    {
                        {1273, 0, 2800},
                        {-272, 0, 1701},
                        {0, 0, 0},
                    },
            },
            {
                .room_no = 38,
                .ene_type = 5,
                .pos_num = 1,
                .cmr_no = 386,
                .rot = {180, 0, 0},
                .pos =
                    {
                        {1113, 200, 904},
                        {0, 0, 1701},
                        {0, 0, 0},
                    },
            },
            {
                .room_no = 41,
                .ene_type = 5,
                .pos_num = 1,
                .cmr_no = 377,
                .rot = {90, 0, 0},
                .pos =
                    {
                        {250, -400, 1707},
                        {0, 0, 0},
                        {0, 0, 0},
                    },
            },
            {
                .room_no = 17,
                .ene_type = 0,
                .pos_num = 1,
                .cmr_no = 183,
                .rot = {0, 0, 0},
                .pos =
                    {
                        {961, -1400, 1142},
                        {0, 0, 0},
                        {0, 0, 0},
                    },
            },
            {
                .room_no = 17,
                .ene_type = 6,
                .pos_num = 1,
                .cmr_no = 173,
                .rot = {180, 0, 0},
                .pos =
                    {
                        {934, -700, 3627},
                        {0, 0, 0},
                        {0, 0, 0},
                    },
            },
            {
                .room_no = 17,
                .ene_type = 7,
                .pos_num = 1,
                .cmr_no = 452,
                .rot = {270, 0, 0},
                .pos =
                    {
                        {480, 200, 1800},
                        {0, 0, 0},
                        {0, 0, 0},
                    },
            },
            {
                .room_no = 22,
                .ene_type = 7,
                .pos_num = 1,
                .cmr_no = 209,
                .rot = {170, 0, 0},
                .pos =
                    {
                        {2900, 0, 11594},
                        {0, 0, 0},
                        {0, 0, 0},
                    },
            },
            {
                .room_no = 22,
                .ene_type = 0,
                .pos_num = 1,
                .cmr_no = 207,
                .rot = {148, 0, 0},
                .pos =
                    {
                        {6305, 0, 2835},
                        {0, 0, 0},
                        {0, 0, 0},
                    },
            },
            {
                .room_no = 16,
                .ene_type = 8,
                .pos_num = 1,
                .cmr_no = 157,
                .rot = {180, 0, 0},
                .pos =
                    {
                        {2321, -500, 6174},
                        {0, 0, 0},
                        {0, 0, 0},
                    },
            },
            {
                .room_no = 16,
                .ene_type = 0,
                .pos_num = 2,
                .cmr_no = 151,
                .rot = {180, 180, 0},
                .pos =
                    {
                        {4500, 0, 8281},
                        {5045, 0, 7592},
                        {0, 0, 0},
                    },
            },
            {
                .room_no = 16,
                .ene_type = 0,
                .pos_num = 2,
                .cmr_no = 160,
                .rot = {0, 270, 0},
                .pos =
                    {
                        {1609, -400, 235},
                        {3702, -400, 350},
                        {0, 0, 0},
                    },
            },
            {
                .room_no = 7,
                .ene_type = 0,
                .pos_num = 1,
                .cmr_no = 63,
                .rot = {90, 0, 0},
                .pos =
                    {
                        {585, 0, 283},
                        {0, 0, 0},
                        {0, 0, 0},
                    },
            },
            {
                .room_no = 7,
                .ene_type = 6,
                .pos_num = 1,
                .cmr_no = 65,
                .rot = {180, 0, 0},
                .pos =
                    {
                        {2271, 1000, 1433},
                        {0, 0, 0},
                        {0, 0, 0},
                    },
            },
            {
                .room_no = 15,
                .ene_type = 0,
                .pos_num = 1,
                .cmr_no = 141,
                .rot = {90, 0, 0},
                .pos =
                    {
                        {300, 0, 360},
                        {0, 0, 0},
                        {0, 0, 0},
                    },
            },
            {
                .room_no = 15,
                .ene_type = 7,
                .pos_num = 1,
                .cmr_no = 145,
                .rot = {180, 0, 0},
                .pos =
                    {
                        {2324, -300, 2199},
                        {0, 0, 0},
                        {0, 0, 0},
                    },
            },
            {
                .room_no = 27,
                .ene_type = 0,
                .pos_num = 2,
                .cmr_no = 273,
                .rot = {140, 310, 0},
                .pos =
                    {
                        {-1990, 0, 3181},
                        {-5, 0, 1314},
                        {0, 0, 0},
                    },
            },
            {
                .room_no = 27,
                .ene_type = 6,
                .pos_num = 1,
                .cmr_no = 274,
                .rot = {237, 0, 0},
                .pos =
                    {
                        {-3300, -1000, 4625},
                        {0, 0, 0},
                        {0, 0, 0},
                    },
            },
            {
                .room_no = 27,
                .ene_type = 0,
                .pos_num = 1,
                .cmr_no = 270,
                .rot = {150, 0, 0},
                .pos =
                    {
                        {-5173, 0, 8353},
                        {0, 0, 0},
                        {0, 0, 0},
                    },
            },
            {
                .room_no = 29,
                .ene_type = 0,
                .pos_num = 1,
                .cmr_no = 343,
                .rot = {90, 0, 0},
                .pos =
                    {
                        {1845, 1500, 5400},
                        {0, 0, 0},
                        {0, 0, 0},
                    },
            },
            {
                .room_no = 29,
                .ene_type = 0,
                .pos_num = 1,
                .cmr_no = 342,
                .rot = {220, 0, 0},
                .pos =
                    {
                        {4845, 3000, 1824},
                        {0, 0, 0},
                        {0, 0, 0},
                    },
            },
            {
                .room_no = 29,
                .ene_type = 0,
                .pos_num = 1,
                .cmr_no = 339,
                .rot = {40, 0, 0},
                .pos =
                    {
                        {1957, 3000, 2100},
                        {0, 0, 0},
                        {0, 0, 0},
                    },
            },
            {
                .room_no = 28,
                .ene_type = 0,
                .pos_num = 2,
                .cmr_no = 396,
                .rot = {90, 270, 0},
                .pos =
                    {
                        {2168, 0, 180},
                        {5874, 300, -136},
                        {0, 0, 0},
                    },
            },
            {
                .room_no = 28,
                .ene_type = 0,
                .pos_num = 1,
                .cmr_no = 401,
                .rot = {30, 0, 0},
                .pos =
                    {
                        {5800, 700, -9000},
                        {0, 0, 0},
                        {0, 0, 0},
                    },
            },
            {
                .room_no = 0,
                .ene_type = 11,
                .pos_num = 0,
                .cmr_no = 0,
                .rot = {0, 0, 0},
                .pos =
                    {
                        {0, 0, 0},
                        {0, 0, 0},
                        {0, 0, 0},
                    },
            },
};

FGST_AP_DAT
    fg_ap_dat4[] =
        {
            {
                .room_no = 26,
                .ene_type = 9,
                .pos_num = 1,
                .cmr_no = 263,
                .rot = {312, 0, 0},
                .pos =
                    {
                        {791, 0, 1548},
                        {0, 0, 0},
                        {0, 0, 0},
                    },
            },
            {
                .room_no = 23,
                .ene_type = 0,
                .pos_num = 1,
                .cmr_no = 230,
                .rot = {90, 0, 0},
                .pos =
                    {
                        {3095, 0, 424},
                        {0, 0, 0},
                        {0, 0, 0},
                    },
            },
            {
                .room_no = 23,
                .ene_type = 0,
                .pos_num = 1,
                .cmr_no = 235,
                .rot = {100, 0, 0},
                .pos =
                    {
                        {176, 50, 5150},
                        {0, 0, 0},
                        {0, 0, 0},
                    },
            },
            {
                .room_no = 21,
                .ene_type = 0,
                .pos_num = 2,
                .cmr_no = 212,
                .rot = {0, 180, 0},
                .pos =
                    {
                        {2151, 0, 1441},
                        {1615, 0, 3322},
                        {0, 0, 0},
                    },
            },
            {
                .room_no = 21,
                .ene_type = 8,
                .pos_num = 1,
                .cmr_no = 218,
                .rot = {90, 0, 0},
                .pos =
                    {
                        {4000, 0, 5000},
                        {0, 0, 0},
                        {0, 0, 0},
                    },
            },
            {
                .room_no = 25,
                .ene_type = 0,
                .pos_num = 2,
                .cmr_no = 253,
                .rot = {270, 90, 0},
                .pos =
                    {
                        {4214, -1300, 3600},
                        {4200, -800, 3837},
                        {0, 0, 0},
                    },
            },
            {
                .room_no = 25,
                .ene_type = 0,
                .pos_num = 1,
                .cmr_no = 255,
                .rot = {270, 0, 0},
                .pos =
                    {
                        {6000, -1500, 7000},
                        {0, 0, 0},
                        {0, 0, 0},
                    },
            },
            {
                .room_no = 40,
                .ene_type = 0,
                .pos_num = 1,
                .cmr_no = 246,
                .rot = {180, 0, 0},
                .pos =
                    {
                        {300, -800, 1826},
                        {0, 0, 0},
                        {0, 0, 0},
                    },
            },
            {
                .room_no = 40,
                .ene_type = 8,
                .pos_num = 1,
                .cmr_no = 244,
                .rot = {0, 0, 0},
                .pos =
                    {
                        {1410, -50, 400},
                        {0, 0, 0},
                        {0, 0, 0},
                    },
            },
            {
                .room_no = 40,
                .ene_type = 5,
                .pos_num = 1,
                .cmr_no = 245,
                .rot = {0, 0, 0},
                .pos =
                    {
                        {2480, -700, 400},
                        {0, 0, 0},
                        {0, 0, 0},
                    },
            },
            {
                .room_no = 3,
                .ene_type = 7,
                .pos_num = 1,
                .cmr_no = 32,
                .rot = {0, 0, 0},
                .pos =
                    {
                        {2040, 0, 1100},
                        {0, 0, 0},
                        {0, 0, 0},
                    },
            },
            {
                .room_no = 3,
                .ene_type = 0,
                .pos_num = 1,
                .cmr_no = 38,
                .rot = {180, 0, 0},
                .pos =
                    {
                        {4807, 0, 3187},
                        {0, 0, 0},
                        {0, 0, 0},
                    },
            },
            {
                .room_no = 3,
                .ene_type = 0,
                .pos_num = 1,
                .cmr_no = 35,
                .rot = {90, 0, 0},
                .pos =
                    {
                        {1626, 0, 487},
                        {0, 0, 0},
                        {0, 0, 0},
                    },
            },
            {
                .room_no = 11,
                .ene_type = 5,
                .pos_num = 1,
                .cmr_no = 117,
                .rot = {0, 0, 0},
                .pos =
                    {
                        {2160, 0, 316},
                        {0, 0, 0},
                        {0, 0, 0},
                    },
            },
            {
                .room_no = 11,
                .ene_type = 0,
                .pos_num = 1,
                .cmr_no = 114,
                .rot = {214, 0, 0},
                .pos =
                    {
                        {1955, 0, 2901},
                        {0, 0, 0},
                        {0, 0, 0},
                    },
            },
            {
                .room_no = 12,
                .ene_type = 0,
                .pos_num = 1,
                .cmr_no = 123,
                .rot = {0, 0, 0},
                .pos =
                    {
                        {1969, 0, 2350},
                        {0, 0, 0},
                        {0, 0, 0},
                    },
            },
            {
                .room_no = 12,
                .ene_type = 5,
                .pos_num = 1,
                .cmr_no = 121,
                .rot = {90, 0, 0},
                .pos =
                    {
                        {180, 700, 150},
                        {0, 0, 0},
                        {0, 0, 0},
                    },
            },
            {
                .room_no = 12,
                .ene_type = 6,
                .pos_num = 1,
                .cmr_no = 120,
                .rot = {270, 0, 0},
                .pos =
                    {
                        {2503, 0, 229},
                        {0, 0, 0},
                        {0, 0, 0},
                    },
            },
            {
                .room_no = 13,
                .ene_type = 5,
                .pos_num = 1,
                .cmr_no = 137,
                .rot = {180, 0, 0},
                .pos =
                    {
                        {6344, 430, 5158},
                        {0, 0, 0},
                        {0, 0, 0},
                    },
            },
            {
                .room_no = 13,
                .ene_type = 0,
                .pos_num = 2,
                .cmr_no = 131,
                .rot = {270, 270, 0},
                .pos =
                    {
                        {4896, 0, 4084},
                        {3749, 0, 3860},
                        {0, 0, 0},
                    },
            },
            {
                .room_no = 13,
                .ene_type = 6,
                .pos_num = 1,
                .cmr_no = 136,
                .rot = {0, 180, 0},
                .pos =
                    {
                        {3515, 0, 2268},
                        {3392, 0, 3923},
                        {0, 0, 0},
                    },
            },
            {
                .room_no = 14,
                .ene_type = 0,
                .pos_num = 1,
                .cmr_no = 324,
                .rot = {270, 0, 0},
                .pos =
                    {
                        {176, 400, 4100},
                        {0, 0, 0},
                        {0, 0, 0},
                    },
            },
            {
                .room_no = 14,
                .ene_type = 8,
                .pos_num = 1,
                .cmr_no = 327,
                .rot = {180, 0, 0},
                .pos =
                    {
                        {2300, -400, 4100},
                        {0, 0, 0},
                        {0, 0, 0},
                    },
            },
            {
                .room_no = 6,
                .ene_type = 0,
                .pos_num = 1,
                .cmr_no = 359,
                .rot = {0, 0, 0},
                .pos =
                    {
                        {1500, 20, 2700},
                        {0, 0, 0},
                        {0, 0, 0},
                    },
            },
            {
                .room_no = 6,
                .ene_type = 9,
                .pos_num = 1,
                .cmr_no = 370,
                .rot = {90, 0, 0},
                .pos =
                    {
                        {2904, -400, 2500},
                        {0, 0, 0},
                        {0, 0, 0},
                    },
            },
            {
                .room_no = 6,
                .ene_type = 0,
                .pos_num = 2,
                .cmr_no = 373,
                .rot = {0, 270, 0},
                .pos =
                    {
                        {4636, 0, 397},
                        {6051, 0, 1500},
                        {0, 0, 0},
                    },
            },
            {
                .room_no = 36,
                .ene_type = 9,
                .pos_num = 1,
                .cmr_no = 312,
                .rot = {180, 0, 0},
                .pos =
                    {
                        {1000, -370, 3124},
                        {0, 0, 0},
                        {0, 0, 0},
                    },
            },
            {
                .room_no = 36,
                .ene_type = 0,
                .pos_num = 1,
                .cmr_no = 313,
                .rot = {141, 0, 0},
                .pos =
                    {
                        {176, -450, 3100},
                        {0, 0, 0},
                        {0, 0, 0},
                    },
            },
            {
                .room_no = 36,
                .ene_type = 0,
                .pos_num = 1,
                .cmr_no = 312,
                .rot = {210, 0, 0},
                .pos =
                    {
                        {1900, -375, 3166},
                        {0, 0, 0},
                        {0, 0, 0},
                    },
            },
            {
                .room_no = 5,
                .ene_type = 0,
                .pos_num = 2,
                .cmr_no = 296,
                .rot = {90, 0, 0},
                .pos =
                    {
                        {296, 0, 9090},
                        {0, 0, 0},
                        {0, 0, 0},
                    },
            },
            {
                .room_no = 5,
                .ene_type = 0,
                .pos_num = 1,
                .cmr_no = 295,
                .rot = {90, 0, 0},
                .pos =
                    {
                        {193, 0, 426},
                        {0, 0, 0},
                        {0, 0, 0},
                    },
            },
            {
                .room_no = 4,
                .ene_type = 0,
                .pos_num = 1,
                .cmr_no = 287,
                .rot = {270, 0, 0},
                .pos =
                    {
                        {6607, 0, 2010},
                        {0, 0, 0},
                        {0, 0, 0},
                    },
            },
            {
                .room_no = 4,
                .ene_type = 6,
                .pos_num = 1,
                .cmr_no = 285,
                .rot = {180, 0, 0},
                .pos =
                    {
                        {1539, 0, 4687},
                        {0, 0, 0},
                        {0, 0, 0},
                    },
            },
            {
                .room_no = 0,
                .ene_type = 0,
                .pos_num = 1,
                .cmr_no = 49,
                .rot = {180, 0, 0},
                .pos =
                    {
                        {2144, -400, 5236},
                        {0, 0, 0},
                        {0, 0, 0},
                    },
            },
            {
                .room_no = 0,
                .ene_type = 0,
                .pos_num = 1,
                .cmr_no = 6,
                .rot = {180, 0, 0},
                .pos =
                    {
                        {2336, 0, 3600},
                        {0, 0, 0},
                        {0, 0, 0},
                    },
            },
            {
                .room_no = 2,
                .ene_type = 0,
                .pos_num = 1,
                .cmr_no = 20,
                .rot = {31, 0, 0},
                .pos =
                    {
                        {176, 0, 205},
                        {0, 0, 0},
                        {0, 0, 0},
                    },
            },
            {
                .room_no = 2,
                .ene_type = 9,
                .pos_num = 1,
                .cmr_no = 23,
                .rot = {0, 0, 0},
                .pos =
                    {
                        {280, -400, 6096},
                        {0, 0, 0},
                        {0, 0, 0},
                    },
            },
            {
                .room_no = 2,
                .ene_type = 6,
                .pos_num = 1,
                .cmr_no = 21,
                .rot = {180, 0, 0},
                .pos =
                    {
                        {545, 0, 2435},
                        {0, 0, 0},
                        {0, 0, 0},
                    },
            },
            {
                .room_no = 1,
                .ene_type = 6,
                .pos_num = 1,
                .cmr_no = 12,
                .rot = {0, 0, 0},
                .pos =
                    {
                        {4123, 0, 2039},
                        {0, 0, 0},
                        {0, 0, 0},
                    },
            },
            {
                .room_no = 1,
                .ene_type = 0,
                .pos_num = 1,
                .cmr_no = 14,
                .rot = {180, 0, 0},
                .pos =
                    {
                        {5638, 0, 3857},
                        {0, 0, 0},
                        {0, 0, 0},
                    },
            },
            {
                .room_no = 1,
                .ene_type = 0,
                .pos_num = 1,
                .cmr_no = 12,
                .rot = {270, 0, 0},
                .pos =
                    {
                        {3358, 0, 2543},
                        {0, 0, 0},
                        {0, 0, 0},
                    },
            },
            {
                .room_no = 9,
                .ene_type = 0,
                .pos_num = 2,
                .cmr_no = 99,
                .rot = {270, 0, 0},
                .pos =
                    {
                        {4504, -1170, 3174},
                        {0, 0, 0},
                        {0, 0, 0},
                    },
            },
            {
                .room_no = 9,
                .ene_type = 0,
                .pos_num = 1,
                .cmr_no = 92,
                .rot = {180, 0, 0},
                .pos =
                    {
                        {670, -1150, 3276},
                        {0, 0, 0},
                        {0, 0, 0},
                    },
            },
            {
                .room_no = 9,
                .ene_type = 5,
                .pos_num = 1,
                .cmr_no = 92,
                .rot = {180, 0, 0},
                .pos =
                    {
                        {800, 750, 3150},
                        {0, 0, 0},
                        {0, 0, 0},
                    },
            },
            {
                .room_no = 35,
                .ene_type = 0,
                .pos_num = 1,
                .cmr_no = 336,
                .rot = {0, 0, 0},
                .pos =
                    {
                        {2592, 0, 300},
                        {0, 0, 0},
                        {0, 0, 0},
                    },
            },
            {
                .room_no = 35,
                .ene_type = 0,
                .pos_num = 1,
                .cmr_no = 337,
                .rot = {180, 0, 0},
                .pos =
                    {
                        {2649, -100, 2221},
                        {0, 0, 0},
                        {0, 0, 0},
                    },
            },
            {
                .room_no = 8,
                .ene_type = 0,
                .pos_num = 1,
                .cmr_no = 41,
                .rot = {90, 0, 0},
                .pos =
                    {
                        {5200, 0, 1793},
                        {0, 0, 0},
                        {0, 0, 0},
                    },
            },
            {
                .room_no = 8,
                .ene_type = 0,
                .pos_num = 2,
                .cmr_no = 89,
                .rot = {90, 90, 0},
                .pos =
                    {
                        {3745, -760, 2570},
                        {6065, -1400, 1924},
                        {0, 0, 0},
                    },
            },
            {
                .room_no = 8,
                .ene_type = 0,
                .pos_num = 1,
                .cmr_no = 85,
                .rot = {320, 0, 0},
                .pos =
                    {
                        {9260, -1400, 2301},
                        {0, 0, 0},
                        {0, 0, 0},
                    },
            },
            {
                .room_no = 38,
                .ene_type = 5,
                .pos_num = 1,
                .cmr_no = 382,
                .rot = {180, 0, 0},
                .pos =
                    {
                        {-743, -250, 3703},
                        {0, 0, 0},
                        {0, 0, 0},
                    },
            },
            {
                .room_no = 24,
                .ene_type = 7,
                .pos_num = 1,
                .cmr_no = 240,
                .rot = {180, 0, 0},
                .pos =
                    {
                        {5500, -1725, 3000},
                        {0, 0, 0},
                        {0, 0, 0},
                    },
            },
            {
                .room_no = 38,
                .ene_type = 0,
                .pos_num = 1,
                .cmr_no = 385,
                .rot = {180, 0, 0},
                .pos =
                    {
                        {1273, 0, 2800},
                        {-272, 0, 1701},
                        {0, 0, 0},
                    },
            },
            {
                .room_no = 38,
                .ene_type = 5,
                .pos_num = 1,
                .cmr_no = 386,
                .rot = {180, 0, 0},
                .pos =
                    {
                        {1113, 200, 904},
                        {0, 0, 1701},
                        {0, 0, 0},
                    },
            },
            {
                .room_no = 41,
                .ene_type = 5,
                .pos_num = 1,
                .cmr_no = 377,
                .rot = {90, 0, 0},
                .pos =
                    {
                        {250, -400, 1707},
                        {0, 0, 0},
                        {0, 0, 0},
                    },
            },
            {
                .room_no = 17,
                .ene_type = 0,
                .pos_num = 1,
                .cmr_no = 183,
                .rot = {0, 0, 0},
                .pos =
                    {
                        {961, -1400, 1142},
                        {0, 0, 0},
                        {0, 0, 0},
                    },
            },
            {
                .room_no = 17,
                .ene_type = 6,
                .pos_num = 1,
                .cmr_no = 173,
                .rot = {180, 0, 0},
                .pos =
                    {
                        {934, -700, 3627},
                        {0, 0, 0},
                        {0, 0, 0},
                    },
            },
            {
                .room_no = 17,
                .ene_type = 7,
                .pos_num = 1,
                .cmr_no = 452,
                .rot = {270, 0, 0},
                .pos =
                    {
                        {480, 200, 1800},
                        {0, 0, 0},
                        {0, 0, 0},
                    },
            },
            {
                .room_no = 22,
                .ene_type = 7,
                .pos_num = 1,
                .cmr_no = 209,
                .rot = {170, 0, 0},
                .pos =
                    {
                        {2900, 0, 11594},
                        {0, 0, 0},
                        {0, 0, 0},
                    },
            },
            {
                .room_no = 22,
                .ene_type = 0,
                .pos_num = 1,
                .cmr_no = 207,
                .rot = {148, 0, 0},
                .pos =
                    {
                        {6305, 0, 2835},
                        {0, 0, 0},
                        {0, 0, 0},
                    },
            },
            {
                .room_no = 16,
                .ene_type = 8,
                .pos_num = 1,
                .cmr_no = 157,
                .rot = {180, 0, 0},
                .pos =
                    {
                        {2321, -500, 6174},
                        {0, 0, 0},
                        {0, 0, 0},
                    },
            },
            {
                .room_no = 16,
                .ene_type = 0,
                .pos_num = 2,
                .cmr_no = 151,
                .rot = {180, 180, 0},
                .pos =
                    {
                        {4500, 0, 8281},
                        {5045, 0, 7592},
                        {0, 0, 0},
                    },
            },
            {
                .room_no = 16,
                .ene_type = 0,
                .pos_num = 2,
                .cmr_no = 160,
                .rot = {0, 270, 0},
                .pos =
                    {
                        {1609, -400, 235},
                        {3702, -400, 350},
                        {0, 0, 0},
                    },
            },
            {
                .room_no = 7,
                .ene_type = 0,
                .pos_num = 1,
                .cmr_no = 63,
                .rot = {90, 0, 0},
                .pos =
                    {
                        {585, 0, 283},
                        {0, 0, 0},
                        {0, 0, 0},
                    },
            },
            {
                .room_no = 7,
                .ene_type = 6,
                .pos_num = 1,
                .cmr_no = 65,
                .rot = {180, 0, 0},
                .pos =
                    {
                        {2271, 1000, 1433},
                        {0, 0, 0},
                        {0, 0, 0},
                    },
            },
            {
                .room_no = 15,
                .ene_type = 0,
                .pos_num = 1,
                .cmr_no = 141,
                .rot = {90, 0, 0},
                .pos =
                    {
                        {300, 0, 360},
                        {0, 0, 0},
                        {0, 0, 0},
                    },
            },
            {
                .room_no = 15,
                .ene_type = 7,
                .pos_num = 1,
                .cmr_no = 145,
                .rot = {180, 0, 0},
                .pos =
                    {
                        {2324, -300, 2199},
                        {0, 0, 0},
                        {0, 0, 0},
                    },
            },
            {
                .room_no = 27,
                .ene_type = 0,
                .pos_num = 2,
                .cmr_no = 273,
                .rot = {140, 310, 0},
                .pos =
                    {
                        {-1990, 0, 3181},
                        {-5, 0, 1314},
                        {0, 0, 0},
                    },
            },
            {
                .room_no = 27,
                .ene_type = 6,
                .pos_num = 1,
                .cmr_no = 274,
                .rot = {237, 0, 0},
                .pos =
                    {
                        {-3300, -1000, 4625},
                        {0, 0, 0},
                        {0, 0, 0},
                    },
            },
            {
                .room_no = 27,
                .ene_type = 0,
                .pos_num = 1,
                .cmr_no = 270,
                .rot = {150, 0, 0},
                .pos =
                    {
                        {-5173, 0, 8353},
                        {0, 0, 0},
                        {0, 0, 0},
                    },
            },
            {
                .room_no = 29,
                .ene_type = 0,
                .pos_num = 1,
                .cmr_no = 343,
                .rot = {90, 0, 0},
                .pos =
                    {
                        {1845, 1500, 5400},
                        {0, 0, 0},
                        {0, 0, 0},
                    },
            },
            {
                .room_no = 29,
                .ene_type = 0,
                .pos_num = 1,
                .cmr_no = 342,
                .rot = {220, 0, 0},
                .pos =
                    {
                        {4845, 3000, 1824},
                        {0, 0, 0},
                        {0, 0, 0},
                    },
            },
            {
                .room_no = 29,
                .ene_type = 0,
                .pos_num = 1,
                .cmr_no = 339,
                .rot = {40, 0, 0},
                .pos =
                    {
                        {1957, 3000, 2100},
                        {0, 0, 0},
                        {0, 0, 0},
                    },
            },
            {
                .room_no = 28,
                .ene_type = 0,
                .pos_num = 2,
                .cmr_no = 396,
                .rot = {90, 270, 0},
                .pos =
                    {
                        {2168, 0, 180},
                        {5874, 300, -136},
                        {0, 0, 0},
                    },
            },
            {
                .room_no = 28,
                .ene_type = 0,
                .pos_num = 1,
                .cmr_no = 401,
                .rot = {30, 0, 0},
                .pos =
                    {
                        {5800, 700, -9000},
                        {0, 0, 0},
                        {0, 0, 0},
                    },
            },
            {
                .room_no = 0,
                .ene_type = 11,
                .pos_num = 0,
                .cmr_no = 0,
                .rot = {0, 0, 0},
                .pos =
                    {
                        {0, 0, 0},
                        {0, 0, 0},
                        {0, 0, 0},
                    },
            },
};


FGST_AP_DAT *fg_ap_dat[5] = {
    fg_ap_dat1, fg_ap_dat1, fg_ap_dat2, fg_ap_dat3, fg_ap_dat3,
};

FG_LOAD_WRK fg_load_wrk = {0};

#define ENEMY_ANM_ADDR 0x1330000
#define ENEMY_DMG_TEX_ADDR 0x13c8000
#define ENEMY_MDL_ADDR 0x13e0000
#define FLOAT_GHOST_SE_LOAD_ADDR 0x1460000

void FloatGhostAppearInit()
{
    int i;

    ap_wrk.fg_mode = 1;
    ap_wrk.fgst_no = -1;

    for (i = 0; i <= 2; i++)
    {
        ap_wrk.fg_pos[0][i] = -1;
        ap_wrk.fg_pos[1][i] = -1;
    }

    ap_wrk.stts |= 0x40;
}

int FloatGhostEntrySet(u_char area)
{
    return 0;
}

int FloatGhostAppearMain()
{
    u_char type;

    switch (ap_wrk.fg_mode)
    {
    case FG_COUNT:
        if (ap_wrk.fg_set_num != 0)
        {
            if (ingame_wrk.msn_no == 3)
            {
                ap_wrk.fg_ap += ap_wrk.ptime * 500;
            }
            else
            {
                ap_wrk.fg_ap += ap_wrk.ptime * 350;
            }
        }

        if (EnemyUseJudge(ET_FUYU))
        {
            if (ap_wrk.fg_ap < 950)
            {
                break;
            }
            ap_wrk.fg_ap = 950;
            break;
        }

        if (!FloatGhostAppearJudge())
        {
            break;
        }

        if (DeadGhostAppearJudge())
        {
            DeadGhostAppearReq();
            ap_wrk.fg_ap = 700;
            ap_wrk.fg_mode = FG_ENTRY;
            break;
        }
        ap_wrk.fg_mode = FG_WAIT;
        if (GetFloatGhostAppearPosType(&type) == 0 && plyr_wrk.mode != PMODE_FINDER)
        {
            break;
        }

        FloatGhostAppearPosSet(0xff, ene_wrk[EWRK_GHOST2].move_box.pos, ene_wrk[EWRK_GHOST2].move_box.rot);
        ene_wrk[EWRK_GHOST2].dat_no = fgst_ap_no[0][ap_wrk.fgst_cnt % 20 + ingame_wrk.msn_no * 20];
        ene_wrk[EWRK_GHOST2].sta = 2;
        ene_wrk[EWRK_GHOST2].type = ET_FUYU;
        ap_wrk.fg_mode = FG_ENTRY;
    break;
    case FG_WAIT:
        if (EnemyUseJudge(1) != 0)
        {
            ap_wrk.fg_mode = FG_COUNT;
            ap_wrk.fg_ap = 950;
            break;
        }

        if (GetFloatGhostAppearPosType(&type) == 0)
        {
            break;
        }

        FloatGhostAppearPosSet(type, ene_wrk[EWRK_GHOST2].move_box.pos, ene_wrk[EWRK_GHOST2].move_box.rot);
        ene_wrk[EWRK_GHOST2].dat_no = fgst_ap_no[0][ap_wrk.fgst_cnt % 20 + ingame_wrk.msn_no * 20];
        ap_wrk.fg_mode = FG_ENTRY;
        ene_wrk[EWRK_GHOST2].sta = 2;
        ene_wrk[EWRK_GHOST2].type = ET_FUYU;
    break;
    }

    return 0;
}

int FloatGhostAppearJudge()
{
    if (plyr_wrk.mode == PMODE_MSG_DISP)
    {
          return 0;
    }

    if(ingame_wrk.mode == INGAME_MODE_GET_ITEM)
    {
        return 0;
    }

    if (ap_wrk.fg_ap >= 1000)
    {
        ap_wrk.fg_ap = 950;
        return 1;
    }

    return 0;
}

int FloatGhostAppearTypeSet(u_char fg_no, u_char wrk_no, u_char room)
{
    int i;
    int ret;

    ret = 0;

    for (i = 0; i <= 2; i++)
    {
        ap_wrk.fg_pos[wrk_no][i] = -1;
    }

    i = 0;

    while (fg_ap_dat[ingame_wrk.msn_no][i].ene_type != 11)
    {
        if (
            fene_dat[ingame_wrk.msn_no][fg_no].dir == fg_ap_dat[ingame_wrk.msn_no][i].ene_type &&
            room == fg_ap_dat[ingame_wrk.msn_no][i].room_no
        )
        {
            ap_wrk.fg_pos[wrk_no][ret] = i;
            ret++;
        }

        i++;
    }

    return ret;
}

int GetFloatGhostAppearPosType(u_char *type)
{
    int i;
    int j;

    for (i = 0; i < 2; i++)
    {
        if (plyr_wrk.pr_info.room_no == area_wrk.room[i])
        {
            if (ap_wrk.fg_pos[i][0] == 0xff)
            {
                *type = 0xff;
                return 1;
            }

            if (plyr_wrk.mode == 1)
            {
                for (j = 0; j < 3; j++)
                {
                    if (fg_ap_dat[ingame_wrk.msn_no][ap_wrk.fg_pos[i][j]].cmr_no == plyr_wrk.pr_info.camera_no)
                    {
                        *type = 0xff;
                        return 1;
                    }
                }
            }
            else if (plyr_wrk.pr_info.cam_type == 0)
            {
                for (j = 0; j < 3; j++)
                {
                    if (fg_ap_dat[ingame_wrk.msn_no][ap_wrk.fg_pos[i][j]].cmr_no == plyr_wrk.pr_info.camera_no)
                    {
                        *type = ap_wrk.fg_pos[i][j];
                        return 1;
                    }
                }
            }

            break;
        }
    }

    return 0;
}

void FloatGhostAppearPosSet(u_char dat_no, float *set_pos, float *set_rot)
{
    int i;
    float dist;
    float dist_bak;
    sceVu0FVECTOR pos;
    sceVu0FVECTOR tv = {0.0f, 0.0f, 3000.0f, 0.0f};
    sceVu0FVECTOR rv = {0.0f, 0.0f, 0.0f, 0.0f};

    set_rot[0] = 0.0f;
    set_rot[1] = 0.0f;
    set_rot[2] = 0.0f;
    set_rot[3] = 0.0f;

    if (dat_no == 0xff)
    {
        rv[1] = SetRot360(GetRndSP(0, 359));
        RotFvector(rv, tv);
        sceVu0AddVector(set_pos, plyr_wrk.move_box.pos, tv);
    }
    else
    {
        dist_bak = set_rot[0];

        for (i = 0; i < fg_ap_dat[ingame_wrk.msn_no][dat_no].pos_num; i++)
        {
            pos[0] = fg_ap_dat[ingame_wrk.msn_no][dat_no].pos[i][0];
            pos[1] = fg_ap_dat[ingame_wrk.msn_no][dat_no].pos[i][1];
            pos[2] = fg_ap_dat[ingame_wrk.msn_no][dat_no].pos[i][2];

            sceVu0AddVector(&pos, room_wrk.pos, &pos);

            dist = GetDistV2(pos, plyr_wrk.move_box.pos);

            if (dist_bak < dist)
            {
                set_pos[0] = pos[0];
                set_pos[1] = pos[1];
                set_pos[2] = pos[2];
                set_pos[3] = pos[3];

                dist_bak = SetRot360(fg_ap_dat[ingame_wrk.msn_no][dat_no].rot[i]);
                set_rot[1] = dist_bak;
                dist_bak = dist;
            }
        }
    }
}

int FloatGhostBattleEnd()
{

    ap_wrk.fg_mode = 1;
    ap_wrk.fg_ap = 0;

    // missing return!!
}

int FloatGhostEscapeEnd()
{
    ap_wrk.fg_mode = 1;
    ap_wrk.fg_ap = 700;

    // missing return!!
}

void FloatGhostLoadReq()
{
    fg_load_wrk = (FG_LOAD_WRK){0};
    fg_load_wrk.mode = FG_NO_REQ;
}

int FloatGhostLoadMain()
{
    switch(fg_load_wrk.mode)
    {
    case FG_LOAD_MODE_READY:
        if (FloatGhostLoadSet())
        {
            fg_load_wrk.mode = FG_LOAD_MDL_LOAD;
        }
        else
        {
            fg_load_wrk.mode = FG_LOAD_MODE_END;
        }
    break;
    case FG_LOAD_MDL_LOAD:
        GetFloatGhostModelLoad();
        fg_load_wrk.mode = FG_LOAD_MDL_WAIT;
    break;
    case FG_LOAD_MDL_WAIT:
        if (IsLoadEndAll() == 0)
        {
            return 0;
        }
        GetFloatGhostModelLoadAfter();
        fg_load_wrk.mode = FG_LOAD_MOT_LOAD;
    case FG_LOAD_MOT_LOAD:
        GetFloatGhostMotionLoad();
        fg_load_wrk.mode = FG_LOAD_MOT_WAIT;
    break;
    case FG_LOAD_MOT_WAIT:
        if (IsLoadEndAll() == 0)
        {
            return 0;
        }
        GetFloatGhostMotionLoadAfter();
        fg_load_wrk.mode = FG_LOAD_SE_LOAD;
    case FG_LOAD_SE_LOAD:
        GetFloatGhostSELoad();
        fg_load_wrk.mode = FG_LOAD_SE_WAIT;
    break;
    case FG_LOAD_SE_WAIT:
        if (IsLoadEndAll() == 0)
        {
            return 0;
        }
        fg_load_wrk.mode = FG_LOAD_SE_TRANS;
        FGTransInit();
    case FG_LOAD_SE_TRANS:
        SeFGhostTransCtrl();
        if (IsEndFgTrans() == 0)
        {
            break;
        }
        fg_load_wrk.mode = FG_LOAD_MODE_END;
    case FG_LOAD_MODE_END:
        return 1;
    break;
    }

    return 0;
}

int FloatGhostLoadSet()
{
    if (ingame_wrk.msn_no == 0 || ingame_wrk.msn_no == 4)
    {
        return 0;
    }

    fg_load_wrk.load_no = fgst_ap_no[ingame_wrk.msn_no][ap_wrk.fgst_cnt % 20];

    ap_wrk.fg_set_num = 1;

    if (ap_wrk.fgst_no == fg_load_wrk.load_no)
    {
        return 0;
    }

    if (ap_wrk.fgst_no != 0xff)
    {
        motReleaseAniMdlBuf(fene_dat[ingame_wrk.msn_no][ap_wrk.fgst_no].anm_no, (u_int *)ENEMY_ANM_ADDR);
    }

    return 1;
}

int FloatGhostSetJudge()
{
    return 1;
}

void GetLoadFloatGhost(u_char set_num, u_char *set_fgst)
{
    set_fgst[0] = fgst_ap_no[ingame_wrk.msn_no][ap_wrk.fgst_cnt % 20];
}

void GetFloatGhostModelLoad()
{
    LoadReq(fene_dat[ingame_wrk.msn_no][fg_load_wrk.load_no].mdl_no + M000_MIKU_MDL, ENEMY_MDL_ADDR);

    ap_wrk.fgst_no = fg_load_wrk.load_no;
}

void GetFloatGhostModelLoadAfter()
{
    motInitEnemyMdl((u_int *)ENEMY_MDL_ADDR, fene_dat[ingame_wrk.msn_no][fg_load_wrk.load_no].mdl_no);
}

void GetFloatGhostMotionLoad()
{
    LoadEneDmgTex(fene_dat[ingame_wrk.msn_no][fg_load_wrk.load_no].mdl_no, (u_int *)ENEMY_DMG_TEX_ADDR);
    LoadReq(fene_dat[ingame_wrk.msn_no][fg_load_wrk.load_no].anm_no + M000_MIKU_ANM, ENEMY_ANM_ADDR);
}

void GetFloatGhostMotionLoadAfter()
{
  motInitEnemyAnm(
    (u_int *)ENEMY_ANM_ADDR,
    fene_dat[ingame_wrk.msn_no][fg_load_wrk.load_no].mdl_no,
    fene_dat[ingame_wrk.msn_no][fg_load_wrk.load_no].anm_no);
}

void GetFloatGhostSELoad()
{
    LoadReq(fene_dat[ingame_wrk.msn_no][fg_load_wrk.load_no].se_no, FLOAT_GHOST_SE_LOAD_ADDR);
}

void FloatGhostAppearStop()
{
    int i;

    for (i = 0; i < 42; i++)
    {
        ap_wrk.room_fg[i] = 0;
    }
}

void FloatGhostAppearStart()
{
    int i;

    for (i = 0; i < 42; i++)
    {
        ap_wrk.room_fg[i] = 1;
    }
}
