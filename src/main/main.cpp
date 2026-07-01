#include "typedefs.h"
#include "common.h"
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
#include "iop/adpcm/iopadpcm.h"
#include "iop/iopmain.h"
#include "mikupan/io/mikupan_controller.h"
#include "mikupan/io/mikupan_file_c.h"
#include "mikupan/gs/mikupan_texture_manager_c.h"
#include "mikupan/ui/mikupan_ui.h"
#include "os/eeiop/se_cmd.h"

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <mikupan/mikupan_memory.h>
#include <sce/libpad.h>

static int mikupan_game_initialized = 0;
static int mikupan_missing_data_prompted = 0;
static char mikupan_missing_data_file[256] = "";

static int MikuPan_TryInitializeGame(void)
{
    char missing_file[sizeof(mikupan_missing_data_file)] = "";

    if (mikupan_game_initialized)
    {
        return 1;
    }

    if (!MikuPan_HasRequiredDataFiles(missing_file, sizeof(missing_file)))
    {
        if (strcmp(mikupan_missing_data_file, missing_file) != 0)
        {
            strncpy(mikupan_missing_data_file, missing_file,
                    sizeof(mikupan_missing_data_file) - 1);
            mikupan_missing_data_file[sizeof(mikupan_missing_data_file) - 1] =
                '\0';
            mikupan_missing_data_prompted = 0;
        }

        if (!mikupan_missing_data_prompted)
        {
            MikuPan_RequestDataFolderSelection(mikupan_missing_data_file);
            mikupan_missing_data_prompted = 1;
        }

        return 0;
    }

    mikupan_missing_data_file[0] = '\0';
    mikupan_missing_data_prompted = 0;

    /// GAME LOGIC ///
    InitSystem();
    InitGameFirst();
    mikupan_game_initialized = 1;
    return 1;
}

SDL_AppResult SDL_AppInit(void **appstate, int argc, char *argv[])
{
    SDL_AppResult result = MikuPan_Init();
    if (result != SDL_APP_CONTINUE)
    {
        return result;
    }

    MikuPan_InitPs2Memory();

    MikuPan_TryInitializeGame();

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

    MikuPan_ProcessEventUi(event);

    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppIterate(void *appstate)
{
    MikuPan_ServiceMissingDataFolderDialog();

    MikuPan_StartFrameUi();
    MikuPan_Clear();

    MikuPan_FlushTextureCache();

    if (!mikupan_game_initialized)
    {
        MikuPan_TryInitializeGame();
        if (!mikupan_game_initialized)
        {
            MikuPan_DrawMissingDataUi(mikupan_missing_data_file);
            MikuPan_EndFrame();
            return SDL_APP_CONTINUE;
        }
    }

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
        else
        {
            /// Empty the packets for 2D stuff otherwise it crashes
            DrawAll2DMes_P2();
        }
    }
    else
    {
        InitGameFirst();
    }

    /* Resolve finder mouse-look capture once per frame. Runs unconditionally so
     * the cursor is released whenever this frame did not request aiming (movie,
     * menu, soft reset, etc.). */
    MikuPan_FinderMouseUpdate();

    MikuPan_EndFrame();

    return SDL_APP_CONTINUE;
}

void SDL_AppQuit(void *appstate, SDL_AppResult result)
{
    if (mikupan_game_initialized)
    {
        IopShutDown();
    }

    if (result == SDL_APP_SUCCESS)
    {
        MikuPan_ShutDownUi();
    }

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
    SetIopCmdSm(IC_SE_INIT, 0, 0, 0);
    EAdpcmCmdStop(0, 0, 0);

    sys_wrk.reset = 0;

    SetSysBackColor(0, 0, 0);

    scene_bg_color = 0;

    while (1)
    {
        EiMain();

        if (EAGetRetStat() == ADPCM_STAT_FULL_STOP || EAGetRetStat() == ADPCM_STAT_LOOPEND_STOP)
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
