#include "common.h"
#include "ig_menu.h"

#include <common/memory_addresses.h>
#include <stdint.h>

/* data 33d8c8 */ STR_DAT ig_menu_str;

void NewgameMenuInit()
{
}

void LoadgameMenuInit()
{
}

void IngameMenuInit()
{
}

void IngameMenuOpenStart()
{
}

void IngameMenuMain()
{
}

void IngameMenuModeSlct()
{
}

int Get4Byte(u_char* addr)
{
    return ((u_int)addr[3] << 24) + ((u_int)addr[2] << 16) + ((u_int)addr[1] << 8) + (u_int)*addr;
}

uint64_t GetIngameMSGAddr(u_char type, int msg_no)
{
    int64_t addr;

    addr = ((u_char*)IG_MSG_OBJ_ADDRESS) + type * 4;
    addr = ((u_char*)IG_MSG_OBJ_ADDRESS) + Get4Byte((u_char *)addr) + msg_no * 4;
    addr = Get4Byte((u_char *)addr);

    return ((u_char*)IG_MSG_OBJ_ADDRESS) + addr;
}

void IngameMenuModeSlctDispInit()
{
}

void IngameMenuModeSlctDisp()
{
}

void DspTopMenu(short int pos_x, short int pos_y, short int alpha, u_short anm)
{
}

void WakuWaku(short int pos_x, short int pos_y, u_char alp, u_char num, u_char pri)
{
}

void WakuWaku2(short int pos_x, short int pos_y, u_char alp, u_char num, u_char pri)
{
}

void DspPlayData(short int pos_x, short int pos_y, float alpha, u_char str_mode)
{
}

void DspMenuTitle(short int pos_x, short int pos_y, float alp, u_char pri, u_char mode)
{
}

char SimpleMoveRuler(float* rslt, u_short t1, u_short t2, u_short t3, u_short dst, u_short* cnt)
{
}

char MoveRuler(short int* mov_x, short int* mov_y, short int x0, short int y0, short int x1, short int y1, u_short t1,
    u_short t2, int t3, u_short* cnt)
{
}

void CmnWindow(u_char win_no, short int pos_x, short int pos_y, u_char alp, u_char rgb)
{
}

void CmnCursol(short int pos_x, short int pos_y, short int wide, short int high, float flsh, u_char alp, int pri)
{
}

u_short Ana2PadDirCnt(u_char chk)
{
    return 0;
    //return ana_pad.on_cnt[chk];
}

void AnaPonChk()
{
}

char CanYouOpenMyMind(char mode)
{
}

int StopCameraMenu()
{
}
