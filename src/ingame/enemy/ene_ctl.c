#include "common.h"
#include "typedefs.h"
#include "enums.h"
#include "ene_ctl.h"

#include <string.h>

#include "sce/libvu0.h"

#include "common/ul_math.h"
#include "main/glob.h"
#include "os/eeiop/eese.h"
#include "os/eeiop/cdvd/eecdvd.h"
#include "os/eeiop/adpcm/ea_dat.h"
#include "os/eeiop/adpcm/ea_ctrl.h"
#include "os/eeiop/adpcm/ea_ghost.h"
#include "os/eeiop/adpcm/ea_autog.h"
#include "ingame/entry/entry.h"
#include "ingame/entry/ap_bgost.h"
#include "ingame/entry/ap_dgost.h"
#include "ingame/entry/ap_fgost.h"
#include "ingame/entry/ap_ggost.h"
#include "ingame/entry/ap_sgost.h"
#include "ingame/plyr/unit_ctl.h"
#include "ingame/plyr/plyr_ctl.h"
#include "ingame/camera/camera.h"
#include "graphics/graph2d/effect.h"
#include "graphics/graph2d/message.h"
#include "graphics/graph2d/effect_sub.h"
#include "ingame/map/map_htck.h"
#include "ingame/map/map_ctrl.h"

ENE_DAT jene_dat0[] = {
    {
        .attr1 = 0,
        .dst_gthr = 8000,
        .way_gthr = 1,
        .atk_ptn = 1,
        .wspd = 3,
        .rspd = 6,
        .hp = 160,
        .atk_rng = 750,
        .hit_rng = 500,
        .chance_rng = 750,
        .hit_adjx = 0,
        .atk_p = 30,
        .atk_h = 50,
        .atk = 1,
        .atk_tm = 4,
        .mdl_no = 58,
        .anm_no = 34,
        .se_no = 1385,
        .adpcm_no = 1572,
        .dead_adpcm = -1,
        .point_base = 50,
        .hint_pic = 255,
        .aura_alp = 32,
        .area = {3, 255, 255, 255, 255, 255},
        .dir = 1000,
        .px = 26500,
        .py = -1600,
        .pz = 15000,
    },
};

AENE_INFO_DAT aene_info_dat0[] = {
    {
        .dat_no = 0,
        .soul_no = 255,
        .dir = 90,
        .px = 21600,
        .py = 0,
        .pz = 14657,
        .adpcm_tm = 0,
        .adpcm_no = -1,
        .rng = 1000,
        .mdl_no = 2,
        .anm_no = 53,
        .point_base = 200,
        .se_no = 4294967295,
        .se_foot = 1452,
    },
    {
        .dat_no = 1,
        .soul_no = 255,
        .dir = 180,
        .px = 26800,
        .py = -1400,
        .pz = 14276,
        .adpcm_tm = 0,
        .adpcm_no = -1,
        .rng = 1000,
        .mdl_no = 2,
        .anm_no = 40,
        .point_base = 200,
        .se_no = 1431,
        .se_foot = 1452,
    },
    {
        .dat_no = 2,
        .soul_no = 255,
        .dir = 0,
        .px = 23375,
        .py = 0,
        .pz = 13250,
        .adpcm_tm = 0,
        .adpcm_no = -1,
        .rng = 0,
        .mdl_no = 61,
        .anm_no = 58,
        .point_base = 0,
        .se_no = 1434,
        .se_foot = 1452,
    },
};

FLY_DATA fly_dat[] = {
    {
        .dmg = 30,
        .hit_rng = 100,
        .mdl_no = 65535,
        .cond = 0,
    },
    {
        .dmg = 50,
        .hit_rng = 100,
        .mdl_no = 65535,
        .cond = 0,
    },
    {
        .dmg = 80,
        .hit_rng = 100,
        .mdl_no = 65535,
        .cond = 0,
    },
    {
        .dmg = 150,
        .hit_rng = 100,
        .mdl_no = 65535,
        .cond = 0,
    },
    {
        .dmg = 50,
        .hit_rng = 100,
        .mdl_no = 67,
        .cond = 3,
    },
    {
        .dmg = 50,
        .hit_rng = 100,
        .mdl_no = 68,
        .cond = 3,
    },
    {
        .dmg = 50,
        .hit_rng = 100,
        .mdl_no = 69,
        .cond = 3,
    },
};

ENE_DAT jene_dat1[] = {
    {
        .attr1 = 0,
        .dst_gthr = 5000,
        .way_gthr = 1,
        .atk_ptn = 1,
        .wspd = 3,
        .rspd = 6,
        .hp = 100,
        .atk_rng = 660,
        .hit_rng = 360,
        .chance_rng = 500,
        .hit_adjx = -50,
        .atk_p = 30,
        .atk_h = 100,
        .atk = 1,
        .atk_tm = 6,
        .mdl_no = 10,
        .anm_no = 2,
        .se_no = 1363,
        .adpcm_no = 1564,
        .dead_adpcm = -1,
        .point_base = 100,
        .hint_pic = 255,
        .aura_alp = 32,
        .area = {11, 255, 255, 255, 255, 255},
        .dir = 1000,
        .px = 24650,
        .py = 0,
        .pz = 16800,
    },
    {
        .attr1 = 0,
        .dst_gthr = 5000,
        .way_gthr = 1,
        .atk_ptn = 1,
        .wspd = 3,
        .rspd = 6,
        .hp = 120,
        .atk_rng = 660,
        .hit_rng = 360,
        .chance_rng = 500,
        .hit_adjx = -50,
        .atk_p = 30,
        .atk_h = 100,
        .atk = 1,
        .atk_tm = 6,
        .mdl_no = 10,
        .anm_no = 2,
        .se_no = 1363,
        .adpcm_no = 1564,
        .dead_adpcm = -1,
        .point_base = 100,
        .hint_pic = 255,
        .aura_alp = 32,
        .area = {12, 11, 13, 255, 255, 255},
        .dir = 1000,
        .px = 28500,
        .py = 0,
        .pz = 15500,
    },
    {
        .attr1 = 0,
        .dst_gthr = 5000,
        .way_gthr = 1,
        .atk_ptn = 1,
        .wspd = 3,
        .rspd = 6,
        .hp = 300,
        .atk_rng = 660,
        .hit_rng = 360,
        .chance_rng = 500,
        .hit_adjx = -50,
        .atk_p = 30,
        .atk_h = 100,
        .atk = 1,
        .atk_tm = 6,
        .mdl_no = 10,
        .anm_no = 2,
        .se_no = 1363,
        .adpcm_no = 1564,
        .dead_adpcm = 1589,
        .point_base = 100,
        .hint_pic = 255,
        .aura_alp = 32,
        .area = {11, 255, 255, 255, 255, 255},
        .dir = 180,
        .px = 24300,
        .py = 0,
        .pz = 16800,
    },
    {
        .attr1 = 64,
        .dst_gthr = 8000,
        .way_gthr = 1,
        .atk_ptn = 1,
        .wspd = 3,
        .rspd = 10,
        .hp = 250,
        .atk_rng = 1250,
        .hit_rng = 650,
        .chance_rng = 1250,
        .hit_adjx = 0,
        .atk_p = 50,
        .atk_h = 150,
        .atk = 1,
        .atk_tm = 8,
        .mdl_no = 13,
        .anm_no = 5,
        .se_no = 1381,
        .adpcm_no = 1567,
        .dead_adpcm = -1,
        .point_base = 200,
        .hint_pic = 255,
        .aura_alp = 32,
        .area = {8, 255, 255, 255, 255, 255},
        .dir = 180,
        .px = 20300,
        .py = 0,
        .pz = 15900,
    },
    {
        .attr1 = 0,
        .dst_gthr = 8000,
        .way_gthr = 1,
        .atk_ptn = 0,
        .wspd = 3,
        .rspd = 7,
        .hp = 150,
        .atk_rng = 900,
        .hit_rng = 350,
        .chance_rng = 700,
        .hit_adjx = 0,
        .atk_p = 50,
        .atk_h = 100,
        .atk = 1,
        .atk_tm = 4,
        .mdl_no = 65,
        .anm_no = 38,
        .se_no = 1389,
        .adpcm_no = 1579,
        .dead_adpcm = -1,
        .point_base = 100,
        .hint_pic = 255,
        .aura_alp = 32,
        .area = {1, 8, 255, 255, 255, 255},
        .dir = 270,
        .px = 16300,
        .py = -600,
        .pz = 12400,
    },
    {
        .attr1 = 0,
        .dst_gthr = 8000,
        .way_gthr = 1,
        .atk_ptn = 1,
        .wspd = 4,
        .rspd = 8,
        .hp = 230,
        .atk_rng = 750,
        .hit_rng = 325,
        .chance_rng = 750,
        .hit_adjx = 0,
        .atk_p = 50,
        .atk_h = 150,
        .atk = 1,
        .atk_tm = 6,
        .mdl_no = 63,
        .anm_no = 36,
        .se_no = 1382,
        .adpcm_no = 1573,
        .dead_adpcm = -1,
        .point_base = 150,
        .hint_pic = 255,
        .aura_alp = 32,
        .area = {16, 22, 255, 255, 255, 255},
        .dir = 1000,
        .px = 23030,
        .py = 0,
        .pz = 21913,
    },
    {
        .attr1 = 0,
        .dst_gthr = 8000,
        .way_gthr = 1,
        .atk_ptn = 2,
        .wspd = 2,
        .rspd = 6,
        .hp = 300,
        .atk_rng = 715,
        .hit_rng = 415,
        .chance_rng = 715,
        .hit_adjx = -50,
        .atk_p = 50,
        .atk_h = 150,
        .atk = 1,
        .atk_tm = 6,
        .mdl_no = 18,
        .anm_no = 9,
        .se_no = 1367,
        .adpcm_no = 1571,
        .dead_adpcm = -1,
        .point_base = 150,
        .hint_pic = 255,
        .aura_alp = 24,
        .area = {10, 255, 255, 255, 255, 255},
        .dir = 1000,
        .px = 22900,
        .py = 0,
        .pz = 16000,
    },
    {
        .attr1 = 0,
        .dst_gthr = 8000,
        .way_gthr = 1,
        .atk_ptn = 0,
        .wspd = 4,
        .rspd = 8,
        .hp = 200,
        .atk_rng = 900,
        .hit_rng = 500,
        .chance_rng = 900,
        .hit_adjx = 0,
        .atk_p = 50,
        .atk_h = 170,
        .atk = 1,
        .atk_tm = 7,
        .mdl_no = 11,
        .anm_no = 3,
        .se_no = 1364,
        .adpcm_no = 1565,
        .dead_adpcm = -1,
        .point_base = 250,
        .hint_pic = 255,
        .aura_alp = 32,
        .area = {23, 255, 255, 255, 255, 255},
        .dir = 1000,
        .px = 29000,
        .py = -100,
        .pz = 29500,
    },
    {
        .attr1 = 0,
        .dst_gthr = 8000,
        .way_gthr = 1,
        .atk_ptn = 0,
        .wspd = 7,
        .rspd = 8,
        .hp = 130,
        .atk_rng = 900,
        .hit_rng = 120,
        .chance_rng = 500,
        .hit_adjx = 0,
        .atk_p = 30,
        .atk_h = 100,
        .atk = 1,
        .atk_tm = 4,
        .mdl_no = 64,
        .anm_no = 37,
        .se_no = 1384,
        .adpcm_no = 1574,
        .dead_adpcm = -1,
        .point_base = 50,
        .hint_pic = 255,
        .aura_alp = 16,
        .area = {17, 16, 23, 255, 255, 255},
        .dir = 1000,
        .px = 29850,
        .py = -800,
        .pz = 22600,
    },
    {
        .attr1 = 0,
        .dst_gthr = 8000,
        .way_gthr = 1,
        .atk_ptn = 0,
        .wspd = 4,
        .rspd = 8,
        .hp = 450,
        .atk_rng = 900,
        .hit_rng = 500,
        .chance_rng = 800,
        .hit_adjx = 0,
        .atk_p = 50,
        .atk_h = 170,
        .atk = 1,
        .atk_tm = 7,
        .mdl_no = 11,
        .anm_no = 3,
        .se_no = 1364,
        .adpcm_no = 1565,
        .dead_adpcm = 1590,
        .point_base = 250,
        .hint_pic = 255,
        .aura_alp = 32,
        .area = {21, 255, 255, 255, 255, 255},
        .dir = 1000,
        .px = 33000,
        .py = -100,
        .pz = 32500,
    },
    {
        .attr1 = 0,
        .dst_gthr = 8000,
        .way_gthr = 1,
        .atk_ptn = 1,
        .wspd = 4,
        .rspd = 9,
        .hp = 650,
        .atk_rng = 800,
        .hit_rng = 500,
        .chance_rng = 800,
        .hit_adjx = 0,
        .atk_p = 50,
        .atk_h = 200,
        .atk = 1,
        .atk_tm = 7,
        .mdl_no = 12,
        .anm_no = 4,
        .se_no = 1365,
        .adpcm_no = 1566,
        .dead_adpcm = 1592,
        .point_base = 300,
        .hint_pic = 5,
        .aura_alp = 32,
        .area = {26, 25, 255, 255, 255, 255},
        .dir = 180,
        .px = 34850,
        .py = -1625,
        .pz = 46650,
    },
    {
        .attr1 = 0,
        .dst_gthr = 8000,
        .way_gthr = 1,
        .atk_ptn = 1,
        .wspd = 4,
        .rspd = 8,
        .hp = 2400,
        .atk_rng = 800,
        .hit_rng = 300,
        .chance_rng = 800,
        .hit_adjx = 0,
        .atk_p = 500,
        .atk_h = 500,
        .atk = 1,
        .atk_tm = 8,
        .mdl_no = 40,
        .anm_no = 28,
        .se_no = 1380,
        .adpcm_no = 1586,
        .dead_adpcm = -1,
        .point_base = 400,
        .hint_pic = 255,
        .aura_alp = 32,
        .area = {26, 255, 255, 255, 255, 255},
        .dir = 1000,
        .px = 34200,
        .py = -1625,
        .pz = 46600,
    },
    {
        .attr1 = 0,
        .dst_gthr = 8000,
        .way_gthr = 1,
        .atk_ptn = 1,
        .wspd = 6,
        .rspd = 8,
        .hp = 500,
        .atk_rng = 600,
        .hit_rng = 250,
        .chance_rng = 600,
        .hit_adjx = 0,
        .atk_p = 500,
        .atk_h = 500,
        .atk = 1,
        .atk_tm = 8,
        .mdl_no = 40,
        .anm_no = 28,
        .se_no = 1380,
        .adpcm_no = 1586,
        .dead_adpcm = -1,
        .point_base = 400,
        .hint_pic = 255,
        .aura_alp = 32,
        .area = {255, 255, 255, 255, 255, 255},
        .dir = 0,
        .px = 0,
        .py = 0,
        .pz = 0,
    },
};

ENE_DAT fene_dat1[] = {
    {
        .attr1 = 0,
        .dst_gthr = 5000,
        .way_gthr = 1,
        .atk_ptn = 1,
        .wspd = 3,
        .rspd = 7,
        .hp = 150,
        .atk_rng = 850,
        .hit_rng = 500,
        .chance_rng = 850,
        .hit_adjx = 0,
        .atk_p = 30,
        .atk_h = 100,
        .atk = 1,
        .atk_tm = 4,
        .mdl_no = 19,
        .anm_no = 10,
        .se_no = 1385,
        .adpcm_no = 1578,
        .dead_adpcm = -1,
        .point_base = 50,
        .hint_pic = 255,
        .aura_alp = 32,
        .area = {255, 255, 255, 255, 255, 255},
        .dir = 0,
        .px = 0,
        .py = 0,
        .pz = 0,
    },
    {
        .attr1 = 0,
        .dst_gthr = 5000,
        .way_gthr = 1,
        .atk_ptn = 1,
        .wspd = 4,
        .rspd = 8,
        .hp = 230,
        .atk_rng = 750,
        .hit_rng = 325,
        .chance_rng = 750,
        .hit_adjx = 0,
        .atk_p = 50,
        .atk_h = 150,
        .atk = 1,
        .atk_tm = 5,
        .mdl_no = 20,
        .anm_no = 11,
        .se_no = 1382,
        .adpcm_no = 1573,
        .dead_adpcm = -1,
        .point_base = 100,
        .hint_pic = 255,
        .aura_alp = 32,
        .area = {255, 255, 255, 255, 255, 255},
        .dir = 0,
        .px = 0,
        .py = 0,
        .pz = 0,
    },
    {
        .attr1 = 0,
        .dst_gthr = 5000,
        .way_gthr = 1,
        .atk_ptn = 0,
        .wspd = 7,
        .rspd = 8,
        .hp = 120,
        .atk_rng = 900,
        .hit_rng = 120,
        .chance_rng = 500,
        .hit_adjx = 0,
        .atk_p = 30,
        .atk_h = 100,
        .atk = 1,
        .atk_tm = 4,
        .mdl_no = 21,
        .anm_no = 12,
        .se_no = 1384,
        .adpcm_no = 1574,
        .dead_adpcm = -1,
        .point_base = 50,
        .hint_pic = 255,
        .aura_alp = 24,
        .area = {255, 255, 255, 255, 255, 255},
        .dir = 0,
        .px = 0,
        .py = 0,
        .pz = 0,
    },
    {
        .attr1 = 0,
        .dst_gthr = 8000,
        .way_gthr = 1,
        .atk_ptn = 0,
        .wspd = 3,
        .rspd = 7,
        .hp = 150,
        .atk_rng = 900,
        .hit_rng = 350,
        .chance_rng = 700,
        .hit_adjx = 0,
        .atk_p = 50,
        .atk_h = 100,
        .atk = 1,
        .atk_tm = 4,
        .mdl_no = 27,
        .anm_no = 17,
        .se_no = 1389,
        .adpcm_no = 1579,
        .dead_adpcm = -1,
        .point_base = 100,
        .hint_pic = 255,
        .aura_alp = 32,
        .area = {255, 255, 255, 255, 255, 255},
        .dir = 0,
        .px = 0,
        .py = 0,
        .pz = 0,
    },
    {
        .attr1 = 0,
        .dst_gthr = 5000,
        .way_gthr = 1,
        .atk_ptn = 0,
        .wspd = 7,
        .rspd = 8,
        .hp = 120,
        .atk_rng = 900,
        .hit_rng = 120,
        .chance_rng = 500,
        .hit_adjx = 0,
        .atk_p = 30,
        .atk_h = 100,
        .atk = 1,
        .atk_tm = 4,
        .mdl_no = 21,
        .anm_no = 12,
        .se_no = 1384,
        .adpcm_no = 1574,
        .dead_adpcm = -1,
        .point_base = 50,
        .hint_pic = 255,
        .aura_alp = 32,
        .area = {255, 255, 255, 255, 255, 255},
        .dir = 5,
        .px = 0,
        .py = 0,
        .pz = 0,
    },
    {
        .attr1 = 0,
        .dst_gthr = 5000,
        .way_gthr = 1,
        .atk_ptn = 1,
        .wspd = 4,
        .rspd = 8,
        .hp = 230,
        .atk_rng = 750,
        .hit_rng = 325,
        .chance_rng = 750,
        .hit_adjx = 0,
        .atk_p = 50,
        .atk_h = 150,
        .atk = 1,
        .atk_tm = 5,
        .mdl_no = 20,
        .anm_no = 11,
        .se_no = 1382,
        .adpcm_no = 1573,
        .dead_adpcm = -1,
        .point_base = 100,
        .hint_pic = 255,
        .aura_alp = 32,
        .area = {255, 255, 255, 255, 255, 255},
        .dir = 7,
        .px = 0,
        .py = 0,
        .pz = 0,
    },
    {
        .attr1 = 0,
        .dst_gthr = 5000,
        .way_gthr = 1,
        .atk_ptn = 1,
        .wspd = 4,
        .rspd = 8,
        .hp = 230,
        .atk_rng = 750,
        .hit_rng = 325,
        .chance_rng = 750,
        .hit_adjx = 0,
        .atk_p = 50,
        .atk_h = 150,
        .atk = 1,
        .atk_tm = 5,
        .mdl_no = 20,
        .anm_no = 11,
        .se_no = 1382,
        .adpcm_no = 1573,
        .dead_adpcm = -1,
        .point_base = 100,
        .hint_pic = 255,
        .aura_alp = 32,
        .area = {255, 255, 255, 255, 255, 255},
        .dir = 8,
        .px = 0,
        .py = 0,
        .pz = 0,
    },
    {
        .attr1 = 0,
        .dst_gthr = 5000,
        .way_gthr = 1,
        .atk_ptn = 2,
        .wspd = 0,
        .rspd = 0,
        .hp = 20,
        .atk_rng = 350,
        .hit_rng = 350,
        .chance_rng = 350,
        .hit_adjx = 0,
        .atk_p = 50,
        .atk_h = 100,
        .atk = 1,
        .atk_tm = 5,
        .mdl_no = 25,
        .anm_no = 16,
        .se_no = 1387,
        .adpcm_no = 1578,
        .dead_adpcm = -1,
        .point_base = 300,
        .hint_pic = 255,
        .aura_alp = 0,
        .area = {255, 255, 255, 255, 255, 255},
        .dir = 10,
        .px = 0,
        .py = 0,
        .pz = 0,
    },
};

