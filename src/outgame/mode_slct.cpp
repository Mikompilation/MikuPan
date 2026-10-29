#include "common.h"
#include "typedefs.h"
#include "structs.h"
#include "enums.h"
#include "mode_slct.h"

#include "mikupan/mikupan_memory.h"
#include "mikupan/ui/mikupan_rml_mode_select.h"
#include "mikupan/ui/mikupan_rml_options.h"

#include <string.h>

#include "main/glob.h"
#include "main/gamemain.h"
#include "outgame/title.h"
#include "mikupan/gameplay/mikupan_title_scene.h"
#include "mc/mc_main.h"
#include "outgame/btl_mode/btl_mode.h"
#include "outgame/btl_mode/btl_dat.h"
#include "ingame/menu/ig_option.h"
#include "ingame/menu/sp_menu.h"
#include "ingame/menu/ig_camra.h"
#include "ingame/menu/ig_album.h"
#include "ingame/menu/item.h"
#include "ingame/menu/ig_file.h"
#include "ingame/menu/ig_glst.h"
#include "ingame/menu/ig_menu.h"
#include "outgame/sound_test.h"
#include "outgame/outgame.h"
#include "outgame/btl_mode/btl_menu.h"
#include "outgame/btl_mode/btl_mode.h"
#include "graphics/graph2d/sprt.h"
#include "graphics/graph2d/tim2.h"
#include "graphics/graph2d/effect_sub.h"
#include "graphics/motion/mdlwork.h"
#include "graphics/graph2d/message.h"
#include "graphics/graph3d/sglib.h"
#include "os/eeiop/cdvd/eecdvd.h"
#include "os/eeiop/adpcm/ea_ctrl.h"
#include "os/eeiop/adpcm/ea_dat.h"
#include "os/eeiop/adpcm/ea_cmd.h"
#include "os/eeiop/eese.h"

#include "data/msl_bg_btl.h" // SPRT_DAT msl_bg_btl[11] = {...};
#include "data/msl_bg_fsm.h" // SPRT_DAT msl_bg_fsm[11] = {...};
#include "data/msl_bg_opt.h" // SPRT_DAT msl_bg_opt[11] = {...};
#include "data/msl_bg_snd.h" // SPRT_DAT msl_bg_snd[11] = {...};
#include "data/msl_bg_sty.h" // SPRT_DAT msl_bg_sty[11] = {...};
#include "data/msel_sprt.h"  // SPRT_DAT msel_sprt[148] = {...};

static u_char sm_csr_info[2] = {0, 0};
int cmn_tex_load = 0;
int title_bgm_no = -1;
static DSP_M_SLCT_WRK dsp_ms;
static int ms_load_id;

void ModeSlctInit(u_char top, u_char end)
{
#ifdef BUILD_EU_VERSION
    ms_load_id = LoadReqLanguage(M_SLCT_CMN_E_PK2, 0xc80000);
#else
    ms_load_id = LoadReq(M_SLCT_CMN_PK2, M_SLCT_CMN_PK2_ADDRESS);
#endif
    memset(&dsp_ms, 0, sizeof(DSP_M_SLCT_WRK));
    dsp_ms.now_mode = top;
    dsp_ms.end_mode = end;
    cmn_tex_load = 1;
    dsp_ms.unsel_col[0] = 20;
    dsp_ms.unsel_col[1] = 15;
    dsp_ms.unsel_col[2] = 10;
    dsp_ms.unsel_col[3] = 128;
}

void ModeSlctLoop()
{
  ModeSlctCtrl(dsp_ms.now_mode);
  dsp_ms.mode_tmr++;
}

void ModeSlctCtrl(u_char mode)
{
    int adpcm_no;

    if (ms_load_id != -1 && IsLoadEnd(ms_load_id))
    {
        ms_load_id = -1;
    }
    
    switch (dsp_ms.main_step)
    {
    case MS_MODE_START:
        if (cmn_tex_load == 0)
        {
#ifdef BUILD_EU_VERSION
            ms_load_id = LoadReqLanguage(M_SLCT_CMN_E_PK2, 0xc80000);
#else
            ms_load_id = LoadReq(M_SLCT_CMN_PK2, M_SLCT_CMN_PK2_ADDRESS);
#endif
        }
        adpcm_no = -1;
        MsLoadCtrl(mode);
        dsp_ms.main_step = MS_MODE_INIT;
        
        if (dsp_ms.now_mode == MS_BATTLE_MODE || dsp_ms.now_mode == MS_MISSION_SELECT)
        {
            adpcm_no = AB000_STR;
            SeStopAll();
        }
        else if (dsp_ms.now_mode == MS_MODE_SLCT)
        {
            adpcm_no = AB027_STR;
        }
        else if (dsp_ms.now_mode == MS_STORY_MODE)
        {
            adpcm_no = AB027_STR;
        }
        else if (dsp_ms.now_mode == MS_SOUND_TEST)
        {
            title_bgm_no = -1;
        }
        else if (dsp_ms.now_mode == MS_OPTION)
        {
            title_bgm_no = -1;
        }
        
        if (adpcm_no != -1)
        {
            if (title_bgm_no != adpcm_no)
            {
                EAdpcmCmdPlay(0, 1, adpcm_no, 0, GetAdpcmVol(adpcm_no), 0x280, 0xfff, 0);
                title_bgm_no = adpcm_no;
            }
        }
        break;
    case MS_MODE_INIT:
        if (ms_load_id == -1)
        {
            if (cmn_tex_load == 0)
            {
                cmn_tex_load = 1;
            }
            dsp_ms.bak_alp = 0;
            dsp_ms.chr_alp = 0;
            dsp_ms.csr[1] = 0;
            if (mode == MS_OPTION)
            {
                StartOptionModeInit(1);
                dsp_ms.csr[2] = 0;
                dsp_ms.opt_mode = MS_MISSION_SELECT;
                MikuPan_RmlOptionsOpen();
            }
            else if (mode == MS_STORY_MODE)
            {
                dsp_ms.sm_slct[2] = cribo.costume;
                dsp_ms.sm_slct[1] = ingame_wrk.difficult;
                dsp_ms.sm_slct[0] = 0;
                SetCsrInfoStoryMode();
            }
            else if (mode == MS_BATTLE_MODE)
            {
                dsp_ms.bm_slct[0] = cribo.costume;
#ifdef BUILD_EU_VERSION
                SetCsrInfoBattleMode();
#endif
            }
            dsp_ms.main_step = MS_MODE_FADE_INN;
            SeSetMVol(opt_wrk.bgm_vol);
            SeSetSteMono(opt_wrk.sound_mode);
        }
    break;
    case MS_MODE_FADE_INN:
        if (dsp_ms.bak_alp + 8 < 128)
        {
            dsp_ms.bak_alp += 8;
            dsp_ms.chr_alp = dsp_ms.bak_alp;
            if (dsp_ms.flm_alp != 128)
            {
                dsp_ms.flm_alp = dsp_ms.chr_alp;
            }
            
            if (dsp_ms.win_alp != 128)
            {
                dsp_ms.win_alp = dsp_ms.bak_alp;
            }
        }
        else
        {
            dsp_ms.flm_alp = 128;
            dsp_ms.win_alp = 128;
            dsp_ms.bak_alp = 128;
            dsp_ms.chr_alp = 128;
            dsp_ms.flm_lng = 0;
            dsp_ms.main_step = MS_MODE_FILM_INN;
        }
    break;
    case MS_MODE_FILM_INN:
        if (dsp_ms.flm_lng + 20 < 100)
        {
            dsp_ms.flm_lng += 20;
        }
        else if (dsp_ms.now_mode == MS_OPTION)
        {
            if (IsEndAdpcmFadeOut())
            {
            EAdpcmCmdPlay(0, 1, AB018_STR, 0, GetAdpcmVol(AB018_STR), 0x280, 0xfff, 0);
                dsp_ms.flm_lng = 100;
                dsp_ms.main_step = MS_MODE_LOOP;
            }
        }
        else
        {
            dsp_ms.flm_lng = 100;
            dsp_ms.main_step = MS_MODE_LOOP;
        }
    break;
    case MS_MODE_LOOP:
        if (ModeSlctPad(mode))
        {
            dsp_ms.main_step = MS_MODE_FILM_OUT;
        }
    break;
    case MS_MODE_FILM_OUT:
        if (dsp_ms.flm_lng - 20 >= 1)
        {
            dsp_ms.flm_lng = dsp_ms.flm_lng - 20;
        }
        else
        {
            dsp_ms.flm_lng = 0;
            dsp_ms.main_step = MS_MODE_FADE_OUT;
        }
    break;
    case MS_MODE_FADE_OUT:
        if (dsp_ms.chr_alp - 8 >= 1)
        {
            dsp_ms.chr_alp -= 8;
            dsp_ms.win_alp = dsp_ms.chr_alp;
            dsp_ms.bak_alp = dsp_ms.chr_alp;
            
            if (
                dsp_ms.next_mode == MS_EXIT || dsp_ms.next_mode == MS_BREAK ||
                dsp_ms.next_mode == MS_CAMERA || dsp_ms.next_mode == MS_PHOT ||
                dsp_ms.next_mode == MS_SAVE || dsp_ms.next_mode == MS_STORY_EXIT
            )
            {
                dsp_ms.flm_alp = dsp_ms.chr_alp;
            }
        }
        else
        {
            dsp_ms.chr_alp = 0;
            dsp_ms.win_alp = 0;
            dsp_ms.bak_alp = 0;
            if (dsp_ms.next_mode == MS_EXIT || dsp_ms.next_mode == MS_BREAK || dsp_ms.next_mode == MS_STORY_EXIT)
            {
                dsp_ms.flm_alp = 0;
            }
            dsp_ms.main_step = MS_MODE_END;
        }
    break;
    case MS_MODE_END:
        dsp_ms.main_step = MS_MODE_START;
        if (dsp_ms.now_mode == MS_SOUND_TEST)
        {
            SeSetMVol(opt_wrk.bgm_vol);
        }
        
        if (dsp_ms.next_mode == MS_EXIT)
        {
            outgame_wrk.mode = OUTGAME_MODE_TITLE;
            EAdpcmFadeOut(0x3c);
            if (title_wrk.mode != TITLE_MODE_SEL_BGMREQ)
            {
                title_wrk.mode = TITLE_INIT_FROM_IN_BGMREQ;
            }
            title_bgm_no = -1;
        }
        else if (dsp_ms.next_mode == MS_BREAK)
        {
            outgame_wrk.mode = OUTGAME_MODE_TITLE;
        }
        else if (dsp_ms.next_mode == MS_STORY_EXIT)
        {
            ingame_wrk.game = 0;
            GameModeChange(GMC_OUT_MENU_IN);
            title_bgm_no = -1;
        }
        else
        {
            int now_mode_4 = MS_SOUND_TEST;
            dsp_ms.now_mode = dsp_ms.next_mode;
            if (dsp_ms.now_mode == now_mode_4)
            {
                SoundTestForModeSlectInit();
            }
        }
    break;
    }
    
    {
        static int calibration_preview_active = 0;
        int calibration_open =
            (dsp_ms.now_mode == 3 && MikuPan_RmlOptionsIsCalibrationOpen());

        if (calibration_open != 0)
        {
            if (calibration_preview_active == 0)
            {
                MikuPan_TitleCalibrationPreviewOpen();
            }

            calibration_preview_active = 1;
            MikuPan_TitleCalibrationPreviewDraw();
        }
        else
        {
            calibration_preview_active = 0;

            if (dsp_ms.now_mode != MS_CAMERA
                && dsp_ms.now_mode != MS_PHOT
                && dsp_ms.now_mode != MS_SAVE
                && cmn_tex_load != 0)
            {
                const bool rml_mode_select =
                    (mode == 0 || mode == 1)
                    && MikuPan_RmlModeSelectIsAvailable() != 0;
                const bool rml_options =
                    mode == 3 && MikuPan_RmlOptionsIsOpen() != 0;

                ModeSlctDspBak(dsp_ms.bak_alp, mode);
                /* The RmlUi options screen draws this film itself, so it is
                 * the same from the title and in game and stays under its
                 * own windows. */
                if (!rml_options)
                {
                    ModeSlctDspFlm(
                        dsp_ms.flm_lng,
                        dsp_ms.flm_alp,
                        mode,
                        rml_mode_select ? 1 : 0);
                }

                if (rml_mode_select)
                {
                    float opacity =
                        static_cast<float>(dsp_ms.chr_alp) / 128.0f;
                    if (dsp_ms.main_step == 5 || dsp_ms.main_step == 6)
                    {
                        const float film_opacity =
                            static_cast<float>(dsp_ms.flm_lng) / 100.0f;
                        if (film_opacity < opacity)
                        {
                            opacity = film_opacity;
                        }
                    }
                    if (mode == 0)
                    {
                        MikuPan_RmlModeSelectShowMain(dsp_ms.csr[0], opacity);
                    }
                    else
                    {
                        MikuPan_RmlModeSelectShowStory(
                            dsp_ms.csr[1],
                            dsp_ms.side,
                            sm_csr_info[0],
                            dsp_ms.sm_slct[0],
                            dsp_ms.sm_slct[1],
                            dsp_ms.sm_slct[2],
                            opacity);
                    }

                    SetSprFile(MikuPan_GetHostAddress(PL_STTS_PK2_ADDRESS));
                    DispCaption(3, dsp_ms.chr_alp);
                }
                else
                {
                    ModeSlctDspChr(dsp_ms.chr_alp, mode);
                }

                if (dsp_ms.now_mode != 3 && !rml_mode_select)
                {
                    ModeSlctDspWin(dsp_ms.win_alp);
                }
                if (!rml_mode_select)
                {
                    ModeSlctDspMsg(dsp_ms.win_alp, mode);
                }
            }
        }
    }
}

