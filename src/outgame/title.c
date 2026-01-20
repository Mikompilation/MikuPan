#include "common.h"
#include "title.h"

#include "btl_mode/btl_menu.h"
#include "ee/kernel.h"
#include "enums.h"
#include "graphics/graph2d/effect_obj.h"
#include "graphics/graph2d/g2d_debug.h"
#include "graphics/graph2d/tim2.h"
#include "graphics/graph3d/sgdma.h"
#include "graphics/graph3d/sglib.h"
#include "graphics/motion/mdlwork.h"
#include "graphics/mov/movie.h"
#include "ingame/menu/ig_album.h"
#include "ingame/menu/ig_camra.h"
#include "ingame/menu/ig_glst.h"
#include "ingame/menu/ig_menu.h"
#include "main/gamemain.h"
#include "main/glob.h"
#include "mc/mc_at.h"
#include "mc/mc_main.h"
#include "memory_album.h"
#include "mikupan/gs/gs_server_c.h"
#include "mikupan/mikupan_logging_c.h"
#include "mikupan/mikupan_memory.h"
#include "mikupan/rendering/mikupan_renderer.h"
#include "os/eeiop/adpcm/ea_cmd.h"
#include "os/eeiop/adpcm/ea_ctrl.h"
#include "os/eeiop/adpcm/ea_dat.h"
#include "os/eeiop/cdvd/eecdvd.h"
#include "os/eeiop/eese.h"
#include "outgame.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* sdata 3576a0 */ int opening_movie_type;
/* data 343070 */ TITLE_WRK title_wrk;
TTL_DSP_WRK ttl_dsp;
OUT_DITHER_STR out_dither = {0};

#define PI 3.1415927f
#define DEG2RAD(x) ((float)(x)*PI/180.0f)

SPRT_DAT font_sprt[20] = {
    {
        .tex0 = 2307304836316669836,
        .u = 0,
        .v = 0,
        .w = 128,
        .h = 48,
        .x = 206,
        .y = 287,
        .pri = 20480,
        .alpha = 128,
    },
    {
        .tex0 = 2307304836316669836,
        .u = 128,
        .v = 0,
        .w = 128,
        .h = 48,
        .x = 257,
        .y = 357,
        .pri = 20480,
        .alpha = 128,
    },
    {
        .tex0 = 2307304836316669836,
        .u = 256,
        .v = 0,
        .w = 128,
        .h = 48,
        .x = 257,
        .y = 357,
        .pri = 20480,
        .alpha = 128,
    },
    {
        .tex0 = 2307304836316669836,
        .u = 384,
        .v = 0,
        .w = 128,
        .h = 48,
        .x = 196,
        .y = 287,
        .pri = 20480,
        .alpha = 128,
    },
    {
        .tex0 = 2307304836316669836,
        .u = 384,
        .v = 0,
        .w = 128,
        .h = 48,
        .x = 303,
        .y = 287,
        .pri = 20480,
        .alpha = 128,
    },
    {
        .tex0 = 2307304836316669836,
        .u = 384,
        .v = 0,
        .w = 128,
        .h = 48,
        .x = 306,
        .y = 287,
        .pri = 20480,
        .alpha = 128,
    },
    {
        .tex0 = 2307304836316669836,
        .u = 0,
        .v = 48,
        .w = 144,
        .h = 48,
        .x = 299,
        .y = 287,
        .pri = 20480,
        .alpha = 128,
    },
    {
        .tex0 = 2307304836316669836,
        .u = 144,
        .v = 48,
        .w = 144,
        .h = 48,
        .x = 244,
        .y = 287,
        .pri = 20480,
        .alpha = 128,
    },
    {
        .tex0 = 2307304836316669836,
        .u = 288,
        .v = 48,
        .w = 144,
        .h = 48,
        .x = 217,
        .y = 287,
        .pri = 20480,
        .alpha = 128,
    },
    {
        .tex0 = 2307304836316669836,
        .u = 0,
        .v = 96,
        .w = 160,
        .h = 48,
        .x = 240,
        .y = 287,
        .pri = 20480,
        .alpha = 128,
    },
    {
        .tex0 = 2307304836316669836,
        .u = 160,
        .v = 96,
        .w = 160,
        .h = 48,
        .x = 241,
        .y = 357,
        .pri = 20480,
        .alpha = 128,
    },
    {
        .tex0 = 2307304836316669836,
        .u = 360,
        .v = 96,
        .w = 40,
        .h = 48,
        .x = 218,
        .y = 358,
        .pri = 36864,
        .alpha = 128,
    },
    {
        .tex0 = 2307304836316669836,
        .u = 360,
        .v = 96,
        .w = 40,
        .h = 48,
        .x = 385,
        .y = 358,
        .pri = 36864,
        .alpha = 128,
    },
    {
        .tex0 = 2307304836316669836,
        .u = 320,
        .v = 96,
        .w = 40,
        .h = 48,
        .x = 234,
        .y = 358,
        .pri = 28672,
        .alpha = 128,
    },
    {
        .tex0 = 2307304836316669836,
        .u = 320,
        .v = 96,
        .w = 40,
        .h = 48,
        .x = 368,
        .y = 358,
        .pri = 28672,
        .alpha = 128,
    },
    {
        .tex0 = 2307304836316669836,
        .u = 0,
        .v = 144,
        .w = 240,
        .h = 48,
        .x = 200,
        .y = 321,
        .pri = 20480,
        .alpha = 128,
    },
    {
        .tex0 = 2307304836316669836,
        .u = 240,
        .v = 144,
        .w = 240,
        .h = 48,
        .x = 200,
        .y = 287,
        .pri = 20480,
        .alpha = 128,
    },
    {
        .tex0 = 2307304836316669836,
        .u = 0,
        .v = 192,
        .w = 458,
        .h = 64,
        .x = 91,
        .y = 315,
        .pri = 20480,
        .alpha = 128,
    },
    {
        .tex0 = 2307314180957448592,
        .u = 1,
        .v = 1,
        .w = 126,
        .h = 48,
        .x = 33,
        .y = 23,
        .pri = 20480,
        .alpha = 128,
    },
    {
        .tex0 = 2307314180957448592,
        .u = 1,
        .v = 51,
        .w = 10,
        .h = 48,
        .x = 159,
        .y = 23,
        .pri = 20480,
        .alpha = 128,
    },
};


SPRT_DAT title_sprt[11] = {
    {
        .tex0 = 2307144857374827264,
        .u = 0,
        .v = 0,
        .w = 512,
        .h = 256,
        .x = 0,
        .y = 0,
        .pri = 40960,
        .alpha = 128,
    },
    {
        .tex0 = 2307154202015606020,
        .u = 0,
        .v = 0,
        .w = 128,
        .h = 128,
        .x = 512,
        .y = 0,
        .pri = 40960,
        .alpha = 128,
    },
    {
        .tex0 = 2307163547864442184,
        .u = 0,
        .v = 0,
        .w = 128,
        .h = 128,
        .x = 512,
        .y = 128,
        .pri = 40960,
        .alpha = 128,
    },
    {
        .tex0 = 2307181689873442188,
        .u = 0,
        .v = 0,
        .w = 256,
        .h = 128,
        .x = 0,
        .y = 256,
        .pri = 40960,
        .alpha = 128,
    },
    {
        .tex0 = 2307199831815300624,
        .u = 0,
        .v = 0,
        .w = 256,
        .h = 128,
        .x = 256,
        .y = 256,
        .pri = 40960,
        .alpha = 128,
    },
    {
        .tex0 = 2307209177596995220,
        .u = 0,
        .v = 0,
        .w = 128,
        .h = 128,
        .x = 512,
        .y = 256,
        .pri = 40960,
        .alpha = 128,
    },
    {
        .tex0 = 2307214124325578456,
        .u = 0,
        .v = 0,
        .w = 128,
        .h = 64,
        .x = 0,
        .y = 384,
        .pri = 40960,
        .alpha = 128,
    },
    {
        .tex0 = 2307219072127903484,
        .u = 0,
        .v = 0,
        .w = 128,
        .h = 64,
        .x = 128,
        .y = 384,
        .pri = 40960,
        .alpha = 128,
    },
    {
        .tex0 = 2307224019930228512,
        .u = 0,
        .v = 0,
        .w = 128,
        .h = 64,
        .x = 256,
        .y = 384,
        .pri = 40960,
        .alpha = 128,
    },
    {
        .tex0 = 2307228967732553540,
        .u = 0,
        .v = 0,
        .w = 128,
        .h = 64,
        .x = 384,
        .y = 384,
        .pri = 40960,
        .alpha = 128,
    },
    {
        .tex0 = 2307233915534878568,
        .u = 0,
        .v = 0,
        .w = 128,
        .h = 64,
        .x = 512,
        .y = 384,
        .pri = 40960,
        .alpha = 128,
    },
};

