#ifndef OUTGAME_BTL_MODE_BTL_MENU_H
#define OUTGAME_BTL_MODE_BTL_MENU_H

#include "btl_mode.h"
#include "typedefs.h"
#include "ingame/event/ev_load.h"

typedef struct
{
    u_char *addr;
    int size;
} BTL_SAVE_STR;

typedef struct
{
    sceVu0FVECTOR pos;
    u_char floor;
} FREE_DAT;

extern MSN_LOAD_DAT stage_load_dat0[];
extern MSN_LOAD_DAT stage_load_dat1[];
extern MSN_LOAD_DAT stage_load_dat2[];
extern MSN_LOAD_DAT stage_load_dat3[];
extern MSN_LOAD_DAT stage_load_dat4[];
extern MSN_LOAD_DAT *stage_load_dat[];
extern FREE_DAT free_dat[];
extern BTL_SAVE_STR btl_save_str[];
extern u_long btl_save_str_num;

void FreeModeMain();
u_char FreeModeRoomNo();
void FreeModePosSet();
void BattleModeInit();
void ClearStageWrk();
void BattleModeMain();
int StageTitleInit();
int StageTitleLoad();
void StageGhostLoadReq();
void StageGhostLoadAfter();
void SaveStoryWrk();
void LoadStoryWrk();

#endif// OUTGAME_BTL_MODE_BTL_MENU_H