AENE_INFO_DAT aene_info_dat1[] = {
    {
        .dat_no = 0,
        .soul_no = 0,
        .dir = 1000,
        .px = 23790,
        .py = 0,
        .pz = 10200,
        .adpcm_tm = 1,
        .adpcm_no = -1,
        .rng = 0,
        .mdl_no = 3,
        .anm_no = 41,
        .point_base = 400,
        .se_no = 1431,
        .se_foot = -1,
    },
    {
        .dat_no = 1,
        .soul_no = 255,
        .dir = 0,
        .px = 22300,
        .py = -1730,
        .pz = 12651,
        .adpcm_tm = 31,
        .adpcm_no = 1746,
        .rng = 0,
        .mdl_no = 3,
        .anm_no = 42,
        .point_base = 200,
        .se_no = 4294967295,
        .se_foot = -1,
    },
    {
        .dat_no = 2,
        .soul_no = 2,
        .dir = 25,
        .px = 26600,
        .py = 0,
        .pz = 13500,
        .adpcm_tm = 1,
        .adpcm_no = 1747,
        .rng = 0,
        .mdl_no = 3,
        .anm_no = 43,
        .point_base = 1250,
        .se_no = 1430,
        .se_foot = 1452,
    },
    {
        .dat_no = 3,
        .soul_no = 255,
        .dir = 180,
        .px = 30041,
        .py = 0,
        .pz = 19312,
        .adpcm_tm = 1,
        .adpcm_no = 1744,
        .rng = 0,
        .mdl_no = 10,
        .anm_no = 44,
        .point_base = 400,
        .se_no = 4294967295,
        .se_foot = -1,
    },
    {
        .dat_no = 4,
        .soul_no = 255,
        .dir = 90,
        .px = 28950,
        .py = 0,
        .pz = 15475,
        .adpcm_tm = 0,
        .adpcm_no = -1,
        .rng = 250,
        .mdl_no = 10,
        .anm_no = 44,
        .point_base = 500,
        .se_no = 1431,
        .se_foot = -1,
    },
    {
        .dat_no = 5,
        .soul_no = 255,
        .dir = 0,
        .px = 28750,
        .py = 0,
        .pz = 17000,
        .adpcm_tm = 1,
        .adpcm_no = 1741,
        .rng = 1000,
        .mdl_no = 3,
        .anm_no = 46,
        .point_base = 200,
        .se_no = 1441,
        .se_foot = 1461,
    },
    {
        .dat_no = 6,
        .soul_no = 255,
        .dir = 0,
        .px = 0,
        .py = 0,
        .pz = 0,
        .adpcm_tm = 0,
        .adpcm_no = -1,
        .rng = 1000,
        .mdl_no = 4,
        .anm_no = 47,
        .point_base = 0,
        .se_no = 4294967295,
        .se_foot = -1,
    },
    {
        .dat_no = 7,
        .soul_no = 255,
        .dir = 1000,
        .px = 15520,
        .py = 0,
        .pz = 25370,
        .adpcm_tm = 0,
        .adpcm_no = -1,
        .rng = 500,
        .mdl_no = 24,
        .anm_no = 48,
        .point_base = 1000,
        .se_no = 1433,
        .se_foot = -1,
    },
    {
        .dat_no = 8,
        .soul_no = 255,
        .dir = 0,
        .px = 23732,
        .py = 400,
        .pz = 20319,
        .adpcm_tm = 0,
        .adpcm_no = -1,
        .rng = 0,
        .mdl_no = 4,
        .anm_no = 49,
        .point_base = 0,
        .se_no = 4294967295,
        .se_foot = -1,
    },
    {
        .dat_no = 9,
        .soul_no = 255,
        .dir = 0,
        .px = 0,
        .py = 0,
        .pz = 0,
        .adpcm_tm = 0,
        .adpcm_no = -1,
        .rng = 0,
        .mdl_no = 4,
        .anm_no = 50,
        .point_base = 0,
        .se_no = 4294967295,
        .se_foot = -1,
    },
    {
        .dat_no = 10,
        .soul_no = 5,
        .dir = 320,
        .px = 25183,
        .py = 0,
        .pz = 25233,
        .adpcm_tm = 1,
        .adpcm_no = 1761,
        .rng = 500,
        .mdl_no = 33,
        .anm_no = 51,
        .point_base = 200,
        .se_no = 1435,
        .se_foot = -1,
    },
    {
        .dat_no = 11,
        .soul_no = 255,
        .dir = 180,
        .px = 29850,
        .py = -50,
        .pz = 20400,
        .adpcm_tm = 0,
        .adpcm_no = -1,
        .rng = 0,
        .mdl_no = 64,
        .anm_no = 52,
        .point_base = 700,
        .se_no = 1432,
        .se_foot = -1,
    },
    {
        .dat_no = 12,
        .soul_no = 6,
        .dir = 10,
        .px = 35000,
        .py = 400,
        .pz = 31500,
        .adpcm_tm = 1,
        .adpcm_no = 1724,
        .rng = 1000,
        .mdl_no = 5,
        .anm_no = 53,
        .point_base = 200,
        .se_no = 4294967295,
        .se_foot = 1466,
    },
    {
        .dat_no = 13,
        .soul_no = 255,
        .dir = 0,
        .px = 34080,
        .py = -1665,
        .pz = 27340,
        .adpcm_tm = 1,
        .adpcm_no = 1792,
        .rng = 0,
        .mdl_no = 6,
        .anm_no = 54,
        .point_base = 1000,
        .se_no = 1440,
        .se_foot = -1,
    },
    {
        .dat_no = 14,
        .soul_no = 7,
        .dir = 190,
        .px = 35350,
        .py = -1100,
        .pz = 42700,
        .adpcm_tm = 1,
        .adpcm_no = 1723,
        .rng = 1000,
        .mdl_no = 5,
        .anm_no = 55,
        .point_base = 200,
        .se_no = 4294967295,
        .se_foot = -1,
    },
    {
        .dat_no = 15,
        .soul_no = 255,
        .dir = 180,
        .px = 34625,
        .py = -3325,
        .pz = 46250,
        .adpcm_tm = 1,
        .adpcm_no = 1718,
        .rng = 0,
        .mdl_no = 47,
        .anm_no = 56,
        .point_base = 400,
        .se_no = 1433,
        .se_foot = -1,
    },
    {
        .dat_no = 16,
        .soul_no = 255,
        .dir = 0,
        .px = 27500,
        .py = -508,
        .pz = 13107,
        .adpcm_tm = 0,
        .adpcm_no = -1,
        .rng = 1000,
        .mdl_no = 1,
        .anm_no = 57,
        .point_base = 400,
        .se_no = 1442,
        .se_foot = 1450,
    },
    {
        .dat_no = 17,
        .soul_no = 255,
        .dir = 0,
        .px = 0,
        .py = 0,
        .pz = 0,
        .adpcm_tm = 0,
        .adpcm_no = -1,
        .rng = 0,
        .mdl_no = 10,
        .anm_no = 44,
        .point_base = 0,
        .se_no = 4294967295,
        .se_foot = -1,
    },
    {
        .dat_no = 18,
        .soul_no = 255,
        .dir = 1000,
        .px = 25000,
        .py = 0,
        .pz = 14750,
        .adpcm_tm = 0,
        .adpcm_no = -1,
        .rng = 100,
        .mdl_no = 10,
        .anm_no = 44,
        .point_base = 700,
        .se_no = 1433,
        .se_foot = -1,
    },
    {
        .dat_no = 19,
        .soul_no = 8,
        .dir = 0,
        .px = 20240,
        .py = -1400,
        .pz = 16980,
        .adpcm_tm = 1,
        .adpcm_no = 1729,
        .rng = 1000,
        .mdl_no = 4,
        .anm_no = 50,
        .point_base = 200,
        .se_no = 1434,
        .se_foot = -1,
    },
    {
        .dat_no = 20,
        .soul_no = 255,
        .dir = 270,
        .px = 16700,
        .py = 0,
        .pz = 17000,
        .adpcm_tm = 1,
        .adpcm_no = 1788,
        .rng = 750,
        .mdl_no = 6,
        .anm_no = 59,
        .point_base = 200,
        .se_no = 1440,
        .se_foot = -1,
    },
    {
        .dat_no = 21,
        .soul_no = 10,
        .dir = 180,
        .px = 22200,
        .py = 0,
        .pz = 16125,
        .adpcm_tm = 1,
        .adpcm_no = 1728,
        .rng = 1000,
        .mdl_no = 4,
        .anm_no = 60,
        .point_base = 400,
        .se_no = 1441,
        .se_foot = -1,
    },
    {
        .dat_no = 22,
        .soul_no = 255,
        .dir = 180,
        .px = 24229,
        .py = 0,
        .pz = 19990,
        .adpcm_tm = 0,
        .adpcm_no = -1,
        .rng = 0,
        .mdl_no = 5,
        .anm_no = 53,
        .point_base = 0,
        .se_no = 4294967295,
        .se_foot = -1,
    },
    {
        .dat_no = 23,
        .soul_no = 11,
        .dir = 145,
        .px = 30331,
        .py = 0,
        .pz = 19876,
        .adpcm_tm = 1,
        .adpcm_no = 1725,
        .rng = 1000,
        .mdl_no = 5,
        .anm_no = 61,
        .point_base = 200,
        .se_no = 1441,
        .se_foot = -1,
    },
    {
        .dat_no = 24,
        .soul_no = 255,
        .dir = 0,
        .px = 23030,
        .py = -2300,
        .pz = 21800,
        .adpcm_tm = 0,
        .adpcm_no = -1,
        .rng = 0,
        .mdl_no = 63,
        .anm_no = 62,
        .point_base = 1000,
        .se_no = 1437,
        .se_foot = -1,
    },
    {
        .dat_no = 25,
        .soul_no = 255,
        .dir = 0,
        .px = 0,
        .py = 0,
        .pz = 0,
        .adpcm_tm = 0,
        .adpcm_no = -1,
        .rng = 0,
        .mdl_no = 3,
        .anm_no = 63,
        .point_base = 0,
        .se_no = 4294967295,
        .se_foot = -1,
    },
    {
        .dat_no = 26,
        .soul_no = 255,
        .dir = 0,
        .px = 0,
        .py = 0,
        .pz = 0,
        .adpcm_tm = 0,
        .adpcm_no = -1,
        .rng = 0,
        .mdl_no = 10,
        .anm_no = 64,
        .point_base = 0,
        .se_no = 4294967295,
        .se_foot = -1,
    },
    {
        .dat_no = 2,
        .soul_no = 255,
        .dir = 10,
        .px = 26850,
        .py = 0,
        .pz = 11000,
        .adpcm_tm = 1,
        .adpcm_no = 1743,
        .rng = 0,
        .mdl_no = 3,
        .anm_no = 43,
        .point_base = 700,
        .se_no = 1434,
        .se_foot = 1452,
    },
    {
        .dat_no = 2,
        .soul_no = 255,
        .dir = 0,
        .px = 24900,
        .py = 0,
        .pz = 17000,
        .adpcm_tm = 1,
        .adpcm_no = 1747,
        .rng = 1000,
        .mdl_no = 3,
        .anm_no = 43,
        .point_base = 400,
        .se_no = 4294967295,
        .se_foot = 1461,
    },
    {
        .dat_no = 19,
        .soul_no = 255,
        .dir = 90,
        .px = 19900,
        .py = 0,
        .pz = 21125,
        .adpcm_tm = 1,
        .adpcm_no = 1726,
        .rng = 1000,
        .mdl_no = 4,
        .anm_no = 58,
        .point_base = 200,
        .se_no = 4294967295,
        .se_foot = 1461,
    },
    {
        .dat_no = 19,
        .soul_no = 13,
        .dir = 180,
        .px = 24100,
        .py = 0,
        .pz = 19900,
        .adpcm_tm = 1,
        .adpcm_no = 1727,
        .rng = 1000,
        .mdl_no = 4,
        .anm_no = 58,
        .point_base = 400,
        .se_no = 4294967295,
        .se_foot = 1452,
    },
    {
        .dat_no = 19,
        .soul_no = 255,
        .dir = 0,
        .px = 24000,
        .py = 400,
        .pz = 26200,
        .adpcm_tm = 1,
        .adpcm_no = 1731,
        .rng = 1000,
        .mdl_no = 4,
        .anm_no = 58,
        .point_base = 200,
        .se_no = 4294967295,
        .se_foot = 1460,
    },
    {
        .dat_no = 19,
        .soul_no = 255,
        .dir = 270,
        .px = 30600,
        .py = 0,
        .pz = 23700,
        .adpcm_tm = 1,
        .adpcm_no = 1735,
        .rng = 1000,
        .mdl_no = 4,
        .anm_no = 58,
        .point_base = 200,
        .se_no = 4294967295,
        .se_foot = 1452,
    },
    {
        .dat_no = 0,
        .soul_no = 255,
        .dir = 0,
        .px = 28530,
        .py = -500,
        .pz = 19166,
        .adpcm_tm = 0,
        .adpcm_no = -1,
        .rng = 0,
        .mdl_no = 9,
        .anm_no = 65,
        .point_base = 700,
        .se_no = 4294967295,
        .se_foot = -1,
    },
};

