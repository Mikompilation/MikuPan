#include "iopadpcm.h"
#include "SDL3/SDL_audio.h"
#include "common.h"
#include "enums.h"
#include "iop/cdvd/iopcdvd.h"
#include "iop/iopmain.h"
#include "mikupan/mikupan_file_c.h"
#include "mikupan/mikupan_logging_c.h"
#include "typedefs.h"

#define min(a, b) (((a) < (b)) ? (a) : (b))
#define max(a, b) (((a) > (b)) ? (a) : (b))
#define clamp(val, min, max)                                                   \
    (((val) > (max)) ? (max) : (((val) < (min)) ? (min) : (val)))

IOP_ADPCM iop_adpcm[2];
ADPCM_CMD now_cmd;
ADPCM_CMD cmd_buf[8];
void *AdpcmSpuBuf[2];
void *AdpcmIopBuf[2];

SDL_Mutex *cmd_lock;

/* ADPCM Notes
 * 
 * double buffered
 * Chunk size is 0x800
 * 
 */

/* ADPCM volume fade timer is 0x1047 usec
 * i.e. every 200.016 audio frames at 48000 */

enum
{
    IA_PLAY = 1,
    IA_PRELOAD = 2,
    IA_STOP = 3,
    IA_PAUSE = 4,
    IA_RESTART = 5,
};

static const s32 tbl_adpcm_filter[16][2] = {
    {  0,   0},
    { 60,   0},
    {115, -52},
    { 98, -55},
    {122, -60}
};

static void adpcm_decode_block(s16 *buffer, s16 *block, s32 *prev1, s32 *prev2)
{
    const s32 header = *block;
    const s32 shift = (header & 0xF) + 16;
    const int id = header >> 4 & 0xF;
    const s32 pred1 = tbl_adpcm_filter[id][0];
    const s32 pred2 = tbl_adpcm_filter[id][1];

    const s8 *blockbytes = (s8 *) &block[1];
    const s8 *blockend = &blockbytes[13];

    for (; blockbytes <= blockend; ++blockbytes)
    {
        s32 data = ((*blockbytes) << 28) & 0xF0000000;
        s32 pcm =
            (data >> shift) + (((pred1 * *prev1) + (pred2 * *prev2) + 32) >> 6);

        pcm = clamp(pcm, -0x8000, 0x7fff);
        *(buffer++) = pcm;

        data = ((*blockbytes) << 24) & 0xF0000000;
        s32 pcm2 =
            (data >> shift) + (((pred1 * pcm) + (pred2 * *prev1) + 32) >> 6);

        pcm2 = clamp(pcm2, -0x8000, 0x7fff);
        *(buffer++) = pcm2;

        *prev2 = pcm;
        *prev1 = pcm2;
    }
}

void IAdpcmPreLoad(ADPCM_CMD *acp)
{
    int channel;
    u_char endld_flg;

    channel = acp->channel;
    iop_adpcm[channel].tune_no = acp->tune_no;
    iop_adpcm[channel].stat = 3;
    iop_adpcm[channel].use = 1;
    iop_adpcm[channel].dbids = 0;
    iop_adpcm[channel].dbidi = 0;
    iop_adpcm[channel].str_lpos = 0;
    iop_adpcm[channel].str_tpos = 0;
    iop_adpcm[channel].loop = acp->loop;
    iop_adpcm[channel].count = acp->start_cnt;
    iop_adpcm[channel].loop_end = 0;

    if (acp->size > (acp->start_cnt * 0x1000))
    {
        iop_adpcm[channel].szFile_bk = acp->size;
        iop_adpcm[channel].szFile = acp->size - (acp->start_cnt * 0x1000);
        iop_adpcm[channel].first = acp->first;
        iop_adpcm[channel].start =
            iop_adpcm[channel].first + 2 * acp->start_cnt;
    }
    else
    {
        iop_adpcm[channel].szFile_bk = acp->size;
        iop_adpcm[channel].szFile = acp->size;
        iop_adpcm[channel].first = acp->first;
        iop_adpcm[channel].start = iop_adpcm[channel].first;
    }

    if (iop_adpcm[channel].szFile <= 266240)
    {
        iop_adpcm[channel].lreq_size = iop_adpcm[channel].szFile;
        endld_flg = 1;
    }
    else
    {
        iop_adpcm[channel].lreq_size = 266240;
        endld_flg = 0;
    }

    ICdvdLoadReqAdpcm(iop_adpcm[channel].start, acp->size,
                      AdpcmIopBuf[channel], channel, 1u, endld_flg);
}

