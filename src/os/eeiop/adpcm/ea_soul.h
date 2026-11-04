#include "mikupan/mikupan_region.h"
#ifndef OS_EEIOP_ADPCM_EA_SOUL_H
#define OS_EEIOP_ADPCM_EA_SOUL_H

#include "typedefs.h"


MIKUPAN_NAMESPACE_BEGIN

void EAdpcmSoulMain();
void AdpcmPreSoulFadeOut(u_short fout_flm);
u_char IsFadeoutEndAdpcmSoul();
void AdpcmPlaySoul(int file_no, u_short vol, int fade_flm);
void AdpcmStopSoul(int fout_flm);


MIKUPAN_NAMESPACE_END
#endif // OS_EEIOP_ADPCM_EA_SOUL_H
