#ifndef IOPSYS_H_
#define IOPSYS_H_

#include "enums.h"
#include "os/eeiop/eeiop.h"
#include "typedefs.h"
#include <SDL3/SDL_thread.h>

enum IOP_CMD_MODE
{
    ICM_INIT = 0,
    ICM_REQ = 1,
    ICM_GET_STAT_ONLY = 2
};

enum TRANS_MEM_TYPE
{
    TRANS_MEM_EE = 0,
    TRANS_MEM_IOP = 1,
    TRANS_MEM_SPU = 2,
    TRANS_PCM = 3,
    TRANS_MEM_NUM = 4
};

typedef struct
{// 0x414
    /* 0x000 */ int get_cmd[256];
    /* 0x400 */ int cmd_num;
    /* 0x404 */ int timer_id;
    ///* 0x408 */ int thread_id;
    SDL_Thread *thread;
    /* 0x40c */ int adpcm_thid;
    /* 0x410 */ unsigned int count;
} IOP_SYS_CTRL;

typedef struct
{// 0x4
    /* 0x0 */ u_short vol;
    /* 0x2 */ u_char mono;
} IOP_MASTER_VOL;

extern IOP_STAT iop_stat;

void *IopDrvFunc(unsigned int command, void *data, int size);

#endif// IOPSYS_H_