// u_char sm_slct_num[3] = {6, 2, 4};
char ModeSlctPad(u_char mode)
{
    u_char rtrn;
    u_char sm_slct_num[3] = {6, 2, 4};
    u_char bm_slct_num[1] = {4};
    int level;
    int rml_command = MIKUPAN_RML_MODE_SELECT_COMMAND_NONE;
    int rml_selection = -1;

    if ((mode == 0 || mode == 1) && MikuPan_RmlModeSelectIsAvailable() != 0)
    {
        rml_selection = MikuPan_RmlModeSelectConsumeSelection();
        rml_command = MikuPan_RmlModeSelectConsumeCommand();

        if (mode == 0 && rml_selection >= 0 && rml_selection < 5)
        {
            if (dsp_ms.csr[0] != rml_selection)
            {
                dsp_ms.csr[0] = static_cast<u_char>(rml_selection);
                SeStartFix(SE_CSR0, 0, 0x1000, 0x1000, 0);
            }
        }
        else if (mode == MS_STORY_MODE)
        {
            if (rml_selection >= sm_csr_info[0] && rml_selection < 5)
            {
                if (dsp_ms.csr[1] != rml_selection)
                {
                    dsp_ms.csr[1] = static_cast<u_char>(rml_selection);
                    dsp_ms.side = 0;
                    SeStartFix(SE_CSR0, 0, 0x1000, 0x1000, 0);
                }
            }
            else if (rml_selection >= 0)
            {
                rml_command = MIKUPAN_RML_MODE_SELECT_COMMAND_NONE;
            }

            if ((rml_command == MIKUPAN_RML_MODE_SELECT_COMMAND_LEFT
                 || rml_command == MIKUPAN_RML_MODE_SELECT_COMMAND_RIGHT)
                && dsp_ms.csr[1] < 3)
            {
                if (dsp_ms.csr[1] != 1
                    || dsp_ms.sm_slct[0] != 0
                    || (ingame_wrk.msn_no == 0 && mc_msn_flg == 1))
                {
                    dsp_ms.side = 1;
                }
                else
                {
                    rml_command = MIKUPAN_RML_MODE_SELECT_COMMAND_NONE;
                }
            }
        }
    }

    rtrn = 0;
    switch (mode)
    {
    case 0:
        if (TRIANGLE_PRESSED() == 1)
        {
            SeStartFix(SE_CANCEL, 0, 0x1000, 0x1000, 0);
            dsp_ms.next_mode = MS_EXIT;
            rtrn = 1;
        }
        else if (CROSS_PRESSED() == 1
                 || rml_command == MIKUPAN_RML_MODE_SELECT_COMMAND_CONFIRM)
        { 
            SeStartFix(SE_CLIC, 0, 0x1000, 0x1000, 0);
            dsp_ms.next_mode = dsp_ms.csr[0] + 1;
            if (dsp_ms.next_mode == MS_OPTION || dsp_ms.next_mode == MS_SOUND_TEST)
            {
                EAdpcmFadeOut(0x3c);
            }
            rtrn = 1;
        }
        else
        {
            if (
                DPAD_UP_PRESSED() == 1 ||
                (DPAD_UP_PRESSED() > 0x19 && (DPAD_UP_PRESSED() % 5) == 1) ||
                Ana2PadDirCnt(0) == 1 ||
                (Ana2PadDirCnt(0) > 0x19 && (Ana2PadDirCnt(0) % 5) == 1)
            )
            {
                SeStartFix(SE_CSR0, 0, 0x1000, 0x1000, 0);
                if (dsp_ms.csr[0])
                {
                    dsp_ms.csr[0]--;
                }
                else
                {
                    dsp_ms.csr[0] = 4;
                }
            }
            else if (
                DPAD_DOWN_PRESSED() == 1 ||
                (DPAD_DOWN_PRESSED() > 0x19 && (DPAD_DOWN_PRESSED() % 5) == 1) ||
                Ana2PadDirCnt(2) == 1 ||
                (Ana2PadDirCnt(2) > 0x19 && Ana2PadDirCnt(2) % 5 == 1)
            )
            {
                SeStartFix(SE_CSR0, 0, 0x1000, 0x1000, 0);
                if (dsp_ms.csr[0] <= 3)
                {
                    dsp_ms.csr[0]++;
                }
                else
                {
                    dsp_ms.csr[0] = 0;
                }
            }
        }
    break;
    case 1:
        if (TRIANGLE_PRESSED() == 1)
        {
            SeStartFix(SE_CANCEL, 0, 0x1000, 0x1000, 0);
            if (dsp_ms.side == 0)
            {
                dsp_ms.next_mode = dsp_ms.end_mode;
                rtrn = 1;
            }
            else
            {
                dsp_ms.side = 0;
                if (
                    dsp_ms.csr[1] == 0 &&
                    dsp_ms.sm_slct[0] == 0 &&
                    (ingame_wrk.msn_no != 0 || mc_msn_flg != 1)
                )
                {
                    dsp_ms.sm_slct[1] = ingame_wrk.difficult;
                }
            }
#ifdef BUILD_EU_VERSION
            cribo.costume = dsp_ms.sm_slct[2];
#endif
        }
        else if (CROSS_PRESSED() == 1
                 || rml_command == MIKUPAN_RML_MODE_SELECT_COMMAND_CONFIRM)
        {
            SeStartFix(SE_CLIC, 0, 0x1000, 0x1000, 0);
            switch (dsp_ms.csr[1])
            {
            case 0:
                dsp_ms.side = 1 - dsp_ms.side;
                if (
                    dsp_ms.side == 0 &&
                    dsp_ms.sm_slct[0] == 0 &&
                    (ingame_wrk.msn_no != 0 || mc_msn_flg != 1)
                )
                {
                    dsp_ms.sm_slct[1] = ingame_wrk.difficult;
                }
            break;
            case 1:
                if (
                    dsp_ms.sm_slct[0] != 0 ||
                    (ingame_wrk.msn_no == 0 && mc_msn_flg == 1)
                )
                dsp_ms.side = 1 - dsp_ms.side;
            break;
            case 2:
                dsp_ms.side = 1 - dsp_ms.side;
            break;
            case 3:
                EAdpcmFadeOut(0x3c);
                dsp_ms.next_mode = MS_STORY_EXIT;
                if (dsp_ms.sm_slct[0] == 0)
                {
                    sys_wrk.load = 1;
                    title_wrk.csr = 0;
                    ingame_wrk.mode = INGAME_MODE_NOMAL;
                }
                else
                {
                    ingame_wrk.msn_no = dsp_ms.sm_slct[0] - 1;
                    sys_wrk.load = 0;
                    mc_msn_flg = 0;
                    title_wrk.csr = 0;
                    if (ingame_wrk.msn_no != 0)
                    {
                        cribo.clear_info = cribo.clear_info & 0xfd;
                    }
                }
                ingame_wrk.difficult = dsp_ms.sm_slct[1];
                cribo.costume = dsp_ms.sm_slct[2];
                if (ingame_wrk.msn_no == 0)
                {
                    cribo.clear_info = cribo.clear_info | 2;
                }
                rtrn = 1;
            break;
            case 4:
                dsp_ms.next_mode = MS_MODE_SLCT;
#ifdef BUILD_EU_VERSION
                cribo.costume = dsp_ms.sm_slct[2];
#endif
                rtrn = 1;
            break;
            }
        }
        else
        {
            switch (dsp_ms.side)
            {
            case 0:
                if (
                    DPAD_UP_PRESSED() == 1 ||
                    (DPAD_UP_PRESSED() > 0x19 && (DPAD_UP_PRESSED() % 5) == 1) ||
                    Ana2PadDirCnt(0) == 1 ||
                    (Ana2PadDirCnt(0) > 0x19 && (Ana2PadDirCnt(0) % 5) == 1)
                )
                {
                    SeStartFix(SE_CSR0, 0, 0x1000, 0x1000, 0);
                    if (dsp_ms.csr[1] > sm_csr_info[0])
                    {
                        dsp_ms.csr[1]--;
                    }
                    else
                    {
                        dsp_ms.csr[1] = 4;
                    }
                }
                else if (
                    DPAD_DOWN_PRESSED() == 1 ||
                    (DPAD_DOWN_PRESSED() > 0x19 && (DPAD_DOWN_PRESSED() % 5) == 1) ||
                    Ana2PadDirCnt(2) == 1 ||
                    (Ana2PadDirCnt(2) > 0x19 && (Ana2PadDirCnt(2) % 5) == 1)
                )
                {
                    SeStartFix(SE_CSR0, 0, 0x1000, 0x1000, 0);
                    if (dsp_ms.csr[1] <= 3)
                    {
                        dsp_ms.csr[1]++;
                    }
                    else
                    {
                        dsp_ms.csr[1] = sm_csr_info[0];
                    }
                }
            break;
            case 1:
                if (
                    rml_command == MIKUPAN_RML_MODE_SELECT_COMMAND_LEFT ||
                    DPAD_LEFT_PRESSED() == 1 ||
                     (DPAD_LEFT_PRESSED() > 0x19 && (DPAD_LEFT_PRESSED() % 5) == 1) ||
                    Ana2PadDirCnt(3) == 1 ||
                    (Ana2PadDirCnt(3) > 0x19 && (Ana2PadDirCnt(3) % 5) == 1)
                )
                {
                    SeStartFix(SE_CSR0, 0, 0x1000, 0x1000, 0);
                    if (dsp_ms.csr[1] == 2)
                    {
                        if (dsp_ms.sm_slct[2] != 0)
                        {
                            dsp_ms.sm_slct[2]--;
                        }
                        else
                        {
                            dsp_ms.sm_slct[2] = sm_csr_info[1];
                        }
                    }
                    else
                    {
                        if (dsp_ms.sm_slct[dsp_ms.csr[1]] == 0)
                        {
                            dsp_ms.sm_slct[dsp_ms.csr[1]] = sm_slct_num[dsp_ms.csr[1]] - 1;
                        }
                        else
                        {
                            dsp_ms.sm_slct[dsp_ms.csr[1]]--;
                        }
                    }
                }
                else if (
                    rml_command == MIKUPAN_RML_MODE_SELECT_COMMAND_RIGHT ||
                    DPAD_RIGHT_PRESSED() == 1 ||
                    (DPAD_RIGHT_PRESSED() > 0x19 && (DPAD_RIGHT_PRESSED() % 5) == 1) ||
                    Ana2PadDirCnt(1) == 1 ||
                    (Ana2PadDirCnt(1) > 0x19 && (Ana2PadDirCnt(1) % 5) == 1)
                )
                {
                    SeStartFix(SE_CSR0, 0, 0x1000, 0x1000, 0);
                    if (dsp_ms.csr[1] == 2)
                    {
                        if (sm_csr_info[1] > dsp_ms.sm_slct[2])
                        {
                            dsp_ms.sm_slct[2]++;
                        }
                        else
                        {
                            dsp_ms.sm_slct[2] = 0;
                        }
                    }
                    else
                    {
                        if (sm_slct_num[dsp_ms.csr[1]] - 1 > dsp_ms.sm_slct[dsp_ms.csr[1]])
                        {
                            dsp_ms.sm_slct[dsp_ms.csr[1]]++;
                        }
                        else
                        {
                            dsp_ms.sm_slct[dsp_ms.csr[1]] = 0;
                        }
                    }
                }
            break;
            }
        }
    break;
    case MS_BATTLE_MODE:
        if (TRIANGLE_PRESSED() == 1)
        {
            SeStartFix(SE_CANCEL, 0, 0x1000, 0x1000, 0);
            if (dsp_ms.side == 0)
            {
                dsp_ms.next_mode = dsp_ms.end_mode;
                rtrn = 1;
            }
            else
            {
                dsp_ms.side = 0;
            }
        }
        else if (CROSS_PRESSED() == 1)
        {
            SeStartFix(SE_CLIC, 0, 0x1000, 0x1000, 0);
            switch (dsp_ms.csr[1])
            {
            case 0:
                dsp_ms.side = 1 - dsp_ms.side;
            break;
            case 1:
                dsp_ms.next_mode = MS_CAMERA;
                rtrn = 1;
            break;
            case 2:
                dsp_ms.next_mode = MS_PHOT;
                rtrn = 1;
            break;
            case 3:
                dsp_ms.next_mode = MS_SAVE;
                rtrn = 1;
            break;
            case 4:
                dsp_ms.next_mode = MS_MISSION_SELECT;
                btl_wrk.stage_no = 0;
                cribo.costume = dsp_ms.bm_slct[0];
                rtrn = 1;
            break;
            case MS_MODE_FILM_OUT:
                dsp_ms.next_mode = MS_MODE_SLCT;
                rtrn = 1;
            break;
            }
        }
        else
        {
            switch (dsp_ms.side)
            {
            case 0:
                if (
                    DPAD_UP_PRESSED() == 1 ||
                     (DPAD_UP_PRESSED() > 0x19 && (DPAD_UP_PRESSED() % 5) == 1) ||
                    Ana2PadDirCnt(0) == 1 ||
                    (Ana2PadDirCnt(0) > 0x19 && (Ana2PadDirCnt(0) % 5) == 1)
                )
                {
                    SeStartFix(SE_CSR0, 0, 0x1000, 0x1000, 0);
                    if (dsp_ms.csr[1] != 0)
                    {
                        dsp_ms.csr[1]--;
                    }
                    else
                    {
                        dsp_ms.csr[1] = 5;
                    }
                }
                else if (
                    DPAD_DOWN_PRESSED() == 1 ||
                    (DPAD_DOWN_PRESSED() > 0x19 && (DPAD_DOWN_PRESSED() % 5) == 1) ||
                    Ana2PadDirCnt(2) == 1 ||
                    (Ana2PadDirCnt(2) > 0x19 && (Ana2PadDirCnt(2) % 5) == 1)
                )
                {
                    SeStartFix(SE_CSR0, 0, 0x1000, 0x1000, 0);
                    if (dsp_ms.csr[1] < 5)
                    {
                        dsp_ms.csr[1]++;
                    }
                    else
                    {
                        dsp_ms.csr[1] = 0;
                    }
                }
            break;
            case 1:
                if (
                    DPAD_LEFT_PRESSED() == 1 ||
                    (DPAD_LEFT_PRESSED() > 0x19 && (DPAD_LEFT_PRESSED() % 5) == 1) ||
                    Ana2PadDirCnt(3) == 1 ||
                    (Ana2PadDirCnt(3) > 0x19 && (Ana2PadDirCnt(3) % 5) == 1)
                )
                {
                    SeStartFix(SE_CSR0, 0, 0x1000, 0x1000, 0);
                    if (dsp_ms.csr[1] == 0)
                    {
                        if (dsp_ms.bm_slct[0] != 0)
                        {
                            dsp_ms.bm_slct[0]--;
                        }
                        else
                        {
                            dsp_ms.bm_slct[0] = sm_csr_info[1];
                        }
                    }
                    else
                    {
                        if (dsp_ms.bm_slct[dsp_ms.csr[1]] == 0)
                        {
                            dsp_ms.bm_slct[dsp_ms.csr[1]] = bm_slct_num[dsp_ms.csr[1]] - 1;
                        }
                        else
                        {
                            dsp_ms.bm_slct[dsp_ms.csr[1]]--;
                        }
                    }
                    
                }
                else if (
                    DPAD_RIGHT_PRESSED() == 1 ||
                    (DPAD_RIGHT_PRESSED() > 0x19 && (DPAD_RIGHT_PRESSED() % 5) == 1) ||
                    Ana2PadDirCnt(1) == 1 ||
                    (Ana2PadDirCnt(1) > 0x19 && Ana2PadDirCnt(1) % 5 == 1)
                )
                {
                    SeStartFix(SE_CSR0, 0, 0x1000, 0x1000, 0);
                    if (dsp_ms.csr[1] == 0)
                    {
                        if (dsp_ms.bm_slct[0] < sm_csr_info[1])
                        {
                            dsp_ms.bm_slct[0]++;
                        }
                        else
                        {
                            dsp_ms.bm_slct[0] = 0;
                        }
                    }
                    else
                    {
                        if (dsp_ms.bm_slct[dsp_ms.csr[1]] < bm_slct_num[dsp_ms.csr[1]] - 1)
                        {
                            dsp_ms.bm_slct[dsp_ms.csr[1]]++;
                        }
                        else
                        {
                            dsp_ms.bm_slct[dsp_ms.csr[1]] = 0;
                        }
                    }
                }
            }
        }
        cribo.costume = dsp_ms.bm_slct[0];
    break;
    case MS_OPTION:
        if (MikuPan_RmlOptionsConsumeExitRequest())
        {
            MikuPan_RmlOptionsClose();
            SeStartFix(SE_CANCEL, 0, 0x1000, 0x1000, 0);
            EAdpcmFadeOut(0x3c);
            dsp_ms.next_mode = dsp_ms.end_mode;
            rtrn = 1;
        }
    break;
    case MS_SOUND_TEST:
        if (SoundTestForModeSlect())
        {
            SeStartFix(SE_CANCEL, 0, 0x1000, 0x1000, 0);
            dsp_ms.next_mode = dsp_ms.end_mode;
            rtrn = 1;
        }
    break;
    case MS_MISSION_SELECT:
        level = CheckClearStage();
        if (TRIANGLE_PRESSED() == 1)
        {
            SeStartFix(SE_CANCEL, 0, 0x1000, 0x1000, 0);
            dsp_ms.next_mode = MS_BATTLE_MODE;
            OutGameModeChange(OUTGAME_MODE_MODESEL);
            rtrn = 1;
        }
        else if (CROSS_PRESSED() == 1)
        {
            SeStartFix(SE_CLIC, 0, 0x1000, 0x1000, 0);
            ingame_wrk.game = 1;
            TitleModeEnd();
            sys_wrk.load = 0;
            SaveStoryWrk();
            GameModeChange(GMC_OUT_MENU_IN);
            title_bgm_no = -1;
            if (R1_PRESSED() != 0)
            {
                battle_scr_effct[btl_wrk.stage_no] = 6;
            }
            else if (R2_PRESSED() != 0)
            {
                battle_scr_effct[btl_wrk.stage_no] = 4;
            }
            else if (L1_PRESSED() != 0)
            {
                battle_scr_effct[btl_wrk.stage_no] = 2;
            }
            else if (L2_PRESSED() != 0)
            {
                battle_scr_effct[btl_wrk.stage_no] = 3;
            }
            else
            {
                battle_scr_effct[btl_wrk.stage_no] = 1;
            }
            if (btl_wrk.stage_no == 19)
            {
                battle_scr_effct[btl_wrk.stage_no] = 0;
            }
        }
        else
        {
            if (
                DPAD_UP_PRESSED() == 1 ||
                (DPAD_UP_PRESSED() > 0x19 && (DPAD_UP_PRESSED() % 5) == 1) ||
                Ana2PadDirCnt(0) == 1 ||
                (Ana2PadDirCnt(0) > 0x19 && (Ana2PadDirCnt(0) % 5) == 1)
            )
            {
                if (btl_wrk.stage_no % 5 != 0)
                {
                    SeStartFix(SE_CSR0, 0, 0x1000, 0x1000, 0);
                    btl_wrk.stage_no--;
                }
                else
                {
                    btl_wrk.stage_no += 4;
                    SeStartFix(SE_CSR0, 0, 0x1000, 0x1000, 0);
                }
            }
            else
            {
                if (
                    DPAD_DOWN_PRESSED() == 1 ||
                    (DPAD_DOWN_PRESSED() > 0x19 && (DPAD_DOWN_PRESSED() % 5) == 1) ||
                    Ana2PadDirCnt(2) == 1 ||
                    (Ana2PadDirCnt(2) > 0x19 && (Ana2PadDirCnt(2) % 5) == 1)
                )
                {
                    if ((btl_wrk.stage_no + 1) % 5 != 0)
                    {
                        SeStartFix(SE_CSR0, 0, 0x1000, 0x1000, 0);
                        btl_wrk.stage_no++;
                    }
                    else
                    {
                        btl_wrk.stage_no -= 4;
                        SeStartFix(SE_CSR0, 0, 0x1000, 0x1000, 0);
                    }
                }
                else
                {
                    if (
                        DPAD_LEFT_PRESSED() == 1 ||
                        (DPAD_LEFT_PRESSED() > 0x19 && (DPAD_LEFT_PRESSED() % 5) == 1) ||
                        Ana2PadDirCnt(3) == 1 ||
                        (Ana2PadDirCnt(3) > 0x19 && (Ana2PadDirCnt(3) % 5) == 1)
                    )
                    {
                        if (btl_wrk.stage_no >= 5)
                        {
                            SeStartFix(SE_CSR0, 0, 0x1000, 0x1000, 0);
                            btl_wrk.stage_no -= 5;
                        }
                    }
                    else if (
                        DPAD_RIGHT_PRESSED() == 1 ||
                        (DPAD_RIGHT_PRESSED() > 0x19 && (DPAD_RIGHT_PRESSED() % 5) == 1) ||
                        Ana2PadDirCnt(1) == 1 ||
                        (Ana2PadDirCnt(1) > 0x19 && (Ana2PadDirCnt(1) % 5) == 1)
                    )
                    {
                        if (btl_wrk.stage_no <= 0xe && btl_wrk.stage_no / 5 < level)
                        {
                            SeStartFix(SE_CSR0, 0, 0x1000, 0x1000, 0);
                            btl_wrk.stage_no += 5;
                        }
                    }
                }
            }
        }
    break;
    case MS_CAMERA:
        if (!isCameraEnd())
        {
            BgFusumaYW(0x606060, 0.0f, GetCamDispAlpha(), 0x7d000);
#ifdef BUILD_EU_VERSION
            SetSprFile(MikuPan_GetHostAddress(EVENT_ADDRESS));
            SetSprFile(MikuPan_GetHostAddress(PL_STTS_PK2_ADDRESS));
            SetSprFile(MikuPan_GetHostAddress(MODE_SELECT_ADDRESS));
#else
            SetSprFile(MikuPan_GetHostAddress(MISSION_TITLE_CARD_ADDRESS));
            SetSprFile(MikuPan_GetHostAddress(PL_STTS_PK2_ADDRESS));
            SetSprFile(MikuPan_GetHostAddress(PL_MTOP_PK2_ADDRESS));
#endif
            CameraCustomMain();
        }
        else
        {
            dsp_ms.next_mode = 2;
            OutGameModeChange(OUTGAME_MODE_MODESEL);
            rtrn = 1;
        }
    break;
    case MS_PHOT:
        if (!isPhotoEnd())
        {
            BgFusumaYW(0x606060, 0.0f, GetPhtDispAlpha(), 0x7d000);
#ifdef BUILD_EU_VERSION
            SetSprFile(MikuPan_GetHostAddress(MODE_SELECT2_ADDRESS));
            SetSprFile(MikuPan_GetHostAddress(PL_STTS_PK2_ADDRESS));
            SetSprFile(MikuPan_GetHostAddress(MODE_SELECT_ADDRESS));
#else
            SetSprFile(MikuPan_GetHostAddress(PL_PHOT_PK2_ADDRESS));
            SetSprFile(MikuPan_GetHostAddress(PL_STTS_PK2_ADDRESS));
            SetSprFile(MikuPan_GetHostAddress(PL_MTOP_PK2_ADDRESS));
#endif
            IngameMenuAlbum(1);
        }
        else
        {
            dsp_ms.next_mode = 2;
            OutGameModeChange(OUTGAME_MODE_MODESEL);
            rtrn = 1;
        }
    break;
    case MS_SAVE:
        if (SavePointMenuMain(3))
        {
            dsp_ms.next_mode = MS_BATTLE_MODE;
            OutGameModeChange(OUTGAME_MODE_MODESEL);
            rtrn = 1;
        }
    break;
    }
    return rtrn;
}

