#ifndef IOPSE_H_
#define IOPSE_H_
#include "common.h"
#include "enums.h"
#include "os/eeiop/eeiop.h"
#include "typedefs.h"

typedef struct
{// 0x10
    /* 0x0 */ unsigned int adrs;
    /* 0x4 */ short unsigned int attribute;
    /* 0x6 */ short unsigned int pitch;
    /* 0x8 */ short unsigned int adsr1;
    /* 0xa */ short unsigned int adsr2;
    /* 0xc */ char efct;
    /* 0xd */ char vol;
    /* 0xe */ char pan;
} SE_PARAM;

typedef struct
{// 0x20
    /* 0x00 */ SE_PARAM *param;
    /* 0x00 */ unsigned int attribute;
    /* 0x08 */ short int v_no;
    /* 0x0a */ short int prm_no;
    /* 0x0c */ short unsigned int pitch;
    /* 0x0e */ short unsigned int vol_l;
    /* 0x10 */ short unsigned int vol_r;
    /* 0x12 */ short unsigned int adsr1;
    /* 0x14 */ short unsigned int adsr2;
    /* 0x16 */ short unsigned int tgt_vol_l;
    /* 0x18 */ short unsigned int tgt_vol_r;
    /* 0x1a */ char inc_vol_l;
    /* 0x1b */ char inc_vol_r;
    /* 0x1c */ unsigned char buf_no;
} SE_WRK_SET;

typedef struct
{// 0x32
    /* 0x00 */ short int btlhit;
    /* 0x02 */ short int voice;
    /* 0x04 */ short int door[3];
    /* 0x0a */ short int foot[8];
    /* 0x1a */ short int srund[2];
    /* 0x1e */ short int ghost[3];
    /* 0x24 */ short int event[2];
    /* 0x28 */ short int wide;
    /* 0x2a */ short int jidou[4];
} SE_START_POINT;

extern u_int se_start_flg;
extern u_int se_stop_flg;

extern u_int snd_buf_top[];

extern u_int *seBuff;

void ISeInit(int mode);
void ISeCmd(IOP_COMMAND *icp);
void ISeMain();

u_int SeGetSndBufTop(int pos);
void SeSetStartPoint(u_char type, u_int no);
int ICdvdTransSeEnd();
void SeSetMasterVol(u_short mvol);

SE_PARAM *SeGetSeParamTbl(int num);
int GetSeAdrs(int se_no);

void sceSdSetParam(u_short entry, u_short value);

#endif// IOPSE_H_