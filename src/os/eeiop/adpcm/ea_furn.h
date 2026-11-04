#include "mikupan/mikupan_region.h"
#ifndef OS_EEIOP_ADPCM_EA_FURN_H
#define OS_EEIOP_ADPCM_EA_FURN_H

#include "typedefs.h"


MIKUPAN_NAMESPACE_BEGIN

void EAdpcmFurnMain();
void AdpcmPlayFurn(int file_no, float *pos, u_short vol, int fade_flm);
void AdpcmStopFurn(int fout_flm);


MIKUPAN_NAMESPACE_END
#endif // OS_EEIOP_ADPCM_EA_FURN_H
