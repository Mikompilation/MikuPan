#include "mikupan/mikupan_region.h"
#ifndef OS_EEIOP_ADPCM_EA_EVENT_H
#define OS_EEIOP_ADPCM_EA_EVENT_H

#include "typedefs.h"


MIKUPAN_NAMESPACE_BEGIN

void EAdpcmEventMain();
void AdpcmPlayEvent(int file_no, u_short start_flm, int fade_flm);
void AdpcmStopEvent(int fout_flm);


MIKUPAN_NAMESPACE_END
#endif // OS_EEIOP_ADPCM_EA_EVENT_H
