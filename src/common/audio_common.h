#ifndef AUDIO_COMMON_H
#define AUDIO_COMMON_H
#include "common.h"

#define SAMPLE_RATE 48000
#define BIT_RATE 16
#define CHANNELS 2

#define min(a, b) (((a) < (b)) ? (a) : (b))
#define max(a, b) (((a) > (b)) ? (a) : (b))
#define clamp(val, min, max)                                                   \
    (((val) > (max)) ? (max) : (((val) < (min)) ? (min) : (val)))

static const s32 tbl_adpcm_filter[16][2] = {
    {  0,   0},
    { 60,   0},
    {115, -52},
    { 98, -55},
    {122, -60}
};

static void adpcm_decode_block(s16 *buffer, s16 *block, s32 *prev1, s32 *prev2)
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

#endif // AUDIO_COMMON_H