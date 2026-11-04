#include "mikupan/mikupan_region.h"
#ifndef OS_EEIOP_ADPCM_EA_DAT_H
#define OS_EEIOP_ADPCM_EA_DAT_H

#include "typedefs.h"


MIKUPAN_NAMESPACE_BEGIN

typedef struct {
	int file_no;
	u_short vol;
} ADPCM_VOL;

#define MAX_VOLUME 0x3fff
#define NO_VOL_TABLE 368

extern ADPCM_VOL adpcm_vol_tbl[NO_VOL_TABLE];

u_short GetAdpcmVol(int file_no);


MIKUPAN_NAMESPACE_END
#endif // OS_EEIOP_ADPCM_EA_DAT_H
