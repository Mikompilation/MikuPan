#ifndef GRAPHICS_GRAPH2D_EFFECT_H
#define GRAPHICS_GRAPH2D_EFFECT_H

#include "typedefs.h"

#include "graphics/graph2d/tim2.h"

typedef struct {
	Q_WORDDATA dat;
	void *pnt[6];
	float fw[3];
	u_int z;
	u_int flow;
	u_int cnt;
	u_int in;
	u_int keep;
	u_int out;
	u_int max;
} EFFECT_CONT;

typedef struct {
	u_char sw;
	u_char alp;
	int scl;
	int rot;
	float x;
	float y;
} BLUR_STR;

typedef struct {
	u_char sw;
	u_char type;
	u_char col;
	u_char alp;
} CONTRAST_STR;

typedef struct {
	u_char sw;
	u_char alp;
} FFRAME_STR;

typedef struct {
	u_char sw;
	u_char type;
	float spd;
	float alp;
	u_char amax;
	u_char cmax;
} DITHER_STR;

typedef struct {
	u_char sw;
	u_char type;
	u_char rate;
} DEFORM_STR;

typedef struct {
	u_char sw;
	u_char col;
	u_char alp;
	u_char alp2;
} NEGA_STR;

typedef struct {
	u_char sw;
} MONO_STR;

typedef struct {
	BLUR_STR bl;
	CONTRAST_STR cn;
	FFRAME_STR ff;
	DITHER_STR dt;
	DEFORM_STR df;
	NEGA_STR ng;
	MONO_STR mn;
} SBTSET;

extern SPRT_DAT effdat[]; // [60] = {...}
extern SPRT_DAT camdat[]; // [14] = {...}
extern int change_efbank; // = 0
extern int eff_blur_off; // = 0
extern int effect_disp_flg; // = 0
extern int eff_filament_off; // = 0
extern int now_buffer[2]; // = {0,0}
extern int eff_dith_off; // = 0
extern int stop_effects; // = 0
extern int monochrome_mode; // = 0
extern int magatoki_mode; // = 0
extern int shibata_set_off; // = 0
extern SBTSET msbtset; // = {0}
extern EFFECT_CONT efcnt[64]; // = {0}
extern EFFECT_CONT efcntm[48]; // = {0}
extern EFFECT_CONT efcnt_cnt[64]; // = {0}
extern EFFECT_CONT efcntm_cnt[48]; // = {0}
extern int look_debugmenu;

void InitEffects();
void InitEffectsEF();
void EffectEndSet();

void* SetZDepthEffect(int fl);
void* SetZDepth2Effect(int fl);
void* SetDitherEffect(int fl, int type, float alpha, float speed, int alpha_max, int color_max, u_int in = 0, u_int keep = 0, u_int out = 0);
void* SetDither2Effect(int fl, int type, float alpha, float speed);
void* SetBlurEffect(int id, int fl, u_char *alpha, u_int scale, u_int rotation, float x, float y);
void* SetDeformEffect(int fl, int type, int rate, u_int in = 0, u_int keep = 0, u_int out = 0);
void* SetFocusEffect(int fl, int volume);
void* SetOverlapEffect(int fl, int alpha);
void* SetFadeFrameEffect(int fl, int alpha, u_int color);
void* SetRenzFlareEffect(int fl, int type, void *position, void *rotation);
void* SetHazeEffect(int fl);
void* SetBlackFilterEffect(int fl, int alpha);
void* SetNegaEffect(int fl, int color, int alpha, u_char *alpha2);
void* SetNegaEffectTimed(int fl, int color, int alpha, u_int in, u_int keep, u_int out);
void* SetContrastEffect(int id, int fl, int color, int alpha);
void* SetMagatokiEffect(int fl, u_int in, u_int out, void *flag);
void* SetSpiritEffect(int fl, int flow, void *position, int r1, int g1, int b1, float scale1, int r2, int g2, int b2, float scale2, void *alpha);
void* SetPBlurEffect(int fl, int alpha);
void* SetFireEffect(int fl, int flow, void *position, int r1, int g1, int b1, float scale1, int r2, int g2, int b2, float scale2);
void* SetFire2Effect(int fl, int flow, void *position, int r1, int g1, int b1, float scale1, int r2, int g2, int b2, float scale2, float scale_rate);
void* SetHaloEffect(int fl, int type, void *position, int r, int g, int b, float scale, int blend);
void* SetRippleEffect(int fl, int type, int time, void *position);
void* SetRipple2Effect(int fl, int type, int time, int r, int g, int b, float scale, float alpha_rate, void *position, void *rotation, int count = 0);
void* SetPartsDeformEffect(int fl, int type, u_int max, float scale_x, float scale_y, void *position, u_int in, u_int keep, u_int out, float *effect, float *speed, float *rate, float *trail_rate);
void* SetEneInEffect(int fl, int type, int alpha, float scale, void *position, u_int in, u_int keep, u_int out);
void* SetEneOutEffect(int fl, int index, int value, float scale);
void* SetDustEffect(int fl, void *position);
void* SetWaterdropEffect(int fl, void *position, int state, float speed, u_int max, int r, int g, int b);
void* SetSunshineEffect(int fl, void *position1, void *position2, void *rotation, u_int max, float scale_x, float scale_y, int r, int g, int b);
void* SetEneFireEffect(int fl, int type, void *position, void *position2, void *color, void *size, u_int life, void *rate);
void* SetTorchEffect(int fl, int type, void *position, float *scale_rate, float *alpha_rate);
void* SetSmokeEffect(int fl, void *position);
void* SetNegaCircleEffect(int fl, float x, float y, float size, int alpha, u_int in, u_int keep, u_int out);
void* SetEneFaceEffect(int fl, int type, int rotation, float x, float y, float z);
void* SetFaceSpiritEffect(int fl, void *position, int r, int g, int b, float *alpha, int index);

void ResetEffects(void *p);
int SearchEmptyEffectBuf();
void EffectZSort();
void EffectZSort2();
void EffectZSort3();
int CheckEffectScrBuffer(int eno);
void ResetEffectScrBuffer(int eno);
void EffectControl(int no);
void SetBlurOff();
void SetDebugMenuSwitch(int sw);
int MikuPan_EffectDebugCount(void);
const char* MikuPan_EffectDebugEnumName(int id);
const char* MikuPan_EffectDebugFunctionName(int id);
const char* MikuPan_EffectDebugNote(int id);
int MikuPan_EffectDebugCanToggle(int id);
int MikuPan_EffectDebugEnabled(int id);
void MikuPan_EffectDebugSetEnabled(int id, int enabled);
void MikuPan_EffectDebugSetAll(int enabled);
void MikuPan_EffectDebugApply(void);
void pblur();
void TmpEffectOff(int id);
void tes_p10();
void tes_p11();
void tes_p20();
void tes_p21();
void tes_p3();

#endif // GRAPHICS_GRAPH2D_EFFECT_H
