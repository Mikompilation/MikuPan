#include "libsd.h"

#include "iop/se/voice.h"
#include "mikupan/mikupan_logging_c.h"

#include <SDL3/SDL_audio.h>
#include <SDL3/SDL_init.h>
#include <SDL3/SDL_platform.h>
#include <SDL3/SDL_stdinc.h>
#include <string.h>

s16 spuRam[(1024 * 1024 * 2) >> 1];
s32 mVolL;
s32 mVolR;
SDL_AudioDeviceID audio_dev;

static u_short sd_param_regs[0x10000];
static u_int sd_switch_regs[0x10000];
static sceSdTransIntrHandler sd_trans_handlers[2];
static void* sd_trans_common[2];
static sceSdSpu2IntrHandler sd_spu2_handler;
static void* sd_spu2_common;
static int sd_initialized;
/* IRQA per core, stored as a half-word (s16) index into spuRam. The SPU2
   interrupt must fire when a voice's playback position reaches this address
   (see MikuPan_SdVoiceReachedAddress) — the ADPCM streamer paces its CD reads
   and SPU uploads off that signal, so firing it any earlier makes the music
   stream free-run through the whole song. */
static u_int sd_core_irqa[2];
static int sd_core_irq_enable[2];

static int VoiceIndexFromEntry(u_short entry)
{
    int core = entry & SD_CORE_1;
    int voice = (entry >> 1) & 0x1f;
    return core * 24 + voice;
}

static int IsRealAudioDriver(const char* drv)
{
    if (drv == NULL) {
        return 0;
    }

#if defined(_WIN32)
    return SDL_strcasecmp(drv, "wasapi") == 0
        || SDL_strcasecmp(drv, "directsound") == 0
        || SDL_strcasecmp(drv, "winmm") == 0;
#elif defined(__linux__)
    return SDL_strcasecmp(drv, "pipewire") == 0
        || SDL_strcasecmp(drv, "pulseaudio") == 0
        || SDL_strcasecmp(drv, "alsa") == 0
        || SDL_strcasecmp(drv, "jack") == 0
        || SDL_strcasecmp(drv, "sndio") == 0;
#elif defined(__APPLE__)
    return SDL_strcasecmp(drv, "coreaudio") == 0;
#else
    return SDL_strcasecmp(drv, "dummy") != 0
        && SDL_strcasecmp(drv, "disk") != 0;
#endif
}

static void LogSDLAudioDiagnostics(void)
{
    int n = SDL_GetNumAudioDrivers();
    int has_real_driver = 0;

    for (int i = 0; i < n; i++) {
        const char* drv = SDL_GetAudioDriver(i);
        if (IsRealAudioDriver(drv)) {
            has_real_driver = 1;
            break;
        }
    }

    if (has_real_driver) {
        return;
    }

    info_log("Platform: %s", SDL_GetPlatform());
    info_log("Available SDL audio drivers: %d", n);

    for (int i = 0; i < n; i++) {
        const char* drv = SDL_GetAudioDriver(i);
        info_log("  driver[%d]=%s", i, drv ? drv : "(null)");
    }

    info_log("WARNING: No real audio backend found.");
}

int sceSdInit(int flag)
{
    (void)flag;

    if (sd_initialized) {
        return 0;
    }

    ClearAudioBuffer();

    if (!SDL_InitSubSystem(SDL_INIT_AUDIO)) {
        info_log("Failed to initialize SDL audio subsystem: %s", SDL_GetError());
        LogSDLAudioDiagnostics();
        audio_dev = 0;
    } else {
        SDL_AudioSpec spec;
        SDL_zero(spec);
        spec.channels = 2;
        spec.format = SDL_AUDIO_S16;
        spec.freq = 48000;

        info_log("Audio Driver: %s", SDL_GetCurrentAudioDriver());
        audio_dev = SDL_OpenAudioDevice(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec);
        if (audio_dev == 0) {
            info_log("Failed to open audio device: %s", SDL_GetError());
            LogSDLAudioDiagnostics();
        } else {
            SDL_ResumeAudioDevice(audio_dev);
        }
    }

    VoicesInit();
    sd_initialized = 1;
    return 0;
}

int sceSdVoiceTrans(short channel, u_short mode, void* m_addr, u_int s_addr, u_int size)
{
    (void)mode;

    if (m_addr == NULL) {
        return -1;
    }

    if (s_addr < sizeof(spuRam)) {
        u_int copy_size = size;
        if (s_addr + copy_size > sizeof(spuRam)) {
            copy_size = sizeof(spuRam) - s_addr;
        }
        memcpy(((u_char*)spuRam) + s_addr, m_addr, copy_size);
    }

    if (channel >= 0 && channel < 2 && sd_trans_handlers[channel] != NULL) {
        sd_trans_handlers[channel](channel, sd_trans_common[channel]);
    }

    return (int)size;
}

