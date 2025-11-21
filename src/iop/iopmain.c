#include "cdvd/iopcdvd.h"
#include "iopsys.h"

#include "os/eeiop/eeiop.h"

#include <SDL3/SDL_timer.h>

IOP_STAT iop_stat;
IOP_MASTER_VOL iop_mv;
IOP_SYS_CTRL iop_sys_ctrl;
static int request_shutdown = 0;
static void IopInitDevice();
static int IopInitMain();
static SDLCALL int IopMain(void *);

void *IopDrvFunc(unsigned int command, void *data, int size)
{
    IOP_COMMAND *icp;
    int i;

    if (command == ICM_INIT)
    {
        iop_mv.vol = 0;
        iop_mv.mono = 0;

        IopInitDevice();

        iop_mv.vol = 0x3FFF;
        //sceSdSetParam(SD_P_MVOLL | SD_CORE_0, iop_mv.vol);
        //sceSdSetParam(SD_P_MVOLR | SD_CORE_0, iop_mv.vol);
        //sceSdSetParam(SD_P_MVOLL | SD_CORE_1, iop_mv.vol);
        //sceSdSetParam(SD_P_MVOLR | SD_CORE_1, iop_mv.vol);

        IopInitMain();
    }
    else if (command == ICM_REQ)
    {
        icp = data;

        //se_start_flg = 0;
        //se_stop_flg = 0;

        for (i = 0; i < (size / sizeof(*icp)); i++)
        {
            if (icp->cmd_no >= IC_SE_INIT && icp->cmd_no <= IC_SE_QUIT)
            {
                //ISeCmd(icp);
            }
            else if (icp->cmd_no >= IC_CDVD_INIT
                     && icp->cmd_no <= IC_CDVD_BREAK)
            {
                ICdvdCmd(icp);
            }
            else if (icp->cmd_no >= IC_ADPCM_INIT
                     && icp->cmd_no <= IC_ADPCM_QUIT)
            {
                //IAdpcmCmd(icp);
            }

            icp->cmd_no = 0;
            icp++;
        }

        //if (se_start_flg)
        //    sceSdSetSwitch(SD_S_KON | SD_CORE_1, se_start_flg);
        //if (se_stop_flg)
        //    sceSdSetSwitch(SD_S_KOFF | SD_CORE_1, se_stop_flg);
    }

    //ICdvdTransSeEnd();
    return &iop_stat;
}

static void IopInitDevice()
{
    //sceSdInit(0);
    ICdvdInit(0);
    //ISeInit(0);
    //IAdpcmInit(0);
}

static int IopInitMain()
{
    iop_sys_ctrl.thread = SDL_CreateThread(IopMain, "IOP", NULL);

    return 0;
}

static SDLCALL int IopMain(void *data)
{
    while (!request_shutdown)
    {
        // The thread was originally woken up by an IOP timer handler
        SDL_DelayNS(4167000);

        //ISeMain();
        ICdvdMain();
        //IAdpcmMain2();
    }

    return 0;
}

void IopShutDown()
{
    request_shutdown = 1;

    SDL_WaitThread(iop_sys_ctrl.thread, NULL);
}
