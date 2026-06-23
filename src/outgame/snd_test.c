#include "common.h"
#include "typedefs.h"
#include "enums.h"
#include "outgame/snd_test.h"

#include "graphics/graph2d/effect_sub.h"
#include "graphics/graph2d/g2d_main.h"
#include "graphics/graph2d/message.h"
#include "main/glob.h"
#include "os/eeiop/adpcm/ea_cmd.h"
#include "os/eeiop/adpcm/ea_ctrl.h"
#include "os/eeiop/adpcm/ea_dat.h"
#include "os/eeiop/cdvd/eecdvd.h"
#include "os/eeiop/eeiop.h"
#include "os/eeiop/eese.h"
#include "os/eeiop/se_data.h"
#include "outgame/outgame.h"

#include <stdio.h>
#include <string.h>

#define SND_TEST_ROW_NUM 11
#define SND_TEST_SE_MAX SE_ENE_JIDOU3
#define SND_TEST_SE_VOL_MAX 0x1000
#define SND_TEST_PITCH_MIN 0x0800
#define SND_TEST_PITCH_MAX 0x1800
#define SND_TEST_PAN_MAX 0x04ff

typedef enum {
    SND_TEST_SRC_ADPCM,
    SND_TEST_SRC_SE,
    SND_TEST_SRC_NUM
} SND_TEST_SOURCE;

typedef struct {
    int src;
    int csr;
    int adpcm_idx;
    int se_no;
    int bank_file_no;
    int bank_type;
    int vol;
    int pitch;
    int pan;
    int loop;
    int load_id;
    int last_file_no;
    int last_voice;
    const char *state_str;
} SND_TEST_WRK;

static SND_TEST_WRK snd_test_wrk;

static void SndTestSetSquare(int pri, float x, float y, float w, float h, u_char r, u_char g, u_char b, u_char a)
{
    SetSquare(pri, x - 320.0f, y - 224.0f, (x - 320.0f) + w, y - 224.0f,
              x - 320.0f, (y - 224.0f) + h, (x - 320.0f) + w, (y - 224.0f) + h,
              r, g, b, a);
}

static void SndTestWrapInt(int *value, int min, int max)
{
    if (*value < min)
    {
        *value = max;
    }
    else if (*value > max)
    {
        *value = min;
    }
}

static void SndTestClampInt(int *value, int min, int max)
{
    if (*value < min)
    {
        *value = min;
    }
    else if (*value > max)
    {
        *value = max;
    }
}

static int SndTestCurrentAdpcmFile(void)
{
    return adpcm_vol_tbl[snd_test_wrk.adpcm_idx].file_no;
}

static void SndTestSetDefaultVol(void)
{
    if (snd_test_wrk.src == SND_TEST_SRC_ADPCM)
    {
        snd_test_wrk.vol = adpcm_vol_tbl[snd_test_wrk.adpcm_idx].vol;
        SndTestClampInt(&snd_test_wrk.vol, 0, MAX_VOLUME);
    }
    else
    {
        SndTestClampInt(&snd_test_wrk.vol, 0, SND_TEST_SE_VOL_MAX);
    }
}

static void SndTestDrawLine(int line, char *str)
{
    SetASCIIString2(1, 40.0f, line * 14 + 48, 1, 0x80, 0x80, 0x80, str);
}

static void SndTestLoadBank(void)
{
    snd_test_wrk.load_id = SeFileLoadAndSet(snd_test_wrk.bank_file_no, snd_test_wrk.bank_type);

    if (snd_test_wrk.load_id >= 0)
    {
        snd_test_wrk.state_str = "BANK LOAD";
    }
    else
    {
        snd_test_wrk.state_str = "BANK LOAD ERR";
    }
}

static void SndTestStop(void)
{
    EAdpcmCmdStop(0, 0, 0);
    SeStopAll();
    snd_test_wrk.last_voice = -1;

    if (snd_test_wrk.src == SND_TEST_SRC_ADPCM)
    {
        snd_test_wrk.state_str = "ADPCM STOP";
    }
    else
    {
        snd_test_wrk.state_str = "SE STOP";
    }
}

static void SndTestPlay(void)
{
    int file_no;
    int voice_no;
    int se_vol;

    if (snd_test_wrk.src == SND_TEST_SRC_ADPCM)
    {
        file_no = SndTestCurrentAdpcmFile();
        EAdpcmCmdStop(0, 0, 0);
        EAdpcmCmdPlay(0, snd_test_wrk.loop, file_no, 0, snd_test_wrk.vol, snd_test_wrk.pan, snd_test_wrk.pitch, 0);

        snd_test_wrk.last_file_no = file_no;
        snd_test_wrk.state_str = "ADPCM PLAY";
        return;
    }

    se_vol = snd_test_wrk.vol;
    SndTestClampInt(&se_vol, 0, SND_TEST_SE_VOL_MAX);

    voice_no = SeStartFix(snd_test_wrk.se_no, 0, se_vol, snd_test_wrk.pitch, 0);
    snd_test_wrk.last_voice = voice_no;

    if (voice_no >= 0)
    {
        snd_test_wrk.state_str = "SE PLAY";
    }
    else
    {
        snd_test_wrk.state_str = "SE NO USE";
    }
}

