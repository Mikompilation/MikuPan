#include "common.h"
#include "typedefs.h"
#include "enums.h"
#include "gamemain.h"

#include "common/memory_addresses.h"
#include "main/glob.h"
#include "outgame/title.h"
#include "outgame/outgame.h"
#include "outgame/mode_slct.h"
#include "ingame/ingame.h"
#include "mc/mc_main.h"
#include "mc/mc_start.h"
#include "graphics/graph3d/gra3d.h"
#include "graphics/graph2d/tim2_new.h"
#include "graphics/graph2d/tim2.h"
#include "graphics/mov/movie.h"
#include "os/eeiop/eese.h"
#include "os/eeiop/cdvd/eecdvd.h"

#ifdef BUILD_EU_VERSION
#include "os/system.h"
#endif

int init_load_id = 0;
int init_load_adr = 0;
int init_load_size = 0;

void GameMain()
{
#ifdef BUILD_EU_VERSION
    switch (sys_wrk.game_mode)
    {
    case GAME_MODE_INIT:
        if (GameInit())
        {
            if (mc_start_flg == 1)
            {
                sys_wrk.game_mode = mc_start_flg;
                mcInit(7, (u_int *)0x00420000, 0);
                mc_start_flg = 0;
            }
            else
            {
                sys_wrk.game_mode = GAME_MODE_OUTGAME;
            }
        }
    break;
    case GAME_MODE_LOAD_LANGUAGE:
        switch (mcLanguageLoadMain())
        {
        case 0:
            // nothing to do
        break;
        case 1:
            sys_wrk.game_mode = GAME_MODE_LANGUAGE;
            InitSelectLanguage();
        break;
        case 2:
            sys_wrk.game_mode = GAME_MODE_MCINIT;
            if (mc_language & 0x80)
            {
                g_db.disp[1].display.DX = 0x27c; // 636
                g_db.disp[1].display.DY = 0x32; // 50
                g_db.disp[0].display.DX = 0x27c; // 636
                g_db.disp[0].display.DY = 0x32; // 50
                sceGsResetGraph(0, 1, 2, 1);
            }
        break;
        }
    break;
    case GAME_MODE_LANGUAGE:
        if (LanguageSelectMain())
        {
            init_load_id = LoadReqLanguage(FNT001_E_PK2, 0x1e30000);
            sys_wrk.game_mode = GAME_MODE_FONT_LOAD_WAIT;
        }
    break;
    case GAME_MODE_FONT_LOAD_WAIT:
        if (IsLoadEnd(init_load_id))
        {
            sys_wrk.game_mode = GAME_MODE_MCINIT;
        }
    break;
    case GAME_MODE_MSG_LOAD:
        if (IsLoadEnd(init_load_id))
        {
            sys_wrk.game_mode = GAME_MODE_OUTGAME;
        }
    break;
    case GAME_MODE_MCINIT:
        sys_wrk.game_mode = GAME_MODE_MCCHECK;
        mcInit(7, NULL, 0);
    break;
    case GAME_MODE_MCCHECK:
        if (mcStartCheckMain())
        {
            mcEnd();
            sys_wrk.game_mode = GAME_MODE_MSG_LOAD;
            init_load_id = LoadReqLanguage(FNT001_E_PK2, 0x1e30000);
            init_load_id = LoadReqLanguage(IG_MSG_E_OBJ, 0x84a000);
        }
    break;
    case GAME_MODE_OUTGAME:
        OutGameCtrl();
    break;
    case GAME_MODE_INGAME:
        InGameCtrl();
    break;
    }
#else
    switch (sys_wrk.game_mode)
    {
    case GAME_MODE_INIT:
        if (GameInit())
        {
            if (mc_start_flg == 1)
            {
                sys_wrk.game_mode = mc_start_flg;
                mcInit(7, NULL, 0);
                mc_start_flg = 0;
            }
            else
            {
                sys_wrk.game_mode = GAME_MODE_OUTGAME;
            }
        }
    break;
    case GAME_MODE_MCCHECK:
        if (mcStartCheckMain())
        {
            sys_wrk.game_mode = GAME_MODE_OUTGAME;
            mcEnd();
        }
    break;
    case GAME_MODE_OUTGAME:
        OutGameCtrl();
    break;
    case GAME_MODE_INGAME:
        InGameCtrl();
    break;
    }
#endif
}

