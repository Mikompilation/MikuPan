#ifndef _LIBSD_H_
#define _LIBSD_H_
#include "common.h"
#include "mikupan/mikupan_audio.h"
#include "typedefs.h"

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

extern s16 spuRam[(1024*1024*2) >> 1];

extern void sceSdSetParam(u_short entry, u_short value);
extern u_short sceSdGetParam(u_short entry);
extern void sceSdSetSwitch(u_short entry, u_int value);
extern u_int sceSdGetSwitch(u_short entry);
extern int sceSdVoiceTrans(short channel, u_short mode, void *m_addr,
                           u_int s_addr, u_int size);

extern void sceSdSetAddr(u_short entry, u_int value);

extern void ClearAudioBuffer();

#endif /* _LIBSD_H_ */