void ModeSlctDspBak(u_char alp, u_char mode)
{
    int i;
    DISP_SPRT ds;

    if (alp == 0x0)
    {
        return;
    }
    
    SetSprFile(MikuPan_GetHostAddress(M_SLCT_CMN_PK2_ADDRESS));
    SetSprFile(MikuPan_GetHostAddress(M_SLCT_STY_PK2_ADDRESS));
    
    for (i = 0; i < 11; i++)
    {
        switch (mode)
        {
        case MS_MODE_START:
            CopySprDToSpr(&ds, &msl_bg_fsm[i]);
            break;
        case MS_MODE_INIT:
            CopySprDToSpr(&ds, &msl_bg_sty[i]);
            ds.r = 0x64;
            ds.g = 0x64;
            ds.b = 0x64;
            break;
        case MS_MODE_FADE_INN:
            CopySprDToSpr(&ds, &msl_bg_btl[i]);
            ds.r = 0x64;
            ds.g = 0x64;
            ds.b = 0x64;
            break;
        case MS_MODE_FILM_INN:
            CopySprDToSpr(&ds, &msl_bg_opt[i]);
            break;
        case MS_MODE_LOOP:
            CopySprDToSpr(&ds, &msl_bg_snd[i]);
            break;
        case MS_MODE_END:
            CopySprDToSpr(&ds, &msl_bg_btl[i]);
            if (0x50 < alp)
            {
                alp = 0x50;
            }
        }
        ds.alpha = alp;
        DispSprD(&ds);
    }
}

