#include "mikupan/mikupan_region.h"
#ifndef OS_EEIOP_ADPCM_EA_GAMEOVER_H
#define OS_EEIOP_ADPCM_EA_GAMEOVER_H

#include "typedefs.h"


MIKUPAN_NAMESPACE_BEGIN

void EAdpcmGoverMain();
void AdpcmPlayGameOver(int file_no, u_short vol, int fade_flm);
void AdpcmStopGameOver(int fout_flm);


MIKUPAN_NAMESPACE_END
#endif // OS_EEIOP_ADPCM_EA_GAMEOVER_H
