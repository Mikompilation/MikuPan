#include "voice.h"
#include "iop/iopmain.h"
#include "mikupan/mikupan_audio.h"
#include "mikupan/mikupan_audio_bus.h"
#include "mikupan/io/mikupan_file_c.h"
#include "mikupan/debug/mikupan_logging_c.h"
#include "sce/libsd.h"
#include <SDL3/SDL_mutex.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

bool loopEnd;
bool loopRepeat;
VOICE voices[VOICE_NUM];
static SDL_Mutex *voice_mutex;
static u_int voice_audio_revision;

#define VOICE_BUFFER_BYTES 8192
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

static s32 GetVoiceLeftVolume(const VOICE *v);
static s32 GetVoiceRightVolume(const VOICE *v);
static s32 GetVoiceAdsrLevel(VOICE *v);
static s32 ApplyEnvelopeToVolume(s32 volume, s32 envelope);
static void ApplyVoiceStreamGain(VOICE *v);
static void RefreshVoiceStreamGains(void);

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

void VoiceLock(void)
{
    LockVoices();
}

void VoiceUnlock(void)
{
    UnlockVoices();
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
        voices[i].buffer = (s16 *)malloc(VOICE_BUFFER_BYTES);
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
        voices[i].resampler = NULL;
        voices[i].resampler_channels = 0;
        voices[i].resampler_flushed = false;
        voices[i].adsr_counter = 0;
        voices[i].stream = NULL;
        if (v->buffer != NULL)
        {
            memset(v->buffer, 0, VOICE_BUFFER_BYTES);
        }
    }
    memset(iop_stat.sev_stat, 0, sizeof(iop_stat.sev_stat));
}

static void ApplyVoiceStreamGain(VOICE *v)
{
    if (v == NULL || v->stream == NULL)
    {
        return;
    }

    if (!SDL_SetAudioStreamGain(v->stream, MikuPan_AudioGetMasterScale()))
    {
        info_log("Failed to set audio stream gain: %s", SDL_GetError());
    }
}

static void RefreshVoiceStreamGains(void)
{
    const u_int revision = MikuPan_AudioGetRevision();

    if (voice_audio_revision == revision)
    {
        return;
    }

    for (int i = 0; i < VOICE_NUM; i++)
    {
        ApplyVoiceStreamGain(&voices[i]);
    }

    voice_audio_revision = revision;
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
    ApplyVoiceStreamGain(v);
    return true;
}