void ModeSlctDspChr(u_char alp, u_char mode)
{
    u_char dsp_rgb[11];
    u_char dsp_alp[11];
    float flsh;

    if (alp != 0x0)
    {
        flsh = SgSinf(((dsp_ms.mode_tmr % 0x3c) * 6.2831855f) / 60.0f);
        flsh = ((flsh + 1.0f) * 0.5f + 1.0f) * 0.5f;

        switch (mode)
        {
        case MS_MODE_SLCT:
            DspModeSlect(alp);
        break;
        case MS_STORY_MODE:
            DspStoryMode(alp, flsh);
        break;
        case MS_BATTLE_MODE:
            DspBattleMode(alp, flsh);
        break;
        case MS_OPTION:
            /* RmlUi owns the PC options menu. Keep the mode-select
             * background/frame, but do not draw the original PS2 option page.
             */
            //SetSprFile(MikuPan_GetHostAddress(PL_STTS_PK2_ADDRESS));
            //SetSprFile(MikuPan_GetHostAddress(PL_OPTI_PK2_ADDRESS));
            //DspOptCtrl(dsp_ms.csr[2], dsp_ms.opt_mode, alp, 0x0);
        break;
        case MS_SOUND_TEST:
            SoundTestForModeSlectDisp(alp, flsh);
        break;
        case MS_MISSION_SELECT:
            SetSprFile(MikuPan_GetHostAddress(M_SLCT_BTL_MSN_PK2_ADDRESS));
            DspMissionSelect(alp);
        }
    }
}