/* 00212ca8 00001218 */ void TitleCtrl()
{
	/* sbss 357bc8 */ static u_int mc_pnum1;
	/* sbss 357bcc */ static u_int mc_pnum2;
	/* sbss 357bd0 */ static u_int mc_atyp1;
	/* sbss 357bd4 */ static u_int mc_atyp2;
	/* sbss 357bd8 */ static u_int mc_slot1;
	/* sbss 357bdc */ static u_int mc_slot2;
	/* sbss 357be0 */ static u_int mc_file1;
	/* sbss 357be4 */ static u_int mc_file2;
	/* sdata 3576a4 */static int title_cnt;

    switch(title_wrk.mode)
    {
    case TITLE_INIT:
        ttl_dsp = (TTL_DSP_WRK){0};

        title_wrk.load_id = LoadReq(TITLE_PK2, SPRITE_ADDRESS);

        InitOutDither();
        MakeOutDither();

        title_wrk.mode = TITLE_WAIT;
    break;

    case TITLE_WAIT:
        if (IsLoadEnd(title_wrk.load_id) == 0)
        {
            break;
        }

        SeStopAll();

        title_wrk.csr = 0;
        ttl_dsp.timer = 0;
        title_wrk.mode = TITLE_TITLE_WAIT;
        title_cnt = 0;
    case TITLE_TITLE_WAIT:
        title_wrk.mode = TITLE_TITLE_WAIT2;

        EAdpcmFadeOut(0x3c);
    case TITLE_TITLE_WAIT2:
        SetSprFile(SPRITE_ADDRESS);

        TitleWaitMode();
        DispOutDither();

        if (IsEndAdpcmFadeOut() != 0)
        {
            EAdpcmCmdPlay(0, 1, AO000_TITLE_STR, 0, GetAdpcmVol(AO000_TITLE_STR), 0x280, 0xfff, 0);

            title_wrk.mode = TITLE_TITLE;
            ttl_dsp.timer = 0;
        }
    break;
    case TITLE_TITLE:
        title_cnt++;

        SetSprFile(SPRITE_ADDRESS);
        TitleWaitMode();
        DispOutDither();
        if (title_cnt >= 60*46 && title_wrk.mode != TITLE_TITLE_SEL_INIT)
        {
            title_wrk.mode = TITLE_MOVIE_STEP1;
            EAdpcmFadeOut(0x3c);
        }
    break;
    case TITLE_MOVIE_STEP1:
        if (IsEndAdpcmFadeOut() != 0)
        {
            title_cnt = 60*3;
            title_wrk.mode = TITLE_MOVIE_STEP2;
        }
    break;
    case TITLE_MOVIE_STEP2:
        if (title_cnt != 0)
        {
            if (--title_cnt != 0)
            {
                break;
            }
        }

        if (opening_movie_type != 0)
        {
            MoviePlay(SCENE_NO_9_20_0);
        }
        else
        {
            MoviePlay(SCENE_NO_9_10_0);
        }

        opening_movie_type = opening_movie_type == 0;
        title_wrk.mode = TITLE_INIT;
    break;
    case TITLE_INIT_FROM_IN_BGMREQ:
        if (IsEndAdpcmFadeOut() != 0)
        {
            EAdpcmCmdPlay(0, 1, AO000_TITLE_STR, 0, GetAdpcmVol(AO000_TITLE_STR), 0x280, 0xfff, 0);

            title_wrk.mode = TITLE_INIT_FROM_IN;
        }
    break;
    case TITLE_INIT_FROM_IN:
        title_wrk.load_id = LoadReq(TITLE_PK2, SPRITE_ADDRESS);

        title_wrk.mode = TITLE_WAIT_FROM_IN;
    break;
    case TITLE_WAIT_FROM_IN:
        if (IsLoadEnd(title_wrk.load_id) == 0)
        {
            break;
        }

        SeStopAll();

        title_wrk.csr = 0;
        title_wrk.mode = TITLE_TITLE_SEL;
    case TITLE_TITLE_SEL_BGMREQ:
        if (IsEndAdpcmFadeOut() != 0)
        {
            EAdpcmCmdPlay(0, 1, AO000_TITLE_STR, 0, GetAdpcmVol(AO000_TITLE_STR), 0x280, 0xfff, 0);

            title_wrk.mode = TITLE_TITLE_SEL_INIT;
        }
    break;
    case TITLE_TITLE_SEL_INIT:
        title_wrk.csr = 1;
        title_wrk.mode = TITLE_TITLE_SEL;
    case TITLE_TITLE_SEL:
        SetSprFile(SPRITE_ADDRESS);

        if (L1_PRESSED() >= 1 && R1_PRESSED()  >= 1)
        {
            ttl_dsp.mode = 0;
        }

        if (ttl_dsp.mode != 0)
        {
            TitleStartSlctYW(0, 0x80);
        }
        else
        {
            SetFTIM2File(FontTextAddress);
            TitleStartSlct();
        }

        DispOutDither();
    break;
    case TITLE_LOAD_PRE:
        if (IsLoadEnd(title_wrk.load_id) != 0)
        {
            sys_wrk.load = 1;
            title_wrk.mode = TITLE_MEMCA_LOAD_WAIT;
            title_wrk.sub_mode = 0;
        }
    break;
    case TITLE_MEMCA_LOAD_WAIT:
        if (IsEndAdpcmFadeOut() != 0)
        {
            EAdpcmCmdPlay(0, 1, AB018_STR, 0, GetAdpcmVol(AB018_STR), 0x280, 0xfff, 0);

            title_wrk.mode = TITLE_MEMCA_LOAD;
        }
    break;
    case TITLE_MEMCA_LOAD:
        if (title_wrk.sub_mode == 0)
        {
            mcInit(1, (u_int *)MikuPan_GetHostPointer(MC_WORK_ADDRESS), 0);
            title_wrk.sub_mode = 7;
        }

        SetSprFile(PL_PSVP_PK2_ADDRESS);
        SetSprFile(PL_SAVE_PK2_ADDRESS);
        SetSprFile(SV_PHT_PK2_ADDRESS);

        switch (McAtLoadChk(1))
        {
            case 0:
                // do nothing ...
            break;
            case 1:
                ingame_wrk.stts &= (0x80 | 0x40 | 0x10 | 0x8 | 0x4 | 0x2 | 0x1);

                InitialDmaBuffer();

                if (ingame_wrk.clear_count != 0)
                {
                    ModeSlctInit(0, 0);

                    OutGameModeChange(OUTGAME_MODE_MODESEL);
                }
                else
                {
                    EAdpcmFadeOut(0x3c);

                    LoadGameInit();

                    ingame_wrk.game = 0;

                    GameModeChange(GAME_MODE_INIT);
                }
            break;
            case 2:
                EAdpcmFadeOut(0x3c);

                title_wrk.mode = TITLE_TITLE_SEL_BGMREQ;

                ingame_wrk.stts &= (0x80 | 0x40 | 0x10 | 0x8 | 0x4 | 0x2 | 0x1);

                InitialDmaBuffer();
            break;
        }
    break;
    case TITLE_MODE_SEL_BGMREQ:
        if (IsEndAdpcmFadeOut() != 0)
        {
            EAdpcmCmdPlay(0, 1, AO000_TITLE_STR, 0, GetAdpcmVol(AO000_TITLE_STR), 0x280, 0xfff, 0);

            title_wrk.mode = TITLE_MODE_SEL;
        }
    break;
    case TITLE_MODE_SEL:
        SetSprFile(SPRITE_ADDRESS);

        if (ttl_dsp.mode != 0)
        {
            TitleSelectModeYW(0, 0x80);
        }
        else
        {
            SetFTIM2File(FontTextAddress);
            TitleSelectMode();
        }

        DispOutDither();
    break;
    case TITLE_DIFF_SEL:
        TitleSelectDifficultyYW();
    break;
    case TITLE_ALBM_LOAD_PRE:
        if (IsLoadEnd(title_wrk.load_id) != 0)
        {
            sys_wrk.load = 1;

            mc_pnum2 = mc_pnum1 = 0xff;
            mc_atyp2 = mc_atyp1 = 0xff;
            mc_slot2 = mc_slot1 = 0xff;
            mc_file2 = mc_file1 = 0xff;

            mcInit(5, (u_int *)MC_WORK_ADDRESS, 0);

            title_wrk.mode = TITLE_ALBM_LOAD0;
        }
    break;
    case TITLE_ALBM_LOAD0:
        if (IsEndAdpcmFadeOut() != 0)
        {
            EAdpcmCmdPlay(0, 1, AB018_STR, 0, GetAdpcmVol(AB018_STR), 0x280, 0xfff, 0);

            title_wrk.mode = TITLE_ALBM_LOAD1;
        }
    break;
    case TITLE_ALBM_LOAD1:
        SetSprFile(PL_PSVP_PK2_ADDRESS);
        SetSprFile(PL_SAVE_PK2_ADDRESS);
        SetSprFile(SV_PHT_PK2_ADDRESS);

        switch (McAtLoadChk(2))
        {
        case 0: break;
        case 1:
            album_save_dat[0] = mc_album_save;
            mc_pnum1 = mc_photo_num;
            mc_atyp1 = mc_album_type;
            mc_slot1 = mc_ctrl.port + 1;
            mc_file1 = mc_ctrl.sel_file + 1;

            memcpy((void *)0xE80000, (void *)0x5a0000, 0x180000);

            mcInit(6, (u_int *)MC_WORK_ADDRESS, 0);

            title_wrk.mode = TITLE_ALBM_LOAD2;
            break;
        case 3: info_log("ここに来るわけがない！ (code shouldnt have reached here!)"); break;
        case 2:
            EAdpcmFadeOut(0x3c);

            title_wrk.mode = TITLE_TITLE_SEL_BGMREQ;
            break;
        }
    break;
    case TITLE_ALBM_LOAD2:
        SetSprFile(PL_SAVE_PK2_ADDRESS);
        SetSprFile(SV_PHT_PK2_ADDRESS);

        switch (McAtLoadChk(2))
        {
        case 0:
            // do nothing ...
        break;
        case 1:
            album_save_dat[1] = mc_album_save;

            mc_pnum2 = mc_photo_num;
            mc_atyp2 = mc_album_type;
            mc_slot2 = mc_ctrl.port + 1;
            mc_file2 = mc_ctrl.sel_file + 1;
            memcpy((void *)0x1000000, (void *)0x5a0000, 0x180000);

            MemAlbmInit(1, mc_pnum1, mc_pnum2, mc_atyp1, mc_atyp2, mc_slot1, mc_slot2, mc_file1, mc_file2 & 0xff);

            title_wrk.load_id = LoadReq(PL_ALBM_FSM_PK2, PL_ALBM_FSM_PK2_ADDRESS);
            title_wrk.load_id = LoadReq(PL_ALBM_PK2, PL_SAVE_PK2_ADDRESS);
            title_wrk.load_id = AlbmDesignLoad(0, mc_atyp1);
            title_wrk.load_id = AlbmDesignLoad(1, mc_atyp2);

            title_wrk.mode = TITLE_ALBM_MAIN_PRE;
        break;
        case 3:
            mc_pnum2 = 0;
            mc_atyp2 = 5;
            mc_slot2 = 0;
            mc_file2 = 0;

            memcpy((void *)0x1000000, (void *)0x5a0000, 0x180000);

            MemAlbmInit(1, mc_pnum1, mc_pnum2, mc_atyp1, mc_atyp2, mc_slot1, mc_slot2, mc_file1, mc_file2 & 0xff);
            NewAlbumInit(1);

            title_wrk.load_id = LoadReq(PL_ALBM_FSM_PK2, PL_ALBM_FSM_PK2_ADDRESS);
            title_wrk.load_id = LoadReq(PL_ALBM_PK2, PL_SAVE_PK2_ADDRESS);
            title_wrk.load_id = AlbmDesignLoad(0, mc_atyp1);
            title_wrk.load_id = AlbmDesignLoad(1, mc_atyp2);

            title_wrk.mode = TITLE_ALBM_MAIN_PRE;
        break;
        case 2:
            EAdpcmFadeOut(0x3c);

            title_wrk.mode = TITLE_TITLE_SEL_BGMREQ;
        break;
        }
    break;
    case TITLE_ALBM_MAIN_PRE:
        if (IsLoadEnd(title_wrk.load_id) != 0)
        {
            title_wrk.mode = TITLE_ALBM_MAIN;
        }
    break;
    case TITLE_ALBM_MAIN:
        SetSprFile(PL_ALBM_FSM_PK2_ADDRESS);
        SetSprFile(PL_SAVE_PK2_ADDRESS);

        switch(SweetMemories(1, 0x80))
        {
        case 0: break;
        case 1:
            memcpy((void *)0x5a0000, (void *)0xe80000, 0x180000);

            mcInit(2, (u_int *)MikuPan_GetHostAddress(MC_WORK_ADDRESS), 0);

            title_wrk.load_side = 0;

            title_wrk.load_id = LoadReq(PL_PSVP_PK2, PL_PSVP_PK2_ADDRESS);
            title_wrk.load_id = LoadReq(PL_SAVE_PK2, PL_SAVE_PK2_ADDRESS);
            title_wrk.load_id = LoadReq(PL_ALBM_SAVE_PK2, SV_PHT_PK2_ADDRESS);

            title_wrk.mode = TITLE_ALBM_SAVE_PRE;
        break;
        case 2:
            memcpy((void *)0x5a0000, (void *)0x1000000, 0x180000);

            mcInit(2, (u_int *)MikuPan_GetHostAddress(MC_WORK_ADDRESS), 0);

            title_wrk.load_side = 1;

            title_wrk.load_id = LoadReq(PL_PSVP_PK2, PL_PSVP_PK2_ADDRESS);
            title_wrk.load_id = LoadReq(PL_SAVE_PK2, PL_SAVE_PK2_ADDRESS);
            title_wrk.load_id = LoadReq(PL_ALBM_SAVE_PK2, SV_PHT_PK2_ADDRESS);

            title_wrk.mode = TITLE_ALBM_SAVE_PRE;
        break;
        case 3:
            title_wrk.load_id = LoadReq(PL_PSVP_PK2, PL_PSVP_PK2_ADDRESS);
            title_wrk.load_id = LoadReq(PL_SAVE_PK2, PL_SAVE_PK2_ADDRESS);
            title_wrk.load_id = LoadReq(PL_ALBM_SAVE_PK2, SV_PHT_PK2_ADDRESS);

            title_wrk.load_side = 0;

            mcInit(5, (u_int *)MikuPan_GetHostAddress(MC_WORK_ADDRESS), 0);

            title_wrk.mode = TITLE_ALBM_LOAD_MODE_PRE;
        break;
        case 4:
            title_wrk.load_id = LoadReq(PL_PSVP_PK2, PL_PSVP_PK2_ADDRESS);
            title_wrk.load_id = LoadReq(PL_SAVE_PK2, PL_SAVE_PK2_ADDRESS);
            title_wrk.load_id = LoadReq(PL_ALBM_SAVE_PK2, SV_PHT_PK2_ADDRESS);

            title_wrk.load_side = 1;

            mcInit(5, (u_int *)MikuPan_GetHostAddress(MC_WORK_ADDRESS), 0);

            title_wrk.mode = TITLE_ALBM_LOAD_MODE_PRE;
        break;
        case 5:
            EAdpcmFadeOut(0x3c);

            title_wrk.mode = TITLE_TITLE_SEL_BGMREQ;
            title_wrk.csr = 0;

            ingame_wrk.stts &= (0x80 | 0x40 | 0x10 | 0x8 | 0x4 | 0x2 | 0x1);

            InitialDmaBuffer();
        break;
        }
    break;
    case TITLE_ALBM_LOAD_MODE_PRE:
        if (IsLoadEnd(title_wrk.load_id) != 0)
        {
            title_wrk.mode = TITLE_ALBM_LOAD_MODE;
        }
    break;
    case TITLE_ALBM_LOAD_MODE:
        SetSprFile(PL_PSVP_PK2_ADDRESS);
        SetSprFile(PL_SAVE_PK2_ADDRESS);
        SetSprFile(SV_PHT_PK2_ADDRESS);

        switch (McAtLoadChk(2))
        {
        case 0:
            // do nothing ...
        break;
        case 3:
        case 1:
            if (title_wrk.load_side == 0)
            {
                memcpy((void *)0xe80000, (void *)0x5a0000, 0x180000);

                mc_pnum1 = mc_photo_num;
                mc_atyp1 = mc_album_type;
                mc_slot1 = mc_ctrl.port + 1;
                mc_file1 = mc_ctrl.sel_file + 1;

                album_save_dat[0] = mc_album_save;

                MemAlbmInit2(0, mc_pnum1, mc_atyp1, mc_slot1, mc_file1);
            }
            else
            {
                memcpy((void *)0x1000000, (void *)0x5a0000, 0x180000);

                mc_pnum2 = mc_photo_num;
                mc_atyp2 = mc_album_type;
                mc_slot2 = mc_ctrl.port + 1;
                mc_file2 = mc_ctrl.sel_file + 1;
                album_save_dat[1] = mc_album_save;
                MemAlbmInit2(1, mc_pnum2, mc_atyp2, mc_slot2, mc_file2);
            }

            title_wrk.load_id = AlbmDesignLoad(0, mc_atyp1);
            title_wrk.load_id = AlbmDesignLoad(1, mc_atyp2);
            title_wrk.load_id = LoadReq(PL_ALBM_PK2, PL_SAVE_PK2_ADDRESS);

            title_wrk.mode = TITLE_ALBM_MAIN_PRE;
        break;
        case 2:
            MemAlbmInit3();
            AlbmDesignLoad(0, mc_atyp1);
            AlbmDesignLoad(1, mc_atyp2);

            title_wrk.load_id = LoadReq(PL_ALBM_PK2, PL_SAVE_PK2_ADDRESS);

            title_wrk.mode = TITLE_ALBM_MAIN_PRE;
        break;
        }
    break;
    case TITLE_ALBM_SAVE_PRE:
        if (IsLoadEnd(title_wrk.load_id) != 0)
        {
            title_wrk.mode = TITLE_ALBM_SAVE;
        }
    break;
    case TITLE_ALBM_SAVE:
        SetSprFile(PL_PSVP_PK2_ADDRESS);
        SetSprFile(PL_SAVE_PK2_ADDRESS);
        SetSprFile(SV_PHT_PK2_ADDRESS);

        switch (McAtAlbmChk())
        {
        case 0:
            // do nothing ...
        break;
        case 1:
            if (title_wrk.load_side == 0)
            {
                mc_pnum1 = mc_photo_num;
                mc_atyp1 = mc_album_type;
                mc_slot1 = mc_ctrl.port + 1;
                mc_file1 = mc_ctrl.sel_file + 1;

                MemAlbmInit2(0, mc_pnum1, mc_atyp1, mc_slot1, mc_file1);
            }
            else
            {
                mc_pnum2 = mc_photo_num;
                mc_atyp2 = mc_album_type;
                mc_slot2 = mc_ctrl.port + 1;
                mc_file2 = mc_ctrl.sel_file + 1;
                MemAlbmInit2(1, mc_pnum2, mc_atyp2, mc_slot2, mc_file2);
            }

            AlbmDesignLoad(0, mc_atyp1);
            AlbmDesignLoad(1, mc_atyp2);

            title_wrk.load_id = LoadReq(PL_ALBM_PK2, PL_SAVE_PK2_ADDRESS);

            title_wrk.mode = TITLE_ALBM_MAIN_PRE;
        break;
        case 2:
            MemAlbmInit3();

            AlbmDesignLoad(0, mc_atyp1);
            AlbmDesignLoad(1, mc_atyp2);

            title_wrk.load_id = LoadReq(PL_ALBM_PK2, PL_SAVE_PK2_ADDRESS);

            title_wrk.mode = TITLE_ALBM_MAIN_PRE;
        break;
        }
    break;
    case TITLE_MODE_SELECT:
        ModeSlctLoop();
    break;
    }

    if (ttl_dsp.timer < 60*2)
    {
        ttl_dsp.timer++;
    }
    else
    {
        ttl_dsp.timer = 0;
    }
}

