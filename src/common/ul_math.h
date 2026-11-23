#include "mikupan/mikupan_region.h"
#ifndef COMMON_UL_MATH_H
#define COMMON_UL_MATH_H

#include "typedefs.h"


MIKUPAN_NAMESPACE_BEGIN

void GetSquare(sceVu0FVECTOR *v0, sceVu0FVECTOR *v1);
float GetDist(float x, float z);
float GetDistV(float *p0, float *p1);
float GetDistV2(float *p0, float *p1);
void RotFvector(float *rot, float *tv);


MIKUPAN_NAMESPACE_END
#endif // COMMON_UL_MATH_H
