#ifndef VOICE_H
#define VOICE_H
#include "common.h"
#include "iopse.h"
#include "typedefs.h"
#include <SDL3/SDL_audio.h>
#include <SDL3/SDL_thread.h>

#define VOICE_NUM 48

typedef enum
{
    VOICE_ADSR_OFF = 0,
    VOICE_ADSR_ATTACK,
    VOICE_ADSR_DECAY,
    VOICE_ADSR_SUSTAIN,
    VOICE_ADSR_RELEASE
} VOICE_ADSR_PHASE;

typedef struct
{
    u8 shift_filter;
    //u16 data;
    u8 loopStart;// Loop control flags. Detect this to find length of song!!
    u8 loop;
    u8 loopEnd;
    u8 filter;
    u8 shift;
} AudioHeader;

typedef struct
{
    int vNo;
    int size;

    bool isPlaying;
    bool endPending;

    s16 *buffer;
    u16 header;
    u_int ssa;// Single Playback Address
    u_int lsa;// Looped Playback Address
    u_int nax;

    s32 histL[2], histR[2];


    s32 volL;
    s32 volR;

    u_short pitch;
    u_short adsr1;
    u_short adsr2;
    // Register-clock envelope at the 48 kHz output rate.
    s32 adsr_level;
    u_int adsr_counter;
    VOICE_ADSR_PHASE adsr_phase;
    float frequency_ratio;
    SDL_AudioStream *stream;
    SDL_AudioStream *resampler;
    int resampler_channels;
    bool resampler_flushed;
} VOICE;

extern VOICE voices[VOICE_NUM];
extern SDL_AudioDeviceID audio_dev;

extern bool loopEnd;
extern bool loopRepeat;

void VoicesInit();
void FillAdpcmHeader(int vNo);
void Key_On(int vNo);
void Key_Off(int vNo);
void VoiceRun();
void VoiceSetKeySwitch(int core, u_int value, int key_on);
void VoiceLock(void);
void VoiceUnlock(void);

static inline s32 DecodeSpu2Volume(u_short value)
{
    if (value & 0x8000) return 0; // Sweep registers are not fixed amplitudes.
    return s32(value & 0x3fff) * 2 - ((value & 0x4000) ? 32768 : 0);
}

static inline void SetPitch(int vNo, u_short val)
{
    voices[vNo].pitch = val;
}

static inline void SetVolLeft(int vNo, u_short val)
{
    voices[vNo].volL = DecodeSpu2Volume(val);
}

static inline void SetVolRight(int vNo, u_short val)
{
    voices[vNo].volR = DecodeSpu2Volume(val);
}

static inline void SetAdsr1(int vNo, u_short val)
{
    voices[vNo].adsr1 = val;
}

static inline void SetAdsr2(int vNo, u_short val)
{
    voices[vNo].adsr2 = val;
}

void CloseVoice(int vNo);
void CloseVoices();

#endif// VOICE_H
