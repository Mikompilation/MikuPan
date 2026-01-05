#include "common.h"
#include "typedefs.h"
#include "enums.h"
#include "main.h"
#include <string.h>
#include "main/glob.h"
#include "main/gamemain.h"
#include "outgame/title.h"
#include "os/system.h"
#include "os/eeiop/eeiop.h"
#include "os/eeiop/eese.h"
#include "graphics/mov/movie.h"
#include "graphics/graph2d/tim2_new.h"
#include "graphics/graph2d/g2d_main.h"
#include "graphics/graph3d/sgdma.h"
#include "graphics/graph3d/gra3d.h"
#include "os/eeiop/adpcm/ea_cmd.h"
#include "mikupan/rendering/mikupan_renderer.h"

#define SDL_MAIN_USE_CALLBACKS 1  /* use the callbacks instead of main() */
#include "graphics/ui/imgui_window_c.h"
#include "gs/texture_manager_c.h"
#include "os/eeiop/se_cmd.h"
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <iop/iopsys.h>
#include <mikupan/mikupan_memory.h>

#include <sce/libpad.h>

const int TARGET_FPS = 60;
const double TARGET_FRAME_TIME = 1000.0 / TARGET_FPS; // milliseconds per frame

SDL_AppResult SDL_AppInit(void **appstate, int argc, char *argv[])
{
    SDL_AppResult result = MikuPan_Init();
    MikuPan_InitPs2Memory();

    /// GAME LOGIC ///
    InitSystem();
    InitGameFirst();

    return result;
}

SDL_AppResult SDL_AppEvent(void *appstate, SDL_Event *event)
{
    if (event->type == SDL_EVENT_QUIT)
        {
        return SDL_APP_SUCCESS;  /* end the program, reporting success to the OS. */
    }

    if (event->type == SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED)
    {
        MikuPan_UpdateWindowSize(event->window.data1, event->window.data2);
    }

    if (event->type == SDL_EVENT_GAMEPAD_REMOVED)
    {
        scePadPortOpen(0, 0 ,0);
    }

    if (event->type == SDL_EVENT_GAMEPAD_ADDED)
    {
        scePadPortOpen(0, 0 ,0);
    }

    ProcessEventImGui(event);

    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppIterate(void *appstate)
{
    uint64_t frameStart = SDL_GetTicks();
    NewFrameImGuiWindow();

    MikuPan_Clear();

    MikuPan_FlushTextureCache();

    if (!SoftResetChk())
    {
        if (!PlayMpegEvent())
        {
            EiMain();
            GameMain();
            CheckDMATrans();
            sceGsSyncPath(0, 0);
            vfunc();
            DrawAll2DMes_P2();
            FlushModel(1);
            ClearTextureCache();
            SeCtrlMain();
        }
    }
    else
    {
        InitGameFirst();
    }

    MikuPan_EndFrame();

    //uint64_t frameEnd = SDL_GetTicks();
    //double frameTime = (double)(frameEnd - frameStart);
    //if (frameTime < TARGET_FRAME_TIME)
    //{
    //    SDL_Delay((Uint32)(TARGET_FRAME_TIME - frameTime));
    //}

    return SDL_APP_CONTINUE;
}

void SDL_AppQuit(void *appstate, SDL_AppResult result)
{
    IopShutDown();
    ShutDownImGuiWindow();
    MikuPan_Shutdown();
}

void InitGameFirst()
{
    sys_wrk.reset = 0;
    gra3dInitFirst();
    gra2dInitBG();
    MovieInitWrk();
    realtime_scene_flg = 0;
    sys_wrk.game_mode = GAME_MODE_INIT;
    outgame_wrk.mode = 0;
    outgame_wrk.mode_next = 4;
    memset(&ingame_wrk, 0, sizeof(INGAME_WRK));
    sys_wrk.sreset_ng = 0;
    opening_movie_type = 0;
}

void CallSoftReset()
{
    int lcount;

    lcount = 0;

    SeStopAll();
    InitSe();
    SetIopCmdSm(1, 0, 0, 0);
    EAdpcmCmdStop(0, 0, 0);

    sys_wrk.reset = 0;

    SetSysBackColor(0, 0, 0);

    scene_bg_color = 0;

    while (1)
    {
        EiMain();

        //if (EAGetRetStat() == 1 || EAGetRetStat() == 2)
        {
            EAdpcmCmdInit(1);
            break;
        }

        lcount++;

        if (lcount > 30)
        {
            EAdpcmCmdStop(0, 0, 0);
            lcount = 0;
        }

        vfunc();
    }
}

int SoftResetChk()
{
    if (
        L1_PRESSED() && L2_PRESSED() && R1_PRESSED() &&
        R2_PRESSED() && START_PRESSED() && SELECT_PRESSED()
    )
    {
        // Re-enabled for debug purposes
        sys_wrk.sreset_count += 1;
    }
    else
    {
        sys_wrk.sreset_count = 0;
    }
    
    if (sys_wrk.sreset_ng != 0)
    {
        sys_wrk.sreset_count = 0;
    }

    if (sys_wrk.sreset_count >= 60)
    {
        sys_wrk.reset = 1;
    }
    
    if (sys_wrk.reset == 1)
    {
        sys_wrk.sreset_count = 0;
        CallSoftReset();
        return 1;
    }
    
    return 0;
}