ENE_DAT jene_dat2[] = {
    {
        .attr1 = 0,
        .dst_gthr = 5000,
        .way_gthr = 1,
        .atk_ptn = 1,
        .wspd = 5,
        .rspd = 7,
        .hp = 200,
        .atk_rng = 670,
        .hit_rng = 456,
        .chance_rng = 670,
        .hit_adjx = 0,
        .atk_p = 50,
        .atk_h = 170,
        .atk = 1,
        .atk_tm = 5,
        .mdl_no = 24,
        .anm_no = 15,
        .se_no = 1366,
        .adpcm_no = 1577,
        .dead_adpcm = -1,
        .point_base = 200,
        .hint_pic = 255,
        .aura_alp = 32,
        .area = {16, 7, 255, 255, 255, 255},
        .dir = 90,
        .px = 21500,
        .py = 0,
        .pz = 19900,
    },
    {
        .attr1 = 64,
        .dst_gthr = 8000,
        .way_gthr = 1,
        .atk_ptn = 1,
        .wspd = 3,
        .rspd = 6,
        .hp = 300,
        .atk_rng = 1000,
        .hit_rng = 650,
        .chance_rng = 1000,
        .hit_adjx = 0,
        .atk_p = 50,
        .atk_h = 150,
        .atk = 1,
        .atk_tm = 8,
        .mdl_no = 13,
        .anm_no = 5,
        .se_no = 1381,
        .adpcm_no = 1567,
        .dead_adpcm = -1,
        .point_base = 250,
        .hint_pic = 255,
        .aura_alp = 32,
        .area = {17, 255, 255, 255, 255, 255},
        .dir = 1000,
        .px = 28800,
        .py = 0,
        .pz = 24000,
    },
    {
        .attr1 = 0,
        .dst_gthr = 5000,
        .way_gthr = 1,
        .atk_ptn = 0,
        .wspd = 5,
        .rspd = 6,
        .hp = 300,
        .atk_rng = 750,
        .hit_rng = 250,
        .chance_rng = 750,
        .hit_adjx = 0,
        .atk_p = 50,
        .atk_h = 150,
        .atk = 1,
        .atk_tm = 5,
        .mdl_no = 23,
        .anm_no = 14,
        .se_no = 1368,
        .adpcm_no = 1576,
        .dead_adpcm = 1596,
        .point_base = 200,
        .hint_pic = 17,
        .aura_alp = 32,
        .area = {21, 255, 255, 255, 255, 255},
        .dir = 270,
        .px = 35750,
        .py = 0,
        .pz = 31350,
    },
    {
        .attr1 = 0,
        .dst_gthr = 5000,
        .way_gthr = 1,
        .atk_ptn = 1,
        .wspd = 5,
        .rspd = 7,
        .hp = 250,
        .atk_rng = 670,
        .hit_rng = 456,
        .chance_rng = 670,
        .hit_adjx = 0,
        .atk_p = 50,
        .atk_h = 170,
        .atk = 1,
        .atk_tm = 5,
        .mdl_no = 24,
        .anm_no = 15,
        .se_no = 1366,
        .adpcm_no = 1577,
        .dead_adpcm = -1,
        .point_base = 300,
        .hint_pic = 255,
        .aura_alp = 32,
        .area = {17, 16, 255, 255, 255, 255},
        .dir = 180,
        .px = 29600,
        .py = -700,
        .pz = 22500,
    },
    {
        .attr1 = 0,
        .dst_gthr = 5000,
        .way_gthr = 1,
        .atk_ptn = 1,
        .wspd = 5,
        .rspd = 7,
        .hp = 250,
        .atk_rng = 670,
        .hit_rng = 456,
        .chance_rng = 670,
        .hit_adjx = 0,
        .atk_p = 50,
        .atk_h = 170,
        .atk = 1,
        .atk_tm = 5,
        .mdl_no = 24,
        .anm_no = 15,
        .se_no = 1366,
        .adpcm_no = 1577,
        .dead_adpcm = -1,
        .point_base = 300,
        .hint_pic = 19,
        .aura_alp = 32,
        .area = {29, 255, 255, 255, 255, 255},
        .dir = 0,
        .px = 8000,
        .py = 3000,
        .pz = 36500,
    },
    {
        .attr1 = 0,
        .dst_gthr = 5000,
        .way_gthr = 1,
        .atk_ptn = 1,
        .wspd = 4,
        .rspd = 7,
        .hp = 250,
        .atk_rng = 1000,
        .hit_rng = 500,
        .chance_rng = 1000,
        .hit_adjx = 0,
        .atk_p = 50,
        .atk_h = 150,
        .atk = 1,
        .atk_tm = 5,
        .mdl_no = 28,
        .anm_no = 18,
        .se_no = 1386,
        .adpcm_no = 1580,
        .dead_adpcm = -1,
        .point_base = 100,
        .hint_pic = 255,
        .aura_alp = 32,
        .area = {27, 15, 255, 255, 255, 255},
        .dir = 90,
        .px = 15750,
        .py = 0,
        .pz = 23700,
    },
    {
        .attr1 = 0,
        .dst_gthr = 5000,
        .way_gthr = 1,
        .atk_ptn = 1,
        .wspd = 5,
        .rspd = 7,
        .hp = 300,
        .atk_rng = 670,
        .hit_rng = 456,
        .chance_rng = 670,
        .hit_adjx = 0,
        .atk_p = 50,
        .atk_h = 170,
        .atk = 1,
        .atk_tm = 5,
        .mdl_no = 24,
        .anm_no = 15,
        .se_no = 1366,
        .adpcm_no = 1577,
        .dead_adpcm = -1,
        .point_base = 300,
        .hint_pic = 255,
        .aura_alp = 32,
        .area = {7, 16, 15, 27, 255, 255},
        .dir = 270,
        .px = 19200,
        .py = 0,
        .pz = 20400,
    },
    {
        .attr1 = 0,
        .dst_gthr = 8000,
        .way_gthr = 1,
        .atk_ptn = 2,
        .wspd = 3,
        .rspd = 6,
        .hp = 300,
        .atk_rng = 715,
        .hit_rng = 415,
        .chance_rng = 715,
        .hit_adjx = -50,
        .atk_p = 50,
        .atk_h = 150,
        .atk = 1,
        .atk_tm = 5,
        .mdl_no = 18,
        .anm_no = 9,
        .se_no = 1367,
        .adpcm_no = 1571,
        .dead_adpcm = 1594,
        .point_base = 250,
        .hint_pic = 255,
        .aura_alp = 24,
        .area = {40, 255, 255, 255, 255, 255},
        .dir = 1000,
        .px = 23000,
        .py = 0,
        .pz = 15900,
    },
    {
        .attr1 = 0,
        .dst_gthr = 5000,
        .way_gthr = 1,
        .atk_ptn = 1,
        .wspd = 3,
        .rspd = 7,
        .hp = 300,
        .atk_rng = 700,
        .hit_rng = 300,
        .chance_rng = 700,
        .hit_adjx = 100,
        .atk_p = 50,
        .atk_h = 150,
        .atk = 1,
        .atk_tm = 5,
        .mdl_no = 22,
        .anm_no = 13,
        .se_no = 1369,
        .adpcm_no = 1575,
        .dead_adpcm = 1595,
        .point_base = 300,
        .hint_pic = 255,
        .aura_alp = 32,
        .area = {3, 255, 255, 255, 255, 255},
        .dir = 1000,
        .px = 27850,
        .py = 0,
        .pz = 12000,
    },
    {
        .attr1 = 0,
        .dst_gthr = 5000,
        .way_gthr = 1,
        .atk_ptn = 0,
        .wspd = 5,
        .rspd = 7,
        .hp = 400,
        .atk_rng = 710,
        .hit_rng = 429,
        .chance_rng = 710,
        .hit_adjx = 0,
        .atk_p = 50,
        .atk_h = 150,
        .atk = 1,
        .atk_tm = 5,
        .mdl_no = 14,
        .anm_no = 6,
        .se_no = 1383,
        .adpcm_no = 1568,
        .dead_adpcm = -1,
        .point_base = 300,
        .hint_pic = 255,
        .aura_alp = 32,
        .area = {1, 255, 255, 255, 255, 255},
        .dir = 90,
        .px = 15800,
        .py = 0,
        .pz = 12500,
    },
    {
        .attr1 = 0,
        .dst_gthr = 5000,
        .way_gthr = 1,
        .atk_ptn = 1,
        .wspd = 5,
        .rspd = 7,
        .hp = 400,
        .atk_rng = 670,
        .hit_rng = 456,
        .chance_rng = 670,
        .hit_adjx = 0,
        .atk_p = 50,
        .atk_h = 200,
        .atk = 1,
        .atk_tm = 5,
        .mdl_no = 24,
        .anm_no = 15,
        .se_no = 1366,
        .adpcm_no = 1577,
        .dead_adpcm = -1,
        .point_base = 300,
        .hint_pic = 255,
        .aura_alp = 32,
        .area = {2, 3, 255, 255, 255, 255},
        .dir = 180,
        .px = 22300,
        .py = 0,
        .pz = 14500,
    },
    {
        .attr1 = 0,
        .dst_gthr = 5000,
        .way_gthr = 1,
        .atk_ptn = 1,
        .wspd = 5,
        .rspd = 7,
        .hp = 400,
        .atk_rng = 670,
        .hit_rng = 456,
        .chance_rng = 670,
        .hit_adjx = 0,
        .atk_p = 50,
        .atk_h = 200,
        .atk = 1,
        .atk_tm = 5,
        .mdl_no = 24,
        .anm_no = 15,
        .se_no = 1366,
        .adpcm_no = 1577,
        .dead_adpcm = -1,
        .point_base = 300,
        .hint_pic = 255,
        .aura_alp = 32,
        .area = {8, 38, 3, 255, 255, 255},
        .dir = 0,
        .px = 20378,
        .py = -1400,
        .pz = 14300,
    },
    {
        .attr1 = 0,
        .dst_gthr = 5000,
        .way_gthr = 1,
        .atk_ptn = 1,
        .wspd = 4,
        .rspd = 8,
        .hp = 300,
        .atk_rng = 750,
        .hit_rng = 325,
        .chance_rng = 750,
        .hit_adjx = 0,
        .atk_p = 50,
        .atk_h = 150,
        .atk = 1,
        .atk_tm = 5,
        .mdl_no = 20,
        .anm_no = 11,
        .se_no = 1382,
        .adpcm_no = 1573,
        .dead_adpcm = -1,
        .point_base = 150,
        .hint_pic = 255,
        .aura_alp = 32,
        .area = {8, 1, 9, 38, 7, 255},
        .dir = 0,
        .px = 20375,
        .py = 0,
        .pz = 15025,
    },
    {
        .attr1 = 0,
        .dst_gthr = 5000,
        .way_gthr = 1,
        .atk_ptn = 1,
        .wspd = 5,
        .rspd = 7,
        .hp = 400,
        .atk_rng = 670,
        .hit_rng = 456,
        .chance_rng = 670,
        .hit_adjx = 0,
        .atk_p = 50,
        .atk_h = 200,
        .atk = 1,
        .atk_tm = 5,
        .mdl_no = 24,
        .anm_no = 15,
        .se_no = 1366,
        .adpcm_no = 1577,
        .dead_adpcm = -1,
        .point_base = 400,
        .hint_pic = 255,
        .aura_alp = 32,
        .area = {5, 4, 255, 255, 255, 255},
        .dir = 270,
        .px = 32000,
        .py = -400,
        .pz = 13000,
    },
    {
        .attr1 = 0,
        .dst_gthr = 5000,
        .way_gthr = 1,
        .atk_ptn = 0,
        .wspd = 3,
        .rspd = 7,
        .hp = 600,
        .atk_rng = 900,
        .hit_rng = 350,
        .chance_rng = 900,
        .hit_adjx = 100,
        .atk_p = 50,
        .atk_h = 200,
        .atk = 1,
        .atk_tm = 7,
        .mdl_no = 16,
        .anm_no = 8,
        .se_no = 1370,
        .adpcm_no = 1570,
        .dead_adpcm = -1,
        .point_base = 500,
        .hint_pic = 255,
        .aura_alp = 32,
        .area = {14, 255, 255, 255, 255, 255},
        .dir = 90,
        .px = 31400,
        .py = 0,
        .pz = 23150,
    },
    {
        .attr1 = 0,
        .dst_gthr = 5000,
        .way_gthr = 1,
        .atk_ptn = 1,
        .wspd = 5,
        .rspd = 7,
        .hp = 500,
        .atk_rng = 670,
        .hit_rng = 456,
        .chance_rng = 670,
        .hit_adjx = 0,
        .atk_p = 50,
        .atk_h = 200,
        .atk = 1,
        .atk_tm = 5,
        .mdl_no = 24,
        .anm_no = 15,
        .se_no = 1366,
        .adpcm_no = 1577,
        .dead_adpcm = -1,
        .point_base = 400,
        .hint_pic = 255,
        .aura_alp = 32,
        .area = {8, 17, 3, 255, 255, 255},
        .dir = 0,
        .px = 0,
        .py = 0,
        .pz = 0,
    },
    {
        .attr1 = 0,
        .dst_gthr = 5000,
        .way_gthr = 1,
        .atk_ptn = 0,
        .wspd = 9,
        .rspd = 10,
        .hp = 600,
        .atk_rng = 600,
        .hit_rng = 250,
        .chance_rng = 600,
        .hit_adjx = 0,
        .atk_p = 100,
        .atk_h = 180,
        .atk = 1,
        .atk_tm = 8,
        .mdl_no = 33,
        .anm_no = 21,
        .se_no = 1371,
        .adpcm_no = 1583,
        .dead_adpcm = 1599,
        .point_base = 400,
        .hint_pic = 29,
        .aura_alp = 32,
        .area = {16, 255, 255, 255, 255, 255},
        .dir = 1000,
        .px = 25183,
        .py = 0,
        .pz = 25233,
    },
    {
        .attr1 = 0,
        .dst_gthr = 5000,
        .way_gthr = 1,
        .atk_ptn = 0,
        .wspd = 4,
        .rspd = 8,
        .hp = 500,
        .atk_rng = 900,
        .hit_rng = 350,
        .chance_rng = 900,
        .hit_adjx = 0,
        .atk_p = 50,
        .atk_h = 220,
        .atk = 1,
        .atk_tm = 7,
        .mdl_no = 16,
        .anm_no = 8,
        .se_no = 1370,
        .adpcm_no = 1570,
        .dead_adpcm = 1593,
        .point_base = 500,
        .hint_pic = 255,
        .aura_alp = 32,
        .area = {29, 255, 255, 255, 255, 255},
        .dir = 270,
        .px = 11800,
        .py = 0,
        .pz = 34075,
    },
    {
        .attr1 = 0,
        .dst_gthr = 5000,
        .way_gthr = 1,
        .atk_ptn = 1,
        .wspd = 5,
        .rspd = 7,
        .hp = 600,
        .atk_rng = 670,
        .hit_rng = 456,
        .chance_rng = 670,
        .hit_adjx = 0,
        .atk_p = 100,
        .atk_h = 200,
        .atk = 1,
        .atk_tm = 5,
        .mdl_no = 24,
        .anm_no = 15,
        .se_no = 1366,
        .adpcm_no = 1577,
        .dead_adpcm = 1597,
        .point_base = 550,
        .hint_pic = 255,
        .aura_alp = 32,
        .area = {29, 255, 255, 255, 255, 255},
        .dir = 1000,
        .px = 10050,
        .py = 3000,
        .pz = 36575,
    },
    {
        .attr1 = 0,
        .dst_gthr = 8000,
        .way_gthr = 1,
        .atk_ptn = 1,
        .wspd = 6,
        .rspd = 8,
        .hp = 500,
        .atk_rng = 600,
        .hit_rng = 250,
        .chance_rng = 600,
        .hit_adjx = 0,
        .atk_p = 500,
        .atk_h = 500,
        .atk = 1,
        .atk_tm = 8,
        .mdl_no = 40,
        .anm_no = 28,
        .se_no = 1380,
        .adpcm_no = 1586,
        .dead_adpcm = -1,
        .point_base = 500,
        .hint_pic = 255,
        .aura_alp = 32,
        .area = {28, 29, 255, 255, 255, 255},
        .dir = 1000,
        .px = 12700,
        .py = 3000,
        .pz = 36900,
    },
    {
        .attr1 = 0,
        .dst_gthr = 8000,
        .way_gthr = 1,
        .atk_ptn = 1,
        .wspd = 6,
        .rspd = 8,
        .hp = 500,
        .atk_rng = 500,
        .hit_rng = 200,
        .chance_rng = 500,
        .hit_adjx = 0,
        .atk_p = 500,
        .atk_h = 500,
        .atk = 1,
        .atk_tm = 8,
        .mdl_no = 40,
        .anm_no = 28,
        .se_no = 1380,
        .adpcm_no = 1586,
        .dead_adpcm = -1,
        .point_base = 500,
        .hint_pic = 255,
        .aura_alp = 32,
        .area = {255, 255, 255, 255, 255, 255},
        .dir = 1000,
        .px = 19050,
        .py = 0,
        .pz = 30325,
    },
};

ENE_DAT fene_dat2[] = {
    {
        .attr1 = 0,
        .dst_gthr = 5000,
        .way_gthr = 1,
        .atk_ptn = 1,
        .wspd = 3,
        .rspd = 7,
        .hp = 250,
        .atk_rng = 700,
        .hit_rng = 500,
        .chance_rng = 700,
        .hit_adjx = 0,
        .atk_p = 30,
        .atk_h = 100,
        .atk = 1,
        .atk_tm = 4,
        .mdl_no = 19,
        .anm_no = 10,
        .se_no = 1385,
        .adpcm_no = 1578,
        .dead_adpcm = -1,
        .point_base = 50,
        .hint_pic = 255,
        .aura_alp = 32,
        .area = {255, 255, 255, 255, 255, 255},
        .dir = 0,
        .px = 0,
        .py = 0,
        .pz = 0,
    },
    {
        .attr1 = 0,
        .dst_gthr = 5000,
        .way_gthr = 1,
        .atk_ptn = 1,
        .wspd = 3,
        .rspd = 8,
        .hp = 300,
        .atk_rng = 750,
        .hit_rng = 325,
        .chance_rng = 750,
        .hit_adjx = 0,
        .atk_p = 50,
        .atk_h = 150,
        .atk = 1,
        .atk_tm = 5,
        .mdl_no = 20,
        .anm_no = 11,
        .se_no = 1382,
        .adpcm_no = 1573,
        .dead_adpcm = -1,
        .point_base = 100,
        .hint_pic = 255,
        .aura_alp = 32,
        .area = {255, 255, 255, 255, 255, 255},
        .dir = 0,
        .px = 0,
        .py = 0,
        .pz = 0,
    },
    {
        .attr1 = 0,
        .dst_gthr = 5000,
        .way_gthr = 1,
        .atk_ptn = 0,
        .wspd = 7,
        .rspd = 8,
        .hp = 180,
        .atk_rng = 900,
        .hit_rng = 120,
        .chance_rng = 500,
        .hit_adjx = 0,
        .atk_p = 30,
        .atk_h = 100,
        .atk = 1,
        .atk_tm = 4,
        .mdl_no = 21,
        .anm_no = 12,
        .se_no = 1384,
        .adpcm_no = 1574,
        .dead_adpcm = -1,
        .point_base = 50,
        .hint_pic = 255,
        .aura_alp = 16,
        .area = {255, 255, 255, 255, 255, 255},
        .dir = 0,
        .px = 0,
        .py = 0,
        .pz = 0,
    },
    {
        .attr1 = 0,
        .dst_gthr = 5000,
        .way_gthr = 1,
        .atk_ptn = 0,
        .wspd = 5,
        .rspd = 7,
        .hp = 250,
        .atk_rng = 1000,
        .hit_rng = 350,
        .chance_rng = 1000,
        .hit_adjx = 0,
        .atk_p = 50,
        .atk_h = 100,
        .atk = 1,
        .atk_tm = 5,
        .mdl_no = 27,
        .anm_no = 17,
        .se_no = 1389,
        .adpcm_no = 1579,
        .dead_adpcm = -1,
        .point_base = 100,
        .hint_pic = 255,
        .aura_alp = 32,
        .area = {255, 255, 255, 255, 255, 255},
        .dir = 0,
        .px = 0,
        .py = 0,
        .pz = 0,
    },
    {
        .attr1 = 0,
        .dst_gthr = 5000,
        .way_gthr = 1,
        .atk_ptn = 1,
        .wspd = 4,
        .rspd = 10,
        .hp = 300,
        .atk_rng = 1000,
        .hit_rng = 500,
        .chance_rng = 1000,
        .hit_adjx = 0,
        .atk_p = 50,
        .atk_h = 150,
        .atk = 1,
        .atk_tm = 5,
        .mdl_no = 28,
        .anm_no = 18,
        .se_no = 1386,
        .adpcm_no = 1580,
        .dead_adpcm = -1,
        .point_base = 150,
        .hint_pic = 255,
        .aura_alp = 32,
        .area = {255, 255, 255, 255, 255, 255},
        .dir = 0,
        .px = 0,
        .py = 0,
        .pz = 0,
    },
    {
        .attr1 = 0,
        .dst_gthr = 5000,
        .way_gthr = 1,
        .atk_ptn = 0,
        .wspd = 7,
        .rspd = 8,
        .hp = 180,
        .atk_rng = 900,
        .hit_rng = 120,
        .chance_rng = 500,
        .hit_adjx = 0,
        .atk_p = 30,
        .atk_h = 100,
        .atk = 1,
        .atk_tm = 4,
        .mdl_no = 21,
        .anm_no = 12,
        .se_no = 1384,
        .adpcm_no = 1574,
        .dead_adpcm = -1,
        .point_base = 50,
        .hint_pic = 255,
        .aura_alp = 16,
        .area = {255, 255, 255, 255, 255, 255},
        .dir = 0,
        .px = 0,
        .py = 0,
        .pz = 0,
    },
    {
        .attr1 = 0,
        .dst_gthr = 5000,
        .way_gthr = 1,
        .atk_ptn = 1,
        .wspd = 3,
        .rspd = 8,
        .hp = 300,
        .atk_rng = 750,
        .hit_rng = 325,
        .chance_rng = 750,
        .hit_adjx = 0,
        .atk_p = 50,
        .atk_h = 150,
        .atk = 1,
        .atk_tm = 5,
        .mdl_no = 20,
        .anm_no = 11,
        .se_no = 1382,
        .adpcm_no = 1573,
        .dead_adpcm = -1,
        .point_base = 100,
        .hint_pic = 255,
        .aura_alp = 32,
        .area = {255, 255, 255, 255, 255, 255},
        .dir = 7,
        .px = 0,
        .py = 0,
        .pz = 0,
    },
    {
        .attr1 = 0,
        .dst_gthr = 5000,
        .way_gthr = 1,
        .atk_ptn = 1,
        .wspd = 3,
        .rspd = 8,
        .hp = 300,
        .atk_rng = 750,
        .hit_rng = 325,
        .chance_rng = 750,
        .hit_adjx = 0,
        .atk_p = 50,
        .atk_h = 150,
        .atk = 1,
        .atk_tm = 5,
        .mdl_no = 20,
        .anm_no = 11,
        .se_no = 1382,
        .adpcm_no = 1573,
        .dead_adpcm = -1,
        .point_base = 100,
        .hint_pic = 255,
        .aura_alp = 32,
        .area = {255, 255, 255, 255, 255, 255},
        .dir = 8,
        .px = 0,
        .py = 0,
        .pz = 0,
    },
    {
        .attr1 = 0,
        .dst_gthr = 5000,
        .way_gthr = 1,
        .atk_ptn = 2,
        .wspd = 0,
        .rspd = 0,
        .hp = 20,
        .atk_rng = 350,
        .hit_rng = 350,
        .chance_rng = 350,
        .hit_adjx = 0,
        .atk_p = 50,
        .atk_h = 100,
        .atk = 1,
        .atk_tm = 5,
        .mdl_no = 25,
        .anm_no = 16,
        .se_no = 1387,
        .adpcm_no = 1578,
        .dead_adpcm = -1,
        .point_base = 300,
        .hint_pic = 255,
        .aura_alp = 0,
        .area = {255, 255, 255, 255, 255, 255},
        .dir = 10,
        .px = 0,
        .py = 0,
        .pz = 0,
    },
};

AENE_INFO_DAT aene_info_dat2[] = {
    {
        .dat_no = 0,
        .soul_no = 255,
        .dir = 90,
        .px = 20300,
        .py = -200,
        .pz = 19900,
        .adpcm_tm = 1,
        .adpcm_no = 1774,
        .rng = 0,
        .mdl_no = 29,
        .anm_no = 66,
        .point_base = 1100,
        .se_no = 1435,
        .se_foot = 1455,
    },
    {
        .dat_no = 1,
        .soul_no = 255,
        .dir = 0,
        .px = 30500,
        .py = 0,
        .pz = 21000,
        .adpcm_tm = 1,
        .adpcm_no = 1776,
        .rng = 1000,
        .mdl_no = 29,
        .anm_no = 66,
        .point_base = 800,
        .se_no = 1435,
        .se_foot = 1455,
    },
    {
        .dat_no = 10,
        .soul_no = 16,
        .dir = 90,
        .px = 11197,
        .py = 3000,
        .pz = 35840,
        .adpcm_tm = 1,
        .adpcm_no = 1749,
        .rng = 1000,
        .mdl_no = 26,
        .anm_no = 73,
        .point_base = 300,
        .se_no = 1438,
        .se_foot = -1,
    },
    {
        .dat_no = 1,
        .soul_no = 255,
        .dir = 338,
        .px = 29400,
        .py = 0,
        .pz = 28000,
        .adpcm_tm = 1,
        .adpcm_no = 1777,
        .rng = 1000,
        .mdl_no = 29,
        .anm_no = 66,
        .point_base = 500,
        .se_no = 1435,
        .se_foot = 1469,
    },
    {
        .dat_no = 9,
        .soul_no = 18,
        .dir = 280,
        .px = 25000,
        .py = -1400,
        .pz = 17800,
        .adpcm_tm = 1,
        .adpcm_no = 1754,
        .rng = 1000,
        .mdl_no = 26,
        .anm_no = 72,
        .point_base = 800,
        .se_no = 4294967295,
        .se_foot = 1461,
    },
    {
        .dat_no = 2,
        .soul_no = 255,
        .dir = 0,
        .px = 10000,
        .py = 0,
        .pz = 31500,
        .adpcm_tm = 0,
        .adpcm_no = -1,
        .rng = 1000,
        .mdl_no = 1,
        .anm_no = 67,
        .point_base = 800,
        .se_no = 4294967295,
        .se_foot = 1449,
    },
    {
        .dat_no = 3,
        .soul_no = 255,
        .dir = 0,
        .px = 24180,
        .py = 350,
        .pz = 20100,
        .adpcm_tm = 1,
        .adpcm_no = 1767,
        .rng = 0,
        .mdl_no = 18,
        .anm_no = 68,
        .point_base = 1100,
        .se_no = 1432,
        .se_foot = 1464,
    },
    {
        .dat_no = 4,
        .soul_no = 255,
        .dir = 0,
        .px = 25100,
        .py = -1400,
        .pz = 20100,
        .adpcm_tm = 0,
        .adpcm_no = -1,
        .rng = 0,
        .mdl_no = 26,
        .anm_no = 69,
        .point_base = 500,
        .se_no = 1431,
        .se_foot = 1455,
    },
    {
        .dat_no = 5,
        .soul_no = 255,
        .dir = 0,
        .px = 10000,
        .py = 0,
        .pz = 34150,
        .adpcm_tm = 0,
        .adpcm_no = -1,
        .rng = 0,
        .mdl_no = 24,
        .anm_no = 48,
        .point_base = 0,
        .se_no = 4294967295,
        .se_foot = -1,
    },
    {
        .dat_no = 6,
        .soul_no = 255,
        .dir = 1000,
        .px = 17000,
        .py = 0,
        .pz = 20500,
        .adpcm_tm = 1,
        .adpcm_no = 1788,
        .rng = 1000,
        .mdl_no = 6,
        .anm_no = 70,
        .point_base = 300,
        .se_no = 1440,
        .se_foot = -1,
    },
    {
        .dat_no = 1,
        .soul_no = 255,
        .dir = 0,
        .px = 26000,
        .py = 0,
        .pz = 13800,
        .adpcm_tm = 1,
        .adpcm_no = 1781,
        .rng = 1000,
        .mdl_no = 29,
        .anm_no = 66,
        .point_base = 1500,
        .se_no = 1435,
        .se_foot = 1461,
    },
    {
        .dat_no = 7,
        .soul_no = 20,
        .dir = 0,
        .px = 32225,
        .py = 0,
        .pz = 10200,
        .adpcm_tm = 0,
        .adpcm_no = -1,
        .rng = 1000,
        .mdl_no = 26,
        .anm_no = 71,
        .point_base = 300,
        .se_no = 1441,
        .se_foot = -1,
    },
    {
        .dat_no = 8,
        .soul_no = 21,
        .dir = 180,
        .px = 34050,
        .py = 0,
        .pz = 17200,
        .adpcm_tm = 0,
        .adpcm_no = -1,
        .rng = 0,
        .mdl_no = 33,
        .anm_no = 51,
        .point_base = 0,
        .se_no = 4294967295,
        .se_foot = -1,
    },
    {
        .dat_no = 9,
        .soul_no = 255,
        .dir = 90,
        .px = 22850,
        .py = -1400,
        .pz = 18000,
        .adpcm_tm = 1,
        .adpcm_no = 1756,
        .rng = 1000,
        .mdl_no = 26,
        .anm_no = 72,
        .point_base = 500,
        .se_no = 4294967295,
        .se_foot = 1461,
    },
    {
        .dat_no = 9,
        .soul_no = 255,
        .dir = 45,
        .px = 24000,
        .py = -1400,
        .pz = 18000,
        .adpcm_tm = 1,
        .adpcm_no = 1759,
        .rng = 750,
        .mdl_no = 26,
        .anm_no = 72,
        .point_base = 300,
        .se_no = 1434,
        .se_foot = 1461,
    },
    {
        .dat_no = 8,
        .soul_no = 23,
        .dir = 320,
        .px = 25183,
        .py = 0,
        .pz = 25233,
        .adpcm_tm = 31,
        .adpcm_no = 1762,
        .rng = 500,
        .mdl_no = 33,
        .anm_no = 51,
        .point_base = 500,
        .se_no = 1442,
        .se_foot = -1,
    },
    {
        .dat_no = 9,
        .soul_no = 24,
        .dir = 270,
        .px = 29200,
        .py = 0,
        .pz = 10100,
        .adpcm_tm = 1,
        .adpcm_no = 1757,
        .rng = 1000,
        .mdl_no = 26,
        .anm_no = 72,
        .point_base = 500,
        .se_no = 4294967295,
        .se_foot = 1455,
    },
};


