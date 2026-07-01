#ifndef MIKUPAN_GRAPH3D_COMPAT_H
#define MIKUPAN_GRAPH3D_COMPAT_H

#include "common.h"
#include "typedefs.h"
#include "sce/libvu0.h"
#include "graphics/graph3d/sg_dat.h"
#include "graphics/graph3d/sglib.h"

#ifdef __cplusplus
extern "C" {
#endif

void MikuPan_OnBlackWhiteClutTransition(u_int **previous_tri2_prim);
void MikuPan_MirrorTri2PacketToHostGs(u_int *prim);
void MikuPan_ApplyBlackWhiteToBaseVertexColor(sceVu0FVECTOR col);
void MikuPan_UploadSgMaterialLighting(SgMaterialC *pmatC,
                                      SgVULightParallel *parallelp,
                                      SgVULightPoint *pointp,
                                      int point_group_count,
                                      const int point_lnum[3],
                                      SgVULightSpot *spotp,
                                      int spot_group_count,
                                      const int spot_lnum[3]);
void MikuPan_UploadCachedSgMaterialLighting(SgMaterialC *pmatC);
void MikuPan_OnSgPreRenderBegin(void *sgd_top);
void MikuPan_OnSgSetPointLights(SgLIGHT *lights, int num);
void MikuPan_OnSgSetSpotLights(SgLIGHT *lights, int num);
void MikuPan_ApplyShadowStrengthFromPs2Alpha(int alpha);

#ifdef __cplusplus
}
#endif

#endif /* MIKUPAN_GRAPH3D_COMPAT_H */
