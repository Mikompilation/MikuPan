#ifndef OUTGAME_MOT_MENU_H
#define OUTGAME_MOT_MENU_H

#include "typedefs.h"

typedef struct {
    u_short mdl_no;
    u_short anm_no;
    const char *name;
} MOT_MENU_ENTRY;

typedef enum {
    MOT_MENU_ACT_NONE = 0,
    MOT_MENU_ACT_EXIT = 1 << 0,
    MOT_MENU_ACT_RELOAD = 1 << 1,
    MOT_MENU_ACT_ENTRY_CHANGED = 1 << 2,
    MOT_MENU_ACT_ANIM_CHANGED = 1 << 3,
} MOT_MENU_ACTION;

typedef struct {
    int entry_no;
    int anim_no;
    int anim_num;
    int frame;
    int frame_num;
    int menu_csr;
    int play;
    int loop;
    int loaded;
    float rot_y;
    float height;
    float zoom;
    float light_power;
    const char *state_str;
} MOT_MENU_WRK;

void MotMenuInit(MOT_MENU_WRK *menu);
int MotMenuCtrl(MOT_MENU_WRK *menu);
void MotMenuDraw(MOT_MENU_WRK *menu);
int MotMenuEntryNum(void);
const MOT_MENU_ENTRY *MotMenuEntry(int entry_no);

#endif // OUTGAME_MOT_MENU_H