void TitleWaitMode()
{
    /* s0 16 */ int i;
    /* f20 58 */ float alp;
    /* 0x0(sp) */ DISP_SPRT ds;
    int f;

    for (i = 0; i < 11; i++)
    {
        CopySprDToSpr(&ds, &title_sprt[i]);
        DispSprD(&ds);
    }

    if (title_wrk.mode == TITLE_TITLE_WAIT)
    {
        return;
    }

    f = ttl_dsp.timer % 120;
    alp = (SgSinf(f / 120.0f * (3.1415927f * 2)) + 1.0f) * 64.0f;

    CopySprDToSpr(&ds, &font_sprt[17]);

    ds.alphar = SCE_GS_SET_ALPHA_1(SCE_GS_ALPHA_CS, SCE_GS_ALPHA_ZERO, SCE_GS_ALPHA_AS, SCE_GS_ALPHA_CD, 0);
    ds.alpha = alp;
    ds.tex1 = SCE_GS_SET_TEX1_1(1, 0, SCE_GS_LINEAR, SCE_GS_LINEAR_MIPMAP_LINEAR, 0, 0, 0);

    DispSprD(&ds);

    ttl_dsp.mode = 0;

    if (CROSS_PRESSED() == 1 || START_PRESSED() == 1)
    {
        ttl_dsp.mode = 1;

        title_wrk.mode = TITLE_TITLE_SEL_INIT;
        title_wrk.csr = 0;

        SeStartFix(1, 0, 0x1000, 0x1000, 0);
    }
}