void DspModeSlect(u_char alp)
{
    int i;
    u_char dsp_rgb[5];
    u_char dsp_alp[5];
    
    for (i = 0; i < 5; i++)
    {
        if (i == dsp_ms.csr[0])
        {
            dsp_alp[i] = alp;
            dsp_rgb[i] = 0x80;
        }
        else
        {
            dsp_alp[i] = alp / 2;
            dsp_rgb[i] = 0x40;
        }
    };
    
    PutChrOne(TM_MS_STORY_MODE, 0, 0, dsp_rgb[0], dsp_alp[0], 0x0);
    PutChrOne(TM_MS_BATTLE_MODE, 0, 0, dsp_rgb[1], dsp_alp[1], 0x0);
    PutChrOne(TM_MS_OPTION, 0, 0, dsp_rgb[2], dsp_alp[2], 0x0);
    PutChrOne(TM_MS_SOUND_TEST, 0, 0, dsp_rgb[3], dsp_alp[3], 0x0);
    PutChrOne(TM_MS_EXIT, 0, 0, dsp_rgb[4], dsp_alp[4], 0x0);
    
    SetSprFile(MikuPan_GetHostAddress(PL_STTS_PK2_ADDRESS));
    DispCaption(3, alp);
}

void DspStoryMode(u_char alp, float flsh)
{
    int i;
    u_int dsp_rgb[11];
    u_char dsp_alp[11];

    for (i = 0; i < 5; i++)
    {
        if (i == dsp_ms.csr[1])
        {
            if (dsp_ms.side)
            {
                dsp_alp[i] = (alp * 7) / 10;
                dsp_rgb[i] = 0x595959;
            }
            else
            {
                dsp_alp[i] = alp;
                dsp_rgb[i] = 0x808080;
            }
        }
        else
        {
            if (sm_csr_info[0] <= i)
            {
                dsp_alp[i] = alp / 2;
                dsp_rgb[i] = 0x404040;
            }
            else
            {
                dsp_alp[i] = dsp_ms.unsel_col[3];
                dsp_rgb[i] =
                    dsp_ms.unsel_col[0] |
                    dsp_ms.unsel_col[1] << 8 |
                    dsp_ms.unsel_col[2] << 16;
            }
        }
    }
    
    for (i = 0; i < 3; i++)
    {
        if ((dsp_ms.side) && (i == dsp_ms.csr[1] ))
        {
            dsp_alp[(i+8)-3] = alp;
            dsp_rgb[(i+8)-3] = 0x808080;
            dsp_alp[i+8] = alp * flsh;
            dsp_rgb[i+8] = 0x808080;
            
        }
        else
        {
            if (i >= sm_csr_info[0])
            {
                dsp_alp[(i+8)-3] = alp / 2;
                dsp_rgb[(i+8)-3] = 0x404040;
                dsp_alp[i+8] = alp / 2;
                dsp_rgb[i+8] = 0x808080;
            }
            else
            {
                dsp_alp[(i+8)-3] = dsp_ms.unsel_col[3];
                dsp_rgb[(i+8)-3] = 
                    dsp_ms.unsel_col[0] |
                    dsp_ms.unsel_col[1] << 8 |
                    dsp_ms.unsel_col[2] << 16;
                dsp_alp[i+8] = dsp_ms.unsel_col[3];
                dsp_rgb[i+8] =
                    dsp_ms.unsel_col[0] |
                    dsp_ms.unsel_col[1] << 8 |
                    dsp_ms.unsel_col[2] << 16;
            }
        }
    }
    
    PutChrOneRGB(TM_SM_CHAPTER, 0, 0, dsp_rgb[0], dsp_alp[0], 0x0);
    PutChrOneRGB(TM_SM_DIFFICULTY, 0, 0, dsp_rgb[1], dsp_alp[1], 0x0);
    PutChrOneRGB(TM_SM_COSTUME, 0, 0, dsp_rgb[2], dsp_alp[2], 0x0);
    PutChrOneRGB(TM_SM_GAME_START, 0, 0, dsp_rgb[3], dsp_alp[3], 0x0);
    PutChrOneRGB(TM_SM_EXIT, 0, 0, dsp_rgb[4], dsp_alp[4], 0x0);
    PutChrOneRGB(TM_SM_PLAY_S, 0x148, 0x65, dsp_rgb[8], dsp_alp[8], 0x2);
    PutChrOneRGB(TM_SM_PLAY_S, 0x23a, 0x65, dsp_rgb[8], dsp_alp[8], 0x0);
    PutChrOneRGB(TM_SM_KAKKO_L, 0x15c, 100, dsp_rgb[8], dsp_alp[8], 0x0);
    PutChrOneRGB(TM_SM_KAKKO_R, 0x225, 100, dsp_rgb[8], dsp_alp[8], 0x0);
    
    switch (dsp_ms.sm_slct[0])
    {
    case 0:
        PutChrOneRGB(TM_SM_CONT, 0, 0, dsp_rgb[5], dsp_alp[5], 0x0);
    break;
    case 1:
        PutChrOneRGB(TM_SM_SHW0, 0, 0, dsp_rgb[5], dsp_alp[5], 0x0);
    break;
    case 2:
        PutChrOneRGB(TM_SM_SHW1, 0, 0, dsp_rgb[5], dsp_alp[5], 0x0);
    break;
    case 3:
        PutChrOneRGB(TM_SM_SHW2, 0, 0, dsp_rgb[5], dsp_alp[5], 0x0);
    break;
    case 4:
        PutChrOneRGB(TM_SM_SHW3, 0, 0, dsp_rgb[5], dsp_alp[5], 0x0);
    break;
    case MS_MODE_FILM_OUT:
        PutChrOneRGB(TM_SM_SHW4, 0, 0, dsp_rgb[5], dsp_alp[5], 0x0);
    break;
    }
    
    PutChrOneRGB(TM_SM_PLAY_S, 0x148, 0x91, dsp_rgb[9], dsp_alp[9], 0x2);
    PutChrOneRGB(TM_SM_PLAY_S, 0x23a, 0x91, dsp_rgb[9], dsp_alp[9], 0x0);
    PutChrOneRGB(TM_SM_KAKKO_L, 0x15c, 0x90, dsp_rgb[9], dsp_alp[9], 0x0);
    PutChrOneRGB(TM_SM_KAKKO_R, 0x225, 0x90, dsp_rgb[9], dsp_alp[9], 0x0);
    
    switch (dsp_ms.sm_slct[1])
    {
    case 0:
        PutChrOneRGB(TM_SM_NORMAL, 0x17e, 0x8d, dsp_rgb[6], dsp_alp[6], 0x0);
    break;
    case 1:
        PutChrOneRGB(TM_SM_NIGHTMARE, 0xfffb, 0, dsp_rgb[6], dsp_alp[6], 0x0);
    break;
    }
    
    PutChrOneRGB(TM_SM_PLAY_S, 0x148, 0xc0, dsp_rgb[10], dsp_alp[10], 0x2);
    PutChrOneRGB(TM_SM_PLAY_S, 0x23a, 0xc0, dsp_rgb[10], dsp_alp[10], 0x0);
    PutChrOneRGB(TM_SM_KAKKO_L, 0x15c, 0xbf, dsp_rgb[10], dsp_alp[10], 0x0);
    PutChrOneRGB(TM_SM_KAKKO_R, 0x225, 0xbf, dsp_rgb[10], dsp_alp[10], 0x0);

    switch (dsp_ms.sm_slct[2])
    {
    case 0:
        PutChrOneRGB(TM_SM_NORMAL, 0x17e, 0xbb, dsp_rgb[7], dsp_alp[7], 0x0);
    break;
    case 1:
        PutChrOneRGB(TM_SM_SPECIAL, 0xfff5, 0, dsp_rgb[7], dsp_alp[7], 0x0);
        PutChrOneRGB(TM_SM_1, 0x20f, 0xbb, dsp_rgb[7], dsp_alp[7], 0x0);
    break;
    case 2:
        PutChrOneRGB(TM_SM_SPECIAL, 0xfff5, 0, dsp_rgb[7], dsp_alp[7], 0x0);
        PutChrOneRGB(TM_SM_2, 0x20f, 0xbb, dsp_rgb[7], dsp_alp[7], 0x0);
    break;
    case 3:
        PutChrOneRGB(TM_SM_SPECIAL, 0xfff5, 0, dsp_rgb[7], dsp_alp[7], 0x0);
        PutChrOneRGB(TM_SM_3, 0x20f, 0xbb, dsp_rgb[7], dsp_alp[7], 0x0);
    break;
    }
    
    SetSprFile(MikuPan_GetHostAddress(PL_STTS_PK2_ADDRESS));
    DispCaption(3, alp);
}

