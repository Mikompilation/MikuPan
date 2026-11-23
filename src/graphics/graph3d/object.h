#include "mikupan/mikupan_region.h"
#ifndef GRAPHICS_GRAPH3D_OBJECT_H
#define GRAPHICS_GRAPH3D_OBJECT_H

#include "typedefs.h"


MIKUPAN_NAMESPACE_BEGIN

extern qword *pstart1;
extern qword *pstart3;
extern qword *pbase1;
extern qword *pbase3;

void objInit();
void objFlush();


MIKUPAN_NAMESPACE_END
#endif // GRAPHICS_GRAPH3D_OBJECT_H
