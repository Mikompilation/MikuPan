#include "common.h"
#include "ig_glst.h"
#include <string.h>

GLIST_INDEX glist_index[108];
static MENU_GLST menu_glist;

void NewgameMenuGlstInit()
{	
    u_int i;

    memset(&menu_glist, 0, sizeof(menu_glist));
    for (i = 0; i < 108; i++)
    {
        glist_index[i].new_flg = 1;
        glist_index[i].disp_flg = 0;
    }
}

void StartGlstModeInit()
{
}

void IngameMenuGlst()
{
}

void IngameMenuGlstDisp()
{
}

void DspGlst(u_char alpha)
{
}

void DspLstOfs(u_short start, u_char alpha)
{
}

void PutRayName(u_char ray_knd, u_short ray_no, u_char ray_msn, short int pos_x, short int pos_y, int rgb, u_char alpha)
{
}

void DspGInf(u_char alpha)
{
}

void DspGdtl(u_char alpha)
{
}

void Sheet(u_short chr_lbl, short int pos_x, short int pos_y, u_short siz_x, u_short siz_y, u_char pri, u_char alp)
{
}

void Fuchidori(u_short chr_lbl, short int pos_x, short int pos_y, u_short siz_x, u_short siz_y, u_char pri, u_char alp)
{
}

void Naraberu(u_short chr_lbl, short int pos_x, short int pos_y, u_char dir, u_short dst, u_char pri, u_char alp)
{
}

void ReSetGhostList(PICTURE_WRK new_pic)
{
}

u_short GetGlistAllNum()
{
}

u_short GetGlistGNum()
{
}

void DispGlistPhoto(u_int line, u_char alpha)
{
}

char CheckGlistComplete(u_int list)
{
}

char CheckGlistRareGhost(u_int kind, u_int no)
{
}