ENE_DAT jene_dat3[] = {
    {
        .attr1 = 0,
        .dst_gthr = 8000,
        .way_gthr = 1,
        .atk_ptn = 1,
        .wspd = 3,
        .rspd = 7,
        .hp = 600,
        .atk_rng = 900,
        .hit_rng = 600,
        .chance_rng = 900,
        .hit_adjx = 0,
        .atk_p = 150,
        .atk_h = 200,
        .atk = 1,
        .atk_tm = 6,
        .mdl_no = 37,
        .anm_no = 25,
        .se_no = 1377,
        .adpcm_no = 1584,
        .dead_adpcm = -1,
        .point_base = 400,
        .hint_pic = 255,
        .aura_alp = 32,
        .area = {14, 255, 255, 255, 255, 255},
        .dir = 0,
        .px = 33300,
        .py = 0,
        .pz = 21000,
    },
    {
        .attr1 = 0,
        .dst_gthr = 8000,
        .way_gthr = 1,
        .atk_ptn = 1,
        .wspd = 3,
        .rspd = 7,
        .hp = 500,
        .atk_rng = 700,
        .hit_rng = 365,
        .chance_rng = 700,
        .hit_adjx = 0,
        .atk_p = 100,
        .atk_h = 200,
        .atk = 1,
        .atk_tm = 6,
        .mdl_no = 32,
        .anm_no = 20,
        .se_no = 1373,
        .adpcm_no = 1582,
        .dead_adpcm = 1601,
        .point_base = 400,
        .hint_pic = 42,
        .aura_alp = 32,
        .area = {6, 255, 255, 255, 255, 255},
        .dir = 90,
        .px = 33300,
        .py = 400,
        .pz = 16500,
    },
    {
        .attr1 = 0,
        .dst_gthr = 8000,
        .way_gthr = 1,
        .atk_ptn = 1,
        .wspd = 3,
        .rspd = 7,
        .hp = 500,
        .atk_rng = 600,
        .hit_rng = 365,
        .chance_rng = 600,
        .hit_adjx = 0,
        .atk_p = 100,
        .atk_h = 200,
        .atk = 1,
        .atk_tm = 6,
        .mdl_no = 34,
        .anm_no = 22,
        .se_no = 1374,
        .adpcm_no = 1582,
        .dead_adpcm = 1602,
        .point_base = 400,
        .hint_pic = 44,
        .aura_alp = 32,
        .area = {22, 255, 255, 255, 255, 255},
        .dir = 225,
        .px = 20400,
        .py = 0,
        .pz = 38300,
    },
    {
        .attr1 = 0,
        .dst_gthr = 8000,
        .way_gthr = 1,
        .atk_ptn = 1,
        .wspd = 3,
        .rspd = 9,
        .hp = 500,
        .atk_rng = 700,
        .hit_rng = 305,
        .chance_rng = 700,
        .hit_adjx = 0,
        .atk_p = 100,
        .atk_h = 200,
        .atk = 1,
        .atk_tm = 6,
        .mdl_no = 35,
        .anm_no = 23,
        .se_no = 1375,
        .adpcm_no = 1582,
        .dead_adpcm = 1600,
        .point_base = 400,
        .hint_pic = 43,
        .aura_alp = 32,
        .area = {21, 255, 255, 255, 255, 255},
        .dir = 180,
        .px = 33200,
        .py = 0,
        .pz = 32800,
    },
    {
        .attr1 = 0,
        .dst_gthr = 8000,
        .way_gthr = 1,
        .atk_ptn = 1,
        .wspd = 5,
        .rspd = 9,
        .hp = 500,
        .atk_rng = 700,
        .hit_rng = 305,
        .chance_rng = 700,
        .hit_adjx = 0,
        .atk_p = 100,
        .atk_h = 200,
        .atk = 1,
        .atk_tm = 6,
        .mdl_no = 36,
        .anm_no = 24,
        .se_no = 1376,
        .adpcm_no = 1582,
        .dead_adpcm = 1598,
        .point_base = 400,
        .hint_pic = 45,
        .aura_alp = 32,
        .area = {16, 255, 255, 255, 255, 255},
        .dir = 270,
        .px = 25500,
        .py = 0,
        .pz = 22800,
    },
    {
        .attr1 = 0,
        .dst_gthr = 8000,
        .way_gthr = 1,
        .atk_ptn = 1,
        .wspd = 3,
        .rspd = 7,
        .hp = 800,
        .atk_rng = 900,
        .hit_rng = 600,
        .chance_rng = 900,
        .hit_adjx = 0,
        .atk_p = 150,
        .atk_h = 200,
        .atk = 1,
        .atk_tm = 6,
        .mdl_no = 37,
        .anm_no = 25,
        .se_no = 1377,
        .adpcm_no = 1584,
        .dead_adpcm = 1604,
        .point_base = 600,
        .hint_pic = 255,
        .aura_alp = 32,
        .area = {1, 255, 255, 255, 255, 255},
        .dir = 270,
        .px = 20200,
        .py = 0,
        .pz = 12400,
    },
    {
        .attr1 = 0,
        .dst_gthr = 8000,
        .way_gthr = 1,
        .atk_ptn = 1,
        .wspd = 6,
        .rspd = 8,
        .hp = 500,
        .atk_rng = 600,
        .hit_rng = 250,
        .chance_rng = 600,
        .hit_adjx = 0,
        .atk_p = 500,
        .atk_h = 500,
        .atk = 1,
        .atk_tm = 8,
        .mdl_no = 40,
        .anm_no = 28,
        .se_no = 1380,
        .adpcm_no = 1586,
        .dead_adpcm = -1,
        .point_base = 400,
        .hint_pic = 255,
        .aura_alp = 32,
        .area = {255, 255, 255, 255, 255, 255},
        .dir = 0,
        .px = 0,
        .py = 0,
        .pz = 0,
    },
    {
        .attr1 = 0,
        .dst_gthr = 8000,
        .way_gthr = 1,
        .atk_ptn = 0,
        .wspd = 4,
        .rspd = 5,
        .hp = 400,
        .atk_rng = 700,
        .hit_rng = 380,
        .chance_rng = 700,
        .hit_adjx = 0,
        .atk_p = 50,
        .atk_h = 150,
        .atk = 1,
        .atk_tm = 8,
        .mdl_no = 15,
        .anm_no = 7,
        .se_no = 1372,
        .adpcm_no = 1569,
        .dead_adpcm = -1,
        .point_base = 300,
        .hint_pic = 255,
        .aura_alp = 32,
        .area = {255, 255, 255, 255, 255, 255},
        .dir = 0,
        .px = 0,
        .py = 0,
        .pz = 0,
    },
    {
        .attr1 = 0,
        .dst_gthr = 5000,
        .way_gthr = 1,
        .atk_ptn = 1,
        .wspd = 4,
        .rspd = 10,
        .hp = 300,
        .atk_rng = 1000,
        .hit_rng = 500,
        .chance_rng = 1000,
        .hit_adjx = 0,
        .atk_p = 50,
        .atk_h = 150,
        .atk = 1,
        .atk_tm = 5,
        .mdl_no = 28,
        .anm_no = 18,
        .se_no = 1386,
        .adpcm_no = 1580,
        .dead_adpcm = -1,
        .point_base = 200,
        .hint_pic = 255,
        .aura_alp = 32,
        .area = {255, 255, 255, 255, 255, 255},
        .dir = 0,
        .px = 0,
        .py = 0,
        .pz = 0,
    },
};

ENE_DAT fene_dat3[] = {
    {
        .attr1 = 0,
        .dst_gthr = 5000,
        .way_gthr = 1,
        .atk_ptn = 0,
        .wspd = 4,
        .rspd = 5,
        .hp = 450,
        .atk_rng = 600,
        .hit_rng = 429,
        .chance_rng = 600,
        .hit_adjx = 0,
        .atk_p = 70,
        .atk_h = 170,
        .atk = 1,
        .atk_tm = 5,
        .mdl_no = 14,
        .anm_no = 6,
        .se_no = 1383,
        .adpcm_no = 1568,
        .dead_adpcm = -1,
        .point_base = 300,
        .hint_pic = 255,
        .aura_alp = 32,
        .area = {255, 255, 255, 255, 255, 255},
        .dir = 0,
        .px = 0,
        .py = 0,
        .pz = 0,
    },
    {
        .attr1 = 0,
        .dst_gthr = 8000,
        .way_gthr = 1,
        .atk_ptn = 0,
        .wspd = 4,
        .rspd = 5,
        .hp = 400,
        .atk_rng = 700,
        .hit_rng = 380,
        .chance_rng = 700,
        .hit_adjx = 0,
        .atk_p = 50,
        .atk_h = 150,
        .atk = 1,
        .atk_tm = 8,
        .mdl_no = 15,
        .anm_no = 7,
        .se_no = 1372,
        .adpcm_no = 1569,
        .dead_adpcm = -1,
        .point_base = 300,
        .hint_pic = 255,
        .aura_alp = 32,
        .area = {255, 255, 255, 255, 255, 255},
        .dir = 0,
        .px = 0,
        .py = 0,
        .pz = 0,
    },
    {
        .attr1 = 0,
        .dst_gthr = 5000,
        .way_gthr = 1,
        .atk_ptn = 0,
        .wspd = 5,
        .rspd = 7,
        .hp = 300,
        .atk_rng = 1000,
        .hit_rng = 350,
        .chance_rng = 1000,
        .hit_adjx = 0,
        .atk_p = 50,
        .atk_h = 100,
        .atk = 1,
        .atk_tm = 5,
        .mdl_no = 27,
        .anm_no = 17,
        .se_no = 1389,
        .adpcm_no = 1579,
        .dead_adpcm = -1,
        .point_base = 200,
        .hint_pic = 255,
        .aura_alp = 32,
        .area = {255, 255, 255, 255, 255, 255},
        .dir = 0,
        .px = 0,
        .py = 0,
        .pz = 0,
    },
    {
        .attr1 = 0,
        .dst_gthr = 5000,
        .way_gthr = 1,
        .atk_ptn = 1,
        .wspd = 4,
        .rspd = 10,
        .hp = 350,
        .atk_rng = 1000,
        .hit_rng = 500,
        .chance_rng = 1000,
        .hit_adjx = 0,
        .atk_p = 50,
        .atk_h = 150,
        .atk = 1,
        .atk_tm = 5,
        .mdl_no = 28,
        .anm_no = 18,
        .se_no = 1386,
        .adpcm_no = 1580,
        .dead_adpcm = -1,
        .point_base = 200,
        .hint_pic = 255,
        .aura_alp = 32,
        .area = {255, 255, 255, 255, 255, 255},
        .dir = 0,
        .px = 0,
        .py = 0,
        .pz = 0,
    },
    {
        .attr1 = 16,
        .dst_gthr = 5000,
        .way_gthr = 1,
        .atk_ptn = 1,
        .wspd = 4,
        .rspd = 6,
        .hp = 300,
        .atk_rng = 750,
        .hit_rng = 250,
        .chance_rng = 750,
        .hit_adjx = 0,
        .atk_p = 50,
        .atk_h = 150,
        .atk = 1,
        .atk_tm = 5,
        .mdl_no = 31,
        .anm_no = 19,
        .se_no = 1388,
        .adpcm_no = 1581,
        .dead_adpcm = -1,
        .point_base = 250,
        .hint_pic = 255,
        .aura_alp = 0,
        .area = {255, 255, 255, 255, 255, 255},
        .dir = 0,
        .px = 0,
        .py = 0,
        .pz = 0,
    },
    {
        .attr1 = 0,
        .dst_gthr = 5000,
        .way_gthr = 1,
        .atk_ptn = 1,
        .wspd = 3,
        .rspd = 8,
        .hp = 350,
        .atk_rng = 750,
        .hit_rng = 325,
        .chance_rng = 750,
        .hit_adjx = 0,
        .atk_p = 50,
        .atk_h = 150,
        .atk = 1,
        .atk_tm = 5,
        .mdl_no = 20,
        .anm_no = 11,
        .se_no = 1382,
        .adpcm_no = 1573,
        .dead_adpcm = -1,
        .point_base = 150,
        .hint_pic = 255,
        .aura_alp = 32,
        .area = {255, 255, 255, 255, 255, 255},
        .dir = 0,
        .px = 0,
        .py = 0,
        .pz = 0,
    },
    {
        .attr1 = 0,
        .dst_gthr = 5000,
        .way_gthr = 1,
        .atk_ptn = 1,
        .wspd = 3,
        .rspd = 7,
        .hp = 300,
        .atk_rng = 700,
        .hit_rng = 500,
        .chance_rng = 700,
        .hit_adjx = 0,
        .atk_p = 30,
        .atk_h = 100,
        .atk = 1,
        .atk_tm = 4,
        .mdl_no = 19,
        .anm_no = 10,
        .se_no = 1385,
        .adpcm_no = 1578,
        .dead_adpcm = -1,
        .point_base = 100,
        .hint_pic = 255,
        .aura_alp = 32,
        .area = {255, 255, 255, 255, 255, 255},
        .dir = 0,
        .px = 0,
        .py = 0,
        .pz = 0,
    },
    {
        .attr1 = 0,
        .dst_gthr = 5000,
        .way_gthr = 1,
        .atk_ptn = 0,
        .wspd = 7,
        .rspd = 8,
        .hp = 150,
        .atk_rng = 900,
        .hit_rng = 120,
        .chance_rng = 500,
        .hit_adjx = 0,
        .atk_p = 30,
        .atk_h = 100,
        .atk = 1,
        .atk_tm = 4,
        .mdl_no = 21,
        .anm_no = 12,
        .se_no = 1384,
        .adpcm_no = 1574,
        .dead_adpcm = -1,
        .point_base = 100,
        .hint_pic = 255,
        .aura_alp = 16,
        .area = {255, 255, 255, 255, 255, 255},
        .dir = 0,
        .px = 0,
        .py = 0,
        .pz = 0,
    },
    {
        .attr1 = 0,
        .dst_gthr = 5000,
        .way_gthr = 1,
        .atk_ptn = 0,
        .wspd = 7,
        .rspd = 8,
        .hp = 150,
        .atk_rng = 900,
        .hit_rng = 120,
        .chance_rng = 500,
        .hit_adjx = 0,
        .atk_p = 30,
        .atk_h = 100,
        .atk = 1,
        .atk_tm = 4,
        .mdl_no = 21,
        .anm_no = 12,
        .se_no = 1384,
        .adpcm_no = 1574,
        .dead_adpcm = -1,
        .point_base = 100,
        .hint_pic = 255,
        .aura_alp = 16,
        .area = {255, 255, 255, 255, 255, 255},
        .dir = 0,
        .px = 0,
        .py = 0,
        .pz = 0,
    },
    {
        .attr1 = 0,
        .dst_gthr = 5000,
        .way_gthr = 1,
        .atk_ptn = 1,
        .wspd = 3,
        .rspd = 7,
        .hp = 400,
        .atk_rng = 700,
        .hit_rng = 500,
        .chance_rng = 700,
        .hit_adjx = 0,
        .atk_p = 30,
        .atk_h = 100,
        .atk = 1,
        .atk_tm = 4,
        .mdl_no = 58,
        .anm_no = 34,
        .se_no = 1385,
        .adpcm_no = 1572,
        .dead_adpcm = -1,
        .point_base = 100,
        .hint_pic = 255,
        .aura_alp = 32,
        .area = {255, 255, 255, 255, 255, 255},
        .dir = 0,
        .px = 0,
        .py = 0,
        .pz = 0,
    },
    {
        .attr1 = 0,
        .dst_gthr = 5000,
        .way_gthr = 1,
        .atk_ptn = 1,
        .wspd = 3,
        .rspd = 7,
        .hp = 200,
        .atk_rng = 700,
        .hit_rng = 500,
        .chance_rng = 700,
        .hit_adjx = 0,
        .atk_p = 30,
        .atk_h = 100,
        .atk = 1,
        .atk_tm = 4,
        .mdl_no = 38,
        .anm_no = 26,
        .se_no = 1378,
        .adpcm_no = 1585,
        .dead_adpcm = -1,
        .point_base = 200,
        .hint_pic = 255,
        .aura_alp = 32,
        .area = {255, 255, 255, 255, 255, 255},
        .dir = 0,
        .px = 0,
        .py = 0,
        .pz = 0,
    },
    {
        .attr1 = 128,
        .dst_gthr = 5000,
        .way_gthr = 1,
        .atk_ptn = 2,
        .wspd = 3,
        .rspd = 7,
        .hp = 250,
        .atk_rng = 660,
        .hit_rng = 360,
        .chance_rng = 660,
        .hit_adjx = 0,
        .atk_p = 30,
        .atk_h = 100,
        .atk = 1,
        .atk_tm = 4,
        .mdl_no = 42,
        .anm_no = 30,
        .se_no = 1379,
        .adpcm_no = 1587,
        .dead_adpcm = -1,
        .point_base = 100,
        .hint_pic = 255,
        .aura_alp = 32,
        .area = {255, 255, 255, 255, 255, 255},
        .dir = 0,
        .px = 0,
        .py = 0,
        .pz = 0,
    },
    {
        .attr1 = 128,
        .dst_gthr = 5000,
        .way_gthr = 1,
        .atk_ptn = 2,
        .wspd = 3,
        .rspd = 7,
        .hp = 250,
        .atk_rng = 660,
        .hit_rng = 360,
        .chance_rng = 660,
        .hit_adjx = 0,
        .atk_p = 30,
        .atk_h = 100,
        .atk = 1,
        .atk_tm = 4,
        .mdl_no = 43,
        .anm_no = 30,
        .se_no = 1379,
        .adpcm_no = 1587,
        .dead_adpcm = -1,
        .point_base = 100,
        .hint_pic = 255,
        .aura_alp = 32,
        .area = {255, 255, 255, 255, 255, 255},
        .dir = 0,
        .px = 0,
        .py = 0,
        .pz = 0,
    },
    {
        .attr1 = 128,
        .dst_gthr = 5000,
        .way_gthr = 1,
        .atk_ptn = 2,
        .wspd = 3,
        .rspd = 7,
        .hp = 250,
        .atk_rng = 660,
        .hit_rng = 360,
        .chance_rng = 660,
        .hit_adjx = 0,
        .atk_p = 30,
        .atk_h = 100,
        .atk = 1,
        .atk_tm = 4,
        .mdl_no = 44,
        .anm_no = 30,
        .se_no = 1379,
        .adpcm_no = 1587,
        .dead_adpcm = -1,
        .point_base = 100,
        .hint_pic = 255,
        .aura_alp = 32,
        .area = {255, 255, 255, 255, 255, 255},
        .dir = 0,
        .px = 0,
        .py = 0,
        .pz = 0,
    },
    {
        .attr1 = 0,
        .dst_gthr = 8000,
        .way_gthr = 1,
        .atk_ptn = 0,
        .wspd = 4,
        .rspd = 5,
        .hp = 400,
        .atk_rng = 700,
        .hit_rng = 380,
        .chance_rng = 700,
        .hit_adjx = 0,
        .atk_p = 50,
        .atk_h = 150,
        .atk = 1,
        .atk_tm = 8,
        .mdl_no = 15,
        .anm_no = 7,
        .se_no = 1372,
        .adpcm_no = 1569,
        .dead_adpcm = -1,
        .point_base = 300,
        .hint_pic = 255,
        .aura_alp = 32,
        .area = {255, 255, 255, 255, 255, 255},
        .dir = 7,
        .px = 0,
        .py = 0,
        .pz = 0,
    },
    {
        .attr1 = 0,
        .dst_gthr = 8000,
        .way_gthr = 1,
        .atk_ptn = 0,
        .wspd = 4,
        .rspd = 5,
        .hp = 400,
        .atk_rng = 700,
        .hit_rng = 380,
        .chance_rng = 700,
        .hit_adjx = 0,
        .atk_p = 50,
        .atk_h = 150,
        .atk = 1,
        .atk_tm = 8,
        .mdl_no = 15,
        .anm_no = 7,
        .se_no = 1372,
        .adpcm_no = 1569,
        .dead_adpcm = -1,
        .point_base = 300,
        .hint_pic = 255,
        .aura_alp = 32,
        .area = {255, 255, 255, 255, 255, 255},
        .dir = 9,
        .px = 0,
        .py = 0,
        .pz = 0,
    },
    {
        .attr1 = 16,
        .dst_gthr = 5000,
        .way_gthr = 1,
        .atk_ptn = 1,
        .wspd = 4,
        .rspd = 7,
        .hp = 300,
        .atk_rng = 750,
        .hit_rng = 250,
        .chance_rng = 750,
        .hit_adjx = 0,
        .atk_p = 50,
        .atk_h = 150,
        .atk = 1,
        .atk_tm = 5,
        .mdl_no = 31,
        .anm_no = 19,
        .se_no = 1388,
        .adpcm_no = 1581,
        .dead_adpcm = -1,
        .point_base = 250,
        .hint_pic = 255,
        .aura_alp = 0,
        .area = {255, 255, 255, 255, 255, 255},
        .dir = 7,
        .px = 0,
        .py = 0,
        .pz = 0,
    },
    {
        .attr1 = 0,
        .dst_gthr = 5000,
        .way_gthr = 1,
        .atk_ptn = 1,
        .wspd = 3,
        .rspd = 7,
        .hp = 400,
        .atk_rng = 700,
        .hit_rng = 500,
        .chance_rng = 700,
        .hit_adjx = 0,
        .atk_p = 30,
        .atk_h = 100,
        .atk = 1,
        .atk_tm = 4,
        .mdl_no = 58,
        .anm_no = 34,
        .se_no = 1385,
        .adpcm_no = 1572,
        .dead_adpcm = -1,
        .point_base = 200,
        .hint_pic = 255,
        .aura_alp = 32,
        .area = {255, 255, 255, 255, 255, 255},
        .dir = 8,
        .px = 0,
        .py = 0,
        .pz = 0,
    },
    {
        .attr1 = 0,
        .dst_gthr = 5000,
        .way_gthr = 1,
        .atk_ptn = 1,
        .wspd = 3,
        .rspd = 7,
        .hp = 300,
        .atk_rng = 700,
        .hit_rng = 500,
        .chance_rng = 700,
        .hit_adjx = 0,
        .atk_p = 30,
        .atk_h = 100,
        .atk = 1,
        .atk_tm = 4,
        .mdl_no = 38,
        .anm_no = 26,
        .se_no = 1378,
        .adpcm_no = 1585,
        .dead_adpcm = -1,
        .point_base = 200,
        .hint_pic = 255,
        .aura_alp = 32,
        .area = {255, 255, 255, 255, 255, 255},
        .dir = 6,
        .px = 0,
        .py = 0,
        .pz = 0,
    },
    {
        .attr1 = 0,
        .dst_gthr = 5000,
        .way_gthr = 1,
        .atk_ptn = 2,
        .wspd = 0,
        .rspd = 0,
        .hp = 20,
        .atk_rng = 350,
        .hit_rng = 350,
        .chance_rng = 350,
        .hit_adjx = 0,
        .atk_p = 50,
        .atk_h = 100,
        .atk = 1,
        .atk_tm = 5,
        .mdl_no = 25,
        .anm_no = 16,
        .se_no = 1387,
        .adpcm_no = 1578,
        .dead_adpcm = -1,
        .point_base = 300,
        .hint_pic = 255,
        .aura_alp = 0,
        .area = {255, 255, 255, 255, 255, 255},
        .dir = 10,
        .px = 0,
        .py = 0,
        .pz = 0,
    },
};