static void SndTestApplyChange(int changed)
{
    switch (snd_test_wrk.csr)
    {
    case 0:
        snd_test_wrk.src += changed;
        SndTestWrapInt(&snd_test_wrk.src, 0, SND_TEST_SRC_NUM - 1);
        SndTestSetDefaultVol();
        break;
    case 1:
        if (snd_test_wrk.src == SND_TEST_SRC_ADPCM)
        {
            snd_test_wrk.adpcm_idx += changed;
            SndTestWrapInt(&snd_test_wrk.adpcm_idx, 0, NO_VOL_TABLE - 1);
            SndTestSetDefaultVol();
        }
        else
        {
            snd_test_wrk.se_no += changed;
            SndTestWrapInt(&snd_test_wrk.se_no, 0, SND_TEST_SE_MAX);
        }
        break;
    case 2:
        if (snd_test_wrk.src == SND_TEST_SRC_SE)
        {
            snd_test_wrk.bank_file_no += changed;
            SndTestWrapInt(&snd_test_wrk.bank_file_no, SSYSTEM_BD, ST014_NINGYOU_BD);
        }
        break;
    case 3:
        snd_test_wrk.vol += changed * 0x100;
        if (snd_test_wrk.src == SND_TEST_SRC_ADPCM)
        {
            SndTestClampInt(&snd_test_wrk.vol, 0, MAX_VOLUME);
        }
        else
        {
            SndTestClampInt(&snd_test_wrk.vol, 0, SND_TEST_SE_VOL_MAX);
        }
        break;
    case 4:
        snd_test_wrk.pitch += changed * 0x80;
        SndTestClampInt(&snd_test_wrk.pitch, SND_TEST_PITCH_MIN, SND_TEST_PITCH_MAX);
        break;
    case 5:
        if (snd_test_wrk.src == SND_TEST_SRC_ADPCM)
        {
            snd_test_wrk.pan += changed * 0x40;
            SndTestClampInt(&snd_test_wrk.pan, 0, SND_TEST_PAN_MAX);
        }
        else
        {
            snd_test_wrk.bank_type += changed;
            SndTestWrapInt(&snd_test_wrk.bank_type, SE_ADDRNO_STATIC, SE_ADDRNO_JIDOU3);
        }
        break;
    case 6:
        if (snd_test_wrk.src == SND_TEST_SRC_ADPCM)
        {
            snd_test_wrk.loop ^= 1;
        }
        break;
    default:
        break;
    }
}

static int SndTestPadCtrl(void)
{
    int changed;

    if (pad[0].one & 0x40)
    {
        SndTestStop();
        OutGameModeChange(OUTGAME_MODE_TITLE);
        return 1;
    }

    if (pad[0].rpt & 0x4000)
    {
        snd_test_wrk.csr++;
        SndTestWrapInt(&snd_test_wrk.csr, 0, SND_TEST_ROW_NUM - 1);
    }
    else if (pad[0].rpt & 0x1000)
    {
        snd_test_wrk.csr--;
        SndTestWrapInt(&snd_test_wrk.csr, 0, SND_TEST_ROW_NUM - 1);
    }

    changed = 0;

    if (pad[0].rpt & 0x2000)
    {
        changed = 1;
    }
    else if (pad[0].rpt & 0x8000)
    {
        changed = -1;
    }

    if (changed != 0)
    {
        SndTestApplyChange(changed);
    }

    if (pad[0].one & 0x800)
    {
        SndTestPlay();
    }

    if (pad[0].one & 0x20)
    {
        switch (snd_test_wrk.csr)
        {
        case 6:
            if (snd_test_wrk.src == SND_TEST_SRC_ADPCM)
            {
                snd_test_wrk.loop ^= 1;
            }
            break;
        case 7:
            if (snd_test_wrk.src == SND_TEST_SRC_SE)
            {
                SndTestLoadBank();
            }
            break;
        case 8:
            SndTestPlay();
            break;
        case 9:
            SndTestStop();
            break;
        case 10:
            SndTestStop();
            OutGameModeChange(OUTGAME_MODE_TITLE);
            return 1;
        default:
            break;
        }
    }

    return 0;
}

static void SndTestLoadCtrl(void)
{
    if (snd_test_wrk.load_id >= 0 && IsLoadEnd(snd_test_wrk.load_id) != 0)
    {
        snd_test_wrk.load_id = -1;
        snd_test_wrk.state_str = "BANK READY";
    }
}