void DspBattleMode(u_char alp, float flsh)
{
    int i;
    u_char dsp_rgb[9];
    u_char dsp_alp[9];

    for (i = 0; i < 6; i++)
    {
        if (i == dsp_ms.csr[1])
        {
            if (dsp_ms.side)
            {
                dsp_alp[i] = (alp * 7) / 10;
                dsp_rgb[i] = 0x59;
            }
            else
            {
                dsp_alp[i] = alp;
                dsp_rgb[i] = 0x80;
            }
        }
        else
        {
            dsp_alp[i] = alp / 2;
            dsp_rgb[i] = 0x40;
        }
    }
    
    for (i = 0; i < 1; i++)
    {
        if (dsp_ms.side && i == dsp_ms.csr[1])
        {
            dsp_alp[i+6] = alp;
            dsp_rgb[i+6] = 0x80;
            dsp_rgb[i+7] = 0x80;
            dsp_alp[i+7] = alp * flsh;
        }
        else
        {
            dsp_alp[i+6] = alp / 2;
            dsp_rgb[i+6] = 0x40;
            dsp_rgb[i+7] = 0x80;
            dsp_alp[i+7] = alp / 2;
        }
    }
    
    PutChrOne(TM_BM_COSTUME, 0, 0xffe7, dsp_rgb[0], dsp_alp[0], 0x0);
#ifdef BUILD_EU_VERSION
    // nothing to do
#else
    PutChrOne(TM_BM_CAMERA, 0, 0xffec, dsp_rgb[1], dsp_alp[1], 0x0);
#endif
    PutChrOne(TM_BM_SETTING, 0, 0xffec, dsp_rgb[1], dsp_alp[1], 0x0);
    PutChrOne(TM_BM_PHOT, 0, 0xfff1, dsp_rgb[2], dsp_alp[2], 0x0);
    PutChrOne(TM_BM_SAVE, 0, 0xfff6, dsp_rgb[3], dsp_alp[3], 0x0);
    
    PutChrOne(TM_BM_MISSION_SELECT, 0, 0xfffb, dsp_rgb[4], dsp_alp[4], 0x0);
    PutChrOne(TM_BM_EXIT, 0, 0, dsp_rgb[5], dsp_alp[5], 0x0);
    PutChrOne(TM_BM_PLAY_S, 0x13a, 99, dsp_rgb[7], dsp_alp[7], 0x2);
    PutChrOne(TM_BM_PLAY_S, 0x248, 99, dsp_rgb[7], dsp_alp[7], 0x0);
    PutChrOne(TM_BM_KAKKO_L, 0x150, 0x62, dsp_rgb[7], dsp_alp[7], 0x0);
    PutChrOne(TM_BM_KAKKO_R, 0x230, 0x62, dsp_rgb[7], dsp_alp[7], 0x0);

    switch (dsp_ms.bm_slct[0])
    {
    case 0:
        PutChrOne(TM_BM_NORMAL, 0x17b, 0x5f, dsp_rgb[6], dsp_alp[6], 0x0);
    break;
    case 1:
        PutChrOne(TM_BM_SPECIAL, 0xfff2, 0xffd2, dsp_rgb[6], dsp_alp[6], 0x0);
        PutChrOne(TM_BM_1, 0x20c, 0x5f, dsp_rgb[6], dsp_alp[6], 0x0);
    break;
    case 2:
        PutChrOne(TM_BM_SPECIAL, 0xfff2, 0xffd2, dsp_rgb[6], dsp_alp[6], 0x0);
        PutChrOne(TM_BM_2, 0x20c, 0x5f, dsp_rgb[6], dsp_alp[6], 0x0);
    break;
    case 3:
        PutChrOne(TM_BM_SPECIAL, 0xfff2, 0xffd2, dsp_rgb[6], dsp_alp[6], 0x0);
        PutChrOne(TM_BM_3, 0x20c, 0x5f, dsp_rgb[6], dsp_alp[6], 0x0);
    break;
    }
    
    SetSprFile(MikuPan_GetHostAddress(PL_STTS_PK2_ADDRESS));
    DispCaption(3, alp);
}