AENE_INFO_DAT aene_info_dat3[] = {
    {
        .dat_no = 0,
        .soul_no = 255,
        .dir = 90,
        .px = 19900,
        .py = 0,
        .pz = 21150,
        .adpcm_tm = 0,
        .adpcm_no = -1,
        .rng = 1000,
        .mdl_no = 59,
        .anm_no = 74,
        .point_base = 700,
        .se_no = 4294967295,
        .se_foot = 1461,
    },
    {
        .dat_no = 1,
        .soul_no = 255,
        .dir = 90,
        .px = 28000,
        .py = 0,
        .pz = 19700,
        .adpcm_tm = 0,
        .adpcm_no = -1,
        .rng = 1000,
        .mdl_no = 59,
        .anm_no = 75,
        .point_base = 1000,
        .se_no = 1438,
        .se_foot = -1,
    },
};


ENE_DAT jene_dat4[] = {
    {
        .attr1 = 0,
        .dst_gthr = 8000,
        .way_gthr = 1,
        .atk_ptn = 1,
        .wspd = 3,
        .rspd = 7,
        .hp = 150,
        .atk_rng = 900,
        .hit_rng = 120,
        .chance_rng = 500,
        .hit_adjx = 0,
        .atk_p = 50,
        .atk_h = 100,
        .atk = 1,
        .atk_tm = 6,
        .mdl_no = 64,
        .anm_no = 37,
        .se_no = 1384,
        .adpcm_no = 1574,
        .dead_adpcm = -1,
        .point_base = 200,
        .hint_pic = 255,
        .aura_alp = 16,
        .area = {20, 35, 255, 255, 255, 255},
        .dir = 90,
        .px = 26400,
        .py = -2800,
        .pz = 17100,
    },
    {
        .attr1 = 0,
        .dst_gthr = 8000,
        .way_gthr = 1,
        .atk_ptn = 1,
        .wspd = 3,
        .rspd = 7,
        .hp = 500,
        .atk_rng = 600,
        .hit_rng = 250,
        .chance_rng = 600,
        .hit_adjx = 0,
        .atk_p = 500,
        .atk_h = 500,
        .atk = 1,
        .atk_tm = 6,
        .mdl_no = 40,
        .anm_no = 28,
        .se_no = 1380,
        .adpcm_no = 1586,
        .dead_adpcm = -1,
        .point_base = 500,
        .hint_pic = 255,
        .aura_alp = 32,
        .area = {18, 17, 14, 255, 255, 255},
        .dir = 270,
        .px = 33750,
        .py = -1925,
        .pz = 24400,
    },
    {
        .attr1 = 0,
        .dst_gthr = 8000,
        .way_gthr = 1,
        .atk_ptn = 1,
        .wspd = 2,
        .rspd = 7,
        .hp = 700,
        .atk_rng = 600,
        .hit_rng = 250,
        .chance_rng = 600,
        .hit_adjx = 0,
        .atk_p = 500,
        .atk_h = 500,
        .atk = 1,
        .atk_tm = 6,
        .mdl_no = 40,
        .anm_no = 28,
        .se_no = 1380,
        .adpcm_no = 1546,
        .dead_adpcm = -1,
        .point_base = 700,
        .hint_pic = 255,
        .aura_alp = 32,
        .area = {34, 255, 255, 255, 255, 255},
        .dir = 1000,
        .px = 45749,
        .py = 3400,
        .pz = 10447,
    },
    {
        .attr1 = 0,
        .dst_gthr = 8000,
        .way_gthr = 1,
        .atk_ptn = 1,
        .wspd = 6,
        .rspd = 8,
        .hp = 500,
        .atk_rng = 600,
        .hit_rng = 250,
        .chance_rng = 600,
        .hit_adjx = 0,
        .atk_p = 500,
        .atk_h = 500,
        .atk = 1,
        .atk_tm = 8,
        .mdl_no = 40,
        .anm_no = 28,
        .se_no = 1380,
        .adpcm_no = 1586,
        .dead_adpcm = -1,
        .point_base = 400,
        .hint_pic = 255,
        .aura_alp = 32,
        .area = {255, 255, 255, 255, 255, 255},
        .dir = 0,
        .px = 0,
        .py = 0,
        .pz = 0,
    },
    {
        .attr1 = 0,
        .dst_gthr = 5000,
        .way_gthr = 1,
        .atk_ptn = 0,
        .wspd = 5,
        .rspd = 7,
        .hp = 300,
        .atk_rng = 1000,
        .hit_rng = 350,
        .chance_rng = 1000,
        .hit_adjx = 0,
        .atk_p = 50,
        .atk_h = 150,
        .atk = 1,
        .atk_tm = 5,
        .mdl_no = 27,
        .anm_no = 17,
        .se_no = 1389,
        .adpcm_no = 1579,
        .dead_adpcm = -1,
        .point_base = 300,
        .hint_pic = 255,
        .aura_alp = 32,
        .area = {30, 255, 255, 255, 255, 255},
        .dir = 1000,
        .px = 26500,
        .py = 3000,
        .pz = 21850,
    },
    {
        .attr1 = 0,
        .dst_gthr = 5000,
        .way_gthr = 1,
        .atk_ptn = 1,
        .wspd = 4,
        .rspd = 10,
        .hp = 350,
        .atk_rng = 1000,
        .hit_rng = 500,
        .chance_rng = 1000,
        .hit_adjx = 0,
        .atk_p = 50,
        .atk_h = 200,
        .atk = 1,
        .atk_tm = 5,
        .mdl_no = 28,
        .anm_no = 18,
        .se_no = 1386,
        .adpcm_no = 1580,
        .dead_adpcm = -1,
        .point_base = 400,
        .hint_pic = 255,
        .aura_alp = 32,
        .area = {30, 255, 255, 255, 255, 255},
        .dir = 1000,
        .px = 24500,
        .py = 3000,
        .pz = 21000,
    },
    {
        .attr1 = 0,
        .dst_gthr = 5000,
        .way_gthr = 1,
        .atk_ptn = 2,
        .wspd = 3,
        .rspd = 7,
        .hp = 160,
        .atk_rng = 700,
        .hit_rng = 500,
        .chance_rng = 700,
        .hit_adjx = 0,
        .atk_p = 100,
        .atk_h = 200,
        .atk = 1,
        .atk_tm = 4,
        .mdl_no = 38,
        .anm_no = 26,
        .se_no = 1378,
        .adpcm_no = 1585,
        .dead_adpcm = -1,
        .point_base = 300,
        .hint_pic = 255,
        .aura_alp = 32,
        .area = {32, 255, 255, 255, 255, 255},
        .dir = 1000,
        .px = 32000,
        .py = 3000,
        .pz = 19000,
    },
    {
        .attr1 = 0,
        .dst_gthr = 5000,
        .way_gthr = 1,
        .atk_ptn = 2,
        .wspd = 8,
        .rspd = 10,
        .hp = 350,
        .atk_rng = 700,
        .hit_rng = 500,
        .chance_rng = 700,
        .hit_adjx = 0,
        .atk_p = 150,
        .atk_h = 220,
        .atk = 1,
        .atk_tm = 4,
        .mdl_no = 38,
        .anm_no = 26,
        .se_no = 1378,
        .adpcm_no = 1585,
        .dead_adpcm = -1,
        .point_base = 300,
        .hint_pic = 255,
        .aura_alp = 32,
        .area = {33, 255, 255, 255, 255, 255},
        .dir = 1000,
        .px = 34000,
        .py = 3400,
        .pz = 15600,
    },
    {
        .attr1 = 0,
        .dst_gthr = 8000,
        .way_gthr = 1,
        .atk_ptn = 1,
        .wspd = 3,
        .rspd = 5,
        .hp = 700,
        .atk_rng = 600,
        .hit_rng = 250,
        .chance_rng = 600,
        .hit_adjx = 0,
        .atk_p = 500,
        .atk_h = 500,
        .atk = 1,
        .atk_tm = 6,
        .mdl_no = 40,
        .anm_no = 28,
        .se_no = 1380,
        .adpcm_no = 1546,
        .dead_adpcm = -1,
        .point_base = 500,
        .hint_pic = 255,
        .aura_alp = 32,
        .area = {34, 33, 255, 255, 255, 255},
        .dir = 1000,
        .px = 42920,
        .py = 3400,
        .pz = 9100,
    },
};

ENE_DAT fene_dat4[] = {
    {
        .attr1 = 0,
        .dst_gthr = 5000,
        .way_gthr = 1,
        .atk_ptn = 0,
        .wspd = 4,
        .rspd = 5,
        .hp = 450,
        .atk_rng = 600,
        .hit_rng = 429,
        .chance_rng = 600,
        .hit_adjx = 0,
        .atk_p = 50,
        .atk_h = 150,
        .atk = 1,
        .atk_tm = 5,
        .mdl_no = 14,
        .anm_no = 6,
        .se_no = 1383,
        .adpcm_no = 1568,
        .dead_adpcm = -1,
        .point_base = 400,
        .hint_pic = 255,
        .aura_alp = 32,
        .area = {255, 255, 255, 255, 255, 255},
        .dir = 0,
        .px = 0,
        .py = 0,
        .pz = 0,
    },
    {
        .attr1 = 0,
        .dst_gthr = 8000,
        .way_gthr = 1,
        .atk_ptn = 0,
        .wspd = 4,
        .rspd = 5,
        .hp = 400,
        .atk_rng = 700,
        .hit_rng = 380,
        .chance_rng = 700,
        .hit_adjx = 0,
        .atk_p = 50,
        .atk_h = 150,
        .atk = 1,
        .atk_tm = 8,
        .mdl_no = 15,
        .anm_no = 7,
        .se_no = 1372,
        .adpcm_no = 1569,
        .dead_adpcm = -1,
        .point_base = 400,
        .hint_pic = 255,
        .aura_alp = 32,
        .area = {255, 255, 255, 255, 255, 255},
        .dir = 0,
        .px = 0,
        .py = 0,
        .pz = 0,
    },
    {
        .attr1 = 16,
        .dst_gthr = 5000,
        .way_gthr = 1,
        .atk_ptn = 1,
        .wspd = 4,
        .rspd = 6,
        .hp = 300,
        .atk_rng = 750,
        .hit_rng = 250,
        .chance_rng = 750,
        .hit_adjx = 0,
        .atk_p = 50,
        .atk_h = 150,
        .atk = 1,
        .atk_tm = 5,
        .mdl_no = 31,
        .anm_no = 19,
        .se_no = 1388,
        .adpcm_no = 1581,
        .dead_adpcm = -1,
        .point_base = 300,
        .hint_pic = 255,
        .aura_alp = 0,
        .area = {255, 255, 255, 255, 255, 255},
        .dir = 0,
        .px = 0,
        .py = 0,
        .pz = 0,
    },
    {
        .attr1 = 0,
        .dst_gthr = 5000,
        .way_gthr = 1,
        .atk_ptn = 1,
        .wspd = 3,
        .rspd = 8,
        .hp = 400,
        .atk_rng = 750,
        .hit_rng = 325,
        .chance_rng = 750,
        .hit_adjx = 0,
        .atk_p = 50,
        .atk_h = 150,
        .atk = 1,
        .atk_tm = 5,
        .mdl_no = 20,
        .anm_no = 11,
        .se_no = 1382,
        .adpcm_no = 1573,
        .dead_adpcm = -1,
        .point_base = 300,
        .hint_pic = 255,
        .aura_alp = 32,
        .area = {255, 255, 255, 255, 255, 255},
        .dir = 0,
        .px = 0,
        .py = 0,
        .pz = 0,
    },
    {
        .attr1 = 0,
        .dst_gthr = 5000,
        .way_gthr = 1,
        .atk_ptn = 1,
        .wspd = 3,
        .rspd = 7,
        .hp = 300,
        .atk_rng = 700,
        .hit_rng = 500,
        .chance_rng = 700,
        .hit_adjx = 0,
        .atk_p = 50,
        .atk_h = 150,
        .atk = 1,
        .atk_tm = 4,
        .mdl_no = 19,
        .anm_no = 10,
        .se_no = 1385,
        .adpcm_no = 1578,
        .dead_adpcm = -1,
        .point_base = 300,
        .hint_pic = 255,
        .aura_alp = 32,
        .area = {255, 255, 255, 255, 255, 255},
        .dir = 0,
        .px = 0,
        .py = 0,
        .pz = 0,
    },
    {
        .attr1 = 0,
        .dst_gthr = 5000,
        .way_gthr = 1,
        .atk_ptn = 0,
        .wspd = 7,
        .rspd = 8,
        .hp = 150,
        .atk_rng = 900,
        .hit_rng = 120,
        .chance_rng = 500,
        .hit_adjx = 0,
        .atk_p = 30,
        .atk_h = 100,
        .atk = 1,
        .atk_tm = 4,
        .mdl_no = 21,
        .anm_no = 12,
        .se_no = 1384,
        .adpcm_no = 1574,
        .dead_adpcm = -1,
        .point_base = 100,
        .hint_pic = 255,
        .aura_alp = 16,
        .area = {255, 255, 255, 255, 255, 255},
        .dir = 0,
        .px = 0,
        .py = 0,
        .pz = 0,
    },
    {
        .attr1 = 0,
        .dst_gthr = 5000,
        .way_gthr = 1,
        .atk_ptn = 0,
        .wspd = 7,
        .rspd = 8,
        .hp = 150,
        .atk_rng = 900,
        .hit_rng = 120,
        .chance_rng = 500,
        .hit_adjx = 0,
        .atk_p = 30,
        .atk_h = 100,
        .atk = 1,
        .atk_tm = 4,
        .mdl_no = 21,
        .anm_no = 12,
        .se_no = 1384,
        .adpcm_no = 1574,
        .dead_adpcm = -1,
        .point_base = 100,
        .hint_pic = 255,
        .aura_alp = 16,
        .area = {255, 255, 255, 255, 255, 255},
        .dir = 0,
        .px = 0,
        .py = 0,
        .pz = 0,
    },
    {
        .attr1 = 0,
        .dst_gthr = 5000,
        .way_gthr = 1,
        .atk_ptn = 1,
        .wspd = 3,
        .rspd = 7,
        .hp = 400,
        .atk_rng = 700,
        .hit_rng = 500,
        .chance_rng = 700,
        .hit_adjx = 0,
        .atk_p = 50,
        .atk_h = 150,
        .atk = 1,
        .atk_tm = 4,
        .mdl_no = 58,
        .anm_no = 34,
        .se_no = 1385,
        .adpcm_no = 1572,
        .dead_adpcm = -1,
        .point_base = 300,
        .hint_pic = 255,
        .aura_alp = 32,
        .area = {255, 255, 255, 255, 255, 255},
        .dir = 0,
        .px = 0,
        .py = 0,
        .pz = 0,
    },
    {
        .attr1 = 128,
        .dst_gthr = 5000,
        .way_gthr = 1,
        .atk_ptn = 2,
        .wspd = 3,
        .rspd = 7,
        .hp = 250,
        .atk_rng = 660,
        .hit_rng = 360,
        .chance_rng = 660,
        .hit_adjx = 0,
        .atk_p = 30,
        .atk_h = 100,
        .atk = 1,
        .atk_tm = 4,
        .mdl_no = 42,
        .anm_no = 30,
        .se_no = 1379,
        .adpcm_no = 1587,
        .dead_adpcm = -1,
        .point_base = 100,
        .hint_pic = 255,
        .aura_alp = 32,
        .area = {255, 255, 255, 255, 255, 255},
        .dir = 0,
        .px = 0,
        .py = 0,
        .pz = 0,
    },
    {
        .attr1 = 128,
        .dst_gthr = 5000,
        .way_gthr = 1,
        .atk_ptn = 2,
        .wspd = 3,
        .rspd = 7,
        .hp = 250,
        .atk_rng = 660,
        .hit_rng = 360,
        .chance_rng = 660,
        .hit_adjx = 0,
        .atk_p = 30,
        .atk_h = 100,
        .atk = 1,
        .atk_tm = 4,
        .mdl_no = 43,
        .anm_no = 31,
        .se_no = 1379,
        .adpcm_no = 1587,
        .dead_adpcm = -1,
        .point_base = 100,
        .hint_pic = 255,
        .aura_alp = 32,
        .area = {255, 255, 255, 255, 255, 255},
        .dir = 0,
        .px = 0,
        .py = 0,
        .pz = 0,
    },
    {
        .attr1 = 128,
        .dst_gthr = 5000,
        .way_gthr = 1,
        .atk_ptn = 2,
        .wspd = 3,
        .rspd = 7,
        .hp = 250,
        .atk_rng = 660,
        .hit_rng = 360,
        .chance_rng = 660,
        .hit_adjx = 0,
        .atk_p = 30,
        .atk_h = 100,
        .atk = 1,
        .atk_tm = 4,
        .mdl_no = 44,
        .anm_no = 32,
        .se_no = 1379,
        .adpcm_no = 1587,
        .dead_adpcm = -1,
        .point_base = 100,
        .hint_pic = 255,
        .aura_alp = 32,
        .area = {255, 255, 255, 255, 255, 255},
        .dir = 0,
        .px = 0,
        .py = 0,
        .pz = 0,
    },
    {
        .attr1 = 0,
        .dst_gthr = 8000,
        .way_gthr = 1,
        .atk_ptn = 0,
        .wspd = 4,
        .rspd = 5,
        .hp = 400,
        .atk_rng = 700,
        .hit_rng = 380,
        .chance_rng = 700,
        .hit_adjx = 0,
        .atk_p = 50,
        .atk_h = 150,
        .atk = 1,
        .atk_tm = 8,
        .mdl_no = 15,
        .anm_no = 7,
        .se_no = 1372,
        .adpcm_no = 1569,
        .dead_adpcm = -1,
        .point_base = 400,
        .hint_pic = 255,
        .aura_alp = 32,
        .area = {255, 255, 255, 255, 255, 255},
        .dir = 7,
        .px = 0,
        .py = 0,
        .pz = 0,
    },
    {
        .attr1 = 0,
        .dst_gthr = 8000,
        .way_gthr = 1,
        .atk_ptn = 0,
        .wspd = 4,
        .rspd = 5,
        .hp = 400,
        .atk_rng = 700,
        .hit_rng = 380,
        .chance_rng = 700,
        .hit_adjx = 0,
        .atk_p = 50,
        .atk_h = 150,
        .atk = 1,
        .atk_tm = 8,
        .mdl_no = 15,
        .anm_no = 7,
        .se_no = 1372,
        .adpcm_no = 1569,
        .dead_adpcm = -1,
        .point_base = 400,
        .hint_pic = 255,
        .aura_alp = 32,
        .area = {255, 255, 255, 255, 255, 255},
        .dir = 9,
        .px = 0,
        .py = 0,
        .pz = 0,
    },
    {
        .attr1 = 16,
        .dst_gthr = 5000,
        .way_gthr = 1,
        .atk_ptn = 1,
        .wspd = 4,
        .rspd = 7,
        .hp = 300,
        .atk_rng = 750,
        .hit_rng = 250,
        .chance_rng = 750,
        .hit_adjx = 0,
        .atk_p = 50,
        .atk_h = 150,
        .atk = 1,
        .atk_tm = 5,
        .mdl_no = 31,
        .anm_no = 19,
        .se_no = 1388,
        .adpcm_no = 1581,
        .dead_adpcm = -1,
        .point_base = 300,
        .hint_pic = 255,
        .aura_alp = 0,
        .area = {255, 255, 255, 255, 255, 255},
        .dir = 7,
        .px = 0,
        .py = 0,
        .pz = 0,
    },
    {
        .attr1 = 0,
        .dst_gthr = 5000,
        .way_gthr = 1,
        .atk_ptn = 1,
        .wspd = 3,
        .rspd = 7,
        .hp = 400,
        .atk_rng = 700,
        .hit_rng = 500,
        .chance_rng = 700,
        .hit_adjx = 0,
        .atk_p = 50,
        .atk_h = 150,
        .atk = 1,
        .atk_tm = 4,
        .mdl_no = 58,
        .anm_no = 34,
        .se_no = 1385,
        .adpcm_no = 1572,
        .dead_adpcm = -1,
        .point_base = 300,
        .hint_pic = 255,
        .aura_alp = 32,
        .area = {255, 255, 255, 255, 255, 255},
        .dir = 8,
        .px = 0,
        .py = 0,
        .pz = 0,
    },
    {
        .attr1 = 0,
        .dst_gthr = 5000,
        .way_gthr = 1,
        .atk_ptn = 2,
        .wspd = 0,
        .rspd = 0,
        .hp = 20,
        .atk_rng = 350,
        .hit_rng = 350,
        .chance_rng = 350,
        .hit_adjx = 0,
        .atk_p = 50,
        .atk_h = 100,
        .atk = 1,
        .atk_tm = 5,
        .mdl_no = 25,
        .anm_no = 16,
        .se_no = 1387,
        .adpcm_no = 1578,
        .dead_adpcm = -1,
        .point_base = 400,
        .hint_pic = 255,
        .aura_alp = 0,
        .area = {255, 255, 255, 255, 255, 255},
        .dir = 10,
        .px = 0,
        .py = 0,
        .pz = 0,
    },
};

