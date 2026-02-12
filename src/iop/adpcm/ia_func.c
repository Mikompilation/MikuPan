#include "iopadpcm.h"

#include "iop/se/iopse.h"

#include <stdlib.h>

#include "memory.h"
#include "os/system.h"
#include "ee/eekernel.h"
#include "iop/iopmain.h"
#include "iop/se/iopse.h"
#include "SDL3/SDL_thread.h"
#include "mikupan/mikupan_memory.h"
#include "sce/libsd.h"


void GetPosCalc(ADPCM_POS_CALC* calcp);

static int IAdpcmMakeThread(u_char channel);

void IaInitDev(u_char channel)
{
    memset(&iop_adpcm[channel], 0, sizeof(IOP_ADPCM));
   AdpcmIopBuf[channel] = (void*)malloc(266240);
   AdpcmSpuBuf[0] = (s16*)MikuPan_GetHostPointer(0x1F3740);
   AdpcmSpuBuf[1] = (s16*)MikuPan_GetHostPointer(0x1F6780);

}

static int IAdpcmMakeThread(u_char channel)
{
    SDL_AudioSpec spec;        
    spec.channels = 2;
    spec.format = SDL_AUDIO_S16;
    spec.freq = 48000;
    
    info_log("Adpcm Thread unused \n");

    /*if (channel == 0)
    {
        iop_adpcm[0].thread = SDL_CreateThread(IAdpcmReadCh0, "Core 0", NULL);
        iop_adpcm[0].stream = SDL_CreateAudioStream(&spec, NULL);
        SDL_BindAudioStream(audio_dev, iop_adpcm[0].stream);
        return 0;
    }
    else
    {
        iop_adpcm[1].thread = SDL_CreateThread(IAdpcmReadCh0, "Core 1", NULL);
        iop_adpcm[1].stream = SDL_CreateAudioStream(&spec, NULL);
        SDL_BindAudioStream(audio_dev, iop_adpcm[1].stream);
        return 1;
    }*/
    return 0;
}

void IaInitEffect()
{
    //sceSdSetCoreAttr(SD_C_EFFECT_ENABLE | SD_CORE_0, 0);
}

void IaInitVolume()
{
    float ll = iop_adpcm[0].vol_ll / 16383.0f;
    float lr = iop_adpcm[0].vol_lr / 16383.0f;
    float rl = iop_adpcm[0].vol_rl / 16383.0f;
    float rr = iop_adpcm[0].vol_rr / 16383.0f;

    float left  = ll + rl;
    float right = lr + rr;

    float gain = (left + right) * 0.5f;

    float pan = 0.0f;
    if (left + right > 0.0001f)
        pan = (right - left) / (left + right);

    SDL_SetAudioStreamGain(iop_adpcm[0].stream, gain);
    //SDL_SetAudioStreamPan(iop_adpcm[0].stream, pan);
}

void IaDbgMemoryCheck()
{
    int tmp;

    //tmp = QueryMemSize();
    //tmp = QueryTotalFreeMemSize();
    //tmp = QueryMaxFreeMemSize();
}

void IaSetRegSsa(u_char channel)
{
    //sceSdSetAddr(iop_adpcm[channel].core | iop_adpcm[channel].vl | SD_VA_SSA, (u_int)AdpcmSpuBuf[channel]);
    //sceSdSetAddr(iop_adpcm[channel].core | iop_adpcm[channel].vr | SD_VA_SSA, (u_int)(AdpcmSpuBuf[channel] + 4096));
}

void IaSetRegAdsr(u_char channel)
{
    /*sceSdSetParam(iop_adpcm[channel].core | iop_adpcm[channel].vl | SD_VP_ADSR1, 0xFu);
    sceSdSetParam(iop_adpcm[channel].core | iop_adpcm[channel].vl | SD_VP_ADSR2, 0x1FC0u);
    sceSdSetParam(iop_adpcm[channel].core | iop_adpcm[channel].vr | SD_VP_ADSR1, 0xFu);
    sceSdSetParam(iop_adpcm[channel].core | iop_adpcm[channel].vr | SD_VP_ADSR2, 0x1FC0u);*/
}

void IaSetRegVol(u_char channel)
{
    /*sceSdSetParam(iop_adpcm[channel].core | iop_adpcm[channel].vl | SD_VP_VOLL, iop_adpcm[channel].vol_ll);
    sceSdSetParam(iop_adpcm[channel].core | iop_adpcm[channel].vl | SD_VP_VOLR, iop_adpcm[channel].vol_lr);
    sceSdSetParam(iop_adpcm[channel].core | iop_adpcm[channel].vr | SD_VP_VOLL, iop_adpcm[channel].vol_rl);
    sceSdSetParam(iop_adpcm[channel].core | iop_adpcm[channel].vr | SD_VP_VOLR, iop_adpcm[channel].vol_rr);*/
}

