
#include "common.h"
#include "typedefs.h"
#include "mc.h"

#include "main/glob.h"
#include "ingame/entry/ap_pgost.h"
#include "ingame/entry/entry.h"
#include "ingame/event/ev_load.h"
#include "ingame/event/ev_main.h"
#include "ingame/event/wan_soul.h"
#include "ingame/ig_glob.h"
#include "ingame/map/find_ctl.h"
#include "ingame/map/furn_spe/furn_spe.h"
#include "ingame/map/item_ctl.h"
#include "ingame/map/map_area.h"
#include "ingame/map/map_ctrl.h"
#include "ingame/menu/ig_glst.h"
#include "ingame/menu/ig_rank.h"
#include "outgame/btl_mode/btl_mode.h"
#include "outgame/memory_album.h"
#include "outgame/mode_slct.h"
#include "mc/mc_main.h"

MC_DATA_STR mc_gamedata_str[] = {
    MC_DATA_HOST(&mc_header,      sizeof(mc_header)),
    MC_DATA_HOST(&opt_wrk,        sizeof(opt_wrk)),
    MC_DATA_HOST(&ingame_wrk,     sizeof(ingame_wrk)),
    MC_DATA_HOST(load_dat_wrk,    sizeof(load_dat_wrk)),
    MC_DATA_HOST(&time_wrk,       sizeof(time_wrk)),
    MC_DATA_HOST(&plyr_wrk,       sizeof(plyr_wrk)),
    MC_DATA_HOST(ene_wrk,         sizeof(ene_wrk)),
    MC_DATA_HOST(&map_wrk,        sizeof(map_wrk)),
    MC_DATA_HOST(&ap_wrk,         sizeof(ap_wrk)),
    MC_DATA_HOST(&ev_wrk,         sizeof(ev_wrk)),
    MC_DATA_HOST(event_stts,      sizeof(event_stts)),
    MC_DATA_HOST(find_stts,       sizeof(find_stts)),
    MC_DATA_HOST(poss_item,       sizeof(poss_item)),
    MC_DATA_HOST(poss_file,       sizeof(poss_file)),
    MC_DATA_HOST(flm_exp_flg,     sizeof(flm_exp_flg)),
    MC_DATA_HOST(item_ap,         sizeof(item_ap)),
    MC_DATA_HOST(&area_wrk,       sizeof(area_wrk)),
    MC_DATA_HOST(furn_attr_flg,   sizeof(furn_attr_flg)),
    MC_DATA_HOST(door_keep,       sizeof(door_keep)),
    MC_DATA_HOST(room_pass,       sizeof(room_pass)),
    MC_DATA_HOST(&pg_wrk,         sizeof(pg_wrk)),
    MC_DATA_HOST(&cam_custom_wrk, sizeof(cam_custom_wrk)),
    MC_DATA_HOST(f_dat_save,      sizeof(f_dat_save)),
    MC_DATA_HOST(wander_soul_wrk, sizeof(wander_soul_wrk)),
    MC_DATA_HOST(glist_index,     sizeof(glist_index)),
    MC_DATA_HOST(&cribo,          sizeof(cribo)),
    MC_DATA_HOST(stage_wrk,       sizeof(stage_wrk)),
    MC_DATA_HOST(&save_rank,      sizeof(save_rank)),
    MC_DATA_PS2(0x01a9f000,       0x6400),
    MC_DATA_PS2(0x01be2380,       0x84120),
};

MC_DATA_STR mc_albumdata_str[] = {
    MC_DATA_HOST(&mc_header,     sizeof(mc_header)),
    MC_DATA_HOST(&mc_album_save, sizeof(mc_album_save)),

    /// dm_albm.cpy_addr
    MC_DATA_PS2(0x005a0000,     0xF000),
    MC_DATA_PS2(0x005b5400,     0x13CF80),
};

MC_DATA_STR mc_albumdata2_str[] = {
    MC_DATA_HOST(&mc_header,     sizeof(mc_header)),
    MC_DATA_HOST(&mc_album_save, sizeof(mc_album_save)),
    MC_DATA_PS2(0x01a90000,      0xF000),

    /// BUFFER_PIC_DST_ADDRESS
    MC_DATA_PS2(0x01aa5400,      0x13CF80),
};

u_long mc_gamedata_str_num = 30;
u_long mc_albumdata_str_num = 4;