// static u_char csr_alpha = 0;
// static u_char csr_brink = 1;
void DspMissionSelect(u_char alp)
{
    int i;
    int ene_no;
    int msn_no;
    int rank;
    int no;
    int page_base_no;
    static u_char csr_alpha = 0;
    static u_char csr_brink = 1;
    int level;
    
    page_base_no = btl_wrk.stage_no;
    no = (page_base_no / 5) * 5;
    
    level = CheckClearStage();
    
    PutChrOne(TM_NS_MN_BT_BS, 0, 0, 0x80, alp, 0x0);
    PutChrOne(TM_NS_RANK, 0, 0, 0x80, alp, 0x0);

    for (i = 0; i <= 4; i++)
    {
        PutChrOne(TM_NS_CHK_BOX, 0, i * 0x28, 0x80, alp, 0);
    }
    
    for (i = 0; i <= 4; i++)
    {
        if (stage_wrk[no + i].rank)
        {
            PutChrOne(TM_NS_CHK_RED, 0, i * 0x28, 0x80, alp, 0x0);
        }
    }
    
    if (csr_brink)
    {
        csr_alpha++;
        if (csr_alpha > 100)
        {
            csr_brink = 0;
        }
    }
    else
    {
        csr_alpha--;
        if (csr_alpha < 0x14)
        {
            csr_brink = 1;
        }
    }
    // iVar8 = (btl_wrk.stage_no % 5) * 0x28;
    SetSquareS(0x25000, -240.0f, (page_base_no % 5) * 0x28 -0x5e, 196.0f, (page_base_no % 5) * 0x28 - 0x40, 0x8c, 0x80, 0x3c, csr_alpha);

    if (page_base_no > 4)
    {
        PutChrOne(TM_NS_NEXT_TRI, 0x17, 0xcb, 0x80, alp, 0x0);
    }
    
    if ((page_base_no < 0xf) && (no / 5 < level))
    {
        PutChrOne(TM_NS_NEXT_TRI, 0x24b, 0xcb, 0x80, alp, 0x2);
    }
    
    for (i = 0; i < 5; i++)
    {
        PutStringYW(0x2e, 0xa, 0x55, i * 0x28 + 0x85, 0x808080, alp, 0x23000, 0x0);
        PutNumberYW(0x3, no + i + 1, 0xcc, i * 0x28 + 0x85, 1.0, 1.0, 0x808080, alp, 0x23000, 2, 1);
        PutNumberYW(0x3, stage_wrk[no + i].best_time / 0xe10, 0x104, i * 0x28 + 0x85, 1.0, 1.0, 0x808080, alp, 0x23000, 1, 1);
        PutStringYW(0x2e, 0xb, 0x111, i * 0x28 + 0x85, 0x808080, alp, 0x23000, 0x0);
        PutNumberYW(0x3, (stage_wrk[no + i].best_time / 0x3c) % 0x3c, 0x11f, i * 0x28 + 0x85, 1.0, 1.0, 0x808080, alp, 0x23000, 2, 1);
        PutStringYW(0x2e, 0xb, 0x13e, i * 0x28 + 0x85, 0x808080, alp, 0x23000, 0x0);
        PutNumberYW(0x3, ((stage_wrk[no + i].best_time * 100) / 0x3c) % 100, 0x14a, i * 0x28 + 0x85, 1.0, 1.0, 0x808080, alp, 0x23000, 2, 1);
        PutNumberYW(0x3, stage_wrk[no + i].best_shot, 0x193, i * 0x28 + 0x85, 1.0, 1.0, 0x808080, alp, 0x23000, 4, 0);
        PutStringYW(0x2e, 0xc, 0x1d4, i * 0x28 + 0x85, 0x808080, alp, 0x23000, 0x0);

        rank = stage_wrk[no + i].rank;
        if ((rank ^ 1) == 0)
        {
            rank = 2;
        }
        
        switch (rank)
        {
        case 0:
        break;
        case 1:
            PutChrOne(TM_NS_E, 0, i * 0x28, 0x80, alp, 0x0);
        break;
        case 2:
            PutChrOne(TM_NS_D, 0, i * 0x28, 0x80, alp, 0x0);
        break;
        case 3:
            PutChrOne(TM_NS_C, 0, i * 0x28, 0x80, alp, 0x0);
        break;
        case 4:
            PutChrOne(TM_NS_B, 0, i * 0x28, 0x80, alp, 0x0);
        break;
        case MS_MODE_FILM_OUT:
            PutChrOne(TM_NS_A, 0, i * 0x28, 0x80, alp, 0x0);
        break;
        case MS_MODE_FADE_OUT:
            PutChrOne(TM_NS_S, 0, i * 0x28, 0x80, alp, 0x0);
        break;
        }
    }

    for (i = 0; i <= 2; i++)
    {
         ene_no = stage_dat[btl_wrk.stage_no].ene_no[i];
         msn_no = stage_dat[btl_wrk.stage_no].map_no;

        if (ene_no == 0xff)
        {
            break;
        }
                
        if (ene_no == 7 && msn_no == 3)
        {
            PutRayName(2, 1, 3, 0x50, i * 0x18 + 0x165, 0x808080, alp);
        }
        else
        {
            PutRayName(1, ene_no, msn_no, 0x50, i * 0x18 + 0x165, 0x808080, alp);
        }
    }

    SetSprFile(MikuPan_GetHostAddress(PL_STTS_PK2_ADDRESS));
    DispCaption(4, alp);
}

CLEAR_BONUS cribo = {0, 0};

void PutChrOne(u_short chr, short int px, short int py, u_char rgb, u_char alp, u_char flp)
{
    DISP_SPRT ds;

    CopySprDToSpr(&ds, &msel_sprt[chr]);

    ds.x += px;
    ds.y += py;
    ds.alpha = alp;
    ds.r = rgb;
    ds.g = rgb;
    ds.b = rgb;
    ds.tex1 = 0x161;

    switch(flp)
    {
    case 1:
        ds.att |= 1;
    break;
    case 2:
        ds.att |= 2;
    break;
    case 3:
        ds.att |= 3;
    break;
    }
    
    DispSprD(&ds);
}

void PutChrOneRGB(u_short chr, short int px, short int py, u_int rgb, u_char alp, u_char flp)
{
    DISP_SPRT ds;

    u_char r = rgb;
    u_char g = rgb >> 8;
    u_char b = rgb >> 16;
    
    CopySprDToSpr(&ds, &msel_sprt[chr]);

    ds.x += px;
    ds.y += py;
    ds.alpha = alp;
    ds.r = r;
    ds.g = g;
    ds.b = b;
    ds.tex1 = 0x161;
    
    switch(flp)
    {
    case 1:
        ds.att |= 1;
    break;
    case 2:
        ds.att |= 2;
    break;
    case 3:
        ds.att |= 3;
    break;
    }

    DispSprD(&ds);
}

void ModeSlctDspFlm(u_char per, u_char alp, u_char mode, u_char hide_title)
{
    short int mx;
    DISP_SPRT ds;

    if (alp == 0)
    {
        return;
    }

    mx = ((100 - per) * 0x1f9) / 100.0f;

    FilmCutter(TM_FLM_OBI, -mx, 0, 0x59, 0x80, alp);

    if (hide_title == 0)
    {
        switch (mode)
        {
        case 0:
            FilmCutter(TM_FLM_M_SLCT, -mx, 0, 0x59, 0x80, alp);
        break;
        case 1:
            FilmCutter(TM_FLM_S_MODE, -mx, 0, 0x59, 0x80, alp);
        break;
        case MS_MODE_FADE_INN:
            FilmCutter(TM_FLM_B_MODE, -mx, 0, 0x59, 0x80, alp);
        break;
        case MS_MODE_FILM_INN:
            FilmCutter(TM_FLM_OPTION, -mx, 0, 0x59, 0x80, alp);
        break;
        case MS_MODE_LOOP:
            FilmCutter(TM_FLM_S_TEST, -mx, 0, 0x59, 0x80, alp);
        break;
        case MS_MODE_END:
            FilmCutter(TM_FLM_MSN_SL, -mx, 0, 0x59, 0x80, alp);
        break;
        }
    }

    PutChrOne(7, 0, 0, 0x80, alp, 0x0);
}

void FilmCutter(u_short chr, short int px, short int py, short int ex, u_char rgb, u_char alp)
{
    short int cut;
    DISP_SPRT ds;
    
    CopySprDToSpr(&ds, &msel_sprt[chr]);
    
    cut = 0;
    
    if (ds.x + px < ex)
    {
        cut = ex - (ds.x + px);
        px = ex - ds.x;
        if (ds.w < cut)
        {
            cut = ds.w;
        }
    }
    
    ds.x += px;
    ds.y += py;
    ds.u += cut;
    ds.w -= cut;
    ds.alpha = alp;
    ds.r = rgb;
    ds.g = rgb;
    ds.b = rgb;
    ds.tex1 = 0x161;
    
    DispSprD(&ds);
}

void ModeSlctDspWin(u_char alp)
{
    DISP_SPRT ds;
    
    if (alp != 0x0)
    {
        DrawMessageBox(0x19000, 24.0f, 340.0f, 593.0f, 84.0f, alp);
    }
}