void IaSetRegPitch(u_char channel)
{
    /*sceSdSetParam(iop_adpcm[channel].core | iop_adpcm[channel].vl | SD_VP_PITCH, iop_adpcm[channel].pitch);
    sceSdSetParam(iop_adpcm[channel].core | iop_adpcm[channel].vr | SD_VP_PITCH, iop_adpcm[channel].pitch);*/
}

void IaSetRegKon(u_char channel)
{
    // What is even going on here? these seems completely wrong
    if (channel) {
        sceSdSetSwitch(iop_adpcm[channel].core | SD_S_KON, 0xC00000u);
    } else {
        sceSdSetSwitch(iop_adpcm[channel].core | SD_S_KON, 3);
    }
}

void IaSetRegKoff(u_char channel)
{
    // What is even going on here? these seems completely wrong
    if (channel) {
        sceSdSetSwitch(iop_adpcm[channel].core | SD_S_KOFF, 0xC00000u);
    } else {
        sceSdSetSwitch(iop_adpcm[channel].core | SD_S_KOFF, 3);
    }
}

void IaSetWrkVolPanPitch(u_char channel, u_short pan, u_short master_vol, u_short pitch)
{
    ADPCM_POS_CALC pcalc;

    pcalc.pan = pan;
    pcalc.master_vol = master_vol;
    GetPosCalc(&pcalc);
    iop_adpcm[channel].pan = pcalc.pan;
    iop_adpcm[channel].vol = pcalc.master_vol;
    iop_adpcm[channel].vol_ll = pcalc.ll;
    iop_adpcm[channel].vol_lr = pcalc.lr;
    iop_adpcm[channel].vol_rl = pcalc.rl;
    iop_adpcm[channel].vol_rr = pcalc.rr;
    iop_adpcm[channel].pitch = pitch;
}

void IaSetWrkFadeParam(u_char channel, int fade_flm, u_short target_vol)
{
    iop_adpcm[channel].fade_flm = fade_flm;
    iop_adpcm[channel].target_vol = target_vol;
}

void IaSetWrkFadeMode(u_char channel, u_char mode, u_short target_vol, int fade_flm)
{
    iop_adpcm[channel].count = 0;
    iop_adpcm[channel].fade_mode = mode;
    iop_adpcm[channel].fade_flm = fade_flm;
    iop_adpcm[channel].target_vol = target_vol;
}

void IaSetWrkFadeInit(u_char channel)
{
    iop_adpcm[channel].count = 0;
    iop_adpcm[channel].fade_mode = ADPCM_FADE_NO;
    iop_adpcm[channel].fade_flm = 0;
    iop_adpcm[channel].target_vol = iop_adpcm[channel].vol;
}

void GetPosCalc(ADPCM_POS_CALC* calcp)
{
    if (!iop_mv.mono) {
        if (calcp->pan < 0x280u) {
            calcp->ll = calcp->master_vol;
            calcp->lr = 0;
            calcp->rl = ((640 - calcp->pan) * calcp->master_vol) / 640;
            calcp->rr = calcp->pan * calcp->master_vol / 640;
        } else {
            calcp->ll = ((1279 - calcp->pan) * calcp->master_vol) / 640;
            calcp->lr = ((calcp->pan - 640) * calcp->master_vol) / 640;
            calcp->rl = 0;
            calcp->rr = calcp->master_vol;
        }
    } else {
        calcp->ll = calcp->master_vol >> 1;
        calcp->lr = calcp->master_vol >> 1;
        calcp->rl = calcp->master_vol >> 1;
        calcp->rr = calcp->master_vol >> 1;
    }
}

void IaSetSteMono()
{
    IaSetWrkVolPanPitch(0, iop_adpcm[0].pan, iop_adpcm[0].vol, iop_adpcm[0].pitch);
    IaSetRegVol(0);
}

static void IaSetStopBlock(u_char channel)
{
    u_char sb_tbl[64];

    memset(sb_tbl, 0, sizeof(sb_tbl));
    sb_tbl[1] = 7;
    sb_tbl[17] = 7;
    sb_tbl[33] = 7;
    sb_tbl[49] = 7;
    while (sceSdVoiceTrans(channel, 0, sb_tbl, (u_int *)snd_buf_top[2 * channel + 26], 0x40u) < 0);
    //sceSdVoiceTransStatus(channel, 1);
}

void IaSetMasterVol(u_short mvol)
{
    float gain = ((float)(mvol & 0x3FFF) / 0x3FFF) * 0.5f;
    SDL_SetAudioStreamGain(iop_adpcm[0].stream, gain);
}