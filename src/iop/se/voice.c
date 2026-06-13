#include "voice.h"
#include "iop/iopmain.h"
#include "mikupan/mikupan_audio.h"
#include "mikupan/mikupan_file_c.h"
#include "mikupan/mikupan_logging_c.h"
#include "sce/libsd.h"
#include <SDL3/SDL_mutex.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

bool loopEnd;
bool loopRepeat;
VOICE voices[VOICE_NUM];
static SDL_Mutex *voice_mutex;

#define VOICE_BUFFER_BYTES (0x15160 * 10)
#define VOICE_BUFFER_SAMPLES (VOICE_BUFFER_BYTES / (int)sizeof(s16))
#define VOICE_BUFFER_STEREO_FRAMES (VOICE_BUFFER_SAMPLES / 2)
#define ADPCM_BLOCK_WORDS 8
#define ADPCM_BLOCK_SAMPLES 28
/* Keep roughly this much PCM queued per voice. Decoding is paced against the
   SDL stream so a voice's playback position (nax) advances in real time —
   the ADPCM music streamer depends on that for its IRQA-paced uploads. */
#define VOICE_TARGET_QUEUE_BYTES 8192
/* One SPU buffer half (128 blocks) per fill keeps at most one IRQA crossing
   per call, so the streamer's IRQ -> upload -> re-arm handshake stays ordered. */
#define VOICE_MAX_BLOCKS_PER_FILL 128

static void LockVoices(void)
{
    if (voice_mutex != NULL)
    {
        SDL_LockMutex(voice_mutex);
    }
}

static void UnlockVoices(void)
{
    if (voice_mutex != NULL)
    {
        SDL_UnlockMutex(voice_mutex);
    }
}

