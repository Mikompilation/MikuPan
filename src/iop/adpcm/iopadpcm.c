#include "iopadpcm.h"
#include "typedefs.h"
#include "common.h"
#include "iop/iopmain.h"
#include "iop/cdvd/iopcdvd.h"
#include "enums.h"
#include "mikupan/mikupan_file_c.h"
#include "SDL3/SDL_audio.h"

#define min(a, b) (((a) < (b)) ? (a) : (b))
#define max(a, b) (((a) > (b)) ? (a) : (b))
#define clamp(val, min, max)                                                   \
    (((val) > (max)) ? (max) : (((val) < (min)) ? (min) : (val)))



IOP_ADPCM iop_adpcm[2];
ADPCM_CMD now_cmd;
ADPCM_CMD cmd_buf[8];

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

int IAdpcmPreLoad(ADPCM_CMD *cmd)
{
    static s16 dec_buf[2][3584];
    void *dec[2] = {dec_buf[0], dec_buf[1]};

    s32 histL[2] = {}, histR[2] = {};

    cmd->f_size = (cmd->f_size + (0x800 - 1)) & ~(0x800 - 1);
    int chunks = cmd->f_size / 0x800 / 2;

    iop_adpcm[0].tune_no = cmd->file_no;

    info_log("adpcm read(%x, %x, %p)", cmd->f_start, cmd->f_size,
             &iop_adpcm[0].data);
    info_log("p: %x", cmd->pitch);
    MikuPan_ReadFileInArchive64(cmd->f_start, cmd->f_size, &iop_adpcm[0].data);

    info_log("chunks: %d", chunks);

    s16 *src = iop_adpcm[0].data;
    s16 *dst;

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

        SDL_PutAudioStreamPlanarData(iop_adpcm[0].stream, (void *) dec, 2,
                                     3584);
    }

    SDL_SetAudioStreamFrequencyRatio(iop_adpcm[0].stream,
                                     (float) cmd->pitch / (float) 0x1000);
    SDL_ResumeAudioStreamDevice(iop_adpcm[0].stream);

    return 0;
}

int IAdpcmStop(ADPCM_CMD *cmd)
{
    return 0;
}

void IAdpcmCmdSlide()
{
    int i;

    SDL_LockMutex(cmd_lock);

    now_cmd = cmd_buf[0];
    for (i = 0; i < 7; i++)
    {
        cmd_buf[i] = cmd_buf[i + 1];
    }

    cmd_buf[7].action = 0;

    SDL_UnlockMutex(cmd_lock);
}

int IAdpcmCmdPlay()
{
    info_log("IAdpcmCmdPlay");

    if (now_cmd.file_no != iop_adpcm[0].tune_no)
    {
        IAdpcmPreLoad(&now_cmd);
        return 0;
    }

    return 0;
}

int IAdpcmCmdPreLoad()
{
    info_log("IAdpcmCmdPreLoad");
    if (now_cmd.file_no == iop_adpcm[0].tune_no)
    {
        if (iop_adpcm[0].state == 5)
        {
            now_cmd.action = 0;
        }
    }

    switch (iop_adpcm[0].state)
    {
        case 4:
            return 0;
        case 0:
        case 5:
            IAdpcmPreLoad(&now_cmd);
            break;
    }

    if (iop_adpcm[0].f_mode != 2)
    {
        ADPCM_CMD cmd;
        cmd.action = 3;
        cmd.channel = 0;
        cmd.file_no = iop_adpcm[0].tune_no;
        IAdpcmStop(&cmd);
    }

    return 0;
}

int IAdpcmCmdStop()
{
    info_log("IAdpcmCmdStop");

    switch (iop_adpcm[0].state)
    {
        case 0:
            now_cmd.action = 0;
            break;
        case 5:
        case 7:
        case 8:
        case 9:
            break;
    }

    return 0;
}

int IAdpcmCmdPause()
{
    info_log("IAdpcmCmdPause");
    return 0;
}

int IAdpcmCmdRestart()
{
    //?? seems like it might do nothing
    return 0;
}

