#include "iopmain.h"
#include "cdvd/iopcdvd.h"
#include "enums.h"
#include "iop/adpcm/iopadpcm.h"
#include "mikupan/mikupan_logging_c.h"
#include "os/eeiop/eeiop.h"

#include <SDL3/SDL_init.h>
#include <SDL3/SDL_timer.h>
#include <SDL3/SDL_audio.h>
#include <SDL3/SDL_log.h>
#include <SDL3/SDL_stdinc.h>

IOP_STAT iop_stat;
IOP_MASTER_VOL iop_mv;
IOP_SYS_CTRL iop_sys_ctrl;

static int request_shutdown = 0;

SDL_AudioDeviceID audio_dev;

static void IopInitDevice();
static int IopInitMain();
static SDLCALL int IopMain(void *data);

static int IsRealAudioDriver(const char *drv)
{
    if (!drv) return 0;

#if defined(_WIN32)
    return (SDL_strcasecmp(drv, "wasapi") == 0 ||
            SDL_strcasecmp(drv, "directsound") == 0 ||
            SDL_strcasecmp(drv, "winmm") == 0);

#elif defined(__linux__)
    return (SDL_strcasecmp(drv, "pipewire") == 0 ||
            SDL_strcasecmp(drv, "pulseaudio") == 0 ||
            SDL_strcasecmp(drv, "alsa") == 0 ||
            SDL_strcasecmp(drv, "jack") == 0 ||
            SDL_strcasecmp(drv, "sndio") == 0);

#elif defined(__APPLE__)
    return (SDL_strcasecmp(drv, "coreaudio") == 0);

#else
    return (SDL_strcasecmp(drv, "dummy") != 0 &&
            SDL_strcasecmp(drv, "disk")  != 0);
#endif
}

static void LogSDLAudioDiagnostics()
{
    const int n = SDL_GetNumAudioDrivers();
    int has_real_driver = 0;

    for (int i = 0; i < n; i++) {
        const char *drv = SDL_GetAudioDriver(i);
        if (IsRealAudioDriver(drv)) {
            has_real_driver = 1;
            break;
        }
    }

    if (has_real_driver) {
        return;
    }

#if defined(_WIN32)
    info_log("Platform: Windows");
#elif defined(__linux__)
    info_log("Platform: Linux");
#elif defined(__APPLE__)
    info_log("Platform: macOS");
#else
    info_log("Platform: Other");
#endif

    info_log("Available SDL audio drivers: %d", n);

    for (int i = 0; i < n; i++) {
        const char *drv = SDL_GetAudioDriver(i);
        info_log("  driver[%d]=%s", i, drv ? drv : "(null)");
    }

    info_log("WARNING: No real audio backend found.");

#if defined(_WIN32)
    info_log("WARNING: If you only see dummy or disk, SDL is likely built without Windows audio backends OR you are loading the wrong SDL3.dll at runtime.");
    info_log("WARNING: Make sure the intended SDL3.dll is next to your .exe, and verify your SDL build enables WASAPI/DirectSound.");
#else
    info_log("WARNING: If you only see dummy or disk, SDL may be built without system audio support.");
    info_log("WARNING: Consider recompiling SDL with the proper audio development packages/backends enabled.");
#endif
}

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
            if (icp->cmd_no >= IC_SE_INIT
                && icp->cmd_no <= IC_SE_QUIT)
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
                IAdpcmCmd(icp);
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
    SDL_AudioSpec spec;
    SDL_zero(spec);

    if (!SDL_Init(SDL_INIT_AUDIO))
    {
        info_log("Failed to initialize SDL audio subsystem: %s", SDL_GetError());
        info_log("Hint: If SDL reports no audio devices and only dummy/disk drivers exist, verify your SDL build and which SDL library is being loaded at runtime.");
        LogSDLAudioDiagnostics();
        info_log("Continuing without audio support");
        audio_dev = 0;
    }
    else
    {
        const char *driver = SDL_GetCurrentAudioDriver();
        info_log("Audio Driver: %s", driver ? driver : "(null)");
        
        spec.channels = 2;
        spec.format = SDL_AUDIO_S16;
        spec.freq = 48000;

        audio_dev = SDL_OpenAudioDevice(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec);

        if (!audio_dev)
        {
            info_log("Failed to open audio device: %s", SDL_GetError());
            LogSDLAudioDiagnostics();
            info_log("Continuing without audio support");
        }
        else
        {
            SDL_ResumeAudioDevice(audio_dev);
        }
    }

    //sceSdInit(0);
    ICdvdInit(0);
    //ISeInit(0);
    IAdpcmInit(0);
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
        IAdpcmMain2();
    }

    return 0;
}

void IopShutDown()
{
    request_shutdown = 1;
    SDL_WaitThread(iop_sys_ctrl.thread, NULL);
}
