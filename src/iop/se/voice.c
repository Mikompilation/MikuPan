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
#define ADSR_LEVEL_BITS 15
#define ADSR_LEVEL_MAX ((1 << ADSR_LEVEL_BITS) - 1)
#define ADSR_FP_SHIFT 16
#define ADSR_LEVEL_MAX_FP ((s32)((int64_t) ADSR_LEVEL_MAX << ADSR_FP_SHIFT))
#define ADSR_MIN_STEP_FP (1 << 8)

static s32 GetVoiceLeftVolume(const VOICE *v);
static s32 GetVoiceRightVolume(const VOICE *v);
static s32 GetVoiceAdsrLevel(VOICE *v);
static s32 ApplyEnvelopeToVolume(s32 volume, s32 envelope);

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
        voices[i].endPending = false;
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
        voices[i].adsr_level = 0;
        voices[i].adsr_phase = VOICE_ADSR_OFF;
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

static void MixStereoSamples(int sampleCount, s16 *samples, VOICE *v)
{
    const s32 base_volume_l = GetVoiceLeftVolume(v);
    const s32 base_volume_r = GetVoiceRightVolume(v);
    s16 *mono_samples = samples + sampleCount;

    memmove(mono_samples, samples, sampleCount * (int)sizeof(s16));

    for (int i = 0; i < sampleCount; i++)
    {
        const s32 envelope = GetVoiceAdsrLevel(v);
        const s32 volume_l = ApplyEnvelopeToVolume(base_volume_l, envelope);
        const s32 volume_r = ApplyEnvelopeToVolume(base_volume_r, envelope);
        s16 sample = mono_samples[i];

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

    v->stream = SDL_CreateAudioStream(&spec, &spec);
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
    v->endPending = false;
    if (vNo >= 24 && vNo < 48)
    {
        iop_stat.sev_stat[vNo - 24].status = VOICE_FREE;
    }
}

static void FinishVoicePlayback(int vNo)
{
    VOICE *v = &voices[vNo];

    v->isPlaying = false;
    v->endPending = false;
    if (vNo >= 24 && vNo < 48)
    {
        iop_stat.sev_stat[vNo - 24].status = VOICE_FREE;
    }
}

static void BeginVoiceEnd(int vNo)
{
    VOICE *v = &voices[vNo];

    if (!v->isPlaying && v->endPending)
    {
        return;
    }

    v->isPlaying = false;

    MikuPan_SdSetVoiceEnd(vNo);

    if (v->stream != NULL)
    {
        SDL_FlushAudioStream(v->stream);
    }

    v->endPending = true;
}

static void FinishVoiceEndIfDrained(int vNo)
{
    VOICE *v = &voices[vNo];

    if (!v->endPending)
    {
        return;
    }

    if (v->stream == NULL || SDL_GetAudioStreamQueued(v->stream) <= 0)
    {
        FinishVoicePlayback(vNo);
    }
}

static bool VoiceAdsrConfigured(const VOICE *v)
{
    return v->adsr1 != 0 || v->adsr2 != 0;
}

/* SPU2 ADSR timing is stepped by hardware counters; this approximates the
   register bit layout and curve shape at the host sample rate. */
static s32 ClampAdsrLevel(int64_t level)
{
    if (level <= 0)
    {
        return 0;
    }

    if (level >= ADSR_LEVEL_MAX_FP)
    {
        return ADSR_LEVEL_MAX_FP;
    }

    return (s32)level;
}

static s32 AdsrRateStep(int rate, int max_rate, int fastest_samples)
{
    if (rate < 0)
    {
        rate = 0;
    }
    else if (rate > max_rate)
    {
        rate = max_rate;
    }

    const int range = max_rate + 1;
    const int fastness = max_rate - rate + 1;

    int64_t step = ((int64_t)fastness * fastness * ADSR_LEVEL_MAX_FP)
                   / ((int64_t)range * range * fastest_samples);

    if (step < ADSR_MIN_STEP_FP)
    {
        step = ADSR_MIN_STEP_FP;
    }

    return (s32)step;
}

static s32 AdsrCurvedStep(s32 base_step, s32 level, bool increasing)
{
    const s32 curve = increasing ? ADSR_LEVEL_MAX_FP - level : level;
    int64_t step = ((int64_t)base_step * curve) / ADSR_LEVEL_MAX_FP;

    if (step < ADSR_MIN_STEP_FP)
    {
        step = ADSR_MIN_STEP_FP;
    }

    return (s32)step;
}

static int AdsrAttackRate(const VOICE *v)
{
    return (v->adsr1 >> 8) & 0x7f;
}

static bool AdsrAttackExponential(const VOICE *v)
{
    return (v->adsr1 & 0x8000) != 0;
}

static int AdsrDecayRate(const VOICE *v)
{
    return (v->adsr1 >> 4) & 0x0f;
}

static s32 AdsrSustainLevel(const VOICE *v)
{
    const int level = v->adsr1 & 0x0f;
    return (s32) (((int64_t)(level + 1) * ADSR_LEVEL_MAX_FP) / 16);
}

static int AdsrReleaseRate(const VOICE *v)
{
    return v->adsr2 & 0x1f;
}

static bool AdsrReleaseExponential(const VOICE *v)
{
    return (v->adsr2 & 0x20) != 0;
}

static int AdsrSustainRate(const VOICE *v)
{
    return (v->adsr2 >> 6) & 0x7f;
}

static bool AdsrSustainDecrease(const VOICE *v)
{
    return (v->adsr2 & 0x2000) != 0;
}

static int AdsrSustainMode(const VOICE *v)
{
    return (v->adsr2 >> 14) & 0x03;
}

static void AdvanceVoiceAdsr(VOICE *v)
{
    s32 step;
    const s32 sustain_level = AdsrSustainLevel(v);

    if (!VoiceAdsrConfigured(v))
    {
        v->adsr_level = ADSR_LEVEL_MAX_FP;
        v->adsr_phase = VOICE_ADSR_SUSTAIN;
        return;
    }

    switch (v->adsr_phase)
    {
    case VOICE_ADSR_ATTACK:
        step = AdsrRateStep(AdsrAttackRate(v), 0x7f, 32);
        if (AdsrAttackExponential(v))
        {
            step = AdsrCurvedStep(step, v->adsr_level, true);
        }

        v->adsr_level = ClampAdsrLevel((int64_t)v->adsr_level + step);
        if (v->adsr_level >= ADSR_LEVEL_MAX_FP)
        {
            v->adsr_level = ADSR_LEVEL_MAX_FP;
            v->adsr_phase = VOICE_ADSR_DECAY;
        }
        break;

    case VOICE_ADSR_DECAY:
        if (v->adsr_level <= sustain_level)
        {
            v->adsr_level = sustain_level;
            v->adsr_phase = VOICE_ADSR_SUSTAIN;
            break;
        }

        step = AdsrRateStep(AdsrDecayRate(v), 0x0f, 64);
        step = AdsrCurvedStep(step, v->adsr_level, false);
        if (v->adsr_level - sustain_level <= step)
        {
            v->adsr_level = sustain_level;
            v->adsr_phase = VOICE_ADSR_SUSTAIN;
        }
        else
        {
            v->adsr_level -= step;
        }
        break;

    case VOICE_ADSR_SUSTAIN:
        if (AdsrSustainMode(v) == 0)
        {
            break;
        }

        step = AdsrRateStep(AdsrSustainRate(v), 0x7f, 64);
        if (AdsrSustainDecrease(v))
        {
            if (AdsrSustainMode(v) >= 2)
            {
                step = AdsrCurvedStep(step, v->adsr_level, false);
            }
            v->adsr_level = ClampAdsrLevel((int64_t)v->adsr_level - step);
        }
        else
        {
            if (AdsrSustainMode(v) >= 2)
            {
                step = AdsrCurvedStep(step, v->adsr_level, true);
            }
            v->adsr_level = ClampAdsrLevel((int64_t)v->adsr_level + step);
        }

        if (v->adsr_level == 0)
        {
            v->adsr_phase = VOICE_ADSR_OFF;
            BeginVoiceEnd(v->vNo);
        }
        break;

    case VOICE_ADSR_RELEASE:
        step = AdsrRateStep(AdsrReleaseRate(v), 0x1f, 64);
        if (AdsrReleaseExponential(v))
        {
            step = AdsrCurvedStep(step, v->adsr_level, false);
        }

        if (v->adsr_level <= step)
        {
            v->adsr_level = 0;
            v->adsr_phase = VOICE_ADSR_OFF;
            BeginVoiceEnd(v->vNo);
        }
        else
        {
            v->adsr_level -= step;
        }
        break;

    case VOICE_ADSR_OFF:
    default:
        v->adsr_level = 0;
        break;
    }
}

static s32 GetVoiceAdsrLevel(VOICE *v)
{
    s32 level;

    if (!VoiceAdsrConfigured(v))
    {
        v->adsr_level = ADSR_LEVEL_MAX_FP;
        v->adsr_phase = VOICE_ADSR_SUSTAIN;
        return ADSR_LEVEL_MAX;
    }

    if (v->adsr_phase == VOICE_ADSR_OFF)
    {
        v->adsr_level = 0;
        return 0;
    }

    v->adsr_level = ClampAdsrLevel(v->adsr_level);
    level = v->adsr_level >> ADSR_FP_SHIFT;

    AdvanceVoiceAdsr(v);

    v->adsr_level = ClampAdsrLevel(v->adsr_level);

    return level;
}

static s32 ApplyEnvelopeToVolume(s32 volume, s32 envelope)
{
    return (s32)(((int64_t)volume * envelope) / ADSR_LEVEL_MAX);
}

static int StereoPairRightVoice(int vNo)
{
    if (vNo == 0)
    {
        return 1;
    }

    if (vNo == 46)
    {
        return 47;
    }

    return -1;
}

static int StereoPairLeftVoice(int vNo)
{
    if (vNo == 1)
    {
        return 0;
    }

    if (vNo == 47)
    {
        return 46;
    }

    return -1;
}

static s32 GetVoiceLeftVolume(const VOICE *v)
{
    return (s32)(((int64_t)mVolL * (int64_t)v->volL) / INT16_MAX);
}

static s32 GetVoiceRightVolume(const VOICE *v)
{
    return (s32)(((int64_t)mVolR * (int64_t)v->volR) / INT16_MAX);
}

static s16 ClampS32ToS16(s32 value)
{
    if (value > INT16_MAX)
    {
        return INT16_MAX;
    }

    if (value < INT16_MIN)
    {
        return INT16_MIN;
    }

    return (s16)value;
}

static bool DecodeVoiceBlock(int vNo, s16 *out)
{
    VOICE *v = &voices[vNo];

    if (v->nax + ADPCM_BLOCK_WORDS > sizeof(spuRam) / sizeof(spuRam[0]))
    {
        info_log("Voice %d decode exceeded SPU RAM", vNo);
        StopVoicePlayback(vNo);
        return false;
    }

    s16 *src = (s16 *) &spuRam[v->nax];

    MikuPan_DecodeAdpcmBlock(out, src, v->histL, v->histR);
    v->nax += ADPCM_BLOCK_WORDS;

    loopEnd = (v->header & (1 << 8)) != 0;
    loopRepeat = (v->header & (1 << 9)) != 0;

    if (loopEnd)
    {
        v->nax = v->lsa;

        if (!loopRepeat)
        {
            BeginVoiceEnd(vNo);
        }
    }

    FillAdpcmHeader(vNo);
    MikuPan_SdVoiceReachedAddress(vNo, v->nax);
    return true;
}

static void MixStereoPairSamples(int sampleCount, VOICE *left, VOICE *right)
{
    s16 *left_samples = left->buffer;
    s16 *left_mono_samples = left_samples + sampleCount;
    const s16 *right_samples = right->buffer;
    const s32 left_base_volume_l = GetVoiceLeftVolume(left);
    const s32 left_base_volume_r = GetVoiceRightVolume(left);
    const s32 right_base_volume_l = GetVoiceLeftVolume(right);
    const s32 right_base_volume_r = GetVoiceRightVolume(right);

    memmove(left_mono_samples, left_samples, sampleCount * (int)sizeof(s16));

    for (int i = 0; i < sampleCount; i++)
    {
        const s32 left_envelope = GetVoiceAdsrLevel(left);
        const s32 right_envelope = GetVoiceAdsrLevel(right);
        const s32 left_volume_l =
            ApplyEnvelopeToVolume(left_base_volume_l, left_envelope);
        const s32 left_volume_r =
            ApplyEnvelopeToVolume(left_base_volume_r, left_envelope);
        const s32 right_volume_l =
            ApplyEnvelopeToVolume(right_base_volume_l, right_envelope);
        const s32 right_volume_r =
            ApplyEnvelopeToVolume(right_base_volume_r, right_envelope);
        const s16 left_sample = left_mono_samples[i];
        const s16 right_sample = right_samples[i];
        const s32 out_l = ApplyVolume(left_sample, left_volume_l)
            + ApplyVolume(right_sample, right_volume_l);
        const s32 out_r = ApplyVolume(left_sample, left_volume_r)
            + ApplyVolume(right_sample, right_volume_r);

        left_samples[i * 2] = ClampS32ToS16(out_l);
        left_samples[i * 2 + 1] = ClampS32ToS16(out_r);
    }
}

static void FillStereoPair(int leftNo, int rightNo)
{
    VOICE *left = &voices[leftNo];
    VOICE *right = &voices[rightNo];

    if (left->stream == NULL || left->buffer == NULL || right->buffer == NULL)
    {
        StopVoicePlayback(leftNo);
        StopVoicePlayback(rightNo);
        return;
    }

    int sampleCount = 0;
    int blockCount = 0;

    while (left->isPlaying && right->isPlaying
           && blockCount < VOICE_MAX_BLOCKS_PER_FILL
           && SDL_GetAudioStreamQueued(left->stream)
                   + sampleCount * 2 * (int)sizeof(s16)
               < VOICE_TARGET_QUEUE_BYTES)
    {
        if (sampleCount + ADPCM_BLOCK_SAMPLES > VOICE_BUFFER_STEREO_FRAMES)
        {
            info_log("Voice pair %d/%d decode exceeded host buffer",
                     leftNo, rightNo);
            StopVoicePlayback(leftNo);
            StopVoicePlayback(rightNo);
            break;
        }

        if (!DecodeVoiceBlock(leftNo, left->buffer + sampleCount)
            || !DecodeVoiceBlock(rightNo, right->buffer + sampleCount))
        {
            break;
        }

        sampleCount += ADPCM_BLOCK_SAMPLES;
        blockCount++;
    }

    if (sampleCount > 0)
    {
        MixStereoPairSamples(sampleCount, left, right);
        UpdateVoiceFrequencyRatio(left);
        SDL_PutAudioStreamData(left->stream, left->buffer,
                               sampleCount * 2 * (int)sizeof(s16));
    }
}

static void FillStereo(int vNo)
{
    s16 *src;

    VOICE *v = &voices[vNo];
    const int pair_left = StereoPairLeftVoice(vNo);
    if (pair_left >= 0 && voices[pair_left].isPlaying)
    {
        return;
    }

    const int pair_right = StereoPairRightVoice(vNo);
    if (pair_right >= 0 && voices[pair_right].isPlaying)
    {
        FillStereoPair(vNo, pair_right);
        return;
    }

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
                BeginVoiceEnd(vNo);
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
        FinishVoiceEndIfDrained(i);
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

    v->endPending = false;
    if (vNo >= 24 && vNo < 48)
    {
        iop_stat.sev_stat[vNo - 24].status = VOICE_USE;
    }

    v->histL[0] = 0;
    v->histL[1] = 0;
    v->histR[0] = 0;
    v->histR[1] = 0;
    v->nax = v->ssa;
    v->adsr_level = 0;
    v->adsr_phase = VOICE_ADSR_ATTACK;
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

    if (!v->isPlaying || !VoiceAdsrConfigured(v))
    {
        StopVoicePlayback(vNo);
        v->adsr_level = 0;
        v->adsr_phase = VOICE_ADSR_OFF;
        if (v->stream)
        {
            SDL_ClearAudioStream(v->stream);
        }
        if (v->buffer != NULL)
        {
            memset(v->buffer, 0, VOICE_BUFFER_BYTES);
        }
        return;
    }

    if (v->adsr_level <= 0 || v->adsr_level > ADSR_LEVEL_MAX_FP)
    {
        v->adsr_level = ADSR_LEVEL_MAX_FP;
    }

    v->adsr_phase = VOICE_ADSR_RELEASE;
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
    v->endPending = false;
    v->size = 0;
    v->adsr_level = 0;
    v->adsr_phase = VOICE_ADSR_OFF;

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
        voices[i].endPending = false;
        voices[i].size = 0;
        voices[i].adsr_level = 0;
        voices[i].adsr_phase = VOICE_ADSR_OFF;
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
        v->lsa = v->nax & ~0x7;
    }
}