void TitleStartSlct()
{    
    char *str_o = "o";
	/* s0 16 */ char *str1 = "ZERO HOUR";
	/* s1 17 */ char *str2 = "NEW GAME";
	/* s2 18 */ char *str3 = "LOAD GAME";
	/* s3 19 */ char *str4 = "ALBUM";
	/* s4 20 */ char *csr0 = "MISSION";

    SetASCIIString(70.0f, 110.0f, str1);

    SetASCIIString(160.0f, 190.0f, str2);
    SetASCIIString(160.0f, 230.0f, str3);
    SetASCIIString(160.0f, 270.0f, str4);
    SetASCIIString(230.0f, 350.0f, csr0);

    SetInteger2(0, 350.0f, 350.0f, 0, 0x80, 0x80, 0x80, ingame_wrk.msn_no);

    SetASCIIString(120.0f, title_wrk.csr * 40 + 190, str_o);

    if (
        DPAD_RIGHT_PRESSED() == 1 ||
        (DPAD_RIGHT_PRESSED() > 25 && (DPAD_RIGHT_PRESSED() % 5) == 1) ||
        Ana2PadDirCnt(1) == 1 ||
        (Ana2PadDirCnt(1) > 25 && (Ana2PadDirCnt(1) % 5) == 1)
    ) {
        ingame_wrk.msn_no++;
    }
    else if (
        DPAD_LEFT_PRESSED() == 1 ||
        (DPAD_LEFT_PRESSED() > 25 && (DPAD_LEFT_PRESSED() % 5) == 1) ||
        Ana2PadDirCnt(3) == 1 ||
        (Ana2PadDirCnt(3) > 25 && (Ana2PadDirCnt(3) % 5) == 1))
    {
        if (ingame_wrk.msn_no != 0)
        {
            ingame_wrk.msn_no--;
        }
    }

    if (CROSS_PRESSED() == 1 || START_PRESSED() == 1)
    {
        if (title_wrk.csr != 0x0)
        {
            title_wrk.load_id = LoadReq(PL_BGBG_PK2, PL_BGBG_PK2_ADDRESS);
            title_wrk.load_id = LoadReq(PL_STTS_PK2, PL_STTS_PK2_ADDRESS);
            title_wrk.load_id = LoadReq(PL_PSVP_PK2, PL_PSVP_PK2_ADDRESS);
            title_wrk.load_id = LoadReq(PL_SAVE_PK2, PL_SAVE_PK2_ADDRESS);
            title_wrk.load_id = LoadReq(SV_PHT_PK2, SV_PHT_PK2_ADDRESS);

            title_wrk.mode = TITLE_LOAD_PRE;

            EAdpcmFadeOut(0x3c);
        }
        else
        {
            NewGameInit();

            title_wrk.mode = TITLE_MODE_SEL;
            title_wrk.csr = 0;
        }

        SeStartFix(1, 0, 0x1000, 0x1000, 0);
    }
    else if (TRIANGLE_PRESSED() == 1)
    {
        ttl_dsp.timer = 0;
        title_wrk.mode = TITLE_TITLE;
        SeStartFix(3, 0, 0x1000, 0x1000, 0);
    }
    else if (
        DPAD_UP_PRESSED() == 1 ||
        (DPAD_UP_PRESSED() > 25 && (DPAD_UP_PRESSED() % 5) == 1) ||
        Ana2PadDirCnt(0) == 1 ||
        (Ana2PadDirCnt(0) > 25 && (Ana2PadDirCnt(0) % 5) == 1)
    ) {
        title_wrk.csr = 0;
        SeStartFix(0, 0, 0x1000, 0x1000, 0);
    }
    else if (
        DPAD_DOWN_PRESSED() == 1 ||
        (DPAD_DOWN_PRESSED() > 25 && (DPAD_DOWN_PRESSED() % 5) == 1) ||
        Ana2PadDirCnt(2) == 1 ||
        (Ana2PadDirCnt(2) > 25 && (Ana2PadDirCnt(2) % 5) == 1)
    ) {
        title_wrk.csr = 1;
        SeStartFix(0, 0, 0x1000, 0x1000, 0);
    }
}

