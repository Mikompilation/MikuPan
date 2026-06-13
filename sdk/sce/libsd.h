#ifndef _LIBSD_H_
#define _LIBSD_H_

#include "common.h"
#include "typedefs.h"

#define SD_CORE_0 0
#define SD_CORE_1 1

#define SD_INIT_HOT 1

#define SD_TRANS_STATUS_CHECK 0
#define SD_TRANS_STATUS_WAIT  1

#define SD_S_PMON   (0x13 << 8)
#define SD_S_NON    (0x14 << 8)
#define SD_S_KON    (0x15 << 8)
#define SD_S_KOFF   (0x16 << 8)
#define SD_S_ENDX   (0x17 << 8)
#define SD_S_VMIXL  (0x18 << 8)
#define SD_S_VMIXEL (0x19 << 8)
#define SD_S_VMIXR  (0x1a << 8)
#define SD_S_VMIXER (0x1b << 8)

#define SD_VA_SSA  ((0x20 << 8) + (0x01 << 6))
#define SD_VA_LSAX ((0x21 << 8) + (0x01 << 6))
#define SD_VA_NAX  ((0x22 << 8) + (0x01 << 6))

#define SD_A_IRQA (0x1f << 8)
#define SD_A_EEA  (0x1d << 8)

#define SD_VP_VOLL  (0x00 << 8)
#define SD_VP_VOLR  (0x01 << 8)
#define SD_VP_PITCH (0x02 << 8)
#define SD_VP_ADSR1 (0x03 << 8)
#define SD_VP_ADSR2 (0x04 << 8)
#define SD_VP_ENVX  (0x05 << 8)
#define SD_VP_VOLXL (0x06 << 8)
#define SD_VP_VOLXR (0x07 << 8)

#define SD_P_MVOLL ((0x09 << 8) + (0x01 << 7))
#define SD_P_MVOLR ((0x0a << 8) + (0x01 << 7))
#define SD_P_EVOLL ((0x0b << 8) + (0x01 << 7))
#define SD_P_EVOLR ((0x0c << 8) + (0x01 << 7))

#define SD_C_IRQ_ENABLE    4
#define SD_C_EFFECT_ENABLE 3

#define SD_REV_MODE_CLEAR_WA 0x100
#define SD_REV_MODE_STUDIO_B 0x007

#define SD_VOICE(no) ((no) << 1)

typedef struct {
    int core;
    int mode;
    int depth_L;
    int depth_R;
    int delay;
    int feedback;
} sceSdEffectAttr;

extern s16 spuRam[(1024 * 1024 * 2) >> 1];
extern s32 mVolL;
extern s32 mVolR;

int sceSdInit(int flag);
void sceSdSetParam(u_short entry, u_short value);
u_short sceSdGetParam(u_short entry);
void sceSdSetSwitch(u_short entry, u_int value);
u_int sceSdGetSwitch(u_short entry);
int sceSdVoiceTrans(short channel, u_short mode, void* m_addr, u_int s_addr, u_int size);
int sceSdVoiceTransStatus(short channel, short flag);
void sceSdSetAddr(u_short entry, u_int value);
void sceSdSetCoreAttr(u_short entry, u_short value);
void sceSdSetEffectAttr(int core, sceSdEffectAttr* attr);
void sceSdSetTransIntrHandler(int channel, sceSdTransIntrHandler handler, void* common);
void sceSdSetSpu2IntrHandler(sceSdSpu2IntrHandler handler, void* common);

void ClearAudioBuffer(void);
void MikuPan_SdShutdown(void);

/* Called by the voice mixer with a voice's playback position (half-word index
   into spuRam) each time it advances to a new ADPCM block; raises the SPU2
   interrupt when the position matches the core's IRQA. */
void MikuPan_SdVoiceReachedAddress(int voice_index, u_int nax);

#endif /* _LIBSD_H_ */