static void SndTestDraw(void)
{
    char tmp_str[256];

    SetASCIIString2(1, 40.0f, 28.0f, 0, 0x20, 0x80, 0x80, "SND TEST");

    sprintf(tmp_str, "SOURCE %s", snd_test_wrk.src == SND_TEST_SRC_ADPCM ? "ADPCM CATALOG" : "SE BANK");
    SndTestDrawLine(0, tmp_str);

    if (snd_test_wrk.src == SND_TEST_SRC_ADPCM)
    {
        sprintf(tmp_str, "INDEX  %03d/%03d", snd_test_wrk.adpcm_idx, NO_VOL_TABLE - 1);
        SndTestDrawLine(1, tmp_str);

        sprintf(tmp_str, "FILE   %04d", SndTestCurrentAdpcmFile());
        SndTestDrawLine(2, tmp_str);
    }
    else
    {
        sprintf(tmp_str, "SE NO  %03d  USE %d", snd_test_wrk.se_no, CheckSeUse(snd_test_wrk.se_no));
        SndTestDrawLine(1, tmp_str);

        sprintf(tmp_str, "BANK   %04d", snd_test_wrk.bank_file_no);
        SndTestDrawLine(2, tmp_str);
    }

    sprintf(tmp_str, "VOL    %04X", snd_test_wrk.vol);
    SndTestDrawLine(3, tmp_str);

    sprintf(tmp_str, "PITCH  %04X", snd_test_wrk.pitch);
    SndTestDrawLine(4, tmp_str);

    if (snd_test_wrk.src == SND_TEST_SRC_ADPCM)
    {
        sprintf(tmp_str, "PAN    %04X", snd_test_wrk.pan);
    }
    else
    {
        sprintf(tmp_str, "TYPE   %02d", snd_test_wrk.bank_type);
    }
    SndTestDrawLine(5, tmp_str);

    if (snd_test_wrk.src == SND_TEST_SRC_ADPCM)
    {
        sprintf(tmp_str, "LOOP   %s", snd_test_wrk.loop != 0 ? "ON" : "OFF");
    }
    else
    {
        sprintf(tmp_str, "LOOP   -");
    }
    SndTestDrawLine(6, tmp_str);

    sprintf(tmp_str, "LOAD BANK%s", snd_test_wrk.src == SND_TEST_SRC_ADPCM ? " -" : "");
    SndTestDrawLine(7, tmp_str);

    sprintf(tmp_str, "PLAY");
    SndTestDrawLine(8, tmp_str);

    sprintf(tmp_str, "STOP");
    SndTestDrawLine(9, tmp_str);

    sprintf(tmp_str, "EXIT");
    SndTestDrawLine(10, tmp_str);

    sprintf(tmp_str, "STATE %s  ADPCM ST %d TUNE %d CNT %d  LAST SE %d",
            snd_test_wrk.state_str, EAGetRetStat(), EAGetRetTune(), EAGetRetCount(), snd_test_wrk.last_voice);
    SetASCIIString2(1, 40.0f, 386.0f, 1, 0x80, 0x80, 0x80, tmp_str);

    sprintf(tmp_str, "X DECIDE  START PLAY  O EXIT");
    SetASCIIString2(1, 40.0f, 410.0f, 1, 0x80, 0x80, 0x80, tmp_str);

    SndTestSetSquare(2, 36.0f, snd_test_wrk.csr * 14 + 46, 420.0f, 12.0f, 0x50, 0x50, 0x64, 0x50);
}

void SndTestInit(void)
{
    memset(&snd_test_wrk, 0, sizeof(snd_test_wrk));

    gra2dInitST();
    EAdpcmCmdStop(0, 0, 0);
    SeStopAll();
    SeSetMVol(SND_TEST_SE_VOL_MAX);

    snd_test_wrk.src = SND_TEST_SRC_ADPCM;
    snd_test_wrk.adpcm_idx = 0;
    snd_test_wrk.se_no = SE_CSR0;
    snd_test_wrk.bank_file_no = SSYSTEM_BD;
    snd_test_wrk.bank_type = SE_ADDRNO_STATIC;
    snd_test_wrk.pitch = 0x0fff;
    snd_test_wrk.pan = 0x0280;
    snd_test_wrk.loop = 0;
    snd_test_wrk.load_id = -1;
    snd_test_wrk.last_file_no = -1;
    snd_test_wrk.last_voice = -1;
    snd_test_wrk.state_str = "READY";
    SndTestSetDefaultVol();
}

void SndTestCtrl(void)
{
    GetIopStatP();
    SeWrkUpdate();
    SndTestLoadCtrl();

    if (SndTestPadCtrl() != 0)
    {
        return;
    }

    SndTestDraw();
    gra2dDraw(GRA2D_CALL_OG);
}
