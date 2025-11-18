#include "item.h"
#include "common.h"
#include "ig_menu.h"

#include <graphics/graph2d/message.h>

void ItemCntInit(void) {
}

void NewgameItemInit()
{
}

void NewgameItemInit2()
{
}

void MissionSelectItemInit()
{
}

void MenuItemInit()
{
}

void LoadgameMenuItemInit()
{
}

void StartItemModeInit()
{
}

void ItemInitAtMenuOpen()
{
}

void FilmPossessionExp()
{
}

void IngameMenuItem()
{
}

void IngameMenuItemUseSlct()
{
}

void IngameMenuItemDisp()
{
}

void PlayerStatusYW(u_char alpha)
{
}

int LoadItem2D(u_char get_type, u_char get_no)
{
}

u_long VramItem2D(u_char mode, u_char type)
{
}

char DspItem2D(u_long tex_addr, float pos_x, float pos_y, int rgb, float alp, float scl_x, float scl_y, char pri)
{
}

void PkTm2(u_long tex_addr, short int pos_x, short int pos_y, short int w, short int h, u_char alp, int pri)
{
}

void PutSpriteYW(u_short top_label, u_short end_label, float pos_x, float pos_y, float rot, int rgb, float alp,
    float scl_x, float scl_y, u_char scl_mode, int pri, u_char by, u_char blnd, u_char z_sw)
{
}

void FlashStarYW(FLSH_CORE* flsh, u_char flsh_max, u_char flsh_min, u_char flsh_flm, u_char mode)
{
}

void CLRtoRGB(int* rgb, u_char r, u_char g, u_char b)
{
}

void RGBtoCLR(int rgb, u_char* r, u_char* g, u_char* b)
{
}

void PutStringYW(u_char msg_knd, u_char msg_no, short int pos_x, short int pos_y, int rgb, u_char alpha, int pri,
                 u_char type)
{
    /* 0x0(sp) */ DISP_STR ds;
    /* 0x40(sp) */ STR_DAT sd = {
        .str = NULL,
        .pos_x = 0,
        .pos_y = 0,
        .type = 0,
        .r = 0xFF,
        .g = 0xFF,
        .b = 0xFF,
        .alpha = 0x80,
        .pri = 0,
    };
    /* v1 3 */ int cnt_ofs;

    sd.str = (u_char *)GetIngameMSGAddr(msg_knd, msg_no);

    CopyStrDToStr(&ds, &sd);

    ds.r = (u_int)rgb >> 16 & 0xff;
    ds.g = ((u_int)rgb & 0xff00) >> 8;
    ds.b = (u_int)rgb & 0xff;

    ds.alpha = alpha;
    ds.pri = pri;

    switch (type)
    {
        case 0:
            ds.pos_x = pos_x;
        ds.pos_y = pos_y;
        break;
        case 1:
            cnt_ofs = ChkChrNumUS(msg_knd, msg_no);

        ds.pos_x = pos_x - cnt_ofs / 2;
        ds.pos_y = pos_y;
        break;
        case 2:
            cnt_ofs = ChkChrNumUS(msg_knd, msg_no);

        ds.pos_x = pos_x - cnt_ofs;
        ds.pos_y = pos_y;
        break;
    }

    SetMessageV2(&ds);
}

void RstMessageYW()
{
}

u_char PutMessageYW(u_char msg_knd, u_char msg_no, short int pos_x, short int pos_y, int rgb, u_char alpha, int pri)
{
}

char ChkChrNumYW(u_char msg_knd, u_char msg_no)
{
}

int ChkChrNumUS(u_char msg_knd, u_char msg_no)
{
    u_char *str = (u_char *) GetIngameMSGAddr(msg_knd, msg_no);

    return GetStrWidth(str);
}

char ChkPageYW(u_char msg_knd, u_char msg_no)
{
}

void PutPageYW(u_char msg_knd, u_char msg_no, u_char open_page, short int pos_x, short int pos_y, int rgb, u_char alpha,
    int pri)
{
}

void PutPage2YW(u_char msg_knd, u_char msg_no, u_char open_page, short int pos_x, short int pos_y, int rgb,
    u_char alpha, int pri)
{
}

void PutNumberYW(u_char font, int num, short int pos_x, short int pos_y, float sx, float sy, int rgb, short int alpha,
    int pri, int digit, int mode)
{
}

void PolySquareYW(float pos_x, float pos_y, u_short bar_l, u_short bar_h, int rgb, float alp, float scl_x, float scl_y,
    int pri, u_char blnd, u_char sw_z, short int rz)
{
}

void YesNoCrslOKR(int pri, float pos_x, float pos_y, int rgb, float alp, float scl)
{
}

void XYAdefaultYW(u_char no)
{
}

void BgFusumaYW(int rgb, float pos_x, float alpha, int pri)
{
}

u_int FromPKZ(u_int pkz_addr, u_char pk2_no)
{
}

void DcdCMP2PK2(u_int cmp_addr, u_int tmp_addr)
{
}
