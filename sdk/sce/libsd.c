#include "libsd.h"
#include "iop/se/voice.h"
#include "mikupan/mikupan_logging_c.h"
#include "typedefs.h"
#include <stdlib.h>

s16 audioBuffer[0x15160 * 10];

static int totalAmt;

int sceSdVoiceTrans(short channel, u_short mode, void *m_addr, u_int s_addr,
                    u_int size)
{
    info_log("VOICE TRANS \n");
    memcpy(&audioBuffer[s_addr], m_addr, size);
    totalAmt += size;
    return size;
}

void sceSdSetSwitch(u_short entry, u_int value)
{
    u32 reg = entry & ~0x3f;
    u8 voice = 0;
    while (value)
    {
        u8 bit = value & 1;

        if (bit)
        {
            switch (reg)
            {
                case SD_S_KON:
                    Key_On(voice, totalAmt);
                    break;

                case SD_S_KOFF:
                    Key_Off(voice);
                    break;
            }
        }
        voice++;
        value >>= 1;
    }
}

void sceSdSetAddr(u_short entry, u_int value)
{
    u32 reg = entry & ~0x3f;
    u32 voice_id = (entry >> 1) & 0x1f;
    switch (reg)
    {
        case SD_VA_SSA:
            voices[voice_id].ssa = value >> 1;
            break;

        case SD_VA_LSAX:
            voices[voice_id].lsa = value >> 1;
            break;
    }
}

u_int sceSdGetSwitch(u_short entry)
{
}

void sceSdSetParam(u_short entry, u_short value)
{
    u32 reg = entry & ~0x3f;
    u32 voice_id = (entry >> 1) & 0x1f;

    switch (reg)
    {
        case SD_VP_PITCH:
            SetPitch(voice_id, value);
            break;
    }
}

u_short sceSdGetParam(u_short entry)
{
}

void ClearAudioBuffer()
{
    memset(audioBuffer, 0, totalAmt);
    totalAmt = 0;
}