void TitleStartSlctYW(u_char pad_off, u_char alp_max)
{
    /* s0 16 */ int i;
	/* s1 17 */ u_char mode;
	/* fp 30 */ u_char adj;
	/* s3 19 */ u_char dsp;
	/* s6 22 */ u_char chr1;
	/* s5 21 */ u_char chr2;
	/* v0 2 */ u_char alp;
	/* s0 16 */ u_char rgb;
	/* 0x0(sp) */ DISP_SPRT ds;

    for (i = 0; i < 11; i++)
    {
        CopySprDToSpr(&ds, &title_sprt[i]);

        ds.alpha = alp_max;

        DispSprD(&ds);
    }

    for (mode = 0; mode < 3; mode++)
    {
        switch (mode)
        {
        case 0:
            chr1 = 8;
            chr2 = 4;

            switch (title_wrk.csr)
            {
            case 0:
                adj = 0;
                dsp = 1;
            break;
            case 1:
                adj = 0;
                dsp = 0;
            break;
            case 2:
                adj = 0;
                dsp = 0;
            break;
            }
        break;
        case 1:
            chr1 = 0;
            chr2 = 5;

            switch (title_wrk.csr)
            {
            case 0:
                adj = 35;
                dsp = 0;
            break;
            case 1:
                adj = 35;
                dsp = 1;
            break;
            case 2:
                adj = 35;
                dsp = 0;
            break;
            }
        break;
        case 2:
            chr1 = 7;
            chr2 = 0xff;

            switch (title_wrk.csr)
            {
            case 0:
                adj = 70;
                dsp = 0;
            break;
            case 1:
                adj = 70;
                dsp = 0;
            break;
            case 2:
                adj = 70;
                dsp = 1;
            break;
            }
        break;
        }

        CopySprDToSpr(&ds, &font_sprt[chr1]);

        ds.y += adj;

        alp = rgb = dsp * (alp_max / 2) + (alp_max / 2);

        ds.alpha = alp;
        ds.r = rgb; ds.g = rgb; ds.b = rgb;

        if (dsp != 0)
        {
            ds.alphar = SCE_GS_SET_ALPHA_1(SCE_GS_ALPHA_CS, SCE_GS_ALPHA_ZERO, SCE_GS_ALPHA_AS, SCE_GS_ALPHA_CD, 0);
        }

        ds.tex1 = SCE_GS_SET_TEX1_1(1, 0, SCE_GS_LINEAR, SCE_GS_LINEAR_MIPMAP_LINEAR, 0, 0, 0);

        DispSprD(&ds);

        if (chr2 != 0xff)
        {
            CopySprDToSpr(&ds, &font_sprt[chr2]);

            ds.y += adj;

            alp = rgb;

            ds.alpha = alp;
            ds.r = alp; ds.g = alp; ds.b = alp;

            if (dsp != 0)
            {
                ds.alphar = SCE_GS_SET_ALPHA_1(SCE_GS_ALPHA_CS, SCE_GS_ALPHA_ZERO, SCE_GS_ALPHA_AS, SCE_GS_ALPHA_CD, 0);
            }

            ds.tex1 = SCE_GS_SET_TEX1_1(1, 0, SCE_GS_LINEAR, SCE_GS_LINEAR_MIPMAP_LINEAR, 0, 0, 0);

            DispSprD(&ds);
        }
    }

    if (pad_off != 0)
    {
        return;
    }

    if (CROSS_PRESSED() == 1 || START_PRESSED() == 1)
    {
        ingame_wrk.clear_count = 0;
        ingame_wrk.ghost_cnt = 0;
        ingame_wrk.rg_pht_cnt = 0;
        ingame_wrk.pht_cnt = 0;
        ingame_wrk.high_score = 0;
        ingame_wrk.difficult = 0;

        cribo.costume = 0;
        cribo.clear_info = 0;

        NewgameMenuAlbumInit();

        realtime_scene_flg = 0;

        MovieInitWrk();

        motInitMsn();

        switch (title_wrk.csr)
        {
        case 0:
            NewGameInit();

            title_wrk.mode = 9;
            title_wrk.csr = 0;

            SeStartFix(1, 0, 0x1000, 0x1000, 0);
        break;
        case 1:
            title_wrk.load_id = LoadReq(PL_BGBG_PK2, PL_BGBG_PK2_ADDRESS);
            title_wrk.load_id = LoadReq(PL_STTS_PK2, PL_STTS_PK2_ADDRESS);
            title_wrk.load_id = LoadReq(PL_PSVP_PK2, PL_PSVP_PK2_ADDRESS);
            title_wrk.load_id = LoadReq(PL_SAVE_PK2, PL_SAVE_PK2_ADDRESS);
            title_wrk.load_id = LoadReq(SV_PHT_PK2, SV_PHT_PK2_ADDRESS);

            ingame_wrk.stts |= 0x20;

            InitialDmaBuffer();

            title_wrk.mode = 6;

            SeStartFix(1, 0, 0x1000, 0x1000, 0);
            EAdpcmFadeOut(60);
        break;
        case 2:
            title_wrk.load_id = LoadReq(PL_BGBG_PK2, PL_BGBG_PK2_ADDRESS);
            title_wrk.load_id = LoadReq(PL_STTS_PK2, PL_STTS_PK2_ADDRESS);
            title_wrk.load_id = LoadReq(PL_PSVP_PK2, PL_PSVP_PK2_ADDRESS);
            title_wrk.load_id = LoadReq(PL_SAVE_PK2, PL_SAVE_PK2_ADDRESS);
            title_wrk.load_id = LoadReq(PL_ALBM_SAVE_PK2, SV_PHT_PK2_ADDRESS);

            ingame_wrk.stts |= 0x20;

            InitialDmaBuffer();

            title_wrk.mode = 14;

            SeStartFix(1, 0, 0x1000, 0x1000, 0);
            EAdpcmFadeOut(60);
        break;
        }
    }
    else if (*key_now[4] == 1)
    {
        ttl_dsp.timer = 0;
        title_wrk.mode = 2;

        SeStartFix(3, 0, 0x1000, 0x1000, 0);
    }
    else if (
        DPAD_DOWN_PRESSED() == 1 ||
        (DPAD_DOWN_PRESSED() > 25 && (DPAD_DOWN_PRESSED() % 5) == 1) ||
        Ana2PadDirCnt(2) == 1 ||
        (Ana2PadDirCnt(2) > 25 && (Ana2PadDirCnt(2) % 5) == 1)
    )
    {
        if (title_wrk.csr < 2)
        {
            title_wrk.csr++;
        }
        else
        {
            title_wrk.csr = 0;
        }

        SeStartFix(0, 0, 0x1000, 0x1000, 0);
    }
    else if (
        DPAD_UP_PRESSED() == 1 ||
        (DPAD_UP_PRESSED() > 25 && (DPAD_UP_PRESSED() % 5) == 1) ||
        Ana2PadDirCnt(0) == 1 ||
        (Ana2PadDirCnt(0) > 25 && (Ana2PadDirCnt(0) % 5) == 1)
    )
    {
        if (title_wrk.csr > 0)
        {
            title_wrk.csr--;
        }
        else
        {
            title_wrk.csr = 2;
        }

        SeStartFix(0, 0, 0x1000, 0x1000, 0);
    }
}

