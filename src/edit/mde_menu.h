#ifndef EDIT_MDE_MENU_H
#define EDIT_MDE_MENU_H

#include "typedefs.h"

typedef enum {
    MDE_MENU_PAGE_SUMMARY,
    MDE_MENU_PAGE_SECTION,
    MDE_MENU_PAGE_ROOM,
    MDE_MENU_PAGE_NUM
} MDE_MENU_PAGE;

typedef enum {
    MDE_MENU_ACT_NONE,
    MDE_MENU_ACT_LOAD,
    MDE_MENU_ACT_MARK_PASS,
    MDE_MENU_ACT_EXIT
} MDE_MENU_ACT;

typedef struct {
    int page;
    int csr;
    int mission_no;
    int floor_no;
    int section_no;
    int room_no;
    int data_no;
    int pos_x;
    int pos_z;
    const char *msg;
} MDE_MENU_WRK;

extern MDE_MENU_WRK mde_menu_wrk;

void MdeMenuInit(void);
void MdeMenuClamp(void);
int MdeMenuCtrl(void);
const char *MdeMenuPageName(int page);
int MdeMenuCurrentRoomId(void);

#endif // EDIT_MDE_MENU_H
