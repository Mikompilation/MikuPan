#ifndef INGAME_MAP_FURN_DAT_H
#define INGAME_MAP_FURN_DAT_H

#include "typedefs.h"
#include "graphics/graph3d/light_dat.h"

typedef struct {
	u_short attr_no;
	u_short dist_n;
	u_short dist_f;
	u_short score;
	u_short fact_no;
	u_char fp_num;
	u_char fp_no;
	u_char fp_rot;
	u_char fp_mak;
	u_char acs_flg;
	u_char ef_type;
	u_char ef_no;
} FURN_DAT;

typedef enum
{
    ST_FACT_VACANT = 0,
    ST_FACT_WAITWAIT = 1,
    ST_FACT_WAIT = 2,
    ST_FACT_EXEC = 3,
    ST_FACT_END = 4
} ST_FACT;

typedef struct {
	u_char TexNum;
	u_char pads;
	u_char AnmON;
	u_char AnmStep;
	u_char AnmCnt;
	u_char AnmLoop;
} TextureAnimation;

typedef struct {
	signed char nochkflg;
	void *p;
	u_char chknum[4];
} SPE_CHK_COND;

typedef struct {
	u_char regmode;
	u_char looptype;
	char nowstep;
} FSPE_TEXTURE_ANM;

typedef struct {
	u_char attribute;
	void *wp;
	void *ap;
	SPE_CHK_COND chk_occ;
	SPE_CHK_COND chk_end;
	ST_FACT state;
	u_char flg;
	u_char eventflg;
	signed char trembleflg;
	u_char prejudge;
	signed char lw_id;
	void *buff;
	u_short furn_id;
	u_short fact_no;
	int fw_id;
	u_char room_no;
	sceVu0FVECTOR *vecp_temp;
	sceVu0FVECTOR util_v;
	sceVu0FVECTOR pos;
	sceVu0FVECTOR rot_speed;
	sceVu0FVECTOR rot_speed2;
	u_short count;
	int adj_valg_num;
	u_short se_no;
	TextureAnimation *pta;
	FSPE_TEXTURE_ANM ta;
} FURN_ACT_WRK;

typedef struct {
	u_short furn_no;
	short int stts;
	sceVu0FVECTOR pos;
	sceVu0FVECTOR rotate;
	float rot;
	int fewrk_no;
	u_short id;
	u_short fno_bk;
	u_short dist;
	u_short score;
	float ratio;
	u_char use;
	u_char cmdflg;
	u_short attr_id;
	u_char room_id;
	u_char fs_flg;
	LIGHT_PACK mylight;
} FURN_WRK;

typedef struct {
	float rot_y;
	float rot_x;
	u_short pos_x;
	short int pos_y;
	u_short pos_z;
	u_short attr_id;
	u_short model_no;
	u_short id;
	short int top;
	short int btm;
	u_char snum;
} FURN_DATA_POP;

