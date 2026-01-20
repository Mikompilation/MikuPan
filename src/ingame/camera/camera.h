#ifndef INGAME_CAMERA_CAMERA_H
#define INGAME_CAMERA_CAMERA_H

#include "typedefs.h"

typedef struct {
	sceVu0FVECTOR p;
	sceVu0FVECTOR i;
	float roll;
	float fov;
	float nearz;
	float farz;
	float ax;
	float ay;
	float cx;
	float cy;
	float zmin;
	float zmax;
	float pad[2];
	sceVu0FMATRIX vs;
	sceVu0FMATRIX vc;
	sceVu0FMATRIX vcv;

	/// World View matrix
	sceVu0FMATRIX wv;

	/// World -> Screen matrix
	sceVu0FMATRIX ws;

	/// World Clip matrix
	sceVu0FMATRIX wc;

    /// World Clip View matrix
	sceVu0FMATRIX wcv;

	/// Look direction vectors
	sceVu0FVECTOR zd;
	sceVu0FVECTOR yd;
} SgCAMERA;

typedef struct {
	u_char type;
	u_char id;
	u_short p0[3];
	u_short p1[3];
	u_short p2[3];
	u_short p3[3];
	float roll[2];
	float fov[2];
} MAP_CAM_DAT;

typedef struct
{ // 0xc
    /* 0x0 */ u_short no;
    /* 0x2 */ u_short no_old;
    /* 0x4 */ u_char kind;
    /* 0x5 */ u_char type;
    /* 0x6 */ u_char change;
    /* 0x7 */ u_char pad;
    /* 0x8 */ MAP_CAM_DAT *mcd;
} MAP_CAM_INFO;

typedef struct
{ // 0x40
  /* 0x00 */ sceVu0FVECTOR i;
  /* 0x10 */ sceVu0FVECTOR p;
  /* 0x20 */ sceVu0FVECTOR rot_y;
  /* 0x30 */ sceVu0FVECTOR rot_x;
} CAM_ID_MOVE;

extern u_int ncam_000[];
extern u_int ncam_001[];
extern u_int ncam_002[];
extern u_int ncam_003[];
extern u_int ncam_006[];
extern u_int ncam_007[];
extern u_int ncam_008[];
extern u_int ncam_009[];
extern u_int ncam_010[];
extern u_int ncam_011[];
extern u_int ncam_012[];
extern u_int ncam_013[];
extern u_int ncam_014[];
extern u_int ncam_015[];
extern u_int ncam_016[];
extern u_int ncam_021[];
extern u_int ncam_022[];
extern u_int ncam_023[];
extern u_int ncam_024[];
extern u_int ncam_025[];
extern u_int ncam_026[];
extern u_int *ncam_comple_dat1[];
extern u_int *ncam_comple_dat2[];
extern u_int *ncam_comple_dat3[];
extern u_int *ncam_comple_dat4[];
extern u_int bcam_000[];
extern u_int *bcam_comple_dat1[];
extern u_int *bcam_comple_dat2[];
extern u_int *bcam_comple_dat3[];
extern u_int *bcam_comple_dat4[];
extern u_int dcam_000[];
extern u_int *dcam_comple_dat1[];
extern u_int *dcam_comple_dat2[];
extern u_int *dcam_comple_dat3[];
extern u_int *dcam_comple_dat4[];
extern u_short dc0000[];
extern u_short dc0010[];
extern u_short dc0020[];
extern u_short dc0030[];
extern u_short dc0040[];
extern u_short dc0050[];
extern u_short dc0060[];
extern u_short dc0070[];
extern u_short dc0080[];
extern u_short dc0090[];
extern u_short dc0100[];
extern u_short dc0110[];
extern u_short dc0120[];
extern u_short dc0130[];
extern u_short dc0140[];
extern u_short dc0150[];
extern u_short dc0160[];
extern u_short dc0170[];
extern u_short dc0180[];
extern u_short dc0190[];
extern u_short dc0200[];
extern u_short dc0210[];
extern u_short dc0220[];
extern u_short dc0230[];
extern u_short dc0240[];
extern u_short dc0250[];
extern u_short dc0260[];
extern u_short dc0270[];
extern u_short dc0280[];
extern u_short dc0290[];
extern u_short dc0300[];
extern u_short dc0310[];
extern u_short dc0320[];
extern u_short dc0330[];
extern u_short dc0340[];
extern u_short dc0350[];
extern u_short dc0360[];
extern u_short dc0370[];
extern u_short dc0380[];
extern u_short dc0390[];
extern u_short dc0400[];
extern u_short dc0410[];
extern u_short dc0420[];
extern u_short dc0430[];
extern u_short dc0440[];
extern u_short dc0450[];
extern u_short dc0460[];
extern u_short dc0470[];
extern u_short dc0480[];
extern u_short dc0490[];
extern u_short dc0500[];
extern u_short dc0510[];
extern u_short *dc_no_tbl0[];
extern u_short dc1000[];
extern u_short dc1010[];
extern u_short dc1020[];
extern u_short dc1030[];
extern u_short dc1040[]; 
extern u_short dc1050[];
extern u_short dc1060[];
extern u_short dc1070[];
extern u_short dc1080[];
extern u_short dc1090[];
extern u_short dc1100[];
extern u_short dc1110[];
extern u_short dc1120[];
extern u_short dc1130[];
extern u_short dc1140[];
extern u_short dc1150[];
extern u_short dc1160[];
extern u_short dc1170[];
extern u_short dc1180[];
extern u_short dc1190[];
extern u_short dc1200[];
extern u_short dc1210[];
extern u_short dc1220[];
extern u_short dc1230[];
extern u_short dc1240[];
extern u_short dc1250[];
extern u_short dc1260[];
extern u_short dc1270[];
extern u_short dc1280[];
extern u_short dc1290[];
extern u_short *dc_no_tbl1[];
extern u_short dc2000[];
extern u_short dc2010[];
extern u_short dc2020[];
extern u_short dc2030[];
extern u_short dc2040[];
extern u_short dc2050[];
extern u_short dc2060[];
extern u_short dc2070[];
extern u_short dc2080[];
extern u_short dc2090[];
extern u_short dc2100[];
extern u_short dc2110[];
extern u_short dc2120[];
extern u_short dc2130[];
extern u_short dc2140[];
extern u_short dc2150[];
extern u_short dc2160[];
extern u_short dc2170[];
extern u_short dc2180[];
extern u_short dc2190[];
extern u_short dc2200[];
extern u_short dc2210[];
extern u_short dc2220[];
extern u_short dc2230[];
extern u_short dc2240[];
extern u_short dc2250[];
extern u_short dc2260[];
extern u_short dc2270[];
extern u_short dc2280[];
extern u_short dc2290[];
extern u_short *dc_no_tbl2[];
extern u_short dc3000[];
extern u_short dc3010[];
extern u_short dc3020[];
extern u_short dc3030[];
extern u_short dc3040[];
extern u_short dc3050[];
extern u_short dc3060[];
extern u_short dc3070[];
extern u_short dc3080[];
extern u_short dc3090[];
extern u_short dc3100[];
extern u_short dc3110[];
extern u_short dc3120[];
extern u_short dc3130[];
extern u_short dc3140[];
extern u_short dc3150[];
extern u_short dc3160[];
extern u_short dc3170[];
extern u_short dc3180[];
extern u_short dc3190[];
extern u_short dc3200[];
extern u_short dc3210[];
extern u_short dc3220[];
extern u_short dc3230[];
extern u_short dc3240[];
extern u_short dc3250[];
extern u_short dc3260[];
extern u_short dc3270[];
extern u_short dc3280[];
extern u_short dc3290[];
extern u_short *dc_no_tbl3[];
extern u_short drm_cam_no[12];
extern u_char DBG_cam_id_move_chk;
extern u_char cd_edit_end;
// extern int msn_no;
// extern int renewal_data_chk;
// extern int renewal_data_chk_cnt;
// extern u_short cdcopy[2];
// extern u_char cam_info_disp;
extern sceVu0FVECTOR adj_cam_pos;
// extern u_short fior_tm;
extern CAM_ID_MOVE cam_id_move;
extern int cam_kind;
extern int cam_type;
extern int cd_step;
extern int cam_id;
extern short int plyr_adj[4];

