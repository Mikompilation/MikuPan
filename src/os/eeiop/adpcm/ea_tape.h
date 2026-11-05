#include "mikupan/mikupan_region.h"
#ifndef OS_EEIOP_ADPCM_EA_TAPE_H
#define OS_EEIOP_ADPCM_EA_TAPE_H

#include "typedefs.h"


MIKUPAN_NAMESPACE_BEGIN

void EAdpcmTapeMain();
void AdpcmPlayTape(int file_no, u_short vol, int fade_flm);
void AdpcmStopTape(int fout_flm);
u_char IsEndAdpcmTape();


MIKUPAN_NAMESPACE_END
#endif // OS_EEIOP_ADPCM_EA_TAPE_H