void IAdpcmPreLoadEnd(int channel)
{
    int i;
    static int cnt = 0;
    channel = 0;

    if (iop_adpcm[channel].stat == ADPCM_STAT_PRELOAD)
    {
        iop_adpcm[channel].stat = ADPCM_STAT_PRELOAD_TRANS;
        iop_adpcm[channel].start +=
            (iop_adpcm[channel].lreq_size + 2047) / 2048;
        iop_adpcm[channel].str_lpos = iop_adpcm[channel].lreq_size;
        iop_adpcm[channel].str_tpos = 0x2000;
        iop_adpcm[channel].pos = 0x2000;
    }

    if (iop_adpcm[channel].use)
        iop_adpcm[channel].stat = ADPCM_STAT_PRELOAD_END;
}

void IAdpcmPlay(ADPCM_CMD *acp)
{
    u_char channel = acp->channel;

    static s16 dec_buf[2][3584];
    void *dec[2] = {dec_buf[0], dec_buf[1]};

    s32 histL[2] = {}, histR[2] = {};

    s16 *src = AdpcmIopBuf[channel];
    s16 *dst;

    int chunks = acp->size / 0x800 / 2;

    // Prevent memleaks by not reading too much into the audio stream.
    // Let's make sure we're actually playing something first!!
    if (SDL_GetAudioStreamQueued(iop_adpcm[channel].stream) >= acp->size
        && iop_adpcm[channel].stat == ADPCM_STAT_PLAY)
    {
        return;
    }

    for (int i = 0; i < chunks; i++)
    {
        dst = dec_buf[0];
        for (int j = 0; j < 128; j++)
        {

            adpcm_decode_block(dst, src, &histL[0], &histL[1]);
            dst += 28;
            src += 8;
        }

        dst = dec_buf[1];
        for (int j = 0; j < 128; j++)
        {

            adpcm_decode_block(dst, src, &histR[0], &histR[1]);
            dst += 28;
            src += 8;
        }

        SDL_PutAudioStreamPlanarData(iop_adpcm[channel].stream, (void *) dec, 2,
                                     3584);
    }

    SDL_SetAudioStreamFrequencyRatio(iop_adpcm[channel].stream,
                                     (float) acp->pitch / (float) 0x1000);

    if (iop_adpcm[channel].stat == ADPCM_STAT_PRELOAD_END)
    {
        iop_adpcm[channel].loop_end = 0;
        iop_adpcm[channel].stat = ADPCM_STAT_PLAY;

        SDL_ResumeAudioStreamDevice(iop_adpcm[channel].stream);
    }

    return;
}

void IAdpcmStop(ADPCM_CMD *acp)
{

    u_char channel;

    SDL_PauseAudioDevice(audio_dev);

    channel = acp->channel;
    //sceSdSetCoreAttr(iop_adpcm[channel].core | 4, 0);
    IaSetWrkFadeInit(channel);

    if (iop_adpcm[channel].stat == ADPCM_STAT_NOPLAY)
    {
        if (iop_adpcm[channel].use)
        {
            iop_adpcm[channel].use = 0;
        }
    }
    else
    {
        if (iop_adpcm[channel].stat == ADPCM_STAT_PRELOAD)
        {
            ICdvdBreak();
            iop_adpcm[channel].use = 0;
        }
        else if (iop_adpcm[channel].stat == ADPCM_STAT_PRELOAD_END)
        {
            iop_adpcm[channel].use = 0;
        }
        else
        {
            if (cdvd_stat.stat == ADPCM_STAT_LOOPEND_STOP)
                ICdvdBreak();
            IaSetRegKoff(channel);
            //sceSdSetTransIntrHandler(channel, 0, 0);
            //sceSdVoiceTransStatus(channel, 1);
            iop_adpcm[channel].use = 0;
        }
    }

    if (IAdpcmChkCmdExist())
    {
        iop_adpcm[channel].stat = ADPCM_STAT_NOPLAY;
    }
    else
    {
        iop_adpcm[channel].stat = ADPCM_STAT_NOPLAY;
    }

    iop_adpcm[channel].tune_no = 0;
}

