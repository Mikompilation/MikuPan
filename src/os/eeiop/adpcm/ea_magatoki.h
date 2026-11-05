#include "mikupan/mikupan_region.h"
#ifndef OS_EEIOP_ADPCM_EA_MAGATOKI_H
#define OS_EEIOP_ADPCM_EA_MAGATOKI_H

#include "typedefs.h"


MIKUPAN_NAMESPACE_BEGIN

void EAdpcmMagatokiMain();
void AdpcmPreMagatokiFadeOut(u_short fout_flm);
u_char IsFadeoutEndAdpcmMagatoki();
void AdpcmMagatokiPlay(int file_no);
void AdpcmMagatokiStop();


MIKUPAN_NAMESPACE_END
#endif // OS_EEIOP_ADPCM_EA_MAGATOKI_H