void VoicesInit()
{
    if (voice_mutex == NULL)
    {
        voice_mutex = SDL_CreateMutex();
        if (voice_mutex == NULL)
        {
            info_log("Failed to create voice mutex: %s", SDL_GetError());
        }
    }

    // SPU2 has 24 voices per core.
    for (int i = 0; i < VOICE_NUM; i++)
    {
        VOICE *v = &voices[i];

        voices[i].vNo = i;
        voices[i].size = 0;
        voices[i].isPlaying = false;
        voices[i].buffer = malloc(VOICE_BUFFER_BYTES);
        voices[i].header = 0;
        voices[i].ssa = 0;
        voices[i].lsa = 0;
        voices[i].nax = 0;
        voices[i].histL[0] = 0;
        voices[i].histL[1] = 0;
        voices[i].histR[0] = 0;
        voices[i].histR[1] = 0;
        voices[i].volL = 0;
        voices[i].volR = 0;
        voices[i].pitch = 0x1000;
        voices[i].adsr1 = 0;
        voices[i].adsr2 = 0;
        voices[i].frequency_ratio = 0.0f;
        voices[i].stream = NULL;
        if (v->buffer != NULL)
        {
            memset(v->buffer, 0, VOICE_BUFFER_BYTES);
        }
    }
    memset(iop_stat.sev_stat, 0, sizeof(iop_stat.sev_stat));
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

static void MixStereoSamples(int sampleCount, s16 *samples, const VOICE *v)
{
    const s32 volume_l =
        (s32)(((int64_t)mVolL * (int64_t)v->volL) / INT16_MAX);
    const s32 volume_r =
        (s32)(((int64_t)mVolR * (int64_t)v->volR) / INT16_MAX);

    for (int i = sampleCount - 1; i >= 0; i--)
    {
        s16 sample = samples[i];
        samples[i * 2] = ApplyVolume(sample, volume_l);
        samples[i * 2 + 1] = ApplyVolume(sample, volume_r);
    }
}

static bool EnsureVoiceStream(VOICE *v)
{
    if (v->stream != NULL)
    {
        return true;
    }

    if (audio_dev == 0)
    {
        return false;
    }

    SDL_AudioSpec spec;
    SDL_zero(spec);
    spec.channels = 2;
    spec.format = SDL_AUDIO_S16;
    spec.freq = 48000;

    v->stream = SDL_CreateAudioStream(&spec, NULL);
    if (v->stream == NULL)
    {
        info_log("Failed to create audio stream: %s", SDL_GetError());
        return false;
    }

    if (!SDL_BindAudioStream(audio_dev, v->stream))
    {
        info_log("Failed to bind audio stream: %s", SDL_GetError());
        SDL_DestroyAudioStream(v->stream);
        v->stream = NULL;
        return false;
    }

    v->frequency_ratio = 0.0f;
    return true;
}

static void UpdateVoiceFrequencyRatio(VOICE *v)
{
    float ratio = v->pitch / (float)0x1000;

    if (ratio <= 0.0f)
    {
        ratio = 1.0f;
    }

    if (v->frequency_ratio == ratio)
    {
        return;
    }

    if (!SDL_SetAudioStreamFrequencyRatio(v->stream, ratio))
    {
        info_log("Failed to set audio stream frequency ratio: %s", SDL_GetError());
        return;
    }

    v->frequency_ratio = ratio;
}

static void StopVoicePlayback(int vNo)
{
    VOICE *v = &voices[vNo];

    v->isPlaying = false;
    if (vNo >= 24 && vNo < 48)
    {
        iop_stat.sev_stat[vNo - 24].status = VOICE_FREE;
    }
}

static void FillStereo(int vNo)
{
    s16 *src;

    VOICE *v = &voices[vNo];
    if (v->stream == NULL || v->buffer == NULL)
    {
        StopVoicePlayback(vNo);
        return;
    }

    s16 *out = v->buffer;

    int sampleCount = 0;
    int blockCount = 0;

    while (v->isPlaying && blockCount < VOICE_MAX_BLOCKS_PER_FILL &&
           SDL_GetAudioStreamQueued(v->stream)
                   + sampleCount * 2 * (int)sizeof(s16)
               < VOICE_TARGET_QUEUE_BYTES)
    {
        if (sampleCount + ADPCM_BLOCK_SAMPLES > VOICE_BUFFER_STEREO_FRAMES)
        {
            info_log("Voice %d decode exceeded host buffer", vNo);
            StopVoicePlayback(vNo);
            break;
        }

        if (v->nax + ADPCM_BLOCK_WORDS > sizeof(spuRam) / sizeof(spuRam[0]))
        {
            info_log("Voice %d decode exceeded SPU RAM", vNo);
            StopVoicePlayback(vNo);
            break;
        }

        src = (s16 *) &spuRam[v->nax];

        MikuPan_DecodeAdpcmBlock(out, src, v->histL, v->histR);
        out += ADPCM_BLOCK_SAMPLES;
        src += ADPCM_BLOCK_WORDS;
        sampleCount += ADPCM_BLOCK_SAMPLES;
        blockCount++;
        v->nax += ADPCM_BLOCK_WORDS;

        loopEnd = (v->header & (1 << 8)) != 0;
        loopRepeat = (v->header & (1 << 9)) != 0;

        if (loopEnd)
        {
            v->nax = v->lsa;

            if (!loopRepeat)
            {
                StopVoicePlayback(vNo);
            }
        }

        FillAdpcmHeader(vNo);
        MikuPan_SdVoiceReachedAddress(vNo, v->nax);
    }

    if (sampleCount > 0)
    {
        MixStereoSamples(sampleCount, v->buffer, v);
        UpdateVoiceFrequencyRatio(v);
        SDL_PutAudioStreamData(v->stream, v->buffer,
                               sampleCount * 2 * (int)sizeof(s16));
    }
}

static void SaveDebugBuffer()
{
    int size = sizeof(spuRam) / sizeof(spuRam[0]);
    MikuPan_WriteFile("AudioBuffer.bin", spuRam, size);
}

void VoiceRun()
{
    LockVoices();
    for (int i = 0; i < VOICE_NUM; i++)
    {
        if (voices[i].isPlaying)
        {
            FillStereo(i);
        }
    }
    UnlockVoices();
}

static void KeyOnUnlocked(int vNo)
{
    if (vNo < 0 || vNo >= VOICE_NUM)
    {
        return;
    }

    VOICE *v = &voices[vNo];
    if (!EnsureVoiceStream(v))
    {
        StopVoicePlayback(vNo);
        return;
    }

    if (vNo >= 24 && vNo < 48)
    {
        iop_stat.sev_stat[vNo - 24].status = VOICE_USE;
    }

    v->histL[0] = 0;
    v->histL[1] = 0;
    v->histR[0] = 0;
    v->histR[1] = 0;
    v->nax = v->ssa;
    v->frequency_ratio = 0.0f;
    SDL_ClearAudioStream(v->stream);
    if (v->buffer != NULL)
    {
        memset(v->buffer, 0, VOICE_BUFFER_BYTES);
    }

    v->isPlaying = true;
    FillAdpcmHeader(vNo);
}

void Key_On(int vNo)
{
    LockVoices();
    KeyOnUnlocked(vNo);
    UnlockVoices();
}

static void KeyOffUnlocked(int vNo)
{
    if (vNo < 0 || vNo >= VOICE_NUM)
    {
        return;
    }

    info_log("Key_Off vNo=%d", vNo);
    VOICE *v = &voices[vNo];
    StopVoicePlayback(vNo);

    if (v->stream)
    {
        SDL_ClearAudioStream(v->stream);
    }

    if (v->buffer != NULL)
    {
        memset(v->buffer, 0, VOICE_BUFFER_BYTES);
    }
}

void Key_Off(int vNo)
{
    LockVoices();
    KeyOffUnlocked(vNo);
    UnlockVoices();
}

void VoiceSetKeySwitch(int core, u_int value, int key_on)
{
    LockVoices();
    for (int voice = 0; voice < 24; voice++)
    {
        if ((value & (1u << voice)) == 0)
        {
            continue;
        }

        int voice_index = core * 24 + voice;
        if (voice_index >= VOICE_NUM)
        {
            continue;
        }

        if (key_on)
        {
            KeyOnUnlocked(voice_index);
        }
        else
        {
            KeyOffUnlocked(voice_index);
        }
    }
    UnlockVoices();
}

static void CloseVoiceUnlocked(int vNo)
{
    if (vNo < 0 || vNo >= VOICE_NUM)
    {
        return;
    }

    VOICE *v = &voices[vNo];

    v->isPlaying = false;
    v->size = 0;

    if (v->stream != NULL)
    {
        SDL_DestroyAudioStream(v->stream);
        v->stream = NULL;
    }
    free(v->buffer);
    v->buffer = NULL;
    if (vNo >= 24 && vNo < 48)
    {
        iop_stat.sev_stat[vNo - 24].status = VOICE_FREE;
    }
}

void CloseVoice(int vNo)
{
    LockVoices();
    CloseVoiceUnlocked(vNo);
    UnlockVoices();
}

static void CloseVoicesUnlocked(void)
{
    for (int i = 0; i < VOICE_NUM; i++)
    {
        if (i < 24)
        {
            iop_stat.sev_stat[i].status = VOICE_FREE;
        }
        voices[i].isPlaying = false;
        voices[i].size = 0;
        if (voices[i].stream != NULL)
        {
            SDL_DestroyAudioStream(voices[i].stream);
            voices[i].stream = NULL;
        }
        free(voices[i].buffer);
        voices[i].buffer = NULL;
    }
    ClearAudioBuffer();
}

void CloseVoices()
{
    LockVoices();
    CloseVoicesUnlocked();
    UnlockVoices();
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
