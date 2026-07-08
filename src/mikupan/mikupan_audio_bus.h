#ifndef MIKUPAN_MIKUPAN_AUDIO_BUS_H
#define MIKUPAN_MIKUPAN_AUDIO_BUS_H

#include "typedefs.h"

#ifdef __cplusplus
extern "C" {
#endif

u_int MikuPan_AudioGetRevision(void);
void MikuPan_AudioBumpRevision(void);
float MikuPan_AudioGetMasterScale(void);

#ifdef __cplusplus
}
#endif

#endif