void CameraMain();
void KonwakuCamCtrl();
void FinderInCameraCtrl();
void NormalCameraCtrl();
int GetCameraInfo(MAP_CAM_INFO *mci);
void GetCameraData(u_char kind, MAP_CAM_INFO *mci);
void SetCamPos0(SgCAMERA *tc, MAP_CAM_INFO *mci);
void SetCamPos1(SgCAMERA *tc, MAP_CAM_INFO *mci);
void SetCamPos2(SgCAMERA *tc, MAP_CAM_INFO *mci);
void SetCamPos3(SgCAMERA *tc, MAP_CAM_INFO *mci);
void SetCamPos4(SgCAMERA *tc, MAP_CAM_INFO *mci);
void SetCamPos5(SgCAMERA *tc, MAP_CAM_INFO *mci);
float GetMCLocalPosPer(u_short cn, u_char kind, u_char id);
void CompleCameraPos(SgCAMERA *tc, SgCAMERA *oc, MAP_CAM_INFO *mci);
u_char CompleReqChk(MAP_CAM_INFO *mci);
void FinderModeCameraCtrl();
void PlyrCamCondChk();
void PconMahiCameraCtrl();
void PconTebureCameraCtrl();
int FinderModePadChk(char *pad_x, char *pad_y, float *ax, float *ay, u_char *jpad_on);
void SetFinderRot();
void CameraIdMoveCtrl();
u_char SetMapCamDat0(MAP_CAM_DAT *mcd);
u_char SetMapCamDat1(MAP_CAM_DAT *mcd);
u_char SetMapCamDat2(MAP_CAM_DAT *mcd, u_char id);
u_char SetMapCamDat3(MAP_CAM_DAT *mcd, u_char id);
u_char SetMapCamDat4(MAP_CAM_DAT *mcd, u_char id);
u_char SetMapCamDat5(MAP_CAM_DAT *mcd);
void SaveCamDat(u_char kind, u_char mn);

#ifdef MATCHING_DECOMP
#ifdef INCLUDING_FROM_CAMERA_C
// fix for undeclared ReqFinderInOverRap in header
void ReqFinderInOverRap(u_short time);
#endif
#else
void ReqFinderInOverRap(u_short time);
#endif

void FinderInOverRapCtrl();
void PlyrDmgCameraCtrl();
void ReqDramaCamera(u_char req, u_short no, u_short time);
void DramaCameraReqCtrl();
void ClearDramaCamReq();

#endif // INGAME_CAMERA_CAMERA_H
