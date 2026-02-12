#ifndef AUDIO_COMMON_H
#define AUDIO_COMMON_H
#include "common.h"
#include "typedefs.h"
#include <SDL3/SDL_audio.h>

#define SAMPLE_RATE 48000
#define BIT_RATE 16
#define CHANNELS 2

#define min(a, b) (((a) < (b)) ? (a) : (b))
#define max(a, b) (((a) > (b)) ? (a) : (b))
#define clamp(val, min, max)                                                   \
    (((val) > (max)) ? (max) : (((val) < (min)) ? (min) : (val)))

#define SD_S_PMON (0x13 << 8)
#define SD_S_NON (0x14 << 8)
// Key On
#define SD_S_KON (0x15 << 8)
// Key Off
#define SD_S_KOFF (0x16 << 8)
#define SD_S_ENDX (0x17 << 8)
#define SD_S_VMIXL (0x18 << 8)
#define SD_S_VMIXEL (0x19 << 8)
#define SD_S_VMIXR (0x1a << 8)
#define SD_S_VMIXER (0x1b << 8)
#define SD_VA_SSA ((0x20 << 8) + (0x01 << 6))
#define SD_VA_LSAX ((0x21 << 8) + (0x01 << 6))
#define SD_VA_NAX ((0x22 << 8) + (0x01 << 6))

#define SD_VP_VOLL (0x00 << 8)
#define SD_VP_VOLR (0x01 << 8)
#define SD_VP_PITCH (0x02 << 8)
#define SD_VP_ADSR1 (0x03 << 8)
#define SD_VP_ADSR2 (0x04 << 8)
#define SD_VP_ENVX (0x05 << 8)
#define SD_VP_VOLXL (0x06 << 8)
#define SD_VP_VOLXR (0x07 << 8)

#define SD_P_MVOLL ((0x09 << 8) + (0x01 << 7))
#define SD_P_MVOLR ((0x0a << 8) + (0x01 << 7))

#define SD_A_EEA (0x1d << 8)

#define SD_P_EVOLL ((0x0b << 8) + (0x01 << 7))
#define SD_P_EVOLR ((0x0c << 8) + (0x01 << 7))

static const s32 tbl_adpcm_filter[16][2] = {
    {  0,   0},
    { 60,   0},
    {115, -52},
    { 98, -55},
    {122, -60}
};

static void MikuPan_DecodeAdpcmBlock(s16 *buffer, s16 *block, s32 *prev1,
                                     s32 *prev2)
{
    const s32 header = *block;
    const s32 shift = (header & 0xF) + BIT_RATE;
    const int id = header >> 4 & 0xF;
    const s32 pred1 = tbl_adpcm_filter[id][0];
    const s32 pred2 = tbl_adpcm_filter[id][1];

    const s8 *blockbytes = (s8 *) &block[1];
    const s8 *blockend = &blockbytes[13];

    for (; blockbytes <= blockend; ++blockbytes)
    {
        s32 data = ((*blockbytes) << 28) & 0xF0000000;
        s32 pcm =
            (data >> shift) + (((pred1 * *prev1) + (pred2 * *prev2) + 32) >> 6);

        pcm = clamp(pcm, -0x8000, 0x7fff);
        *(buffer++) = pcm;

        data = ((*blockbytes) << 24) & 0xF0000000;
        s32 pcm2 =
            (data >> shift) + (((pred1 * pcm) + (pred2 * *prev1) + 32) >> 6);

        pcm2 = clamp(pcm2, -0x8000, 0x7fff);
        *(buffer++) = pcm2;

        *prev2 = pcm;
        *prev1 = pcm2;
    }
}

static void MikuPan_FillBuffer(int size, u_char channel, s16 **src_buf,
                               s16 **dec_buf, SDL_AudioStream *stream)
{
    void *dec[2] = {dec_buf[0], dec_buf[1]};

    s32 histL[2] = {}, histR[2] = {};

    s16 *src = src_buf[channel];
    s16 *dst;

    int chunks = size / 0x800 / 2;

    // Prevent memleaks by not reading too much into the audio stream.
    if (SDL_GetAudioStreamQueued(stream) >= size)
    {
        return;
    }

    for (int i = 0; i < chunks; i++)
    {
        dst = dec_buf[0];
        for (int j = 0; j < 128; j++)
        {

            MikuPan_DecodeAdpcmBlock(dst, src, &histL[0], &histL[1]);
            dst += 28;
            src += 8;
        }

        dst = dec_buf[1];
        for (int j = 0; j < 128; j++)
        {

            MikuPan_DecodeAdpcmBlock(dst, src, &histR[0], &histR[1]);
            dst += 28;
            src += 8;
        }

        SDL_PutAudioStreamPlanarData(stream, (void *) dec, CHANNELS, 3584);
    }
}

#endif// AUDIO_COMMON_H