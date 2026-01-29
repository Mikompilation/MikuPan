#include "common.h"
#include "furn_ctl.h"

void FurnEfctSet(/* s3 19 */ FURN_WRK *fwp)
{

}

void FurnEfctFree(FURN_WRK *fwp)
{

}

INCLUDE_ASM(const s32, "ingame/map/furn_eff", FurnEfctSetUse);

INCLUDE_ASM(const s32, "ingame/map/furn_eff", FurnEfctSetPos0);

INCLUDE_ASM(const s32, "ingame/map/furn_eff", FurnEfctSetPos1);

INCLUDE_ASM(const s32, "ingame/map/furn_eff", FurnEfctSetRot0);

INCLUDE_ASM(const s32, "ingame/map/furn_eff", FurnEfctGetFefct);

INCLUDE_ASM(const s32, "ingame/map/furn_eff", FurnEfctFreeFefct);

void FurnHintDeformCtrl()
{
    
}