extern FURN_DAT furn_dat[];
extern u_int furn_attr_dat[0][5];
extern sceVu0FVECTOR furn_photo_center00;
extern sceVu0FVECTOR furn_photo_center01;
extern sceVu0FVECTOR furn_photo_center02;
extern sceVu0FVECTOR furn_photo_center03;
extern sceVu0FVECTOR furn_photo_center04;
extern sceVu0FVECTOR furn_photo_center05;
extern sceVu0FVECTOR furn_photo_center06;
extern sceVu0FVECTOR furn_photo_center07;
extern sceVu0FVECTOR furn_photo_center08;
extern sceVu0FVECTOR furn_photo_center09;
extern sceVu0FVECTOR furn_photo_center10;
extern sceVu0FVECTOR furn_photo_center11;
extern sceVu0FVECTOR furn_photo_center12;
extern sceVu0FVECTOR furn_photo_center13;
extern sceVu0FVECTOR furn_photo_center14;
extern sceVu0FVECTOR furn_photo_center15;
extern sceVu0FVECTOR furn_photo_center16;
extern sceVu0FVECTOR furn_photo_center17;
extern sceVu0FVECTOR furn_photo_center18;
extern sceVu0FVECTOR furn_photo_center19;
extern sceVu0FVECTOR furn_photo_center20[];
extern sceVu0FVECTOR furn_photo_center21;
extern sceVu0FVECTOR furn_photo_center22;
extern sceVu0FVECTOR furn_photo_center23;
extern sceVu0FVECTOR furn_photo_center24;
extern sceVu0FVECTOR furn_photo_center25;
extern sceVu0FVECTOR furn_photo_center26;
extern sceVu0FVECTOR furn_photo_center27;
extern sceVu0FVECTOR furn_photo_center28;
extern sceVu0FVECTOR furn_photo_center29;
extern sceVu0FVECTOR furn_photo_center30;
extern sceVu0FVECTOR furn_photo_center31;
extern sceVu0FVECTOR furn_photo_center32;
extern sceVu0FVECTOR furn_photo_center33;
extern sceVu0FVECTOR furn_photo_center34;
extern sceVu0FVECTOR furn_photo_center35;
extern sceVu0FVECTOR furn_photo_center36;
extern sceVu0FVECTOR furn_photo_center37;
extern sceVu0FVECTOR furn_photo_center38;
extern sceVu0FVECTOR furn_photo_center39;
extern sceVu0FVECTOR furn_photo_center40;
extern sceVu0FVECTOR furn_photo_center41;
extern sceVu0FVECTOR furn_photo_center42;
extern sceVu0FVECTOR furn_photo_center43;
extern sceVu0FVECTOR furn_photo_center44;
extern sceVu0FVECTOR furn_photo_center45;
extern sceVu0FVECTOR furn_photo_center46;
extern sceVu0FVECTOR furn_photo_center47;
extern sceVu0FVECTOR furn_photo_center48;
extern sceVu0FVECTOR furn_photo_center49;
extern sceVu0FVECTOR furn_photo_center50;
extern sceVu0FVECTOR furn_photo_center51[];
extern sceVu0FVECTOR furn_photo_center52;
extern sceVu0FVECTOR furn_photo_center53;
extern sceVu0FVECTOR furn_photo_center54;
extern sceVu0FVECTOR furn_photo_center55;
extern sceVu0FVECTOR furn_photo_center56;
extern sceVu0FVECTOR furn_photo_center57;
extern sceVu0FVECTOR furn_photo_center58;
extern sceVu0FVECTOR furn_photo_center59;
extern sceVu0FVECTOR furn_photo_center60;
extern sceVu0FVECTOR furn_photo_center61;
extern sceVu0FVECTOR furn_photo_center62;
extern sceVu0FVECTOR furn_photo_center63;
extern sceVu0FVECTOR furn_photo_center64;
extern sceVu0FVECTOR furn_photo_center65;
extern sceVu0FVECTOR furn_photo_center66;
extern sceVu0FVECTOR furn_photo_center67;
extern sceVu0FVECTOR furn_photo_center68;
extern sceVu0FVECTOR furn_photo_center69;
extern sceVu0FVECTOR furn_photo_center70;
extern sceVu0FVECTOR furn_photo_center71;
extern sceVu0FVECTOR furn_photo_center72;
extern sceVu0FVECTOR furn_photo_center73;
extern sceVu0FVECTOR furn_photo_center74;
extern sceVu0FVECTOR furn_photo_center75;
extern sceVu0FVECTOR furn_photo_center76;
extern sceVu0FVECTOR furn_photo_center77;
extern sceVu0FVECTOR furn_photo_center78;
extern sceVu0FVECTOR furn_photo_center79;
extern sceVu0FVECTOR furn_photo_center80;
extern sceVu0FVECTOR furn_photo_center81;
extern sceVu0FVECTOR furn_photo_center82;
extern sceVu0FVECTOR furn_photo_center83;
extern sceVu0FVECTOR furn_photo_center84;
extern sceVu0FVECTOR furn_photo_center85;
extern sceVu0FVECTOR furn_photo_center86;
extern sceVu0FVECTOR furn_photo_center87;
extern sceVu0FVECTOR furn_photo_center88;
extern sceVu0FVECTOR furn_photo_center89;
extern sceVu0FVECTOR furn_photo_center90;
extern sceVu0FVECTOR furn_photo_center91;
extern sceVu0FVECTOR furn_photo_center92;
extern sceVu0FVECTOR furn_photo_center93;
extern sceVu0FVECTOR furn_photo_center94;
extern sceVu0FVECTOR furn_photo_center95;
extern sceVu0FVECTOR furn_photo_center96;
extern sceVu0FVECTOR furn_photo_center97;
extern sceVu0FVECTOR furn_photo_center98;
extern sceVu0FVECTOR furn_photo_center99;
extern sceVu0FVECTOR furn_photo_center100;
extern sceVu0FVECTOR furn_photo_center101;
extern sceVu0FVECTOR furn_photo_center102;
extern sceVu0FVECTOR *fpc_dat[];
extern u_char fpc_rot_dat[0][8];

FURN_DAT* GetFurnDatP(u_short furn_id);

#endif // INGAME_MAP_FURN_DAT_H
