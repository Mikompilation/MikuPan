#include "mikupan/mikupan_region.h"
#ifndef GRAPHICS_GRAPH2D_RARE_ENE_H
#define GRAPHICS_GRAPH2D_RARE_ENE_H

#include "typedefs.h"


MIKUPAN_NAMESPACE_BEGIN

extern u_int pazz_textbl[];

void LoadRareEneTex();
void DrawRareEne_Sub(int mno, int dno, float *pos, int tblno, int ani, int al);
void DrawRareEne();


MIKUPAN_NAMESPACE_END
#endif // GRAPHICS_GRAPH2D_RARE_ENE_H