AENE_INFO_DAT aene_info_dat4[] = {
    {
        .dat_no = 0,
        .soul_no = 255,
        .dir = 282,
        .px = 32700,
        .py = -1925,
        .pz = 24600,
        .adpcm_tm = 31,
        .adpcm_no = 1790,
        .rng = 1000,
        .mdl_no = 6,
        .anm_no = 76,
        .point_base = 1000,
        .se_no = 1439,
        .se_foot = 1452,
    },
};

ENE_DAT* jene_dat[] = { jene_dat0, jene_dat1, jene_dat2, jene_dat3, jene_dat4, };
ENE_DAT* fene_dat[] = { fene_dat1, fene_dat1, fene_dat2, fene_dat3, fene_dat4, };
AENE_INFO_DAT* aene_info_dat[] = { aene_info_dat0, aene_info_dat1, aene_info_dat2, aene_info_dat3, aene_info_dat4, };

#include "graphics/graph2d/effect_ene.h"
#include "graphics/motion/mdlwork.h"
#include "ingame/entry/ap_zgost.h"
#include "ingame/event/ev_main.h"
#include "ingame/event/wan_soul.h"
#include "ingame/menu/ig_camra.h"
#include "outgame/btl_mode/btl_mode.h"
int erootd0[20][3] = {0};
int erootd1[20][3] = {0};
int erootd2[20][3] = {0};

u_char req_dmg_ef[3] = {0, 0, 0};
static u_short es_adpcm_tm = 0;

#define SCREEN_WIDTH   640
#define SCREEN_HEIGHT  224

#define PI 3.1415927f
#define PI_HALF 1.5707964f

#define ADDRESS 0x7e0000

void EneCtrlMain()
{
    u_char i;
    static u_char flag = 0;

    if (DBG_cam_id_move_chk)
    {
        if (flag == 0)
        {
            flag = 1;

            ReqEneStop(1, 0);
        }
    }
    else if (flag != 0)
    {
        flag = 0;

        ReqEneStop(0, 0);
    }

    for (i = 0; i < 4; i++)
    {
        if (ene_wrk[i].sta & 0x1)
        {
            if (ene_wrk[i].sta & 0x2000)
            {
                EneDmgTexDel(i);
            }

            if (EneWrkReleaseChk(&ene_wrk[i]) == 0)
            {
                EneRule(&ene_wrk[i]);
            }
        }
        else
        {
            if (ene_wrk[i].sta & 0x2)
            {
                EneEntryChk(i);
            }
        }
    }

    for (i = 0; i < 10; i++)
    {
        if (fly_wrk[i].sta & 0x1 && (fly_wrk[i].sta & 0x4) == 0 && plyr_wrk.ap_timer == 0)
        {
            FlyRule(&fly_wrk[i]);
            MoveCtrl(&fly_wrk[i].move_box);
        }
    }

    plyr_wrk.pr_info.room_old = plyr_wrk.pr_info.room_no;
}

void EneEntryChk(u_char no)
{
    switch (ene_wrk[no].type)
    {
    case 0:
        JEneEntry(no, ene_wrk[no].dat_no);
    break;
    case 1:
        FEneEntry(no, ene_wrk[no].dat_no);
    break;
    case 2:
        AEneEntry(no, ene_wrk[no].dat_no);
    break;
    }

    motSetAnmPacket(no);
    SetEneTexture(no);
    SetEneDmgTex(no);
    RequestSpirit(no, 1);
}

void JEneEntry(u_char wrk_no, u_char dat_no)
{
    ENE_WRK *ew;
    sceVu0FVECTOR p;
    float rot;
    u_int sta_save;
    u_char i;
    u_char cnt;

    ew = &ene_wrk[wrk_no];

    sta_save = ew->sta & 0x80000000;

    p[0] = ew->move_box.pos[0];
    p[1] = ew->move_box.pos[1];
    p[2] = ew->move_box.pos[2];
    p[3] = ew->move_box.pos[3];

    InitEneWrk(wrk_no);

    ew->type = 0;
    ew->dat_no = dat_no;
    ew->sta |= sta_save | 0x1;
    ew->dat = &jene_dat[ingame_wrk.msn_no][dat_no];
    ew->hp = ew->dat->hp;

    if ((ew->sta & 0x80000000) != 0)
    {
        ew->move_box.pos[0] = p[0];
        ew->move_box.pos[1] = p[1];
        ew->move_box.pos[2] = p[2];
        ew->move_box.pos[3] = p[3];

        time_wrk.zerohour = 0x708;
    }
    else if (ingame_wrk.game == 1)
    {
        ew->move_box.pos[0] = p[0];
        ew->move_box.pos[1] = p[1];
        ew->move_box.pos[2] = p[2];
        ew->move_box.pos[3] = p[3];
    }
    else
    {
        ew->move_box.pos[0] = ew->dat->px;
        ew->move_box.pos[1] = ew->dat->py;
        ew->move_box.pos[2] = ew->dat->pz;
        ew->move_box.pos[3] = 0;
    }

    EnePosInfoSet(ew);

    if (ew->dat->dir == 1000)
    {
        rot = GetTrgtRotY(ew->move_box.pos, plyr_wrk.move_box.pos);
    }
    else
    {
        rot = (ew->dat->dir * PI) / 180.0f;

        RotLimitChk(&rot);
    }

    ew->move_box.rot[0] = 0.0f;
    ew->move_box.rot[1] = rot;
    ew->move_box.rot[2] = 0.0f;
    ew->move_box.rot[3] = 0.0f;

    EneBlinkDataSet(ew);

    for (i = 0, cnt = 0; i < 6; i++)
    {
        ew->area[i] = ew->dat->area[i];

        if (ew->area[i] != 0xff)
        {
            cnt++;
        }
    }

    if (cnt == 1 && ingame_wrk.game != cnt)
    {
        DoorLockBattleSet();
    }

    AdpcmPlayGhost(ew->dat->adpcm_no, &ew->move_box.pos, MAX_VOLUME, wrk_no, 200);

    ew->nee_size = 10000.0f;
    ew->nee_col = ew->dat->aura_alp | 0x40404500;
    ew->nee_rate = 1.0;
    ew->nee = SetEffects(0x1c, 2, 1, &ew->move_box.pos, &ew->mpos, &ew->nee_col, &ew->nee_size, 0xa0, &ew->nee_rate);
    ew->se_area_no = SeGetGhostPos(ew->dat->se_no, ew->type);

    EneActSet(ew, 9);

    if (ZeroGhostBattleJudge() != 0)
    {
        ene_wrk[wrk_no].sta |= 0x80000000;
    }
}

void FEneEntry(u_char wrk_no, u_char dat_no)
{
    ENE_WRK *ew;
    sceVu0FVECTOR tv;
    float rot;

    ew = &ene_wrk[wrk_no];

    tv[0] = ew->move_box.pos[0];
    tv[1] = ew->move_box.pos[1];
    tv[2] = ew->move_box.pos[2];
    tv[3] = ew->move_box.pos[3];

    rot = ew->move_box.rot[1];

    RotLimitChk(&rot);
    InitEneWrk(wrk_no);

    ew->dat_no = dat_no;
    ew->type = 1;
    ew->dat = &fene_dat[ingame_wrk.msn_no][dat_no];
    ew->sta |= 1;
    ew->hp = ew->dat->hp;
    ew->move_box.pos[0] = tv[0];
    ew->move_box.pos[1] = tv[1];
    ew->move_box.pos[2] = tv[2];
    ew->move_box.pos[3] = tv[3];

    EnePosInfoSet(ew);

    ew->move_box.rot[1] = rot;

    EneBlinkDataSet(ew);

    ew->nee_size = 10000.0f;
    ew->nee_col = ew->dat->aura_alp | 0x40404500;
    ew->nee_rate = 1.0f;
    ew->nee = SetEffects(0x1c, 2, 1, ew->move_box.pos, ew->mpos, &ew->nee_col, &ew->nee_size, 0xa0, &ew->nee_rate);
    ew->se_area_no = SeGetGhostPos(ew->dat->se_no, ew->type);

    AdpcmPlayGhost(ene_wrk[wrk_no].dat->adpcm_no, &ew->move_box.pos, MAX_VOLUME, wrk_no, 200);
    EneActSet(ew, 9);
}

void AEneEntry(u_char wrk_no, u_char dat_no)
{
    ENE_WRK *ew;
    AENE_INFO_DAT *aie;
    float rot;
    static ENE_DAT tmp_aene_dat;

    ew = &ene_wrk[wrk_no];

    InitEneWrk(wrk_no);

    ew->dat_no = dat_no;
    aie = &aene_info_dat[ingame_wrk.msn_no][dat_no];

    ew->type = 2;
    ew->sta |= 0x1;

    ew->dat = &tmp_aene_dat;
    ew->dat->mdl_no = aie->mdl_no;
    ew->dat->se_no = aie->se_no;
    ew->dat->anm_no = aie->anm_no;
    ew->dat->point_base = aie->point_base;

    ew->aie = aie;
    ew->hp = 100;
    ew->move_box.pos[0] = aie->px;
    ew->move_box.pos[1] = aie->py;
    ew->move_box.pos[2] = aie->pz;
    ew->move_box.pos[3] = 0.0f;

    EnePosInfoSet(ew);

    if (aie->dir == 1000)
    {
        rot = GetTrgtRotY(ew->move_box.pos, plyr_wrk.move_box.pos);
    }
    else
    {
        rot = (aie->dir * PI) / 180.0f;

        RotLimitChk(&rot);
    }

    ew->move_box.rot[0] = 0.0;
    ew->move_box.rot[1] = rot;
    ew->move_box.rot[2] = 0.0;
    ew->move_box.rot[3] = 0.0;

    EneBlinkDataSet(ew);
    EneActSet(ew, 2);

    if (ew->dat->se_no != 0xffffffff)
    {
        ew->se_area_no = SeGetAutoGhostPos(ew->dat->se_no);
    }
    else
    {
        ew->se_area_no = -1;
    }

    es_adpcm_tm = 0;
}

void EneRule(ENE_WRK *ew)
{
    u_char act_no;
    u_char view_chk;
    char *str1 = "JIBAKU ENE ";
    char *str2 = "FUYU ENE   ";
    char *str3 = "JIDOU ENE  ";
    char *str_clr = "           ";

    if (dbg_wrk.param_enetype != 0)
    {
        switch (ew->type)
        {
            case 0:
                SetASCIIString2( 0x10, 430.0, ew->move_box.idx * 10 + 10, 1, 0xdc, 0x32, 0x32, str1);
            break;
            case 1:
                SetASCIIString2(0x10, 430.0, ew->move_box.idx * 10 + 10, 1, 0x32, 0xdc, 0x32, str2);
            break;
            case 2:
                SetASCIIString2(0x10, 430.0, ew->move_box.idx * 10 + 10, 1, 0xdc, 0xdc, 0x32, str3);
            break;
        }

        SetInteger2(0x10, 535.0f, ew->move_box.idx * 10 + 10, 1, 0xff, 0xff, 0xff, ew->dat_no);
    }
    else
    {

        SetASCIIString2(0x10, 430.0, ew->move_box.idx * 10 + 10, 1, 0xdc, 0xdc, 0x32, str_clr);
    }

    if (plyr_wrk.ap_timer == 0 || ew->dmg != 0)
    {
        EneInDispChk(ew);
        EneLightCtrl(ew);
        EneBlinkPosSet(ew);

        if (ew->type == 2)
        {
            EneSpeakAdpcmCtrl(ew);
        }
        else
        {
            EneAdpcmPlayChk(ew);
            EneCondCtrl(ew);
            EneNormalEffectCtrl(ew);
            EneSeOmenChk(ew, 1);
            EneMoveCtrl(ew);

            if (EnePRecogChkChk(ew) != 0)
            {
                view_chk = EnePRecogChk(ew, &act_no);
            }
            else
            {
                view_chk = 0;
                act_no = ew->act_no;
            }

            EneActIniChk(ew, view_chk, act_no);
            EneActRule(ew);
            EneAuraCtrl(ew);
            EneMinDmgCtrl(ew);
            EneAniResolutionCtrl(ew);
        }

        if ((ew->sta & 0x30000000) == 0 && (ew->sta & 0x8000000) == 0 && dbg_wrk.param_enestop == 0)
        {
            MoveCtrl(&ew->move_box);
        }

        if (ew->stm_view == 0)
        {
            EneBlinkCtrl(ew);
        }
    }
}

void EneCondCtrl(ENE_WRK *ew)
{
    if (ew->stm_slow != 0)
    {
        ew->stm_slow -= 1;
    }

    if (ew->stm_view != 0)
    {
        ew->stm_view -=1;
    }

    if (ew->stm_stop != 0)
    {
        ew->stm_stop -= 1;

        if (!ew->stm_stop)
        {
            ene_wrk[ew->move_box.idx].sta &= ~0x10000000;
        }
    }
}

void EneAuraCtrl(ENE_WRK *ew)
{
    if (plyr_wrk.mode == 0x2)
    {
        ew->tr_rate = 50;
        ew->tr_rate2 = 50;
    }

    if ((ew->dat->attr1 & 0x10 || (ingame_wrk.difficult != 0 && ingame_wrk.game == 0)) && ew->sta & 0x2000000)
    {
        if (plyr_wrk.mode != 1 && plyr_wrk.mode != 2 && ew->act_no != 8)
        {
            ew->tr_rate = 0;
        }
        else
        {
            ew->tr_rate = 128;
        }
    }

    if (plyr_wrk.mode == 1 || efcnt[12].dat.uc8[0] != 0 || ew->sta & 0x8000 || ew->hp == 0)
    {
        ew->d_pd2 = 0.0f;
    }
    else
    {
        ew->d_pd2 = 60.0f;
    }

    if (efcnt[12].dat.uc8[0] != 0)
    {
        ew->nee_rate = 0.0f;
    }
    else if (ew->hp != 0)
    {
        if (ew->tr_rate != 0)
        {
            ew->nee_rate = ew->nee_rate + 0.025f < 1.0f ? ew->nee_rate + 0.025f : 1.0f;
        }
        else
        {
            ew->nee_rate = ew->nee_rate > 0.025f ? ew->nee_rate - 0.025f : 0.0f;
        }
    }
}

void InitEneWrk(u_char wrk_no)
{
    ENE_WRK *ew;
    int i;

    ew = &ene_wrk[wrk_no];

    *ew = (ENE_WRK){0};

    InitMoveBox(&ew->move_box);

    ew->move_box.idx = wrk_no;

    ew->pr_anime = 1.0f;

    ew->plight_svo = 0xff;
    ew->slight_svo = 0xff;

    for (i = 0; i < 2; i++)
    {
        ew->plight_svm[i] = -1;
        ew->slight_svm[i] = -1;
    }

    for (i = 0; i < 2; i++)
    {
        ew->se_omen[i] = -1;
    }

    for (i = 0; i < 6; i++)
    {
        ew->area[i] = 0xff;
    }

    ew->nee = NULL;

    ew->pdf = NULL;

    ew->pdf2 = NULL;
}

