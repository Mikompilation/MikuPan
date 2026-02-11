#ifndef INGAME_ENEMY_ENE_CTL_H
#define INGAME_ENEMY_ENE_CTL_H

#include "typedefs.h"
#include "ingame/ig_glob.h"

typedef struct {
	u_int attr1;
	u_short dst_gthr;
	u_char way_gthr;
	u_char atk_ptn;
	u_char wspd;
	u_char rspd;
	u_short hp;
	u_short atk_rng;
	u_short hit_rng;
	u_short chance_rng;
	short int hit_adjx;
	u_short atk_p;
	u_short atk_h;
	u_char atk;
	u_char atk_tm;
	u_short mdl_no;
	u_short anm_no;
	u_int se_no;
	u_int adpcm_no;
	int dead_adpcm;
	u_short point_base;
	u_char hint_pic;
	u_char aura_alp;
	u_char area[6];
	u_short dir;
	u_short px;
	short int py;
	u_short pz;
} ENE_DAT;

typedef struct {
	u_char dat_no;
	u_char soul_no;
	u_short dir;
	u_short px;
	short int py;
	u_short pz;
	u_short adpcm_tm;
	int adpcm_no;
	u_short rng;
	u_short mdl_no;
	u_short anm_no;
	u_short point_base;
	u_int se_no;
	int se_foot;
} AENE_INFO_DAT;

#include "ingame/enemy/move_ctl.h"

typedef struct {
	u_short dmg;
	u_short hit_rng;
	u_short mdl_no;
	u_short cond;
} FLY_DATA;

typedef struct {
	u_short sta;
	u_char ene;
	u_char reserve;
	MOVE_BOX move_box;
	FLY_DATA *dat;
	void *ep;
	sceVu0FVECTOR adjp;
	sceVu0FVECTOR adjmv;
} FLY_WRK;

typedef struct {
	int flow;
	float rrad[64];
	float rrotx[64];
	float rroty[64];
	float racc[64];
	float rbrk[64];
	u_char rscl[64];
} SPAWAY;

struct ENE_WRK {
	u_int sta;
	u_int sta2;
	u_short hp;
	u_short dmg;
	u_short dmg_old;
	u_short atk_tm;
	u_char type;
	u_char dat_no;
	u_char act_no;
	u_char anime_no;
	MOVE_BOX move_box;
	ENE_DAT *dat;
	MPOS mpos;
	LIGHT_PACK mylight;
	float dist_p_e;
	float dist_c_e;
	float pra_per;
	float pr_anime;
	u_short pra_time;
	u_short bloop;
	u_char bjob_no;
	u_char bpos_no;
	u_char bwait_time;
	u_char recog_tm;
	P_INT bcomm_add;
	long int bcomm_add_top;
	u_char tr_rate;
	u_char ajob_no;
	u_char apos_no;
	u_char await_time;
	P_INT acomm_add;
	long int acomm_add_top;
	void *pdf;
	float d_pd;
	void *pdf2;
	float d_pd2;
	void *nee;
	float nee_rate;
	float nee_size;
	u_int nee_col;
	int se_omen[2];
	sceVu0FVECTOR adjp;
	sceVu0FVECTOR pp;
	sceVu0FVECTOR sp;
	sceVu0FVECTOR si;
	sceVu0FVECTOR bep;
	u_short in_finder_tm;
	u_char area[6];
	u_short tr_time;
	u_char tr_max;
	u_char dmg_type;
	u_char room_no;
	char se_area_no;
	u_char ani_reso;
	u_char ani_reso_tm;
	short int fp[2];
	short int fp2[3][2];
	u_char eroot[2];
	u_char plight_svo;
	u_char slight_svo;
	u_char plight_svm[2];
	u_char slight_svm[2];
	float pa_radius;
	AENE_INFO_DAT *aie;
	u_short stm_slow;
	u_short stm_view;
	u_short stm_stop;
	u_char tr_rate2;
};