void MsLoadCtrl(u_char mode)
{    
    switch(mode)
    {
    case MS_MODE_SLCT:
        ms_load_id = LoadReq(EFF001_PK2, EFFECT_ADDRESS);
#ifdef BUILD_EU_VERSION
        ms_load_id = LoadReqLanguage(M_SLCT_FSM_E_PK2,0xcc0470);
#else
        ms_load_id = LoadReq(M_SLCT_FSM_PK2,M_SLCT_STY_PK2_ADDRESS);
#endif
    break;
    case MS_STORY_MODE:
        ingame_wrk.game = 0;
#ifdef BUILD_EU_VERSION
        ms_load_id = LoadReqLanguage(M_SLCT_STY_E_PK2, 0xcc0470);
#else
        ms_load_id = LoadReq(M_SLCT_STY_PK2, M_SLCT_STY_PK2_ADDRESS);
#endif
        if (ingame_wrk.difficult != 0x0)
        {
            dsp_ms.sm_slct[1] = 0x1;
        }
        
    break;
    case MS_BATTLE_MODE:
        ingame_wrk.game = 1;
#ifdef BUILD_EU_VERSION
        ms_load_id = LoadReqLanguage(M_SLCT_BTL_E_PK2, 0xcc0470);
        ms_load_id = LoadReqLanguage(M_SLCT_BTL_CHR_E_PK2, 0xd4a850);
#else
        ms_load_id = LoadReq(M_SLCT_BTL_PK2, M_SLCT_STY_PK2_ADDRESS);
        ms_load_id = LoadReq(M_SLCT_BTL_CHR_PK2, M_SLCT_BTL_CHR_PK2_ADDRESS);
#endif
        
    break;
    case MS_OPTION:
#ifdef BUILD_EU_VERSION
        ms_load_id = LoadReqLanguage(M_SLCT_OPT_E_PK2, 0xcc0470);
        ms_load_id = LoadReqLanguage(PL_OPTI_E_PK2, 0xddc430);
        ms_load_id = LoadReqLanguage(PL_STTS_E_PK2, 0x1ce0000);
#else
        ms_load_id = LoadReq(M_SLCT_OPT_PK2, M_SLCT_STY_PK2_ADDRESS);
        ms_load_id = LoadReq(PL_OPTI_PK2, PL_OPTI_PK2_ADDRESS);
        ms_load_id = LoadReq(PL_STTS_PK2, PL_STTS_PK2_ADDRESS);
#endif
        break;
    case MS_SOUND_TEST:
#ifdef BUILD_EU_VERSION
        ms_load_id = LoadReqLanguage(M_SLCT_SND_E_PK2, 0xcc0470);
#else
        ms_load_id = LoadReq(M_SLCT_SND_PK2, M_SLCT_STY_PK2_ADDRESS);
#endif
    break;
    case MS_MISSION_SELECT:
#ifdef BUILD_EU_VERSION
        if (cmn_tex_load == 0)
        {
            ms_load_id = LoadReqLanguage(M_SLCT_BTL_E_PK2, 0xcc0470);
        }        
        ms_load_id = LoadReqLanguage(M_SLCT_BTL_MSN_E_PK2, 0xdcb100);
#else
        if (cmn_tex_load == 0)
        {
            ms_load_id = LoadReq(M_SLCT_BTL_PK2, M_SLCT_STY_PK2_ADDRESS);
        }        
        ms_load_id = LoadReq(M_SLCT_BTL_MSN_PK2, M_SLCT_BTL_MSN_PK2_ADDRESS);
#endif
    break;
    case MS_CAMERA:
        CameraCustomInit();
        OutGameInitCamera();
#ifdef BUILD_EU_VERSION
        ms_load_id = LoadReq(PL_BGBG_PK2, 0x1cfefc0);
        ms_load_id = LoadReqLanguage(PL_MTOP_E_PK2, 0x1d10000);
        ms_load_id = LoadReqLanguage(PL_STTS_E_PK2, 0x1ce0000);
        ms_load_id = LoadReqLanguage(PL_CAME_E_PK2, 0x1e90000);
#else
        ms_load_id = LoadReq(PL_BGBG_PK2, PL_BGBG_PK2_ADDRESS);
        ms_load_id = LoadReq(PL_MTOP_PK2, PL_MTOP_PK2_ADDRESS);
        ms_load_id = LoadReq(PL_STTS_PK2, PL_STTS_PK2_ADDRESS);
        ms_load_id = LoadReq(PL_CAME_PK2, MISSION_TITLE_CARD_ADDRESS);
#endif
    break;
    case MS_PHOT:
#ifdef BUILD_EU_VERSION
        ms_load_id = LoadReq(PL_BGBG_PK2, 0x1cfefc0);
        ms_load_id = LoadReqLanguage(PL_MTOP_E_PK2, 0x1d10000);
        ms_load_id = LoadReqLanguage(PL_STTS_E_PK2, 0x1ce0000);
        ms_load_id = LoadReqLanguage(PL_PHOT_E_PK2, 0x1d51db0);
#else
        ms_load_id = LoadReq(PL_BGBG_PK2, PL_BGBG_PK2_ADDRESS);
        ms_load_id = LoadReq(PL_MTOP_PK2, PL_MTOP_PK2_ADDRESS);
        ms_load_id = LoadReq(PL_STTS_PK2, PL_STTS_PK2_ADDRESS);
        ms_load_id = LoadReq(PL_PHOT_PK2, PL_PHOT_PK2_ADDRESS);
#endif
        StartAlbumModeInit();
        OutGameInitPhoto();
    break;
    case MS_SAVE:
        motInitMsn();
#ifdef BUILD_EU_VERSION
        ms_load_id = LoadReq(PL_BGBG_PK2, 0x1cfefc0);
#else
        ms_load_id = LoadReq(PL_BGBG_PK2, PL_BGBG_PK2_ADDRESS);
#endif
        BtlModSaveInit();
    break;
    }
}

void TitleModeEnd()
{
    dsp_ms.main_step = MS_MODE_START;
    dsp_ms.flm_alp = 0;
    dsp_ms.win_alp = 0;
    dsp_ms.bak_alp = 0;
    dsp_ms.chr_alp = 0;
    dsp_ms.flm_lng = 0;
    
    if (dsp_ms.next_mode == MS_EXIT)
    {
        title_wrk.mode = TITLE_INIT_FROM_IN_BGMREQ;
    }
    else if (dsp_ms.next_mode == MS_BREAK)
    {
        outgame_wrk.mode = OUTGAME_MODE_TITLE;
    }
    else
    {
        dsp_ms.now_mode = dsp_ms.next_mode;
    }
    
    cmn_tex_load = 0;
}

void ChangeModeSel(int mode)
{
    dsp_ms.next_mode = mode;
    dsp_ms.now_mode = dsp_ms.next_mode;
    OutGameModeChange(OUTGAME_MODE_MODESEL);
}

void BattleModeExit()
{
    ChangeModeSel(MS_BATTLE_MODE);
}

void BattleModeNext()
{
    ChangeModeSel(MS_MISSION_SELECT);
}

int CheckClearStage()
{
    int i;
    int j;
    
    for (i = 0; i < 4; i++)
    {
        for (j = 0; j < 5; j++)
        {
            if (stage_wrk[i*5+j].rank == 0)
            {
                break;
            }
        }
        
        if (j != 5)
        {
            break;
        }
    }
    
    return i;
}

void ModeSlctDspMsg(u_char alp, u_char mode)
{
    switch (mode)
    {
    case MS_MODE_START:
        PutStringYW(IGMSG_MODE_SLCT, dsp_ms.csr[0], 0x50, 0x165, 0x808080, alp, 0x1000, 0x0);
    break;
    case MS_MODE_INIT:
        PutStringYW(IGMSG_MODE_SLCT, dsp_ms.csr[1] + 0x5, 0x50, 0x165, 0x808080, alp, 0x1000, 0x0);
    break;
    case MS_MODE_FADE_INN:
        PutStringYW(IGMSG_MODE_SLCT, dsp_ms.csr[1] + 0xa, 0x50, 0x165, 0x808080, alp, 0x1000, 0x0);
    break;
    }
}

void SetCsrInfoStoryMode()
{
    sm_csr_info[0] = 2;
    sm_csr_info[1] = 1;
    dsp_ms.csr[1] = 2;
    
    if (cribo.clear_info & 1)
    {
        dsp_ms.csr[1] = 0;
        sm_csr_info[0] = 0;
        
        if (ingame_wrk.clear_count >= 3)
        {
            sm_csr_info[1] = 3;
        }
        else
        {
            sm_csr_info[1] = 2;
        }
    }
    else
    {
        if (CheckClearAll() != 0)
        {
            dsp_ms.csr[1] = 1;
            sm_csr_info[0] = 1;
        }
    }
}

#ifdef BUILD_EU_VERSION
void SetCsrInfoBattleMode()
{
    sm_csr_info[1] = 1;

    if (cribo.clear_info & 1)
    {
        if (ingame_wrk.clear_count > 2)
        {
            sm_csr_info[1] = 3;
        }
        else
        {
            sm_csr_info[1] = 2;
        }
    }
}
#endif