int EneWrkReleaseChk(ENE_WRK *ew)
{
    int result;

    result = 0;
    if (map_wrk.mirror_flg != 0 || EnemyUseJudge(0) >= 2)
    {
        if (ew->pdf2 != NULL)
        {
            ResetEffects(ew->pdf2);
            ew->pdf2 = NULL;
        }
    }

    if (ew->sta & 0x2000)
    {
        if (ew->sta & 0x8000)
        {
            return 0;
        }

        if (ew->atk_tm != 0)
        {
            return 0;
        }

        if (plyr_wrk.sta & 8)
        {
            return 1;
        }

        result = 1;
        switch (ew->type)
        {
            case 0:
                BattleEndEventOpenJudge(ew->dat_no);
                AdpcmStopGhost(0x14);
                if (ingame_wrk.msn_no == 1 && ew->dat_no == 0)
                {
                    OpenCameraMenu();
                }
                break;
            case 1:
                if (EnemyNoDeadGhostJudge(ew->dat_no) != 0)
                {
                    if (ew->hp != 0)
                    {
                        FloatGhostEscapeEnd();
                    }
                    else
                    {
                        DeadGhostBattleEnd();
                    }
                }
                else if (ew->dat_no != ap_wrk.ggst_no)
                {
                    if (ew->hp != 0)
                    {
                        FloatGhostEscapeEnd();
                    }
                    else
                    {
                        FloatGhostBattleEnd();
                    }
                }
                AdpcmStopGhost(0x14);
                break;
            case 2:
                BattleEndEventOpenJudge(ew->dat_no);
                if (ew->aie->soul_no != 0xFF)
                {
                    CallWanderSoul(ew->aie->soul_no, ew->mpos.p1);
                }
                es_adpcm_tm = 0;
                AdpcmStopAutoGhost(0xA);
                break;
        }
        if (ew->nee != NULL)
        {
            ResetEffects(ew->nee);
        }
        if (ew->pdf != NULL)
        {
            ResetEffects(ew->pdf);
        }
        if (ew->pdf2 != NULL)
        {
            ResetEffects(ew->pdf2);
        }
        if (ew->plight_svm != NULL)
        {
            room_wrk.mylight[0].point_num -= 1;
        }
        EneSeOmenChk(ew, 0);
        if (ew->hp != 0)
        {
            RequestSpirit(ew->move_box.idx, 0);
            ReleaseEneTexture(ew->move_box.idx);
            motReleaseAnmPacket(ew->move_box.idx);
        }
        memset(ew, 0, 0x430);
        if (ap_wrk.zh_mode != 0)
        {
            ZeroHourOutReq();
        }
        DoorLockBattleAfter();
        if (ingame_wrk.game == 1)
        {
            BattleModeClear();
        }
    }
    return result;
}

void FuyuEneRelease()
{
    u_char i;

    for (i = 0; i < 3; i++)
    {
        if (ene_wrk[i].sta & 0x1 && ene_wrk[i].type == 1)
        {
            ene_wrk[i].sta |= 0x2000;

            EneWrkReleaseChk(&ene_wrk[i]);
        }
    }
}

void EnePosInfoSet(ENE_WRK *ew)
{
    ew->room_no = GetPointRoom(ew->move_box.pos[0], ew->move_box.pos[2]);

    if (ew->room_no != 0xff && ew->dat->attr1 & 0x40)
    {
        GetPointHeightChk((ew->move_box).pos);
    }
}

void EneSeOmenChk(ENE_WRK *ew, u_char id)
{
    float dist_tbl[2] = {4000.0f, 1250.0f};
    u_short se_tbl[2] = {54, 55};
    float dist;
    int i;

    if (ew->type != 0x2)
    {
        if (id == 0)
        {
            for (i = 0; i < 2; i++)
            {
                if (ew->se_omen[i] != -1)
                {
                    SeFadeFlame(ew->se_omen[i], 180, 0);

                    ew->se_omen[i] = -1;
                }
            }
        }
        else if (ew->hp != 0)
        {
            dist = GetDistV(plyr_wrk.move_box.pos, ew->move_box.pos);

            for (i = 0; i < 2; i++)
            {
                if (ew->se_omen[i] == -1)
                {
                    if ((dist_tbl[i] >= dist) && (ew->se_area_no != -1))
                    {
                        ew->se_omen[i] = SeStartGhostFlame(se_tbl[i], ew->move_box.idx, 180, 4096, 4096, ew->se_area_no);
                    }
                }
                else if (dist_tbl[i] < dist)
                {
                    SeFadeFlame(ew->se_omen[i], 180, 0);

                    ew->se_omen[i] = -1;
                }
            }
        }
    }
}

void EneMoveCtrl(ENE_WRK *ew)
{
    MOVE_BOX *mb;
    sceVu0FVECTOR tv;
    u_char i;

    mb = &ew->move_box;

    if (dbg_wrk.param_enestop == 0 && (ew->sta & (0x8000000 | 0x30000000)) == 0)
    {
#ifdef BUILD_EU_VERSION
        sceVu0ScaleVector(tv, ew->move_box.spd, sys_wrk.move_conv);
#else
        tv[0] = mb->spd[0];
        tv[1] = mb->spd[1];
        tv[2] = mb->spd[2];
        tv[3] = mb->spd[3];
#endif

        if (ew->stm_slow != 0)
        {
            sceVu0ScaleVector(tv, tv, 0.5f);
        }

        if ((ew->sta & (0x400000 | 0x800000)) == 0)
        {
            RotFvector(mb->rot, tv);
        }

        sceVu0AddVector(mb->pos, mb->pos, tv);

        EnePosInfoSet(ew);

        sceVu0AddVector(mb->rot, mb->rot, mb->rspd);

        for (i = 0; i < 3; i++)
        {
            RotLimitChk(&mb->rot[i]);
        }

        if (mb->trot[3] != 0.0f)
        {
            tv[1] = mb->trot[1] - mb->rot[1];

            RotLimitChk(&tv[1]);

            if (tv[1] > 0.0f)
            {
                if (mb->trot[3] > tv[1])
                {
                    mb->trot[3] = 0.0f;

                    mb->rot[1] = mb->trot[1];
                }
                else
                {
                    mb->rot[1] += mb->trot[3];

                    RotLimitChk(&mb->rot[1]);
                }
            }
            else if (-mb->trot[3] < tv[1])
            {
                mb->trot[3] = 0.0f;

                mb->rot[1] = mb->trot[1];
            }
            else
            {
                mb->rot[1] -= mb->trot[3];

                RotLimitChk(&mb->rot[1]);
            }
        }
    }
}

u_char EnePRecogChkChk(ENE_WRK *ew)
{
    u_char req;

    req = 0;

    if (ew->sta & 0x4)
    {
        req = 1;
        ene_wrk[(ew->move_box).idx].sta &= ~0x4;
    }
    else
    {
        if (ew->act_no == 0xff || ew->act_no == 2 || ew->act_no == 3 || ew->act_no == 4)
        {
            if (ew->recog_tm == 0)
            {
                ew->recog_tm = 0;
                req = 1;
            }
            else
            {
                ew->recog_tm--;
            }
        }
    }

    return req;
}

u_char EnePRecogChk(ENE_WRK *ew, u_char *act_no)
{
    u_char new_act;

    new_act = ew->act_no;

    *act_no = new_act;

    if ((ew->dat->attr1 & 0x40) == 0)
    {
        new_act = 4;
    }
    else if (ew->sta & (0x8 | 0x10))
    {
        new_act = 4;

        if (HitChkSegment2All(
            plyr_wrk.move_box.pos,
            ew->move_box.pos,
            GetDistV(plyr_wrk.move_box.pos, ew->move_box.pos) / 50.0f))
        {
            new_act = 3;
        }
    }
    else
    {
        new_act = 2;

        if (
            HitDmgChk(ew) != 0 ||
            CaptureOnChk1(ew) != 0 || CaptureOnChk2(ew) != 0 ||
            CaptureOnChk3(ew) != 0 || CaptureOnChk4(ew) != 0
        )
        {
            if (PlyrOutAreaChk(ew) == 0)
            {
                new_act = 3;
            }
        }
    }

    ene_wrk[ew->move_box.idx].sta &= ~0x10 & ~0x8;

    switch (new_act)
    {
    case 3:
        ene_wrk[ew->move_box.idx].sta |= 8;
    break;
    case 4:
        ene_wrk[ew->move_box.idx].sta |= 0x10 | 0x8;
    break;
    }

    *act_no = new_act;

    return ew->act_no != new_act;
}

u_char HitDmgChk(ENE_WRK *ew)
{
    u_char chk;

    chk = 0;

    if (ew->sta & 0x80)
    {
        chk = 1;

        ene_wrk[ew->move_box.idx].sta &= ~0x80;
    }

    return chk;
}

u_char CaptureOnChk1(ENE_WRK *ew)
{
    u_char chk;
    float gthr_rng;

    chk = 0;

    if (ew->dat->way_gthr & 0x1)
    {
        gthr_rng = ew->dat->dst_gthr;

        if (GetDistV(plyr_wrk.move_box.pos, ew->move_box.pos) <= gthr_rng)
        {
            chk = 1;
        }
    }

    return chk;
}

u_char CaptureOnChk3(ENE_WRK *ew)
{
    u_char chk;

    chk = 0;

    if (ew->dat->way_gthr & 0x4)
    {
        chk = PlyrSeFootChk(ew) != 0;
    }

    return chk;
}

u_char CaptureOnChk4(ENE_WRK *ew)
{
    return 0;
}

u_char CaptureOnChk2(ENE_WRK *ew)
{
    sceVu0FVECTOR rot;
    float gthr_rng;
    u_char chk;

    chk = 0;

    if (ew->dat->way_gthr & 0x2)
    {
        gthr_rng = ew->dat->dst_gthr;

        if (GetDistV(plyr_wrk.move_box.pos, ew->move_box.pos) <= gthr_rng)
        {
            GetTrgtRot(ew->move_box.pos, plyr_wrk.move_box.pos, rot, 2);

            rot[1] -= ew->move_box.rot[1];

            RotLimitChk(&rot[1]);

            if (rot[1] < 0.0f)
            {
                rot[1] = -rot[1];
            }

            if (rot[1] <= PI_HALF)
            {
                chk = 1;
            }
        }
    }

    return chk;
}

u_char PlyrSeFootChk(ENE_WRK *ew)
{
    float rng;
    u_char chk;

    chk = 0;
    rng = 0.0f;

    if (plyr_wrk.mvsta & 0x1)
    {
        rng = 5000.0f;
    }
    else if (plyr_wrk.mvsta & 0x2)
    {
        rng = 2500.0f;
    }
    else if (plyr_wrk.mvsta & 0x8)
    {
        rng = 2500.0f;
    }

    if (GetDistV(plyr_wrk.move_box.pos, ew->move_box.pos) <= rng)
    {
        chk = 1;
    }

    return chk;
}

void EneActIniChk(ENE_WRK *ew, u_char view_chk, u_char act_no)
{
    if (view_chk || ew->sta & 0x1000)
    {
        ene_wrk[ew->move_box.idx].sta &= ~0x1000;

        EneActSet(ew, act_no);
    }
}

void EneActIniSet(ENE_WRK *ew, u_char act_no)
{
    EneActSet(ew, act_no);
}

void EneActSet(ENE_WRK *ew, u_char act_no)
{
    MOVE_BOX *mb;

    u_long v0, v1; // not in STAB
    u_long t3;     // not in STAB but trivial to remove (keeping it for symmetry)

    mb = &ew->move_box;

    ew->act_no = act_no;
    mb->comm_add_top = ADDRESS;

    v1 = (u_int)((u_long)(ew->type) * 2 + ADDRESS);
    v0 = ((ADDRESS | (((u_char *)MikuPan_GetHostPointer((u_int)v1))[0] | (((u_char *)MikuPan_GetHostPointer((u_int)v1))[1] << 8)))) + (u_long)(ew->dat_no * 2);
    v1 = ((ADDRESS | (((u_char *)MikuPan_GetHostPointer((u_int)v0))[0] | (((u_char *)MikuPan_GetHostPointer((u_int)v0))[1] << 8)))) + (u_long)(    act_no * 2);
    t3 = ((ADDRESS | (((u_char *)MikuPan_GetHostPointer((u_int)v1))[0] | (((u_char *)MikuPan_GetHostPointer((u_int)v1))[1] << 8))));

    mb->comm_add.wrk = t3;
    mb->pos_no = 0;
    mb->wait_time = 1;

    mb->spd[0] = 0.0f;
    mb->spd[1] = 0.0f;
    mb->spd[2] = 0.0f;
    mb->spd[3] = 0.0f;

    mb->rspd[0] = 0.0f;
    mb->rspd[1] = 0.0f;
    mb->rspd[2] = 0.0f;
    mb->rspd[3] = 0.0f;

    mb->rot[2] = 0.0f;

    ene_wrk[mb->idx].sta &= 0xfeffffff;
    ene_wrk[mb->idx].sta &= 0xffbfffff;
    ene_wrk[mb->idx].sta &= 0xff7fffff;
    ene_wrk[mb->idx].sta &= 0xfdffffff;
    ene_wrk[mb->idx].sta &= 0xfff7ffff;
    ene_wrk[mb->idx].sta &= 0xfffeffff;
    ene_wrk[mb->idx].sta &= 0xfffdffff;
}

void EneBlinkDataSet(ENE_WRK *ew)
{
    u_long v0, v1; // not in STAB
    u_long a3;     // not in STAB but trivial to remove (keeping it for symmetry)

    ew->bcomm_add_top = ADDRESS;

    v1 = (u_int)((u_long)(ew->type) * 2 + ADDRESS);
    v0 = ((ADDRESS | (((u_char *)MikuPan_GetHostPointer((u_int)v1))[0] | (((u_char *)MikuPan_GetHostPointer((u_int)v1))[1] << 8)))) + (u_long)(ew->dat_no * 2);
    v1 = ((ADDRESS | (((u_char *)MikuPan_GetHostPointer((u_int)v0))[0] | (((u_char *)MikuPan_GetHostPointer((u_int)v0))[1] << 8))));
    a3 = ((ADDRESS | (((u_char *)MikuPan_GetHostPointer((u_int)v1))[0] | (((u_char *)MikuPan_GetHostPointer((u_int)v1))[1] << 8))));

    ew->bcomm_add.wrk = a3;
    ew->bpos_no = 0;
    ew->bwait_time = 1;
}

void EneARatioDataSet(ENE_WRK *ew, u_char anime_no)
{
    u_long v0, v1, v2; // not in STAB
    u_long a3;         // not in STAB but trivial to remove (keeping it for symmetry)

    ew->acomm_add_top = ADDRESS;

    v0 = (u_int)((u_long)(ew->type) * 2 + ADDRESS);
    v1 = ((ADDRESS | (((u_char *)MikuPan_GetHostPointer((u_int)v0))[0] | (((u_char *)MikuPan_GetHostPointer((u_int)v0))[1] << 8)))) + (u_long)(ew->dat_no * 2);
    v2 = ((ADDRESS | (((u_char *)MikuPan_GetHostPointer((u_int)v1))[0] | (((u_char *)MikuPan_GetHostPointer((u_int)v1))[1] << 8)))) + (u_long)(             2);
    v0 = ((ADDRESS | (((u_char *)MikuPan_GetHostPointer((u_int)v2))[0] | (((u_char *)MikuPan_GetHostPointer((u_int)v2))[1] << 8)))) + (u_long)(  anime_no * 2);
    a3 = ((ADDRESS | (((u_char *)MikuPan_GetHostPointer((u_int)v0))[0] | (((u_char *)MikuPan_GetHostPointer((u_int)v0))[1] << 8))));

    ew->acomm_add.wrk = a3;
    ew->apos_no = 0;
    ew->await_time = 1;
}

void EneActRule(ENE_WRK *ew)
{
    float dist;
    u_char act_no;

    act_no = 0xff;

    if ((ew->sta & 0x8000000) == 0)
    {
        if (EneActPreferChk(ew, &act_no) == 0)
        {
            if (ew->sta & 0x8000)
            {
                EneAtkCtrl(ew);
            }

            if (ew->sta & 0x10000000)
            {
                return;
            }

            switch (ew->act_no)
            {
            case 5:
                dist = GetDistV2(plyr_wrk.move_box.pos, ew->move_box.pos);

                if (dist >= ew->dat->atk_rng + 400.0f)
                {
                    act_no = 2;
                }
                else if (ew->sta & 0x10000 && dist <= ew->dat->chance_rng)
                {
                    ene_wrk[ew->move_box.idx].sta |= 0x20000;
                }
            break;
            case 4:
                if (dbg_wrk.param_enestop == 0)
                {
                    dist = GetDistV2(plyr_wrk.move_box.pos, ew->move_box.pos);

                    if (dist <= ew->dat->atk_rng && ((ew->sta & 0x480000) == 0)
                    && HitChkSegment2AllChk(plyr_wrk.move_box.pos, ew->move_box.pos, dist / 50.0f) == 0)
                    {
                        act_no = 5;
                    }
                }
            break;
            }
        }

        if (act_no != 0xff)
        {
            EneActSet(ew, act_no);
        }
    }
}

int EneActPreferChk(ENE_WRK *ew, u_char *act_no)
{
    u_char n;
    u_char prefer_act[4] = {0xff, 6, 7, 8};

    n = 0;

    if (PlyrOutAreaChk(ew) != 0)
    {
        n = 1;
    }
    else
    {
        switch (EneDmgChk(ew))
        {
        case 1:
            n = 2;
        break;
        case 2:
            n = 3;
        break;

        }
    }

    *act_no = prefer_act[n];

    return n;
}

void EneAniResolutionCtrl(ENE_WRK *ew)
{
    u_char no;
    static u_char reso[4][2] = {
        {0xff, 0xff},
        {0xff, 0xff},
        {0xff, 0xff},
        {0xff, 0xff},
    };

    u_char bVar3;

    no = ew->move_box.idx;

    if (dbg_wrk.param_enestop != 0)
    {
        ew->ani_reso = 0;
    }
    else if (ew->sta & 0x18000000)
    {
        ew->ani_reso = 0;
        ew->ani_reso_tm = 0;
        reso[no][0] = reso[no][1] = 0xff;
    }
    else if (ew->sta & 0x20000000 && ew->hp != 0)
    {
        ew->ani_reso = 5;
        ew->ani_reso_tm = 0;
        reso[no][0] = reso[no][1] = 0xff;
    }
    else if (ew->stm_slow != 0)
    {
        ew->ani_reso = 3;
        ew->ani_reso_tm = 0;
        reso[no][0] = reso[no][1] = 0xff;
    }
    else if ((ew->act_no == 3 || ew->act_no == 4) && ew->anime_no == 2)
    {
        if (reso[no][0] != 0xff)
        {
            if (reso[no][0] == ew->ani_reso)
            {
                reso[no][0] = 0xff;
            }
            else if (reso[no][1] == 0)
            {
                if (ew->ani_reso > reso[no][0])
                {
                    ew->ani_reso--;
                }
                else
                {
                    ew->ani_reso++;
                }

                reso[no][1] = 0xf;
            }
            else
            {
                reso[no][1]--;
            }
        }
        else
        {
            if (ew->ani_reso_tm == 0)
            {
                reso[no][0] = GetRndSP(2, 8);

                if (reso[no][0] > 5)
                {
                    reso[no][0] = 1;
                }

                ew->ani_reso_tm = GetRndSP(0x14, 100);

                reso[no][1] = 0xf;
            }
            else
            {

                ew->ani_reso_tm--;
            }
        }
    }
    else
    {
        if (ew->act_no == 8)
        {
            ew->ani_reso = 1;
        }
        else
        {
            ew->ani_reso = 1;
        }
    }
}

void EneAtkCtrl(ENE_WRK *ew)
{
    if (ew->atk_tm == 0)
    {
        ene_wrk[ew->move_box.idx].sta &= ~0x8000;
    }
    else
    {
        ew->atk_tm--;
    }
}

u_char EneDmgChk(ENE_WRK *ew)
{
    u_char result = 0;

    if (ew->dmg != 0)
    {
        ClrEneStaDmg(ew);

        if ((ew->sta & 0x80000000) && !(ew->sta & 0x800))
        {
            ew->dmg = 0;
            return result;
        }

        if (ew->hp <= ew->dmg)
        {
            result = 2;
            ew->hp = 0;
            ew->bwait_time = 0;
            plyr_wrk.cond_tm = 0;
        }
        else
        {
            ew->hp -= ew->dmg;

            if (ew->sta & 0x800)
            {
                result = 1;
            }
            else
            {
                if (req_dmg_ef[ew->move_box.idx] == 0)
                {
                    req_dmg_ef[ew->move_box.idx] = 1;
                }
            }

        }

        ene_wrk[ew->move_box.idx].sta |= 0x80;

        ew->dmg_old = ew->dmg;
        ew->dmg = 0;

        if (ew->dmg_old != 0)
        {
            SetEneDmgEffect1(ew->move_box.idx);
        }

    }
    return result;
}