void IAdpcmMain2()
{
    if (!now_cmd.action)
    {
        IAdpcmCmdSlide();
    }
    else
    {
        switch (now_cmd.action)
        {
            case 1:
                IAdpcmCmdPlay();
                break;
            case 2:
                IAdpcmCmdPreLoad();
                break;
            case 3:
                IAdpcmCmdStop();
                break;
            case 4:
                IAdpcmCmdPause();
                break;
            case 5:
                IAdpcmCmdRestart();
                break;
        }
    }

    if (iop_adpcm[0].state)
    {
        iop_adpcm[0].state_timeout = 0;
    }
    else
    {
        iop_adpcm[0].state_timeout = min(iop_adpcm[0].state_timeout + 1, 16);
    }

    if (iop_adpcm[0].state_timeout < 16)
    {
        iop_stat.adpcm.status = iop_adpcm[0].state;
    }
    else
    {
        iop_stat.adpcm.status = 1;
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

    adpcm.action = 0;
    switch (cmd->cmd_no)
    {
        case IC_ADPCM_PLAY:
            adpcm.action = IA_PLAY;
            adpcm.fin_flm = cmd->data6;
            adpcm.f_start = cmd->data2;
            adpcm.f_size = cmd->data3;
            adpcm.start_flm = cmd->data7;
            adpcm.file_no = cmd->data1;
            adpcm.voll = cmd->data5;
            adpcm.volr = adpcm.voll;
            adpcm.pan = cmd->data5 >> 16;
            adpcm.pitch = cmd->data6 >> 16;
            adpcm.field_1B = cmd->data4 & 2;
            adpcm.channel = cmd->data4 & 1;
            if (adpcm.fin_flm)
            {
                adpcm.field_1D = 1;
                adpcm.voll = 0;
            }
            else
            {
                adpcm.field_1D = 0;
            }
            break;
        case IC_ADPCM_PRELOAD:
            adpcm.action = IA_PRELOAD;
            adpcm.file_no = cmd->data1;
            adpcm.f_start = cmd->data2;
            adpcm.f_size = cmd->data3;
            adpcm.start_flm = cmd->data7;
            adpcm.channel = cmd->data4 & 1;
            break;
        case IC_ADPCM_LOADEND_PLAY:
            adpcm.action = IA_PLAY;
            adpcm.fin_flm = cmd->data6;
            adpcm.start_flm = cmd->data7;
            adpcm.file_no = cmd->data1;
            adpcm.voll = cmd->data5;
            adpcm.volr = adpcm.voll;
            adpcm.pan = cmd->data5 >> 16;
            adpcm.pitch = cmd->data6 >> 16;
            adpcm.field_1B = cmd->data4 & 2;
            adpcm.channel = cmd->data4 & 1;
            if (adpcm.fin_flm)
            {
                adpcm.field_1D = 1;
                adpcm.voll = 0;
            }
            else
            {
                adpcm.field_1D = 0;
            }
            break;
        case IC_ADPCM_STOP:
            adpcm.action = IA_STOP;
            adpcm.fin_flm = cmd->data6;
            adpcm.file_no = cmd->data1;
            adpcm.channel = cmd->data4 & 1;
            break;
        case IC_ADPCM_PAUSE:
            adpcm.action = IA_PAUSE;
            adpcm.fin_flm = cmd->data6;
            adpcm.file_no = cmd->data1;
            adpcm.channel = cmd->data4 & 1;
            break;
        case IC_ADPCM_RESTART:
            adpcm.action = IA_RESTART;
            adpcm.fin_flm = cmd->data6;
            adpcm.file_no = cmd->data1;
            adpcm.channel = cmd->data4 & 1;
            break;
        default:
            break;
    }

    // Insert into cmd buffer
    if (adpcm.action)
    {
        SDL_LockMutex(cmd_lock);
        for (i = 0; i < 8; i++)
        {
            if (cmd_buf[i].action == 0)
            {
                info_log("adpcm: inserted command %d", adpcm.action);
                cmd_buf[i] = adpcm;
                break;
            }
        }
        SDL_UnlockMutex(cmd_lock);
    }
}

void IAdpcmCmd(IOP_COMMAND *cmd)
{
    switch (cmd->cmd_no)
    {
        case IC_ADPCM_PLAY:
        case IC_ADPCM_PRELOAD:
        case IC_ADPCM_LOADEND_PLAY:
        case IC_ADPCM_STOP:
        case IC_ADPCM_PAUSE:
        case IC_ADPCM_RESTART:
            IAdpcmAddCmd(cmd);
            break;
        default:
            info_log("Error: ADPCM Command %d not yet implemented!",
                     cmd->cmd_no);
            break;
    }
}