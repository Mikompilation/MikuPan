#ifndef INGAME_PLYR_PLYR_CTL_H
#define INGAME_PLYR_PLYR_CTL_H

#include "typedefs.h"
#include "main/glob.h"


enum {
	PWARP_MODE_NO_REQ = 0,
	PWARP_MODE_IN_REQ = 1,
	PWARP_MODE_ROOM_LOAD = 2,
	PWARP_MODE_OUT_REQ = 3,
	PWARP_MODE_END = 4
};

typedef struct { // 0x3
	/* 0x0 */ u_char mode;
	/* 0x1 */ u_char time;
	/* 0x2 */ u_char req_no;
} PWARP_WRK;

typedef struct { // 0x8
	/* 0x0 */ u_char room;
	/* 0x1 */ u_char floor;
	/* 0x2 */ u_short pos_x;
	/* 0x4 */ short int pos_y;
	/* 0x6 */ u_short pos_z;
} PWARP_DAT;

extern float photo_rng_tbl[];
extern u_short photo_frame_tbl[0][2];
extern float circle_radius_tbl[];
extern u_char charge_max_tbl[];
extern u_char ini_charge_tbl[];
extern u_short photo_dmg_tbl[];
// extern float photo_dist_ratio[0];
// extern float photo_center_ratio[0];
// extern float photo_charge_ratio[0];
// extern int pa_radius_set;
// extern float paz;
// extern float spd_ld;
// extern float spd_sd;
// extern float spd_su;
// extern float spd_sdr;
// extern float spd_sur;
// extern PWARP_DAT pwarp_dat[0];
extern u_char plyr_vib_time;
extern PWARP_WRK pwarp_wrk;

#define PI (float)0.0031415927
#define DEG2RAD(x) ((float)(x)*PI/180.0f)


void PlyrCtrlMain();
int PlyrDoorOpenChk();
u_char ShortPauseChk();
void PlyrCondChk();
void PlyrDmgChk();
void PlyrVibCtrl(u_char time);
void PlyrFinderModeChk();
void FinderInSet();
void FinderEndSet();
void ClrEneSta();
void PlyrHeightCtrl(float *tv);
void PlyrNormalCtrl();
void PlyrSpotMoveCtrl();
void PlyrFinderCtrl();
void PlyrCamRotCngChk();
void EneHPchk();
void FModeScreenEffect();
void PlyrDmgCtrl();
u_char LeverGachaChk();
void PlyrFinderEnd();
void PlyrNAnimeCtrl();
void SetPlyrAnime(u_char anime_no, u_char frame);
void PlyrTrembleChk();
void ReqPlyrHPdown(u_short deg, u_char prio);
void PlyrHPdwonCtrl();
void PlyrSpotLightOnChk();
void SetPlyrSpotLight(u_char id);
void PlyrSubAtkChk();
u_char PSAchk0();
u_char PSAchk1();
u_char PSAchk3();
u_char PSAchk2();
void PSAchk4(u_char id);
void PSAchk5();
void PlyrPhotoChk();
void EneFrameHitChk();
void PlyrChargeCtrl(u_char req, ENE_WRK *ew);
void PhotoFlyChk();
int FrameInsideChk(float *tv, float *tx, float *ty);
int FrameInsideChkFurn(FURN_WRK *fw, float *degree, u_int fsta);
int FrameInsideChkRare(int wrk_no, float *degree);
u_short PhotoDmgChk();
u_short PhotoDmgChkSub(ENE_WRK *ew);
void PhotoPointSet();
void PhotoPointChkEne();
void PhotoPointChkFurn();
void PhotoPointChkRare();
void PhotoPointCulcEne(ENE_WRK *ew, PHOTO_WRK *pw);
void PhotoPointCulcFurn(FURN_WRK *fw, PHOTO_WRK *pw, float dist, float degree, u_int stts);
void PhotoPointCulcRare(u_char wrk_no, PHOTO_WRK *pw, float dist, float degree);
void PlyrMpRecoverChk(u_int recov);
void PlyrFModeMoveCtrl();
void PlyrActionChk();
void PlyrNModeMoveCtrl();
void PlyrHitTurnChk(MOVE_BOX *mb, float *tv);
u_char PlyrSpecialMoveChk2(float *mv);
void PlyrSpecialMoveChk(float *mv);
void PlyrPosSet(MOVE_BOX *mb,float *tv);
void PlyrKonwakuMove(MOVE_BOX *mb, float *tv);
void PlyrMovePad(MOVE_BOX *mb, float *tv);
void GetMoveSpeed(float *tv);
void PlyrMovePadV(MOVE_BOX *mb, float *tv);
void CngPlyrRotRapid(float rot0);
void PlyrMovePadFind(float *tv);
float GetMovePad(u_char id);
int MovePadEnableChk(u_char *dir_save);
void PadInfoTmpSave(u_char *dir_save, u_char dir_now, float *rot_save, float rot_now);
u_char PlyrMoveStaChk(float pad_chk);
u_int PlyrLeverInputChk();
u_char PlyrMoveHitChk(MOVE_BOX *mb, float *tv, u_char id);
void InitPhotoWrk();
void InitSubjectWrk(SUBJECT_WRK *sw);
void PlyrBattleChk();
void PlyrLightSet();
void PlyrMessageDisp();
int ReqPlyrSpeAnime(u_char anime_no, u_char frame);
void PlyrSpeAnimeCtrl();
void PlyrDWalkTmCtrl();
float GetEnePowerDegree();
float CulcEP(float *v0, float *v1);
float CulcEP2(float *v0, float *v1);
int ChkPhotoAble();
int SearchRareEne();
int SearchFurnHint();
int GetFurnHintPos(FURN_WRK *fw, float *tv, u_int *fsta);
float GetPlyrSpd();
int PlyrNeckDirectionChk(float *p);
u_char NeckTargetEneChk(float *p);
u_char NeckTargetItemChk(float *p);
u_char NeckTargetDoorChk(float *p);
u_char PlyrNoticeObjectChk(float *ov, float *dist);
void ReqPlayerStop(u_char frame);
void PlayerWarpReq(u_char dat_no);
int PlayerWarpCtrl();
void PlayerWarpRoomLoadReq();
int ShutterChanceChk();
void PlayerWarpReq2(u_char dat_no);

#endif // INGAME_PLYR_PLYR_CTL_H
