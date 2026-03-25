#ifndef _LIBSD_H_
#define _LIBSD_H_
#include "common.h"
#include "mikupan/mikupan_audio.h"
#include "typedefs.h"

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
