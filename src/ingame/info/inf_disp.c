#include "common.h"
#include "typedefs.h"
#include "enums.h"
#include "inf_disp.h"

#include "graphics/graph2d/effect.h"
#include "graphics/graph2d/effect_ene.h"
#include "graphics/graph2d/effect_sub.h"
#include "graphics/graph2d/message.h"
#include "graphics/graph2d/number.h"
// #include "graphics/graph2d/tim2.h" // (miss) DispSprD
#include "graphics/graph3d/sglib.h"
#include "ingame/event/ev_main.h"
#include "ingame/info/inf_disp.h"
#include "ingame/menu/ig_menu.h"
#include "ingame/menu/item.h"
#include "ingame/plyr/plyr_ctl.h"
#include "main/glob.h"
#include "os/eeiop/eese.h"
#include "outgame/btl_mode/btl_mode.h"
#include "ingame/plyr/plyr_ctl.h"


static void FndrInit();
static void SttsFade();
static void FndrFade();
static void WeakPoint(short int pos_x, short int pos_y);
static void DspBigCircle(u_short lu_chr, short int pos_x, short int pos_y, u_char alp, short int size_r, u_char cl_ptn);
static void PointerNP(short int cx, short int cy, u_char red, u_char alp, float siz);
static void PointerEN(short int pos_x, short int pos_y, u_char red, u_char alp, float size);
static void FilmZansu(int number, short int pos_x, short int pos_y, short int num_x, short int num_y, int digit);
static void NumDisp(u_char number, u_char no, short int pos_x, short int pos_y, short int num_x, short int num_y, u_char mode);
static void EdogawaLamp(short int pos_x, short int pos_y, u_char out);
static void NewFndrBase(short int pos_x, short int pos_y);
static void BigBon(u_char bon_num, short int sx, short int sy, short int ex, short int ey, u_char mov_tim);
static void BigBon2(u_char bon_num, short int sx, short int sy, short int ex, short int ey, u_char mov_tim);
static char GetEnePoint();
static void MafubaInit(float t2_s1, float t2_s2);
static u_char Mafuba(short int pos_x, short int pos_y, u_char stts, u_char t1, u_char t2, u_char t3, u_char t4, u_char t5, u_char t1_sw, float t2_s1, float t2_s2);
static void MafubaFinish(short int pos_x, short int pos_y, u_char start, u_char time);
static void DameEffNumber(u_char type, u_short num, short int pos_x, short int pos_y, u_char stts, u_char num_alp, u_char num_siz_x, u_char num_siz_y);
static void SndEffLoop();
static void ShowEneCtrl(short int pos_x, short int pos_y);
static char EneDamegeCtrl(short int pos_x, short int pos_y);
static void ShowEnePower(u_char blu_bar, u_char red_bar, short int pos_x, short int pos_y, u_char alp, u_char bar_over);
static void Koutakun(u_char blu_bar, short int pos_x, short int pos_y, u_char alp);
static void ShowMikuPower(short int pos_x, short int pos_y);
static void ShowMikuPower2(short int pos_x, short int pos_y);
static void DspKotaku(u_short chr, short int pos_x, short int pos_y, u_char alp, u_char dsp_u, u_char dsp_w, u_short dsp_x);
static void GayBar(u_short chr, short int pos_x, short int pos_y, u_char alp, float scl);
static void GayBar2(u_short top_chr, u_short end_chr, short int pos_x, short int pos_y, u_char alp, float scl);
static void JetCD(JET_SET *ninjin, u_char num, u_char tim, u_char spd, u_char wid, u_short chr, u_char hgh, short int mov_h, short int mov_v, short int px, short int base, u_char alp);
static void SyonenJet(u_short chr, u_char hgh, short int mov_h, short int mov_v, short int px, short int base, u_char alp, float scl_x, float scl_y);
static void TameKin2(short int pos_x, short int pos_y);
static void ZanzoSS(ZAN *zz);
static void ZanzoSM(ZAN *zz, u_char num1, u_char num2, u_char num3, u_char num4, u_char mode);
static void ZanzoLS(u_char *c);
static void ZanzoLM(u_char *zz, u_char per);
static void AhoBon(u_short chr, short int px, short int py, u_char rot, u_char rgb, u_char alp, u_char siz, u_char now_num, u_char ini_num);
static void BakBon(u_short chr, short int px, short int py, u_char flr);
static void PhotoScoreDisp(int count, int wait);
static void BukiyouMan(short int px, short int py);
static void DameDameInit();
static void GekisyaBoyGeisyaGirl(short int px, short int py);
static char KanKinoSyogai(short int pos_x, short int pos_y);

SPRT_DAT spr_dat[] = {
    {
        .tex0 = 2306907363936967232,
        .u = 1,
        .v = 1,
        .w = 338,
        .h = 480,
        .x = -19,
        .y = -16,
        .pri = 184320,
        .alpha = 128,
    },
    {
        .tex0 = 2306907363936967232,
        .u = 1,
        .v = 1,
        .w = 338,
        .h = 480,
        .x = 319,
        .y = -16,
        .pri = 184320,
        .alpha = 128,
    },
    {
        .tex0 = 2306925503664168516,
        .u = 2,
        .v = 2,
        .w = 152,
        .h = 105,
        .x = 28,
        .y = 335,
        .pri = 122880,
        .alpha = 128,
    },
    {
        .tex0 = 2306928251303403208,
        .u = 1,
        .v = 0,
        .w = 22,
        .h = 31,
        .x = 41,
        .y = 377,
        .pri = 102400,
        .alpha = 128,
    },
    {
        .tex0 = 2306928251303403208,
        .u = 23,
        .v = 0,
        .w = 22,
        .h = 31,
        .x = 41,
        .y = 377,
        .pri = 102400,
        .alpha = 128,
    },
    {
        .tex0 = 2306928251303403208,
        .u = 44,
        .v = 0,
        .w = 22,
        .h = 31,
        .x = 41,
        .y = 377,
        .pri = 102400,
        .alpha = 128,
    },
    {
        .tex0 = 2306928251303403208,
        .u = 67,
        .v = 0,
        .w = 22,
        .h = 31,
        .x = 41,
        .y = 377,
        .pri = 102400,
        .alpha = 128,
    },
    {
        .tex0 = 2306928251303403208,
        .u = 89,
        .v = 0,
        .w = 22,
        .h = 31,
        .x = 41,
        .y = 377,
        .pri = 102400,
        .alpha = 128,
    },
    {
        .tex0 = 2306928251303403208,
        .u = 1,
        .v = 32,
        .w = 22,
        .h = 31,
        .x = 41,
        .y = 377,
        .pri = 102400,
        .alpha = 128,
    },
    {
        .tex0 = 2306928251303403208,
        .u = 23,
        .v = 32,
        .w = 22,
        .h = 31,
        .x = 41,
        .y = 377,
        .pri = 102400,
        .alpha = 128,
    },
    {
        .tex0 = 2306928251303403208,
        .u = 45,
        .v = 32,
        .w = 22,
        .h = 31,
        .x = 41,
        .y = 377,
        .pri = 102400,
        .alpha = 128,
    },
    {
        .tex0 = 2306928251303403208,
        .u = 67,
        .v = 32,
        .w = 22,
        .h = 31,
        .x = 41,
        .y = 377,
        .pri = 102400,
        .alpha = 128,
    },
    {
        .tex0 = 2306928251303403208,
        .u = 89,
        .v = 32,
        .w = 22,
        .h = 31,
        .x = 41,
        .y = 377,
        .pri = 102400,
        .alpha = 128,
    },
    {
        .tex0 = 2306925503664168516,
        .u = 156,
        .v = 1,
        .w = 74,
        .h = 63,
        .x = 31,
        .y = 362,
        .pri = 81920,
        .alpha = 128,
    },
    {
        .tex0 = 2306990511149326477,
        .u = 1,
        .v = 1,
        .w = 18,
        .h = 20,
        .x = 0,
        .y = 0,
        .pri = 53248,
        .alpha = 128,
    },
    {
        .tex0 = 2306990511149326477,
        .u = 19,
        .v = 1,
        .w = 18,
        .h = 20,
        .x = 0,
        .y = 0,
        .pri = 53248,
        .alpha = 128,
    },
    {
        .tex0 = 2306990511149326477,
        .u = 37,
        .v = 1,
        .w = 18,
        .h = 20,
        .x = 0,
        .y = 0,
        .pri = 53248,
        .alpha = 128,
    },
    {
        .tex0 = 2306990511149326477,
        .u = 55,
        .v = 1,
        .w = 18,
        .h = 20,
        .x = 0,
        .y = 0,
        .pri = 53248,
        .alpha = 128,
    },
    {
        .tex0 = 2306990511149326477,
        .u = 73,
        .v = 1,
        .w = 18,
        .h = 20,
        .x = 0,
        .y = 0,
        .pri = 53248,
        .alpha = 128,
    },
    {
        .tex0 = 2306990511149326477,
        .u = 1,
        .v = 21,
        .w = 18,
        .h = 20,
        .x = 0,
        .y = 0,
        .pri = 53248,
        .alpha = 128,
    },
    {
        .tex0 = 2306990511149326477,
        .u = 19,
        .v = 21,
        .w = 18,
        .h = 20,
        .x = 0,
        .y = 0,
        .pri = 53248,
        .alpha = 128,
    },
    {
        .tex0 = 2306990511149326477,
        .u = 37,
        .v = 21,
        .w = 18,
        .h = 20,
        .x = 0,
        .y = 0,
        .pri = 53248,
        .alpha = 128,
    },
    {
        .tex0 = 2306990511149326477,
        .u = 55,
        .v = 21,
        .w = 18,
        .h = 20,
        .x = 0,
        .y = 0,
        .pri = 53248,
        .alpha = 128,
    },
    {
        .tex0 = 2306990511149326477,
        .u = 73,
        .v = 21,
        .w = 18,
        .h = 20,
        .x = 0,
        .y = 0,
        .pri = 53248,
        .alpha = 128,
    },
    {
        .tex0 = 2306932787862609625,
        .u = 49,
        .v = 69,
        .w = 48,
        .h = 14,
        .x = 57,
        .y = 344,
        .pri = 53248,
        .alpha = 128,
    },
    {
        .tex0 = 2306992847611535518,
        .u = 1,
        .v = 1,
        .w = 100,
        .h = 50,
        .x = 9,
        .y = 312,
        .pri = 40960,
        .alpha = 128,
    },
    {
        .tex0 = 2306932787862609625,
        .u = 1,
        .v = 1,
        .w = 22,
        .h = 32,
        .x = 37,
        .y = 3,
        .pri = 61440,
        .alpha = 128,
    },
    {
        .tex0 = 2306932787862609625,
        .u = 25,
        .v = 1,
        .w = 22,
        .h = 32,
        .x = 60,
        .y = 3,
        .pri = 61440,
        .alpha = 128,
    },
    {
        .tex0 = 2306932787862609625,
        .u = 49,
        .v = 1,
        .w = 22,
        .h = 32,
        .x = 83,
        .y = 2,
        .pri = 61440,
        .alpha = 128,
    },
    {
        .tex0 = 2306932787862609625,
        .u = 73,
        .v = 1,
        .w = 22,
        .h = 32,
        .x = 106,
        .y = 4,
        .pri = 61440,
        .alpha = 128,
    },
    {
        .tex0 = 2306932787862609625,
        .u = 97,
        .v = 1,
        .w = 22,
        .h = 32,
        .x = 129,
        .y = 3,
        .pri = 61440,
        .alpha = 128,
    },
    {
        .tex0 = 2306932787862609625,
        .u = 1,
        .v = 35,
        .w = 22,
        .h = 32,
        .x = 154,
        .y = 3,
        .pri = 61440,
        .alpha = 128,
    },
    {
        .tex0 = 2306932787862609625,
        .u = 25,
        .v = 35,
        .w = 22,
        .h = 32,
        .x = 177,
        .y = 3,
        .pri = 61440,
        .alpha = 128,
    },
    {
        .tex0 = 2306932787862609625,
        .u = 49,
        .v = 35,
        .w = 22,
        .h = 32,
        .x = 199,
        .y = 3,
        .pri = 61440,
        .alpha = 128,
    },
    {
        .tex0 = 2306932787862609625,
        .u = 73,
        .v = 35,
        .w = 22,
        .h = 32,
        .x = 223,
        .y = 3,
        .pri = 61440,
        .alpha = 128,
    },
    {
        .tex0 = 2306932787862609625,
        .u = 97,
        .v = 35,
        .w = 22,
        .h = 32,
        .x = 245,
        .y = 3,
        .pri = 61440,
        .alpha = 128,
    },
    {
        .tex0 = 2306932787862609625,
        .u = 1,
        .v = 69,
        .w = 22,
        .h = 32,
        .x = 269,
        .y = 3,
        .pri = 61440,
        .alpha = 128,
    },
    {
        .tex0 = 2306932787862609625,
        .u = 25,
        .v = 69,
        .w = 22,
        .h = 32,
        .x = 292,
        .y = 2,
        .pri = 61440,
        .alpha = 128,
    },
    {
        .tex0 = 2307015251233644817,
        .u = 0,
        .v = 1,
        .w = 32,
        .h = 32,
        .x = 0,
        .y = 0,
        .pri = 40960,
        .alpha = 128,
    },
    {
        .tex0 = 2307015251233644817,
        .u = 32,
        .v = 1,
        .w = 32,
        .h = 32,
        .x = 0,
        .y = 0,
        .pri = 40960,
        .alpha = 128,
    },
    {
        .tex0 = 2307015251233644817,
        .u = 64,
        .v = 1,
        .w = 32,
        .h = 32,
        .x = 0,
        .y = 0,
        .pri = 40960,
        .alpha = 128,
    },
    {
        .tex0 = 2307015251233644817,
        .u = 96,
        .v = 1,
        .w = 32,
        .h = 32,
        .x = 0,
        .y = 0,
        .pri = 40960,
        .alpha = 128,
    },
    {
        .tex0 = 2307015251233644817,
        .u = 0,
        .v = 33,
        .w = 32,
        .h = 32,
        .x = 0,
        .y = 0,
        .pri = 40960,
        .alpha = 128,
    },
    {
        .tex0 = 2307015251233644817,
        .u = 32,
        .v = 33,
        .w = 32,
        .h = 32,
        .x = 0,
        .y = 0,
        .pri = 40960,
        .alpha = 128,
    },
    {
        .tex0 = 2307015251233644817,
        .u = 64,
        .v = 33,
        .w = 32,
        .h = 32,
        .x = 0,
        .y = 0,
        .pri = 40960,
        .alpha = 128,
    },
    {
        .tex0 = 2307015251233644817,
        .u = 96,
        .v = 33,
        .w = 32,
        .h = 32,
        .x = 0,
        .y = 0,
        .pri = 40960,
        .alpha = 128,
    },
    {
        .tex0 = 2307015251233644817,
        .u = 0,
        .v = 65,
        .w = 32,
        .h = 32,
        .x = 0,
        .y = 0,
        .pri = 40960,
        .alpha = 128,
    },
    {
        .tex0 = 2307015251233644817,
        .u = 32,
        .v = 65,
        .w = 32,
        .h = 32,
        .x = 0,
        .y = 0,
        .pri = 40960,
        .alpha = 128,
    },
    {
        .tex0 = 2307015251233644817,
        .u = 64,
        .v = 65,
        .w = 32,
        .h = 32,
        .x = 0,
        .y = 0,
        .pri = 40960,
        .alpha = 128,
    },
    {
        .tex0 = 2307015251233644817,
        .u = 96,
        .v = 65,
        .w = 32,
        .h = 32,
        .x = 0,
        .y = 0,
        .pri = 40960,
        .alpha = 128,
    },
    {
        .tex0 = 2307006317702717680,
        .u = 3,
        .v = 0,
        .w = 22,
        .h = 28,
        .x = 107,
        .y = 4,
        .pri = 143360,
        .alpha = 128,
    },
    {
        .tex0 = 2307006317702717680,
        .u = 25,
        .v = 0,
        .w = 22,
        .h = 28,
        .x = 130,
        .y = 4,
        .pri = 143360,
        .alpha = 128,
    },
    {
        .tex0 = 2307006317702717680,
        .u = 48,
        .v = 0,
        .w = 22,
        .h = 28,
        .x = 153,
        .y = 4,
        .pri = 143360,
        .alpha = 128,
    },
    {
        .tex0 = 2307006317702717680,
        .u = 73,
        .v = 0,
        .w = 22,
        .h = 28,
        .x = 177,
        .y = 4,
        .pri = 143360,
        .alpha = 128,
    },
    {
        .tex0 = 2307006317702717680,
        .u = 97,
        .v = 0,
        .w = 22,
        .h = 28,
        .x = 199,
        .y = 4,
        .pri = 143360,
        .alpha = 128,
    },
    {
        .tex0 = 2307006317702717680,
        .u = 1,
        .v = 30,
        .w = 22,
        .h = 28,
        .x = 223,
        .y = 4,
        .pri = 143360,
        .alpha = 128,
    },
    {
        .tex0 = 2307006317702717680,
        .u = 25,
        .v = 30,
        .w = 22,
        .h = 28,
        .x = 83,
        .y = 4,
        .pri = 143360,
        .alpha = 128,
    },
    {
        .tex0 = 2307006317702717680,
        .u = 49,
        .v = 30,
        .w = 22,
        .h = 28,
        .x = 247,
        .y = 4,
        .pri = 143360,
        .alpha = 128,
    },
    {
        .tex0 = 2307006317702717680,
        .u = 73,
        .v = 30,
        .w = 22,
        .h = 28,
        .x = 61,
        .y = 4,
        .pri = 143360,
        .alpha = 128,
    },
    {
        .tex0 = 2307006317702717680,
        .u = 97,
        .v = 30,
        .w = 22,
        .h = 28,
        .x = 269,
        .y = 4,
        .pri = 143360,
        .alpha = 128,
    },
    {
        .tex0 = 2307006317702717680,
        .u = 2,
        .v = 59,
        .w = 22,
        .h = 28,
        .x = 38,
        .y = 4,
        .pri = 143360,
        .alpha = 128,
    },
    {
        .tex0 = 2307006317702717680,
        .u = 25,
        .v = 59,
        .w = 22,
        .h = 28,
        .x = 292,
        .y = 3,
        .pri = 143360,
        .alpha = 128,
    },
    {
        .tex0 = 2307020199037018453,
        .u = 1,
        .v = 1,
        .w = 126,
        .h = 126,
        .x = 0,
        .y = 0,
        .pri = 61440,
        .alpha = 128,
    },
    {
        .tex0 = 2306907363936967232,
        .u = 343,
        .v = 3,
        .w = 59,
        .h = 107,
        .x = 35,
        .y = 256,
        .pri = 102400,
        .alpha = 128,
    },
    {
        .tex0 = 2306907363936967232,
        .u = 451,
        .v = 2,
        .w = 34,
        .h = 32,
        .x = 45,
        .y = 321,
        .pri = 81920,
        .alpha = 128,
    },
    {
        .tex0 = 2306954777021423497,
        .u = 1,
        .v = 1,
        .w = 126,
        .h = 24,
        .x = 107,
        .y = 67,
        .pri = 122880,
        .alpha = 128,
    },
    {
        .tex0 = 2306954777021423497,
        .u = 1,
        .v = 27,
        .w = 31,
        .h = 24,
        .x = 233,
        .y = 67,
        .pri = 122880,
        .alpha = 128,
    },
    {
        .tex0 = 2306955049617629082,
        .u = 1,
        .v = 1,
        .w = 16,
        .h = 7,
        .x = 109,
        .y = 81,
        .pri = 81920,
        .alpha = 128,
    },
    {
        .tex0 = 2306955049617629082,
        .u = 1,
        .v = 1,
        .w = 16,
        .h = 7,
        .x = 177,
        .y = 81,
        .pri = 77824,
        .alpha = 128,
    },
    {
        .tex0 = 2306959587383746460,
        .u = 0,
        .v = 1,
        .w = 128,
        .h = 6,
        .x = -19,
        .y = 81,
        .pri = 61440,
        .alpha = 128,
    },
    {
        .tex0 = 2306907363936967232,
        .u = 375,
        .v = 222,
        .w = 136,
        .h = 288,
        .x = 479,
        .y = 160,
        .pri = 81920,
        .alpha = 128,
    },
    {
        .tex0 = 2306987763442950217,
        .u = 1,
        .v = 1,
        .w = 92,
        .h = 90,
        .x = 507,
        .y = 357,
        .pri = 4096,
        .alpha = 128,
    },
    {
        .tex0 = 2306978417661255621,
        .u = 169,
        .v = 43,
        .w = 40,
        .h = 40,
        .x = 534,
        .y = 384,
        .pri = 61440,
        .alpha = 128,
    },
    {
        .tex0 = 2306978417661255621,
        .u = 127,
        .v = 43,
        .w = 40,
        .h = 40,
        .x = 534,
        .y = 384,
        .pri = 61440,
        .alpha = 128,
    },
    {
        .tex0 = 2306978417661255621,
        .u = 85,
        .v = 43,
        .w = 40,
        .h = 40,
        .x = 534,
        .y = 384,
        .pri = 61440,
        .alpha = 128,
    },
    {
        .tex0 = 2306978417661255621,
        .u = 43,
        .v = 43,
        .w = 40,
        .h = 40,
        .x = 534,
        .y = 384,
        .pri = 61440,
        .alpha = 128,
    },
    {
        .tex0 = 2306978417661255621,
        .u = 1,
        .v = 43,
        .w = 40,
        .h = 40,
        .x = 534,
        .y = 384,
        .pri = 61440,
        .alpha = 128,
    },
    {
        .tex0 = 2306978417661255621,
        .u = 169,
        .v = 1,
        .w = 40,
        .h = 40,
        .x = 534,
        .y = 384,
        .pri = 61440,
        .alpha = 128,
    },
    {
        .tex0 = 2306978417661255621,
        .u = 127,
        .v = 1,
        .w = 40,
        .h = 40,
        .x = 534,
        .y = 384,
        .pri = 61440,
        .alpha = 128,
    },
    {
        .tex0 = 2306978417661255621,
        .u = 85,
        .v = 1,
        .w = 40,
        .h = 40,
        .x = 534,
        .y = 384,
        .pri = 61440,
        .alpha = 128,
    },
    {
        .tex0 = 2306978417661255621,
        .u = 43,
        .v = 1,
        .w = 40,
        .h = 40,
        .x = 534,
        .y = 384,
        .pri = 61440,
        .alpha = 128,
    },
    {
        .tex0 = 2306978417661255621,
        .u = 1,
        .v = 1,
        .w = 40,
        .h = 40,
        .x = 534,
        .y = 384,
        .pri = 61440,
        .alpha = 128,
    },
    {
        .tex0 = 2306907363936967232,
        .u = 343,
        .v = 111,
        .w = 168,
        .h = 46,
        .x = 143,
        .y = 388,
        .pri = 163840,
        .alpha = 128,
    },
    {
        .tex0 = 2306907363936967232,
        .u = 343,
        .v = 158,
        .w = 168,
        .h = 40,
        .x = 311,
        .y = 387,
        .pri = 163840,
        .alpha = 128,
    },
    {
        .tex0 = 2306907363936967232,
        .u = 403,
        .v = 50,
        .w = 19,
        .h = 46,
        .x = 479,
        .y = 387,
        .pri = 163840,
        .alpha = 128,
    },
    {
        .tex0 = 2306907363936967232,
        .u = 424,
        .v = 50,
        .w = 64,
        .h = 40,
        .x = 288,
        .y = 28,
        .pri = 122880,
        .alpha = 128,
    },
    {
        .tex0 = 2306959587383746460,
        .u = 1,
        .v = 11,
        .w = 62,
        .h = 48,
        .x = 288,
        .y = 23,
        .pri = 81920,
        .alpha = 128,
    },
    {
        .tex0 = 2306943095784120100,
        .u = 1,
        .v = 1,
        .w = 126,
        .h = 126,
        .x = 194,
        .y = 99,
        .pri = 102400,
        .alpha = 128,
    },
    {
        .tex0 = 2306943095784120100,
        .u = 1,
        .v = 1,
        .w = 126,
        .h = 126,
        .x = 320,
        .y = 99,
        .pri = 102400,
        .alpha = 128,
    },
    {
        .tex0 = 2306943095784120100,
        .u = 1,
        .v = 1,
        .w = 126,
        .h = 126,
        .x = 194,
        .y = 225,
        .pri = 102400,
        .alpha = 128,
    },
    {
        .tex0 = 2306943095784120100,
        .u = 1,
        .v = 1,
        .w = 126,
        .h = 126,
        .x = 320,
        .y = 225,
        .pri = 102400,
        .alpha = 128,
    },
    {
        .tex0 = 2306952029315047237,
        .u = 1,
        .v = 1,
        .w = 126,
        .h = 126,
        .x = 194,
        .y = 99,
        .pri = 122880,
        .alpha = 128,
    },
    {
        .tex0 = 2306952029315047237,
        .u = 1,
        .v = 1,
        .w = 126,
        .h = 126,
        .x = 320,
        .y = 99,
        .pri = 122880,
        .alpha = 128,
    },
    {
        .tex0 = 2306952029315047237,
        .u = 1,
        .v = 1,
        .w = 126,
        .h = 126,
        .x = 194,
        .y = 225,
        .pri = 122880,
        .alpha = 128,
    },
    {
        .tex0 = 2306952029315047237,
        .u = 1,
        .v = 1,
        .w = 126,
        .h = 126,
        .x = 320,
        .y = 225,
        .pri = 122880,
        .alpha = 128,
    },
    {
        .tex0 = 2306934023672340218,
        .u = 1,
        .v = 1,
        .w = 62,
        .h = 62,
        .x = 289,
        .y = 193,
        .pri = 81920,
        .alpha = 128,
    },
    {
        .tex0 = 2306938560298655491,
        .u = 1,
        .v = 1,
        .w = 68,
        .h = 68,
        .x = 286,
        .y = 190,
        .pri = 102400,
        .alpha = 128,
    },
    {
        .tex0 = 2307098813043614609,
        .u = 1,
        .v = 1,
        .w = 22,
        .h = 35,
        .x = 588,
        .y = 166,
        .pri = 81920,
        .alpha = 128,
    },
    {
        .tex0 = 2307098813043614609,
        .u = 49,
        .v = 1,
        .w = 22,
        .h = 62,
        .x = 588,
        .y = 201,
        .pri = 81920,
        .alpha = 128,
    },
    {
        .tex0 = 2307098813043614609,
        .u = 73,
        .v = 1,
        .w = 22,
        .h = 62,
        .x = 588,
        .y = 263,
        .pri = 81920,
        .alpha = 128,
    },
    {
        .tex0 = 2307098813043614609,
        .u = 97,
        .v = 1,
        .w = 22,
        .h = 62,
        .x = 588,
        .y = 325,
        .pri = 81920,
        .alpha = 128,
    },
    {
        .tex0 = 2307098813043614609,
        .u = 25,
        .v = 1,
        .w = 22,
        .h = 35,
        .x = 588,
        .y = 387,
        .pri = 81920,
        .alpha = 128,
    },
    {
        .tex0 = 2307093865107071873,
        .u = 1,
        .v = 1,
        .w = 28,
        .h = 20,
        .x = 586,
        .y = 165,
        .pri = 40960,
        .alpha = 128,
    },
    {
        .tex0 = 2307093865107071873,
        .u = 1,
        .v = 20,
        .w = 28,
        .h = 8,
        .x = 586,
        .y = 291,
        .pri = 40960,
        .alpha = 128,
    },
    {
        .tex0 = 2307093865107071873,
        .u = 1,
        .v = 29,
        .w = 28,
        .h = 20,
        .x = 586,
        .y = 405,
        .pri = 40960,
        .alpha = 128,
    },
    {
        .tex0 = 2307091668097082130,
        .u = 1,
        .v = 1,
        .w = 46,
        .h = 160,
        .x = 577,
        .y = 337,
        .pri = 53248,
        .alpha = 128,
    },
    {
        .tex0 = 2307075585794417408,
        .u = 1,
        .v = 1,
        .w = 8,
        .h = 62,
        .x = 596,
        .y = 174,
        .pri = 61440,
        .alpha = 128,
    },
    {
        .tex0 = 2307075585794417408,
        .u = 11,
        .v = 1,
        .w = 8,
        .h = 62,
        .x = 596,
        .y = 236,
        .pri = 61440,
        .alpha = 128,
    },
    {
        .tex0 = 2307075585794417408,
        .u = 21,
        .v = 1,
        .w = 8,
        .h = 62,
        .x = 596,
        .y = 298,
        .pri = 61440,
        .alpha = 128,
    },
    {
        .tex0 = 2307075585794417408,
        .u = 31,
        .v = 1,
        .w = 8,
        .h = 57,
        .x = 596,
        .y = 360,
        .pri = 61440,
        .alpha = 128,
    },
    {
        .tex0 = 2307076822744998665,
        .u = 1,
        .v = 1,
        .w = 8,
        .h = 62,
        .x = 596,
        .y = 174,
        .pri = 69632,
        .alpha = 128,
    },
    {
        .tex0 = 2307076822744998665,
        .u = 11,
        .v = 1,
        .w = 8,
        .h = 62,
        .x = 596,
        .y = 236,
        .pri = 69632,
        .alpha = 128,
    },
    {
        .tex0 = 2307076822744998665,
        .u = 21,
        .v = 1,
        .w = 8,
        .h = 62,
        .x = 596,
        .y = 298,
        .pri = 69632,
        .alpha = 128,
    },
    {
        .tex0 = 2307076822744998665,
        .u = 31,
        .v = 1,
        .w = 8,
        .h = 57,
        .x = 596,
        .y = 360,
        .pri = 69632,
        .alpha = 128,
    },
    {
        .tex0 = 2306960685755572160,
        .u = 0,
        .v = 0,
        .w = 12,
        .h = 16,
        .x = 1,
        .y = 0,
        .pri = 69632,
        .alpha = 128,
    },
    {
        .tex0 = 2306960685755572160,
        .u = 12,
        .v = 0,
        .w = 13,
        .h = 16,
        .x = 0,
        .y = 0,
        .pri = 69632,
        .alpha = 128,
    },
    {
        .tex0 = 2306960685755572160,
        .u = 25,
        .v = 0,
        .w = 13,
        .h = 16,
        .x = 0,
        .y = 0,
        .pri = 69632,
        .alpha = 128,
    },
    {
        .tex0 = 2306960685755572160,
        .u = 38,
        .v = 0,
        .w = 13,
        .h = 16,
        .x = 0,
        .y = 0,
        .pri = 69632,
        .alpha = 128,
    },
    {
        .tex0 = 2306960685755572160,
        .u = 51,
        .v = 0,
        .w = 13,
        .h = 16,
        .x = 0,
        .y = 0,
        .pri = 69632,
        .alpha = 128,
    },
    {
        .tex0 = 2306960685755572160,
        .u = 0,
        .v = 16,
        .w = 12,
        .h = 16,
        .x = 1,
        .y = 0,
        .pri = 69632,
        .alpha = 128,
    },
    {
        .tex0 = 2306960685755572160,
        .u = 12,
        .v = 16,
        .w = 13,
        .h = 16,
        .x = 0,
        .y = 0,
        .pri = 69632,
        .alpha = 128,
    },
    {
        .tex0 = 2306960685755572160,
        .u = 25,
        .v = 16,
        .w = 13,
        .h = 16,
        .x = 0,
        .y = 0,
        .pri = 69632,
        .alpha = 128,
    },
    {
        .tex0 = 2306960685755572160,
        .u = 38,
        .v = 16,
        .w = 13,
        .h = 16,
        .x = 0,
        .y = 0,
        .pri = 69632,
        .alpha = 128,
    },
    {
        .tex0 = 2306960685755572160,
        .u = 51,
        .v = 16,
        .w = 13,
        .h = 16,
        .x = 0,
        .y = 0,
        .pri = 69632,
        .alpha = 128,
    },
    {
        .tex0 = 2307001782284394671,
        .u = 1,
        .v = 1,
        .w = 18,
        .h = 20,
        .x = 0,
        .y = 0,
        .pri = 53248,
        .alpha = 128,
    },
    {
        .tex0 = 2307001782284394671,
        .u = 19,
        .v = 1,
        .w = 18,
        .h = 20,
        .x = 0,
        .y = 0,
        .pri = 53248,
        .alpha = 128,
    },
    {
        .tex0 = 2307001782284394671,
        .u = 37,
        .v = 1,
        .w = 18,
        .h = 20,
        .x = 0,
        .y = 0,
        .pri = 53248,
        .alpha = 128,
    },
    {
        .tex0 = 2307001782284394671,
        .u = 55,
        .v = 1,
        .w = 18,
        .h = 20,
        .x = 0,
        .y = 0,
        .pri = 53248,
        .alpha = 128,
    },
    {
        .tex0 = 2307001782284394671,
        .u = 73,
        .v = 1,
        .w = 18,
        .h = 20,
        .x = 0,
        .y = 0,
        .pri = 53248,
        .alpha = 128,
    },
    {
        .tex0 = 2307001782284394671,
        .u = 1,
        .v = 21,
        .w = 18,
        .h = 20,
        .x = 0,
        .y = 0,
        .pri = 53248,
        .alpha = 128,
    },
    {
        .tex0 = 2307001782284394671,
        .u = 19,
        .v = 21,
        .w = 18,
        .h = 20,
        .x = 0,
        .y = 0,
        .pri = 53248,
        .alpha = 128,
    },
    {
        .tex0 = 2307001782284394671,
        .u = 37,
        .v = 21,
        .w = 18,
        .h = 20,
        .x = 0,
        .y = 0,
        .pri = 53248,
        .alpha = 128,
    },
    {
        .tex0 = 2307001782284394671,
        .u = 55,
        .v = 21,
        .w = 18,
        .h = 20,
        .x = 0,
        .y = 0,
        .pri = 53248,
        .alpha = 128,
    },
    {
        .tex0 = 2307001782284394671,
        .u = 73,
        .v = 21,
        .w = 18,
        .h = 20,
        .x = 0,
        .y = 0,
        .pri = 53248,
        .alpha = 128,
    },
    {
        .tex0 = 2307001782284394671,
        .u = 1,
        .v = 42,
        .w = 96,
        .h = 16,
        .x = 107,
        .y = 111,
        .pri = 53248,
        .alpha = 128,
    },
    {
        .tex0 = 2307001782284394671,
        .u = 99,
        .v = 1,
        .w = 126,
        .h = 16,
        .x = 0,
        .y = 0,
        .pri = 53248,
        .alpha = 128,
    },
    {
        .tex0 = 2307001782284394671,
        .u = 99,
        .v = 19,
        .w = 108,
        .h = 16,
        .x = 0,
        .y = 0,
        .pri = 53248,
        .alpha = 128,
    },
    {
        .tex0 = 2307001782284394671,
        .u = 99,
        .v = 37,
        .w = 100,
        .h = 16,
        .x = 0,
        .y = 0,
        .pri = 53248,
        .alpha = 128,
    },
    {
        .tex0 = 2307120390893249606,
        .u = 1,
        .v = 1,
        .w = 62,
        .h = 38,
        .x = 288,
        .y = 28,
        .pri = 81920,
        .alpha = 128,
    },
    {
        .tex0 = 2307142793442665705,
        .u = 1,
        .v = 1,
        .w = 62,
        .h = 38,
        .x = 288,
        .y = 28,
        .pri = 57344,
        .alpha = 128,
    },
    {
        .tex0 = 2307121627843830863,
        .u = 1,
        .v = 1,
        .w = 62,
        .h = 38,
        .x = 288,
        .y = 28,
        .pri = 73728,
        .alpha = 128,
    },
    {
        .tex0 = 2307130562515608664,
        .u = 1,
        .v = 1,
        .w = 104,
        .h = 84,
        .x = 267,
        .y = 5,
        .pri = 65536,
        .alpha = 128,
    },
    {
        .tex0 = 2307132210643248284,
        .u = 1,
        .v = 1,
        .w = 62,
        .h = 38,
        .x = 288,
        .y = 28,
        .pri = 73728,
        .alpha = 128,
    },
    {
        .tex0 = 2307141145315026085,
        .u = 1,
        .v = 1,
        .w = 104,
        .h = 84,
        .x = 267,
        .y = 5,
        .pri = 65536,
        .alpha = 128,
    },
    {
        .tex0 = 2307100462244996021,
        .u = 1,
        .v = 1,
        .w = 10,
        .h = 62,
        .x = 594,
        .y = 155,
        .pri = 65536,
        .alpha = 128,
    },
    {
        .tex0 = 2307100462244996021,
        .u = 13,
        .v = 1,
        .w = 3,
        .h = 61,
        .x = 596,
        .y = 155,
        .pri = 65536,
        .alpha = 128,
    },
    {
        .tex0 = 2307100462244996021,
        .u = 18,
        .v = 2,
        .w = 8,
        .h = 32,
        .x = 596,
        .y = 158,
        .pri = 57344,
        .alpha = 128,
    },
    {
        .tex0 = 2307020884017193334,
        .u = 1,
        .v = 1,
        .w = 16,
        .h = 7,
        .x = 109,
        .y = 81,
        .pri = 86016,
        .alpha = 128,
    },
    {
        .tex0 = 2307021571211960699,
        .u = 1,
        .v = 1,
        .w = 12,
        .h = 24,
        .x = 314,
        .y = 168,
        .pri = 81920,
        .alpha = 128,
    },
    {
        .tex0 = 2307021571211960699,
        .u = 29,
        .v = 1,
        .w = 24,
        .h = 12,
        .x = 353,
        .y = 219,
        .pri = 81920,
        .alpha = 128,
    },
    {
        .tex0 = 2307021571211960699,
        .u = 15,
        .v = 1,
        .w = 12,
        .h = 24,
        .x = 313,
        .y = 256,
        .pri = 81920,
        .alpha = 128,
    },
    {
        .tex0 = 2307021571211960699,
        .u = 29,
        .v = 15,
        .w = 24,
        .h = 12,
        .x = 264,
        .y = 219,
        .pri = 81920,
        .alpha = 128,
    },
    {
        .tex0 = 2307022809236283776,
        .u = 1,
        .v = 1,
        .w = 16,
        .h = 32,
        .x = 312,
        .y = 164,
        .pri = 81920,
        .alpha = 128,
    },
    {
        .tex0 = 2307022809236283776,
        .u = 31,
        .v = 1,
        .w = 32,
        .h = 16,
        .x = 349,
        .y = 216,
        .pri = 81920,
        .alpha = 128,
    },
    {
        .tex0 = 2307022809236283776,
        .u = 47,
        .v = 31,
        .w = 16,
        .h = 32,
        .x = 311,
        .y = 252,
        .pri = 81920,
        .alpha = 128,
    },
    {
        .tex0 = 2307022809236283776,
        .u = 1,
        .v = 47,
        .w = 32,
        .h = 16,
        .x = 260,
        .y = 216,
        .pri = 81920,
        .alpha = 128,
    },
    {
        .tex0 = 2307109396916773822,
        .u = 1,
        .v = 73,
        .w = 80,
        .h = 52,
        .x = 508,
        .y = 373,
        .pri = 81920,
        .alpha = 128,
    },
    {
        .tex0 = 2307109396916773822,
        .u = 1,
        .v = 1,
        .w = 126,
        .h = 16,
        .x = 0,
        .y = 0,
        .pri = 53248,
        .alpha = 128,
    },
    {
        .tex0 = 2307109396916773822,
        .u = 83,
        .v = 73,
        .w = 19,
        .h = 16,
        .x = 126,
        .y = 0,
        .pri = 53248,
        .alpha = 128,
    },
    {
        .tex0 = 2307109396916773822,
        .u = 1,
        .v = 19,
        .w = 116,
        .h = 16,
        .x = 0,
        .y = 0,
        .pri = 53248,
        .alpha = 128,
    },
    {
        .tex0 = 2307109396916773822,
        .u = 1,
        .v = 37,
        .w = 114,
        .h = 16,
        .x = 0,
        .y = 0,
        .pri = 53248,
        .alpha = 128,
    },
    {
        .tex0 = 2307109396916773822,
        .u = 1,
        .v = 55,
        .w = 120,
        .h = 16,
        .x = 0,
        .y = 0,
        .pri = 53248,
        .alpha = 128,
    },
    {
        .tex0 = 2307118742765609986,
        .u = 1,
        .v = 55,
        .w = 126,
        .h = 16,
        .x = 0,
        .y = 0,
        .pri = 53248,
        .alpha = 128,
    },
    {
        .tex0 = 2307118742765609986,
        .u = 1,
        .v = 73,
        .w = 15,
        .h = 16,
        .x = 126,
        .y = 0,
        .pri = 53248,
        .alpha = 128,
    },
    {
        .tex0 = 2307118742765609986,
        .u = 1,
        .v = 1,
        .w = 126,
        .h = 16,
        .x = 0,
        .y = 0,
        .pri = 53248,
        .alpha = 128,
    },
    {
        .tex0 = 2307118742765609986,
        .u = 1,
        .v = 37,
        .w = 23,
        .h = 16,
        .x = 126,
        .y = 0,
        .pri = 53248,
        .alpha = 128,
    },
    {
        .tex0 = 2307118742765609986,
        .u = 1,
        .v = 19,
        .w = 120,
        .h = 16,
        .x = 0,
        .y = 0,
        .pri = 53248,
        .alpha = 128,
    },
    {
        .tex0 = 2307123279391957859,
        .u = 159,
        .v = 128,
        .w = 26,
        .h = 33,
        .x = 0,
        .y = 0,
        .pri = 40960,
        .alpha = 128,
    },
    {
        .tex0 = 2307123279391957859,
        .u = 159,
        .v = 128,
        .w = 26,
        .h = 33,
        .x = 0,
        .y = -33,
        .pri = 40960,
        .alpha = 128,
    },
    {
        .tex0 = 2307123279391957859,
        .u = 159,
        .v = 95,
        .w = 26,
        .h = 67,
        .x = 0,
        .y = 0,
        .pri = 40960,
        .alpha = 128,
    },
    {
        .tex0 = 2307123279391957859,
        .u = 159,
        .v = 95,
        .w = 26,
        .h = 67,
        .x = 0,
        .y = -67,
        .pri = 40960,
        .alpha = 128,
    },
    {
        .tex0 = 2307123279391957859,
        .u = 159,
        .v = 75,
        .w = 26,
        .h = 86,
        .x = 0,
        .y = 0,
        .pri = 40960,
        .alpha = 128,
    },
    {
        .tex0 = 2307123279391957859,
        .u = 159,
        .v = 75,
        .w = 26,
        .h = 86,
        .x = 0,
        .y = -86,
        .pri = 40960,
        .alpha = 128,
    },
    {
        .tex0 = 2307123279391957859,
        .u = 159,
        .v = 56,
        .w = 26,
        .h = 105,
        .x = 0,
        .y = 0,
        .pri = 40960,
        .alpha = 128,
    },
    {
        .tex0 = 2307123279391957859,
        .u = 159,
        .v = 56,
        .w = 26,
        .h = 105,
        .x = 0,
        .y = -105,
        .pri = 40960,
        .alpha = 128,
    },
    {
        .tex0 = 2307123279391957859,
        .u = 159,
        .v = 37,
        .w = 26,
        .h = 124,
        .x = 0,
        .y = 0,
        .pri = 409600,
        .alpha = 128,
    },
    {
        .tex0 = 2307123279391957859,
        .u = 159,
        .v = 37,
        .w = 26,
        .h = 124,
        .x = 0,
        .y = -124,
        .pri = 409600,
        .alpha = 128,
    },
    {
        .tex0 = 2307123279391957859,
        .u = 159,
        .v = 18,
        .w = 26,
        .h = 143,
        .x = 0,
        .y = 0,
        .pri = 409600,
        .alpha = 128,
    },
    {
        .tex0 = 2307123279391957859,
        .u = 159,
        .v = 18,
        .w = 26,
        .h = 143,
        .x = 0,
        .y = -143,
        .pri = 409600,
        .alpha = 128,
    },
    {
        .tex0 = 2307123279391957859,
        .u = 159,
        .v = 1,
        .w = 26,
        .h = 161,
        .x = 0,
        .y = 0,
        .pri = 409600,
        .alpha = 128,
    },
    {
        .tex0 = 2307123279391957859,
        .u = 159,
        .v = 1,
        .w = 26,
        .h = 161,
        .x = 0,
        .y = -161,
        .pri = 409600,
        .alpha = 128,
    },
    {
        .tex0 = 2307123279391957859,
        .u = 7,
        .v = 219,
        .w = 168,
        .h = 8,
        .x = 0,
        .y = 0,
        .pri = 430080,
        .alpha = 128,
    },
    {
        .tex0 = 2307123279391957859,
        .u = 112,
        .v = 219,
        .w = 63,
        .h = 8,
        .x = 0,
        .y = 0,
        .pri = 61440,
        .alpha = 128,
    },
    {
        .tex0 = 2307123279391957859,
        .u = 112,
        .v = 219,
        .w = 63,
        .h = 8,
        .x = -63,
        .y = 0,
        .pri = 61440,
        .alpha = 128,
    },
    {
        .tex0 = 2307123279391957859,
        .u = 7,
        .v = 185,
        .w = 122,
        .h = 32,
        .x = 48,
        .y = 137,
        .pri = 512000,
        .alpha = 128,
    },
    {
        .tex0 = 2307123279391957859,
        .u = 7,
        .v = 185,
        .w = 122,
        .h = 32,
        .x = 48,
        .y = 175,
        .pri = 512000,
        .alpha = 128,
    },
    {
        .tex0 = 2307123279391957859,
        .u = 7,
        .v = 185,
        .w = 122,
        .h = 32,
        .x = 48,
        .y = 213,
        .pri = 512000,
        .alpha = 128,
    },
    {
        .tex0 = 2307123279391957859,
        .u = 7,
        .v = 185,
        .w = 122,
        .h = 32,
        .x = 48,
        .y = 251,
        .pri = 512000,
        .alpha = 128,
    },
    {
        .tex0 = 2307123279391957859,
        .u = 7,
        .v = 185,
        .w = 122,
        .h = 32,
        .x = 48,
        .y = 289,
        .pri = 512000,
        .alpha = 128,
    },
    {
        .tex0 = 2307123279391957859,
        .u = 7,
        .v = 185,
        .w = 122,
        .h = 32,
        .x = 48,
        .y = 327,
        .pri = 512000,
        .alpha = 128,
    },
    {
        .tex0 = 2307123279391957859,
        .u = 7,
        .v = 185,
        .w = 122,
        .h = 32,
        .x = 48,
        .y = 365,
        .pri = 512000,
        .alpha = 128,
    },
    {
        .tex0 = 2307132624168100967,
        .u = 1,
        .v = 1,
        .w = 124,
        .h = 30,
        .x = 48,
        .y = 138,
        .pri = 491520,
        .alpha = 128,
    },
    {
        .tex0 = 2307132624168100967,
        .u = 1,
        .v = 33,
        .w = 124,
        .h = 30,
        .x = 48,
        .y = 176,
        .pri = 491520,
        .alpha = 128,
    },
    {
        .tex0 = 2307132624168100967,
        .u = 1,
        .v = 65,
        .w = 124,
        .h = 30,
        .x = 48,
        .y = 214,
        .pri = 491520,
        .alpha = 128,
    },
    {
        .tex0 = 2307132624168100967,
        .u = 1,
        .v = 97,
        .w = 124,
        .h = 30,
        .x = 48,
        .y = 252,
        .pri = 491520,
        .alpha = 128,
    },
    {
        .tex0 = 2307132624168100967,
        .u = 127,
        .v = 1,
        .w = 124,
        .h = 30,
        .x = 48,
        .y = 290,
        .pri = 491520,
        .alpha = 128,
    },
    {
        .tex0 = 2307132624168100967,
        .u = 127,
        .v = 33,
        .w = 124,
        .h = 30,
        .x = 48,
        .y = 328,
        .pri = 491520,
        .alpha = 128,
    },
    {
        .tex0 = 2307137158511633576,
        .u = 1,
        .v = 1,
        .w = 124,
        .h = 30,
        .x = 45,
        .y = 367,
        .pri = 491520,
        .alpha = 128,
    },
    {
        .tex0 = 2307132624168100967,
        .u = 127,
        .v = 97,
        .w = 124,
        .h = 30,
        .x = 48,
        .y = 35,
        .pri = 491520,
        .alpha = 128,
    },
    {
        .tex0 = 2307132624168100967,
        .u = 127,
        .v = 65,
        .w = 124,
        .h = 30,
        .x = 48,
        .y = 366,
        .pri = 491520,
        .alpha = 128,
    },
    {
        .tex0 = 2307123279391957859,
        .u = 7,
        .v = 153,
        .w = 122,
        .h = 30,
        .x = 48,
        .y = 138,
        .pri = 471040,
        .alpha = 128,
    },
    {
        .tex0 = 2307146505502401740,
        .u = 1,
        .v = 1,
        .w = 62,
        .h = 30,
        .x = 0,
        .y = 0,
        .pri = 4096,
        .alpha = 128,
    },
    {
        .tex0 = 2307146505502401740,
        .u = 1,
        .v = 33,
        .w = 62,
        .h = 30,
        .x = 0,
        .y = 0,
        .pri = 4096,
        .alpha = 128,
    },
    {
        .tex0 = 2307146505502401740,
        .u = 1,
        .v = 65,
        .w = 62,
        .h = 30,
        .x = 0,
        .y = 0,
        .pri = 4096,
        .alpha = 128,
    },
    {
        .tex0 = 2307146505502401740,
        .u = 1,
        .v = 97,
        .w = 62,
        .h = 30,
        .x = 0,
        .y = 0,
        .pri = 4096,
        .alpha = 128,
    },
    {
        .tex0 = 2307146505502401740,
        .u = 65,
        .v = 1,
        .w = 94,
        .h = 30,
        .x = 0,
        .y = 0,
        .pri = 4096,
        .alpha = 128,
    },
    {
        .tex0 = 2307146505502401740,
        .u = 65,
        .v = 33,
        .w = 94,
        .h = 30,
        .x = 0,
        .y = 0,
        .pri = 4096,
        .alpha = 128,
    },
    {
        .tex0 = 2307146505502401740,
        .u = 65,
        .v = 65,
        .w = 94,
        .h = 30,
        .x = 0,
        .y = 0,
        .pri = 4096,
        .alpha = 128,
    },
    {
        .tex0 = 2307146505502401740,
        .u = 65,
        .v = 97,
        .w = 94,
        .h = 30,
        .x = 0,
        .y = 0,
        .pri = 4096,
        .alpha = 128,
    },
    {
        .tex0 = 2307216461928670857,
        .u = 1,
        .v = 1,
        .w = 254,
        .h = 58,
        .x = 192,
        .y = 24,
        .pri = 512000,
        .alpha = 128,
    },
    {
        .tex0 = 2307216461928670857,
        .u = 1,
        .v = 61,
        .w = 170,
        .h = 58,
        .x = 446,
        .y = 24,
        .pri = 512000,
        .alpha = 128,
    },
    {
        .tex0 = 2307216461928670857,
        .u = 173,
        .v = 61,
        .w = 54,
        .h = 50,
        .x = 230,
        .y = 28,
        .pri = 491520,
        .alpha = 128,
    },
    {
        .tex0 = 2307225807710365453,
        .u = 1,
        .v = 1,
        .w = 40,
        .h = 40,
        .x = 320,
        .y = 46,
        .pri = 471040,
        .alpha = 128,
    },
    {
        .tex0 = 2307225807710365453,
        .u = 43,
        .v = 1,
        .w = 40,
        .h = 40,
        .x = 320,
        .y = 46,
        .pri = 471040,
        .alpha = 128,
    },
    {
        .tex0 = 2307225807710365453,
        .u = 85,
        .v = 1,
        .w = 40,
        .h = 40,
        .x = 320,
        .y = 46,
        .pri = 471040,
        .alpha = 128,
    },
    {
        .tex0 = 2307225807710365453,
        .u = 1,
        .v = 43,
        .w = 40,
        .h = 40,
        .x = 320,
        .y = 46,
        .pri = 471040,
        .alpha = 128,
    },
    {
        .tex0 = 2307225807710365453,
        .u = 43,
        .v = 43,
        .w = 40,
        .h = 40,
        .x = 320,
        .y = 46,
        .pri = 471040,
        .alpha = 128,
    },
    {
        .tex0 = 2307225807710365453,
        .u = 85,
        .v = 43,
        .w = 40,
        .h = 40,
        .x = 424,
        .y = 47,
        .pri = 471040,
        .alpha = 128,
    },
    {
        .tex0 = 2307231855025366873,
        .u = 1,
        .v = 1,
        .w = 29,
        .h = 26,
        .x = 523,
        .y = 53,
        .pri = 450560,
        .alpha = 128,
    },
    {
        .tex0 = 2307231855025366873,
        .u = 30,
        .v = 1,
        .w = 29,
        .h = 26,
        .x = 523,
        .y = 53,
        .pri = 450560,
        .alpha = 128,
    },
    {
        .tex0 = 2307231855025366873,
        .u = 59,
        .v = 1,
        .w = 29,
        .h = 26,
        .x = 523,
        .y = 53,
        .pri = 450560,
        .alpha = 128,
    },
    {
        .tex0 = 2307231855025366873,
        .u = 88,
        .v = 1,
        .w = 29,
        .h = 26,
        .x = 523,
        .y = 53,
        .pri = 450560,
        .alpha = 128,
    },
    {
        .tex0 = 2307231855025366873,
        .u = 1,
        .v = 29,
        .w = 29,
        .h = 26,
        .x = 523,
        .y = 53,
        .pri = 450560,
        .alpha = 128,
    },
    {
        .tex0 = 2307231855025366873,
        .u = 30,
        .v = 29,
        .w = 29,
        .h = 26,
        .x = 573,
        .y = 53,
        .pri = 450560,
        .alpha = 128,
    },
    {
        .tex0 = 2307231855025366873,
        .u = 59,
        .v = 29,
        .w = 29,
        .h = 26,
        .x = 573,
        .y = 53,
        .pri = 450560,
        .alpha = 128,
    },
    {
        .tex0 = 2307231855025366873,
        .u = 88,
        .v = 29,
        .w = 29,
        .h = 26,
        .x = 573,
        .y = 53,
        .pri = 450560,
        .alpha = 128,
    },
    {
        .tex0 = 2307231855025366873,
        .u = 1,
        .v = 57,
        .w = 29,
        .h = 26,
        .x = 573,
        .y = 53,
        .pri = 450560,
        .alpha = 128,
    },
    {
        .tex0 = 2307231855025366873,
        .u = 30,
        .v = 57,
        .w = 29,
        .h = 26,
        .x = 573,
        .y = 53,
        .pri = 450560,
        .alpha = 128,
    },
    {
        .tex0 = 2307231855025366873,
        .u = 59,
        .v = 57,
        .w = 29,
        .h = 26,
        .x = 523,
        .y = 53,
        .pri = 450560,
        .alpha = 128,
    },
    {
        .tex0 = 2307198731096729216,
        .u = 1,
        .v = 1,
        .w = 62,
        .h = 6,
        .x = 290,
        .y = 42,
        .pri = 471040,
        .alpha = 128,
    },
    {
        .tex0 = 2307198731096729216,
        .u = 1,
        .v = 9,
        .w = 62,
        .h = 6,
        .x = 352,
        .y = 42,
        .pri = 471040,
        .alpha = 128,
    },
    {
        .tex0 = 2307198731096729216,
        .u = 1,
        .v = 17,
        .w = 24,
        .h = 6,
        .x = 414,
        .y = 42,
        .pri = 471040,
        .alpha = 128,
    },
    {
        .tex0 = 2307198731096729216,
        .u = 1,
        .v = 25,
        .w = 62,
        .h = 6,
        .x = 290,
        .y = 42,
        .pri = 450560,
        .alpha = 128,
    },
    {
        .tex0 = 2307198731096729216,
        .u = 1,
        .v = 33,
        .w = 62,
        .h = 6,
        .x = 352,
        .y = 42,
        .pri = 450560,
        .alpha = 128,
    },
    {
        .tex0 = 2307198731096729216,
        .u = 27,
        .v = 17,
        .w = 24,
        .h = 6,
        .x = 414,
        .y = 42,
        .pri = 450560,
        .alpha = 128,
    },
    {
        .tex0 = 2307198731096729216,
        .u = 1,
        .v = 41,
        .w = 62,
        .h = 6,
        .x = 290,
        .y = 42,
        .pri = 491520,
        .alpha = 128,
    },
    {
        .tex0 = 2307198731096729216,
        .u = 1,
        .v = 49,
        .w = 62,
        .h = 6,
        .x = 352,
        .y = 42,
        .pri = 491520,
        .alpha = 128,
    },
    {
        .tex0 = 2307198731096729216,
        .u = 1,
        .v = 57,
        .w = 24,
        .h = 6,
        .x = 414,
        .y = 42,
        .pri = 491520,
        .alpha = 128,
    },
    {
        .tex0 = 2307226904940291921,
        .u = 1,
        .v = 1,
        .w = 25,
        .h = 25,
        .x = 0,
        .y = 42,
        .pri = 450560,
        .alpha = 128,
    },
    {
        .tex0 = 2307234190345676666,
        .u = 1,
        .v = 1,
        .w = 54,
        .h = 50,
        .x = 230,
        .y = 28,
        .pri = 491520,
        .alpha = 128,
    },
    {
        .tex0 = 2307235013839595406,
        .u = 1,
        .v = 1,
        .w = 20,
        .h = 20,
        .x = 434,
        .y = 57,
        .pri = 471040,
        .alpha = 128,
    },
    {
        .tex0 = 2307480206073441920,
        .u = 1,
        .v = 1,
        .w = 62,
        .h = 62,
        .x = 433,
        .y = 177,
        .pri = 69632,
        .alpha = 128,
    },
    {
        .tex0 = 2307392245143219200,
        .u = 1,
        .v = 1,
        .w = 48,
        .h = 48,
        .x = 497,
        .y = 121,
        .pri = 102400,
        .alpha = 128,
    },
    {
        .tex0 = 2307392931196087305,
        .u = 1,
        .v = 1,
        .w = 18,
        .h = 17,
        .x = 513,
        .y = 136,
        .pri = 102400,
        .alpha = 128,
    },
    {
        .tex0 = 2307394030707715089,
        .u = 1,
        .v = 1,
        .w = 18,
        .h = 17,
        .x = 513,
        .y = 136,
        .pri = 102400,
        .alpha = 128,
    },
    {
        .tex0 = 2307412176073206809,
        .u = 1,
        .v = 1,
        .w = 122,
        .h = 42,
        .x = 47,
        .y = 120,
        .pri = 102400,
        .alpha = 128,
    },
    {
        .tex0 = 2307412176073206809,
        .u = 1,
        .v = 43,
        .w = 122,
        .h = 42,
        .x = 47,
        .y = 170,
        .pri = 102400,
        .alpha = 128,
    },
    {
        .tex0 = 2307412176073206809,
        .u = 1,
        .v = 85,
        .w = 122,
        .h = 42,
        .x = 47,
        .y = 220,
        .pri = 102400,
        .alpha = 128,
    },
    {
        .tex0 = 2307412176073206809,
        .u = 1,
        .v = 127,
        .w = 122,
        .h = 42,
        .x = 47,
        .y = 270,
        .pri = 102400,
        .alpha = 128,
    },
    {
        .tex0 = 2307421108463250586,
        .u = 1,
        .v = 1,
        .w = 58,
        .h = 46,
        .x = 277,
        .y = 129,
        .pri = 102400,
        .alpha = 128,
    },
    {
        .tex0 = 2307421108463250586,
        .u = 59,
        .v = 1,
        .w = 58,
        .h = 46,
        .x = 277,
        .y = 179,
        .pri = 102400,
        .alpha = 128,
    },
    {
        .tex0 = 2307421108463250586,
        .u = 1,
        .v = 47,
        .w = 58,
        .h = 46,
        .x = 478,
        .y = 129,
        .pri = 102400,
        .alpha = 128,
    },
    {
        .tex0 = 2307421108463250586,
        .u = 59,
        .v = 47,
        .w = 58,
        .h = 46,
        .x = 478,
        .y = 179,
        .pri = 102400,
        .alpha = 128,
    },
    {
        .tex0 = 2307430454380276958,
        .u = 1,
        .v = 1,
        .w = 44,
        .h = 44,
        .x = 204,
        .y = 271,
        .pri = 102400,
        .alpha = 128,
    },
    {
        .tex0 = 2307430454380276958,
        .u = 45,
        .v = 1,
        .w = 44,
        .h = 44,
        .x = 250,
        .y = 271,
        .pri = 102400,
        .alpha = 128,
    },
    {
        .tex0 = 2307430454380276958,
        .u = 89,
        .v = 1,
        .w = 44,
        .h = 44,
        .x = 296,
        .y = 271,
        .pri = 102400,
        .alpha = 128,
    },
    {
        .tex0 = 2307430454380276958,
        .u = 133,
        .v = 1,
        .w = 44,
        .h = 44,
        .x = 342,
        .y = 271,
        .pri = 102400,
        .alpha = 128,
    },
    {
        .tex0 = 2307430454380276958,
        .u = 177,
        .v = 1,
        .w = 44,
        .h = 44,
        .x = 388,
        .y = 271,
        .pri = 102400,
        .alpha = 128,
    },
    {
        .tex0 = 2307430454380276958,
        .u = 1,
        .v = 45,
        .w = 44,
        .h = 44,
        .x = 204,
        .y = 271,
        .pri = 102400,
        .alpha = 128,
    },
    {
        .tex0 = 2307430454380276958,
        .u = 45,
        .v = 45,
        .w = 44,
        .h = 44,
        .x = 250,
        .y = 271,
        .pri = 102400,
        .alpha = 128,
    },
    {
        .tex0 = 2307430454380276958,
        .u = 89,
        .v = 45,
        .w = 44,
        .h = 44,
        .x = 296,
        .y = 271,
        .pri = 102400,
        .alpha = 128,
    },
    {
        .tex0 = 2307430454380276958,
        .u = 133,
        .v = 45,
        .w = 44,
        .h = 44,
        .x = 342,
        .y = 271,
        .pri = 102400,
        .alpha = 128,
    },
    {
        .tex0 = 2307430454380276958,
        .u = 177,
        .v = 45,
        .w = 44,
        .h = 44,
        .x = 388,
        .y = 271,
        .pri = 102400,
        .alpha = 128,
    },
    {
        .tex0 = 2307430454380276958,
        .u = 1,
        .v = 89,
        .w = 20,
        .h = 20,
        .x = 380,
        .y = 134,
        .pri = 102400,
        .alpha = 128,
    },
    {
        .tex0 = 2307430454380276958,
        .u = 21,
        .v = 89,
        .w = 102,
        .h = 20,
        .x = 373,
        .y = 141,
        .pri = 102400,
        .alpha = 128,
    },
    {
        .tex0 = 2307441723233578336,
        .u = 1,
        .v = 1,
        .w = 40,
        .h = 30,
        .x = 474,
        .y = 131,
        .pri = 102400,
        .alpha = 128,
    },
    {
        .tex0 = 2307441723233578336,
        .u = 40,
        .v = 1,
        .w = 50,
        .h = 30,
        .x = 461,
        .y = 272,
        .pri = 102400,
        .alpha = 128,
    },
    {
        .tex0 = 2307477047258197361,
        .u = 1,
        .v = 1,
        .w = 254,
        .h = 254,
        .x = 193,
        .y = 97,
        .pri = 204800,
        .alpha = 128,
    },
    {
        .tex0 = 2307495189201104501,
        .u = 1,
        .v = 1,
        .w = 240,
        .h = 32,
        .x = 298,
        .y = 97,
        .pri = 102400,
        .alpha = 128,
    },
    {
        .tex0 = 2307495189201104501,
        .u = 1,
        .v = 33,
        .w = 240,
        .h = 32,
        .x = 288,
        .y = 97,
        .pri = 102400,
        .alpha = 128,
    },
    {
        .tex0 = 2307495189201104501,
        .u = 1,
        .v = 65,
        .w = 240,
        .h = 32,
        .x = 218,
        .y = 239,
        .pri = 102400,
        .alpha = 128,
    },
    {
        .tex0 = 2307495189201104501,
        .u = 1,
        .v = 97,
        .w = 240,
        .h = 32,
        .x = 211,
        .y = 239,
        .pri = 102400,
        .alpha = 128,
    },
    {
        .tex0 = 2307495189201104501,
        .u = 1,
        .v = 129,
        .w = 240,
        .h = 32,
        .x = 502,
        .y = 239,
        .pri = 102400,
        .alpha = 128,
    },
    {
        .tex0 = 2307439387912252703,
        .u = 1,
        .v = 1,
        .w = 144,
        .h = 30,
        .x = 220,
        .y = 130,
        .pri = 102400,
        .alpha = 128,
    },
    {
        .tex0 = 2307439387912252703,
        .u = 1,
        .v = 31,
        .w = 144,
        .h = 30,
        .x = 220,
        .y = 161,
        .pri = 102400,
        .alpha = 128,
    },
    {
        .tex0 = 2307439387912252703,
        .u = 1,
        .v = 61,
        .w = 144,
        .h = 30,
        .x = 220,
        .y = 192,
        .pri = 102400,
        .alpha = 128,
    },
    {
        .tex0 = 2307426332284365824,
        .u = 1,
        .v = 1,
        .w = 254,
        .h = 254,
        .x = 324,
        .y = 84,
        .pri = 61440,
        .alpha = 128,
    },
    {
        .tex0 = 2307435676992318724,
        .u = 1,
        .v = 1,
        .w = 38,
        .h = 126,
        .x = 578,
        .y = 84,
        .pri = 61440,
        .alpha = 128,
    },
    {
        .tex0 = 2307435676992318724,
        .u = 65,
        .v = 1,
        .w = 38,
        .h = 126,
        .x = 578,
        .y = 210,
        .pri = 61440,
        .alpha = 128,
    },
    {
        .tex0 = 2307075172336706307,
        .u = 0,
        .v = 0,
        .w = 14,
        .h = 16,
        .x = 28,
        .y = 96,
        .pri = 122880,
        .alpha = 128,
    },
    {
        .tex0 = 2307075172336706307,
        .u = 0,
        .v = 16,
        .w = 14,
        .h = 16,
        .x = 28,
        .y = 310,
        .pri = 122880,
        .alpha = 128,
    },
    {
        .tex0 = 2307075172336706307,
        .u = 0,
        .v = 0,
        .w = 14,
        .h = 10,
        .x = 28,
        .y = 206,
        .pri = 122880,
        .alpha = 128,
    },
    {
        .tex0 = 2307075172336706307,
        .u = 14,
        .v = 0,
        .w = 14,
        .h = 12,
        .x = 29,
        .y = 97,
        .pri = 102400,
        .alpha = 128,
    },
    {
        .tex0 = 2307075172336706307,
        .u = 14,
        .v = 14,
        .w = 14,
        .h = 12,
        .x = 29,
        .y = 315,
        .pri = 102400,
        .alpha = 128,
    },
    {
        .tex0 = 2307074760019845888,
        .u = 0,
        .v = 0,
        .w = 14,
        .h = 16,
        .x = 28,
        .y = 103,
        .pri = 102400,
        .alpha = 128,
    },
    {
        .tex0 = 2307074760019845888,
        .u = 0,
        .v = 16,
        .w = 14,
        .h = 16,
        .x = 28,
        .y = 119,
        .pri = 102400,
        .alpha = 128,
    },
    {
        .tex0 = 2307074760019845888,
        .u = 14,
        .v = 1,
        .w = 14,
        .h = 15,
        .x = 28,
        .y = 119,
        .pri = 102400,
        .alpha = 128,
    },
    {
        .tex0 = 2307471412261104968,
        .u = 1,
        .v = 1,
        .w = 254,
        .h = 254,
        .x = 345,
        .y = 87,
        .pri = 225280,
        .alpha = 128,
    },
    {
        .tex0 = 2307395543744162816,
        .u = 1,
        .v = 1,
        .w = 80,
        .h = 36,
        .x = 207,
        .y = 28,
        .pri = 204800,
        .alpha = 128,
    },
    {
        .tex0 = 2307395543744162816,
        .u = 1,
        .v = 1,
        .w = 78,
        .h = 36,
        .x = 282,
        .y = 28,
        .pri = 204800,
        .alpha = 128,
    },
    {
        .tex0 = 2307395543744162816,
        .u = 1,
        .v = 1,
        .w = 80,
        .h = 36,
        .x = 397,
        .y = 28,
        .pri = 204800,
        .alpha = 128,
    },
    {
        .tex0 = 2307395543744162816,
        .u = 1,
        .v = 1,
        .w = 78,
        .h = 36,
        .x = 472,
        .y = 28,
        .pri = 204800,
        .alpha = 128,
    },
    {
        .tex0 = 2307123279391957859,
        .u = 1,
        .v = 61,
        .w = 78,
        .h = 28,
        .x = 35,
        .y = 37,
        .pri = 40960,
        .alpha = 128,
    },
    {
        .tex0 = 2307123279391957859,
        .u = 1,
        .v = 91,
        .w = 78,
        .h = 28,
        .x = 35,
        .y = 75,
        .pri = 40960,
        .alpha = 128,
    },
    {
        .tex0 = 2307123279391957859,
        .u = 1,
        .v = 1,
        .w = 78,
        .h = 28,
        .x = 35,
        .y = 113,
        .pri = 40960,
        .alpha = 128,
    },
    {
        .tex0 = 2307123279391957859,
        .u = 1,
        .v = 31,
        .w = 78,
        .h = 28,
        .x = 35,
        .y = 151,
        .pri = 40960,
        .alpha = 128,
    },
    {
        .tex0 = 2307123279391957859,
        .u = 81,
        .v = 61,
        .w = 78,
        .h = 28,
        .x = 35,
        .y = 37,
        .pri = 40960,
        .alpha = 128,
    },
    {
        .tex0 = 2307123279391957859,
        .u = 81,
        .v = 91,
        .w = 78,
        .h = 28,
        .x = 35,
        .y = 75,
        .pri = 40960,
        .alpha = 128,
    },
    {
        .tex0 = 2307123279391957859,
        .u = 1,
        .v = 61,
        .w = 78,
        .h = 28,
        .x = 35,
        .y = 113,
        .pri = 40960,
        .alpha = 128,
    },
    {
        .tex0 = 2307123279391957859,
        .u = 81,
        .v = 1,
        .w = 78,
        .h = 28,
        .x = 35,
        .y = 113,
        .pri = 40960,
        .alpha = 128,
    },
    {
        .tex0 = 2307123279391957859,
        .u = 81,
        .v = 31,
        .w = 78,
        .h = 28,
        .x = 35,
        .y = 113,
        .pri = 40960,
        .alpha = 128,
    },
    {
        .tex0 = 2307123279391957859,
        .u = 1,
        .v = 121,
        .w = 78,
        .h = 28,
        .x = 35,
        .y = 113,
        .pri = 40960,
        .alpha = 128,
    },
    {
        .tex0 = 2307424132120226992,
        .u = 3,
        .v = 1,
        .w = 60,
        .h = 30,
        .x = 35,
        .y = 37,
        .pri = 61440,
        .alpha = 128,
    },
    {
        .tex0 = 2307424132120226992,
        .u = 3,
        .v = 33,
        .w = 60,
        .h = 30,
        .x = 35,
        .y = 75,
        .pri = 61440,
        .alpha = 128,
    },
    {
        .tex0 = 2307424132120226992,
        .u = 3,
        .v = 65,
        .w = 60,
        .h = 30,
        .x = 35,
        .y = 113,
        .pri = 61440,
        .alpha = 128,
    },
    {
        .tex0 = 2307429078848810228,
        .u = 0,
        .v = 0,
        .w = 52,
        .h = 26,
        .x = 314,
        .y = 287,
        .pri = 163840,
        .alpha = 128,
    },
    {
        .tex0 = 2307429078848810228,
        .u = 0,
        .v = 26,
        .w = 52,
        .h = 26,
        .x = 314,
        .y = 340,
        .pri = 163840,
        .alpha = 128,
    },
    {
        .tex0 = 2307429078848810228,
        .u = 64,
        .v = 0,
        .w = 52,
        .h = 26,
        .x = 314,
        .y = 368,
        .pri = 163840,
        .alpha = 128,
    },
    {
        .tex0 = 2307429078848810228,
        .u = 64,
        .v = 26,
        .w = 52,
        .h = 26,
        .x = 314,
        .y = 394,
        .pri = 163840,
        .alpha = 128,
    },
    {
        .tex0 = 2307424132120226992,
        .u = 69,
        .v = 3,
        .w = 18,
        .h = 26,
        .x = 0,
        .y = 0,
        .pri = 122880,
        .alpha = 128,
    },
    {
        .tex0 = 2307424132120226992,
        .u = 89,
        .v = 3,
        .w = 18,
        .h = 26,
        .x = 30,
        .y = 0,
        .pri = 122880,
        .alpha = 128,
    },
    {
        .tex0 = 2307424132120226992,
        .u = 109,
        .v = 3,
        .w = 18,
        .h = 26,
        .x = 60,
        .y = 0,
        .pri = 122880,
        .alpha = 128,
    },
    {
        .tex0 = 2307424132120226992,
        .u = 69,
        .v = 34,
        .w = 18,
        .h = 26,
        .x = 90,
        .y = 0,
        .pri = 122880,
        .alpha = 128,
    },
    {
        .tex0 = 2307424132120226992,
        .u = 89,
        .v = 34,
        .w = 18,
        .h = 26,
        .x = 121,
        .y = 0,
        .pri = 122880,
        .alpha = 128,
    },
    {
        .tex0 = 2307424132120226992,
        .u = 109,
        .v = 34,
        .w = 18,
        .h = 26,
        .x = 151,
        .y = 0,
        .pri = 122880,
        .alpha = 128,
    },
    {
        .tex0 = 2307424132120226992,
        .u = 69,
        .v = 67,
        .w = 18,
        .h = 26,
        .x = 180,
        .y = 0,
        .pri = 122880,
        .alpha = 128,
    },
    {
        .tex0 = 2307424132120226992,
        .u = 89,
        .v = 67,
        .w = 18,
        .h = 26,
        .x = 210,
        .y = 0,
        .pri = 122880,
        .alpha = 128,
    },
    {
        .tex0 = 2307424132120226992,
        .u = 109,
        .v = 67,
        .w = 18,
        .h = 26,
        .x = 240,
        .y = 0,
        .pri = 122880,
        .alpha = 128,
    },
    {
        .tex0 = 2307424132120226992,
        .u = 69,
        .v = 99,
        .w = 18,
        .h = 26,
        .x = 270,
        .y = 0,
        .pri = 122880,
        .alpha = 128,
    },
    {
        .tex0 = 2307424132120226992,
        .u = 89,
        .v = 99,
        .w = 18,
        .h = 26,
        .x = 464,
        .y = 33,
        .pri = 184320,
        .alpha = 128,
    },
    {
        .tex0 = 2307424132120226992,
        .u = 109,
        .v = 99,
        .w = 18,
        .h = 26,
        .x = 464,
        .y = 33,
        .pri = 102400,
        .alpha = 128,
    },
    {
        .tex0 = 2307424132120226992,
        .u = 137,
        .v = 25,
        .w = 15,
        .h = 15,
        .x = 0,
        .y = 0,
        .pri = 102400,
        .alpha = 128,
    },
    {
        .tex0 = 2307433064511352097,
        .u = 1,
        .v = 1,
        .w = 35,
        .h = 29,
        .x = 69,
        .y = 265,
        .pri = 122880,
        .alpha = 128,
    },
    {
        .tex0 = 2307433064511352097,
        .u = 1,
        .v = 1,
        .w = 35,
        .h = 29,
        .x = 263,
        .y = 265,
        .pri = 122880,
        .alpha = 128,
    },
    {
        .tex0 = 2307433064511352097,
        .u = 1,
        .v = 1,
        .w = 35,
        .h = 29,
        .x = 69,
        .y = 391,
        .pri = 122880,
        .alpha = 128,
    },
    {
        .tex0 = 2307433064511352097,
        .u = 1,
        .v = 1,
        .w = 35,
        .h = 29,
        .x = 263,
        .y = 391,
        .pri = 122880,
        .alpha = 128,
    },
    {
        .tex0 = 2307424132120226992,
        .u = 34,
        .v = 0,
        .w = 32,
        .h = 64,
        .x = 267,
        .y = 15,
        .pri = 184320,
        .alpha = 128,
    },
    {
        .tex0 = 2307424132120226992,
        .u = 34,
        .v = 64,
        .w = 32,
        .h = 64,
        .x = 267,
        .y = 15,
        .pri = 184320,
        .alpha = 128,
    },
    {
        .tex0 = 2307424132120226992,
        .u = 0,
        .v = 0,
        .w = 32,
        .h = 128,
        .x = 289,
        .y = -18,
        .pri = 184320,
        .alpha = 128,
    },
    {
        .tex0 = 2307414099143765031,
        .u = 1,
        .v = 1,
        .w = 254,
        .h = 10,
        .x = 0,
        .y = 0,
        .pri = 184320,
        .alpha = 128,
    },
    {
        .tex0 = 2307414099143765031,
        .u = 1,
        .v = 13,
        .w = 254,
        .h = 10,
        .x = 254,
        .y = 0,
        .pri = 184320,
        .alpha = 128,
    },
    {
        .tex0 = 2307414099143765031,
        .u = 1,
        .v = 25,
        .w = 254,
        .h = 10,
        .x = 0,
        .y = 172,
        .pri = 184320,
        .alpha = 128,
    },
    {
        .tex0 = 2307414099143765031,
        .u = 1,
        .v = 37,
        .w = 254,
        .h = 10,
        .x = 254,
        .y = 172,
        .pri = 184320,
        .alpha = 128,
    },
    {
        .tex0 = 2307414099143765031,
        .u = 1,
        .v = 49,
        .w = 84,
        .h = 10,
        .x = 508,
        .y = 0,
        .pri = 184320,
        .alpha = 128,
    },
    {
        .tex0 = 2307414099143765031,
        .u = 87,
        .v = 49,
        .w = 84,
        .h = 10,
        .x = 508,
        .y = 172,
        .pri = 184320,
        .alpha = 128,
    },
    {
        .tex0 = 2307414099143765031,
        .u = 1,
        .v = 61,
        .w = 11,
        .h = 66,
        .x = 0,
        .y = 10,
        .pri = 184320,
        .alpha = 128,
    },
    {
        .tex0 = 2307414099143765031,
        .u = 14,
        .v = 61,
        .w = 11,
        .h = 66,
        .x = 0,
        .y = 76,
        .pri = 184320,
        .alpha = 128,
    },
    {
        .tex0 = 2307414099143765031,
        .u = 53,
        .v = 61,
        .w = 11,
        .h = 30,
        .x = 0,
        .y = 142,
        .pri = 184320,
        .alpha = 128,
    },
    {
        .tex0 = 2307414099143765031,
        .u = 27,
        .v = 61,
        .w = 11,
        .h = 66,
        .x = 581,
        .y = 10,
        .pri = 184320,
        .alpha = 128,
    },
    {
        .tex0 = 2307414099143765031,
        .u = 40,
        .v = 61,
        .w = 11,
        .h = 66,
        .x = 581,
        .y = 76,
        .pri = 184320,
        .alpha = 128,
    },
    {
        .tex0 = 2307414099143765031,
        .u = 53,
        .v = 93,
        .w = 11,
        .h = 30,
        .x = 581,
        .y = 142,
        .pri = 184320,
        .alpha = 128,
    },
    {
        .tex0 = 2307414099143765031,
        .u = 66,
        .v = 61,
        .w = 72,
        .h = 58,
        .x = 0,
        .y = 0,
        .pri = 204800,
        .alpha = 128,
    },
    {
        .tex0 = 2307435950730423614,
        .u = 1,
        .v = 1,
        .w = 54,
        .h = 38,
        .x = 0,
        .y = 0,
        .pri = 122880,
        .alpha = 128,
    },
    {
        .tex0 = 2307438287191584071,
        .u = 1,
        .v = 1,
        .w = 61,
        .h = 43,
        .x = 0,
        .y = 0,
        .pri = 122880,
        .alpha = 128,
    },
    {
        .tex0 = 2307415196374707371,
        .u = 0,
        .v = 0,
        .w = 48,
        .h = 32,
        .x = 0,
        .y = 0,
        .pri = 102400,
        .alpha = 128,
    },
    {
        .tex0 = 2307510445997632868,
        .u = 1,
        .v = 193,
        .w = 52,
        .h = 36,
        .x = 0,
        .y = 0,
        .pri = 143360,
        .alpha = 128,
    },
    {
        .tex0 = 2307414099143765031,
        .u = 189,
        .v = 49,
        .w = 66,
        .h = 46,
        .x = 0,
        .y = 0,
        .pri = 143360,
        .alpha = 128,
    },
    {
        .tex0 = 2307430728050191640,
        .u = 0,
        .v = 0,
        .w = 16,
        .h = 21,
        .x = 0,
        .y = 0,
        .pri = 184320,
        .alpha = 128,
    },
    {
        .tex0 = 2307430728050191640,
        .u = 16,
        .v = 0,
        .w = 16,
        .h = 21,
        .x = 0,
        .y = 0,
        .pri = 184320,
        .alpha = 128,
    },
    {
        .tex0 = 2307430728050191640,
        .u = 32,
        .v = 0,
        .w = 16,
        .h = 21,
        .x = 0,
        .y = 0,
        .pri = 184320,
        .alpha = 128,
    },
    {
        .tex0 = 2307430728050191640,
        .u = 48,
        .v = 0,
        .w = 16,
        .h = 21,
        .x = 0,
        .y = 0,
        .pri = 184320,
        .alpha = 128,
    },
    {
        .tex0 = 2307430728050191640,
        .u = 0,
        .v = 21,
        .w = 16,
        .h = 21,
        .x = 0,
        .y = 0,
        .pri = 184320,
        .alpha = 128,
    },
    {
        .tex0 = 2307430728050191640,
        .u = 16,
        .v = 21,
        .w = 16,
        .h = 21,
        .x = 0,
        .y = 0,
        .pri = 184320,
        .alpha = 128,
    },
    {
        .tex0 = 2307430728050191640,
        .u = 32,
        .v = 21,
        .w = 16,
        .h = 21,
        .x = 0,
        .y = 0,
        .pri = 184320,
        .alpha = 128,
    },
    {
        .tex0 = 2307430728050191640,
        .u = 48,
        .v = 21,
        .w = 16,
        .h = 21,
        .x = 0,
        .y = 0,
        .pri = 184320,
        .alpha = 128,
    },
    {
        .tex0 = 2307430728050191640,
        .u = 0,
        .v = 42,
        .w = 16,
        .h = 21,
        .x = 0,
        .y = 0,
        .pri = 184320,
        .alpha = 128,
    },
    {
        .tex0 = 2307430728050191640,
        .u = 16,
        .v = 42,
        .w = 16,
        .h = 21,
        .x = 0,
        .y = 0,
        .pri = 184320,
        .alpha = 128,
    },
    {
        .tex0 = 2307396367170972708,
        .u = 8,
        .v = 9,
        .w = 12,
        .h = 4,
        .x = 107,
        .y = 0,
        .pri = 143360,
        .alpha = 128,
    },
    {
        .tex0 = 2307396367170972708,
        .u = 8,
        .v = 9,
        .w = 12,
        .h = 4,
        .x = 107,
        .y = 148,
        .pri = 143360,
        .alpha = 128,
    },
    {
        .tex0 = 2307396367170972708,
        .u = 1,
        .v = 9,
        .w = 5,
        .h = 12,
        .x = 0,
        .y = 70,
        .pri = 143360,
        .alpha = 128,
    },
    {
        .tex0 = 2307396367170972708,
        .u = 1,
        .v = 9,
        .w = 5,
        .h = 12,
        .x = 221,
        .y = 70,
        .pri = 143360,
        .alpha = 128,
    },
    {
        .tex0 = 2307075172336706307,
        .u = 0,
        .v = 0,
        .w = 14,
        .h = 16,
        .x = 28,
        .y = 80,
        .pri = 163840,
        .alpha = 128,
    },
    {
        .tex0 = 2307075172336706307,
        .u = 0,
        .v = 16,
        .w = 14,
        .h = 16,
        .x = 28,
        .y = 232,
        .pri = 163840,
        .alpha = 128,
    },
    {
        .tex0 = 2307075172336706307,
        .u = 0,
        .v = 0,
        .w = 14,
        .h = 10,
        .x = 28,
        .y = 159,
        .pri = 163840,
        .alpha = 128,
    },
    {
        .tex0 = 2307075172336706307,
        .u = 14,
        .v = 0,
        .w = 14,
        .h = 12,
        .x = 28,
        .y = 81,
        .pri = 143360,
        .alpha = 128,
    },
    {
        .tex0 = 2307075172336706307,
        .u = 14,
        .v = 14,
        .w = 14,
        .h = 12,
        .x = 29,
        .y = 237,
        .pri = 143360,
        .alpha = 128,
    },
    {
        .tex0 = 2307074760019845888,
        .u = 0,
        .v = 0,
        .w = 14,
        .h = 16,
        .x = 28,
        .y = 89,
        .pri = 143360,
        .alpha = 128,
    },
    {
        .tex0 = 2307074760019845888,
        .u = 0,
        .v = 16,
        .w = 14,
        .h = 16,
        .x = 28,
        .y = 105,
        .pri = 143360,
        .alpha = 128,
    },
    {
        .tex0 = 2307074760019845888,
        .u = 14,
        .v = 0,
        .w = 14,
        .h = 16,
        .x = 28,
        .y = 105,
        .pri = 143360,
        .alpha = 128,
    },
    {
        .tex0 = 2307510445997632868,
        .u = 1,
        .v = 1,
        .w = 104,
        .h = 169,
        .x = 24,
        .y = 70,
        .pri = 204800,
        .alpha = 128,
    },
    {
        .tex0 = 2307510445997632868,
        .u = 129,
        .v = 1,
        .w = 104,
        .h = 98,
        .x = 24,
        .y = 240,
        .pri = 204800,
        .alpha = 128,
    },
    {
        .tex0 = 2307510445997632868,
        .u = 129,
        .v = 129,
        .w = 104,
        .h = 80,
        .x = 32,
        .y = 166,
        .pri = 204800,
        .alpha = 128,
    },
    {
        .tex0 = 2307510445997632868,
        .u = 1,
        .v = 1,
        .w = 104,
        .h = 169,
        .x = 512,
        .y = 70,
        .pri = 204800,
        .alpha = 128,
    },
    {
        .tex0 = 2307510445997632868,
        .u = 129,
        .v = 1,
        .w = 104,
        .h = 98,
        .x = 512,
        .y = 240,
        .pri = 204800,
        .alpha = 128,
    },
    {
        .tex0 = 2307510445997632868,
        .u = 129,
        .v = 129,
        .w = 104,
        .h = 80,
        .x = 528,
        .y = 166,
        .pri = 204800,
        .alpha = 128,
    },
    {
        .tex0 = 2307429078848810228,
        .u = 0,
        .v = 0,
        .w = 52,
        .h = 26,
        .x = 50,
        .y = 368,
        .pri = 163840,
        .alpha = 128,
    },
    {
        .tex0 = 2307429078848810228,
        .u = 0,
        .v = 26,
        .w = 52,
        .h = 26,
        .x = 333,
        .y = 340,
        .pri = 163840,
        .alpha = 128,
    },
    {
        .tex0 = 2307429078848810228,
        .u = 64,
        .v = 0,
        .w = 52,
        .h = 26,
        .x = 333,
        .y = 368,
        .pri = 163840,
        .alpha = 128,
    },
    {
        .tex0 = 2307429078848810228,
        .u = 64,
        .v = 26,
        .w = 52,
        .h = 26,
        .x = 333,
        .y = 395,
        .pri = 163840,
        .alpha = 128,
    },
    {
        .tex0 = 2307513191489384296,
        .u = 97,
        .v = 1,
        .w = 30,
        .h = 30,
        .x = 63,
        .y = 121,
        .pri = 163840,
        .alpha = 128,
    },
    {
        .tex0 = 2307513191489384296,
        .u = 97,
        .v = 1,
        .w = 30,
        .h = 30,
        .x = 547,
        .y = 121,
        .pri = 163840,
        .alpha = 128,
    },
    {
        .tex0 = 2307439936460074331,
        .u = 0,
        .v = 0,
        .w = 64,
        .h = 32,
        .x = 49,
        .y = 253,
        .pri = 122880,
        .alpha = 128,
    },
    {
        .tex0 = 2307439936460074331,
        .u = 0,
        .v = 32,
        .w = 64,
        .h = 32,
        .x = 526,
        .y = 253,
        .pri = 122880,
        .alpha = 128,
    },
    {
        .tex0 = 2307434713779842357,
        .u = 1,
        .v = 1,
        .w = 62,
        .h = 54,
        .x = 119,
        .y = 63,
        .pri = 143360,
        .alpha = 128,
    },
    {
        .tex0 = 2307434713779842357,
        .u = 1,
        .v = 1,
        .w = 62,
        .h = 54,
        .x = 456,
        .y = 63,
        .pri = 143360,
        .alpha = 128,
    },
    {
        .tex0 = 2307434713779842357,
        .u = 1,
        .v = 1,
        .w = 62,
        .h = 54,
        .x = 119,
        .y = 290,
        .pri = 143360,
        .alpha = 128,
    },
    {
        .tex0 = 2307434713779842357,
        .u = 1,
        .v = 1,
        .w = 62,
        .h = 54,
        .x = 456,
        .y = 290,
        .pri = 143360,
        .alpha = 128,
    },
    {
        .tex0 = 2307396367170972708,
        .u = 1,
        .v = 1,
        .w = 20,
        .h = 6,
        .x = 309,
        .y = 71,
        .pri = 163840,
        .alpha = 128,
    },
    {
        .tex0 = 2307396367170972708,
        .u = 1,
        .v = 1,
        .w = 20,
        .h = 6,
        .x = 309,
        .y = 329,
        .pri = 163840,
        .alpha = 128,
    },
    {
        .tex0 = 2307396367170972708,
        .u = 23,
        .v = 1,
        .w = 8,
        .h = 16,
        .x = 126,
        .y = 195,
        .pri = 163840,
        .alpha = 128,
    },
    {
        .tex0 = 2307396367170972708,
        .u = 23,
        .v = 1,
        .w = 8,
        .h = 16,
        .x = 504,
        .y = 195,
        .pri = 163840,
        .alpha = 128,
    },
    {
        .tex0 = 2307513877543300985,
        .u = 1,
        .v = 1,
        .w = 48,
        .h = 24,
        .x = 0,
        .y = 0,
        .pri = 163840,
        .alpha = 128,
    },
    {
        .tex0 = 2307516215146360702,
        .u = 1,
        .v = 1,
        .w = 48,
        .h = 22,
        .x = 49,
        .y = 73,
        .pri = 163840,
        .alpha = 128,
    },
    {
        .tex0 = 2307516215146360702,
        .u = 49,
        .v = 1,
        .w = 48,
        .h = 22,
        .x = 148,
        .y = 73,
        .pri = 163840,
        .alpha = 128,
    },
    {
        .tex0 = 2307516215146360702,
        .u = 1,
        .v = 25,
        .w = 48,
        .h = 22,
        .x = 379,
        .y = 73,
        .pri = 163840,
        .alpha = 128,
    },
    {
        .tex0 = 2307516215146360702,
        .u = 49,
        .v = 25,
        .w = 48,
        .h = 22,
        .x = 543,
        .y = 73,
        .pri = 163840,
        .alpha = 128,
    },
    {
        .tex0 = 2307188837772764480,
        .u = 1,
        .v = 1,
        .w = 126,
        .h = 178,
        .x = 0,
        .y = 0,
        .pri = 520192,
        .alpha = 128,
    },
    {
        .tex0 = 2307188837772764480,
        .u = 129,
        .v = 1,
        .w = 126,
        .h = 178,
        .x = 320,
        .y = 0,
        .pri = 520192,
        .alpha = 128,
    },
    {
        .tex0 = 2307409975908019273,
        .u = 1,
        .v = 1,
        .w = 122,
        .h = 32,
        .x = 40,
        .y = 183,
        .pri = 163840,
        .alpha = 128,
    },
    {
        .tex0 = 2307409975908019273,
        .u = 1,
        .v = 31,
        .w = 122,
        .h = 32,
        .x = 40,
        .y = 221,
        .pri = 163840,
        .alpha = 128,
    },
    {
        .tex0 = 2307409975908019273,
        .u = 1,
        .v = 61,
        .w = 122,
        .h = 32,
        .x = 40,
        .y = 259,
        .pri = 163840,
        .alpha = 128,
    },
    {
        .tex0 = 2307409975908019273,
        .u = 1,
        .v = 91,
        .w = 122,
        .h = 32,
        .x = 40,
        .y = 298,
        .pri = 163840,
        .alpha = 128,
    },
    {
        .tex0 = 2307400630059183109,
        .u = 1,
        .v = 1,
        .w = 110,
        .h = 78,
        .x = 53,
        .y = 82,
        .pri = 102400,
        .alpha = 128,
    },
    {
        .tex0 = 2307391694313663488,
        .u = 1,
        .v = 1,
        .w = 56,
        .h = 30,
        .x = 199,
        .y = 44,
        .pri = 163840,
        .alpha = 128,
    },
    {
        .tex0 = 2307075172336706307,
        .u = 0,
        .v = 0,
        .w = 14,
        .h = 16,
        .x = 593,
        .y = 31,
        .pri = 163840,
        .alpha = 128,
    },
    {
        .tex0 = 2307075172336706307,
        .u = 0,
        .v = 16,
        .w = 14,
        .h = 16,
        .x = 593,
        .y = 309,
        .pri = 163840,
        .alpha = 128,
    },
    {
        .tex0 = 2307075172336706307,
        .u = 0,
        .v = 0,
        .w = 14,
        .h = 10,
        .x = 593,
        .y = 173,
        .pri = 163840,
        .alpha = 128,
    },
    {
        .tex0 = 2307075172336706307,
        .u = 14,
        .v = 0,
        .w = 14,
        .h = 12,
        .x = 593,
        .y = 31,
        .pri = 143360,
        .alpha = 128,
    },
    {
        .tex0 = 2307075172336706307,
        .u = 14,
        .v = 14,
        .w = 14,
        .h = 12,
        .x = 593,
        .y = 314,
        .pri = 143360,
        .alpha = 128,
    },
    {
        .tex0 = 2307074760019845888,
        .u = 0,
        .v = 0,
        .w = 14,
        .h = 16,
        .x = 593,
        .y = 38,
        .pri = 143360,
        .alpha = 128,
    },
    {
        .tex0 = 2307074760019845888,
        .u = 0,
        .v = 16,
        .w = 14,
        .h = 16,
        .x = 593,
        .y = 54,
        .pri = 143360,
        .alpha = 128,
    },
    {
        .tex0 = 2307074760019845888,
        .u = 14,
        .v = 0,
        .w = 14,
        .h = 16,
        .x = 593,
        .y = 54,
        .pri = 143360,
        .alpha = 128,
    },
    {
        .tex0 = 2307480895616068749,
        .u = 1,
        .v = 1,
        .w = 270,
        .h = 254,
        .x = 50,
        .y = 103,
        .pri = 102400,
        .alpha = 128,
    },
    {
        .tex0 = 2307480895616068749,
        .u = 289,
        .v = 1,
        .w = 128,
        .h = 62,
        .x = 50,
        .y = 357,
        .pri = 102400,
        .alpha = 128,
    },
    {
        .tex0 = 2307480895616068749,
        .u = 289,
        .v = 65,
        .w = 142,
        .h = 62,
        .x = 178,
        .y = 357,
        .pri = 102400,
        .alpha = 128,
    },
    {
        .tex0 = 2307480895616068749,
        .u = 1,
        .v = 1,
        .w = 270,
        .h = 254,
        .x = 320,
        .y = 103,
        .pri = 102400,
        .alpha = 128,
    },
    {
        .tex0 = 2307480895616068749,
        .u = 289,
        .v = 1,
        .w = 128,
        .h = 62,
        .x = 462,
        .y = 357,
        .pri = 102400,
        .alpha = 128,
    },
    {
        .tex0 = 2307480895616068749,
        .u = 289,
        .v = 65,
        .w = 142,
        .h = 62,
        .x = 320,
        .y = 357,
        .pri = 102400,
        .alpha = 128,
    },
    {
        .tex0 = 2307480895616068749,
        .u = 289,
        .v = 129,
        .w = 8,
        .h = 50,
        .x = 212,
        .y = 104,
        .pri = 102400,
        .alpha = 128,
    },
    {
        .tex0 = 2307480895616068749,
        .u = 299,
        .v = 129,
        .w = 8,
        .h = 64,
        .x = 162,
        .y = 162,
        .pri = 102400,
        .alpha = 128,
    },
    {
        .tex0 = 2307480895616068749,
        .u = 369,
        .v = 129,
        .w = 8,
        .h = 6,
        .x = 134,
        .y = 220,
        .pri = 102400,
        .alpha = 128,
    },
    {
        .tex0 = 2307480895616068749,
        .u = 369,
        .v = 137,
        .w = 8,
        .h = 6,
        .x = 70,
        .y = 220,
        .pri = 102400,
        .alpha = 128,
    },
    {
        .tex0 = 2307480895616068749,
        .u = 385,
        .v = 129,
        .w = 58,
        .h = 8,
        .x = 162,
        .y = 154,
        .pri = 102400,
        .alpha = 128,
    },
    {
        .tex0 = 2307480895616068749,
        .u = 385,
        .v = 139,
        .w = 36,
        .h = 8,
        .x = 134,
        .y = 226,
        .pri = 102400,
        .alpha = 128,
    },
    {
        .tex0 = 2307480895616068749,
        .u = 385,
        .v = 149,
        .w = 72,
        .h = 8,
        .x = 70,
        .y = 212,
        .pri = 102400,
        .alpha = 128,
    },
    {
        .tex0 = 2307480895616068749,
        .u = 385,
        .v = 159,
        .w = 28,
        .h = 8,
        .x = 50,
        .y = 226,
        .pri = 102400,
        .alpha = 128,
    },
    {
        .tex0 = 2307480895616068749,
        .u = 309,
        .v = 129,
        .w = 8,
        .h = 50,
        .x = 421,
        .y = 103,
        .pri = 102400,
        .alpha = 128,
    },
    {
        .tex0 = 2307480895616068749,
        .u = 319,
        .v = 129,
        .w = 8,
        .h = 64,
        .x = 471,
        .y = 161,
        .pri = 102400,
        .alpha = 128,
    },
    {
        .tex0 = 2307480895616068749,
        .u = 369,
        .v = 145,
        .w = 8,
        .h = 6,
        .x = 499,
        .y = 219,
        .pri = 102400,
        .alpha = 128,
    },
    {
        .tex0 = 2307480895616068749,
        .u = 369,
        .v = 153,
        .w = 8,
        .h = 6,
        .x = 563,
        .y = 219,
        .pri = 102400,
        .alpha = 128,
    },
    {
        .tex0 = 2307480895616068749,
        .u = 385,
        .v = 169,
        .w = 58,
        .h = 8,
        .x = 421,
        .y = 153,
        .pri = 102400,
        .alpha = 128,
    },
    {
        .tex0 = 2307480895616068749,
        .u = 385,
        .v = 179,
        .w = 36,
        .h = 8,
        .x = 471,
        .y = 225,
        .pri = 102400,
        .alpha = 128,
    },
    {
        .tex0 = 2307480895616068749,
        .u = 385,
        .v = 189,
        .w = 72,
        .h = 8,
        .x = 499,
        .y = 211,
        .pri = 102400,
        .alpha = 128,
    },
    {
        .tex0 = 2307480895616068749,
        .u = 385,
        .v = 199,
        .w = 28,
        .h = 8,
        .x = 563,
        .y = 225,
        .pri = 102400,
        .alpha = 128,
    },
    {
        .tex0 = 2307480895616068749,
        .u = 329,
        .v = 129,
        .w = 8,
        .h = 50,
        .x = 212,
        .y = 370,
        .pri = 102400,
        .alpha = 128,
    },
    {
        .tex0 = 2307480895616068749,
        .u = 339,
        .v = 129,
        .w = 8,
        .h = 64,
        .x = 162,
        .y = 298,
        .pri = 102400,
        .alpha = 128,
    },
    {
        .tex0 = 2307480895616068749,
        .u = 369,
        .v = 161,
        .w = 8,
        .h = 6,
        .x = 134,
        .y = 298,
        .pri = 102400,
        .alpha = 128,
    },
    {
        .tex0 = 2307480895616068749,
        .u = 369,
        .v = 169,
        .w = 8,
        .h = 6,
        .x = 70,
        .y = 298,
        .pri = 102400,
        .alpha = 128,
    },
    {
        .tex0 = 2307480895616068749,
        .u = 385,
        .v = 209,
        .w = 58,
        .h = 8,
        .x = 162,
        .y = 362,
        .pri = 102400,
        .alpha = 128,
    },
    {
        .tex0 = 2307480895616068749,
        .u = 385,
        .v = 219,
        .w = 36,
        .h = 8,
        .x = 134,
        .y = 290,
        .pri = 102400,
        .alpha = 128,
    },
    {
        .tex0 = 2307480895616068749,
        .u = 385,
        .v = 229,
        .w = 72,
        .h = 8,
        .x = 70,
        .y = 304,
        .pri = 102400,
        .alpha = 128,
    },
    {
        .tex0 = 2307480895616068749,
        .u = 385,
        .v = 239,
        .w = 28,
        .h = 8,
        .x = 50,
        .y = 290,
        .pri = 102400,
        .alpha = 128,
    },
    {
        .tex0 = 2307480895616068749,
        .u = 349,
        .v = 129,
        .w = 8,
        .h = 50,
        .x = 420,
        .y = 369,
        .pri = 102400,
        .alpha = 128,
    },
    {
        .tex0 = 2307480895616068749,
        .u = 359,
        .v = 129,
        .w = 8,
        .h = 64,
        .x = 470,
        .y = 297,
        .pri = 102400,
        .alpha = 128,
    },
    {
        .tex0 = 2307480895616068749,
        .u = 369,
        .v = 177,
        .w = 8,
        .h = 6,
        .x = 498,
        .y = 297,
        .pri = 102400,
        .alpha = 128,
    },
    {
        .tex0 = 2307480895616068749,
        .u = 369,
        .v = 185,
        .w = 8,
        .h = 6,
        .x = 562,
        .y = 297,
        .pri = 102400,
        .alpha = 128,
    },
    {
        .tex0 = 2307480895616068749,
        .u = 289,
        .v = 200,
        .w = 58,
        .h = 8,
        .x = 420,
        .y = 361,
        .pri = 102400,
        .alpha = 128,
    },
    {
        .tex0 = 2307480895616068749,
        .u = 289,
        .v = 210,
        .w = 36,
        .h = 8,
        .x = 470,
        .y = 289,
        .pri = 102400,
        .alpha = 128,
    },
    {
        .tex0 = 2307480895616068749,
        .u = 289,
        .v = 220,
        .w = 72,
        .h = 8,
        .x = 498,
        .y = 303,
        .pri = 102400,
        .alpha = 128,
    },
    {
        .tex0 = 2307480895616068749,
        .u = 289,
        .v = 230,
        .w = 28,
        .h = 8,
        .x = 562,
        .y = 289,
        .pri = 102400,
        .alpha = 128,
    },
    {
        .tex0 = 2307490240256847505,
        .u = 1,
        .v = 1,
        .w = 45,
        .h = 72,
        .x = 89,
        .y = 94,
        .pri = 184320,
        .alpha = 128,
    },
    {
        .tex0 = 2307490240256847505,
        .u = 48,
        .v = 1,
        .w = 49,
        .h = 72,
        .x = 83,
        .y = 91,
        .pri = 184320,
        .alpha = 128,
    },
    {
        .tex0 = 2307490240256847505,
        .u = 1,
        .v = 75,
        .w = 85,
        .h = 52,
        .x = 66,
        .y = 105,
        .pri = 184320,
        .alpha = 128,
    },
    {
        .tex0 = 2307492987963223765,
        .u = 1,
        .v = 1,
        .w = 50,
        .h = 62,
        .x = 83,
        .y = 96,
        .pri = 184320,
        .alpha = 128,
    },
    {
        .tex0 = 2307492987963223765,
        .u = 53,
        .v = 1,
        .w = 50,
        .h = 14,
        .x = 83,
        .y = 158,
        .pri = 184320,
        .alpha = 128,
    },
    {
        .tex0 = 2307495324425432806,
        .u = 1,
        .v = 1,
        .w = 41,
        .h = 62,
        .x = 88,
        .y = 92,
        .pri = 184320,
        .alpha = 128,
    },
    {
        .tex0 = 2307495324425432806,
        .u = 44,
        .v = 1,
        .w = 41,
        .h = 11,
        .x = 88,
        .y = 154,
        .pri = 184320,
        .alpha = 128,
    },
    {
        .tex0 = 2307497660887641847,
        .u = 1,
        .v = 1,
        .w = 48,
        .h = 62,
        .x = 85,
        .y = 92,
        .pri = 184320,
        .alpha = 128,
    },
    {
        .tex0 = 2307497660887641847,
        .u = 58,
        .v = 1,
        .w = 48,
        .h = 12,
        .x = 85,
        .y = 154,
        .pri = 184320,
        .alpha = 128,
    },
    {
        .tex0 = 2307399942864415744,
        .u = 1,
        .v = 1,
        .w = 119,
        .h = 83,
        .x = 0,
        .y = 0,
        .pri = 204800,
        .alpha = 128,
    },
    {
        .tex0 = 2307399942864415744,
        .u = 1,
        .v = 1,
        .w = 119,
        .h = 83,
        .x = 119,
        .y = 0,
        .pri = 204800,
        .alpha = 128,
    },
    {
        .tex0 = 2307399942864415744,
        .u = 1,
        .v = 1,
        .w = 119,
        .h = 83,
        .x = 0,
        .y = 83,
        .pri = 204800,
        .alpha = 128,
    },
    {
        .tex0 = 2307399942864415744,
        .u = 1,
        .v = 1,
        .w = 119,
        .h = 83,
        .x = 119,
        .y = 83,
        .pri = 204800,
        .alpha = 128,
    },
    {
        .tex0 = 2307409288781442116,
        .u = 1,
        .v = 1,
        .w = 160,
        .h = 112,
        .x = 0,
        .y = 0,
        .pri = 409600,
        .alpha = 128,
    },
    {
        .tex0 = 2307411624102767749,
        .u = 14,
        .v = 3,
        .w = 35,
        .h = 9,
        .x = 514,
        .y = 35,
        .pri = 40960,
        .alpha = 128,
    },
    {
        .tex0 = 2307411624102767749,
        .u = 14,
        .v = 3,
        .w = 35,
        .h = 9,
        .x = 514,
        .y = 65,
        .pri = 40960,
        .alpha = 128,
    },
    {
        .tex0 = 2307411624102767749,
        .u = 14,
        .v = 3,
        .w = 35,
        .h = 9,
        .x = 514,
        .y = 95,
        .pri = 40960,
        .alpha = 128,
    },
    {
        .tex0 = 2307411624102767749,
        .u = 68,
        .v = 1,
        .w = 50,
        .h = 20,
        .x = 552,
        .y = 27,
        .pri = 40960,
        .alpha = 128,
    },
    {
        .tex0 = 2307411624102767749,
        .u = 68,
        .v = 25,
        .w = 50,
        .h = 20,
        .x = 552,
        .y = 57,
        .pri = 40960,
        .alpha = 128,
    },
    {
        .tex0 = 2307411624102767749,
        .u = 0,
        .v = 36,
        .w = 60,
        .h = 26,
        .x = 552,
        .y = 87,
        .pri = 40960,
        .alpha = 128,
    },
    {
        .tex0 = 2307411624102767749,
        .u = 3,
        .v = 17,
        .w = 57,
        .h = 19,
        .x = 297,
        .y = 366,
        .pri = 40960,
        .alpha = 128,
    },
    {
        .tex0 = 2307411624102767749,
        .u = 67,
        .v = 46,
        .w = 49,
        .h = 18,
        .x = 464,
        .y = 87,
        .pri = 40960,
        .alpha = 128,
    },
    {
        .tex0 = 2307413960564976790,
        .u = 9,
        .v = 47,
        .w = 16,
        .h = 16,
        .x = 313,
        .y = 76,
        .pri = 40960,
        .alpha = 128,
    },
    {
        .tex0 = 2307413960564976790,
        .u = 9,
        .v = 47,
        .w = 16,
        .h = 16,
        .x = 313,
        .y = 338,
        .pri = 40960,
        .alpha = 128,
    },
    {
        .tex0 = 2307413960564976790,
        .u = 9,
        .v = 47,
        .w = 16,
        .h = 16,
        .x = 151,
        .y = 206,
        .pri = 40960,
        .alpha = 128,
    },
    {
        .tex0 = 2307413960564976790,
        .u = 9,
        .v = 47,
        .w = 16,
        .h = 16,
        .x = 473,
        .y = 206,
        .pri = 40960,
        .alpha = 128,
    },
    {
        .tex0 = 2307413960564976790,
        .u = 4,
        .v = 5,
        .w = 28,
        .h = 28,
        .x = 473,
        .y = 55,
        .pri = 40960,
        .alpha = 128,
    },
    {
        .tex0 = 2307413960564976790,
        .u = 45,
        .v = 33,
        .w = 64,
        .h = 30,
        .x = 39,
        .y = 397,
        .pri = 40960,
        .alpha = 128,
    },
    {
        .tex0 = 2307413960564976790,
        .u = 42,
        .v = 1,
        .w = 64,
        .h = 30,
        .x = 536,
        .y = 397,
        .pri = 40960,
        .alpha = 128,
    },
    {
        .tex0 = 2307415196373658791,
        .u = 1,
        .v = 1,
        .w = 22,
        .h = 8,
        .x = 0,
        .y = 0,
        .pri = 307200,
        .alpha = 128,
    },
    {
        .tex0 = 2307415196373658791,
        .u = 1,
        .v = 11,
        .w = 22,
        .h = 8,
        .x = 0,
        .y = 0,
        .pri = 307200,
        .alpha = 128,
    },
    {
        .tex0 = 2307415196373658791,
        .u = 1,
        .v = 21,
        .w = 22,
        .h = 8,
        .x = 0,
        .y = 0,
        .pri = 307200,
        .alpha = 128,
    },
    {
        .tex0 = 2307415196373658791,
        .u = 26,
        .v = 1,
        .w = 11,
        .h = 20,
        .x = 0,
        .y = 0,
        .pri = 307200,
        .alpha = 128,
    },
    {
        .tex0 = 2307415196373658791,
        .u = 43,
        .v = 1,
        .w = 20,
        .h = 20,
        .x = 0,
        .y = 0,
        .pri = 307200,
        .alpha = 128,
    },
    {
        .tex0 = 2307486119371091123,
        .u = 2,
        .v = 436,
        .w = 48,
        .h = 61,
        .x = 224,
        .y = 405,
        .pri = 4096,
        .alpha = 128,
    },
    {
        .tex0 = 2307486119371091123,
        .u = 1,
        .v = 347,
        .w = 79,
        .h = 47,
        .x = 142,
        .y = 365,
        .pri = 4096,
        .alpha = 128,
    },
    {
        .tex0 = 2307486119371091123,
        .u = 82,
        .v = 324,
        .w = 35,
        .h = 75,
        .x = 204,
        .y = 359,
        .pri = 4096,
        .alpha = 128,
    },
    {
        .tex0 = 2307486119371091123,
        .u = 1,
        .v = 394,
        .w = 63,
        .h = 42,
        .x = 223,
        .y = 370,
        .pri = 4096,
        .alpha = 128,
    },
    {
        .tex0 = 2307486119371091123,
        .u = 240,
        .v = 2,
        .w = 76,
        .h = 64,
        .x = 263,
        .y = 370,
        .pri = 4096,
        .alpha = 128,
    },
    {
        .tex0 = 2307486119371091123,
        .u = 87,
        .v = 453,
        .w = 32,
        .h = 44,
        .x = 305,
        .y = 369,
        .pri = 4096,
        .alpha = 128,
    },
    {
        .tex0 = 2307486119371091123,
        .u = 156,
        .v = 380,
        .w = 76,
        .h = 60,
        .x = 297,
        .y = 318,
        .pri = 4096,
        .alpha = 128,
    },
    {
        .tex0 = 2307486119371091123,
        .u = 1,
        .v = 249,
        .w = 55,
        .h = 37,
        .x = 155,
        .y = 282,
        .pri = 4096,
        .alpha = 128,
    },
    {
        .tex0 = 2307486119371091123,
        .u = 1,
        .v = 286,
        .w = 78,
        .h = 61,
        .x = 135,
        .y = 311,
        .pri = 4096,
        .alpha = 128,
    },
    {
        .tex0 = 2307486119371091123,
        .u = 75,
        .v = 231,
        .w = 34,
        .h = 46,
        .x = 144,
        .y = 318,
        .pri = 4096,
        .alpha = 128,
    },
    {
        .tex0 = 2307486119371091123,
        .u = 83,
        .v = 280,
        .w = 34,
        .h = 41,
        .x = 203,
        .y = 324,
        .pri = 4096,
        .alpha = 128,
    },
    {
        .tex0 = 2307486119371091123,
        .u = 117,
        .v = 280,
        .w = 36,
        .h = 51,
        .x = 230,
        .y = 325,
        .pri = 4096,
        .alpha = 128,
    },
    {
        .tex0 = 2307486119371091123,
        .u = 80,
        .v = 398,
        .w = 34,
        .h = 49,
        .x = 258,
        .y = 317,
        .pri = 4096,
        .alpha = 128,
    },
    {
        .tex0 = 2307486119371091123,
        .u = 178,
        .v = 5,
        .w = 57,
        .h = 61,
        .x = 258,
        .y = 316,
        .pri = 4096,
        .alpha = 128,
    },
    {
        .tex0 = 2307486119371091123,
        .u = 159,
        .v = 324,
        .w = 60,
        .h = 55,
        .x = 299,
        .y = 268,
        .pri = 4096,
        .alpha = 128,
    },
    {
        .tex0 = 2307486119371091123,
        .u = 1,
        .v = 168,
        .w = 46,
        .h = 38,
        .x = 142,
        .y = 254,
        .pri = 4096,
        .alpha = 128,
    },
    {
        .tex0 = 2307486119371091123,
        .u = 76,
        .v = 140,
        .w = 97,
        .h = 90,
        .x = 186,
        .y = 241,
        .pri = 4096,
        .alpha = 128,
    },
    {
        .tex0 = 2307486119371091123,
        .u = 118,
        .v = 396,
        .w = 34,
        .h = 79,
        .x = 275,
        .y = 246,
        .pri = 4096,
        .alpha = 128,
    },
    {
        .tex0 = 2307486119371091123,
        .u = 322,
        .v = 3,
        .w = 42,
        .h = 27,
        .x = 309,
        .y = 249,
        .pri = 4096,
        .alpha = 128,
    },
    {
        .tex0 = 2307486119371091123,
        .u = 1,
        .v = 206,
        .w = 43,
        .h = 43,
        .x = 196,
        .y = 225,
        .pri = 4096,
        .alpha = 128,
    },
    {
        .tex0 = 2307486119371091123,
        .u = 160,
        .v = 443,
        .w = 131,
        .h = 46,
        .x = 193,
        .y = 314,
        .pri = 4096,
        .alpha = 128,
    },
    {
        .tex0 = 2307486119371091123,
        .u = 167,
        .v = 241,
        .w = 75,
        .h = 83,
        .x = 289,
        .y = 159,
        .pri = 4096,
        .alpha = 128,
    },
    {
        .tex0 = 2307486119371091123,
        .u = 76,
        .v = 3,
        .w = 98,
        .h = 137,
        .x = 168,
        .y = 117,
        .pri = 4096,
        .alpha = 128,
    },
    {
        .tex0 = 2307486119371091123,
        .u = 118,
        .v = 335,
        .w = 39,
        .h = 61,
        .x = 257,
        .y = 190,
        .pri = 4096,
        .alpha = 128,
    },
    {
        .tex0 = 2307486119371091123,
        .u = 210,
        .v = 206,
        .w = 33,
        .h = 16,
        .x = 233,
        .y = 310,
        .pri = 4096,
        .alpha = 128,
    },
    {
        .tex0 = 2307486119371091123,
        .u = 176,
        .v = 68,
        .w = 79,
        .h = 135,
        .x = 289,
        .y = 32,
        .pri = 4096,
        .alpha = 128,
    },
    {
        .tex0 = 2307486119371091123,
        .u = 174,
        .v = 205,
        .w = 35,
        .h = 36,
        .x = 328,
        .y = 37,
        .pri = 4096,
        .alpha = 128,
    },
    {
        .tex0 = 2307486119371091123,
        .u = 1,
        .v = 75,
        .w = 75,
        .h = 93,
        .x = 90,
        .y = 175,
        .pri = 4096,
        .alpha = 128,
    },
    {
        .tex0 = 2307486119371091123,
        .u = 318,
        .v = 76,
        .w = 118,
        .h = 101,
        .x = 115,
        .y = 134,
        .pri = 4096,
        .alpha = 128,
    },
    {
        .tex0 = 2307486119371091123,
        .u = 1,
        .v = 3,
        .w = 66,
        .h = 72,
        .x = 64,
        .y = 112,
        .pri = 4096,
        .alpha = 128,
    },
    {
        .tex0 = 2307486119371091123,
        .u = 312,
        .v = 177,
        .w = 127,
        .h = 100,
        .x = 175,
        .y = 231,
        .pri = 4096,
        .alpha = 128,
    },
    {
        .tex0 = 2307486119371091123,
        .u = 365,
        .v = 4,
        .w = 36,
        .h = 28,
        .x = 200,
        .y = 232,
        .pri = 4096,
        .alpha = 128,
    },
    {
        .tex0 = 2307486119371091123,
        .u = 401,
        .v = 4,
        .w = 69,
        .h = 72,
        .x = 285,
        .y = 290,
        .pri = 4096,
        .alpha = 128,
    },
    {
        .tex0 = 2307486119371091123,
        .u = 307,
        .v = 279,
        .w = 81,
        .h = 91,
        .x = 316,
        .y = 333,
        .pri = 4096,
        .alpha = 128,
    },
    {
        .tex0 = 2307486119371091123,
        .u = 306,
        .v = 381,
        .w = 122,
        .h = 82,
        .x = 369,
        .y = 357,
        .pri = 4096,
        .alpha = 128,
    },
    {
        .tex0 = 2307486119371091123,
        .u = 243,
        .v = 206,
        .w = 38,
        .h = 33,
        .x = 201,
        .y = 346,
        .pri = 4096,
        .alpha = 128,
    },
    {
        .tex0 = 2307486119371091123,
        .u = 53,
        .v = 450,
        .w = 32,
        .h = 45,
        .x = 282,
        .y = 366,
        .pri = 4096,
        .alpha = 128,
    },
    {
        .tex0 = 2307486119371091123,
        .u = 0,
        .v = 0,
        .w = 0,
        .h = 0,
        .x = 0,
        .y = 308,
        .pri = 4096,
        .alpha = 128,
    },
    {
        .tex0 = 2307486119371091123,
        .u = 256,
        .v = 108,
        .w = 56,
        .h = 50,
        .x = 218,
        .y = 308,
        .pri = 4096,
        .alpha = 128,
    },
    {
        .tex0 = 2307486119371091123,
        .u = 0,
        .v = 0,
        .w = 0,
        .h = 0,
        .x = 0,
        .y = 308,
        .pri = 4096,
        .alpha = 128,
    },
    {
        .tex0 = 2307486119371091123,
        .u = 83,
        .v = 280,
        .w = 34,
        .h = 41,
        .x = 203,
        .y = 324,
        .pri = 4096,
        .alpha = 128,
    },
    {
        .tex0 = 2307486119371091123,
        .u = 283,
        .v = 207,
        .w = 19,
        .h = 34,
        .x = 262,
        .y = 321,
        .pri = 4096,
        .alpha = 128,
    },
    {
        .tex0 = 2307486119371091123,
        .u = 255,
        .v = 158,
        .w = 60,
        .h = 44,
        .x = 177,
        .y = 334,
        .pri = 4096,
        .alpha = 128,
    },
    {
        .tex0 = 2307486119371091123,
        .u = 319,
        .v = 30,
        .w = 47,
        .h = 43,
        .x = 75,
        .y = 131,
        .pri = 4096,
        .alpha = 128,
    },
    {
        .tex0 = 2307486119371091123,
        .u = 242,
        .v = 306,
        .w = 58,
        .h = 66,
        .x = 260,
        .y = 261,
        .pri = 4096,
        .alpha = 128,
    },
    {
        .tex0 = 2307486119371091123,
        .u = 233,
        .v = 385,
        .w = 56,
        .h = 57,
        .x = 302,
        .y = 271,
        .pri = 4096,
        .alpha = 128,
    },
    {
        .tex0 = 2307486119371091123,
        .u = 242,
        .v = 242,
        .w = 63,
        .h = 63,
        .x = 223,
        .y = 350,
        .pri = 4096,
        .alpha = 128,
    },
    {
        .tex0 = 2307486119371091123,
        .u = 257,
        .v = 68,
        .w = 56,
        .h = 40,
        .x = 143,
        .y = 322,
        .pri = 4096,
        .alpha = 128,
    },
    {
        .tex0 = 2307486119371091123,
        .u = 235,
        .v = 375,
        .w = 50,
        .h = 11,
        .x = 275,
        .y = 321,
        .pri = 4096,
        .alpha = 128,
    },
    {
        .tex0 = 2307077510006874886,
        .u = 1,
        .v = 1,
        .w = 92,
        .h = 22,
        .x = 273,
        .y = 154,
        .pri = 163840,
        .alpha = 128,
    },
    {
        .tex0 = 2307077510006874886,
        .u = 1,
        .v = 36,
        .w = 126,
        .h = 27,
        .x = 252,
        .y = 171,
        .pri = 81920,
        .alpha = 128,
    },
    {
        .tex0 = 2307077510006874886,
        .u = 116,
        .v = 7,
        .w = 11,
        .h = 27,
        .x = 378,
        .y = 171,
        .pri = 81920,
        .alpha = 128,
    },
    {
        .tex0 = 2307087544124220223,
        .u = 1,
        .v = 1,
        .w = 124,
        .h = 30,
        .x = 48,
        .y = 35,
        .pri = 491520,
        .alpha = 128,
    },
    {
        .tex0 = 2307087544124220223,
        .u = 1,
        .v = 33,
        .w = 124,
        .h = 30,
        .x = 48,
        .y = 35,
        .pri = 491520,
        .alpha = 128,
    },
    {
        .tex0 = 2307087544124220223,
        .u = 1,
        .v = 65,
        .w = 124,
        .h = 30,
        .x = 48,
        .y = 35,
        .pri = 491520,
        .alpha = 128,
    },
    {
        .tex0 = 2307087954159379296,
        .u = 1,
        .v = 1,
        .w = 24,
        .h = 24,
        .x = 23,
        .y = 250,
        .pri = 102400,
        .alpha = 128,
    },
    {
        .tex0 = 2307087954159379296,
        .u = 1,
        .v = 1,
        .w = 24,
        .h = 24,
        .x = 593,
        .y = 250,
        .pri = 102400,
        .alpha = 128,
    },
    {
        .tex0 = 2307276798702987200,
        .u = 1,
        .v = 1,
        .w = 254,
        .h = 254,
        .x = 197,
        .y = 119,
        .pri = 512000,
        .alpha = 128,
    },
    {
        .tex0 = 2307312532830890180,
        .u = 1,
        .v = 1,
        .w = 129,
        .h = 254,
        .x = 451,
        .y = 119,
        .pri = 512000,
        .alpha = 128,
    },
    {
        .tex0 = 2307312532830890180,
        .u = 132,
        .v = 1,
        .w = 123,
        .h = 51,
        .x = 197,
        .y = 373,
        .pri = 512000,
        .alpha = 128,
    },
    {
        .tex0 = 2307312532830890180,
        .u = 132,
        .v = 54,
        .w = 123,
        .h = 51,
        .x = 320,
        .y = 373,
        .pri = 512000,
        .alpha = 128,
    },
    {
        .tex0 = 2307312532830890180,
        .u = 132,
        .v = 107,
        .w = 123,
        .h = 51,
        .x = 443,
        .y = 373,
        .pri = 512000,
        .alpha = 128,
    },
    {
        .tex0 = 2307312532830890180,
        .u = 132,
        .v = 160,
        .w = 14,
        .h = 51,
        .x = 566,
        .y = 373,
        .pri = 512000,
        .alpha = 128,
    },
    {
        .tex0 = 2307348266958793160,
        .u = 2,
        .v = 1,
        .w = 160,
        .h = 150,
        .x = 0,
        .y = 0,
        .pri = 512000,
        .alpha = 128,
    },
    {
        .tex0 = 2307348266958793160,
        .u = 166,
        .v = 1,
        .w = 24,
        .h = 55,
        .x = 68,
        .y = 30,
        .pri = 471040,
        .alpha = 128,
    },
    {
        .tex0 = 2307348266958793160,
        .u = 166,
        .v = 73,
        .w = 24,
        .h = 55,
        .x = 68,
        .y = 30,
        .pri = 491520,
        .alpha = 128,
    },
    {
        .tex0 = 2307348266958793160,
        .u = 192,
        .v = 1,
        .w = 18,
        .h = 70,
        .x = 71,
        .y = 15,
        .pri = 430080,
        .alpha = 128,
    },
    {
        .tex0 = 2307348266958793160,
        .u = 192,
        .v = 73,
        .w = 18,
        .h = 70,
        .x = 71,
        .y = 15,
        .pri = 450560,
        .alpha = 128,
    },
    {
        .tex0 = 2307348266958793160,
        .u = 212,
        .v = 1,
        .w = 16,
        .h = 67,
        .x = 72,
        .y = 15,
        .pri = 389120,
        .alpha = 128,
    },
    {
        .tex0 = 2307348266958793160,
        .u = 212,
        .v = 73,
        .w = 16,
        .h = 67,
        .x = 72,
        .y = 15,
        .pri = 409600,
        .alpha = 128,
    },
    {
        .tex0 = 2307426332284365824,
        .u = 1,
        .v = 1,
        .w = 254,
        .h = 203,
        .x = 173,
        .y = 207,
        .pri = 307200,
        .alpha = 128,
    },
    {
        .tex0 = 2307426332284365824,
        .u = 1,
        .v = 206,
        .w = 40,
        .h = 49,
        .x = 427,
        .y = 256,
        .pri = 307200,
        .alpha = 128,
    },
    {
        .tex0 = 2307426332284365824,
        .u = 43,
        .v = 206,
        .w = 40,
        .h = 49,
        .x = 427,
        .y = 305,
        .pri = 307200,
        .alpha = 128,
    },
    {
        .tex0 = 2307426332284365824,
        .u = 85,
        .v = 206,
        .w = 40,
        .h = 49,
        .x = 427,
        .y = 354,
        .pri = 307200,
        .alpha = 128,
    },
    {
        .tex0 = 2307426332284365824,
        .u = 127,
        .v = 206,
        .w = 40,
        .h = 49,
        .x = 427,
        .y = 403,
        .pri = 307200,
        .alpha = 128,
    },
    {
        .tex0 = 2307444473152482564,
        .u = 73,
        .v = 23,
        .w = 168,
        .h = 14,
        .x = 378,
        .y = 216,
        .pri = 184320,
        .alpha = 128,
    },
    {
        .tex0 = 2307444473152482564,
        .u = 73,
        .v = 1,
        .w = 174,
        .h = 20,
        .x = 375,
        .y = 213,
        .pri = 184320,
        .alpha = 128,
    },
    {
        .tex0 = 2307444473152482564,
        .u = 73,
        .v = 39,
        .w = 2,
        .h = 14,
        .x = 380,
        .y = 216,
        .pri = 163840,
        .alpha = 128,
    },
    {
        .tex0 = 2307444473152482564,
        .u = 1,
        .v = 6,
        .w = 70,
        .h = 44,
        .x = 54,
        .y = 107,
        .pri = 163840,
        .alpha = 128,
    },
    {
        .tex0 = 2307444473152482564,
        .u = 1,
        .v = 56,
        .w = 70,
        .h = 44,
        .x = 54,
        .y = 185,
        .pri = 163840,
        .alpha = 128,
    },
    {
        .tex0 = 2307444473152482564,
        .u = 77,
        .v = 45,
        .w = 70,
        .h = 44,
        .x = 54,
        .y = 247,
        .pri = 163840,
        .alpha = 128,
    },
    {
        .tex0 = 2307444473152482564,
        .u = 77,
        .v = 92,
        .w = 70,
        .h = 24,
        .x = 54,
        .y = 303,
        .pri = 163840,
        .alpha = 128,
    },
    {
        .tex0 = 2307445295438409096,
        .u = 1,
        .v = 10,
        .w = 10,
        .h = 10,
        .x = -16,
        .y = 11,
        .pri = 245760,
        .alpha = 128,
    },
    {
        .tex0 = 2307445295438409096,
        .u = 19,
        .v = 10,
        .w = 10,
        .h = 10,
        .x = 199,
        .y = 11,
        .pri = 245760,
        .alpha = 128,
    },
    {
        .tex0 = 2307515805110250891,
        .u = 1,
        .v = 1,
        .w = 310,
        .h = 208,
        .x = 165,
        .y = 92,
        .pri = 286720,
        .alpha = 128,
    },
    {
        .tex0 = 2307515805110250891,
        .u = 1,
        .v = 211,
        .w = 46,
        .h = 44,
        .x = 101,
        .y = 243,
        .pri = 245760,
        .alpha = 128,
    },
    {
        .tex0 = 2307515805110250891,
        .u = 47,
        .v = 211,
        .w = 46,
        .h = 44,
        .x = 101,
        .y = 106,
        .pri = 245760,
        .alpha = 128,
    },
    {
        .tex0 = 2307515805110250891,
        .u = 93,
        .v = 211,
        .w = 46,
        .h = 44,
        .x = 503,
        .y = 106,
        .pri = 245760,
        .alpha = 128,
    },
    {
        .tex0 = 2307515805110250891,
        .u = 139,
        .v = 211,
        .w = 46,
        .h = 44,
        .x = 503,
        .y = 243,
        .pri = 245760,
        .alpha = 128,
    },
    {
        .tex0 = 2307515805110250891,
        .u = 190,
        .v = 214,
        .w = 114,
        .h = 4,
        .x = 0,
        .y = 0,
        .pri = 266240,
        .alpha = 128,
    },
    {
        .tex0 = 2307515805110250891,
        .u = 187,
        .v = 223,
        .w = 8,
        .h = 8,
        .x = 0,
        .y = 0,
        .pri = 266240,
        .alpha = 128,
    },
    {
        .tex0 = 2307529685237542864,
        .u = 1,
        .v = 1,
        .w = 106,
        .h = 24,
        .x = 48,
        .y = 396,
        .pri = 266240,
        .alpha = 128,
    },
    {
        .tex0 = 2307529685237542864,
        .u = 1,
        .v = 27,
        .w = 106,
        .h = 24,
        .x = 114,
        .y = 396,
        .pri = 266240,
        .alpha = 128,
    },
    {
        .tex0 = 2307529685237542864,
        .u = 1,
        .v = 53,
        .w = 106,
        .h = 24,
        .x = 114,
        .y = 395,
        .pri = 266240,
        .alpha = 128,
    },
    {
        .tex0 = 2307565145628422258,
        .u = 1,
        .v = 1,
        .w = 190,
        .h = 22,
        .x = 0,
        .y = 0,
        .pri = 266240,
        .alpha = 128,
    },
    {
        .tex0 = 2307565145628422258,
        .u = 1,
        .v = 25,
        .w = 190,
        .h = 22,
        .x = 0,
        .y = 0,
        .pri = 266240,
        .alpha = 128,
    },
    {
        .tex0 = 2307565145628422258,
        .u = 1,
        .v = 49,
        .w = 190,
        .h = 22,
        .x = 0,
        .y = 0,
        .pri = 266240,
        .alpha = 128,
    },
    {
        .tex0 = 2307565145628422258,
        .u = 1,
        .v = 73,
        .w = 190,
        .h = 22,
        .x = 0,
        .y = 0,
        .pri = 266240,
        .alpha = 128,
    },
    {
        .tex0 = 2307565145628422258,
        .u = 1,
        .v = 97,
        .w = 190,
        .h = 22,
        .x = 0,
        .y = 0,
        .pri = 266240,
        .alpha = 128,
    },
    {
        .tex0 = 2307565145628422258,
        .u = 1,
        .v = 121,
        .w = 190,
        .h = 22,
        .x = 0,
        .y = 0,
        .pri = 266240,
        .alpha = 128,
    },
    {
        .tex0 = 2307565145628422258,
        .u = 1,
        .v = 145,
        .w = 190,
        .h = 22,
        .x = 0,
        .y = 0,
        .pri = 266240,
        .alpha = 128,
    },
    {
        .tex0 = 2307565145628422258,
        .u = 1,
        .v = 169,
        .w = 190,
        .h = 22,
        .x = 0,
        .y = 0,
        .pri = 266240,
        .alpha = 128,
    },
    {
        .tex0 = 2307547416003424241,
        .u = 1,
        .v = 1,
        .w = 190,
        .h = 22,
        .x = 0,
        .y = 0,
        .pri = 266240,
        .alpha = 128,
    },
    {
        .tex0 = 2307547416003424241,
        .u = 1,
        .v = 25,
        .w = 190,
        .h = 22,
        .x = 0,
        .y = 0,
        .pri = 266240,
        .alpha = 128,
    },
    {
        .tex0 = 2307547416003424241,
        .u = 1,
        .v = 49,
        .w = 190,
        .h = 22,
        .x = 0,
        .y = 0,
        .pri = 266240,
        .alpha = 128,
    },
    {
        .tex0 = 2307547416003424241,
        .u = 1,
        .v = 73,
        .w = 190,
        .h = 22,
        .x = 0,
        .y = 0,
        .pri = 266240,
        .alpha = 128,
    },
    {
        .tex0 = 2307547416003424241,
        .u = 1,
        .v = 97,
        .w = 190,
        .h = 22,
        .x = 0,
        .y = 0,
        .pri = 266240,
        .alpha = 128,
    },
    {
        .tex0 = 2307547416003424241,
        .u = 1,
        .v = 121,
        .w = 190,
        .h = 22,
        .x = 280,
        .y = 386,
        .pri = 266240,
        .alpha = 128,
    },
    {
        .tex0 = 2307547416003424241,
        .u = 1,
        .v = 145,
        .w = 190,
        .h = 22,
        .x = 400,
        .y = 406,
        .pri = 266240,
        .alpha = 128,
    },
    {
        .tex0 = 2307547416003424241,
        .u = 1,
        .v = 169,
        .w = 190,
        .h = 22,
        .x = 0,
        .y = 0,
        .pri = 266240,
        .alpha = 128,
    },
    {
        .tex0 = 2307547416003424241,
        .u = 1,
        .v = 193,
        .w = 190,
        .h = 22,
        .x = 0,
        .y = 0,
        .pri = 266240,
        .alpha = 128,
    },
    {
        .tex0 = 2307525149819219855,
        .u = 1,
        .v = 1,
        .w = 30,
        .h = 62,
        .x = 248,
        .y = 76,
        .pri = 245760,
        .alpha = 128,
    },
    {
        .tex0 = 2307525149819219855,
        .u = 1,
        .v = 1,
        .w = 30,
        .h = 62,
        .x = 361,
        .y = 76,
        .pri = 245760,
        .alpha = 128,
    },
    {
        .tex0 = 2307525149819219855,
        .u = 32,
        .v = 1,
        .w = 80,
        .h = 62,
        .x = 265,
        .y = 74,
        .pri = 245760,
        .alpha = 128,
    },
    {
        .tex0 = 2307525149819219855,
        .u = 113,
        .v = 1,
        .w = 46,
        .h = 62,
        .x = 330,
        .y = 80,
        .pri = 245760,
        .alpha = 128,
    },
    {
        .tex0 = 2307525149819219855,
        .u = 161,
        .v = 1,
        .w = 46,
        .h = 62,
        .x = 330,
        .y = 80,
        .pri = 245760,
        .alpha = 128,
    },
    {
        .tex0 = 2307525149819219855,
        .u = 209,
        .v = 1,
        .w = 46,
        .h = 62,
        .x = 330,
        .y = 80,
        .pri = 245760,
        .alpha = 128,
    },
    {
        .tex0 = 2307525149819219855,
        .u = 1,
        .v = 65,
        .w = 46,
        .h = 62,
        .x = 330,
        .y = 82,
        .pri = 245760,
        .alpha = 128,
    },
    {
        .tex0 = 2307525149819219855,
        .u = 49,
        .v = 65,
        .w = 46,
        .h = 62,
        .x = 330,
        .y = 82,
        .pri = 245760,
        .alpha = 128,
    },
    {
        .tex0 = 2307525149819219855,
        .u = 97,
        .v = 65,
        .w = 46,
        .h = 62,
        .x = 330,
        .y = 80,
        .pri = 245760,
        .alpha = 128,
    },
    {
        .tex0 = 2307525149819219855,
        .u = 145,
        .v = 65,
        .w = 46,
        .h = 62,
        .x = 330,
        .y = 80,
        .pri = 245760,
        .alpha = 128,
    },
    {
        .tex0 = 2307525149819219855,
        .u = 193,
        .v = 65,
        .w = 46,
        .h = 62,
        .x = 330,
        .y = 82,
        .pri = 245760,
        .alpha = 128,
    },
    {
        .tex0 = 2307203267723044489,
        .u = 65,
        .v = 1,
        .w = 62,
        .h = 95,
        .x = 0,
        .y = 0,
        .pri = 61440,
        .alpha = 128,
    },
    {
        .tex0 = 2307203267723044489,
        .u = 65,
        .v = 98,
        .w = 10,
        .h = 19,
        .x = 62,
        .y = 0,
        .pri = 61440,
        .alpha = 128,
    },
    {
        .tex0 = 2307203267723044489,
        .u = 77,
        .v = 98,
        .w = 10,
        .h = 19,
        .x = 62,
        .y = 19,
        .pri = 61440,
        .alpha = 128,
    },
    {
        .tex0 = 2307203267723044489,
        .u = 89,
        .v = 98,
        .w = 10,
        .h = 19,
        .x = 62,
        .y = 38,
        .pri = 61440,
        .alpha = 128,
    },
    {
        .tex0 = 2307203267723044489,
        .u = 101,
        .v = 98,
        .w = 10,
        .h = 19,
        .x = 62,
        .y = 57,
        .pri = 61440,
        .alpha = 128,
    },
    {
        .tex0 = 2307203267723044489,
        .u = 113,
        .v = 98,
        .w = 10,
        .h = 19,
        .x = 62,
        .y = 76,
        .pri = 61440,
        .alpha = 128,
    },
    {
        .tex0 = 2307203267723044489,
        .u = 1,
        .v = 1,
        .w = 62,
        .h = 105,
        .x = 0,
        .y = 0,
        .pri = 61440,
        .alpha = 128,
    },
    {
        .tex0 = 2307198731096729216,
        .u = 1,
        .v = 1,
        .w = 22,
        .h = 45,
        .x = 0,
        .y = 0,
        .pri = 40960,
        .alpha = 128,
    },
    {
        .tex0 = 2307198731096729216,
        .u = 25,
        .v = 1,
        .w = 22,
        .h = 45,
        .x = 0,
        .y = 45,
        .pri = 40960,
        .alpha = 128,
    },
    {
        .tex0 = 2307198731096729216,
        .u = 1,
        .v = 1,
        .w = 22,
        .h = 45,
        .x = 338,
        .y = 0,
        .pri = 40960,
        .alpha = 128,
    },
    {
        .tex0 = 2307198731096729216,
        .u = 25,
        .v = 1,
        .w = 22,
        .h = 45,
        .x = 338,
        .y = 45,
        .pri = 40960,
        .alpha = 128,
    },
    {
        .tex0 = 2307198731096729216,
        .u = 1,
        .v = 48,
        .w = 46,
        .h = 7,
        .x = 157,
        .y = 0,
        .pri = 40960,
        .alpha = 128,
    },
    {
        .tex0 = 2307198731096729216,
        .u = 1,
        .v = 48,
        .w = 46,
        .h = 7,
        .x = 157,
        .y = 83,
        .pri = 40960,
        .alpha = 128,
    },
    {
        .tex0 = 2307203952703219370,
        .u = 1,
        .v = 11,
        .w = 3,
        .h = 20,
        .x = 0,
        .y = 3,
        .pri = 81920,
        .alpha = 128,
    },
    {
        .tex0 = 2307203952703219370,
        .u = 6,
        .v = 11,
        .w = 3,
        .h = 20,
        .x = 0,
        .y = 23,
        .pri = 81920,
        .alpha = 128,
    },
    {
        .tex0 = 2307203952703219370,
        .u = 11,
        .v = 11,
        .w = 3,
        .h = 20,
        .x = 0,
        .y = 43,
        .pri = 81920,
        .alpha = 128,
    },
    {
        .tex0 = 2307203952703219370,
        .u = 16,
        .v = 11,
        .w = 3,
        .h = 20,
        .x = 91,
        .y = 3,
        .pri = 81920,
        .alpha = 128,
    },
    {
        .tex0 = 2307203952703219370,
        .u = 21,
        .v = 11,
        .w = 3,
        .h = 20,
        .x = 91,
        .y = 23,
        .pri = 81920,
        .alpha = 128,
    },
    {
        .tex0 = 2307203952703219370,
        .u = 26,
        .v = 11,
        .w = 3,
        .h = 20,
        .x = 91,
        .y = 43,
        .pri = 81920,
        .alpha = 128,
    },
    {
        .tex0 = 2307203952703219370,
        .u = 1,
        .v = 1,
        .w = 62,
        .h = 3,
        .x = 0,
        .y = 0,
        .pri = 81920,
        .alpha = 128,
    },
    {
        .tex0 = 2307203952703219370,
        .u = 31,
        .v = 11,
        .w = 32,
        .h = 3,
        .x = 62,
        .y = 0,
        .pri = 81920,
        .alpha = 128,
    },
    {
        .tex0 = 2307203952703219370,
        .u = 1,
        .v = 6,
        .w = 62,
        .h = 3,
        .x = 0,
        .y = 63,
        .pri = 81920,
        .alpha = 128,
    },
    {
        .tex0 = 2307203952703219370,
        .u = 31,
        .v = 16,
        .w = 32,
        .h = 3,
        .x = 62,
        .y = 63,
        .pri = 81920,
        .alpha = 128,
    },
    {
        .tex0 = 2307204639897986735,
        .u = 1,
        .v = 11,
        .w = 3,
        .h = 20,
        .x = 0,
        .y = 3,
        .pri = 77824,
        .alpha = 128,
    },
    {
        .tex0 = 2307204639897986735,
        .u = 6,
        .v = 11,
        .w = 3,
        .h = 20,
        .x = 0,
        .y = 23,
        .pri = 77824,
        .alpha = 128,
    },
    {
        .tex0 = 2307204639897986735,
        .u = 11,
        .v = 11,
        .w = 3,
        .h = 20,
        .x = 0,
        .y = 43,
        .pri = 77824,
        .alpha = 128,
    },
    {
        .tex0 = 2307204639897986735,
        .u = 16,
        .v = 11,
        .w = 3,
        .h = 20,
        .x = 91,
        .y = 3,
        .pri = 77824,
        .alpha = 128,
    },
    {
        .tex0 = 2307204639897986735,
        .u = 21,
        .v = 11,
        .w = 3,
        .h = 20,
        .x = 91,
        .y = 23,
        .pri = 77824,
        .alpha = 128,
    },
    {
        .tex0 = 2307204639897986735,
        .u = 26,
        .v = 11,
        .w = 3,
        .h = 20,
        .x = 91,
        .y = 43,
        .pri = 77824,
        .alpha = 128,
    },
    {
        .tex0 = 2307204639897986735,
        .u = 1,
        .v = 1,
        .w = 62,
        .h = 3,
        .x = 0,
        .y = 0,
        .pri = 77824,
        .alpha = 128,
    },
    {
        .tex0 = 2307204639897986735,
        .u = 31,
        .v = 11,
        .w = 32,
        .h = 3,
        .x = 62,
        .y = 0,
        .pri = 77824,
        .alpha = 128,
    },
    {
        .tex0 = 2307204639897986735,
        .u = 1,
        .v = 6,
        .w = 62,
        .h = 3,
        .x = 0,
        .y = 63,
        .pri = 77824,
        .alpha = 128,
    },
    {
        .tex0 = 2307204639897986735,
        .u = 31,
        .v = 16,
        .w = 32,
        .h = 3,
        .x = 62,
        .y = 63,
        .pri = 77824,
        .alpha = 128,
    },
    {
        .tex0 = 2307239965063456436,
        .u = 1,
        .v = 1,
        .w = 144,
        .h = 128,
        .x = 0,
        .y = 0,
        .pri = 40960,
        .alpha = 128,
    },
    {
        .tex0 = 2307239965063456436,
        .u = 147,
        .v = 1,
        .w = 32,
        .h = 32,
        .x = 0,
        .y = 0,
        .pri = 102400,
        .alpha = 128,
    },
    {
        .tex0 = 2307239965063456436,
        .u = 179,
        .v = 1,
        .w = 32,
        .h = 32,
        .x = 0,
        .y = 0,
        .pri = 102400,
        .alpha = 128,
    },
    {
        .tex0 = 2307239965063456436,
        .u = 1,
        .v = 165,
        .w = 116,
        .h = 30,
        .x = 0,
        .y = 0,
        .pri = 61440,
        .alpha = 128,
    },
    {
        .tex0 = 2307239965063456436,
        .u = 1,
        .v = 195,
        .w = 116,
        .h = 30,
        .x = 0,
        .y = 0,
        .pri = 61440,
        .alpha = 128,
    },
    {
        .tex0 = 2307239965063456436,
        .u = 1,
        .v = 225,
        .w = 116,
        .h = 30,
        .x = 0,
        .y = 0,
        .pri = 61440,
        .alpha = 128,
    },
    {
        .tex0 = 2307239965063456436,
        .u = 119,
        .v = 207,
        .w = 48,
        .h = 24,
        .x = 0,
        .y = 0,
        .pri = 40960,
        .alpha = 128,
    },
    {
        .tex0 = 2307239965063456436,
        .u = 219,
        .v = 231,
        .w = 20,
        .h = 24,
        .x = 0,
        .y = 0,
        .pri = 61440,
        .alpha = 128,
    },
    {
        .tex0 = 2307239965063456436,
        .u = 167,
        .v = 207,
        .w = 20,
        .h = 24,
        .x = 0,
        .y = 0,
        .pri = 61440,
        .alpha = 128,
    },
    {
        .tex0 = 2307239965063456436,
        .u = 187,
        .v = 207,
        .w = 20,
        .h = 24,
        .x = 0,
        .y = 0,
        .pri = 61440,
        .alpha = 128,
    },
    {
        .tex0 = 2307239965063456436,
        .u = 207,
        .v = 207,
        .w = 20,
        .h = 24,
        .x = 0,
        .y = 0,
        .pri = 61440,
        .alpha = 128,
    },
    {
        .tex0 = 2307239965063456436,
        .u = 227,
        .v = 207,
        .w = 20,
        .h = 24,
        .x = 0,
        .y = 0,
        .pri = 61440,
        .alpha = 128,
    },
    {
        .tex0 = 2307239965063456436,
        .u = 119,
        .v = 231,
        .w = 20,
        .h = 24,
        .x = 0,
        .y = 0,
        .pri = 61440,
        .alpha = 128,
    },
    {
        .tex0 = 2307239965063456436,
        .u = 139,
        .v = 231,
        .w = 20,
        .h = 24,
        .x = 0,
        .y = 0,
        .pri = 61440,
        .alpha = 128,
    },
    {
        .tex0 = 2307239965063456436,
        .u = 159,
        .v = 231,
        .w = 20,
        .h = 24,
        .x = 0,
        .y = 0,
        .pri = 61440,
        .alpha = 128,
    },
    {
        .tex0 = 2307239965063456436,
        .u = 179,
        .v = 231,
        .w = 20,
        .h = 24,
        .x = 0,
        .y = 0,
        .pri = 61440,
        .alpha = 128,
    },
    {
        .tex0 = 2307239965063456436,
        .u = 199,
        .v = 231,
        .w = 20,
        .h = 24,
        .x = 0,
        .y = 0,
        .pri = 61440,
        .alpha = 128,
    },
    {
        .tex0 = 2307075172336706307,
        .u = 0,
        .v = 0,
        .w = 14,
        .h = 16,
        .x = 593,
        .y = 30,
        .pri = 81920,
        .alpha = 128,
    },
    {
        .tex0 = 2307075172336706307,
        .u = 0,
        .v = 16,
        .w = 14,
        .h = 16,
        .x = 593,
        .y = 310,
        .pri = 81920,
        .alpha = 128,
    },
    {
        .tex0 = 2307075172336706307,
        .u = 0,
        .v = 0,
        .w = 14,
        .h = 10,
        .x = 593,
        .y = 173,
        .pri = 81920,
        .alpha = 128,
    },
    {
        .tex0 = 2307075172336706307,
        .u = 14,
        .v = 0,
        .w = 14,
        .h = 12,
        .x = 594,
        .y = 31,
        .pri = 61440,
        .alpha = 128,
    },
    {
        .tex0 = 2307075172336706307,
        .u = 14,
        .v = 14,
        .w = 14,
        .h = 12,
        .x = 594,
        .y = 315,
        .pri = 61440,
        .alpha = 128,
    },
    {
        .tex0 = 2307074760019845888,
        .u = 0,
        .v = 0,
        .w = 14,
        .h = 16,
        .x = 593,
        .y = 37,
        .pri = 61440,
        .alpha = 128,
    },
    {
        .tex0 = 2307074760019845888,
        .u = 0,
        .v = 16,
        .w = 14,
        .h = 16,
        .x = 593,
        .y = 53,
        .pri = 61440,
        .alpha = 128,
    },
    {
        .tex0 = 2307074760019845888,
        .u = 14,
        .v = 1,
        .w = 14,
        .h = 15,
        .x = 593,
        .y = 53,
        .pri = 61440,
        .alpha = 128,
    },
    {
        .tex0 = 2307364759633209920,
        .u = 1,
        .v = 1,
        .w = 88,
        .h = 60,
        .x = 0,
        .y = 0,
        .pri = 102400,
        .alpha = 128,
    },
    {
        .tex0 = 2307364759633209920,
        .u = 91,
        .v = 1,
        .w = 88,
        .h = 60,
        .x = 0,
        .y = 0,
        .pri = 102400,
        .alpha = 128,
    },
    {
        .tex0 = 2307364759633209920,
        .u = 1,
        .v = 63,
        .w = 88,
        .h = 60,
        .x = 0,
        .y = 0,
        .pri = 102400,
        .alpha = 128,
    },
    {
        .tex0 = 2307364759633209920,
        .u = 91,
        .v = 63,
        .w = 88,
        .h = 60,
        .x = 0,
        .y = 0,
        .pri = 102400,
        .alpha = 128,
    },
    {
        .tex0 = 2307364759633209920,
        .u = 1,
        .v = 125,
        .w = 88,
        .h = 60,
        .x = 0,
        .y = 0,
        .pri = 102400,
        .alpha = 128,
    },
    {
        .tex0 = 2307364759633209920,
        .u = 91,
        .v = 125,
        .w = 88,
        .h = 60,
        .x = 0,
        .y = 0,
        .pri = 102400,
        .alpha = 128,
    },
    {
        .tex0 = 2307364759633209920,
        .u = 1,
        .v = 187,
        .w = 88,
        .h = 60,
        .x = 0,
        .y = 0,
        .pri = 102400,
        .alpha = 128,
    },
    {
        .tex0 = 2307364759633209920,
        .u = 91,
        .v = 187,
        .w = 88,
        .h = 60,
        .x = 0,
        .y = 0,
        .pri = 102400,
        .alpha = 128,
    },
    {
        .tex0 = 2307382900501326660,
        .u = 1,
        .v = 1,
        .w = 88,
        .h = 60,
        .x = 0,
        .y = 0,
        .pri = 102400,
        .alpha = 128,
    },
    {
        .tex0 = 2307382900501326660,
        .u = 91,
        .v = 1,
        .w = 88,
        .h = 60,
        .x = 0,
        .y = 0,
        .pri = 102400,
        .alpha = 128,
    },
    {
        .tex0 = 2307382900501326660,
        .u = 1,
        .v = 63,
        .w = 88,
        .h = 60,
        .x = 0,
        .y = 0,
        .pri = 102400,
        .alpha = 128,
    },
    {
        .tex0 = 2307382900501326660,
        .u = 91,
        .v = 63,
        .w = 88,
        .h = 60,
        .x = 0,
        .y = 0,
        .pri = 102400,
        .alpha = 128,
    },
    {
        .tex0 = 2307285593789442048,
        .u = 1,
        .v = 1,
        .w = 124,
        .h = 38,
        .x = 37,
        .y = 247,
        .pri = 40960,
        .alpha = 128,
    },
    {
        .tex0 = 2307285593789442048,
        .u = 125,
        .v = 1,
        .w = 124,
        .h = 38,
        .x = 183,
        .y = 247,
        .pri = 40960,
        .alpha = 128,
    },
    {
        .tex0 = 2307285593789442048,
        .u = 249,
        .v = 1,
        .w = 124,
        .h = 38,
        .x = 332,
        .y = 247,
        .pri = 40960,
        .alpha = 128,
    },
    {
        .tex0 = 2307285593789442048,
        .u = 373,
        .v = 1,
        .w = 124,
        .h = 38,
        .x = 479,
        .y = 247,
        .pri = 40960,
        .alpha = 128,
    },
    {
        .tex0 = 2307285593789442048,
        .u = 1,
        .v = 41,
        .w = 350,
        .h = 58,
        .x = 144,
        .y = 34,
        .pri = 40960,
        .alpha = 128,
    },
    {
        .tex0 = 2307285593789442048,
        .u = 419,
        .v = 82,
        .w = 45,
        .h = 45,
        .x = 0,
        .y = 103,
        .pri = 40960,
        .alpha = 128,
    },
    {
        .tex0 = 2307285593789442048,
        .u = 466,
        .v = 82,
        .w = 45,
        .h = 45,
        .x = 110,
        .y = 103,
        .pri = 40960,
        .alpha = 128,
    },
    {
        .tex0 = 2307285593789442048,
        .u = 419,
        .v = 82,
        .w = 45,
        .h = 45,
        .x = 0,
        .y = 213,
        .pri = 40960,
        .alpha = 128,
    },
    {
        .tex0 = 2307285593789442048,
        .u = 466,
        .v = 82,
        .w = 45,
        .h = 45,
        .x = 110,
        .y = 213,
        .pri = 40960,
        .alpha = 128,
    },
    {
        .tex0 = 2307321328923912452,
        .u = 1,
        .v = 1,
        .w = 72,
        .h = 95,
        .x = 63,
        .y = 131,
        .pri = 61440,
        .alpha = 128,
    },
    {
        .tex0 = 2307321328923912452,
        .u = 75,
        .v = 1,
        .w = 128,
        .h = 128,
        .x = 180,
        .y = 117,
        .pri = 61440,
        .alpha = 128,
    },
    {
        .tex0 = 2307321328923912452,
        .u = 1,
        .v = 127,
        .w = 128,
        .h = 128,
        .x = 330,
        .y = 117,
        .pri = 61440,
        .alpha = 128,
    },
    {
        .tex0 = 2307321328923912452,
        .u = 131,
        .v = 131,
        .w = 104,
        .h = 90,
        .x = 484,
        .y = 137,
        .pri = 61440,
        .alpha = 128,
    },
    {
        .tex0 = 2307461516656454912,
        .u = 1,
        .v = 1,
        .w = 254,
        .h = 129,
        .x = 0,
        .y = 0,
        .pri = 204800,
        .alpha = 128,
    },
    {
        .tex0 = 2307461516656454912,
        .u = 1,
        .v = 1,
        .w = 254,
        .h = 129,
        .x = 0,
        .y = 0,
        .pri = 204800,
        .alpha = 128,
    },
    {
        .tex0 = 2307461516656454912,
        .u = 1,
        .v = 1,
        .w = 254,
        .h = 129,
        .x = 0,
        .y = 0,
        .pri = 204800,
        .alpha = 128,
    },
    {
        .tex0 = 2307461516656454912,
        .u = 1,
        .v = 1,
        .w = 254,
        .h = 129,
        .x = 0,
        .y = 0,
        .pri = 204800,
        .alpha = 128,
    },
    {
        .tex0 = 2307461516656454912,
        .u = 1,
        .v = 1,
        .w = 254,
        .h = 129,
        .x = 0,
        .y = 0,
        .pri = 204800,
        .alpha = 128,
    },
    {
        .tex0 = 2307461516656454912,
        .u = 1,
        .v = 1,
        .w = 254,
        .h = 129,
        .x = 0,
        .y = 0,
        .pri = 204800,
        .alpha = 128,
    },
    {
        .tex0 = 2307461516656454912,
        .u = 1,
        .v = 132,
        .w = 254,
        .h = 120,
        .x = 0,
        .y = 0,
        .pri = 204800,
        .alpha = 128,
    },
    {
        .tex0 = 2307461516656454912,
        .u = 1,
        .v = 132,
        .w = 254,
        .h = 120,
        .x = 0,
        .y = 0,
        .pri = 204800,
        .alpha = 128,
    },
    {
        .tex0 = 2307461516656454912,
        .u = 1,
        .v = 132,
        .w = 254,
        .h = 120,
        .x = 0,
        .y = 0,
        .pri = 204800,
        .alpha = 128,
    },
    {
        .tex0 = 2307461516656454912,
        .u = 1,
        .v = 132,
        .w = 254,
        .h = 120,
        .x = 0,
        .y = 0,
        .pri = 204800,
        .alpha = 128,
    },
    {
        .tex0 = 2307461516656454912,
        .u = 1,
        .v = 132,
        .w = 254,
        .h = 120,
        .x = 0,
        .y = 0,
        .pri = 204800,
        .alpha = 128,
    },
    {
        .tex0 = 2307461516656454912,
        .u = 1,
        .v = 132,
        .w = 254,
        .h = 120,
        .x = 0,
        .y = 0,
        .pri = 204800,
        .alpha = 128,
    },
    {
        .tex0 = 2307417537332259008,
        .u = 1,
        .v = 1,
        .w = 510,
        .h = 260,
        .x = 0,
        .y = 0,
        .pri = 204800,
        .alpha = 128,
    },
    {
        .tex0 = 2307417537332259008,
        .u = 1,
        .v = 1,
        .w = 510,
        .h = 260,
        .x = 0,
        .y = 0,
        .pri = 204800,
        .alpha = 128,
    },
    {
        .tex0 = 2307417537332259008,
        .u = 1,
        .v = 1,
        .w = 510,
        .h = 260,
        .x = 0,
        .y = 0,
        .pri = 204800,
        .alpha = 128,
    },
    {
        .tex0 = 2307417537332259008,
        .u = 1,
        .v = 1,
        .w = 510,
        .h = 260,
        .x = 0,
        .y = 0,
        .pri = 204800,
        .alpha = 128,
    },
    {
        .tex0 = 2307417537332259008,
        .u = 1,
        .v = 1,
        .w = 510,
        .h = 260,
        .x = 0,
        .y = 0,
        .pri = 204800,
        .alpha = 128,
    },
    {
        .tex0 = 2307417537332259008,
        .u = 1,
        .v = 1,
        .w = 510,
        .h = 260,
        .x = 0,
        .y = 0,
        .pri = 204800,
        .alpha = 128,
    },
    {
        .tex0 = 2307417537332259008,
        .u = 1,
        .v = 263,
        .w = 510,
        .h = 242,
        .x = 0,
        .y = 0,
        .pri = 204800,
        .alpha = 128,
    },
    {
        .tex0 = 2307417537332259008,
        .u = 1,
        .v = 263,
        .w = 510,
        .h = 242,
        .x = 0,
        .y = 0,
        .pri = 204800,
        .alpha = 128,
    },
    {
        .tex0 = 2307417537332259008,
        .u = 1,
        .v = 263,
        .w = 510,
        .h = 242,
        .x = 0,
        .y = 0,
        .pri = 204800,
        .alpha = 128,
    },
    {
        .tex0 = 2307417537332259008,
        .u = 1,
        .v = 263,
        .w = 510,
        .h = 242,
        .x = 0,
        .y = 0,
        .pri = 204800,
        .alpha = 128,
    },
    {
        .tex0 = 2307417537332259008,
        .u = 1,
        .v = 263,
        .w = 510,
        .h = 242,
        .x = 0,
        .y = 0,
        .pri = 204800,
        .alpha = 128,
    },
    {
        .tex0 = 2307417537332259008,
        .u = 1,
        .v = 263,
        .w = 510,
        .h = 242,
        .x = 0,
        .y = 0,
        .pri = 204800,
        .alpha = 128,
    },
    {
        .tex0 = 2307399944072473152,
        .u = 337,
        .v = 119,
        .w = 166,
        .h = 116,
        .x = 0,
        .y = 0,
        .pri = 81920,
        .alpha = 128,
    },
    {
        .tex0 = 2307399944072473152,
        .u = 337,
        .v = 1,
        .w = 166,
        .h = 116,
        .x = 0,
        .y = 0,
        .pri = 81920,
        .alpha = 128,
    },
    {
        .tex0 = 2307399944072473152,
        .u = 169,
        .v = 1,
        .w = 166,
        .h = 116,
        .x = 0,
        .y = 0,
        .pri = 81920,
        .alpha = 128,
    },
    {
        .tex0 = 2307399944072473152,
        .u = 169,
        .v = 119,
        .w = 166,
        .h = 116,
        .x = 0,
        .y = 0,
        .pri = 81920,
        .alpha = 128,
    },
    {
        .tex0 = 2307399944072473152,
        .u = 1,
        .v = 119,
        .w = 166,
        .h = 116,
        .x = 0,
        .y = 0,
        .pri = 81920,
        .alpha = 128,
    },
    {
        .tex0 = 2307399944072473152,
        .u = 1,
        .v = 1,
        .w = 166,
        .h = 116,
        .x = 0,
        .y = 0,
        .pri = 81920,
        .alpha = 128,
    },
    {
        .tex0 = 2307427017331649733,
        .u = 69,
        .v = 1,
        .w = 148,
        .h = 50,
        .x = 217,
        .y = 35,
        .pri = 40960,
        .alpha = 128,
    },
    {
        .tex0 = 2307427017331649733,
        .u = 69,
        .v = 1,
        .w = 148,
        .h = 50,
        .x = 429,
        .y = 34,
        .pri = 40960,
        .alpha = 128,
    },
    {
        .tex0 = 2307436364255276297,
        .u = 1,
        .v = 1,
        .w = 17,
        .h = 24,
        .x = 0,
        .y = 0,
        .pri = 40960,
        .alpha = 128,
    },
    {
        .tex0 = 2307436364255276297,
        .u = 20,
        .v = 1,
        .w = 17,
        .h = 24,
        .x = 0,
        .y = 0,
        .pri = 40960,
        .alpha = 128,
    },
    {
        .tex0 = 2307436364255276297,
        .u = 39,
        .v = 1,
        .w = 17,
        .h = 24,
        .x = 0,
        .y = 0,
        .pri = 40960,
        .alpha = 128,
    },
    {
        .tex0 = 2307436364255276297,
        .u = 58,
        .v = 1,
        .w = 17,
        .h = 24,
        .x = 0,
        .y = 0,
        .pri = 40960,
        .alpha = 128,
    },
    {
        .tex0 = 2307436364255276297,
        .u = 77,
        .v = 1,
        .w = 17,
        .h = 24,
        .x = 0,
        .y = 0,
        .pri = 40960,
        .alpha = 128,
    },
    {
        .tex0 = 2307436364255276297,
        .u = 96,
        .v = 1,
        .w = 17,
        .h = 24,
        .x = 0,
        .y = 0,
        .pri = 40960,
        .alpha = 128,
    },
    {
        .tex0 = 2307436364255276297,
        .u = 115,
        .v = 1,
        .w = 17,
        .h = 24,
        .x = 0,
        .y = 0,
        .pri = 40960,
        .alpha = 128,
    },
    {
        .tex0 = 2307436364255276297,
        .u = 134,
        .v = 1,
        .w = 17,
        .h = 24,
        .x = 0,
        .y = 0,
        .pri = 40960,
        .alpha = 128,
    },
    {
        .tex0 = 2307436364255276297,
        .u = 153,
        .v = 1,
        .w = 17,
        .h = 24,
        .x = 0,
        .y = 0,
        .pri = 40960,
        .alpha = 128,
    },
    {
        .tex0 = 2307436364255276297,
        .u = 172,
        .v = 1,
        .w = 17,
        .h = 24,
        .x = 0,
        .y = 0,
        .pri = 40960,
        .alpha = 128,
    },
    {
        .tex0 = 2307436364255276297,
        .u = 1,
        .v = 27,
        .w = 17,
        .h = 24,
        .x = 0,
        .y = 0,
        .pri = 40960,
        .alpha = 128,
    },
    {
        .tex0 = 2307436364255276297,
        .u = 20,
        .v = 27,
        .w = 17,
        .h = 24,
        .x = 0,
        .y = 0,
        .pri = 40960,
        .alpha = 128,
    },
    {
        .tex0 = 2307436364255276297,
        .u = 39,
        .v = 27,
        .w = 17,
        .h = 24,
        .x = 0,
        .y = 0,
        .pri = 40960,
        .alpha = 128,
    },
    {
        .tex0 = 2307436364255276297,
        .u = 58,
        .v = 27,
        .w = 17,
        .h = 24,
        .x = 0,
        .y = 0,
        .pri = 40960,
        .alpha = 128,
    },
    {
        .tex0 = 2307436364255276297,
        .u = 77,
        .v = 27,
        .w = 17,
        .h = 24,
        .x = 0,
        .y = 0,
        .pri = 40960,
        .alpha = 128,
    },
    {
        .tex0 = 2307436364255276297,
        .u = 96,
        .v = 27,
        .w = 17,
        .h = 24,
        .x = 0,
        .y = 0,
        .pri = 40960,
        .alpha = 128,
    },
    {
        .tex0 = 2307436364255276297,
        .u = 115,
        .v = 27,
        .w = 17,
        .h = 24,
        .x = 0,
        .y = 0,
        .pri = 40960,
        .alpha = 128,
    },
    {
        .tex0 = 2307436364255276297,
        .u = 134,
        .v = 27,
        .w = 17,
        .h = 24,
        .x = 0,
        .y = 0,
        .pri = 40960,
        .alpha = 128,
    },
    {
        .tex0 = 2307436364255276297,
        .u = 153,
        .v = 27,
        .w = 17,
        .h = 24,
        .x = 0,
        .y = 0,
        .pri = 40960,
        .alpha = 128,
    },
    {
        .tex0 = 2307436364255276297,
        .u = 172,
        .v = 27,
        .w = 17,
        .h = 24,
        .x = 0,
        .y = 0,
        .pri = 40960,
        .alpha = 128,
    },
    {
        .tex0 = 2307436364255276297,
        .u = 191,
        .v = 27,
        .w = 17,
        .h = 24,
        .x = 480,
        .y = 45,
        .pri = 40960,
        .alpha = 128,
    },
    {
        .tex0 = 2307436364255276297,
        .u = 1,
        .v = 53,
        .w = 53,
        .h = 21,
        .x = 253,
        .y = 50,
        .pri = 40960,
        .alpha = 128,
    },
    {
        .tex0 = 2307436364255276297,
        .u = 56,
        .v = 53,
        .w = 75,
        .h = 21,
        .x = 241,
        .y = 47,
        .pri = 40960,
        .alpha = 128,
    },
    {
        .tex0 = 2307436364255276297,
        .u = 133,
        .v = 53,
        .w = 23,
        .h = 21,
        .x = 0,
        .y = 0,
        .pri = 40960,
        .alpha = 128,
    },
    {
        .tex0 = 2307436364255276297,
        .u = 165,
        .v = 51,
        .w = 90,
        .h = 30,
        .x = 251,
        .y = 45,
        .pri = 40960,
        .alpha = 128,
    },
    {
        .tex0 = 2307436364255276297,
        .u = 109,
        .v = 76,
        .w = 26,
        .h = 26,
        .x = 517,
        .y = 47,
        .pri = 40960,
        .alpha = 128,
    },
    {
        .tex0 = 2307436364255276297,
        .u = 1,
        .v = 82,
        .w = 196,
        .h = 32,
        .x = 295,
        .y = 44,
        .pri = 40960,
        .alpha = 128,
    },
    {
        .tex0 = 2307427017331649733,
        .u = 1,
        .v = 1,
        .w = 66,
        .h = 50,
        .x = 364,
        .y = 35,
        .pri = 40960,
        .alpha = 128,
    },
    {
        .tex0 = 2307567070913637632,
        .u = 1,
        .v = 1,
        .w = 254,
        .h = 356,
        .x = 33,
        .y = 46,
        .pri = 491520,
        .alpha = 128,
    },
    {
        .tex0 = 2307567070913637632,
        .u = 257,
        .v = 1,
        .w = 254,
        .h = 356,
        .x = 353,
        .y = 46,
        .pri = 491520,
        .alpha = 128,
    },
    {
        .tex0 = 2307215226119005440,
        .u = 1,
        .v = 1,
        .w = 15,
        .h = 24,
        .x = 0,
        .y = 0,
        .pri = 163840,
        .alpha = 128,
    },
    {
        .tex0 = 2307215226119005440,
        .u = 16,
        .v = 1,
        .w = 15,
        .h = 24,
        .x = 0,
        .y = 0,
        .pri = 163840,
        .alpha = 128,
    },
    {
        .tex0 = 2307215226119005440,
        .u = 31,
        .v = 1,
        .w = 15,
        .h = 24,
        .x = 0,
        .y = 0,
        .pri = 163840,
        .alpha = 128,
    },
    {
        .tex0 = 2307215226119005440,
        .u = 46,
        .v = 1,
        .w = 15,
        .h = 24,
        .x = 0,
        .y = 0,
        .pri = 163840,
        .alpha = 128,
    },
    {
        .tex0 = 2307215226119005440,
        .u = 61,
        .v = 1,
        .w = 15,
        .h = 24,
        .x = 0,
        .y = 0,
        .pri = 163840,
        .alpha = 128,
    },
    {
        .tex0 = 2307215226119005440,
        .u = 76,
        .v = 1,
        .w = 15,
        .h = 24,
        .x = 0,
        .y = 0,
        .pri = 163840,
        .alpha = 128,
    },
    {
        .tex0 = 2307215226119005440,
        .u = 91,
        .v = 1,
        .w = 15,
        .h = 24,
        .x = 0,
        .y = 0,
        .pri = 163840,
        .alpha = 128,
    },
    {
        .tex0 = 2307215226119005440,
        .u = 106,
        .v = 1,
        .w = 15,
        .h = 24,
        .x = 0,
        .y = 0,
        .pri = 163840,
        .alpha = 128,
    },
    {
        .tex0 = 2307215226119005440,
        .u = 121,
        .v = 1,
        .w = 15,
        .h = 24,
        .x = 0,
        .y = 0,
        .pri = 163840,
        .alpha = 128,
    },
    {
        .tex0 = 2307215226119005440,
        .u = 136,
        .v = 1,
        .w = 15,
        .h = 24,
        .x = 0,
        .y = 0,
        .pri = 163840,
        .alpha = 128,
    },
    {
        .tex0 = 2307215226119005440,
        .u = 151,
        .v = 1,
        .w = 15,
        .h = 24,
        .x = 450,
        .y = 27,
        .pri = 163840,
        .alpha = 128,
    },
    {
        .tex0 = 2307215226119005440,
        .u = 166,
        .v = 1,
        .w = 15,
        .h = 24,
        .x = 0,
        .y = 0,
        .pri = 163840,
        .alpha = 128,
    },
    {
        .tex0 = 2307215226119005440,
        .u = 1,
        .v = 25,
        .w = 134,
        .h = 24,
        .x = 38,
        .y = 27,
        .pri = 163840,
        .alpha = 128,
    },
    {
        .tex0 = 2307215226119005440,
        .u = 1,
        .v = 49,
        .w = 76,
        .h = 24,
        .x = 190,
        .y = 27,
        .pri = 163840,
        .alpha = 128,
    },
    {
        .tex0 = 2307215226119005440,
        .u = 1,
        .v = 97,
        .w = 72,
        .h = 24,
        .x = 313,
        .y = 28,
        .pri = 163840,
        .alpha = 128,
    },
    {
        .tex0 = 2307215226119005440,
        .u = 1,
        .v = 73,
        .w = 24,
        .h = 24,
        .x = 492,
        .y = 27,
        .pri = 163840,
        .alpha = 128,
    },
    {
        .tex0 = 2307215226119005440,
        .u = 1,
        .v = 121,
        .w = 86,
        .h = 24,
        .x = 41,
        .y = 389,
        .pri = 163840,
        .alpha = 128,
    },
    {
        .tex0 = 2307215226119005440,
        .u = 1,
        .v = 145,
        .w = 50,
        .h = 24,
        .x = 324,
        .y = 28,
        .pri = 163840,
        .alpha = 128,
    },
    {
        .tex0 = 2307215226119005440,
        .u = 2,
        .v = 177,
        .w = 70,
        .h = 30,
        .x = 252,
        .y = 54,
        .pri = 163840,
        .alpha = 128,
    },
    {
        .tex0 = 2307215226119005440,
        .u = 2,
        .v = 209,
        .w = 70,
        .h = 30,
        .x = 339,
        .y = 60,
        .pri = 163840,
        .alpha = 128,
    },
    {
        .tex0 = 2307215226119005440,
        .u = 74,
        .v = 177,
        .w = 102,
        .h = 30,
        .x = 431,
        .y = 54,
        .pri = 163840,
        .alpha = 128,
    },
    {
        .tex0 = 2307215226119005440,
        .u = 74,
        .v = 209,
        .w = 70,
        .h = 30,
        .x = 555,
        .y = 59,
        .pri = 163840,
        .alpha = 128,
    },
    {
        .tex0 = 2307215226119005440,
        .u = 183,
        .v = 1,
        .w = 82,
        .h = 28,
        .x = 0,
        .y = 0,
        .pri = 8192,
        .alpha = 128,
    },
    {
        .tex0 = 2307215226119005440,
        .u = 183,
        .v = 29,
        .w = 82,
        .h = 28,
        .x = 0,
        .y = 30,
        .pri = 8192,
        .alpha = 128,
    },
    {
        .tex0 = 2307215226119005440,
        .u = 183,
        .v = 57,
        .w = 82,
        .h = 28,
        .x = 0,
        .y = 60,
        .pri = 8192,
        .alpha = 128,
    },
    {
        .tex0 = 2307215226119005440,
        .u = 183,
        .v = 85,
        .w = 82,
        .h = 28,
        .x = 0,
        .y = 90,
        .pri = 8192,
        .alpha = 128,
    },
    {
        .tex0 = 2307215226119005440,
        .u = 183,
        .v = 113,
        .w = 82,
        .h = 28,
        .x = 0,
        .y = 120,
        .pri = 8192,
        .alpha = 128,
    },
    {
        .tex0 = 2307215226119005440,
        .u = 183,
        .v = 141,
        .w = 82,
        .h = 28,
        .x = 0,
        .y = 150,
        .pri = 8192,
        .alpha = 128,
    },
    {
        .tex0 = 2307215226119005440,
        .u = 267,
        .v = 1,
        .w = 110,
        .h = 74,
        .x = 0,
        .y = 0,
        .pri = 61440,
        .alpha = 128,
    },
    {
        .tex0 = 2307219621884765968,
        .u = 45,
        .v = 33,
        .w = 64,
        .h = 30,
        .x = 55,
        .y = 31,
        .pri = 143360,
        .alpha = 128,
    },
    {
        .tex0 = 2307219621884765968,
        .u = 42,
        .v = 1,
        .w = 64,
        .h = 30,
        .x = 521,
        .y = 25,
        .pri = 143360,
        .alpha = 128,
    },
    {
        .tex0 = 2307215226119005440,
        .u = 267,
        .v = 77,
        .w = 30,
        .h = 22,
        .x = 0,
        .y = 0,
        .pri = 61440,
        .alpha = 128,
    },
    {
        .tex0 = 2307215226119005440,
        .u = 266,
        .v = 77,
        .w = 30,
        .h = 22,
        .x = 0,
        .y = 0,
        .pri = 61440,
        .alpha = 128,
    },
    {
        .tex0 = 2307215226119005440,
        .u = 267,
        .v = 77,
        .w = 30,
        .h = 22,
        .x = 0,
        .y = 0,
        .pri = 61440,
        .alpha = 128,
    },
    {
        .tex0 = 2307215226119005440,
        .u = 266,
        .v = 77,
        .w = 30,
        .h = 22,
        .x = 0,
        .y = 0,
        .pri = 61440,
        .alpha = 128,
    },
    {
        .tex0 = 2307216047264016132,
        .u = 1,
        .v = 1,
        .w = 20,
        .h = 6,
        .x = 309,
        .y = 71,
        .pri = 61440,
        .alpha = 128,
    },
    {
        .tex0 = 2307216047264016132,
        .u = 1,
        .v = 1,
        .w = 20,
        .h = 6,
        .x = 309,
        .y = 329,
        .pri = 61440,
        .alpha = 128,
    },
    {
        .tex0 = 2307216047264016132,
        .u = 23,
        .v = 1,
        .w = 8,
        .h = 16,
        .x = 126,
        .y = 195,
        .pri = 61440,
        .alpha = 128,
    },
    {
        .tex0 = 2307216047264016132,
        .u = 23,
        .v = 1,
        .w = 8,
        .h = 16,
        .x = 504,
        .y = 195,
        .pri = 61440,
        .alpha = 128,
    },
    {
        .tex0 = 2307216047264016132,
        .u = 8,
        .v = 9,
        .w = 12,
        .h = 4,
        .x = 202,
        .y = 101,
        .pri = 61440,
        .alpha = 128,
    },
    {
        .tex0 = 2307216047264016132,
        .u = 8,
        .v = 9,
        .w = 12,
        .h = 4,
        .x = 202,
        .y = 249,
        .pri = 61440,
        .alpha = 128,
    },
    {
        .tex0 = 2307216047264016132,
        .u = 1,
        .v = 9,
        .w = 5,
        .h = 12,
        .x = 95,
        .y = 171,
        .pri = 61440,
        .alpha = 128,
    },
    {
        .tex0 = 2307216047264016132,
        .u = 1,
        .v = 9,
        .w = 5,
        .h = 12,
        .x = 316,
        .y = 171,
        .pri = 61440,
        .alpha = 128,
    },
    {
        .tex0 = 2307217285355448071,
        .u = 1,
        .v = 1,
        .w = 52,
        .h = 36,
        .x = 0,
        .y = 0,
        .pri = 61440,
        .alpha = 128,
    },
    {
        .tex0 = 2307220858768238369,
        .u = 1,
        .v = 1,
        .w = 54,
        .h = 38,
        .x = 0,
        .y = 0,
        .pri = 40960,
        .alpha = 128,
    },
    {
        .tex0 = 2307225394319763242,
        .u = 0,
        .v = 0,
        .w = 52,
        .h = 26,
        .x = 50,
        .y = 368,
        .pri = 122880,
        .alpha = 128,
    },
    {
        .tex0 = 2307225394319763242,
        .u = 0,
        .v = 26,
        .w = 52,
        .h = 26,
        .x = 333,
        .y = 340,
        .pri = 122880,
        .alpha = 128,
    },
    {
        .tex0 = 2307225394319763242,
        .u = 64,
        .v = 0,
        .w = 52,
        .h = 26,
        .x = 333,
        .y = 368,
        .pri = 122880,
        .alpha = 128,
    },
    {
        .tex0 = 2307225394319763242,
        .u = 64,
        .v = 26,
        .w = 52,
        .h = 26,
        .x = 333,
        .y = 395,
        .pri = 122880,
        .alpha = 128,
    },
    {
        .tex0 = 2307225394319763242,
        .u = 0,
        .v = 0,
        .w = 52,
        .h = 26,
        .x = 313,
        .y = 242,
        .pri = 24576,
        .alpha = 128,
    },
    {
        .tex0 = 2307225394319763242,
        .u = 0,
        .v = 26,
        .w = 52,
        .h = 26,
        .x = 313,
        .y = 292,
        .pri = 24576,
        .alpha = 128,
    },
    {
        .tex0 = 2307225394319763242,
        .u = 64,
        .v = 0,
        .w = 52,
        .h = 26,
        .x = 314,
        .y = 321,
        .pri = 24576,
        .alpha = 128,
    },
    {
        .tex0 = 2307225394319763242,
        .u = 64,
        .v = 26,
        .w = 52,
        .h = 26,
        .x = 314,
        .y = 347,
        .pri = 24576,
        .alpha = 128,
    },
    {
        .tex0 = 2307226492691588942,
        .u = 1,
        .v = 1,
        .w = 48,
        .h = 24,
        .x = 0,
        .y = 0,
        .pri = 28672,
        .alpha = 128,
    },
    {
        .tex0 = 2307075172336706307,
        .u = 0,
        .v = 0,
        .w = 14,
        .h = 16,
        .x = 593,
        .y = 30,
        .pri = 81920,
        .alpha = 128,
    },
    {
        .tex0 = 2307075172336706307,
        .u = 0,
        .v = 16,
        .w = 14,
        .h = 16,
        .x = 593,
        .y = 310,
        .pri = 81920,
        .alpha = 128,
    },
    {
        .tex0 = 2307075172336706307,
        .u = 0,
        .v = 0,
        .w = 14,
        .h = 10,
        .x = 593,
        .y = 173,
        .pri = 81920,
        .alpha = 128,
    },
    {
        .tex0 = 2307075172336706307,
        .u = 14,
        .v = 0,
        .w = 14,
        .h = 12,
        .x = 594,
        .y = 31,
        .pri = 61440,
        .alpha = 128,
    },
    {
        .tex0 = 2307075172336706307,
        .u = 14,
        .v = 14,
        .w = 14,
        .h = 12,
        .x = 594,
        .y = 315,
        .pri = 61440,
        .alpha = 128,
    },
    {
        .tex0 = 2307074760019845888,
        .u = 0,
        .v = 0,
        .w = 14,
        .h = 16,
        .x = 593,
        .y = 37,
        .pri = 61440,
        .alpha = 128,
    },
    {
        .tex0 = 2307074760019845888,
        .u = 0,
        .v = 16,
        .w = 14,
        .h = 16,
        .x = 593,
        .y = 53,
        .pri = 61440,
        .alpha = 128,
    },
    {
        .tex0 = 2307074760019845888,
        .u = 14,
        .v = 1,
        .w = 14,
        .h = 15,
        .x = 593,
        .y = 53,
        .pri = 61440,
        .alpha = 128,
    },
    {
        .tex0 = 2307461516723629056,
        .u = 1,
        .v = 1,
        .w = 92,
        .h = 128,
        .x = 0,
        .y = 0,
        .pri = 184320,
        .alpha = 128,
    },
    {
        .tex0 = 2307461516723629056,
        .u = 95,
        .v = 1,
        .w = 202,
        .h = 136,
        .x = 0,
        .y = 0,
        .pri = 122880,
        .alpha = 128,
    },
    {
        .tex0 = 2307461516723629056,
        .u = 299,
        .v = 1,
        .w = 64,
        .h = 8,
        .x = 0,
        .y = 0,
        .pri = 163840,
        .alpha = 128,
    },
    {
        .tex0 = 2307461516723629056,
        .u = 299,
        .v = 11,
        .w = 64,
        .h = 8,
        .x = 0,
        .y = 0,
        .pri = 163840,
        .alpha = 128,
    },
    {
        .tex0 = 2307461516723629056,
        .u = 309,
        .v = 21,
        .w = 8,
        .h = 64,
        .x = 0,
        .y = 0,
        .pri = 163840,
        .alpha = 128,
    },
    {
        .tex0 = 2307461516723629056,
        .u = 299,
        .v = 21,
        .w = 8,
        .h = 64,
        .x = 0,
        .y = 0,
        .pri = 163840,
        .alpha = 128,
    },
    {
        .tex0 = 2307461516723629056,
        .u = 1,
        .v = 141,
        .w = 14,
        .h = 20,
        .x = 0,
        .y = 0,
        .pri = 20480,
        .alpha = 128,
    },
    {
        .tex0 = 2307461516723629056,
        .u = 15,
        .v = 141,
        .w = 14,
        .h = 20,
        .x = 0,
        .y = 0,
        .pri = 20480,
        .alpha = 128,
    },
    {
        .tex0 = 2307461516723629056,
        .u = 29,
        .v = 141,
        .w = 14,
        .h = 20,
        .x = 0,
        .y = 0,
        .pri = 20480,
        .alpha = 128,
    },
    {
        .tex0 = 2307461516723629056,
        .u = 43,
        .v = 141,
        .w = 14,
        .h = 20,
        .x = 0,
        .y = 0,
        .pri = 20480,
        .alpha = 128,
    },
    {
        .tex0 = 2307461516723629056,
        .u = 57,
        .v = 141,
        .w = 14,
        .h = 20,
        .x = 0,
        .y = 0,
        .pri = 20480,
        .alpha = 128,
    },
    {
        .tex0 = 2307461516723629056,
        .u = 71,
        .v = 141,
        .w = 14,
        .h = 20,
        .x = 0,
        .y = 0,
        .pri = 20480,
        .alpha = 128,
    },
    {
        .tex0 = 2307461516723629056,
        .u = 85,
        .v = 141,
        .w = 14,
        .h = 20,
        .x = 0,
        .y = 0,
        .pri = 20480,
        .alpha = 128,
    },
    {
        .tex0 = 2307461516723629056,
        .u = 99,
        .v = 141,
        .w = 14,
        .h = 30,
        .x = 0,
        .y = 0,
        .pri = 20480,
        .alpha = 128,
    },
    {
        .tex0 = 2307461516723629056,
        .u = 113,
        .v = 141,
        .w = 14,
        .h = 20,
        .x = 0,
        .y = 0,
        .pri = 20480,
        .alpha = 128,
    },
    {
        .tex0 = 2307461516723629056,
        .u = 127,
        .v = 141,
        .w = 14,
        .h = 20,
        .x = 0,
        .y = 0,
        .pri = 20480,
        .alpha = 128,
    },
    {
        .tex0 = 2307461516723629056,
        .u = 141,
        .v = 141,
        .w = 20,
        .h = 28,
        .x = 0,
        .y = 0,
        .pri = 20480,
        .alpha = 128,
    },
    {
        .tex0 = 2307461516723629056,
        .u = 161,
        .v = 141,
        .w = 20,
        .h = 28,
        .x = 0,
        .y = 0,
        .pri = 20480,
        .alpha = 128,
    },
    {
        .tex0 = 2307461516723629056,
        .u = 181,
        .v = 141,
        .w = 20,
        .h = 28,
        .x = 0,
        .y = 0,
        .pri = 20480,
        .alpha = 128,
    },
    {
        .tex0 = 2307461516723629056,
        .u = 201,
        .v = 141,
        .w = 20,
        .h = 28,
        .x = 0,
        .y = 0,
        .pri = 20480,
        .alpha = 128,
    },
    {
        .tex0 = 2307461516723629056,
        .u = 221,
        .v = 141,
        .w = 20,
        .h = 28,
        .x = 0,
        .y = 0,
        .pri = 20480,
        .alpha = 128,
    },
    {
        .tex0 = 2307461516723629056,
        .u = 141,
        .v = 169,
        .w = 20,
        .h = 28,
        .x = 0,
        .y = 0,
        .pri = 20480,
        .alpha = 128,
    },
    {
        .tex0 = 2307461516723629056,
        .u = 161,
        .v = 169,
        .w = 20,
        .h = 28,
        .x = 0,
        .y = 0,
        .pri = 20480,
        .alpha = 128,
    },
    {
        .tex0 = 2307461516723629056,
        .u = 181,
        .v = 169,
        .w = 20,
        .h = 28,
        .x = 0,
        .y = 0,
        .pri = 20480,
        .alpha = 128,
    },
    {
        .tex0 = 2307461516723629056,
        .u = 201,
        .v = 169,
        .w = 20,
        .h = 28,
        .x = 0,
        .y = 0,
        .pri = 20480,
        .alpha = 128,
    },
    {
        .tex0 = 2307461516723629056,
        .u = 221,
        .v = 169,
        .w = 20,
        .h = 28,
        .x = 0,
        .y = 0,
        .pri = 20480,
        .alpha = 128,
    },
    {
        .tex0 = 2307461516723629056,
        .u = 221,
        .v = 197,
        .w = 20,
        .h = 28,
        .x = 577,
        .y = 37,
        .pri = 163840,
        .alpha = 128,
    },
    {
        .tex0 = 2307461516723629056,
        .u = 1,
        .v = 161,
        .w = 108,
        .h = 24,
        .x = 239,
        .y = 28,
        .pri = 163840,
        .alpha = 128,
    },
    {
        .tex0 = 2307461516723629056,
        .u = 1,
        .v = 185,
        .w = 108,
        .h = 24,
        .x = 239,
        .y = 50,
        .pri = 163840,
        .alpha = 128,
    },
    {
        .tex0 = 2307461516723629056,
        .u = 1,
        .v = 209,
        .w = 108,
        .h = 32,
        .x = 412,
        .y = 36,
        .pri = 163840,
        .alpha = 128,
    },
    {
        .tex0 = 2307461516723629056,
        .u = 243,
        .v = 141,
        .w = 41,
        .h = 19,
        .x = 351,
        .y = 125,
        .pri = 163840,
        .alpha = 128,
    },
    {
        .tex0 = 2307461516723629056,
        .u = 245,
        .v = 159,
        .w = 79,
        .h = 22,
        .x = 332,
        .y = 96,
        .pri = 163840,
        .alpha = 128,
    },
    {
        .tex0 = 2307461516723629056,
        .u = 243,
        .v = 183,
        .w = 12,
        .h = 28,
        .x = 46,
        .y = 106,
        .pri = 163840,
        .alpha = 128,
    },
    {
        .tex0 = 2307464263221947908,
        .u = 1,
        .v = 33,
        .w = 126,
        .h = 30,
        .x = 491,
        .y = 39,
        .pri = 20480,
        .alpha = 128,
    },
    {
        .tex0 = 2307395544818953216,
        .u = 0,
        .v = 0,
        .w = 38,
        .h = 40,
        .x = 0,
        .y = 0,
        .pri = 143360,
        .alpha = 128,
    },
    {
        .tex0 = 2307395544818953216,
        .u = 0,
        .v = 40,
        .w = 38,
        .h = 40,
        .x = 0,
        .y = 0,
        .pri = 143360,
        .alpha = 128,
    },
    {
        .tex0 = 2307395544818953216,
        .u = 0,
        .v = 80,
        .w = 38,
        .h = 40,
        .x = 0,
        .y = 0,
        .pri = 143360,
        .alpha = 128,
    },
    {
        .tex0 = 2307395544818953216,
        .u = 38,
        .v = 0,
        .w = 38,
        .h = 40,
        .x = 0,
        .y = 0,
        .pri = 143360,
        .alpha = 128,
    },
    {
        .tex0 = 2307395544818953216,
        .u = 38,
        .v = 40,
        .w = 38,
        .h = 40,
        .x = 0,
        .y = 0,
        .pri = 143360,
        .alpha = 128,
    },
    {
        .tex0 = 2307395544818953216,
        .u = 38,
        .v = 80,
        .w = 20,
        .h = 20,
        .x = 0,
        .y = 0,
        .pri = 143360,
        .alpha = 128,
    },
    {
        .tex0 = 2307395544818953216,
        .u = 38,
        .v = 100,
        .w = 20,
        .h = 20,
        .x = 0,
        .y = 0,
        .pri = 143360,
        .alpha = 128,
    },
    {
        .tex0 = 2307400080304417825,
        .u = 0,
        .v = 0,
        .w = 32,
        .h = 32,
        .x = 0,
        .y = 0,
        .pri = 143360,
        .alpha = 128,
    },
    {
        .tex0 = 2307400080304417825,
        .u = 0,
        .v = 32,
        .w = 32,
        .h = 32,
        .x = 0,
        .y = 0,
        .pri = 143360,
        .alpha = 128,
    },
    {
        .tex0 = 2307400080304417825,
        .u = 0,
        .v = 64,
        .w = 32,
        .h = 32,
        .x = 0,
        .y = 0,
        .pri = 143360,
        .alpha = 128,
    },
    {
        .tex0 = 2307400080304417825,
        .u = 32,
        .v = 0,
        .w = 32,
        .h = 32,
        .x = 0,
        .y = 0,
        .pri = 143360,
        .alpha = 128,
    },
    {
        .tex0 = 2307400080304417825,
        .u = 32,
        .v = 32,
        .w = 32,
        .h = 32,
        .x = 0,
        .y = 0,
        .pri = 143360,
        .alpha = 128,
    },
    {
        .tex0 = 2307400080304417825,
        .u = 32,
        .v = 64,
        .w = 32,
        .h = 32,
        .x = 0,
        .y = 0,
        .pri = 143360,
        .alpha = 128,
    },
    {
        .tex0 = 2307400080304417825,
        .u = 64,
        .v = 0,
        .w = 32,
        .h = 32,
        .x = 0,
        .y = 0,
        .pri = 143360,
        .alpha = 128,
    },
    {
        .tex0 = 2307400080304417825,
        .u = 64,
        .v = 32,
        .w = 32,
        .h = 32,
        .x = 0,
        .y = 0,
        .pri = 143360,
        .alpha = 128,
    },
    {
        .tex0 = 2307400080304417825,
        .u = 64,
        .v = 64,
        .w = 32,
        .h = 32,
        .x = 0,
        .y = 0,
        .pri = 143360,
        .alpha = 128,
    },
    {
        .tex0 = 2307400080304417825,
        .u = 96,
        .v = 0,
        .w = 32,
        .h = 32,
        .x = 0,
        .y = 0,
        .pri = 143360,
        .alpha = 128,
    },
    {
        .tex0 = 2307400080304417825,
        .u = 68,
        .v = 96,
        .w = 48,
        .h = 32,
        .x = 0,
        .y = 0,
        .pri = 143360,
        .alpha = 128,
    },
    {
        .tex0 = 2307400080304417825,
        .u = 0,
        .v = 96,
        .w = 38,
        .h = 13,
        .x = 0,
        .y = 0,
        .pri = 143360,
        .alpha = 128,
    },
    {
        .tex0 = 2307435403255295042,
        .u = 1,
        .v = 1,
        .w = 110,
        .h = 134,
        .x = 24,
        .y = 70,
        .pri = 184320,
        .alpha = 128,
    },
    {
        .tex0 = 2307435403255295042,
        .u = 113,
        .v = 1,
        .w = 110,
        .h = 134,
        .x = 24,
        .y = 204,
        .pri = 184320,
        .alpha = 128,
    },
    {
        .tex0 = 2307435403255295042,
        .u = 1,
        .v = 1,
        .w = 110,
        .h = 134,
        .x = 506,
        .y = 70,
        .pri = 184320,
        .alpha = 128,
    },
    {
        .tex0 = 2307435403255295042,
        .u = 113,
        .v = 1,
        .w = 110,
        .h = 134,
        .x = 506,
        .y = 204,
        .pri = 184320,
        .alpha = 128,
    },
    {
        .tex0 = 2307435403255295042,
        .u = 1,
        .v = 137,
        .w = 80,
        .h = 80,
        .x = 25,
        .y = 91,
        .pri = 163840,
        .alpha = 128,
    },
    {
        .tex0 = 2307435403255295042,
        .u = 1,
        .v = 137,
        .w = 80,
        .h = 80,
        .x = 25,
        .y = 237,
        .pri = 163840,
        .alpha = 128,
    },
    {
        .tex0 = 2307435403255295042,
        .u = 1,
        .v = 137,
        .w = 80,
        .h = 80,
        .x = 537,
        .y = 91,
        .pri = 163840,
        .alpha = 128,
    },
    {
        .tex0 = 2307435403255295042,
        .u = 1,
        .v = 137,
        .w = 80,
        .h = 80,
        .x = 537,
        .y = 237,
        .pri = 163840,
        .alpha = 128,
    },
    {
        .tex0 = 2307444748031438150,
        .u = 0,
        .v = 0,
        .w = 34,
        .h = 34,
        .x = 0,
        .y = 0,
        .pri = 143360,
        .alpha = 128,
    },
    {
        .tex0 = 2307444748031438150,
        .u = 34,
        .v = 0,
        .w = 34,
        .h = 34,
        .x = 0,
        .y = 0,
        .pri = 143360,
        .alpha = 128,
    },
    {
        .tex0 = 2307444748031438150,
        .u = 68,
        .v = 0,
        .w = 34,
        .h = 34,
        .x = 0,
        .y = 0,
        .pri = 143360,
        .alpha = 128,
    },
    {
        .tex0 = 2307444748031438150,
        .u = 102,
        .v = 0,
        .w = 34,
        .h = 34,
        .x = 0,
        .y = 0,
        .pri = 143360,
        .alpha = 128,
    },
    {
        .tex0 = 2307444748031438150,
        .u = 136,
        .v = 0,
        .w = 34,
        .h = 34,
        .x = 0,
        .y = 0,
        .pri = 143360,
        .alpha = 128,
    },
    {
        .tex0 = 2307444748031438150,
        .u = 170,
        .v = 0,
        .w = 34,
        .h = 34,
        .x = 0,
        .y = 0,
        .pri = 143360,
        .alpha = 128,
    },
    {
        .tex0 = 2307444748031438150,
        .u = 204,
        .v = 0,
        .w = 34,
        .h = 34,
        .x = 0,
        .y = 0,
        .pri = 143360,
        .alpha = 128,
    },
    {
        .tex0 = 2307444748031438150,
        .u = 0,
        .v = 34,
        .w = 34,
        .h = 34,
        .x = 0,
        .y = 0,
        .pri = 143360,
        .alpha = 128,
    },
    {
        .tex0 = 2307444748031438150,
        .u = 34,
        .v = 34,
        .w = 34,
        .h = 34,
        .x = 0,
        .y = 0,
        .pri = 143360,
        .alpha = 128,
    },
    {
        .tex0 = 2307444748031438150,
        .u = 68,
        .v = 34,
        .w = 34,
        .h = 34,
        .x = 0,
        .y = 0,
        .pri = 143360,
        .alpha = 128,
    },
    {
        .tex0 = 2307444748031438150,
        .u = 102,
        .v = 34,
        .w = 34,
        .h = 34,
        .x = 0,
        .y = 0,
        .pri = 143360,
        .alpha = 128,
    },
    {
        .tex0 = 2307444748031438150,
        .u = 136,
        .v = 34,
        .w = 34,
        .h = 34,
        .x = 0,
        .y = 0,
        .pri = 143360,
        .alpha = 128,
    },
    {
        .tex0 = 2307444748031438150,
        .u = 170,
        .v = 34,
        .w = 34,
        .h = 34,
        .x = 0,
        .y = 0,
        .pri = 143360,
        .alpha = 128,
    },
    {
        .tex0 = 2307444748031438150,
        .u = 204,
        .v = 34,
        .w = 34,
        .h = 34,
        .x = 0,
        .y = 0,
        .pri = 143360,
        .alpha = 128,
    },
    {
        .tex0 = 2307444748031438150,
        .u = 0,
        .v = 68,
        .w = 34,
        .h = 34,
        .x = 0,
        .y = 0,
        .pri = 143360,
        .alpha = 128,
    },
    {
        .tex0 = 2307444748031438150,
        .u = 34,
        .v = 68,
        .w = 34,
        .h = 34,
        .x = 0,
        .y = 0,
        .pri = 143360,
        .alpha = 128,
    },
    {
        .tex0 = 2307444748031438150,
        .u = 68,
        .v = 68,
        .w = 34,
        .h = 34,
        .x = 0,
        .y = 0,
        .pri = 143360,
        .alpha = 128,
    },
    {
        .tex0 = 2307444748031438150,
        .u = 102,
        .v = 68,
        .w = 34,
        .h = 34,
        .x = 0,
        .y = 0,
        .pri = 143360,
        .alpha = 128,
    },
    {
        .tex0 = 2307453681563413895,
        .u = 0,
        .v = 0,
        .w = 30,
        .h = 30,
        .x = 0,
        .y = 0,
        .pri = 143360,
        .alpha = 128,
    },
    {
        .tex0 = 2307453681563413895,
        .u = 30,
        .v = 0,
        .w = 30,
        .h = 30,
        .x = 0,
        .y = 0,
        .pri = 143360,
        .alpha = 128,
    },
    {
        .tex0 = 2307453681563413895,
        .u = 60,
        .v = 0,
        .w = 30,
        .h = 30,
        .x = 0,
        .y = 0,
        .pri = 143360,
        .alpha = 128,
    },
    {
        .tex0 = 2307453681563413895,
        .u = 90,
        .v = 0,
        .w = 30,
        .h = 30,
        .x = 0,
        .y = 0,
        .pri = 143360,
        .alpha = 128,
    },
    {
        .tex0 = 2307453681563413895,
        .u = 0,
        .v = 30,
        .w = 30,
        .h = 30,
        .x = 0,
        .y = 0,
        .pri = 143360,
        .alpha = 128,
    },
    {
        .tex0 = 2307453681563413895,
        .u = 30,
        .v = 30,
        .w = 30,
        .h = 30,
        .x = 0,
        .y = 0,
        .pri = 143360,
        .alpha = 128,
    },
    {
        .tex0 = 2307453681563413895,
        .u = 60,
        .v = 30,
        .w = 30,
        .h = 30,
        .x = 0,
        .y = 0,
        .pri = 143360,
        .alpha = 128,
    },
    {
        .tex0 = 2307453681563413895,
        .u = 90,
        .v = 30,
        .w = 30,
        .h = 30,
        .x = 0,
        .y = 0,
        .pri = 143360,
        .alpha = 128,
    },
    {
        .tex0 = 2307453681563413895,
        .u = 0,
        .v = 60,
        .w = 30,
        .h = 30,
        .x = 0,
        .y = 0,
        .pri = 143360,
        .alpha = 128,
    },
    {
        .tex0 = 2307453681563413895,
        .u = 30,
        .v = 60,
        .w = 30,
        .h = 30,
        .x = 0,
        .y = 0,
        .pri = 143360,
        .alpha = 128,
    },
    {
        .tex0 = 2307453681563413895,
        .u = 60,
        .v = 60,
        .w = 30,
        .h = 30,
        .x = 0,
        .y = 0,
        .pri = 143360,
        .alpha = 128,
    },
    {
        .tex0 = 2307453681563413895,
        .u = 120,
        .v = 0,
        .w = 34,
        .h = 34,
        .x = 0,
        .y = 0,
        .pri = 143360,
        .alpha = 128,
    },
    {
        .tex0 = 2307453681563413895,
        .u = 154,
        .v = 0,
        .w = 34,
        .h = 34,
        .x = 0,
        .y = 0,
        .pri = 143360,
        .alpha = 128,
    },
    {
        .tex0 = 2307453681563413895,
        .u = 188,
        .v = 0,
        .w = 34,
        .h = 34,
        .x = 0,
        .y = 0,
        .pri = 143360,
        .alpha = 128,
    },
    {
        .tex0 = 2307453681563413895,
        .u = 120,
        .v = 34,
        .w = 34,
        .h = 34,
        .x = 0,
        .y = 0,
        .pri = 143360,
        .alpha = 128,
    },
    {
        .tex0 = 2307453681563413895,
        .u = 154,
        .v = 34,
        .w = 34,
        .h = 34,
        .x = 0,
        .y = 0,
        .pri = 143360,
        .alpha = 128,
    },
    {
        .tex0 = 2307453681563413895,
        .u = 120,
        .v = 68,
        .w = 34,
        .h = 34,
        .x = 0,
        .y = 0,
        .pri = 143360,
        .alpha = 128,
    },
    {
        .tex0 = 2307453681563413895,
        .u = 154,
        .v = 68,
        .w = 34,
        .h = 34,
        .x = 0,
        .y = 0,
        .pri = 143360,
        .alpha = 128,
    },
    {
        .tex0 = 2307454091531431368,
        .u = 1,
        .v = 1,
        .w = 20,
        .h = 6,
        .x = 309,
        .y = 71,
        .pri = 102400,
        .alpha = 128,
    },
    {
        .tex0 = 2307454091531431368,
        .u = 1,
        .v = 1,
        .w = 20,
        .h = 6,
        .x = 309,
        .y = 329,
        .pri = 102400,
        .alpha = 128,
    },
    {
        .tex0 = 2307454091531431368,
        .u = 23,
        .v = 1,
        .w = 8,
        .h = 16,
        .x = 126,
        .y = 195,
        .pri = 102400,
        .alpha = 128,
    },
    {
        .tex0 = 2307454091531431368,
        .u = 23,
        .v = 1,
        .w = 8,
        .h = 16,
        .x = 504,
        .y = 195,
        .pri = 102400,
        .alpha = 128,
    },
    {
        .tex0 = 2307454091531431368,
        .u = 8,
        .v = 9,
        .w = 12,
        .h = 4,
        .x = 202,
        .y = 101,
        .pri = 102400,
        .alpha = 128,
    },
    {
        .tex0 = 2307454091531431368,
        .u = 8,
        .v = 9,
        .w = 12,
        .h = 4,
        .x = 202,
        .y = 249,
        .pri = 102400,
        .alpha = 128,
    },
    {
        .tex0 = 2307454091531431368,
        .u = 1,
        .v = 9,
        .w = 5,
        .h = 12,
        .x = 95,
        .y = 171,
        .pri = 102400,
        .alpha = 128,
    },
    {
        .tex0 = 2307454091531431368,
        .u = 1,
        .v = 9,
        .w = 5,
        .h = 12,
        .x = 316,
        .y = 171,
        .pri = 102400,
        .alpha = 128,
    },
    {
        .tex0 = 2307459865174388180,
        .u = 0,
        .v = 0,
        .w = 52,
        .h = 26,
        .x = 50,
        .y = 368,
        .pri = 163840,
        .alpha = 128,
    },
    {
        .tex0 = 2307459865174388180,
        .u = 0,
        .v = 26,
        .w = 52,
        .h = 26,
        .x = 333,
        .y = 340,
        .pri = 163840,
        .alpha = 128,
    },
    {
        .tex0 = 2307459865174388180,
        .u = 64,
        .v = 0,
        .w = 52,
        .h = 26,
        .x = 333,
        .y = 368,
        .pri = 163840,
        .alpha = 128,
    },
    {
        .tex0 = 2307459865174388180,
        .u = 64,
        .v = 26,
        .w = 52,
        .h = 26,
        .x = 333,
        .y = 395,
        .pri = 163840,
        .alpha = 128,
    },
    {
        .tex0 = 2307459865174388180,
        .u = 0,
        .v = 0,
        .w = 52,
        .h = 26,
        .x = 326,
        .y = 208,
        .pri = 163840,
        .alpha = 128,
    },
    {
        .tex0 = 2307459865174388180,
        .u = 0,
        .v = 26,
        .w = 52,
        .h = 26,
        .x = 325,
        .y = 122,
        .pri = 163840,
        .alpha = 128,
    },
    {
        .tex0 = 2307459865174388180,
        .u = 64,
        .v = 0,
        .w = 52,
        .h = 26,
        .x = 325,
        .y = 152,
        .pri = 163840,
        .alpha = 128,
    },
    {
        .tex0 = 2307459865174388180,
        .u = 64,
        .v = 26,
        .w = 52,
        .h = 26,
        .x = 326,
        .y = 92,
        .pri = 163840,
        .alpha = 128,
    },
    {
        .tex0 = 2307455329622863307,
        .u = 1,
        .v = 1,
        .w = 52,
        .h = 36,
        .x = 0,
        .y = 0,
        .pri = 102400,
        .alpha = 128,
    },
    {
        .tex0 = 2307462613954506232,
        .u = 1,
        .v = 1,
        .w = 90,
        .h = 30,
        .x = 221,
        .y = 199,
        .pri = 20480,
        .alpha = 128,
    },
    {
        .tex0 = 2307312805562362278,
        .u = 2,
        .v = 2,
        .w = 81,
        .h = 50,
        .x = 0,
        .y = 0,
        .pri = 143360,
        .alpha = 128,
    },
    {
        .tex0 = 2307312805562362278,
        .u = 2,
        .v = 2,
        .w = 81,
        .h = 50,
        .x = 81,
        .y = 0,
        .pri = 143360,
        .alpha = 128,
    },
    {
        .tex0 = 2307312805562362278,
        .u = 2,
        .v = 2,
        .w = 89,
        .h = 50,
        .x = 0,
        .y = 0,
        .pri = 143360,
        .alpha = 128,
    },
    {
        .tex0 = 2307312805562362278,
        .u = 2,
        .v = 2,
        .w = 89,
        .h = 50,
        .x = 89,
        .y = 0,
        .pri = 143360,
        .alpha = 128,
    },
    {
        .tex0 = 2307308271083497861,
        .u = 1,
        .v = 1,
        .w = 96,
        .h = 78,
        .x = -15,
        .y = -16,
        .pri = 143360,
        .alpha = 128,
    },
    {
        .tex0 = 2307308271083497861,
        .u = 1,
        .v = 1,
        .w = 96,
        .h = 78,
        .x = 81,
        .y = -16,
        .pri = 143360,
        .alpha = 128,
    },
    {
        .tex0 = 2307308271083497861,
        .u = 1,
        .v = 1,
        .w = 104,
        .h = 78,
        .x = -15,
        .y = -16,
        .pri = 143360,
        .alpha = 128,
    },
    {
        .tex0 = 2307308271083497861,
        .u = 1,
        .v = 1,
        .w = 104,
        .h = 78,
        .x = 89,
        .y = -16,
        .pri = 143360,
        .alpha = 128,
    },
    {
        .tex0 = 2307277073649084354,
        .u = 0,
        .v = 0,
        .w = 158,
        .h = 23,
        .x = 3,
        .y = 3,
        .pri = 143360,
        .alpha = 128,
    },
    {
        .tex0 = 2307277073649084354,
        .u = 0,
        .v = 0,
        .w = 171,
        .h = 23,
        .x = 3,
        .y = 3,
        .pri = 143360,
        .alpha = 128,
    },
    {
        .tex0 = 2307294802200373443,
        .u = 0,
        .v = 0,
        .w = 154,
        .h = 19,
        .x = 4,
        .y = 27,
        .pri = 143360,
        .alpha = 128,
    },
    {
        .tex0 = 2307294802200373443,
        .u = 0,
        .v = 0,
        .w = 169,
        .h = 19,
        .x = 4,
        .y = 27,
        .pri = 143360,
        .alpha = 128,
    },
    {
        .tex0 = 2307294802200373443,
        .u = 0,
        .v = 0,
        .w = 196,
        .h = 19,
        .x = 4,
        .y = 27,
        .pri = 143360,
        .alpha = 128,
    },
    {
        .tex0 = 2307294802200373443,
        .u = 0,
        .v = 0,
        .w = 169,
        .h = 19,
        .x = -4,
        .y = 27,
        .pri = 143360,
        .alpha = 128,
    },
    {
        .tex0 = 2307294802200373443,
        .u = 0,
        .v = 0,
        .w = 62,
        .h = 19,
        .x = 49,
        .y = 27,
        .pri = 143360,
        .alpha = 128,
    },
    {
        .tex0 = 2307294802200373443,
        .u = 0,
        .v = 0,
        .w = 84,
        .h = 19,
        .x = 38,
        .y = 27,
        .pri = 143360,
        .alpha = 128,
    },
    {
        .tex0 = 2307294802200373443,
        .u = 0,
        .v = 0,
        .w = 104,
        .h = 19,
        .x = 28,
        .y = 27,
        .pri = 143360,
        .alpha = 128,
    },
    {
        .tex0 = 2307294802200373443,
        .u = 0,
        .v = 0,
        .w = 104,
        .h = 19,
        .x = 28,
        .y = 27,
        .pri = 143360,
        .alpha = 128,
    },
    {
        .tex0 = 2307232818306098816,
        .u = 0,
        .v = 0,
        .w = 126,
        .h = 20,
        .x = 68,
        .y = 25,
        .pri = 143360,
        .alpha = 128,
    },
    {
        .tex0 = 2307232818306098816,
        .u = 129,
        .v = 133,
        .w = 94,
        .h = 42,
        .x = 426,
        .y = 783,
        .pri = 143360,
        .alpha = 128,
    },
    {
        .tex0 = 2307232818306098816,
        .u = 1,
        .v = 23,
        .w = 62,
        .h = 44,
        .x = 219,
        .y = 532,
        .pri = 143360,
        .alpha = 128,
    },
    {
        .tex0 = 2307232818306098816,
        .u = 1,
        .v = 111,
        .w = 126,
        .h = 20,
        .x = 288,
        .y = 492,
        .pri = 143360,
        .alpha = 128,
    },
    {
        .tex0 = 2307232818306098816,
        .u = 1,
        .v = 1,
        .w = 126,
        .h = 20,
        .x = 98,
        .y = 438,
        .pri = 143360,
        .alpha = 128,
    },
    {
        .tex0 = 2307232818306098816,
        .u = 129,
        .v = 89,
        .w = 94,
        .h = 42,
        .x = 98,
        .y = 427,
        .pri = 143360,
        .alpha = 128,
    },
    {
        .tex0 = 2307232818306098816,
        .u = 129,
        .v = 133,
        .w = 94,
        .h = 42,
        .x = 514,
        .y = 620,
        .pri = 143360,
        .alpha = 128,
    },
    {
        .tex0 = 2307232818306098816,
        .u = 225,
        .v = 89,
        .w = 158,
        .h = 42,
        .x = 280,
        .y = 304,
        .pri = 143360,
        .alpha = 128,
    },
    {
        .tex0 = 2307232818306098816,
        .u = 1,
        .v = 23,
        .w = 126,
        .h = 20,
        .x = 541,
        .y = 329,
        .pri = 143360,
        .alpha = 128,
    },
    {
        .tex0 = 2307232818306098816,
        .u = 1,
        .v = 133,
        .w = 126,
        .h = 20,
        .x = 316,
        .y = 374,
        .pri = 143360,
        .alpha = 128,
    },
    {
        .tex0 = 2307232818306098816,
        .u = 1,
        .v = 45,
        .w = 126,
        .h = 20,
        .x = 457,
        .y = 474,
        .pri = 143360,
        .alpha = 128,
    },
    {
        .tex0 = 2307232818306098816,
        .u = 1,
        .v = 67,
        .w = 126,
        .h = 20,
        .x = 532,
        .y = 512,
        .pri = 143360,
        .alpha = 128,
    },
    {
        .tex0 = 2307232818306098816,
        .u = 1,
        .v = 177,
        .w = 126,
        .h = 20,
        .x = 220,
        .y = 316,
        .pri = 143360,
        .alpha = 128,
    },
    {
        .tex0 = 2307232818306098816,
        .u = 257,
        .v = 45,
        .w = 126,
        .h = 42,
        .x = 184,
        .y = 188,
        .pri = 143360,
        .alpha = 128,
    },
    {
        .tex0 = 2307232818306098816,
        .u = 257,
        .v = 45,
        .w = 126,
        .h = 42,
        .x = 43,
        .y = 213,
        .pri = 143360,
        .alpha = 128,
    },
    {
        .tex0 = 2307232818306098816,
        .u = 385,
        .v = 1,
        .w = 126,
        .h = 42,
        .x = 574,
        .y = 598,
        .pri = 143360,
        .alpha = 128,
    },
    {
        .tex0 = 2307232818306098816,
        .u = 1,
        .v = 221,
        .w = 222,
        .h = 20,
        .x = 414,
        .y = 842,
        .pri = 143360,
        .alpha = 128,
    },
    {
        .tex0 = 2307232818306098816,
        .u = 385,
        .v = 177,
        .w = 126,
        .h = 42,
        .x = 312,
        .y = 304,
        .pri = 143360,
        .alpha = 128,
    },
    {
        .tex0 = 2307232818306098816,
        .u = 225,
        .v = 89,
        .w = 158,
        .h = 42,
        .x = 264,
        .y = 372,
        .pri = 143360,
        .alpha = 128,
    },
    {
        .tex0 = 2307232818306098816,
        .u = 225,
        .v = 133,
        .w = 158,
        .h = 42,
        .x = 246,
        .y = 650,
        .pri = 143360,
        .alpha = 128,
    },
    {
        .tex0 = 2307232818306098816,
        .u = 129,
        .v = 1,
        .w = 126,
        .h = 20,
        .x = 70,
        .y = 307,
        .pri = 143360,
        .alpha = 128,
    },
    {
        .tex0 = 2307232818306098816,
        .u = 1,
        .v = 177,
        .w = 126,
        .h = 20,
        .x = 220,
        .y = 554,
        .pri = 143360,
        .alpha = 128,
    },
    {
        .tex0 = 2307232818306098816,
        .u = 257,
        .v = 1,
        .w = 126,
        .h = 42,
        .x = 284,
        .y = 423,
        .pri = 143360,
        .alpha = 128,
    },
    {
        .tex0 = 2307232818306098816,
        .u = 129,
        .v = 45,
        .w = 126,
        .h = 20,
        .x = 65,
        .y = 468,
        .pri = 143360,
        .alpha = 128,
    },
    {
        .tex0 = 2307232818306098816,
        .u = 305,
        .v = 177,
        .w = 78,
        .h = 42,
        .x = 342,
        .y = 541,
        .pri = 143360,
        .alpha = 128,
    },
    {
        .tex0 = 2307232818306098816,
        .u = 225,
        .v = 177,
        .w = 78,
        .h = 42,
        .x = 258,
        .y = 532,
        .pri = 143360,
        .alpha = 128,
    },
    {
        .tex0 = 2307232818306098816,
        .u = 1,
        .v = 155,
        .w = 126,
        .h = 20,
        .x = 207,
        .y = 772,
        .pri = 143360,
        .alpha = 128,
    },
    {
        .tex0 = 2307232818306098816,
        .u = 129,
        .v = 177,
        .w = 94,
        .h = 42,
        .x = 120,
        .y = 712,
        .pri = 143360,
        .alpha = 128,
    },
    {
        .tex0 = 2307232818306098816,
        .u = 1,
        .v = 89,
        .w = 126,
        .h = 20,
        .x = 326,
        .y = 223,
        .pri = 143360,
        .alpha = 128,
    },
    {
        .tex0 = 2307232818306098816,
        .u = 1,
        .v = 199,
        .w = 126,
        .h = 20,
        .x = 65,
        .y = 451,
        .pri = 143360,
        .alpha = 128,
    },
    {
        .tex0 = 2307232818306098816,
        .u = 129,
        .v = 67,
        .w = 126,
        .h = 20,
        .x = 450,
        .y = 329,
        .pri = 143360,
        .alpha = 128,
    },
    {
        .tex0 = 2307232818306098816,
        .u = 129,
        .v = 23,
        .w = 126,
        .h = 20,
        .x = 284,
        .y = 529,
        .pri = 143360,
        .alpha = 128,
    },
    {
        .tex0 = 2307232818306098816,
        .u = 125,
        .v = 91,
        .w = 18,
        .h = 20,
        .x = 242,
        .y = 382,
        .pri = 143360,
        .alpha = 128,
    },
    {
        .tex0 = 2307315141957429703,
        .u = 97,
        .v = 1,
        .w = 30,
        .h = 30,
        .x = 399,
        .y = 111,
        .pri = 143360,
        .alpha = 128,
    },
    {
        .tex0 = 2307315141957429703,
        .u = 65,
        .v = 33,
        .w = 30,
        .h = 30,
        .x = 11,
        .y = 287,
        .pri = 143360,
        .alpha = 128,
    },
    {
        .tex0 = 2307315141957429703,
        .u = 97,
        .v = 33,
        .w = 30,
        .h = 30,
        .x = 399,
        .y = 287,
        .pri = 143360,
        .alpha = 128,
    },
    {
        .tex0 = 2307315141957429703,
        .u = 33,
        .v = 33,
        .w = 30,
        .h = 30,
        .x = 11,
        .y = 199,
        .pri = 143360,
        .alpha = 128,
    },
    {
        .tex0 = 2307315141957429703,
        .u = 33,
        .v = 33,
        .w = 30,
        .h = 30,
        .x = 399,
        .y = 199,
        .pri = 143360,
        .alpha = 128,
    },
    {
        .tex0 = 2307315141957429703,
        .u = 1,
        .v = 33,
        .w = 27,
        .h = 30,
        .x = 286,
        .y = 111,
        .pri = 143360,
        .alpha = 128,
    },
    {
        .tex0 = 2307315141957429703,
        .u = 1,
        .v = 33,
        .w = 30,
        .h = 30,
        .x = 205,
        .y = 287,
        .pri = 143360,
        .alpha = 128,
    },
    {
        .tex0 = 2307324076630288856,
        .u = 1,
        .v = 1,
        .w = 197,
        .h = 51,
        .x = 11,
        .y = 90,
        .pri = 143360,
        .alpha = 128,
    },
    {
        .tex0 = 2307315141957429703,
        .u = 97,
        .v = 1,
        .w = 30,
        .h = 30,
        .x = 399,
        .y = 351,
        .pri = 143360,
        .alpha = 128,
    },
    {
        .tex0 = 2307315141957429703,
        .u = 65,
        .v = 33,
        .w = 30,
        .h = 30,
        .x = 11,
        .y = 627,
        .pri = 143360,
        .alpha = 128,
    },
    {
        .tex0 = 2307315141957429703,
        .u = 97,
        .v = 33,
        .w = 30,
        .h = 30,
        .x = 399,
        .y = 627,
        .pri = 143360,
        .alpha = 128,
    },
    {
        .tex0 = 2307315141957429703,
        .u = 33,
        .v = 33,
        .w = 30,
        .h = 30,
        .x = 11,
        .y = 489,
        .pri = 143360,
        .alpha = 128,
    },
    {
        .tex0 = 2307315141957429703,
        .u = 33,
        .v = 33,
        .w = 30,
        .h = 30,
        .x = 399,
        .y = 489,
        .pri = 143360,
        .alpha = 128,
    },
    {
        .tex0 = 2307315141957429703,
        .u = 1,
        .v = 33,
        .w = 30,
        .h = 30,
        .x = 288,
        .y = 351,
        .pri = 143360,
        .alpha = 128,
    },
    {
        .tex0 = 2307315141957429703,
        .u = 1,
        .v = 33,
        .w = 30,
        .h = 30,
        .x = 205,
        .y = 627,
        .pri = 143360,
        .alpha = 128,
    },
    {
        .tex0 = 2307324076630288856,
        .u = 1,
        .v = 1,
        .w = 197,
        .h = 51,
        .x = 11,
        .y = 330,
        .pri = 143360,
        .alpha = 128,
    },
    {
        .tex0 = 2307315141957429703,
        .u = 97,
        .v = 1,
        .w = 30,
        .h = 30,
        .x = 399,
        .y = 679,
        .pri = 143360,
        .alpha = 128,
    },
    {
        .tex0 = 2307315141957429703,
        .u = 65,
        .v = 33,
        .w = 30,
        .h = 30,
        .x = 11,
        .y = 855,
        .pri = 143360,
        .alpha = 128,
    },
    {
        .tex0 = 2307315141957429703,
        .u = 97,
        .v = 33,
        .w = 30,
        .h = 30,
        .x = 399,
        .y = 855,
        .pri = 143360,
        .alpha = 128,
    },
    {
        .tex0 = 2307315141957429703,
        .u = 33,
        .v = 33,
        .w = 30,
        .h = 30,
        .x = 11,
        .y = 767,
        .pri = 143360,
        .alpha = 128,
    },
    {
        .tex0 = 2307315141957429703,
        .u = 33,
        .v = 33,
        .w = 30,
        .h = 30,
        .x = 399,
        .y = 767,
        .pri = 143360,
        .alpha = 128,
    },
    {
        .tex0 = 2307315141957429703,
        .u = 1,
        .v = 33,
        .w = 30,
        .h = 30,
        .x = 288,
        .y = 679,
        .pri = 143360,
        .alpha = 128,
    },
    {
        .tex0 = 2307315141957429703,
        .u = 1,
        .v = 33,
        .w = 30,
        .h = 30,
        .x = 205,
        .y = 855,
        .pri = 143360,
        .alpha = 128,
    },
    {
        .tex0 = 2307324076630288856,
        .u = 1,
        .v = 1,
        .w = 197,
        .h = 51,
        .x = 11,
        .y = 658,
        .pri = 143360,
        .alpha = 128,
    },
    {
        .tex0 = 2307315141957429703,
        .u = 65,
        .v = 1,
        .w = 30,
        .h = 30,
        .x = 411,
        .y = 212,
        .pri = 143360,
        .alpha = 128,
    },
    {
        .tex0 = 2307315141957429703,
        .u = 97,
        .v = 1,
        .w = 30,
        .h = 30,
        .x = 599,
        .y = 212,
        .pri = 143360,
        .alpha = 128,
    },
    {
        .tex0 = 2307315141957429703,
        .u = 65,
        .v = 33,
        .w = 30,
        .h = 30,
        .x = 411,
        .y = 288,
        .pri = 143360,
        .alpha = 128,
    },
    {
        .tex0 = 2307315141957429703,
        .u = 97,
        .v = 33,
        .w = 30,
        .h = 30,
        .x = 599,
        .y = 288,
        .pri = 143360,
        .alpha = 128,
    },
    {
        .tex0 = 2307315141957429703,
        .u = 33,
        .v = 33,
        .w = 30,
        .h = 30,
        .x = 411,
        .y = 250,
        .pri = 143360,
        .alpha = 128,
    },
    {
        .tex0 = 2307315141957429703,
        .u = 33,
        .v = 33,
        .w = 30,
        .h = 30,
        .x = 599,
        .y = 250,
        .pri = 143360,
        .alpha = 128,
    },
    {
        .tex0 = 2307315141957429703,
        .u = 1,
        .v = 33,
        .w = 30,
        .h = 30,
        .x = 505,
        .y = 212,
        .pri = 143360,
        .alpha = 128,
    },
    {
        .tex0 = 2307315141957429703,
        .u = 1,
        .v = 33,
        .w = 30,
        .h = 30,
        .x = 505,
        .y = 288,
        .pri = 143360,
        .alpha = 128,
    },
    {
        .tex0 = 2307315141957429703,
        .u = 65,
        .v = 1,
        .w = 31,
        .h = 21,
        .x = 411,
        .y = 391,
        .pri = 143360,
        .alpha = 128,
    },
    {
        .tex0 = 2307315141957429703,
        .u = 65,
        .v = 33,
        .w = 30,
        .h = 30,
        .x = 411,
        .y = 771,
        .pri = 143360,
        .alpha = 128,
    },
    {
        .tex0 = 2307315141957429703,
        .u = 97,
        .v = 33,
        .w = 30,
        .h = 30,
        .x = 607,
        .y = 771,
        .pri = 143360,
        .alpha = 128,
    },
    {
        .tex0 = 2307315141957429703,
        .u = 33,
        .v = 33,
        .w = 30,
        .h = 30,
        .x = 411,
        .y = 576,
        .pri = 143360,
        .alpha = 128,
    },
    {
        .tex0 = 2307315141957429703,
        .u = 33,
        .v = 33,
        .w = 30,
        .h = 30,
        .x = 607,
        .y = 581,
        .pri = 143360,
        .alpha = 128,
    },
    {
        .tex0 = 2307315141957429703,
        .u = 1,
        .v = 33,
        .w = 31,
        .h = 30,
        .x = 506,
        .y = 771,
        .pri = 143360,
        .alpha = 128,
    },
    {
        .tex0 = 2307315141957429703,
        .u = 1,
        .v = 33,
        .w = 0,
        .h = 30,
        .x = 457,
        .y = 391,
        .pri = 143360,
        .alpha = 128,
    },
    {
        .tex0 = 2307324076630288856,
        .u = 1,
        .v = 1,
        .w = 195,
        .h = 51,
        .x = 442,
        .y = 370,
        .pri = 143360,
        .alpha = 128,
    },
    {
        .tex0 = 2307333010162264601,
        .u = 1,
        .v = 1,
        .w = 254,
        .h = 48,
        .x = 282,
        .y = 48,
        .pri = 143360,
        .alpha = 128,
    },
    {
        .tex0 = 2307333010162264601,
        .u = 1,
        .v = 53,
        .w = 50,
        .h = 48,
        .x = 536,
        .y = 48,
        .pri = 143360,
        .alpha = 128,
    },
    {
        .tex0 = 2307303735665174852,
        .u = 1,
        .v = 1,
        .w = 152,
        .h = 90,
        .x = 466,
        .y = 124,
        .pri = 143360,
        .alpha = 128,
    },
    {
        .tex0 = 2307241750697158529,
        .u = 1,
        .v = 1,
        .w = 164,
        .h = 24,
        .x = 466,
        .y = 124,
        .pri = 143360,
        .alpha = 128,
    },
    {
        .tex0 = 2307241750697158529,
        .u = 1,
        .v = 27,
        .w = 164,
        .h = 24,
        .x = 466,
        .y = 124,
        .pri = 143360,
        .alpha = 128,
    },
    {
        .tex0 = 2307241750697158529,
        .u = 1,
        .v = 53,
        .w = 164,
        .h = 24,
        .x = 466,
        .y = 124,
        .pri = 143360,
        .alpha = 128,
    },
    {
        .tex0 = 2307241750697158529,
        .u = 1,
        .v = 79,
        .w = 164,
        .h = 24,
        .x = 466,
        .y = 124,
        .pri = 143360,
        .alpha = 128,
    },
};

u_short flip_table[] = {
    0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 1, 3, 0, 2, 1, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 0, 2, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 1, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 2, 0, 0, 0, 0, 0, 0,
    2, 0, 0, 0, 2, 1, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 2, 2,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 2, 1, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 2, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1,
    1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 1, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 2, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 0, 2, 0, 2, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 1, 0, 0, 0, 0, 0, 1, 0, 0, 1, 0, 0, 0, 0, 0, 1, 0, 1, 0, 0, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 0, 0, 0, 0, 0, 0, 0};

SPRT_DAT raze_sprt = {
    .tex0 = 2307256178430748708,
    .u = 0,
    .v = 0,
    .w = 60,
    .h = 16,
    .x = 0,
    .y = 0,
    .pri = 0,
    .alpha = 128,
};

INFO_WRK info_wrk = {0};

static FLSH_CORE flsh_lgt;
static FLSH_CORE flsh_chrg[2];
static FLSH_CORE feramp;

static INF_DSP inf_dsp;
static NEW_INF new_inf;
static LIFE_BALL life_ball[5];
static PRTCL_BALL prtcl_ball[5][24];
static FLSH_CORE flsh_max[5];
static ZAN zanzo[12];
static ZAN dmg_scr;
static JET_SET jet1[25];
static JET_SET jet2[25];
static u_char znz[12][6];

#define PI 3.1415927f
#define DEG2RAD(x) ((float)(x)*PI/180.0f)


#ifdef BUILD_EU_VERSION
#define PL_FNDR_PK2_ADDRESS 0x1d83000
#define PL_LIFE_PK2_ADDRESS 0x1ded000
#else
#define PL_FNDR_PK2_ADDRESS 0x1d88100
#define PL_LIFE_PK2_ADDRESS 0x1df2100
#endif

void InformationDispInit()
{
    info_wrk = (INFO_WRK){0};
    inf_dsp = (INF_DSP){0};
    new_inf = (NEW_INF){0};

    memset(&jet1, 0, sizeof(jet1));
    memset(&jet2, 0, sizeof(jet2));

    point_get_end = 0;
}

void InformationDispMain()
{
    short int fndr_mx;
    short int fndr_my;

    if (inf_dsp.disp_pause != 0)
    {
        return;
    }

    fndr_mx = (((plyr_wrk.fp[0] - 320) * 8.5f) / 10.0f);
    fndr_my = (((plyr_wrk.fp[1] - 209) * 8.5f) / 10.0f);

    InformationDispModeCtrl();
    InformationDispWrkSet();

    if (inf_dsp.fndr_mode_tmr == 1)
    {
        FndrInit();
    }

    if (inf_dsp.fndr_dsp_flg != 0)
    {
        FinderDisp(fndr_mx, fndr_my);
    }

    if (sys_wrk.game_mode == GAME_MODE_INGAME)
    {
        if ((ingame_wrk.stts & 0x20) == 0 && MesStatusCheck() == 0)
        {
            if (inf_dsp.fndr_dsp_flg != 0)
            {
                EdogawaLamp(fndr_mx, fndr_my, 0);
            }
            else if (isDispLamp() != 0)
            {
                SetSprFile(PL_LIFE_PK2_ADDRESS);
                EdogawaLamp(230, 352, 1);
            }
            else
            {
                new_inf.rmp_alp = 0;
            }
        }
    }

    if (inf_dsp.stts_dsp_flg != 0)
    {
        if (inf_dsp.fndr_dsp_flg != 0)
        {
            PlayerInformationDisp(fndr_mx, fndr_my);
        }
        else
        {
            PlayerInformationDisp(0, 0);
        }
    }

    if (inf_dsp.scor_dsp != 0)
    {
        PhotoScoreDisp(plyr_wrk.ap_timer, plyr_wrk.sta & 0x1 ? 1 : 60);
    }
}

void InformationDispModeCtrl()
{
    if (inf_dsp.stts_dsp_flg != 0)
    {
        inf_dsp.stts_dsp_flg = 2;
    }

    if (inf_dsp.fndr_dsp_flg != 0)
    {
        inf_dsp.fndr_dsp_flg = 2;
    }

    if (plyr_wrk.mode == PMODE_FINDER)
    {
        inf_dsp.stts_dsp_flg = 3;
        inf_dsp.fndr_dsp_flg = 3;
    }

    if (plyr_wrk.hp < info_wrk.plyr_hp)
    {
        inf_dsp.stts_fade_tmr = 180;
    }

    if (inf_dsp.stts_fade_tmr != 0)
    {
        inf_dsp.stts_dsp_flg = 3;
    }

    if (ev_wrk.movie_on != 0)
    {
        inf_dsp.stts_dsp_flg = 0;
        inf_dsp.fndr_dsp_flg = 0;
    }

    if (ingame_wrk.stts & 0x20)
    {
        inf_dsp.stts_dsp_flg = 0;
        inf_dsp.fndr_dsp_flg = 0;
    }

    if (ingame_wrk.mode == INGAME_MODE_MENU)
    {
        inf_dsp.stts_dsp_flg = 0;
        inf_dsp.fndr_dsp_flg = 0;
    }

    if (plyr_wrk.ap_timer != 0)
    {
        inf_dsp.phot_shot = 10;
        inf_dsp.fndr_dsp_flg = 0;
        inf_dsp.stts_dsp_flg = 0;
    }

    else if (inf_dsp.phot_shot != 0)
    {
        inf_dsp.phot_shot--;
        inf_dsp.stts_dsp_flg = 1;
        inf_dsp.fndr_dsp_flg = 1;
    }

    SttsFade();
    FndrFade();

    if (inf_dsp.fndr_dsp_flg != 0)
    {
        SetSprFile(PL_FNDR_PK2_ADDRESS);
        SetSprFile(PL_LIFE_PK2_ADDRESS);
    }

    if (inf_dsp.stts_dsp_flg != 0)
    {
        SetSprFile(PL_LIFE_PK2_ADDRESS);
    }
    else
    {
        new_inf.red_bar = (plyr_wrk.hp * 243) / 500.0f;
    }
}

static void FndrInit()
{
    int i;
    short int init;

    for (i = 0; i < 5; i++)
    {
        init = (2883584 + 983040 * i) / 65536; // same as "44 + 15 * i", but with a higher timer-related resolution?

        if (init < 0)
        {
            init = 0;
        }

        flsh_max[i].flsh_tmr = init;
    }

    new_inf.bon_bak = 0xff;
    new_inf.mfb_stts = 0;

    new_inf.pht_scr = photo_wrk.score;
    new_inf.pht_cnt_bak = ingame_wrk.pht_cnt;

    new_inf.buki_use_flg = 0;
    new_inf.buki_use_alp = 0;
    new_inf.buki_use_flr = 0;

    new_inf.dsp_dmg_flg = 0;
    new_inf.rmp_alp = 0;

    for (i = 0; i < 3; i++)
    {
        new_inf.shot_kind[i] = 0;
    }

    new_inf.shot_kind2 = 0;

    for (i = 0; i < 12; i++)
    {
        ZanzoSS(&zanzo[i]);
    }

    MafubaInit(0.0f, 0.0f);

    new_inf.dsp_dmg_step = 0;
    new_inf.dsp_dmg_step2 = 0;
    new_inf.dsp_dmg_alp = 0;

    new_inf.dmg_num_alp = 0;
    new_inf.dmg_str_alp = 0;
    new_inf.dmg_eff_alp = 0;
    new_inf.dmg_wait_tm = 0;

    new_inf.tm_max = 0;

    for (i = 0; i < 12; i++)
    {
        new_inf.chrg_rot[i] = 0;
        new_inf.chrg_flr[i] = 0;
        new_inf.chrg_stp[i] = 0;
        new_inf.dray_tmr[i] = 0;
    }
}

void InformationDispWrkSet()
{
    info_wrk.plyr_hp = plyr_wrk.hp;
    info_wrk.film_num = poss_item[plyr_wrk.film_no];
    info_wrk.frot_x = plyr_wrk.frot_x;
    info_wrk.plyr_pos_x = plyr_wrk.move_box.pos[0];
    info_wrk.plyr_pos_z = plyr_wrk.move_box.pos[2];

    if (inf_dsp.stts_fade_tmr != 0)
    {
        inf_dsp.stts_fade_tmr--;
    }

    if (inf_dsp.flm_cng_tmr != 0)
    {
        inf_dsp.flm_cng_tmr--;
    }

    if (plyr_wrk.mode == PMODE_FINDER)
    {
        if (inf_dsp.fndr_mode_tmr < 0xffff)
        {
            inf_dsp.fndr_mode_tmr++;
        }

        inf_dsp.fndr_fade_tmr = 10;
    }
    else if (inf_dsp.fndr_fade_tmr != 0)
    {
        inf_dsp.fndr_fade_tmr--;
    }
    else
    {
        inf_dsp.fndr_mode_tmr = 0;
    }
}

void PlayerInformationDisp(short int pos_x, short int pos_y)
{
    if (ingame_wrk.mode != INGAME_MODE_SPD_OPT)
    {
        ShowMikuPower2(pos_x, pos_y);
    }
}

void FinderDisp(short int pos_x, short int pos_y)
{
    short bon_x[4] = { 116, 494, 116, 494 };
    short bon_y[4] = {  86,  86, 333, 333 };
    char bon_c[12] = {
        10, 8, 6, 0, 1, 2, 3, 4, 5, 7, 9, 11,
    };
    u_char n;
    u_char sx;

    inf_dsp.lucky7 = ShutterChanceChk();

    NewFndrBase(pos_x, pos_y);
    WeakPoint(pos_x, pos_y);
    FilmZansu(info_wrk.film_num, pos_x, pos_y, 41, 377, 2);
    Mafuba(pos_x, pos_y, enedmg_status, 10, 10, 15, 10, 10, 1, 2.0f, 1.0f);
    MafubaFinish(pos_x, pos_y, eneseal_status, 60);
    ShowEneCtrl(pos_x, pos_y);
    GekisyaBoyGeisyaGirl(pos_x, pos_y);
    BukiyouMan(pos_x, pos_y);
    KanKinoSyogai(pos_x, pos_y);
    TameKin2(pos_x, pos_y);

    if (plyr_wrk.charge_num != 0)
    {
        n = cam_custom_wrk.charge_max * 2 + 6;
        sx = (12 - n) / 2;

        sx = sx + plyr_wrk.charge_num - 1;
        n = sx % 4;

        BigBon2(sx, bon_x[n], bon_y[n], 320, 224, 20);
    }
}

float NC_ADJ = 0.95f;

void PhotoScoreDispOnOff(int on_off)
{
    if (on_off != 0)
    {
        inf_dsp.scor_dsp = 1;
    }
    else
    {
        inf_dsp.scor_dsp = 0;
    }
}

static void SttsFade()
{
    switch (inf_dsp.stts_dsp_flg)
    {
    case 0:
        inf_dsp.stts_fade_alp = 0;
    break;
    case 1:
        inf_dsp.stts_fade_alp = 100;
    break;
    case 2:
        if (inf_dsp.stts_fade_alp - 10 > 0)
        {
            inf_dsp.stts_fade_alp -= 10;
        }
        else
        {
            inf_dsp.stts_fade_alp = 0;
            inf_dsp.stts_dsp_flg = 0;
        }
    break;
    case 3:
        if (inf_dsp.stts_fade_alp + 10 < 100)
        {
            inf_dsp.stts_fade_alp += 10;
        }
        else
        {
            inf_dsp.stts_fade_alp = 100;
            inf_dsp.stts_dsp_flg = 1;
        }
    break;
    }
}

static void FndrFade()
{
    switch (inf_dsp.fndr_dsp_flg)
    {
    case 1:
        inf_dsp.fndr_fade_alp = 100;
    break;
    case 2:
        if (inf_dsp.fndr_fade_alp - 10 > 0)
        {
            inf_dsp.fndr_fade_alp -= 10;
        }
        else
        {
            inf_dsp.fndr_fade_alp = 0;
            inf_dsp.fndr_dsp_flg = 0;
        }
    break;
    case 0:
        inf_dsp.fndr_fade_alp = 0;
    break;
    case 3:
        if (inf_dsp.fndr_fade_alp + 5 < 100)
        {
            inf_dsp.fndr_fade_alp += 5;
        }
        else
        {
            inf_dsp.fndr_fade_alp = 100;
            inf_dsp.fndr_dsp_flg = 1;
        }
    break;
    }
}

static void WeakPoint(short int pos_x, short int pos_y)
{
    int i;
    u_char alp;
    u_char siz;
    float ex;
    float ey;
    u_char hnt_alp_num;
    u_char ray_alp_num;
    short int hnt_all_alp;
    u_char chrg_max;
    u_char chrg_alp;
    float chrg_rate;
    short int big_size;

    alp = (inf_dsp.fndr_fade_alp * 128) / 100.0f;

    PutSpriteYW(FND_CNT_CIR_SGT, FND_CNT_CIR_SGT, pos_x, pos_y, 0.0f, 0x808080, alp * 0.5f, 1.0f, 1.0f, 0, 0xff, 1, 0, 1);

    big_size = circle_radius_tbl[cam_custom_wrk.charge_range];

    DspBigCircle(0x57, pos_x, pos_y, alp / 3.0f, big_size, 0);

    for (i = 0; i < 4; i++)
    {
        if (ene_wrk[i].sta & 0x1)
        {
            if (inf_dsp.weak5_alp[i] + 8 < alp)
            {
                inf_dsp.weak5_alp[i] += 8;
            }
            else
            {
                inf_dsp.weak5_alp[i] = alp;
            }
        }
        else
        {
            if (alp < inf_dsp.weak5_alp[i])
            {
                inf_dsp.weak5_alp[i] = alp;
            }

            if (inf_dsp.weak5_alp[i] - 8 > 0)
            {
                inf_dsp.weak5_alp[i] -= 8;
            }
            else
            {
                inf_dsp.weak5_alp[i] = 0;
            }
        }

        if (plyr_wrk.mode != 0x1)
        {
            if (alp < inf_dsp.weak5_alp[i])
            {
                inf_dsp.weak5_alp[i] = alp;
            }
        }

        if (ene_wrk[i].sta & 0x400)
        {
            if (inf_dsp.weak5_tmr[i] < 10)
            {
                inf_dsp.weak5_tmr[i]++;
            }
            else
            {
                inf_dsp.weak5_tmr[i] = 10;
            }

            new_inf.ex[i] = ene_wrk[i].fp[0];
            new_inf.ey[i] = ene_wrk[i].fp[1];
        }
        else
        {
            if (inf_dsp.weak5_tmr[i] != 0)
            {
                inf_dsp.weak5_tmr[i]--;
            }
            else
            {
                inf_dsp.weak5_tmr[i] = 0;
                new_inf.ex[i] = plyr_wrk.fp[0];
                new_inf.ey[i] = plyr_wrk.fp[1];
            }
        }
    }

    for (i = 0; i < 1; i++)
    {
        if (plyr_wrk.fhp[i][3] != 0.0f)
        {
            GetCamI2DPos(plyr_wrk.fhp[i], &new_inf.hx[i], &new_inf.hy[i]);

            if (inf_dsp.hint5_alp[i] + 8 < alp)
            {
                inf_dsp.hint5_alp[i] += 8;
            }
            else
            {
                inf_dsp.hint5_alp[i] = alp;
            }

            if (inf_dsp.hint5_tmr[i] < 10)
            {
                inf_dsp.hint5_tmr[i]++;
            }
            else
            {
                inf_dsp.hint5_tmr[i] = 10;
            }
        }
        else
        {
            if (alp < inf_dsp.hint5_alp[i])
            {
                inf_dsp.hint5_alp[i] = alp;
            }
            if (inf_dsp.hint5_alp[i] - 8 > 0)
            {
                inf_dsp.hint5_alp[i] -= 8;
            }
            else
            {
                inf_dsp.hint5_alp[i] = 0;
            }

            if (inf_dsp.hint5_tmr[i] != 0)
            {
                inf_dsp.hint5_tmr[i]--;
            }
            else
            {
                inf_dsp.hint5_tmr[i] = 0;

                new_inf.hx[i] = plyr_wrk.fp[0];
                new_inf.hy[i] = plyr_wrk.fp[1];
            }
        }

        if (plyr_wrk.mode != 1)
        {
            if (inf_dsp.hint5_alp[i] > alp)
            {
                inf_dsp.hint5_alp[i] = alp;
            }
        }
    }

    hnt_alp_num = 0;
    hnt_all_alp = 0;

    for (i = 0; i < 1; i++)
    {
        if (inf_dsp.hint5_alp[i] != 0)
        {
            hnt_all_alp += inf_dsp.hint5_alp[i];
            hnt_alp_num++;
        }
    }

    if (hnt_alp_num != 0)
    {
        chrg_max = (float)hnt_all_alp / (float)hnt_alp_num;
        chrg_alp = 120.0f - (chrg_max * 60) / 128.0f;

        FlashStarYW(flsh_chrg, 0x64, 0x32, chrg_alp, 0);

        chrg_alp = chrg_max * flsh_chrg[0].alpha / 100.0f;
        big_size = circle_radius_tbl[cam_custom_wrk.charge_range];

        DspBigCircle(91, pos_x, pos_y, chrg_alp, big_size, 1);
    }

    ray_alp_num = 0;

    if (pa_radius_set != 0)
    {
        siz = ene_wrk[plyr_wrk.near_ene_no].pa_radius + ene_wrk[plyr_wrk.near_ene_no].pa_radius;
    }
    else
    {
        siz = 64;
    }

    for (i = 0; i < 4; i++)
    {
        ex = ((new_inf.ex[i] - plyr_wrk.fp[0]) * (inf_dsp.weak5_tmr[i] / 10.0f)) +
            (((plyr_wrk.fp[0] - 0x140) * 8.5f) / 10.0f);

        ey = ((new_inf.ey[i] - plyr_wrk.fp[1]) * (inf_dsp.weak5_tmr[i] / 10.0f)) +
            (((plyr_wrk.fp[1] - 0xd1) * 8.5f) / 10.0f);

        if (inf_dsp.weak5_alp[i] != 0)
        {
            char dummy = ~dummy; // HACK: fixes diff

            PointerNP(ex, ey, 0, inf_dsp.weak5_alp[i], siz);

            ray_alp_num++;
        }
    }

    if (GetEnePowerDegree() == -1.0f)
    {
        if (plyr_wrk.charge_rate < 0.1f)
        {
            chrg_rate = 0.1f;
        }
        else if (plyr_wrk.charge_rate <= 3.0f)
        {
            chrg_rate = plyr_wrk.charge_rate;
        }
        else
        {
            chrg_rate = 3.0f;
        }

        chrg_max = alp * chrg_rate / 3.0f;
        chrg_alp = 120.0f - (chrg_rate * 60.0f) / 3.0f;

        FlashStarYW(&flsh_chrg[1], 0x64, 0x32, chrg_alp, 0);

        chrg_alp = chrg_max * flsh_chrg[1].alpha / 100.0f;
        big_size = circle_radius_tbl[cam_custom_wrk.charge_range];

        if (chrg_alp != 0)
        {
            DspBigCircle(91, pos_x, pos_y, chrg_alp, big_size, 1);
        }
    }

    if (ray_alp_num == 0)
    {
        PointerNP(pos_x, pos_y, 0, alp, siz);
    }
}

static void DspBigCircle(u_short lu_chr, short int pos_x, short int pos_y, u_char alp, short int size_r, u_char cl_ptn)
{
    int i;
    DISP_SPRT ds;

    for (i = lu_chr; i <= lu_chr + 3; i++)
    {
        CopySprDToSpr(&ds, &spr_dat[i]);

        ds.x += pos_x;
        ds.y += pos_y;

        switch (cl_ptn)
        {
        case 1:
            ds.r = 55;
            ds.g = 70;
            ds.b = 255;
        break;
        case 2:
            ds.r = 0;
            ds.g = 161;
            ds.b = 38;
        break;
        }

        ds.alphar = 72;
        ds.alpha  = alp;

        if (inf_dsp.lucky7 != 0)
        {
            ds.r = 254;
            ds.g = 91;
            ds.b = 7;
        }

        ds.scw = size_r / 110.0f;
        ds.sch = ds.scw;
        ds.csx = spr_dat[lu_chr + 3].x;
        ds.csy = spr_dat[lu_chr + 3].y;

        if (flip_table[i] == 1)
        {
            ds.att |= 1;
        }

        if (flip_table[i] == 2)
        {
            ds.att |= 2;
        }

        if (flip_table[i] == 3)
        {
            ds.att |= 3;
        }

        ds.tex1 = SCE_GS_SET_TEX1_1(1, 0, SCE_GS_LINEAR, SCE_GS_LINEAR_MIPMAP_LINEAR, 0, 0, 0);
        ds.zbuf = SCE_GS_SET_ZBUF_1(0x8c, SCE_GS_PSMCT24, 1);

        DispSprD(&ds);
    }
}

static void PointerNP(short int cx, short int cy, u_char red, u_char alp, float siz)
{
    PointerEN(cx, cy, red, alp * 0.5f, siz);

    if (red != 0)
    {
        SetEffects(32, 1, (float)cx, (float)cy, siz * 0.5f, alp, red, 0x80 - red, 0x80 - red);
    }
}

static void PointerEN(short int pos_x, short int pos_y, u_char red, u_char alp, float size)
{
    float scl;
    DISP_SPRT ds;

    scl = size / 64.0f;

    CopySprDToSpr(&ds, &spr_dat[96]);

    ds.x += pos_x;
    ds.y += pos_y;
    ds.scw = scl;
    ds.sch = scl;
    ds.csx = ds.x + (ds.w >> 1) + pos_x;
    ds.csy = ds.y + (ds.h >> 1) + pos_y;
    ds.r = 0x80;
    ds.g = 0x80 - red;
    ds.b = ds.g;
    ds.alpha = alp;
    ds.tex1 = SCE_GS_SET_TEX1_1(1, 0, SCE_GS_LINEAR, SCE_GS_LINEAR_MIPMAP_LINEAR, 0, 0, 0);
    ds.zbuf = SCE_GS_SET_ZBUF_1(0x8c, SCE_GS_PSMCT24, 1);

    DispSprD(&ds);
}

static void FilmZansu(int number, short int pos_x, short int pos_y, short int num_x, short int num_y, int digit)
{
    int i;
    int multi10;
    int multi10_bak;
    int dsp_num;
    u_char dsp_mode;
    int disp_digit;

    multi10 = 1;

    PutSpriteYW(FND_CONT_BASE, FND_CONT_BASE, pos_x, pos_y, 0.0f, 0x808080,
                inf_dsp.fndr_fade_alp * 128 / 100.0f, 1.0f, 1.0f, 0, 0xff, 1, 0, 0);

    for (i = 0; i < digit; i++)
    {
        multi10 *= 10;
        multi10_bak = multi10;
    }

    number = number % multi10;

    if (inf_dsp.fndr_mode_tmr == 1)
    {
        info_wrk.film_bak = info_wrk.film_num;
    }

    if (info_wrk.film_num != info_wrk.film_bak)
    {
        inf_dsp.flm_cng_tmr = 80;

        info_wrk.film_bak = info_wrk.film_num;
    }

    disp_digit = 1;

    multi10 = multi10_bak;

    for (i = 0; i < digit; i++)
    {
        multi10 /=10;

        dsp_mode = 0;

        dsp_num = (number % (multi10 * 10)) / multi10;

        if (inf_dsp.flm_cng_tmr != 0)
        {
            if (inf_dsp.flm_cng_tmr < 21)
            {
                if (i != digit - disp_digit)
                {
                    dsp_mode = ((number / (multi10 / 10)) % 10 != 9) ? dsp_mode : disp_digit;
                }
                else
                {
                    dsp_mode = 1;
                }
            }
            else
            {
                dsp_num = ((number + 1) % (multi10 * 10)) / multi10;
            }
        }

        NumDisp(dsp_num, i, pos_x, pos_y, num_x, num_y, dsp_mode);
    }

    PutSpriteYW(FND_CONT_MASK, FND_CONT_MASK, pos_x, pos_y, 0.0f, 0x808080,
                inf_dsp.fndr_fade_alp * 128 / 100.0f, 1.0f, 1.0f, 0, 0xff, 1, 0, 0);
}

static void NumDisp(u_char number, u_char no, short int pos_x, short int pos_y, short int num_x, short int num_y, u_char mode)
{
    float alp;
    float slot_mov;
    int i;
    short int dsp_y;
    signed char dsp_v;
    signed char dsp_h;
    DISP_SPRT ds;
    u_char spr_no;
    short int tmp_y;

    alp = inf_dsp.fndr_fade_alp * 128 / 100.0f;

    if (mode == 0)
    {
        slot_mov = 0.0f;
    }
    else
    {
        slot_mov = spr_dat[3].h * inf_dsp.flm_cng_tmr / 20.0f;
    }

    for (i = 0; i < 4; i++)
    {
        spr_no = (number + 9 + i) % 10;

        CopySprDToSpr(&ds, &spr_dat[spr_no + 3]);

        tmp_y = pos_y + num_y + slot_mov + ds.h - ds.h * i;

        YVH_CUT(tmp_y, ds.h, pos_y + 365, pos_y + 423, &dsp_y, (signed char *)&dsp_v, (u_char *)&dsp_h);

        ds.x = pos_x + num_x + no * 29;
        ds.y = tmp_y + dsp_y;
        ds.h = dsp_h;
        ds.v += dsp_v;
        ds.alpha = alp;
        ds.tex1 = SCE_GS_SET_TEX1_1(1, 0, SCE_GS_LINEAR, SCE_GS_LINEAR_MIPMAP_LINEAR, 0, 0, 0);

        DispSprD(&ds);
    }
}

void YVH_CUT(short int tmp_y, u_char tmp_h, short int up_lmt, short int dwn_lmt, short int *dsp_y, signed char *dsp_v, u_char *dsp_h)
{
    if (tmp_y + tmp_h < up_lmt || tmp_y > dwn_lmt)
    {
        *dsp_v = 0;
        *dsp_h = 0;
        *dsp_y = tmp_y;
    }
    else if (tmp_y < up_lmt)
    {
        *dsp_v = up_lmt - tmp_y;
        *dsp_h = tmp_h - *dsp_v;
        *dsp_y = *dsp_v;
    }
    else if (tmp_y + tmp_h > dwn_lmt)
    {
        *dsp_v = 0;
        *dsp_h = dwn_lmt - tmp_y;
        *dsp_y = 0;
    }
    else
    {
        *dsp_v = 0;
        *dsp_h = tmp_h;
        *dsp_y = 0;
    }
}

static void EdogawaLamp(short int pos_x, short int pos_y, u_char out)
{
    float chk;
    float pow;
    float lag_pow;
    float cng_pow;
    float cmn_pow;
    signed char pow_add;
    u_char f_max;
    float f;
    u_char sml_alp;
    u_char lag_alp;
    u_char cmn_alp;
    int i;
    u_char ene_num;
    u_char obj;
    float tmp; // HACK: fixes operand swap

    chk = GetEnePowerDegree();

    if (chk == -1.0f)
    {
        pow = 100.0f;
    }
    else
    {
        pow = chk;
    }

    if (pow - inf_dsp.rmp_pow > 0.0f)
    {
        if (pow - inf_dsp.rmp_pow < 4.0f)
        {
            pow_add = pow - inf_dsp.rmp_pow;
        }
        else
        {
            pow_add = 4;
        }
    }
    else
    {
        if (pow - inf_dsp.rmp_pow < 0.0f)
        {
            if (inf_dsp.rmp_pow - pow < 4.0f)
            {
                pow_add = -(inf_dsp.rmp_pow - pow);
            }
            else
            {
                pow_add = -4;
            }
        }
        else
        {
            pow_add = 0;
        }
    }

    if (pow_add > 0)
    {
        if (inf_dsp.rmp_pow + pow_add < pow)
        {
            inf_dsp.rmp_pow += pow_add;
        }
        else
        {
            inf_dsp.rmp_pow = pow;
        }
    }
    else if (pow_add < 0)
    {
        if (inf_dsp.rmp_pow + pow_add > pow)
        {
            inf_dsp.rmp_pow += pow_add;

        }
        else
        {
            inf_dsp.rmp_pow = pow;
        }
    }

    cng_pow = inf_dsp.rmp_pow;

    tmp = 4.0f; if (cng_pow < 4.0f) tmp = (u_char)cng_pow;  // why the cast? maybe f_max is involved?

    cmn_pow = tmp * (SgCosf((inf_dsp.rmp_f * (PI * 2)) / 120.0f) - 1.0f) * 0.5f;

    if (inf_dsp.rmp_f < 120)
    {
        inf_dsp.rmp_f++;
    }
    else
    {
        inf_dsp.rmp_f = 0;
    }

    if (chk == -1.0f)
    {
        cmn_pow = 0;
    }

    f = cng_pow + cmn_pow;

    if (f != 0.0f)
    {
        lag_pow = f;
    }
    else
    {
        lag_pow = 0.0f;
    }

    if (out == 0)
    {
        cmn_alp = (inf_dsp.fndr_fade_alp * 128) / 100.0f;

        new_inf.rmp_alp = 0;
    }
    else
    {
        if (new_inf.rmp_alp + 8 < 0x80)
        {
            new_inf.rmp_alp += 8;
        }
        else
        {
            new_inf.rmp_alp = 0x80;
        }

        cmn_alp = new_inf.rmp_alp;
    }

    sml_alp = cmn_alp * cng_pow / 100.0f;
    lag_alp = cmn_alp * lag_pow / 100.0f;

    ene_num = 0;

    for (i = 0; i < 4; i++)
    {
        if (ene_wrk[i].sta & 0x1)
        {
            ene_num++;
        }
    }

#ifdef MATCHING_DECOMP
    {
        register int i asm("v1");

        for (i = 4; i >= 0; i--) // HACK: fixes wrong register
        {
            // debug code?
        }
    }
#endif

    if (ene_num != 0)
    {
        obj = 2;
    }
    else
    {
        obj = 1;
    }

    if (chk == 0.0f)
    {
        obj = 0;
    }

    if (out != 0)
    {
        PutSpriteYW(FND_RMP_CV, FND_RMP_CV, 0.0f, 0.0f, 0.0f, 0x808080, cmn_alp, 1.0f, 1.0f, 0, 0xff, 1, 0, 1);
    }

    PutSpriteYW(FND_RMP_BASE, FND_RMP_BASE, pos_x, pos_y, 0.0f, 0x808080, cmn_alp, 1.0f, 1.0f, 0, 0xff, 1, 0, 1);

    switch (obj)
    {
    case 0:
         // do nothing ...
    break;
    case 1:
        PutSpriteYW(FND_RMP_BLU_SML, FND_RMP_BLU_SML, pos_x, pos_y, 0.0f, 0x808080, sml_alp, 1.0f, 1.0f, 0, 0xff, 1, 1, 1);
        PutSpriteYW(FND_RMP_BLU_SML, FND_RMP_BLU_SML, pos_x, pos_y, 0.0f, 0x808080, sml_alp, 1.0f, 1.0f, 0, 0xff, 1, 1, 1);
        PutSpriteYW(FND_RMP_BLU_BIG, FND_RMP_BLU_BIG, pos_x, pos_y, 0.0f, 0x808080, lag_alp, 1.0f, 1.0f, 0, 0xff, 1, 1, 1);
        PutSpriteYW(FND_RMP_BLU_BIG, FND_RMP_BLU_BIG, pos_x, pos_y, 0.0f, 0x808080, lag_alp, 1.0f, 1.0f, 0, 0xff, 1, 1, 1);
    break;
    case 2:
        PutSpriteYW(FND_RMP_RED_SML, FND_RMP_RED_SML, pos_x, pos_y, 0.0f, 0x808080, sml_alp, 1.0f, 1.0f, 0, 0xff, 1, 1, 1);
        PutSpriteYW(FND_RMP_RED_SML, FND_RMP_RED_SML, pos_x, pos_y, 0.0f, 0x808080, sml_alp, 1.0f, 1.0f, 0, 0xff, 1, 1, 1);
        PutSpriteYW(FND_RMP_RED_BIG, FND_RMP_RED_BIG, pos_x, pos_y, 0.0f, 0x808080, lag_alp, 1.0f, 1.0f, 0, 0xff, 1, 1, 1);
        PutSpriteYW(FND_RMP_RED_BIG, FND_RMP_RED_BIG, pos_x, pos_y, 0.0f, 0x808080, lag_alp, 1.0f, 1.0f, 0, 0xff, 1, 1, 1);
    break;
    }

    PutSpriteYW(FND_RMP_GRS, FND_RMP_GRS, pos_x, pos_y, 0.0f, 0x808080, (int)(cmn_alp / 2), 1.0f, 1.0f, 0, 0xff, 1, 1, 1);
}

static void NewFndrBase(short int pos_x, short int pos_y)
{
    u_char alp;

    alp = (inf_dsp.fndr_fade_alp * 128) / 100.0f;

    PutSpriteYW(FND_FRAME_L, FND_FRAME_R, pos_x, pos_y, 0.0f, 0x808080, alp, 1.0f, 1.0f, 0x0, 0xff, 0x1, 0x0, 0x0);
    PutSpriteYW(FND_STK_GAUGE_L, FND_STK_GAUGE_R, pos_x, pos_y, 0.0f, 0x808080, alp, 1.0f, 1.0f, 0x0, 0xff, 0x1, 0x0, 0x0);
    PutSpriteYW(FND_SEAL_GAUGE, FND_SEAL_GAUGE, pos_x, pos_y, 0.0f, 0x808080, alp, 1.0f, 1.0f, 0x0, 0xff, 0x1, 0x0, 0x0);
}

static void BigBon(u_char bon_num, short int sx, short int sy, short int ex, short int ey, u_char mov_tim)
{
    int i;
    int j;
    float pr1;
    short int msx;
    short int msy;
    short int mex;
    short int mey;
    u_char mtm;
    u_char per;
    u_char alp;
    short pos_x;
    short pos_y;

    if (bon_num != new_inf.bon_bak)
    {
        if (new_inf.bon_bak < bon_num)
        {
            if (new_inf.bon_mov_cnt[bon_num] == 0)
            {
                new_inf.bon_mov_cnt[bon_num] = mov_tim;
                new_inf.bon_mov_sx[bon_num] = sx;
                new_inf.bon_mov_sy[bon_num] = sy;
                new_inf.bon_mov_ex[bon_num] = ex;
                new_inf.bon_mov_ey[bon_num] = ey;
                new_inf.bon_mov_tm[bon_num] = mov_tim;

                ZanzoLS(znz[bon_num]);
            }
        }

        new_inf.bon_bak = bon_num;
    }

    for (i = 0; i < 12; i++)
    {
        if (new_inf.bon_mov_cnt[i] != 0)
        {
            msx = new_inf.bon_mov_sx[i];
            msy = new_inf.bon_mov_sy[i];
            mex = new_inf.bon_mov_ex[i];
            mey = new_inf.bon_mov_ey[i];
            mtm = new_inf.bon_mov_tm[i];

            per = (new_inf.bon_mov_cnt[i] * 100) / (float)mtm;

            ZanzoLM(znz[i], per);

            new_inf.bon_mov_cnt[i]--;

            for (j = 0; j < 6; j++)
            {
                if (znz[i][j] != 0xff)
                {
                    pr1 = znz[i][j] / 100.0f;

                    alp = SgSinf(pr1) * 320.0f * SgSinf(pr1) * (25 - j * j) / 25.0f;

                    pos_x = (msx - mex) * pr1 * pr1 + mex;
                    pos_y = (msy - mey) * pr1 * pr1 + mey;

                    PutSpriteYW(FND_BIG_NUM_00 + i, FND_BIG_NUM_00 + i, pos_x, pos_y, 0.0f, 0x808080, alp / 10.0f, 3.0f, 3.0f, 0, 0xff, 0, 1, 1);
                }
            }
        }
    }
}

static void BigBon2(u_char bon_num, short int sx, short int sy, short int ex, short int ey, u_char mov_tim)
{
    int i;
    int j;
    u_char alp;
    float spd;
    short int msx;
    short int msy;
    short int mex;
    short int mey;
    u_char mtm;
    u_char per;
    short pos_x;
    short pos_y;
    float pr1;
    float rad;

    if (bon_num != new_inf.bon_bak)
    {
        if (new_inf.bon_bak < bon_num)
        {
            if (new_inf.bon_mov_cnt[bon_num] == 0)
            {
                new_inf.bon_mov_cnt[bon_num] = mov_tim;
                new_inf.bon_mov_sx[bon_num] = sx;
                new_inf.bon_mov_sy[bon_num] = sy;
                new_inf.bon_mov_ex[bon_num] = ex;
                new_inf.bon_mov_ey[bon_num] = ey;
                new_inf.bon_mov_tm[bon_num] = mov_tim;

                ZanzoLS(znz[bon_num]);
            }
        }

        new_inf.bon_bak = bon_num;
    }

    for (i = 0; i < 12; i++)
    {
        if (new_inf.bon_mov_cnt[i] != 0)
        {
            msx = new_inf.bon_mov_sx[i];
            msy = new_inf.bon_mov_sy[i];
            mex = new_inf.bon_mov_ex[i];
            mey = new_inf.bon_mov_ey[i];
            mtm = new_inf.bon_mov_tm[i];

            per = (new_inf.bon_mov_cnt[i] * 100) / (float)mtm;

            ZanzoLM(znz[i], per);

            new_inf.bon_mov_cnt[i]--;

            for (j = 0; j < 3; j++)
            {
                if (znz[i][j] != 0xff)
                {
                    rad = znz[i][j] * PI / 100.0f;
                    alp = SgSinf(rad) * 160.0f * (25 - j * j) / 25.0f;

                    if (znz[i][j] <= 70)
                    {
                        spd = (70 - znz[i][j]) / 70.0f;
                    }
                    else
                    {
                        spd = 0.0f;
                    }

                    pos_x = (mex - msx) * spd * spd + msx;
                    pos_y = (mey - msy) * spd * spd + msy;

                    PutSpriteYW(FND_BIG_NUM_00 + i, FND_BIG_NUM_00 + i, pos_x,pos_y, 0.0f, 0x808080, alp / 10.0f, 3.0f , 3.0f, 0, 0xff, 0, 1, 1);
                }
            }
        }
    }
}

static char GetEnePoint()
{
    int i;

    if (new_inf.pht_cnt_bak == ingame_wrk.pht_cnt || photo_wrk.score == 0)
    {
        return 0;
    }

    new_inf.pht_cnt_bak = ingame_wrk.pht_cnt;
    new_inf.pht_scr = photo_wrk.score;

    for (i = 0; i < 4; i++)
    {
        new_inf.ene_no[i] = 0xffff;
        new_inf.ene_dmg[i] = 0xffff;
    }

    new_inf.hit_hp_ovr_ene = 60;
    new_inf.hit_hp_ovr_ene_wait = 0;
    new_inf.dsp_dmg_flg = 0;
    new_inf.dsp_dmg_num = 0;
    new_inf.dsp_dmg_step = 0;
    new_inf.dsp_dmg_step2 = 0;

    for (i = 0; i < 4; i++)
    {
        if (photo_wrk.ene[i].no != 0xffff && photo_wrk.ene[i].dmg != 0)
        {
            new_inf.ene_no[new_inf.dsp_dmg_flg] = photo_wrk.ene[i].wrk_no;
            new_inf.ene_dmg[new_inf.dsp_dmg_flg] = photo_wrk.ene[i].dmg;
            new_inf.dsp_dmg_flg++;
        }
    }

    if (new_inf.dsp_dmg_flg != 0)
    {
        new_inf.dsp_dmg_num = new_inf.dsp_dmg_flg;
        new_inf.dsp_dmg_step = 1;
        new_inf.dsp_dmg_step2 = 1;
    }

    return 1;
}

static void MafubaInit(float t2_s1, float t2_s2)
{
    new_inf.mfb_drain = 0;
    new_inf.mfb_wait = 0;
    new_inf.mfb_effect = 0;
    new_inf.se_wait = 0;
    new_inf.se_num = 0;
    new_inf.mfb_lgt_alp = 0;
    new_inf.mfb_lfl_alp = 0;
    new_inf.mfb_num_alp = 0;
    new_inf.mfb_nfl_alp = 0;
    new_inf.mfb_num_siz_x = t2_s1 * 10.0f;
    new_inf.ene_dead_tmr = 0;
    new_inf.ene_dead_alp = 0;
    new_inf.mfb_stts = 0;
    new_inf.mfb_num_siz_y = new_inf.mfb_num_siz_x;

    ZanzoSS(&dmg_scr);

    new_inf.shot_mov = 0;
    new_inf.shot_pse = 0;
    new_inf.shot_lgt = 0;
    new_inf.shot_fad = 0;
}

static u_char Mafuba(short int pos_x, short int pos_y, u_char stts, u_char t1, u_char t2, u_char t3, u_char t4, u_char t5, u_char t1_sw, float t2_s1, float t2_s2)
{
    int j;
    u_char alp_max;

    alp_max = (inf_dsp.fndr_fade_alp * 128) / 100.0f;

    if (GetEnePoint() != 0)
    {
        MafubaInit(t2_s1, t2_s2);
    }

    switch (stts)
    {
    case 1:
        if (new_inf.mfb_stts == 0x0)
        {
            new_inf.mfb_stts = stts;
        }
    break;
    case 2:
        if (new_inf.mfb_stts == 0x1)
        {
            DameDameInit();

            new_inf.mfb_lgt_bak = new_inf.mfb_lgt_alp;
            new_inf.mfb_stts = stts;
            new_inf.mfb_effect = t2;
        }

        if (new_inf.mfb_stts == (0x4 | 0x2))
        {
            new_inf.mfb_stts = 0x0;
        }
    break;
    default:
        new_inf.mfb_stts = 0x0;
    break;
    }

    switch(new_inf.mfb_stts)
    {
    case 1:
        SndEffLoop();
        if (t1_sw != 0)
        {
            if (new_inf.mfb_drain < t1)
            {
                new_inf.mfb_drain++;
            }
            else {
                new_inf.mfb_drain = t1;
            }
        }
        else
        {
            if (new_inf.mfb_drain > 0)
            {
                new_inf.mfb_drain--;
            }
            else {
                new_inf.mfb_drain = 0;
            }
        }

        new_inf.mfb_lgt_alp = alp_max * (new_inf.mfb_drain / (float)t1);
        new_inf.mfb_lfl_alp = new_inf.mfb_lgt_alp;
    break;
    case 2:
        if (new_inf.mfb_drain > 0)
        {
            new_inf.mfb_drain--;
        }

        new_inf.mfb_lfl_alp = (alp_max * new_inf.mfb_drain) / (float)t1;

        if (new_inf.mfb_effect > 0)
        {
            new_inf.mfb_effect--;
        }
        else
        {
            new_inf.mfb_stts = 0x2 | 0x1;
            new_inf.mfb_wait = t3;
        }

        new_inf.mfb_lgt_alp = (new_inf.mfb_lgt_bak * new_inf.mfb_effect) / (float)t2;
        new_inf.mfb_num_alp = (alp_max * (t2 - new_inf.mfb_effect)) / (float)t2;
        new_inf.mfb_num_siz_x = new_inf.mfb_num_siz_y = ((t2_s2 - (t2_s2 - t2_s1) * (new_inf.mfb_effect / (float)t2) * (new_inf.mfb_effect / (float)t2)) * 10.0f);
    break;
    case 3:
        if (new_inf.mfb_wait != 0)
        {
            new_inf.mfb_wait--;
        }
        else
        {
            new_inf.mfb_stts = 0x4;
            new_inf.mfb_effect = t4;
        }
    break;
    case 4:
        if (new_inf.mfb_effect > 0)
        {
            new_inf.mfb_effect--;
        }
        else
        {
            new_inf.mfb_stts = 0x4 | 0x1;
            new_inf.mfb_drain = t5;
        }

        new_inf.mfb_nfl_alp = alp_max * SgSinf(new_inf.mfb_effect * PI / t4);
    break;
    case 5:
        if (new_inf.mfb_drain > 0)
        {
            new_inf.mfb_drain = new_inf.mfb_drain + -1;
        }
        else
        {
            new_inf.mfb_stts = 0x4 | 0x2;
            point_get_end = 1;
        }
        new_inf.mfb_num_alp = (alp_max * new_inf.mfb_drain) / (float)t5;
    break;
    }

    if (new_inf.mfb_lgt_alp != 0)
    {
        PutSpriteYW(FND_SEAL_FNT, FND_SEAL_FNT, pos_x, pos_y, 0.0f, 0x808080, new_inf.mfb_lgt_alp, 1.0f, 1.0f, 0, 0xff, 1, 1, 0);
    }

    if (new_inf.mfb_lfl_alp != 0)
    {
        PutSpriteYW(FND_SEAL_LIT_FLA, FND_SEAL_LIT_FLA, pos_x - 5, pos_y+0x10f, 0.0f, 0x808080, new_inf.mfb_lfl_alp * 0.5f, 1.0f, 1.0f, 0, 0xff, 1, 1, 0);
    }

    ZanzoSM(&dmg_scr, new_inf.mfb_stts, new_inf.mfb_num_alp, new_inf.mfb_num_siz_x, new_inf.mfb_num_siz_y, 1);

    for (j = 0; j < 6; j++)
    {
        if (dmg_scr.num2[j] != 0xff)
        {
            DameEffNumber(1, new_inf.pht_scr, pos_x, pos_y, dmg_scr.num1[j], (dmg_scr.num2[j] * (6 - j)) / 6.0f,  dmg_scr.num3[j],  dmg_scr.num4[j]);
        }
    }

    if (new_inf.mfb_nfl_alp != 0)
    {
        PutSpriteYW(FND_DM_FNT_FLA, FND_DM_FNT_FLA, pos_x, pos_y, 0.0f, 0x808080, new_inf.mfb_nfl_alp * 0.5f, 1.0f, 1.0f, 0, 0xff, 1, 1, 0);
    }

    return new_inf.mfb_stts;
}

static void MafubaFinish(short int pos_x, short int pos_y, u_char start, u_char time)
{
    u_char alp_max;

    alp_max = (inf_dsp.fndr_fade_alp * 128) / 100.0f;

    if (start != 0)
    {
        new_inf.ene_dead_tmr = time;
    }
    else if (new_inf.ene_dead_tmr > 0)
    {
        new_inf.ene_dead_tmr--;
    }

    if (new_inf.ene_dead_tmr != 0)
    {
        if (new_inf.ene_dead_alp + 16 < alp_max)
        {
            new_inf.ene_dead_alp += 16;
        }
        else
        {
            new_inf.ene_dead_alp = alp_max;
        }
    }
    else
    {
        if (new_inf.ene_dead_alp - 16 > 0)
        {
            new_inf.ene_dead_alp -= 16;
        }
        else
        {
            new_inf.ene_dead_alp = 0x0;
        }
    }

    if (new_inf.ene_dead_alp != 0)
    {
        PutSpriteYW(FND_SEAL_FNT, FND_SEAL_FNT, pos_x, pos_y, 0.0f, 0x808080, new_inf.ene_dead_alp, 1.0f, 1.0f, 0, 0xff, 1, 1, 0);
        PutSpriteYW(FND_SEAL_LIT_FLA, FND_SEAL_LIT_FLA, pos_x - 5, pos_y + 271, 0.0f, 0x808080, new_inf.ene_dead_alp * 0.5f, 1.0f, 1.0f, 0, 0xff, 1, 1, 0);
    }
}

static void DameEffNumber(u_char type, u_short num, short int pos_x, short int pos_y, u_char stts, u_char num_alp, u_char num_siz_x, u_char num_siz_y)
{
    u_char keta;
    u_char adj;
    float temp;

    if (num_siz_x == 0xff || num_siz_y == 0xff)
    {
        return;
    }

    keta = 1;

    temp = num;

    while (10.0f <= temp)
    {
        temp /= 10.0f;

        keta++;
    }

    if (num_alp != 0)
    {
        if (type == 0)
        {
            adj = (keta * 18) * 0.5f;

            PutNumberYW(13, num, pos_x - (adj - 135), pos_y + 92, num_siz_x / 10.0f, num_siz_y / 10.0f, 0x808080, num_alp, -1, keta, 0);
        }
        else
        {
            adj = (keta * 15) * 0.5f;

            PutNumberYW(7, num, pos_x - (adj - 59), pos_y + 327, num_siz_x / 10.0f, num_siz_y / 10.0f, 0x808080, num_alp, -1, keta, 0);
            PutSpriteYW(FND_FNT_SPIRIT, FND_FNT_SPIRIT, pos_x, pos_y, 0.0f, 0x808080, num_alp, num_siz_x / 10.0f, num_siz_y / 10.0f, 0, 0xff, 1, 0, 0);
        }
    }
}

static void SndEffLoop()
{
    if (new_inf.se_wait != 0x0)
    {
        new_inf.se_wait--;
    }
    else
    {
        if (new_inf.se_num < 5)
        {
            SeStartFix(17, 0, 0x1000, 0x1000, 0);
        }

        new_inf.se_wait = 5;
        new_inf.se_num++;
    }
}

static void ShowEneCtrl(short int pos_x, short int pos_y)
{
    u_char ene_no;
    u_char alp_max;
    u_short ene_hp;
    u_char ep_bar;

    ene_no = plyr_wrk.near_ene_no;

    alp_max = (inf_dsp.fndr_fade_alp * 128) / 100.0f;

    if (ene_no != 0xff)
    {
        if (ene_wrk[ene_no].hp <= 500)
        {
            ene_hp = ene_wrk[ene_no].hp;

            new_inf.bar_over = 0;
        }
        else
        {
            ene_hp = 500;

            new_inf.bar_over = ene_wrk[ene_no].hp + 12;
        }

        ep_bar = (ene_hp * 100) / 500.0f;
        new_inf.ep_bar = (ep_bar * 153) / 100.0f;

        new_inf.ep_tmr = 180;

        if (new_inf.ep_alp[0] + 8 < alp_max)
        {
            new_inf.ep_alp[0] += 8;
        }
        else
        {
            new_inf.ep_alp[0] = alp_max;
        }
    }
    else
    {
        if (new_inf.ep_tmr != 0)
        {
            new_inf.ep_tmr--;
        }
        else
        {
            if (new_inf.ep_alp[0] - 8 > 0)
            {
                new_inf.ep_alp[0] -= 8;
            }
            else
            {
                new_inf.ep_alp[0] = 0;
            }
        }
    }

    if (new_inf.dsp_dmg_flg != 0)
    {
        switch (EneDamegeCtrl(pos_x, pos_y))
        {
        case 0:
        default:
            return;
        break;
        case 1:
            // do nothing ...
        break;
        case 2:
            new_inf.ep_alp[0] = 0x0;
            new_inf.ep_bar = 0x0;
        break;
        }
    }

    ShowEnePower(new_inf.ep_bar, new_inf.ep_bar, pos_x, pos_y, new_inf.ep_alp[0], new_inf.bar_over);
}

static char EneDamegeCtrl(short int pos_x, short int pos_y)
{
    u_char alp_max;
    u_char ep_bar1;
    u_char ep_bar2;
    u_char dsp_no;
    u_short ene_no;
    u_short ene_dmg;
    u_short bar_red;
    u_short bar_blu;
    u_char hp_perc_blu;
    u_char hp_perc_red;

    alp_max = (inf_dsp.fndr_fade_alp * 128) / 100.0f;

    dsp_no = new_inf.dsp_dmg_num - new_inf.dsp_dmg_flg;
    ene_no = new_inf.ene_no[dsp_no];
    ene_dmg = new_inf.ene_dmg[dsp_no];

    switch(new_inf.dsp_dmg_step)
    {
    case 1:
        new_inf.dsp_dmg_alp = 0;
        new_inf.dsp_dmg_red = ene_dmg + ene_wrk[ene_no].hp;
        new_inf.dsp_dmg_blu = ene_wrk[ene_no].hp;
        new_inf.dsp_dmg_step = 2;
        new_inf.dsp_dmg_step2 = 1;
    break;
    case 2:
        if (new_inf.dsp_dmg_alp + 8 < alp_max)
        {
            new_inf.dsp_dmg_alp += 8;
        }
        else
        {
            new_inf.dsp_dmg_alp = alp_max;
            new_inf.dsp_dmg_step = 3;
        }
    break;
    case 3:
        if (new_inf.dsp_dmg_blu < new_inf.dsp_dmg_red)
        {
            new_inf.dsp_dmg_red--;
        }
        else if (new_inf.dsp_dmg_step2 == 7)
        {
            new_inf.dsp_dmg_red = new_inf.dsp_dmg_blu;
            new_inf.dsp_dmg_step2 = 0;

            if (new_inf.dsp_dmg_flg == 1)
            {
                if (plyr_wrk.near_ene_no == 0xff)
                {
                    new_inf.dsp_dmg_step = 6;
                }
                else if (ene_no == plyr_wrk.near_ene_no)
                {
                    new_inf.dsp_dmg_step = 5;
                }
            }
            else
            {
                new_inf.dsp_dmg_step = 4;
            }
        }
    break;
    case 4:
        if (new_inf.dsp_dmg_alp - 8 > 0)
        {
            new_inf.dsp_dmg_alp -= 8;
        }
        else
        {
            new_inf.dsp_dmg_alp = 0;
            new_inf.dsp_dmg_step = 5;
        }
    break;
    case 5:
        if (new_inf.dsp_dmg_flg != 0)
        {
            new_inf.dsp_dmg_flg--;
        }
        if (new_inf.dsp_dmg_flg != 0)
        {
            new_inf.dsp_dmg_step = 1;
        }
        else
        {
            new_inf.dsp_dmg_alp = 0;
            new_inf.dsp_dmg_step = 0;

            return 1;
        }
    break;
    case 6:
        if (new_inf.dsp_dmg_alp - 8 > 0)
        {
            new_inf.dsp_dmg_alp -= 8;
        }
        else
        {
            new_inf.dsp_dmg_alp = 0;
            new_inf.dsp_dmg_step = 7;
        }
    break;
    case 7:
        if (new_inf.dsp_dmg_flg != 0)
        {
            new_inf.dsp_dmg_flg--;
        }

        if (new_inf.dsp_dmg_flg != 0)
        {
            new_inf.dsp_dmg_step = 1;
        }
        else
        {
            new_inf.dsp_dmg_alp = 0;
            new_inf.dsp_dmg_step = 0;

            return 2;
        }
    break;
    }

    if (new_inf.dsp_dmg_blu > 500)
    {
        bar_blu = 500;

        new_inf.bar_over = new_inf.dsp_dmg_blu + 12;
    }
    else
    {
        bar_blu = new_inf.dsp_dmg_blu;

        new_inf.bar_over = 0;
    }

    hp_perc_blu = (bar_blu * 100) / 500.0f;
    ep_bar1 = (hp_perc_blu * 153) / 100.0f;

    if (new_inf.dsp_dmg_red > 500)
    {
        bar_red = 500;
    }
    else
    {
        bar_red = new_inf.dsp_dmg_red;
    }

    hp_perc_red = (bar_red * 100) / 500.0f;
    ep_bar2 = (hp_perc_red * 153) / 100.0f;

    ShowEnePower(ep_bar1, ep_bar2, pos_x, pos_y, new_inf.dsp_dmg_alp, new_inf.bar_over);

    switch(new_inf.dsp_dmg_step2)
    {
    case 1:
        new_inf.dmg_num_mov_tim = 20;
        new_inf.dmg_str_mov_tim = 20;
        new_inf.dmg_num_mov = 100;
        new_inf.dmg_num_alp = 0;
        new_inf.dmg_str_mov = 100;
        new_inf.dmg_str_alp = 0;
        new_inf.dmg_eff_alp = 0;
        new_inf.dmg_wait_tm = 0;
        new_inf.dsp_dmg_step2 = 2;
    break;
    case 2:
        if (new_inf.dmg_num_mov_tim >= 16)
        {
            new_inf.dmg_num_mov_tim--;
        }
        else
        {
            new_inf.dsp_dmg_step2 = 3;
        }

        new_inf.dmg_num_mov =  100.0f * (new_inf.dmg_num_mov_tim / 20.0f) * (new_inf.dmg_num_mov_tim / 20.0f);
        new_inf.dmg_num_alp = alp_max * (1.0f - new_inf.dmg_num_mov_tim / 20.0f) * (1.0f - new_inf.dmg_num_mov_tim / 20.0f);
    break;

    case 3:
        if (new_inf.dmg_num_mov_tim != 0)
        {
            new_inf.dmg_num_mov_tim--;
        }

        if (new_inf.dmg_str_mov_tim != 0)
        {
            new_inf.dmg_str_mov_tim--;
        }

        if (new_inf.dmg_num_mov_tim == 0 && new_inf.dmg_str_mov_tim == 0 )
        {
            new_inf.dmg_wait_tm = 10;
            new_inf.dsp_dmg_step2 = 4;
        }

        new_inf.dmg_num_mov = 100.0f * (new_inf.dmg_num_mov_tim / 20.0f) * (new_inf.dmg_num_mov_tim / 20.0f);
        new_inf.dmg_num_alp = alp_max * (1.0f - new_inf.dmg_num_mov_tim / 20.0f) * (1.0f - new_inf.dmg_num_mov_tim / 20.0f);
        new_inf.dmg_str_mov = 100.0f * (new_inf.dmg_str_mov_tim / 20.0f) * (new_inf.dmg_str_mov_tim / 20.0f);
        new_inf.dmg_str_alp = alp_max * (1.0f - new_inf.dmg_str_mov_tim / 20.0f) * (1.0f - new_inf.dmg_str_mov_tim / 20.0f);
    break;
    case 4:
        if (new_inf.dmg_wait_tm != 0)
        {
            new_inf.dmg_wait_tm--;
        }
        else
        {
            new_inf.dmg_eff_tim = 10;
            new_inf.dsp_dmg_step2 = 5;
        }
    break;
    case 5:
        if (new_inf.dmg_eff_tim != 0)
        {
            new_inf.dmg_eff_tim--;
        }
        else
        {
            new_inf.dsp_dmg_step2 = 6;
        }

        new_inf.dmg_eff_alp = alp_max * SgSinf((new_inf.dmg_eff_tim * PI) / 10.0f);
    break;
    case 6:
        if (new_inf.dmg_num_alp - 8 > 0)
        {
            new_inf.dmg_num_alp -= 8;
        }
        else
        {
            new_inf.dmg_num_alp = 0;
        }

        if (new_inf.dmg_str_alp - 8 > 0)
        {
            new_inf.dmg_str_alp -= 8;
        }
        else
        {
            new_inf.dmg_str_alp = 0;
        }

        if (new_inf.dmg_num_alp == 0 && new_inf.dmg_str_alp == 0)
        {
            new_inf.dsp_dmg_step2 = 7;
        }
    }

    if (new_inf.dmg_num_alp != 0)
    {
        DameEffNumber(0, new_inf.ene_dmg[dsp_no], pos_x + new_inf.dmg_num_mov, pos_y, 0, new_inf.dmg_num_alp, 10, 10);
    }

    if (new_inf.dmg_str_alp != 0)
    {
        PutSpriteYW(FND_DM_FNT_DAMAGE, FND_DM_FNT_DAMAGE, pos_x + new_inf.dmg_str_mov, pos_y, 0.0f, 0x808080, new_inf.dmg_str_alp, 1.0f, 1.0f, 0, 0xff, 1, 0, 0);
    }

    if (new_inf.dmg_eff_alp != 0)
    {
        DameEffNumber(0, new_inf.ene_dmg[dsp_no], pos_x, pos_y, 0, new_inf.dmg_eff_alp, 10, 10);

        PutSpriteYW(FND_DM_FNT_DAMAGE, FND_DM_FNT_DAMAGE, pos_x, pos_y, 0.0f, 0x808080, new_inf.dmg_eff_alp, 1.0f, 1.0f, 0, 0xff, 1, 0, 0);
    }

    return 0;
}

static void ShowEnePower(u_char blu_bar, u_char red_bar, short int pos_x, short int pos_y, u_char alp, u_char bar_over)
{
    float rgb_per;
    float alp_per;
    int ovr_ene_rgb;
    u_char red0;
    u_char alp0;

    if (alp != 0)
    {
        PutSpriteYW(65, 66, pos_x, pos_y, 0.0f, 0x808080, alp, 1.0f, 1.0f, 0, 0xff, 1, 0, 0);

#ifdef BUILD_EU_VERSION
        GayBar(143, pos_x, pos_y, alp, red_bar / 16.0f);
#else
        GayBar(147, pos_x, pos_y, alp, red_bar / 16.0f);
#endif
        GayBar(67, pos_x, pos_y, alp, blu_bar / 16.0f);

        if (bar_over & 0xFF)
        {
            rgb_per = (new_inf.hit_hp_ovr_ene / 60.0f);
            red0 = SgSinf(rgb_per * PI) * 32.0f;

            CLRtoRGB(&ovr_ene_rgb, red0 + 0x63, 0x4d, 0x43);

            alp_per = bar_over / 300.0f;
            alp0 = alp * alp_per * 0.5f + alp * 0.5f;

            PutSpriteYW(68, 68, pos_x, pos_y, 0.0f, ovr_ene_rgb, alp0, 9.5625f, 1.0f, 0, 0xff, 1, 0, 0);

            if (new_inf.hit_hp_ovr_ene_wait != 0)
            {
                new_inf.hit_hp_ovr_ene_wait--;
            }
            else if (new_inf.hit_hp_ovr_ene != 0)
            {
                new_inf.hit_hp_ovr_ene--;
            }
        }
        else
        {
            new_inf.hit_hp_ovr_ene = 0;
            new_inf.hit_hp_ovr_ene_wait = 0;
        }

        Koutakun(blu_bar, pos_x, pos_y, alp);
    }
}

static void Koutakun(u_char blu_bar, short int pos_x, short int pos_y, u_char alp)
{
    short int dsp_u;
    short int dsp_w;
    short int dsp_x;

    switch (new_inf.liner_w)
    {
    case 0:
        if (new_inf.liner_x + 1 < blu_bar + 128)
        {
            new_inf.liner_x++;
        }
        else
        {
            new_inf.liner_x = 0;
            new_inf.liner_w = 10;
        }
    break;
    default:
        if (new_inf.liner_w != 0)
        {
            new_inf.liner_w--;
        }
    break;
    }

    if (new_inf.liner_x < blu_bar)
    {
        dsp_u = 128 - new_inf.liner_x;
        dsp_w = new_inf.liner_x;
        dsp_x = 128;
    }
    else if (new_inf.liner_x < 128)
    {
        dsp_u = 128 - new_inf.liner_x;
        dsp_w = blu_bar;
        dsp_x = 128;
    }
    else if (new_inf.liner_x < blu_bar + 128)
    {
        dsp_u = 0;
        dsp_w = blu_bar - (new_inf.liner_x - 128);
        dsp_x = new_inf.liner_x;
    }

    DspKotaku(69, pos_x, pos_y, alp, dsp_u, dsp_w, dsp_x);
}

static void ShowMikuPower(short int pos_x, short int pos_y)
{
    u_char alp;
    u_short bar_h1;
    u_short bar_h2;

    alp = (inf_dsp.stts_fade_alp * 128) / 100.0f;

    bar_h1 = 284 * plyr_wrk.hp / 500.0f; // pixel value in range [0, 284]
    bar_h2 = 284 - bar_h1; // the rest of the 284 pixels

    CmnWindow(32, pos_x, pos_y, alp, 0x80);

    PolySquareYW(pos_x + 594,      0 + 140 + pos_y, 10, bar_h2, 0x991e2d, alp, 1.0f, 1.0f, 0x3000, 0, 0, 0);
    PolySquareYW(pos_x + 594, bar_h2 + 140 + pos_y, 10, bar_h1, 0x91bef0, alp, 1.0f, 1.0f, 0x3000, 0, 0, 0);
}

static void ShowMikuPower2(short int pos_x, short int pos_y)
{
    static FLSH_CORE flsh_life;
    u_char alp;
    u_short blu_bar;
    float scl_y;

    alp = (inf_dsp.stts_fade_alp * 128) / 100.0f;

    PutSpriteYW(LIFEBAR_WAKU_1, LIFEBAR_WAKU_5, pos_x, pos_y, 0.0f, 0x808080, alp, 1.0f, 1.0f, 0, 0xff, 1, 0, 1);

    blu_bar = (plyr_wrk.hp * 243) / 500.0f;

    if (new_inf.blu_bak == blu_bar)
    {
        if (blu_bar != new_inf.red_bar)
        {
            if (blu_bar < new_inf.red_bar - 1)
            {
                new_inf.red_bar--;
            }
            else
            {
                new_inf.red_bar = blu_bar;
            }
        }

        if (new_inf.fire_mask != 0)
        {
            new_inf.fire_mask--;
        }
    }
    else
    {
        if (new_inf.fire_mask < 7)
        {
            new_inf.fire_mask++;
        }

        new_inf.blu_bak = blu_bar;
    }

    GayBar2(110, 113, pos_x, pos_y - (short)(new_inf.red_bar - 243), (alp * 4) / 5, new_inf.red_bar / 243.0f);

    if (new_inf.fire_mask == 7)
    {
#ifdef BUILD_EU_VERSION
        JetCD(jet1, 20,  5, 10, 6, 141, 61, pos_x, pos_y - (short)(blu_bar - 243), pos_x + 596, pos_y - (short)(blu_bar - 243) + 174, alp);
        JetCD(jet2, 20, 24,  2, 1, 140, 62, pos_x, pos_y - (short)(blu_bar - 243), pos_x + 596, pos_y - (short)(blu_bar - 243) + 174, alp);
#else
        JetCD(jet1, 20,  5, 10, 6, 145, 61, pos_x, pos_y - (short)(blu_bar - 243), pos_x + 596, pos_y - (short)(blu_bar - 243) + 174, alp);
        JetCD(jet2, 20, 24,  2, 1, 144, 62, pos_x, pos_y - (short)(blu_bar - 243), pos_x + 596, pos_y - (short)(blu_bar - 243) + 174, alp);
#endif
    }

    GayBar2(106, 109, pos_x, pos_y - (short)(blu_bar - 243), (alp * 4) / 5, blu_bar / 243.0f);

    if (new_inf.fire_mask != 0)
    {
#ifdef BUILD_EU_VERSION
        SyonenJet(142, 35, pos_x, pos_y - (short)(blu_bar - 243), pos_x + 596, pos_y - (short)(blu_bar - 243) + 174, alp, 1.0f, new_inf.fire_mask / 10.0f);
#else
        SyonenJet(146, 35, pos_x, pos_y - (short)(blu_bar - 243), pos_x + 596, pos_y - (short)(blu_bar - 243) + 174, alp, 1.0f, new_inf.fire_mask / 10.0f);
#endif
    }

    if (plyr_wrk.hp == 500)
    {
        FlashStarYW(&flsh_life, alp, alp / 2, 120, 0);

        PutSpriteYW(LIFEBAR_WAKU_FLA1, LIFEBAR_WAKU_FLA1, pos_x, pos_y, 0.0f, 0x808080, flsh_life.alpha, 1.0f,  1.0f, 0, 0xff, 1, 1, 1);
        PutSpriteYW(LIFEBAR_WAKU_FLA2, LIFEBAR_WAKU_FLA2, pos_x, pos_y, 0.0f, 0x808080, flsh_life.alpha, 1.0f, 27.5f, 0, 0xff, 1, 1, 1);
        PutSpriteYW(LIFEBAR_WAKU_FLA3, LIFEBAR_WAKU_FLA3, pos_x, pos_y, 0.0f, 0x808080, flsh_life.alpha, 1.0f,  1.0f, 0, 0xff, 1, 1, 1);

        return;
    }

    if (blu_bar < new_inf.red_bar)
    {
        return;
    }

    switch (new_inf.liner_wait)
    {
    case 0:
        if (new_inf.liner_y + 4 < blu_bar)
        {
            new_inf.liner_y += 4;
        }
        else
        {
            new_inf.liner_y = 0;
            new_inf.liner_wait = 10;
        }
    break;
    default:
        if (new_inf.liner_wait != 0)
        {
            new_inf.liner_wait--;
        }
    break;
    }

    if (blu_bar == 0)
    {
        return;
    }

    scl_y = blu_bar * SgSinf((new_inf.liner_y * PI) / blu_bar) / 200.0f;

    PutSpriteYW(LIFEBAR_WAKU_FLA4, LIFEBAR_WAKU_FLA4, pos_x, pos_y - new_inf.liner_y, 0.0f, 0x808080, (int)(alp / 2), 4.0f / 23.0f, scl_y, 0, 0xff, 1, 0, 1);
}

static void DspKotaku(u_short chr, short int pos_x, short int pos_y, u_char alp, u_char dsp_u, u_char dsp_w, u_short dsp_x)
{
    DISP_SPRT ds;

    CopySprDToSpr(&ds, &spr_dat[chr]);

    ds.x += pos_x;
    ds.x += dsp_x;

    ds.y += pos_y;
    ds.u += dsp_u;
    ds.w = dsp_w;
    ds.alpha = alp * 0.5f;
    ds.alphar = SCE_GS_SET_ALPHA_1(SCE_GS_ALPHA_CS, SCE_GS_ALPHA_ZERO, SCE_GS_ALPHA_AS, SCE_GS_ALPHA_CD, 0);

    DispSprD(&ds);
}

static void GayBar(u_short chr, short int pos_x, short int pos_y, u_char alp, float scl)
{
    DISP_SPRT ds;

    CopySprDToSpr(&ds, &spr_dat[chr]);

    ds.x += pos_x;
    ds.y += pos_y;
    ds.scw = scl;
    ds.sch = 1.0f;
    ds.csx = spr_dat[chr].x + pos_x;
    ds.csy = spr_dat[chr].y + pos_y;
    ds.alpha = alp;
    ds.tex1 = SCE_GS_SET_TEX1_1(1, 0, SCE_GS_LINEAR, SCE_GS_LINEAR_MIPMAP_LINEAR, 0, 0, 0);

    DispSprD(&ds);
}

static void GayBar2(u_short top_chr, u_short end_chr, short int pos_x, short int pos_y, u_char alp, float scl)
{
    int i;
    DISP_SPRT ds;

    if (scl != 0.0f && alp != 0)
    {
        for (i = top_chr; i <= end_chr; i++)
        {
            CopySprDToSpr(&ds, &spr_dat[i]);

            ds.x += pos_x;
            ds.y += pos_y;
            ds.csx = spr_dat[top_chr].x + pos_x + spr_dat[top_chr].w * 0.5f;
            ds.csy = spr_dat[top_chr].y + pos_y;
            ds.scw = 1.0f;
            ds.sch = scl;
            ds.zbuf = SCE_GS_SET_ZBUF_1(0x8c, SCE_GS_PSMCT24, 1);
            ds.tex1 = SCE_GS_SET_TEX1_1(1, 0, SCE_GS_LINEAR, SCE_GS_LINEAR_MIPMAP_LINEAR, 0, 0, 0);
            ds.alpha = alp;

            DispSprD(&ds);
        }
    }
}

static void JetCD(JET_SET *ninjin, u_char num, u_char tim, u_char spd, u_char wid, u_short chr, u_char hgh, short int mov_h, short int mov_v, short int px, short int base, u_char alp)
{
    int i;
    float nin_scl_x;
    float min_scl_y;
    u_char min_alp;

    for (i = 0; i < num; i++)
    {
        if (ninjin[i].tim == 0)
        {
            ninjin[i].tim = rand() % tim + 1;
            ninjin[i].spd = rand() % spd + 1;
            ninjin[i].mov_x = rand() % wid + 1;
            ninjin[i].mov_y = 0;
            ninjin[i].tim_max = ninjin[i].tim;
        }
        else
        {
            ninjin[i].tim--;
            ninjin[i].mov_y += ninjin[i].spd;
        }

        min_alp = (alp * ninjin[i].tim) / (float)ninjin[i].tim_max;
        nin_scl_x = ninjin[i].tim / (float)ninjin[i].tim_max;
        min_scl_y = (1.0f - nin_scl_x) * 4.0f;

        SyonenJet(chr, hgh, mov_h + ninjin[i].mov_x, mov_v - ninjin[i].mov_y, px, base, min_alp, nin_scl_x, min_scl_y);
    }

    if (new_inf.issei_timer != 0)
    {
        new_inf.issei_timer--;
    }
    else
    {
        new_inf.issei_timer = 0xff;
    }
}

static void SyonenJet(u_short chr, u_char hgh, short int mov_h, short int mov_v, short int px, short int base, u_char alp, float scl_x, float scl_y)
{
    DISP_SPRT ds;
    short hh;

    CopySprDToSpr(&ds,spr_dat + (u_short)chr);

    if (ds.y + mov_v < base)
    {
        if (base - (ds.y + mov_v) < hgh)
        {
            hh = base - (ds.y + mov_v);
            ds.h = hh;
        }
    }

    ds.x += mov_h;
    ds.y += mov_v;
    ds.scw = scl_x;
    ds.sch = scl_y;
    ds.csx = ds.x + (ds.w >> 1);
    ds.csy = base;
    ds.alpha = alp;
    ds.zbuf = SCE_GS_SET_ZBUF_1(0x8c, SCE_GS_PSMCT24, 1);
    ds.tex1 = SCE_GS_SET_TEX1_1(1, 0, SCE_GS_LINEAR, SCE_GS_LINEAR_MIPMAP_LINEAR, 0, 0, 0);

    DispSprD(&ds);
}

static void TameKin2(short int pos_x, short int pos_y)
{
    int i;
    int j;
    u_char alp;
    float per;
    u_char charge_max;
    u_char ini_num;
    u_char top;
    u_char chrg_rot;
    u_char chrg_alp;
    u_char chrg_siz;
    u_char chrg_rgb;
    u_char chrg_flr;
    u_char init_alp;
    float rad;

    alp = (inf_dsp.fndr_fade_alp * 128) / 100.0f;

    charge_max = cam_custom_wrk.charge_max * 2 + 6;

    for (i = 0; i < charge_max; i++)
    {
        PutSpriteYW(FND_STK_NUM_BASE_00 + i, FND_STK_NUM_BASE_00 + i, pos_x + 143, pos_y + 389, 0.0f, 0x808080, alp, 1.0f, 1.0f, 0, 0xff, 1, 0, 0);
    }

    top = (12 - charge_max) / 2;

    ini_num = ini_charge_tbl[plyr_wrk.film_no];

    for (i = top; i < top + ini_num; i++)
    {
        new_inf.chrg_stp[i] = 3;
    }

    if (ini_num != 0)
    {
        init_alp = (SgCosf((new_inf.tm_max * (PI * 2)) / 60.0f) + 1.0f) * 32.0f + 64.0f;
    }
    else
    {
        init_alp = 0;
    }

    for (i = top; i < top + charge_max; i++)
    {
        if (i - top < plyr_wrk.charge_num)
        {
            if (new_inf.chrg_stp[i] == 0)
            {
                new_inf.chrg_rot[i] = 0;
                new_inf.chrg_flr[i] = 0;
                new_inf.chrg_stp[i] = 1;
                new_inf.dray_tmr[i] = 0;
            }
        }
        else
        {
            new_inf.chrg_stp[i] = 0;

            ZanzoSS(zanzo + i);
        }

        switch (new_inf.chrg_stp[i])
        {
        case 0:
            new_inf.chrg_flr[i] = 0;

            if (new_inf.chrg_rot[i] != 0)
            {
                new_inf.chrg_rot[i]--;
            }

            chrg_alp = (new_inf.chrg_rot[i] * 64) / 20.0f;
            chrg_rot = 0;
            chrg_siz = 100;
            chrg_rgb = 128;
            chrg_flr = 0;
        break;
        case 1:
            if (new_inf.chrg_rot[i] < 20)
            {
                new_inf.chrg_rot[i]++;
            }
            else
            {
                new_inf.chrg_rot[i] = 20;
                new_inf.chrg_stp[i] = 2;
            }

            per = new_inf.chrg_rot[i] / 20.0f;

            chrg_rot = 180.0f - 180.0f * per * per;
            chrg_alp = 64.0f * per * per;
            chrg_siz = 375.0f - 275.0f * per * per;
            chrg_rgb = 128.0f * per * per;
            chrg_flr = 0;
        break;
        case 2:
            chrg_rot = 0;
            chrg_alp = 64;
            chrg_siz = 100;
            chrg_rgb = 128;

            if (new_inf.chrg_flr[i] < 20)
            {
                new_inf.chrg_flr[i]++;
            }
            else
            {
                new_inf.chrg_flr[i] = 20;
                new_inf.chrg_stp[i] = 3;
            }

            rad = (new_inf.chrg_flr[i] * PI) / 20.0f;
            chrg_flr = SgSinf(rad) * 128.0f;
        break;
        case 3:
            if (new_inf.chrg_stp[charge_max + top - 1] != 3)
            {
                chrg_rot = 0;
                chrg_alp = 64;
                chrg_siz = 100;
                chrg_rgb = 128;
                chrg_flr = 0;
            }
            else
            {
                chrg_rot = 0;
                chrg_siz = 100;
                chrg_rgb = 128;
                chrg_alp = (SgCosf((new_inf.tm_max * 6.2831855f) / 60.0f) + 1.0f) * 32.0f + 64.0f;
                chrg_flr = 0;
            }
        break;
        }

        if (new_inf.dray_tmr[i] == 0)
        {
            if (i - top < ini_num)
            {
                ZanzoSM(zanzo + i, chrg_rot, init_alp, chrg_siz, 0xff, 0);
            }
            else
            {
                ZanzoSM(zanzo + i, chrg_rot, chrg_alp, chrg_siz, 0xff, 0);
            }

            new_inf.dray_tmr[i] = 2;
        }
        else
        {
            new_inf.dray_tmr[i]--;
        }

        for (j = 0; j < 6; j++)
        {
            AhoBon(i + 26, pos_x + 143, pos_y + 389, zanzo[i].num1[j], chrg_rgb, zanzo[i].num2[j], zanzo[i].num3[j], i - top, ini_num);
        }

        BakBon(i + 26, pos_x + 143, pos_y + 389, chrg_flr);
    }

    if (new_inf.chrg_stp[charge_max + top - 1] == 3 || ini_num != 0)
    {
        if (new_inf.tm_max < 60)
        {
            new_inf.tm_max++;
        }
        else
        {
            new_inf.tm_max = 0;
        }
    }
    else
    {
        new_inf.tm_max = 0;
    }
}

static void ZanzoSS(ZAN *zz)
{
    int i;

    for (i = 0; i < 6; i++)
    {
        zz->num1[i] = 0xFF;
        zz->num2[i] = 0xFF;
        zz->num3[i] = 0xFF;
        zz->num4[i] = 0xFF;
    }
}

static void ZanzoSM(ZAN *zz, u_char num1, u_char num2, u_char num3, u_char num4, u_char mode)
{
    int i;

    for (i = 5; i > 0; i--)
    {
        zz->num1[i] = zz->num1[i-1];
        zz->num2[i] = zz->num2[i-1];
        zz->num3[i] = zz->num3[i-1];
        zz->num4[i] = zz->num4[i-1];
    }

    zz->num1[0] = num1;
    zz->num2[0] = num2;
    zz->num3[0] = num3;
    zz->num4[0] = num4;

    if (mode == 0)
    {
        for (i = 5; i > 0; i--)
        {
            if (zz->num1[i] == zz->num1[0])
            {
                zz->num1[i] = 0xff;
            }
            if (zz->num2[i] == zz->num2[0])
            {
                zz->num2[i] = 0xff;
            }
            if (zz->num3[i] == zz->num3[0])
            {
                zz->num3[i] = 0xff;
            }
            if (zz->num4[i] == zz->num4[0])
            {
                zz->num4[i] = 0xff;
            }
        }
    }
    else
    {
        for (i = 5; i > 0; i--)
        {
            if (zz->num3[i] == zz->num3[0])
            {
                zz->num3[i] = 0xff;
            }

            if (zz->num4[i] == zz->num4[0])
            {
                zz->num4[i] = 0xff;
            }
        }
    }
}

static void ZanzoLS(u_char *c)
{
    int i;

    for (i = 0; i < 6; i++)
    {
        c[i] = 0xFF;
    }
}

static void ZanzoLM(u_char *zz, u_char per)
{
    int i;

    for (i = 5; i > 0; i--)
    {
        zz[i] = zz[i-1];
    }

    zz[0] = per;

    for (i = 5; i > 0; i--)
    {
        if (zz[i] == per)
        {
            zz[i] = 0xff;

            per = zz[0];
        }
    }
}

static void AhoBon(u_short chr, short int px, short int py, u_char rot, u_char rgb, u_char alp, u_char siz, u_char now_num, u_char ini_num)
{
    float xo;
    float yo;
    DISP_SPRT ds;

    if (rot != 0xff && alp != 0xff && siz != 0xff)
    {
        CopySprDToSpr(&ds, &spr_dat[chr]);

        ds.x += px;
        ds.y += py;

        xo = ds.x + ds.w * 0.5f;
        yo = ds.y + ds.h * 0.5f;

        ds.crx = xo;
        ds.cry = yo;

        ds.rot = -rot;

        ds.csx = xo;
        ds.csy = yo;

        ds.scw = siz / 100.0f;
        ds.sch = siz / 100.0f;

        ds.alphar = SCE_GS_SET_ALPHA_1(SCE_GS_ALPHA_CS, SCE_GS_ALPHA_ZERO, SCE_GS_ALPHA_AS, SCE_GS_ALPHA_CD, 0);
        ds.alpha = alp * 0.5f;

        if (inf_dsp.lucky7 != 0)
        {
            ds.r = 0x80;
            ds.g = 0;
            ds.b = 0;
        }
        else if (now_num < ini_num)
        {
            ds.r = 0xff;
            ds.g = 0x7f;
            ds.b = 0;
        }
        else
        {
            ds.r = rgb;
            ds.g = rgb;
            ds.b = rgb;
        }

        ds.tex1 = SCE_GS_SET_TEX1_1(1, 0, SCE_GS_LINEAR, SCE_GS_LINEAR_MIPMAP_LINEAR, 0, 0, 0);

        DispSprD(&ds);
    }
}

static void BakBon(u_short chr, short int px, short int py, u_char flr)
{
    float xo;
    float yo;
    DISP_SPRT ds;
    int rgb;

    CopySprDToSpr(&ds, &spr_dat[chr]);

    ds.x += px;
    ds.y += py;

    xo = ds.x + ds.w * 0.5f;
    yo = ds.y + ds.h * 0.5f;

    if (inf_dsp.lucky7 != 0)
    {
        rgb = 0x800000;
    }
    else
    {
        rgb = 0x808080;
    }

    if (flr != 0)
    {
        PutSpriteYW(FND_SEAL_LIT_FLA, FND_SEAL_LIT_FLA, xo - 63.0f, yo - 63.0f, 0.0f, rgb, flr, 25.0f / 63.0f, 25.0f / 63.0f, 0, 0xff, 1, 1, 0);
    }
}

static void PhotoScoreDisp(int count, int wait)
{
    u_char alp_table2[31] = {
         0,  3,  3,  4,  4,  5,  5,  6,  8, 10, 12, 13, 14,  14,  15, 15,
        16, 17, 18, 20, 23, 26, 30, 36, 44, 54, 66, 80, 96, 112, 128,
    };
    static int e_flow = 0xff;
    static float fcnt;
    float fnum;
    float fslide;
    u_char alp;
    int cnt;
    float fw;
    float f;
    DISP_SPRT ds;
    SPRT_DAT raze_sprt = {
        .tex0 = SCE_GS_SET_TEX0_1(0xa0900 / 64, 2, SCE_GS_PSMT8, 6, 4, 1, SCE_GS_MODULATE, 0xa0a80 / 64, SCE_GS_PSMCT32, 0, 0, 1),
        .u = 0,
        .v = 0,
        .w = 60,
        .h = 16,
        .x = 0,
        .y = 0,
        .pri = 0x1000,
        .alpha = 0x80,
    };
    short int pos_x;

    fnum = 24.0f;

    fslide = 4.0f;

    if (count + 1 == wait)
    {
        e_flow = 0;
    }

    CopySprDToSpr(&ds, &raze_sprt);

    switch (e_flow)
    {
    case 0:
        fcnt = (1.0f - SgSinf(0.0f)) * fnum;
        e_flow++;
    break;
    case 1:
        // do nothing ...
    break;
    case 2:
    default:
        return;
    break;
    }

    if (wait - count < 10)
    {
        alp = alp_table2[(wait - count) * 3];
        cnt = (wait - count) * 9;
    }
    else if (count <= 10)
    {
        alp = alp_table2[count * 3];
        cnt = count * 9;
    }
    else
    {
        alp = alp_table2[30];
        cnt = 90;
    }

    fw = fcnt * 2.0f + 1.0f;

    fw = fw != (int)fw ? (int)fw + 1 : fw; // (positive) round up to integer

    ds.alpha = alp;

    for (f = 0.0f; f < fcnt * 2.0f + 1.0f; f += 1.0f)
    {
        pos_x = (f - (fw - 1.0f) * 0.5f) * fslide + 273.0f;

        NumberDisp(photo_wrk.score, pos_x, 43, 2, 0xff, 0xff, 0xff, alp, 0, 7, 0);

        ds.x = pos_x + 0xaf;
        ds.y = 55.0f;

        DispSprD(&ds);
    }

    fcnt = (1.0f - SgSinf(DEG2RAD(cnt))) * fnum;

    if (count < 2)
    {
        e_flow++;
    }
}

char point_get_end = 0;

void InfoDispPause()
{
    inf_dsp.disp_pause = 1;
}

void InfoDispRestart()
{
    inf_dsp.disp_pause = 0;
}

static void BukiyouMan(short int px, short int py)
{
    u_char alp_max;
    u_char num;
    u_char buki_alp;
    u_char alp;

    alp_max = (inf_dsp.fndr_fade_alp * 128) / 100.0f;

    PutSpriteYW(FND_WEP_GAUGE, FND_WEP_GAUGE, px, py, 0.0f, 0x808080, alp_max, 1.0f, 1.0f, 0, 0xff, 1, 0, 0);

    if (cam_custom_wrk.set_sub == 0xff)
    {
        return;
    }

    if (CheckCamSubEffect() != 0)
    {
        new_inf.buki_use_flg = 1;
    }
    else
    {
        new_inf.buki_use_flg = 0;
    }

    PutSpriteYW(FND_WEP_FNT_0B + cam_custom_wrk.set_sub, FND_WEP_FNT_0B + cam_custom_wrk.set_sub, px, py, 0.0f, 0x808080, (alp_max * 4) / 5.0f, 1.0f, 0.98f, 0, 0xff, 1, 0, 0);

    if (poss_item[5] > 9)
    {
        num = 9;
    }
    else
    {
        num = poss_item[5];
    }

    PutNumberYW(12, num, px + 570, py + 369, 1.0f, 1.0f, 0x808080, alp_max, -1, 1, 0);

    if (new_inf.buki_use_flg != 0)
    {
        if (new_inf.buki_use_alp + 0x10 >= alp_max)
        {
            new_inf.buki_use_alp = alp_max;
        }
        else
        {
            new_inf.buki_use_alp += 16;
        }
    }
    else
    {
        if (new_inf.buki_use_alp - 0x10 > 0)
        {
            new_inf.buki_use_alp -= 16;
        }
        else
        {
            new_inf.buki_use_alp = 0;
        }
    }

    if (new_inf.buki_use_flg != 0 && alp_max > new_inf.buki_use_alp)
    {
        new_inf.buki_use_flr = new_inf.buki_use_alp;
    }
    else
    {
        if (new_inf.buki_use_flr - 16 > 0)
        {
            new_inf.buki_use_flr -= 16;
        }
        else
        {
            new_inf.buki_use_flr = 0;
        }
    }

    if (new_inf.buki_use_alp == alp_max)
    {
        alp = (int)((SgCosf(new_inf.buki_use_tmr * (PI * 2) / 60.0f) + 1.0f) * 50.0f * 0.5f) + 50;

        buki_alp = new_inf.buki_use_alp * alp / 100.0f;

        if (new_inf.buki_use_tmr < 60)
        {
            new_inf.buki_use_tmr++;
        }
        else
        {
            new_inf.buki_use_tmr = 0;
        }
    }
    else
    {
        buki_alp = new_inf.buki_use_alp;

        new_inf.buki_use_tmr = 0;
    }

    if (new_inf.buki_use_alp != 0)
    {
        PutSpriteYW(cam_custom_wrk.set_sub + FND_WEP_FNT_0A, cam_custom_wrk.set_sub + FND_WEP_FNT_0A, px, py, 0.0f, 0x808080, buki_alp, 1.0f, 0.98f, 0, 0xff, 1, 1, 0);
    }

    if (new_inf.buki_use_flr != 0)
    {
        PutSpriteYW(FND_WEP_GAUGE_FLA, FND_WEP_GAUGE_FLA, px, py, 0.0f, 0x808080, new_inf.buki_use_alp, 1.0f, 1.0f, 0, 0xff, 1, 1, 0);
    }
}

static void DameDameInit()
{
    int i;

    for (i = 0; i < 4; i++)
    {
        new_inf.shot_kind[i] = 0xff;
        new_inf.shot_kind2 = 0xff;
    }

    new_inf.shot_num = 0;

    if (photo_wrk.bonus_sta & 0x20)
    {
        new_inf.shot_kind[new_inf.shot_num] = 1;
        new_inf.shot_num++;
    }

    if (photo_wrk.bonus_sta & 0x10)
    {
        new_inf.shot_kind[new_inf.shot_num] = 2;
        new_inf.shot_num++;
    }

    if (photo_wrk.bonus_sta & 0x8)
    {
        new_inf.shot_kind[new_inf.shot_num] = 3;
        new_inf.shot_num++;
    }

    if (photo_wrk.bonus_sta & 0x2)
    {
        new_inf.shot_kind[new_inf.shot_num] = 4;
        new_inf.shot_num++;
    }

    if (photo_wrk.bonus_sta & 0x4)
    {
        new_inf.shot_kind[new_inf.shot_num] = 5;
        new_inf.shot_num++;
    }

    if (photo_wrk.bonus_sta & 0x80)
    {
        new_inf.shot_kind[new_inf.shot_num] = 6;
        new_inf.shot_num++;
    }

    if (photo_wrk.bonus_sta & 0x100)
    {
        new_inf.shot_kind2 = 1;
    }

    if (photo_wrk.bonus_sta & 0x200)
    {
        new_inf.shot_kind2 = 2;
    }

    if (photo_wrk.bonus_sta & 0x400)
    {
        new_inf.shot_kind2 = 3;
    }

    if (photo_wrk.bonus_sta & 0x800)
    {
        new_inf.shot_kind2 = 4;
    }

    new_inf.shot_mov = 20;
    new_inf.shot_pse = 30;
    new_inf.shot_lgt = 20;
    new_inf.shot_fad = 20;
}

static void GekisyaBoyGeisyaGirl(short int px, short int py)
{
    int i;
    u_char alp1;
    u_char alp2;
    u_char alp_max;
    u_char mov;
    short int y[10];

    alp_max = (inf_dsp.fndr_fade_alp * 128) / 100.0f;

    if (new_inf.shot_mov != 0)
    {
        new_inf.shot_mov--;

        mov = 100.0f * (1.0f - new_inf.shot_mov / 20.0f) * (1.0f - new_inf.shot_mov / 20.0f);
        alp1 = alp_max * (1.0f - new_inf.shot_mov / 20.0f) * (1.0f - new_inf.shot_mov / 20.0f);
        alp2 = 0;
    }
    else if (new_inf.shot_pse != 0)
    {
        new_inf.shot_pse--;

        mov = 100;
        alp1 = 0x80;
        alp2 = 0;
    }
    else if (new_inf.shot_lgt != 0)
    {
        new_inf.shot_lgt--;

        mov = 100;
        alp1 = 0x80;
        alp2 = alp_max * SgSinf((new_inf.shot_lgt * PI) / 20.0f);
    }
    else if (new_inf.shot_fad != 0)
    {
        new_inf.shot_fad--;

        mov = 100;
        alp1 = (alp_max * new_inf.shot_fad) / 20.0f;
        alp2 = 0;
    }
    else
    {
        return;
    }

    for (i = 0; i < 7; i++)
    {
        y[i] = -1;
    }

    switch(new_inf.shot_num)
    {
    case 1:
        y[0] = 313;
    break;
    case 2:
        y[0] = 313;
        y[1] = 329;
    break;
    case 3:
        y[0] = 313;
        y[1] = 329;
        y[2] = 345;
    break;
    case 4:
        y[0] = 313;
        y[1] = 329;
        y[2] = 345;
        y[3] = 361;
    break;
    case 5:
        y[0] = 313;
        y[1] = 329;
        y[2] = 345;
        y[3] = 361;
        y[4] = 377;
    // forgot to break?
    case 6:
        y[0] = 313;
        y[1] = 329;
        y[2] = 345;
        y[3] = 361;
        y[4] = 377;
        y[5] = 393;
    break;
    }

    i = 0;

    SetSprFile(PL_LIFE_PK2_ADDRESS);

    while (y[i] != -1)
    {
        switch(new_inf.shot_kind[i])
        {
        case 1:
            if (alp1 != 0)
            {
                PutSpriteYW(FND_END, FND_END, mov + px, y[i] + py, 0.0f, 0x808080, alp1, 1.0f, 1.0f, 0, 0xff, 1, 0, 0);
            }

            if (alp2 != 0)
            {
                PutSpriteYW(FND_END, FND_END, mov + px, y[i] + py, 0.0f, 0x808080, alp2, 1.0f, 1.0f, 0, 0xff, 1, 1, 0);
            }
        break;
        case 2:
            if (alp1 != 0)
            {
                PutSpriteYW(FND_FNT_TRIPLE1, FND_FNT_TRIPLE2, mov + px, y[i] + py, 0.0f, 0x808080, alp1, 1.0f, 1.0f, 0, 0xff, 1, 0, 0);
            }

            if (alp2 != 0)
            {
                PutSpriteYW(FND_FNT_TRIPLE1, FND_FNT_TRIPLE2, mov + px, y[i] + py, 0.0f, 0x808080, alp2, 1.0f, 1.0f, 0, 0xff, 1, 1, 0);
            }
        break;
        case 3:
            if (alp1 != 0)
            {
                PutSpriteYW(FND_FNT_DOUBLE1, FND_FNT_DOUBLE2, mov + px, y[i] + py, 0.0f, 0x808080, alp1, 1.0f, 1.0f, 0, 0xff, 1, 0, 0);
            }

            if (alp2 != 0)
            {
                PutSpriteYW(FND_FNT_DOUBLE1, FND_FNT_DOUBLE2, mov + px, y[i] + py, 0.0f, 0x808080, alp2, 1.0f, 1.0f, 0, 0xff, 1, 1, 0);
            }
        break;
        case 4:
            if (alp1 != 0)
            {
                PutSpriteYW(FND_DM_FNT_CLOSE, FND_DM_FNT_CLOSE, mov + px, y[i] + py, 0.0f, 0x808080, alp1, 1.0f, 1.0f, 0, 0xff, 1, 0, 0);
            }

            if (alp2 != 0)
            {
                PutSpriteYW(FND_DM_FNT_CLOSE, FND_DM_FNT_CLOSE, mov + px, y[i] + py, 0.0f, 0x808080, alp2, 1.0f, 1.0f, 0, 0xff, 1, 1, 0);
            }
        break;
        case 5:
            if (alp1 != 0)
            {
                PutSpriteYW(FND_DM_FNT_CORE, FND_DM_FNT_CORE, mov + px, y[i] + py, 0.0f, 0x808080, alp1, 1.0f, 1.0f, 0, 0xff, 1, 0, 0);
            }

            if (alp2 != 0)
            {
                PutSpriteYW(FND_DM_FNT_CORE, FND_DM_FNT_CORE, mov + px, y[i] + py, 0.0f, 0x808080, alp2, 1.0f, 1.0f, 0, 0xff, 1, 1, 0);
            }
        break;
        case 6:
            if (alp1 != 0)
            {
                PutSpriteYW(FND_DM_FNT_SPCIAL, FND_DM_FNT_SPCIAL, mov + px, y[i] + py, 0.0f, 0x808080, alp1, 1.0f, 1.0f, 0, 0xff, 1, 0, 0);
            }

            if (alp2 != 0)
            {
                PutSpriteYW(FND_DM_FNT_SPCIAL, FND_DM_FNT_SPCIAL, mov + px, y[i] + py, 0.0f, 0x808080, alp2, 1.0f, 1.0f, 0, 0xff, 1, 1, 0);
            }
        break;
        }

        i++;
    }

    y[7] = 297;

    switch (new_inf.shot_kind2)
    {
    case 1:
        if (alp1 != 0)
        {
            PutSpriteYW(FND_FNT_IMPACT1, FND_FNT_IMPACT2, mov + px, y[7] + py, 0.0f, 0x808080, alp1, 1.0f, 1.0f, 0, 0xff, 1, 0, 0);
        }

        if (alp2 != 0)
        {
            PutSpriteYW(FND_FNT_IMPACT1, FND_FNT_IMPACT2, mov + px, y[7] + py, 0.0f, 0x808080, alp2, 1.0f, 1.0f, 0, 0xff, 1, 1, 0);
        }
    break;
    case 2:
        if (alp1 != 0)
        {
            PutSpriteYW(FND_FNT_SLOW, FND_FNT_SLOW, mov + px, y[7] + py, 0.0f, 0x808080, alp1, 1.0f, 1.0f, 0, 0xff, 1, 0, 0);
        }

        if (alp2 != 0)
        {
            PutSpriteYW(FND_FNT_SLOW, FND_FNT_SLOW, mov + px, y[7] + py, 0.0f, 0x808080, alp2, 1.0f, 1.0f, 0, 0xff, 1, 1, 0);
        }
    break;
    case 3:
        if (alp1 != 0)
        {
            PutSpriteYW(FND_FNT_FIND, FND_FNT_FIND, mov + px, y[7] + py, 0.0f, 0x808080, alp1, 1.0f, 1.0f, 0, 0xff, 1, 0, 0);
        }

        if (alp2 != 0)
        {
            PutSpriteYW(FND_FNT_FIND, FND_FNT_FIND, mov + px, y[7] + py, 0.0f, 0x808080, alp2, 1.0f, 1.0f, 0, 0xff, 1, 1, 0);
        }
    break;
    case 4:
        if (alp1 != 0)
        {
            PutSpriteYW(FND_FNT_BIND, FND_FNT_BIND, mov + px,y[7] + py, 0.0f, 0x808080, alp1, 1.0f, 1.0f, 0, 0xff, 1, 0, 0);
        }

        if (alp2 != 0)
        {
            PutSpriteYW(FND_FNT_BIND, FND_FNT_BIND, mov + px, y[7] + py, 0.0f, 0x808080, alp2, 1.0f, 1.0f, 0, 0xff, 1, 1, 0);
        }
    break;
    }
}

static char KanKinoSyogai(short int pos_x, short int pos_y)
{
    int i;
    char dir;
    u_char alp;

    if (cam_custom_wrk.set_spe == 0xff)
    {
        return 1;
    }

    if (cam_custom_wrk.set_spe != 0)
    {
        return 1;
    }

    alp = (inf_dsp.fndr_fade_alp * 128) / 100.0f;

    for (i = 0; i < 4; i++)
    {
        PutSpriteYW(KAN_BU + i, KAN_BU + i, pos_x, pos_y, 0.0f, 0x808080, alp, 1.0f, 1.0f, 0x0, 0xff, 0x1, 0x0, 0x1);
    }

    if (plyr_wrk.spe1_dir & 0x1)
    {
        PutSpriteYW(KAN_FU, KAN_FU, pos_x, pos_y, 0.0f, 0x808080, alp, 1.0f, 1.0f, 0x0, 0xff, 0x1, 0x1, 0x1);
    }

    if (plyr_wrk.spe1_dir & 0x2)
    {
        PutSpriteYW(KAN_FR, KAN_FR, pos_x, pos_y, 0.0f, 0x808080, alp, 1.0f, 1.0f, 0x0, 0xff, 0x1, 0x1, 0x1);
    }

    if (plyr_wrk.spe1_dir & 0x4)
    {
        PutSpriteYW(KAN_FD, KAN_FD, pos_x, pos_y, 0.0f, 0x808080, alp, 1.0f, 1.0f, 0x0, 0xff, 0x1, 0x1, 0x1);
    }

    if (plyr_wrk.spe1_dir & 0x8)
    {
        PutSpriteYW(KAN_FL, KAN_FL, pos_x, pos_y, 0.0f, 0x808080, alp, 1.0f, 1.0f, 0x0, 0xff, 0x1, 0x1, 0x1);
    }

    return dir;
}

int isDispLamp()
{
    if (
        ingame_wrk.mode == INGAME_MODE_GAME_OVER ||
        efcnt[12].dat.uc8[0] != 0 ||
        (ingame_wrk.game == 0 && poss_item[9] == 0) ||
        eff_filament_off != 0 ||
        (ingame_wrk.game == 1 && isBattleModeGame() == 0) ||
        ingame_wrk.mode == INGAME_MODE_SPD_OPT ||
        plyr_wrk.mode == PMODE_MSG_DISP ||
        (ingame_wrk.game == 0 && ingame_wrk.difficult != 0)
    )
    {
        return 0;
    }

    return 1;
}
