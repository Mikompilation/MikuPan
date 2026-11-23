#include "mikupan/mikupan_region.h"
#ifndef MIKUPAN_MIKUPAN_AUDIO_BUS_H
#define MIKUPAN_MIKUPAN_AUDIO_BUS_H

#include "typedefs.h"


MIKUPAN_NAMESPACE_BEGIN

#ifdef __cplusplus
MIKUPAN_EXTERN_C {
#endif

u_int MikuPan_AudioGetRevision(void);
void MikuPan_AudioBumpRevision(void);
float MikuPan_AudioGetMasterScale(void);

#ifdef __cplusplus
}
#endif


MIKUPAN_NAMESPACE_END
#endif