void TitleLoadCtrl()
{
    /// does nothing
}

void TitleSelectMode()
{
    /* s1 17 */ int i;
    /* 0x0(sp) */ char *mode_str[9] = {
        "STORY MODE",
        "BATTLE MODE",
        "OPTION",
        "MAP DATA EDIT",
        "SOUND TEST",
        "SCENE TEST",
        "MOTION TEST",
        "ROOM SIZE CHECK",
        "LAYOUT TEST",
    };
    /* s5 21 */ char *csr0 = "o";

    for (i = 0; i < 9; i++)
    {
        SetASCIIString(110.0f, 30 + 40 * i, mode_str[i]);
    }

    SetASCIIString(30.0f, 30 + title_wrk.csr * 40, csr0);


    if (
        DPAD_UP_PRESSED() == 1 ||
        (DPAD_UP_PRESSED() > 25 && (DPAD_UP_PRESSED() % 5) == 1) ||
        Ana2PadDirCnt(0) == 1 ||
        (Ana2PadDirCnt(0) > 25 && (Ana2PadDirCnt(0) % 5) == 1)
    )
    {
        if (title_wrk.csr > 0)
        {
            title_wrk.csr--;
        }
        else
        {
            title_wrk.csr = 0x8;
        }

        SeStartFix(0, 0, 0x1000, 0x1000, 0);
    }
    else if (
        DPAD_DOWN_PRESSED() == 1 ||
        (DPAD_DOWN_PRESSED() > 25 && (DPAD_DOWN_PRESSED() % 5) == 1) ||
        Ana2PadDirCnt(2) == 1 ||
        (Ana2PadDirCnt(2) > 25 && (Ana2PadDirCnt(2) % 5) == 1)
    )
    {
        if (title_wrk.csr < 8)
        {
            title_wrk.csr++;
        }
        else
        {
            title_wrk.csr = 0;
        }

        SeStartFix(0, 0, 0x1000, 0x1000, 0);
    }
    else if (*key_now[4] == 1)
    {
        title_wrk.mode = 3;

        SeStartFix(3, 0, 0x1000, 0x1000, 0);
    }
    else if (*key_now[5] == 1)
    {
        switch(title_wrk.csr)
        {
            case 0:
                ingame_wrk.game = 0;

                GameModeChange(GAME_MODE_INIT);
                break;
            case 1:
                OutGameModeChange(OUTGAME_MODE_BATTLE);
                break;
            case 2:
                OutGameModeChange(OUTGAME_MODE_OPTION);
                break;
            case 3: // MAP DATA EDIT
                break;
            case 4:
                OutGameModeChange(OUTGAME_MODE_SOUND_TEST);
                break;
            case 5:
                OutGameModeChange(OUTGAME_MODE_SCENE_TEST);
                break;
            case 6:
                OutGameModeChange(OUTGAME_MODE_MOTION_TEST);
                break;
            case 7:
                OutGameModeChange(OUTGAME_MODE_ROOM_SIZE_CHECK);
                break;
            case 8:
                OutGameModeChange(OUTGAME_MODE_LAYOUT_TEST);
                break;
        }

        SeStartFix(1, 0, 0x1000, 0x1000, 0);
    }
}