extern ENE_DAT jene_dat0[];
extern AENE_INFO_DAT aene_info_dat0[];
extern FLY_DATA fly_dat[];
extern ENE_DAT jene_dat1[];
extern ENE_DAT fene_dat1[];
extern AENE_INFO_DAT aene_info_dat1[];
extern ENE_DAT jene_dat2[];
extern ENE_DAT fene_dat2[];
extern AENE_INFO_DAT aene_info_dat2[];
extern ENE_DAT jene_dat3[];
extern ENE_DAT fene_dat3[];
extern AENE_INFO_DAT aene_info_dat3[];
extern ENE_DAT jene_dat4[];
extern ENE_DAT fene_dat4[];
extern AENE_INFO_DAT aene_info_dat4[];
extern ENE_DAT *jene_dat[];
extern ENE_DAT *fene_dat[];
extern AENE_INFO_DAT *aene_info_dat[];
extern u_char req_dmg_ef[3];
extern int ene_dead_load;
extern int ene_dead_mode;
extern int erootd0[20][3];
extern int erootd1[20][3];
extern int erootd2[20][3];
extern u_char er_max_tbl[3];
extern FLY_WRK fly_wrk[10];
extern ENE_WRK ene_wrk[4]; // Belongs in glob.h


void EneCtrlMain();
void EneEntryChk(u_char no);
void JEneEntry(u_char wrk_no, u_char dat_no);
void FEneEntry(u_char wrk_no, u_char dat_no);
void AEneEntry(u_char wrk_no, u_char dat_no);
void EneRule(ENE_WRK *ew);
void EneCondCtrl(ENE_WRK *ew);
void EneAuraCtrl(ENE_WRK *ew);
void InitEneWrk(u_char wrk_no);
int EneWrkReleaseChk(ENE_WRK *ew);
void FuyuEneRelease();
void EnePosInfoSet(ENE_WRK *ew);
void EneSeOmenChk(ENE_WRK *ew, u_char id);
void EneMoveCtrl(ENE_WRK *ew);
u_char EnePRecogChkChk(ENE_WRK *ew);
u_char EnePRecogChk(ENE_WRK *ew, u_char *act_no);
u_char HitDmgChk(ENE_WRK *ew);
u_char CaptureOnChk1(ENE_WRK *ew);
u_char CaptureOnChk3(ENE_WRK *ew);
u_char CaptureOnChk4(ENE_WRK *ew);
u_char CaptureOnChk2(ENE_WRK *ew);
u_char PlyrSeFootChk(ENE_WRK *ew);
void EneActIniChk(ENE_WRK *ew, u_char view_chk, u_char act_no);
void EneActIniSet(ENE_WRK *ew, u_char act_no);
void EneActSet(ENE_WRK *ew, u_char act_no);
void EneBlinkDataSet(ENE_WRK *ew);
void EneARatioDataSet(ENE_WRK *ew, u_char anime_no);
void EneActRule(ENE_WRK *ew);
int EneActPreferChk(ENE_WRK *ew, u_char *act_no);
void EneAniResolutionCtrl(ENE_WRK *ew);
void EneAtkCtrl(ENE_WRK *ew);
u_char EneDmgChk(ENE_WRK *ew);
void ClrEneStaDmg(ENE_WRK *ew);
u_char PlyrOutAreaChk(ENE_WRK *ew);
u_char EneTrtryChk(ENE_WRK *ew, u_char room_no);
void EneLightCtrl(ENE_WRK *ew);
void EneInDispChk(ENE_WRK *ew);
void EneBlinkPosSet(ENE_WRK *ew);
void FlyRule(FLY_WRK *fw);
int FlyPlyrHitChk(FLY_WRK *fw, sceVu0FVECTOR tv);
void FlyAtkHit(FLY_WRK *fw);
void EneMinDmgCtrl(ENE_WRK *ew);
void EneNormalEffectCtrl(ENE_WRK *ew);
char EndAutoEne(u_char type);
void ReqEneSpeakAdpcmCnt(u_short time);
void EneSpeakAdpcmCtrl(ENE_WRK *ew);
void GhostDeadInit(int wrk_no);
void GhostDeadMain();
int GhostDeadLoadEndJudge();
int EnemyUseJudge(u_char ene_type);
int ZeroGhostBattleJudge();
void EneAdpcmPlayChk(ENE_WRK *ew);

#endif // INGAME_ENEMY_ENE_CTL_H
