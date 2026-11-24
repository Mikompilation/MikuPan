#include "mikupan/mikupan_region.h"
#ifndef OS_EEIOP_ADPCM_EA_HISO_H
#define OS_EEIOP_ADPCM_EA_HISO_H

#include "typedefs.h"


MIKUPAN_NAMESPACE_BEGIN

void AdpcmPlayHiso(int file_no, int fade_flm);
void AdpcmStopHiso(int fout_flm);
void EAdpcmHisoMain();


MIKUPAN_NAMESPACE_END
#endif // OS_EEIOP_ADPCM_EA_HISO_H
