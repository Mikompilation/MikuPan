#ifndef MAIN_GLOB_H
#define MAIN_GLOB_H

#include "typedefs.h"

#include "glob_types.h"
#include "ingame/camera/camera_types.h"

#define DPAD_UP_PRESSED() *key_now[0]
#define DPAD_DOWN_PRESSED() *key_now[1]
#define DPAD_LEFT_PRESSED() *key_now[2]
#define DPAD_RIGHT_PRESSED() *key_now[3]
#define TRIANGLE_PRESSED() *key_now[4]
#define CROSS_PRESSED() *key_now[5]
#define SQUARE_PRESSED() *key_now[6]
#define CIRCLE_PRESSED() *key_now[7]
#define L1_PRESSED() *key_now[8]
#define L2_PRESSED() *key_now[9]
#define R1_PRESSED() *key_now[10]
#define R2_PRESSED() *key_now[11]
#define START_PRESSED() *key_now[12]
#define SELECT_PRESSED() *key_now[13]
#define L3_PRESSED() *key_now[14]
#define R3_PRESSED() *key_now[15]

extern u_int realtime_scene_flg;
extern u_int scene_bg_load_flg;
extern SYS_WRK sys_wrk;
extern TIME_WRK time_wrk;
extern OPTION_WRK opt_wrk;
extern OUTGAME_WRK outgame_wrk;
extern PAD_STRUCT pad[2];
extern u_short *key_now[32];
extern u_short *key_bak[32];
extern SgCAMERA camera;
extern INGAME_WRK ingame_wrk;
extern PLYR_WRK plyr_wrk;
extern ENE_WRK ene_wrk[4];
extern MAP_WRK map_wrk;
extern ROOM_WRK room_wrk;
extern u_char poss_item[200];
extern u_char poss_file[4][50];
extern u_char flm_exp_flg[5];
extern MAP_CAM_DAT map_cam_dat[500];
extern MAP_CAM_DAT map_cam_dat2[500];
extern MAP_CAM_DAT map_cam_dat3[300];
extern MAP_CAM_DAT map_cam_dat4[300];
extern PHOTO_WRK photo_wrk;
extern PFILE_WRK pfile_wrk;
extern CAM_CUSTOM_WRK cam_custom_wrk;
extern DEBUG_NUMBERS dbg_wrk;

#endif // MAIN_GLOB_H