void TitleSelectModeYW(u_char pad_off, u_char alp_max)
{
    	/* s0 16 */ int i;
	/* s4 20 */ u_char mode;
	/* 0x94(sp) */ u_char adj;
	/* s3 19 */ u_char dsp;
	/* 0x98(sp) */ u_char chr1;
	/* s5 21 */ u_char chr2;
	/* v0 2 */ u_char alp;
	/* s0 16 */ u_char rgb;
	/* 0x0(sp) */ DISP_SPRT ds;

    for (i = 0; i < 11; i++)
    {
        CopySprDToSpr(&ds, &title_sprt[i]);

        ds.alpha = alp_max;

        DispSprD(&ds);
    }

    for (mode = 0; mode < 2; mode++)
    {
        switch (mode)
        {
        case 0:
            chr1 = 3;
            chr2 = 6;

            switch (title_wrk.csr)
            {
            case 0:
                adj = 17;
                dsp = 1;
            break;
            case 1:
                adj = 17;
                dsp = 0;
            break;
            case 2:
                adj = 17;
                dsp = 0;
            break;
            }
        break;
        case 1:
            chr1 = 9;
            chr2 = 0xff;

            switch (title_wrk.csr)
            {
            case 0:
                adj = 52;
                dsp = 0;
            break;
            case 1:
                adj = 52;
                dsp = 1;
            break;
            case 2:
                adj = 52;
                dsp = 0;
            break;
            }
        break;
        }

        CopySprDToSpr(&ds, &font_sprt[chr1]);

        ds.y += adj;

        alp = rgb = dsp * (alp_max / 2) + (alp_max / 2);

        ds.alpha = alp;
        ds.r = rgb; ds.g = rgb; ds.b = rgb;

        if (dsp != 0)
        {
            ds.alphar = SCE_GS_SET_ALPHA_1(SCE_GS_ALPHA_CS, SCE_GS_ALPHA_ZERO, SCE_GS_ALPHA_AS, SCE_GS_ALPHA_CD, 0);
        }

        ds.tex1 = SCE_GS_SET_TEX1_1(1, 0, SCE_GS_LINEAR, SCE_GS_LINEAR_MIPMAP_LINEAR, 0, 0, 0);

        DispSprD(&ds);

        if (chr2 != 0xff)
        {
            CopySprDToSpr(&ds, &font_sprt[chr2]);

            ds.y += adj;

            alp = rgb;

            ds.alpha = alp;
            ds.r = alp; ds.g = alp; ds.b = alp;

            if (dsp != 0)
            {
                ds.alphar = SCE_GS_SET_ALPHA_1(SCE_GS_ALPHA_CS, SCE_GS_ALPHA_ZERO, SCE_GS_ALPHA_AS, SCE_GS_ALPHA_CD, 0);
            }

            ds.tex1 = SCE_GS_SET_TEX1_1(1, 0, SCE_GS_LINEAR, SCE_GS_LINEAR_MIPMAP_LINEAR, 0, 0, 0);

            DispSprD(&ds);
        }
    }

    if (pad_off != 0)
    {
        return;
    }

    if (*key_now[5] == 1 || *key_now[12] == 1)
    {
        switch (title_wrk.csr)
        {
        case 0:
            EAdpcmFadeOut(60);
            SeStartFix(1, 0, 0x1000, 0x1000, 0);

            ingame_wrk.game = 0;

            GameModeChange(0);
        break;
        case 1:
            SeStartFix(1, 0, 0x1000, 0x1000, 0);
            ModeSlctInit(3, 5);

            title_wrk.mode = 27;

            OutGameModeChange(OUTGAME_MODE_MODESEL);
            EAdpcmFadeOut(60);
        break;
        }
    }
    else if (*key_now[4] == 1)
    {
        title_wrk.mode = 23;
        SeStartFix(3, 0, 0x1000, 0x1000, 0);
    }
    else if (
        DPAD_DOWN_PRESSED() == 1 ||
        (DPAD_DOWN_PRESSED() > 25 && (DPAD_DOWN_PRESSED() % 5) == 1) ||
        Ana2PadDirCnt(2) == 1 ||
        (Ana2PadDirCnt(2) > 25 && (Ana2PadDirCnt(2) % 5) == 1)
    )
    {
        if (title_wrk.csr == 0)
        {
            title_wrk.csr++;
        }
        else
        {
            title_wrk.csr = 0;
        }

        SeStartFix(0, 0, 0x1000, 0x1000, 0);
    }
    else if (
        DPAD_UP_PRESSED() == 1 ||
        (DPAD_UP_PRESSED() > 25 && (DPAD_UP_PRESSED() % 5) == 1) ||
        Ana2PadDirCnt(0) == 1 ||
        (Ana2PadDirCnt(0) > 25 && (Ana2PadDirCnt(0) % 5) == 1)
    )
    {
        if (title_wrk.csr > 0)
        {
            title_wrk.csr--;
        }
        else
        {
            title_wrk.csr = 1;
        }

        SeStartFix(0, 0, 0x1000, 0x1000, 0);
    }
}

void TitleSelectDifficultyYW()
{
    	/* s0 16 */ int i;
	/* s0 16 */ u_char chr;
	/* f0 38 */ float alp;
	/* 0x0(sp) */ DISP_SPRT ds;
    int f;

    for (i = 0; i < 11; i++)
    {
        CopySprDToSpr(&ds, &title_sprt[i]);
        DispSprD(&ds);
    }

    CopySprDToSpr(&ds, &font_sprt[16]);

    ds.y += 35.0f;
    ds.alpha = 0x40;
    ds.r = 0x40;
    ds.g = 0x40;
    ds.b = 0x40;
    ds.tex1 = 0x161;

    DispSprD(&ds);

    f = ttl_dsp.timer % 120;
    alp = (SgSinf(f / 120.0f * (PI * 2)) + 1.0f) * 64.0f;

    for (chr = 11; chr < 15; chr++)
    {
        CopySprDToSpr(&ds, &font_sprt[chr]);

        if (chr == 12 || chr == 14)
        {
            ds.att |= 0x2;
        }

        ds.alpha = alp;
        ds.alphar = SCE_GS_SET_ALPHA_1(SCE_GS_ALPHA_CS, SCE_GS_ALPHA_ZERO, SCE_GS_ALPHA_AS, SCE_GS_ALPHA_CD, 0);
        ds.tex1 = SCE_GS_SET_TEX1_1(1, 0, SCE_GS_LINEAR, SCE_GS_LINEAR_MIPMAP_LINEAR, 0, 0, 0);

        DispSprD(&ds);
    }

    switch (title_wrk.csr)
    {
    case 0:
        chr = 1;
    break;
    case 1:
        chr = 10;
    break;
    case 2:
        chr = 2;
    break;
    }

    CopySprDToSpr(&ds, &font_sprt[chr]);

    ds.alphar = SCE_GS_SET_ALPHA_1(SCE_GS_ALPHA_CS, SCE_GS_ALPHA_ZERO, SCE_GS_ALPHA_AS, SCE_GS_ALPHA_CD, 0);
    ds.tex1 = SCE_GS_SET_TEX1_1(1, 0, SCE_GS_LINEAR, SCE_GS_LINEAR_MIPMAP_LINEAR, 0, 0, 0);

    DispSprD(&ds);

    if (*key_now[5] == 1 || *key_now[0xc] == 1)
    {
        title_wrk.mode = 9;

        SeStartFix(1, 0, 0x1000, 0x1000, 0);
    }
    else if (*key_now[4] == 1)
    {
        title_wrk.mode = 9;

        SeStartFix(3, 0, 0x1000, 0x1000, 0);
    }
    else if (
        *key_now[3] == 1 ||
        (*key_now[3] > 25 && (*key_now[3] % 5) == 1) ||
        Ana2PadDirCnt(1) == 1 ||
        (Ana2PadDirCnt(1) > 25 && (Ana2PadDirCnt(1) % 5) == 1)
    )
    {
        if (title_wrk.csr <= 1)
        {
            title_wrk.csr++;
        }
        else
        {
            title_wrk.csr = 0;
        }

        SeStartFix(0, 0, 0x1000, 0x1000, 0);
    }
    else if (
        DPAD_LEFT_PRESSED() == 1 ||
        (DPAD_LEFT_PRESSED() > 25 && (DPAD_LEFT_PRESSED() % 5) == 1) ||
        Ana2PadDirCnt(3) == 1 ||
        (Ana2PadDirCnt(3) > 25 && (Ana2PadDirCnt(3) % 5) == 1)
    )
    {
        if (title_wrk.csr > 0)
        {
            title_wrk.csr--;
        }
        else
        {
            title_wrk.csr = 0x2;
        }

        SeStartFix(0, 0, 0x1000, 0x1000, 0);
    }
}

void NewGameInit()
{
    if (ttl_dsp.mode != 0)
    {
        ingame_wrk.msn_no = 0;
    }

    sys_wrk.load = 0;

    title_wrk.csr = 0;

    cribo.costume = 0;
    cribo.clear_info = 0;

    mc_msn_flg = 0;

    NewgameMenuRankInit();
    CameraCustomNewgameInit();
    NewgameMenuGlstInit();
    ClearStageWrk();
}

