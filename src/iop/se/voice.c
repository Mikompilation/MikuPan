#include "voice.h"
#include "iop/iopmain.h"
#include "iop/se/iopse.h"
#include "mikupan/mikupan_audio.h"
#include "sce/libsd.h"
#include "typedefs.h"
#include <stdlib.h>

VOICE voices[24];

void VoicesInit()
{

    // The ps2 contains 24 seperate voices
    for (int i = 0; i < 24; i++)
    {
        voices[i].vNo = i;
        iop_stat.sev_stat[i].status = VOICE_FREE;
        voices[i].size = 0;
        memset(&voices[i].header, 0, sizeof(AudioHeader));
    }
}

VOICE *GetFreeVoice()
{
    for (int i = 0; i < 24; i++)
    {
        if (iop_stat.sev_stat[i].status == VOICE_FREE)
        {
            return &voices[i];
        }
    }
    return NULL;
}

void FillVoice(VOICE voice)
{
    if (voices == NULL)
    {
        VoicesInit();
    }

    VOICE *v = GetFreeVoice();
    iop_stat.sev_stat[v->vNo].status = VOICE_RESERVED;
    voices[v->vNo] = voice;
}

static void FillVoiceBuffer(int vNo)
{
    int chunks = 4096 / 0x800 / 1;
    int samples = chunks * 128 * 28;
    int bytes = samples * sizeof(s16);
    s32 histL[2] = {0}, histR[2] = {0};

    s16 *src = (s16 *) malloc(bytes);

    voices[vNo].buffer = malloc(bytes);
    s16 *out = voices[vNo].buffer;

    for (int i = 0; i < chunks; i++)
    //while (true)
    {
        memcpy(src, &audioBuffer[voices[vNo].nax], bytes);

        for (int j = 0; j < 128; j++)
        {
            MikuPan_DecodeAdpcmBlock(out, src, histL, histR);
            out += 28;
            src += 8;
        }
        //voices[vNo].nax++;
        FillAdpcmHeader(vNo);
        if ((voices[vNo].nax & 0x7) == 0)
        {
            u_char isEnd = LOOPEND(voices[vNo].header);

            if (isEnd != 0)
            {
                break;
            }
        }
    }
    SDL_PutAudioStreamData(voices[vNo].stream, voices[vNo].buffer, samples);
}

void Key_On(int vNo, int size)
{
    iop_stat.sev_stat[vNo].status = VOICE_USE;

    SDL_AudioSpec spec;
    spec.channels = 1;
    spec.format = SDL_AUDIO_S16;
    spec.freq = 48000;

    voices[vNo].size = size;
    voices[vNo].stream = SDL_CreateAudioStream(&spec, NULL);
    SDL_BindAudioStream(audio_dev, voices[vNo].stream);
    voices[vNo].nax = voices[vNo].ssa;
    FillAdpcmHeader(vNo);
    //FillVoiceBuffer(vNo);
    SDL_ResumeAudioStreamDevice(voices[vNo].stream);
}

void Key_Off(int vNo)
{
    VOICE v = voices[vNo];
    iop_stat.sev_stat[vNo].status = VOICE_FREE;
    SDL_PauseAudioStreamDevice(v.stream);
    SDL_ClearAudioStream(v.stream);
}

void CloseVoices()
{
    for (int i = 0; i < 24; i++)
    {
        iop_stat.sev_stat[i].status = VOICE_FREE;
        voices[i].size = 0;
        ClearAudioBuffer();
        free(voices[i].buffer);
        SDL_DestroyAudioStream(voices[i].stream);
    }
}

void FillAdpcmHeader(int vNo)
{
    VOICE *v = &voices[vNo];
    s16 *bytes = audioBuffer;
    v->header = bytes[v->nax & ~0x7];
    //memcpy(&v->header, &bytes[v->nax & ~0x7], sizeof(u16));

    u_char loop = LOOP(v->header);
    u_char loopStart = LOOPSTART(v->header);
    u_char loopEnd = LOOPEND(v->header);

    if (LOOPSTART(v->header))
    {
        v->lsa = v->ssa & ~0x7;
    }
}

void SetPitch(int vNo, u_short val)
{
    SDL_SetAudioStreamFrequencyRatio(voices[vNo].stream, val);
}