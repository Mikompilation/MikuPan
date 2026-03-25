#include "voice.h"
#include "iop/iopmain.h"
#include "iop/se/iopse.h"
#include "mikupan/mikupan_audio.h"
#include "mikupan/mikupan_file_c.h"
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
        voices[i].size = 0;
        memset(&voices[i].header, 0, sizeof(AudioHeader));
    }

    memset (iop_stat.sev_stat, 0, 24);
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

static void MixSamples(int sampleCount, s16 *samples, VOICE v)
{
    for (int i = 0; i < sampleCount; i++)
    {
        s16 sample = samples[i];
        sample = ApplyVolume(sample, v.adsr1);
        sample = ApplyVolume(sample, v.volL);
        samples[i] = sample;
    }
}

static void FillVoiceBuffer(int vNo)
{
    s32 histL[2] = {0}, histR[2] = {0};

    s16 *src;

    voices[vNo].buffer = malloc(0x15160 * 10);
    s16 *out = voices[vNo].buffer;

    int sampleCount = 0;

    bool loopEnd;

    while (true)
    {
        src = (s16 *) &spuRam[voices[vNo].nax];

        MikuPan_DecodeAdpcmBlock(out, src, histL, histR);
        out += 28;
        src += 8;
        sampleCount += 28;
        voices[vNo].nax += 8;

        if ((voices[vNo].nax & 0x7) == 0)
        {
            loopEnd = (voices[vNo].header & (1 << 8)) != 0;
            FillAdpcmHeader(vNo);
            if (loopEnd)
            {
                int byteCount = (out - voices[vNo].buffer) * sizeof(s16);
                voices[vNo].nax = voices[vNo].lsa;
                MixSamples(sampleCount, voices[vNo].buffer, voices[vNo]);

                SDL_SetAudioStreamFrequencyRatio(
                    voices[vNo].stream, voices[vNo].pitch / (float) 0x1000);
                SDL_PutAudioStreamData(voices[vNo].stream, voices[vNo].buffer,
                                       byteCount);
                break;
            }
        }
    }
}

static void SaveDebugBuffer()
{
    int size = sizeof(spuRam) / sizeof(spuRam[0]);
    MikuPan_WriteFile("AudioBuffer.bin", spuRam, size);
}

void Key_On(int vNo)
{
    iop_stat.sev_stat[vNo].status = VOICE_USE;

    SDL_AudioSpec spec;
    spec.channels = 1;
    spec.format = SDL_AUDIO_S16;
    spec.freq = 48000;

    voices[vNo].stream = SDL_CreateAudioStream(&spec, NULL);
    SDL_BindAudioStream(audio_dev, voices[vNo].stream);
    voices[vNo].nax = voices[vNo].ssa;
    //SaveDebugBuffer();
    FillAdpcmHeader(vNo);
    FillVoiceBuffer(vNo);
    SDL_ResumeAudioStreamDevice(voices[vNo].stream);
}

void Key_Off(int vNo)
{
    VOICE v = voices[vNo];
    iop_stat.sev_stat[vNo].status = VOICE_FREE;
    SDL_PauseAudioStreamDevice(v.stream);
    SDL_ClearAudioStream(v.stream);
}

void CloseVoice(int vNo)
{
    voices[vNo].size = 0;
    free(voices[vNo].buffer);
    SDL_DestroyAudioStream(voices[vNo].stream);
    iop_stat.sev_stat[vNo].status = VOICE_FREE;
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
    u16 *bytes = (u16 *) spuRam;
    v->header = bytes[v->nax & ~0x7];

    if (v->header & (1 << 10))
    {
        v->lsa = v->ssa & ~0x7;
    }
}