int GameInit()
{
    if (GameInitLoad())
    {
        gra3dInit();
        return 1;
  }
    
  return 0;
}

int GameInitLoad()
{
    switch (sys_wrk.load_mode)
    {
    case INGAME_INIT_LOAD_START:
        sys_wrk.load_mode = INGAME_INIT_LOAD_MENU;
    case INGAME_INIT_LOAD_MENU:
#ifdef BUILD_EU_VERSION
        init_load_id = LoadReqLanguage(IG_MSG_E_OBJ, IG_MSG_OBJ_ADDRESS);
#else
        init_load_id = LoadReq(IG_MSG_OBJ, &IG_MSG_OBJ_ADDRESS);
#endif
        sys_wrk.load_mode = INGAME_INIT_WAIT_MENU;
    break;
    case INGAME_INIT_WAIT_MENU:
        if (!IsLoadEnd(init_load_id))
        {
            return 0;
        }
        sys_wrk.load_mode = INGAME_INIT_LOAD_EFCT;
    case INGAME_INIT_LOAD_EFCT:
#ifdef BUILD_EU_VERSION
        init_load_id = LoadReqLanguage(FNT001_E_PK2, 0x1e30000);
#else
        init_load_id = LoadReq(FNT001_PK2, &FontTextAddress);
#endif
        init_load_id = LoadReq(EFF001_PK2, &EFFECT_ADDRESS);
        sys_wrk.load_mode = INGAME_INIT_WAIT_EFCT;
    break;
    case INGAME_INIT_WAIT_EFCT:
        if (!IsLoadEnd(init_load_id))
        {
            return 0;
        }

        MakeFontTexSendPacket();
        SetETIM2File(EFFECT_ADDRESS);
        sys_wrk.load_mode = INGAME_INIT_LOAD_CAME;
    case INGAME_INIT_LOAD_CAME:
#ifdef BUILD_EU_VERSION
        init_load_id = SeFileLoadAndSet(SSYSTEM_BD, SE_ADDRNO_STATIC);
        init_load_id = SeFileLoadAndSet(SGY000_BD, SE_ADDRNO_BTLHIT);
#else
        init_load_id = SeFileLoadAndSet(SSYSTEM_BD, SE_ADDRNO_STATIC);
        init_load_id = SeFileLoadAndSet(SGY000_BD, SE_ADDRNO_BTLHIT);
#endif
        sys_wrk.load_mode = INGAME_INIT_WAIT_CAME;
    break;
    case INGAME_INIT_WAIT_CAME:
        if (!IsLoadEnd(init_load_id))
        {
            return 0;
        }
        sys_wrk.load_mode = INGAME_INIT_LOAD_PHOTO;
    case INGAME_INIT_LOAD_PHOTO:
        sys_wrk.load_mode = 0;
        return 1;
    }
    
    return 0;
}

void GameModeChange(u_char mode)
{
    switch (mode)
    {
    case GAME_MODE_INIT:
        MovieInitWrk();
        sys_wrk.game_mode = GAME_MODE_INGAME;
        ingame_wrk.mode = INGAME_MODE_FIRST_LOAD;
    break;
    case GAME_MODE_MCCHECK:
        sys_wrk.game_mode = GAME_MODE_OUTGAME;
        if (ingame_wrk.game == INGAME_MODE_INIT)
        {
            title_wrk.mode = TITLE_INIT_FROM_IN;
            outgame_wrk.mode = OUTGAME_MODE_BATTLE;
            BattleModeNext();
        }
        else
        {
            title_wrk.mode = TITLE_INIT_FROM_IN;
            outgame_wrk.mode = OUTGAME_MODE_TITLE_TOP;
        }
        SetReverbVolume(0x2fff);
    break;
    case GAME_MODE_OUTGAME:
        sys_wrk.game_mode = GAME_MODE_OUTGAME;
        if (ingame_wrk.game != INGAME_MODE_INIT)
        {
            title_wrk.mode = TITLE_INIT_FROM_IN;
        }
        SetReverbVolume(0x2fff);
    break;
    case GAME_MODE_INGAME:
        sys_wrk.game_mode = GAME_MODE_OUTGAME;
        title_wrk.mode = TITLE_INIT_FROM_IN;
        SetReverbVolume(0x2fff);
    break;
    }
}

#ifdef BUILD_EU_VERSION
int LanguageSelectMain()
{
    return SetSelectLanguage(sys_wrk.language);
}
#endif
