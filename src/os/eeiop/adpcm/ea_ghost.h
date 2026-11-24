#include "mikupan/mikupan_region.h"
#ifndef OS_EEIOP_ADPCM_EA_GHOST_H
#define OS_EEIOP_ADPCM_EA_GHOST_H

#include "typedefs.h"


MIKUPAN_NAMESPACE_BEGIN

void AdpcmPlayGhost(int file_no, sceVu0FVECTOR *mpos, u_short vol, u_char ewrk_no, int fade_flm);
void AdpcmStopGhost(int fout_flm);
void EAdpcmGhostMain();


MIKUPAN_NAMESPACE_END
#endif // OS_EEIOP_ADPCM_EA_GHOST_H