static void StopVoicePlayback(int vNo)
{
    VOICE *v = &voices[vNo];
    v->isPlaying = v->endPending = false;
    v->adsr_level = 0;
    v->adsr_phase = VOICE_ADSR_OFF;
    if (v->stream) SDL_ClearAudioStream(v->stream);
    if (v->resampler) SDL_ClearAudioStream(v->resampler);
    MikuPan_SdSetVoiceEnd(vNo);
    if (vNo >= 24) iop_stat.sev_stat[vNo - 24].status = VOICE_FREE;
}
static void FinishVoicePlayback(int vNo)
{
    voices[vNo].isPlaying = voices[vNo].endPending = false;
    if (vNo >= 24) iop_stat.sev_stat[vNo - 24].status = VOICE_FREE;
}
static void BeginVoiceEnd(int vNo)
{
    VOICE *v = &voices[vNo];
    if (!v->isPlaying) return;
    v->isPlaying = false;
    v->endPending = true;
    MikuPan_SdSetVoiceEnd(vNo);
}
static bool VoiceAdsrConfigured(const VOICE *v) { return v->adsr1 || v->adsr2; }
static void AdvanceVoiceAdsr(VOICE *v)
{
    int shift, step;
    int target = 0;
    bool decreasing = false;
    bool exponential = false;

    switch (v->adsr_phase)
    {
    case VOICE_ADSR_ATTACK:
        shift = (v->adsr1 >> 10) & 31;
        step = 7 - ((v->adsr1 >> 8) & 3);
        exponential = (v->adsr1 & 0x8000) != 0;
        target = ADSR_LEVEL_MAX;
        break;
    case VOICE_ADSR_DECAY:
        shift = (v->adsr1 >> 4) & 15;
        step = -8;
        decreasing = exponential = true;
        target = ((v->adsr1 & 15) + 1) * 2048;
        break;
    case VOICE_ADSR_SUSTAIN:
        shift = (v->adsr2 >> 8) & 31;
        decreasing = (v->adsr2 & 0x4000) != 0;
        step = 7 - ((v->adsr2 >> 6) & 3);
        if (decreasing)
            step = -step - 1;
        exponential = (v->adsr2 & 0x8000) != 0;
        break;
    case VOICE_ADSR_RELEASE:
        shift = v->adsr2 & 31;
        step = -8;
        decreasing = true;
        exponential = (v->adsr2 & 0x20) != 0;
        break;
    case VOICE_ADSR_OFF:
    default:
        v->adsr_level = 0;
        return;
    }

    u_int increment = 0x8000u >> (shift > 11 ? shift - 11 : 0);
    step *= 1 << (shift < 11 ? 11 - shift : 0);
    if (exponential)
    {
        if (decreasing)
            step = (step * v->adsr_level) >> 15;
        else if (v->adsr_level > 0x6000)
            increment >>= 2;
    }
    // Very slow rates still advance the counter by one, matching the SPU2 reference.
    v->adsr_counter += increment != 0 ? increment : 1;
    if (v->adsr_counter >= 0x8000)
    {
        v->adsr_counter = 0;
        int level = v->adsr_level + step;
        v->adsr_level = level < 0 ? 0 : (level > ADSR_LEVEL_MAX ? ADSR_LEVEL_MAX : level);
    }

    if (v->adsr_phase == VOICE_ADSR_SUSTAIN)
    {
        if (v->adsr_level != 0)
            return;
    }
    else if (!(decreasing ? v->adsr_level <= target : v->adsr_level >= target))
    {
        return;
    }
    else if (v->adsr_phase != VOICE_ADSR_RELEASE)
    {
        v->adsr_phase = (VOICE_ADSR_PHASE)(v->adsr_phase + 1);
        return;
    }

    v->adsr_phase = VOICE_ADSR_OFF;
    BeginVoiceEnd(v->vNo);
}
static s32 GetVoiceAdsrLevel(VOICE *v)
{
    if (!VoiceAdsrConfigured(v)) return ADSR_LEVEL_MAX;
    AdvanceVoiceAdsr(v);
    return v->adsr_level;
}
static s32 ApplyEnvelopeToVolume(s32 volume, s32 envelope)
{ return s32((int64_t(volume) * envelope) >> 15); }
static s32 GetVoiceLeftVolume(const VOICE *v)
{
    return (s32)(((int64_t)mVolL[v->vNo / 24] * (int64_t)v->volL) / INT16_MAX);
}
static s32 GetVoiceRightVolume(const VOICE *v)
{
    return (s32)(((int64_t)mVolR[v->vNo / 24] * (int64_t)v->volR) / INT16_MAX);
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
static int StereoPairRightVoice(int vNo)
{
    if (vNo == 0)
    {
        return 1;
    }

    return -1;
}
static int StereoPairLeftVoice(int vNo)
{
    if (vNo == 1)
    {
        return 0;
    }

    return -1;
}
// Resample first, then apply register-clock envelopes to 48 kHz output frames.
// SDL keeps its band-limited resampler; pitch never changes the envelope clock.
static bool EnsureResampler(VOICE *v, int channels)
{
    if (v->resampler && v->resampler_channels != channels)
    {
        SDL_DestroyAudioStream(v->resampler);
        v->resampler = NULL;
    }
    if (!v->resampler)
    {
        SDL_AudioSpec spec{};
        spec.channels = channels;
        spec.format = SDL_AUDIO_S16;
        spec.freq = 48000;
        v->resampler = SDL_CreateAudioStream(&spec, &spec);
        v->resampler_channels = channels;
        v->resampler_flushed = false;
        v->frequency_ratio = 0;
    }
    if (!v->resampler) return false;
    float ratio = v->pitch / float(0x1000);
    if (ratio <= 0) ratio = 1.0f;
    if (ratio != v->frequency_ratio)
    {
        if (!SDL_SetAudioStreamFrequencyRatio(v->resampler, ratio)) return false;
        v->frequency_ratio = ratio;
    }
    return true;
}
static void MixOutput(int frames, VOICE *left, VOICE *right)
{
    s16 *pcm = left->buffer;
    const s32 ll = GetVoiceLeftVolume(left), lr = GetVoiceRightVolume(left);
    const s32 rl = right ? GetVoiceLeftVolume(right) : 0;
    const s32 rr = right ? GetVoiceRightVolume(right) : 0;
    if (!right) memmove(pcm + frames, pcm, frames * sizeof(s16));
    for (int i = 0; i < frames; ++i)
    {
        const s16 l = right ? pcm[i * 2] : pcm[frames + i];
        const s16 r = right ? pcm[i * 2 + 1] : 0;
        const s32 le = GetVoiceAdsrLevel(left);
        const s32 re = right ? GetVoiceAdsrLevel(right) : 0;
        pcm[i * 2] = ClampS32ToS16(ApplyVolume(l, ApplyEnvelopeToVolume(ll, le))
            + ApplyVolume(r, ApplyEnvelopeToVolume(rl, re)));
        pcm[i * 2 + 1] = ClampS32ToS16(ApplyVolume(l, ApplyEnvelopeToVolume(lr, le))
            + ApplyVolume(r, ApplyEnvelopeToVolume(rr, re)));
    }
}
static void FillOutput(int leftNo, int rightNo)
{
    VOICE *left = &voices[leftNo];
    VOICE *right = rightNo >= 0 ? &voices[rightNo] : NULL;
    const int channels = right ? 2 : 1;
    if (!left->stream || !left->buffer || !EnsureResampler(left, channels))
    {
        StopVoicePlayback(leftNo);
        if (right) StopVoicePlayback(rightNo);
        return;
    }
    int blocks = 0;
    bool failed = false;
    while (SDL_GetAudioStreamQueued(left->stream) < VOICE_TARGET_QUEUE_BYTES)
    {
        const int available = SDL_GetAudioStreamAvailable(left->resampler);
        if (available < 0) { failed = true; break; }
        const int room = (VOICE_TARGET_QUEUE_BYTES - SDL_GetAudioStreamQueued(left->stream)) / 4;
        const int frames = available / (channels * int(sizeof(s16))) < room
            ? available / (channels * int(sizeof(s16))) : room;
        if (frames > 0)
        {
            const int bytes = frames * channels * int(sizeof(s16));
            if (SDL_GetAudioStreamData(left->resampler, left->buffer, bytes) != bytes)
            { failed = true; break; }
            MixOutput(frames, left, right);
            if (!SDL_PutAudioStreamData(left->stream, left->buffer, frames * 4))
            { failed = true; break; }
            continue;
        }
        if (!left->isPlaying || (right && !right->isPlaying))
        {
            BeginVoiceEnd(leftNo);
            if (right) BeginVoiceEnd(rightNo);
            if (!left->resampler_flushed)
            {
                if (!SDL_FlushAudioStream(left->resampler)) { failed = true; break; }
                left->resampler_flushed = true;
                continue;
            }
            if (!SDL_FlushAudioStream(left->stream)) failed = true;
            break;
        }
        if (blocks >= VOICE_MAX_BLOCKS_PER_FILL) break;
        s16 a[ADPCM_BLOCK_SAMPLES], b[ADPCM_BLOCK_SAMPLES], interleaved[ADPCM_BLOCK_SAMPLES * 2];
        if (!DecodeVoiceBlock(leftNo, a) || (right && !DecodeVoiceBlock(rightNo, b)))
        { failed = true; break; }
        for (int i = 0; i < ADPCM_BLOCK_SAMPLES; ++i)
        {
            interleaved[i * 2] = a[i];
            interleaved[i * 2 + 1] = right ? b[i] : 0;
        }
        if (!SDL_PutAudioStreamData(left->resampler, right ? interleaved : a,
                                    ADPCM_BLOCK_SAMPLES * channels * int(sizeof(s16))))
        { failed = true; break; }
        ++blocks;
    }
    if (failed)
    {
        info_log("Voice %d audio submission failed: %s", leftNo, SDL_GetError());
        StopVoicePlayback(leftNo);
        if (right) StopVoicePlayback(rightNo);
        return;
    }
    if (left->endPending && left->resampler_flushed
        && SDL_GetAudioStreamAvailable(left->resampler) == 0
        && SDL_GetAudioStreamQueued(left->stream) <= 0)
    {
        FinishVoicePlayback(leftNo);
        if (right) FinishVoicePlayback(rightNo);
    }
}
void VoiceRun()
{
    LockVoices();
    RefreshVoiceStreamGains();
    for (int i = 0; i < VOICE_NUM; ++i)
    {
        VOICE *v = &voices[i];
        if (!v->isPlaying && !v->endPending) continue;
        const int partner = StereoPairLeftVoice(i);
        if (partner >= 0 && (voices[partner].isPlaying || voices[partner].endPending)) continue;
        const int right = StereoPairRightVoice(i);
        const bool paired = right >= 0 && ((voices[right].isPlaying && v->isPlaying)
            || (v->endPending && v->resampler_channels == 2));
        FillOutput(i, paired ? right : -1);
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
    v->adsr_counter = 0;
    v->adsr_phase = VOICE_ADSR_ATTACK;
    if (v->resampler) SDL_ClearAudioStream(v->resampler);
    v->resampler_flushed = false;
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

    if (v->adsr_level <= 0 || v->adsr_level > ADSR_LEVEL_MAX)
    {
        v->adsr_level = ADSR_LEVEL_MAX;
    }

    v->adsr_counter = 0;
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
    if (v->resampler) { SDL_DestroyAudioStream(v->resampler); v->resampler = NULL; }
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
        if (voices[i].resampler) { SDL_DestroyAudioStream(voices[i].resampler); voices[i].resampler = NULL; }
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
    if (v->nax >= sizeof(spuRam) / sizeof(spuRam[0]))
    {
        StopVoicePlayback(vNo);
        v->header = 0;
        return;
    }
    v->header = bytes[v->nax & ~0x7];

    if (v->header & (1 << 10))
    {
        v->lsa = v->nax & ~0x7;
    }
}