int sceSdVoiceTransStatus(short channel, short flag)
{
    (void)channel;
    (void)flag;
    return 1;
}

void sceSdSetSwitch(u_short entry, u_int value)
{
    u_int reg = entry & ~0x3f;
    int core = entry & SD_CORE_1;

    sd_switch_regs[entry] = value;

    if (reg == SD_S_KON || reg == SD_S_KOFF) {
        sd_switch_regs[SD_S_ENDX | core] &= ~value;
    }

    if (reg != SD_S_KON && reg != SD_S_KOFF) {
        return;
    }

    VoiceSetKeySwitch(core, value, reg == SD_S_KON);
}

u_int sceSdGetSwitch(u_short entry)
{
    return sd_switch_regs[entry];
}

void sceSdSetAddr(u_short entry, u_int value)
{
    u_int reg = entry & ~0x3f;
    int voice_index = VoiceIndexFromEntry(entry);

    if (reg == SD_A_IRQA) {
        sd_core_irqa[entry & SD_CORE_1] = value >> 1;
        return;
    }

    if (voice_index < 0 || voice_index >= VOICE_NUM) {
        return;
    }

    switch (reg) {
    case SD_VA_SSA:
        voices[voice_index].ssa = value >> 1;
        break;
    case SD_VA_LSAX:
        voices[voice_index].lsa = value >> 1;
        break;
    case SD_VA_NAX:
        voices[voice_index].nax = value >> 1;
        break;
    default:
        break;
    }
}

static void SetMasterVolLeft(s32 val)
{
    mVolL = val << 1;
}

static void SetMasterVolRight(s32 val)
{
    mVolR = val << 1;
}

void sceSdSetParam(u_short entry, u_short value)
{
    u_int reg = entry & ~0x3f;
    int voice_index = VoiceIndexFromEntry(entry);

    sd_param_regs[entry] = value;

    switch (reg) {
    case SD_VP_PITCH:
        if (voice_index < VOICE_NUM) {
            SetPitch(voice_index, value);
        }
        break;
    case SD_VP_VOLL:
        if (voice_index < VOICE_NUM) {
            SetVolLeft(voice_index, value);
        }
        break;
    case SD_VP_VOLR:
        if (voice_index < VOICE_NUM) {
            SetVolRight(voice_index, value);
        }
        break;
    case SD_P_MVOLL:
        SetMasterVolLeft(value);
        break;
    case SD_P_MVOLR:
        SetMasterVolRight(value);
        break;
    case SD_VP_ADSR1:
        if (voice_index < VOICE_NUM) {
            SetAdsr1(voice_index, value);
        }
        break;
    case SD_VP_ADSR2:
        if (voice_index < VOICE_NUM) {
            SetAdsr2(voice_index, value);
        }
        break;
    default:
        break;
    }
}

u_short sceSdGetParam(u_short entry)
{
    return sd_param_regs[entry];
}

void sceSdSetCoreAttr(u_short entry, u_short value)
{
    if ((entry & SD_C_IRQ_ENABLE) != 0) {
        sd_core_irq_enable[entry & SD_CORE_1] = value != 0;
    }
}

void MikuPan_SdVoiceReachedAddress(int voice_index, u_int nax)
{
    int core = voice_index >= 24 ? SD_CORE_1 : SD_CORE_0;

    if (!sd_core_irq_enable[core] || sd_spu2_handler == NULL) {
        return;
    }

    if (nax == sd_core_irqa[core]) {
        sd_spu2_handler(core == SD_CORE_1 ? 2 : 1, sd_spu2_common);
    }
}

void MikuPan_SdSetVoiceEnd(int voice_index)
{
    int core;
    int voice;

    if (voice_index < 0 || voice_index >= VOICE_NUM) {
        return;
    }

    core = voice_index >= 24 ? SD_CORE_1 : SD_CORE_0;
    voice = voice_index % 24;
    sd_switch_regs[SD_S_ENDX | core] |= 1u << voice;
}

void sceSdSetEffectAttr(int core, sceSdEffectAttr* attr)
{
    (void)core;
    (void)attr;
}

void sceSdSetTransIntrHandler(int channel, sceSdTransIntrHandler handler, void* common)
{
    if (channel >= 0 && channel < 2) {
        sd_trans_handlers[channel] = handler;
        sd_trans_common[channel] = common;
    }
}

void sceSdSetSpu2IntrHandler(sceSdSpu2IntrHandler handler, void* common)
{
    sd_spu2_handler = handler;
    sd_spu2_common = common;
}

void ClearAudioBuffer(void)
{
    memset(spuRam, 0, sizeof(spuRam));
}

void MikuPan_SdShutdown(void)
{
    if (!sd_initialized) {
        return;
    }

    CloseVoices();
    if (audio_dev != 0) {
        SDL_CloseAudioDevice(audio_dev);
        audio_dev = 0;
    }
    sd_initialized = 0;
}
