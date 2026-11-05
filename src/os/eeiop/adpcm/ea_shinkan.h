#include "mikupan/mikupan_region.h"
#ifndef OS_EEIOP_ADPCM_EA_SHINKAN_H
#define OS_EEIOP_ADPCM_EA_SHINKAN_H

#include "typedefs.h"


MIKUPAN_NAMESPACE_BEGIN

void EAdpcmShinkanMain();
void AdpcmPlayShinkan(int file_no, int fade_flm);
void AdpcmStopShinkan(int fout_flm);
u_char IsEndAdpcmShinkan();


MIKUPAN_NAMESPACE_END
#endif // OS_EEIOP_ADPCM_EA_SHINKAN_H
