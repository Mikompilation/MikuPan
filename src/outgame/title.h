#ifndef OUTGAME_TITLE_H
#define OUTGAME_TITLE_H

#include "typedefs.h"

typedef struct {
	int load_id;
	u_char mode;
	u_char sub_mode;
	u_char next_mode;
	u_char csr;
	u_char load_side;
} TITLE_WRK;

typedef struct {
    u_short timer;
    u_char mode;
#ifdef BUILD_EU_VERSION
    u_char no_disp;
#endif
} TTL_DSP_WRK;

typedef struct { // 0x10
    /* 0x0 */ float cnt;
    /* 0x4 */ float alp;
    /* 0x8 */ float spd;
    /* 0xc */ u_char alpmx;
    /* 0xd */ u_char colmx;
    /* 0xe */ u_short type;
} OUT_DITHER_STR;

#ifdef BUILD_EU_VERSION
typedef struct {
    int logo_flow;
    int lang_sel_flow;
    int cnt;
    u_char alp;
    u_char dummy;
    u_short load_id;
} TITLE_SYS;
#endif

#include "outgame/mode_slct.h"

// extern SPRT_DAT title_sprt[11];
// extern SPRT_DAT font_sprt[20];
extern int opening_movie_type;
extern TITLE_WRK title_wrk;
/* sbss 357be8 */ extern /*static*/ TTL_DSP_WRK ttl_dsp;
/* bss 402e40 */ extern /*static*/ OUT_DITHER_STR out_dither;

int TitleUseRoomBackground(void);
void TitleSetUseRoomBackground(int enabled);
int TitleDebugWindowVisible(void);
void TitleSetDebugWindowVisible(int enabled);
void TitleCtrl();
void TitleWaitMode();
void TitleStartSlct();
void TitleStartSlctYW(u_char pad_off, u_char alp_max);
void TitleLoadCtrl();
void TitleSelectMode();
void TitleSelectModeYW(u_char pad_off, u_char alp_max);
void TitleSelectDifficultyYW();
void NewGameInit();
void LoadGameInit();
void InitOutDither();
void MakeOutDither();
void DispOutDither();
int AlbmDesignLoad(u_char side, u_char type);

#ifdef BUILD_EU_VERSION
void InitTecmotLogo();
int SetTecmoLogo();
void InitSelectLanguage();
int SetSelectLanguage(int cur_pos);
#endif

#endif // OUTGAME_TITLE_H