void LoadGameInit()
{
    sys_wrk.load = 1;
    title_wrk.csr = 0;
    ingame_wrk.mode = 6;

    cribo.clear_info &= 0x4;
    cribo.costume = 0;

    LoadgameMenuRankInit();
}

void InitOutDither()
{
    out_dither.cnt = 0.0f;
    out_dither.spd = 8.0f;
    out_dither.alp = 64.0f; // Looks way to visible unlike og hardware
    //out_dither.alp = 20.0f;
    out_dither.alpmx = 0x40;
    out_dither.colmx = 0x40;
    out_dither.type = 7;
}

void MakeOutDither()
{
    u_char pat[0x4000];
    u_int pal[0x100];
    int i;
    static sceGsLoadImage gs_limage1;
    static sceGsLoadImage gs_limage2;

    SetVURand(0.51387006f);

    for (i = 0; i < 0x4000; i++)
    {
        pat[i] = out_dither.alpmx * vu0Rand();
    }

    for (i = 0; i < 0x100; i++)
    {
        pal[i] = (u_char)(out_dither.colmx * vu0Rand());
        pal[i] = 0 \
            | i      << 24 \
            | pal[i] << 16 \
            | pal[i] <<  8 \
            | pal[i] <<  0;
    }

    sceGsSetDefLoadImage(&gs_limage1, 0x37fc, 2, SCE_GS_PSMT8, 0, 0, 128, 128);
    sceGsSetDefLoadImage(&gs_limage2, 0x383c, 1, SCE_GS_PSMCT32, 0, 0, 16, 32);

    FlushCache(0);

    sceGsExecLoadImage(&gs_limage1, (u_long128 *)pat);
    sceGsExecLoadImage(&gs_limage2, (u_long128 *)pal);
    
    sceGsSyncPath(0, 0);
}

void DispOutDither()
{
    	/* 0x0(sp) */ SPRITE_DATA sd2 = {
        .g_GsTex0 = {
              .TBP0 = 0x37FC,
              .TBW = 0x2,
              .PSM = 0x13,
              .TW = 0x7,
              .TH = 0x7,
              .TCC = 0x1,
              .TFX = 0x0,
              .CBP = 0x383C,
              .CPSM = 0x0,
              .CSM = 0x0,
              .CSA = 0x0,
              .CLD = 0x1,
        },
           .g_nTexSizeW = 0x80,
           .g_nTexSizeH = 0x80,
           .g_bMipmapLv = 0,
           .g_GsMiptbp1 = 0,
           .g_GsMiptbp2 = 0,
           .pos_x = -320.5f,
           .pos_y = -224.5f,
           .pos_z = 0xFFFFF000,
           .size_w = 640.0f,
           .size_h = 448.0f,
           .scale_w = 1.0f,
           .scale_h = 1.0f,
           .clamp_u = 0,
           .clamp_v = 0,
           .rot_center = 0,
           .angle = 0.0f,
           .r = 0x80,
           .g = 0x80,
           .b = 0x80,
           .alpha = 0x80,
           .mask = 0x0,
    };
	/* 0x60(sp) */ DRAW_ENV de2 = {
        .tex1 = SCE_GS_SET_TEX1_1(1, 0, SCE_GS_LINEAR, SCE_GS_LINEAR_MIPMAP_LINEAR, 0, 0, 0),
        .alpha = SCE_GS_SET_ALPHA_1(SCE_GS_ALPHA_CS, SCE_GS_ALPHA_CD, SCE_GS_ALPHA_AS, SCE_GS_ALPHA_CD, 0),
        .zbuf = SCE_GS_SET_ZBUF_1(0x8c, SCE_GS_PSMCT24, 1),
        .test = SCE_GS_SET_TEST_1(1, SCE_GS_ALPHA_GEQUAL, 0, SCE_GS_AFAIL_KEEP, 0, 0, 1, SCE_GS_DEPTH_GREATER),
        .clamp = SCE_GS_SET_CLAMP_1(SCE_GS_REPEAT, SCE_GS_REPEAT, 0, 0, 0, 0),
        .prim = SCE_GIF_SET_TAG(1, SCE_GS_TRUE, SCE_GS_TRUE, SCE_GS_SET_PRIM(SCE_GS_PRIM_SPRITE, 0, 1, 0, 1, 0, 1, 0, 0), SCE_GIF_PACKED, 5)
    };

    switch(out_dither.type)
    {
    case 1:
        de2.alpha = 0x44;
    break;
    case 2:
        de2.alpha = 0x48;
    break;
    case 3:
        de2.alpha = 0x41;
    break;
    case 4:
        sd2.r = 0x80;
        sd2.g = 0x0;
        sd2.b = 0x80;
        de2.alpha = 0x41;
    break;
    case 5:
        sd2.r = 0x80;
        sd2.g = 0x80;
        sd2.b = 0x0;
        de2.alpha = 0x41;
    break;
    case 6:
        de2.alpha = 0x49;
    break;
    case 7:
        de2.alpha = 0x42;
    break;
    }

    sd2.clamp_u = SCE_GS_SET_CLAMP_1(SCE_GS_REPEAT, SCE_GS_REPEAT, 0, 0, 40, 0);
    sd2.clamp_v = SCE_GS_SET_CLAMP_1(SCE_GS_REPEAT, SCE_GS_REPEAT, 0, 0, 32, 0);
    sd2.alpha = (u_char)((SgSinf(DEG2RAD(out_dither.cnt)) + 1.0f) * out_dither.alp);
    SetTexDirectS2(-2, &sd2, &de2, 0);

    sd2.clamp_u = SCE_GS_SET_CLAMP_1(SCE_GS_REPEAT, SCE_GS_REPEAT, 64, 0, 44, 0);
    sd2.clamp_v = SCE_GS_SET_CLAMP_1(SCE_GS_REPEAT, SCE_GS_REPEAT, 0, 0, 32, 0);
    sd2.alpha = (u_char)((SgSinf(DEG2RAD(out_dither.cnt + 120.0f)) + 1.0f) * out_dither.alp);
    SetTexDirectS2(-2, &sd2, &de2, 0);

    sd2.clamp_u = SCE_GS_SET_CLAMP_1(SCE_GS_REPEAT, SCE_GS_REPEAT, 0, 0, 40, 0);
    sd2.clamp_v = SCE_GS_SET_CLAMP_1(SCE_GS_REPEAT, SCE_GS_REPEAT, 64, 0, 36, 0);
    sd2.alpha = (u_char)((SgSinf(DEG2RAD(out_dither.cnt + 240.0f)) + 1.0f) * out_dither.alp);
    SetTexDirectS2(-2, &sd2, &de2, 0);

    out_dither.cnt += out_dither.spd;
}

int AlbmDesignLoad(u_char side, u_char type)
{
    /* a2 6 */ int64_t addr;
    /* v0 2 */ int load_id;

    if (side == 0)
    {
        addr = MikuPan_GetHostAddress(PL_FNDR_PK2_ADDRESS);
    }

    else if (side == 1)
    {
        addr = MikuPan_GetHostAddress(PL_ALBM_SIDE_1_ADDRESS);
    }

    switch(type)
    {
        case 0:
            load_id = LoadReq(PL_ALBM_BW_PK2, addr);
            break;
        case 1:
            load_id = LoadReq(PL_ALBM_BP_PK2, addr);
            break;
        case 2:
            load_id = LoadReq(PL_ALBM_BR_PK2, addr);
            break;
        case 3:
            load_id = LoadReq(PL_ALBM_BG_PK2, addr);
            break;
        case 4:
            load_id = LoadReq(PL_ALBM_BB_PK2, addr);
            break;
        case 5:
            load_id = LoadReq(PL_ALBM_BO_PK2, addr);
            break;
        default:
            load_id = -1;
            break;
    }

    return load_id;
}