void IAdpcmReadCh0()
{
}

void IAdpcmReadCh1()
{
}

static void IAdpcmFadeVol(IOP_COMMAND *iop)
{
}

static void IAdpcmPos(IOP_COMMAND *icp)
{
    int channel;

    channel = icp->data3;
    IaSetWrkVolPanPitch(channel, (icp->data5 >> 16) & 0xffff,
                        icp->data5 & 0xffff, icp->data6 & 0xffff);
    IaSetRegVol(channel);
    IaSetRegPitch(channel);
}

static void IAdpcmMvol(IOP_COMMAND *icp)
{
    u_short mvol = icp->data1 & 0xffff;

    IaSetMasterVol(mvol);
}

void IAdpcmMain2()
{
    IOP_ADPCM *iap;
    ADPCM_CMD ac;

    iap = &iop_adpcm[0];

    if (now_cmd.cmd_type == AC_NONE)
    {
        if (cmd_buf[0].cmd_type == AC_NONE)
        {
        }
        else
        {
            IAdpcmCmdSlide();
        }
    }
    else
    {
        switch (now_cmd.cmd_type)
        {
            case AC_PLAY:
                IAdpcmCmdPlay();
                break;
            case AC_PRELOAD:
                IAdpcmCmdPreLoad();
                break;
            case AC_STOP:
                IAdpcmCmdStop();
                break;
            case AC_PAUSE:
                IAdpcmCmdPause();
                break;
            case AC_RESTART:
                IAdpcmCmdRestart();
                break;
        }
    }
    switch (iap->fade_mode)
    {
        case ADPCM_FADE_NO:
            break;
        case ADPCM_FADE_IN_PLAY:
        case ADPCM_FADE_OUT_STOP:
        case ADPCM_FADE:
            if (iap->fade_flm <= iap->fade_count)
            {
                if (!iap->target_vol && iap->fade_mode == 2)
                {
                    ac.cmd_type = 3;
                    ac.channel = 0;
                    ac.tune_no = iop_adpcm[0].tune_no;
                    IAdpcmStop(&ac);
                }
                iap->vol = iap->target_vol;
                iap->fade_count = 0;
                iap->fade_flm = 0;
                iap->target_vol = 0;
                iap->fade_mode = 0;
            }
            break;
    }

    iop_stat.adpcm.tune_no = iap->tune_no;
    iop_stat.adpcm.count = iap->count;

    if (iap->stat == ADPCM_STAT_NOPLAY)
    {
        iap->stop_cnt++;
        if (iap->stop_cnt >= 41)
        {
            iap->stop_cnt = 16;
        }
    }
    else
    {
        iap->stop_cnt = 0;
    }

    if (iap->stop_cnt >= 16)
    {
        if (iap->loop_end)
        {
            iop_stat.adpcm.status = ADPCM_STAT_LOOPEND_STOP;
        }
        else
        {
            iop_stat.adpcm.status = ADPCM_STAT_FULL_STOP;
        }
    }
    else
    {
        iop_stat.adpcm.status = iap->stat;
    }
}

void IAdpcmInit(int mode)
{
    SDL_AudioSpec spec;

    spec.channels = 2;
    spec.format = SDL_AUDIO_S16;
    spec.freq = 48000;

    iop_adpcm[0].stream = SDL_CreateAudioStream(&spec, NULL);
    SDL_BindAudioStream(audio_dev, iop_adpcm[0].stream);
    cmd_lock = SDL_CreateMutex();
}

