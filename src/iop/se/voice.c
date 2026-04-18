#include "voice.h"
#include "iop/iopmain.h"
#include "iop/se/iopse.h"
#include "mikupan/mikupan_audio.h"
#include "mikupan/mikupan_file_c.h"
#include "sce/libsd.h"
#include "typedefs.h"
#include "mikupan/mikupan_logging_c.h"
#include <stdlib.h>

bool loopEnd;
bool loopRepeat;
VOICE voices[24];

void VoicesInit()
{
    // The ps2 contains 24 seperate voices
    for (int i = 0; i < 24; i++)
    {
        voices[i].vNo = i;
        voices[i].size = 0;
        voices[i].buffer = malloc(0x15160 * 10);
        memset(&voices[i].header, 0, sizeof(AudioHeader));
    }
    memset(iop_stat.sev_stat, 0, 24);
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

static s16* MixSamples(int sampleCount, s16 *samples, VOICE v)
{
    s16 *buffer = samples;
    s16 volume = (s32) v.volL;

    volume = volume * v.mVolL / 16383;

    for (int i = 0; i < sampleCount; i++)
    {
        s16 sample = samples[i];
        s16 mixed = ApplyVolume(sample, volume);
        buffer[i] = mixed;
    }
    return buffer;
}

static void FillMono(int vNo)
{
    s16 *src;

    VOICE *v = &voices[vNo];
    s16 *out = v->buffer;

    int sampleCount = 0;

    while (v->isPlaying)
    {
        src = (s16 *) &spuRam[v->nax];

        MikuPan_DecodeAdpcmBlock(out, src, v->histL, v->histR);
        out += 28;
        src += 8;
        sampleCount += 28;
        v->nax += 8;

        if ((v->nax & 0x7) == 0)
        {
            loopEnd = (v->header & (1 << 8)) != 0;
            loopRepeat = (v->header & (1 << 9)) != 0;

            FillAdpcmHeader(vNo);
            if (loopEnd)
            {
                int byteCount = (out - v->buffer) * sizeof(s16);
                v->nax = v->lsa;

                out = v->buffer;

                v->buffer = MixSamples(sampleCount, v->buffer, *v);

                SDL_SetAudioStreamFrequencyRatio(v->stream, v->pitch / (float) 0x1000);

                if (SDL_GetAudioStreamQueued(v->stream) < 4096)
                {
                    SDL_PutAudioStreamData(v->stream, v->buffer, byteCount);
                }
                sampleCount = 0;

                if (!loopRepeat)
                {
                    iop_stat.sev_stat[vNo].status = VOICE_FREE;
                    v->isPlaying = false;
                }
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

void VoiceRun()
{
    for(int i = 0; i < VOICE_NUM; i++)
    {
        if (voices[i].isPlaying)
        {
            FillMono(i);
        }
    }
}

void Key_On(int vNo)
{
    SDL_AudioSpec spec;
    spec.channels = 1;
    spec.format = SDL_AUDIO_S16;
    spec.freq = 48000;

    iop_stat.sev_stat[vNo].status = VOICE_USE;
    VOICE *v = &voices[vNo];

    v->histL[0] = 0;
    v->histL[1] = 0;
    v->nax = v->ssa;

    v->stream = SDL_CreateAudioStream(&spec, NULL);
    SDL_BindAudioStream(audio_dev, v->stream);

    v->isPlaying = true;
    FillAdpcmHeader(vNo);
    if (SDL_AudioStreamDevicePaused(v->stream))
    {
        SDL_ResumeAudioStreamDevice(v->stream);
    }
}

void Key_Off(int vNo)
{
    VOICE *v = &voices[vNo];

    if (v->stream)
    {
        SDL_ClearAudioStream(v->stream);
        SDL_UnbindAudioStream(v->stream);
        SDL_DestroyAudioStream(v->stream);
        v->stream = NULL;
    }

    memset(v->buffer, 0, 0x15160 * 10);
}

void CloseVoice(int vNo)
{
    voices[vNo].size = 0;
    free(voices[vNo].buffer);

    SDL_UnbindAudioStream(voices[vNo].stream);
    SDL_DestroyAudioStream(voices[vNo].stream);
    voices[vNo].stream = NULL;
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