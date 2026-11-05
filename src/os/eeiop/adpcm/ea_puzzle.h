#include "mikupan/mikupan_region.h"
#ifndef OS_EEIOP_ADPCM_EA_PUZZLE_H
#define OS_EEIOP_ADPCM_EA_PUZZLE_H

#include "typedefs.h"


MIKUPAN_NAMESPACE_BEGIN

void EAdpcmPuzzleMain();
void AdpcmPrePuzzleFadeOut(u_short fout_flm);
u_char IsEndAdpcmPuzzle();
void AdpcmPreLoadPuzzle(int file_no);
u_char IsPreLoadEndAdpcmPuzzle();
void AdpcmPuzzlePreLoadEndPlay();
void AdpcmPuzzlePlay(int file_no);
void AdpcmPuzzleEnd();


MIKUPAN_NAMESPACE_END
#endif // OS_EEIOP_ADPCM_EA_PUZZLE_H
