#ifndef VOICE_H
#define VOICE_H
#include "common.h"
#include "typedefs.h"
#include <SDL3/SDL_audio.h>

#define SD_VOICE(no) ((no) << 1)
#define LOOPEND(flags) (flags & 1 << 8)
#define LOOP(flags) (flags & 1 << 9)
#define LOOPSTART(flags) (flags & 1 << 10)

enum SE_VOICE_STAT
{
    VOICE_FREE = 0,
    VOICE_USE = 1,
    VOICE_LOOP = 2,
    VOICE_RESERVED = 3
};
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
    s16 *buffer;
    u16 header;
    u_int ssa;// Single Playback Address
    u_int lsa;// Looped Playback Address
    u_int nax;

    u_short volL;
    u_short volR;
    u_short pitch;
    u_short adsr1;
    u_short adsr2;
    SDL_AudioStream *stream;
} VOICE;

extern VOICE voices[24];

void VoicesInit();
VOICE *GetFreeVoice();
void FillAdpcmHeader(int vNo);
void Key_On(int vNo);
void Key_Off(int vNo);

static inline void SetPitch(int vNo, u_short val)
{
    voices[vNo].pitch = val;
}

static inline void SetVolLeft(int vNo, u_short val)
{
    voices[vNo].volL = val;
}

static inline void SetVolRight(int vNo, u_short val)
{
    voices[vNo].volR = val;
}

static inline void SetAdsr1(int vNo, u_short val)
{
    voices[vNo].adsr1 = val;
}

static inline void SetAdsr2(int vNo, u_short val)
{
    voices[vNo].adsr2 = val;
}

static inline s16 ApplyVolume(s16 sample, s16 vol)
{
    return (vol * sample) >> 15;
}

void CloseVoice(int vNo);
void CloseVoices();

#endif// VOICE_H