void IAdpcmAddCmd(IOP_COMMAND *cmd)
{
    ADPCM_CMD adpcm = {};
    int i;

    adpcm.cmd_type = 0;
    switch (cmd->cmd_no)
    {
        case IC_ADPCM_PLAY:
            adpcm.cmd_type = AC_PLAY;
            adpcm.fade_flm = cmd->data6;
            adpcm.first = cmd->data2;
            adpcm.size = cmd->data3;
            adpcm.start_cnt = cmd->data7;
            adpcm.tune_no = cmd->data1;
            adpcm.vol = cmd->data5;
            adpcm.target_vol = adpcm.vol;
            adpcm.pan = cmd->data5 >> 16;
            adpcm.pitch = cmd->data6 >> 16;
            adpcm.loop = cmd->data4 & 2;
            adpcm.channel = cmd->data4 & 1;
            if (adpcm.fade_flm)
            {
                adpcm.fade_mode = 1;
                adpcm.vol = 0;
            }
            else
            {
                adpcm.fade_mode = 0;
            }
            break;
        case IC_ADPCM_PRELOAD:
            adpcm.cmd_type = AC_PRELOAD;
            adpcm.tune_no = cmd->data1;
            adpcm.first = cmd->data2;
            adpcm.size = cmd->data3;
            adpcm.start_cnt = cmd->data7;
            adpcm.channel = cmd->data4 & 1;
            break;
        case IC_ADPCM_LOADEND_PLAY:
            adpcm.cmd_type = AC_PLAY;
            adpcm.fade_flm = cmd->data6;
            adpcm.start_cnt = cmd->data7;
            adpcm.tune_no = cmd->data1;
            adpcm.vol = cmd->data5;
            adpcm.target_vol = adpcm.vol;
            adpcm.pan = cmd->data5 >> 16;
            adpcm.pitch = cmd->data6 >> 16;
            adpcm.loop = cmd->data4 & 2;
            adpcm.channel = cmd->data4 & 1;
            if (adpcm.fade_flm)
            {
                adpcm.fade_mode = 1;
                adpcm.vol = 0;
            }
            else
            {
                adpcm.fade_mode = 0;
            }
            break;
        case IC_ADPCM_STOP:
            adpcm.cmd_type = AC_STOP;
            adpcm.fade_flm = cmd->data6;
            adpcm.tune_no = cmd->data1;
            adpcm.channel = cmd->data4 & 1;
            break;
        case IC_ADPCM_PAUSE:
            adpcm.cmd_type = AC_PAUSE;
            adpcm.fade_flm = cmd->data6;
            adpcm.tune_no = cmd->data1;
            adpcm.channel = cmd->data4 & 1;
            break;
        case IC_ADPCM_RESTART:
            adpcm.cmd_type = AC_RESTART;
            adpcm.fade_flm = cmd->data6;
            adpcm.tune_no = cmd->data1;
            adpcm.channel = cmd->data4 & 1;
            break;
        default:
            break;
    }

    // Insert into cmd buffer
    if (adpcm.cmd_type)
    {
        SDL_LockMutex(cmd_lock);
        for (i = 0; i < 8; i++)
        {
            if (cmd_buf[i].cmd_type == AC_NONE)
            {
                info_log("adpcm: inserted command %d", adpcm.cmd_type);
                cmd_buf[i] = adpcm;
                break;
            }
        }
        SDL_UnlockMutex(cmd_lock);
    }
}

void IAdpcmCmd(IOP_COMMAND *icp)
{
    switch (icp->cmd_no)
    {
        case IC_ADPCM_PLAY:
        case IC_ADPCM_PRELOAD:
        case IC_ADPCM_LOADEND_PLAY:
        case IC_ADPCM_STOP:
        case IC_ADPCM_PAUSE:
        case IC_ADPCM_RESTART:
            IAdpcmAddCmd(icp);
            break;
        case IC_ADPCM_INIT:
            IAdpcmInit(icp->data1);
            break;
        case IC_ADPCM_FADE_VOL:
            IAdpcmFadeVol(icp);
            break;
        case IC_ADPCM_POS:
            IAdpcmPos(icp);
            break;
        case IC_ADPCM_MVOL:
            IAdpcmMvol(icp);
        case IC_ADPCM_QUIT:
            break;
        default:
            info_log("Error: ADPCM Command %d not yet implemented!",
                     icp->cmd_no);
            break;
    }
}

void SetLoopFlgSize(u_int size_byte, u_int *start, u_short core)
{
    return;
}

void IAdpcmLoadEndStream(int channel)
{
    iop_adpcm[channel].str_lpos += iop_adpcm[channel].lreq_size;
    iop_adpcm[channel].start += (iop_adpcm[channel].lreq_size + 2047) / 2048;
}

void IAdpcmLoadEndPreOnly(int channel)
{
    IAdpcmPreLoadEnd(channel);
}
