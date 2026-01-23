#ifndef IOPMAIN_H_
#define IOPMAIN_H_

#include "os/eeiop/eeiop.h"
#include "typedefs.h"
#include "SDL3/SDL_thread.h"
#include "SDL3/SDL_audio.h"

enum IOP_STEREO_SET {
    IS_STEREO = 0,
    IS_MONAURAL = 1
};

enum IOP_CMD_MODE {
    ICM_INIT = 0,
    ICM_REQ = 1,
    ICM_GET_STAT_ONLY = 2
};

enum TRANS_MEM_TYPE {
    TRANS_MEM_EE = 0,
    TRANS_MEM_IOP = 1,
    TRANS_MEM_SPU = 2,
    TRANS_PCM = 3,
    TRANS_MEM_NUM = 4
};

typedef struct { // 0x414
    int get_cmd[256];
    int cmd_num;
    int timer_id;
    //int thread_id;
    SDL_Thread* thread;
    int adpcm_thid;
    unsigned int count;
} IOP_SYS_CTRL;

typedef struct { // 0x4
    /* 0x0 */ u_short vol;
    /* 0x2 */ u_char mono;
} IOP_MASTER_VOL;

extern IOP_STAT iop_stat;
extern IOP_MASTER_VOL iop_mv;
extern IOP_SYS_CTRL iop_sys_ctrl;

extern SDL_AudioDeviceID audio_dev;

IOP_STAT* GetIopStatP();
void *IopDrvFunc(unsigned int command, void *data, int size);
static void IopInitDevice();
static int IopInitMain();
static int IopMain(void *);
void IopShutDown();

#endif // IOPMAIN_H_