void ClrEneStaDmg(ENE_WRK *ew)
{
    if (ew->sta & 0x800)
    {
        ene_wrk[ew->move_box.idx].sta &= ~0x400000;
    }

    ew->stm_stop = 0;
    ew->stm_view = 0;
    ew->stm_slow = 0;

    ene_wrk[ew->move_box.idx].sta &= ~0x10000000 & ~0x20000000;
}

u_char PlyrOutAreaChk(ENE_WRK *ew)
{
    u_char result;

    result = 0;

    if (
        (ew->act_no == 0xff || ew->act_no == 9 || ew->act_no == 2 || ew->act_no == 3 || ew->act_no == 4) &&
        ((plyr_wrk.mode != 7 && plyr_wrk.mode != 2) || (plyr_wrk.atk_no != ew->move_box.idx))
    )
    {
        result = EneTrtryChk(ew,plyr_wrk.pr_info.room_no) == 0;
    }

    return result;
}

u_char EneTrtryChk(ENE_WRK *ew, u_char room_no)
{
    u_char result;
    u_char i;

    result = 0;

    if (ew->sta & 0x80000000 || ingame_wrk.game == 1)
    {
        return 1;
    }

    if (ew->type == 1)
    {
        if (ap_wrk.room_fg[plyr_wrk.pr_info.room_no] == 0)
        {
            return 0;
        }

        if (ew->area[0] == 0xff)
        {
            if (room_no == ew->room_no)
            {
                ew->area[0] = room_no;
            }

            return 1;
        }

        if  ((ew->dat->attr1 & 0x80) == 0 && ew->area[1] == 0xff && room_no != ew->area[0])
        {
            ew->area[1] = room_no;
        }
    }

    for (i = 0; i < 6; i++)
    {
        if (ew->area[i] == room_no)
        {
            result = 1;
            break;
        }
    }

    return result;
}

void EneLightCtrl(ENE_WRK *ew)
{
    sceVu0FVECTOR tv;
    sceVu0FVECTOR tv2;
    POINT_WRK *p;
    SPOT_WRK *s;
    u_char id;
    u_char i;

    if (ew->mylight.parallel_num != 0)
    {
        ew->mylight.parallel->direction[0] = camera.p[0] - ew->mpos.p0[0];
        ew->mylight.parallel->direction[1] = -1.0f;
        ew->mylight.parallel->direction[2] = camera.p[2] - ew->mpos.p0[2];
        ew->mylight.parallel->direction[3] = 0.0f;
    }

    for (i = 0; i < 2; i++)
    {
        id = ew->plight_svm[i];

        if (id != 0xff)
        {
            p = &room_wrk.mylight[i].point[id];

            tv[0] = ew->pp[0];
            tv[1] = ew->pp[1];
            tv[2] = ew->pp[2];
            tv[3] = ew->pp[3];

            RotFvector(ew->move_box.rot, tv);
            sceVu0AddVector(tv, tv, ew->mpos.p0);

            p->pos[0] = tv[0];
            p->pos[1] = tv[1];
            p->pos[2] = tv[2];
            p->pos[3] = tv[3];
        }
    }

    id = ew->plight_svo;

    if (id != 0xff)
    {
        p = &ew->mylight.point[id];

        tv[0] = ew->pp[0];
        tv[1] = ew->pp[1];
        tv[2] = ew->pp[2];
        tv[3] = ew->pp[3];

        RotFvector(ew->move_box.rot, tv);
        sceVu0AddVector(tv, tv, ew->mpos.p0);

        p->pos[0] = tv[0];
        p->pos[1] = tv[1];
        p->pos[2] = tv[2];
        p->pos[3] = tv[3];
    }

    for (i = 0; i < 2; i++)
    {
        id = ew->slight_svm[i];

        if (id != 0xff)
        {
            s= &room_wrk.mylight[i].spot[id];

            tv[0] = ew->sp[0];
            tv[1] = ew->sp[1];
            tv[2] = ew->sp[2];
            tv[3] = ew->sp[3];

            RotFvector(ew->move_box.rot, tv);
            sceVu0AddVector(tv, tv, ew->mpos.p0);

            s->pos[0] = tv[0];
            s->pos[1] = tv[1];
            s->pos[2] = tv[2];
            s->pos[3] = tv[3];
        }
    }

    id = ew->slight_svo;

    if (id != 0xff)
    {
        s = &ew->mylight.spot[id];

        tv[0] = ew->sp[0];
        tv[1] = ew->sp[1];
        tv[2] = ew->sp[2];
        tv[3] = ew->sp[3];

        RotFvector(ew->move_box.rot, tv);
        sceVu0AddVector(tv, tv, ew->mpos.p0);

        s->pos[0] = tv[0];
        s->pos[1] = tv[1];
        s->pos[2] = tv[2];
        s->pos[3] = tv[3];

        tv2[0] = ew->si[0];
        tv2[1] = ew->si[1];
        tv2[2] = ew->si[2];
        tv2[3] = ew->si[3];

        RotFvector(ew->move_box.rot, tv2);
        sceVu0AddVector(tv2, tv2, ew->mpos.p0);
        sceVu0SubVector(tv2, tv, tv2);

        s->direction[0] = tv2[0];
        s->direction[1] = tv2[1];
        s->direction[2] = tv2[2];
        s->direction[3] = tv2[3];
    }
}

void EneInDispChk(ENE_WRK *ew)
{
    PP_JUDGE ppj;
    sceVu0FVECTOR tv;
    sceVu0FVECTOR rv;
    float rot;
    float tx;
    float ty;


    rot = GetTrgtRotY(camera.p, camera.i);

    GetCamI2DPos(ew->mpos.p0, &tx, &ty);

    ew->sta2 &= ~0x1;

    if (
        (ew->sta & 0x80000) == 0 &&
        OutSightChk(ew->mpos.p0, camera.p, rot, PI, 5000.0f) == 0 &&
        tx >= 0.0f && tx <= SCREEN_WIDTH &&
        ty >= 0.0f && ty <= SCREEN_HEIGHT
    )
    {

        ppj.num = 1;
        ppj.result[0] = 0;

        tv[0] = 0.0f;
        tv[1] = 0.0f;
        tv[2] = 200.0f;
        tv[3] = 0.0f;

        GetTrgtRot(ew->mpos.p0, camera.p, rv, 3);

        RotFvector(rv, tv);

        sceVu0AddVector(ppj.p[0], ew->mpos.p0, tv);

        CheckPointDepth(&ppj);

        if (ppj.result[0] != 0)
        {

            ew->sta2 |= 0x1;
        }
    }
}

void EneBlinkPosSet(ENE_WRK *ew)
{
    sceVu0FVECTOR tv = {0.0f, 0.0f, 300.0f, 0.0f};
    sceVu0FVECTOR tr;

    GetTrgtRot(ew->mpos.p1, camera.p, tr, 3);

    RotFvector(tr, tv);

    sceVu0AddVector(ew->bep, ew->mpos.p1, tv);

    if (ew->sta & 0x200000)
    {
        ew->mpos.p0[0] = ew->move_box.pos[0];
        ew->mpos.p0[2] = ew->move_box.pos[2];
        ew->mpos.p0[3] = ew->move_box.pos[3];
        ew->mpos.p0[1] = ew->move_box.pos[1] - 500.0f;
    }
}

void FlyRule(FLY_WRK *fw)
{
    MOVE_BOX *mb;
    sceVu0FVECTOR tv;
    sceVu0FVECTOR av;

    mb = &fw->move_box;

    if (fw->adjp[3] != 0)
    {
        tv[0] = fw->adjp[0];
        tv[1] = fw->adjp[1];
        tv[2] = fw->adjp[2];
        tv[3] = fw->adjp[3];

        RotFvector(ene_wrk[fw->ene].move_box.rot, tv);

        if (fw->adjmv[3] != 0)
        {
            av[0] = fw->adjmv[0];
            av[1] = fw->adjmv[1];
            av[2] = fw->adjmv[2];
            av[3] = fw->adjmv[3];

            RotFvector(ene_wrk[fw->ene].move_box.rot, av);
            sceVu0AddVector(tv, tv, av);
        }

        sceVu0AddVector(fw->move_box.pos, ene_wrk[fw->ene].move_box.pos, tv);

        fw->move_box.rot[0] = ene_wrk[fw->ene].move_box.rot[0];
        fw->move_box.rot[1] = ene_wrk[fw->ene].move_box.rot[1];
        fw->move_box.rot[2] = ene_wrk[fw->ene].move_box.rot[2];
        fw->move_box.rot[3] = ene_wrk[fw->ene].move_box.rot[3];

        return;
    }

    if (mb->trot[3] != 0)
    {
        tv[1] = mb->trot[1] - mb->rot[1];
        RotLimitChk(&tv[1]);

        if (tv[1] > 0)
        {
            if ((mb->trot[3] > tv[1]) == 0)
            {
                mb->rot[1] += mb->trot[3];
                RotLimitChk(&mb->rot[1]);
            }
            else
            {
                mb->trot[3] = 0;
                mb->rot[1] = mb->trot[1];
            }
        }
        else if (-mb->trot[3] < tv[1])
        {
            mb->trot[3] = 0;
            mb->rot[1] = mb->trot[1];
        }
        else
        {
            mb->rot[1] -= mb->trot[3];
            RotLimitChk(&mb->rot[1]);
        }
    }

    tv[0] = fw->move_box.spd[0];
    tv[1] = fw->move_box.spd[1];
    tv[2] = fw->move_box.spd[2];
    tv[3] = fw->move_box.spd[3];

    if (!(fw->sta & 0x10))
    {
        RotFvector(fw->move_box.rot, tv);
    }

    if (FlyPlyrHitChk(fw, tv) != 0)
    {
        FlyAtkHit(fw);
    }

    sceVu0AddVector(fw->move_box.pos, fw->move_box.pos, tv);
}

int FlyPlyrHitChk(FLY_WRK *fw, sceVu0FVECTOR tv)
{
    sceVu0FVECTOR pf;
    sceVu0FVECTOR pp;
    int result;

    result = 0;

    if ((fw->sta & 0x8) == 0)
    {
        return 0;
    }

    if (fw->sta & 0x2)
    {
        return 0;
    }

    pf[0] = fw->move_box.pos[0];
    pf[1] = fw->move_box.pos[1];
    pf[2] = fw->move_box.pos[2];
    pf[3] = fw->move_box.pos[3];

    pp[0] = plyr_wrk.bwp[0];
    pp[1] = plyr_wrk.bwp[1];
    pp[2] = plyr_wrk.bwp[2];
    pp[3] = plyr_wrk.bwp[3];

    if (
        __builtin_fabsf(pf[0] - pp[0]) <= fw->dat->hit_rng + __builtin_fabsf(tv[0]) &&
        __builtin_fabsf(pf[1] - pp[1]) <= fw->dat->hit_rng + __builtin_fabsf(tv[1]) + 300.0f &&
        __builtin_fabsf(pf[2] - pp[2]) <= fw->dat->hit_rng + __builtin_fabsf(tv[2])
    )
    {
        result = 1;

        if (tv[0] != 0.0f)
        {
            tv[0] = __builtin_fabsf(pp[0] - pf[0]) - fw->dat->hit_rng;

            if (pp[0] < pf[0])
            {
                tv[0] = -tv[0];
            }
        }


        if (tv[2] != 0.0f)
        {
            tv[2] = __builtin_fabsf(pp[2] - pf[2]) - fw->dat->hit_rng;

            if (pp[2] < pf[2])
            {
                tv[2] = -tv[2];
            }
        }
    }

    return result;
}

void FlyAtkHit(FLY_WRK *fw)
{
    sceVu0FVECTOR rv;

    plyr_wrk.dmg += fw->dat->dmg;

    SeStartFix(0x21, 0, 0x1000, 0x1000, 0x0);

    fw->sta |= 0x2;

    ReqFinderInOverRap(0);

    if (plyr_wrk.mode == 1)
    {
        FinderEndSet();
    }

    plyr_wrk.cond_tm = 0;
    plyr_wrk.dmg_type = 0;
    plyr_wrk.dmg_cam_flag = 0;

    if ((plyr_wrk.mvsta & 0x200000) == 0 && (plyr_wrk.sta & 0x8) == 0 && plyr_wrk.mode != 2)
    {
        plyr_wrk.dmg_type = 1;
        plyr_wrk.mvsta &= ~0x8 & ~0x4 & ~0x2 & ~0x1;

        GetTrgtRot(plyr_wrk.move_box.pos, (fw->move_box).pos, rv, 2);

        rv[1] -= plyr_wrk.move_box.rot[1];

        RotLimitChk(&rv[1]);
        ConvertRot2Dir(rv[1], 2);
        SetPlyrAnime(62, 0x0);

        plyr_wrk.cond = fw->dat->cond;
        plyr_wrk.cond_tm = 480;
    }
}

void EneMinDmgCtrl(ENE_WRK *ew)
{
    u_char no;
    static u_char wait_time[3];

    no = ew->move_box.idx;

    switch(req_dmg_ef[no])
    {
    case 0:
    break;
    case 1:
        if (ew->sta & 0x800)
        {
            req_dmg_ef[no] = 0;
        }
        else
        {
            req_dmg_ef[no]++;
        }
    break;
    case 2:
        if (ew->se_area_no != -1)
        {
            SeStartGhost(56, no, 0, 0x1000, 0x1000, ew->se_area_no);
        }

        ene_wrk[no].sta |= 0x20000000;
        ene_wrk[no].sta |= 0x2000000;
        ene_wrk[no].sta |= 0x1000000;

        ene_wrk[no].tr_rate = 80;
        wait_time[no] = 120;
        req_dmg_ef[no]++;
    break;
    case 3:
        if (wait_time[no] == 0)
        {
            req_dmg_ef[no]++;
        }
        else
        {
            wait_time[no]--;
        }
    break;
    case 4:
        ene_wrk[no].sta &= ~0x20000000;
        ene_wrk[no].sta &= ~0x2000000;
        ene_wrk[no].sta &= ~0x1000000;

        req_dmg_ef[no] = 0;
    break;
    }
}

void EneNormalEffectCtrl(ENE_WRK *ew)
{
    sceVu0FVECTOR rv;
    u_char no;
    static u_char ne_job[3] = {0, 0, 0};
    static u_short time[3];
    static float spd[3];
    static float rate[3];
    static float trate[3];
    static sceVu0FVECTOR padj[3];
    static void *dp[3];

    no = ew->move_box.idx;

    if (
        plyr_wrk.mode != 1 || map_wrk.mirror_flg != 0 || EnemyUseJudge(0) > 1 ||
        ew->sta & 0x80000 || ew->ani_reso == 0 || ew->hp == 0
    )
    {
        if (ne_job[no] >= 2)
        {
            ResetEffects(dp[no]);
            ne_job[no] = 0;
        }

        return;
    }

    switch(ne_job[no])
    {
    case 0:
        time[no] = GetRndSP(60, 120);
        ne_job[no]++;
    break;
    case 1:
        if (time[no] != 0)
        {
            time[no]--;
        }
        else if (ew->tr_rate != 0)
        {
            ne_job[no]++;
        }
    break;
    case 2:
        spd[no] = GetRndSP(1, 2);
        rate[no] = 1.0f;
        trate[no] = 0.95f;
        time[no] = 50;

        ew->mpos.p9[0] = ew->bep[0];
        ew->mpos.p9[1] = ew->bep[1];
        ew->mpos.p9[2] = ew->bep[2];
        ew->mpos.p9[3] = ew->bep[3];

        dp[0] = SetEffects(0x1b, 4, 5, 0x28, 0.3f, 1.1f, ew->mpos.p9, 50, 0, 50, 0, &spd[no], &rate[no], &trate[no]);

        ne_job[no]++;

        padj[no][0] = 0.0f;
        padj[no][1] = -7.0f;
        padj[no][2] = 0.0f;
        padj[no][3] = 0.0f;
    break;
    case 3:
        if (time[no] == 0)
        {
            time[no] = 50;
            ne_job[no]++;
        }
        else
        {
            time[no]--;
            sceVu0AddVector(ew->mpos.p9, ew->mpos.p9, padj[no]);
        }
    break;
    case 4:
        if (time[no] == 0)
        {
            ne_job[no] = 0;
        }
        else
        {
            time[no]--;
        }
    break;
    }
}

char EndAutoEne(u_char type)
{
    char result;

    result = 0;

    if (ene_wrk[3].sta & 1)
    {
        if (type == 0)
        {
            ene_wrk[3].sta |= 0x800;
        }
        else
        {
            ene_wrk[3].sta |= 0x2000;

            EneWrkReleaseChk(&ene_wrk[3]);
        }

        result = 1;
    }

    return result;
}

void ReqEneSpeakAdpcmCnt(u_short time)
{
    es_adpcm_tm = time;
}

void EneSpeakAdpcmCtrl(ENE_WRK *ew)
{
    if (es_adpcm_tm != 0 && --es_adpcm_tm == 0 && ew->aie->adpcm_no != -1)
    {
        AdpcmPlayAutoGhost(ew->aie->adpcm_no, &ew->mpos.p0, MAX_VOLUME, ew->move_box.idx, 5);
    }
}

int ene_dead_load = 0;
int ene_dead_mode = 0;

void GhostDeadInit(int wrk_no)
{
    if (ene_wrk[wrk_no].type == 0)
    {
        if (SettleGhostBattleEnd() != 0)
        {
            ene_dead_load = 1;
        }
        else if (BindGhostBattleEnd(wrk_no) != 0)
        {
            ene_dead_load = 1;
            ene_dead_mode = 3;
            ingame_wrk.mode = 18;
        }
        else if (ingame_wrk.msn_no == 3 && ene_wrk[wrk_no].dat_no == 0)
        {
            DeadGhostLoadReq();
            ene_dead_load = 1;
            ene_dead_mode = 1;
            ingame_wrk.mode = 18;
        }
    }
    else if (ene_wrk[wrk_no].type == 1)
    {
        if (EnemyNoDeadGhostJudge(ene_wrk[wrk_no].dat_no) != 0)
        {
            return;
        }

        if (ene_wrk[wrk_no].dat_no == ap_wrk.ggst_no)
        {
            GuardGhostBattleEnd();
            return;
        }

        ingame_wrk.mode = 18;
        ene_dead_mode = 5;
        ap_wrk.fgst_cnt++;

        FloatGhostLoadReq();

        ene_dead_load = 1;
    }

}

void GhostDeadMain()
{
    static int gd_load_id;

    switch(ene_dead_mode)
    {
    case 0:
        ingame_wrk.mode = 6;
        ene_dead_load = 0;
    break;
    case 1:
        Mission03BindGhost01Delete();
        gd_load_id = SeFileLoadAndSet(SGY000_BD, SE_ADDRNO_BTLHIT);
        ene_dead_mode = 2;
    break;
    case 2:
        if (IsLoadEnd(gd_load_id) != 0)
        {
            ene_dead_mode = 4;
        }
    break;
    case 3:
        if (BindGhostLoad() != 0)
        {
            ingame_wrk.mode = 6;
            ene_dead_load = 0;
            ene_dead_mode = 0;
        }
    break;
    case 4:
        if (DeadGhostLoad() != 0)
        {
            ingame_wrk.mode = 6;
            ene_dead_load = 0;
            ene_dead_mode = 0;
        }
    break;
    case 5:
        if (FloatGhostLoadMain() != 0)
        {
            ingame_wrk.mode = 6;
            ene_dead_load = 0;
            ene_dead_mode = 0;
        }
    break;
    }
}

int GhostDeadLoadEndJudge()
{
    return ene_dead_load == 0;
}

int EnemyUseJudge(u_char ene_type)
{
    int i;
    int ene_num;
    int cnt;

    ene_num = ene_type != 0 ? 3 : 4;

    cnt = 0;

    for (i = 0; i < ene_num; i++)
    {
        cnt += ene_wrk[i].sta & 0x1;
    }

    return cnt;
}

int ZeroGhostBattleJudge()
{
    int i;
    int result;
    u_char no[3] = {0xff, 0xff, 0xff};

    result = 1;

    switch (ingame_wrk.msn_no)
    {
    case 1:
        no[0] = 11;
    break;
    case 2:
        no[0] = 19;
    break;
    case 4:
        no[0] = 1;
        no[1] = 2;
        no[2] = 8;
    break;
    default:
        result = 0;
    break;
    }

    if (result)
    {
        result = 0;

        for (i = 0; i < 3; i++)
        {
            if (ene_wrk[i].sta & 0x1)
            {
                if (no[0] == ene_wrk[i].dat_no || no[1] == ene_wrk[i].dat_no || no[2] == ene_wrk[i].dat_no)
                {
                    result = 1;
                    break;
                }
            }
        }
    }

    return result;
}

void EneAdpcmPlayChk(ENE_WRK *ew)
{
    if (adpcm_map.ghost.use == 0)
    {
        AdpcmPlayGhost(ew->dat->adpcm_no, &ew->move_box.pos, MAX_VOLUME, ew->move_box.idx, 200);
    }
}
