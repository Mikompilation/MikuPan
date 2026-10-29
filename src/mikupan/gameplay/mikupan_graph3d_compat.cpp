#include "mikupan/gameplay/mikupan_graph3d_compat.h"

#include <stdint.h>

#include "sce/libgraph.h"

#include "graphics/graph3d/sglib.h"
#include "graphics/graph3d/sglight.h"
#include "mikupan/mikupan_memory.h"
#include "mikupan/mikupan_types.h"
#include "mikupan/rendering/mikupan_meshcache.h"
#include "mikupan/rendering/mikupan_renderer.h"

void MikuPan_OnBlackWhiteClutTransition(u_int **previous_tri2_prim)
{
    /* Reset the original TRI2 packet shortcut and drop the mesh cache across
     * a B/W transition.
     */
    if (previous_tri2_prim != NULL)
    {
        *previous_tri2_prim = NULL;
    }

    MikuPan_MeshCache_Flush();
}


void MikuPan_UploadSgMaterialLighting(SgMaterialC *pmatC,
                                      SgVULightParallel *parallelp,
                                      SgVULightPoint *pointp,
                                      int point_group_count,
                                      const int point_lnum[3],
                                      SgVULightSpot *spotp,
                                      int spot_group_count,
                                      const int spot_lnum[3])
{
    const float *spot_intens = NULL;
    const float *spot_intens_b = NULL;

    if (pmatC == NULL || parallelp == NULL)
    {
        return;
    }

    if (SgLightCoordp != NULL)
    {
        spot_intens = SgLightCoordp->Spot_intens;
        spot_intens_b = SgLightCoordp->Spot_intens_b;
    }

    MikuPan_SetMaterial(&pmatC->Ambient,
                        &pmatC->Diffuse,
                        &pmatC->Specular,
                        &pmatC->Emission);

    MikuPan_SetBakedLighting(SgInfiniteNum,
                             parallelp->Parallel_Ambient,
                             parallelp->Parallel_DColor,
                             parallelp->Parallel_SColor,
                             point_group_count,
                             point_group_count != 0 ? point_lnum : NULL,
                             pointp != NULL ? pointp->Point_DColor : NULL,
                             pointp != NULL ? pointp->Point_SColor : NULL,
                             pointp != NULL ? pointp->Point_btimes : NULL,
                             spot_group_count,
                             spot_group_count != 0 ? spot_lnum : NULL,
                             spotp != NULL ? spotp->Spot_DColor : NULL,
                             spotp != NULL ? spotp->Spot_SColor : NULL,
                             spotp != NULL ? spotp->Spot_btimes : NULL,
                             spot_intens,
                             spot_intens_b);
}

void MikuPan_UploadCachedSgMaterialLighting(SgMaterialC *pmatC)
{
    qword *base;
    SgVULightSpot *spotp = NULL;
    SgVULightPoint *pointp = NULL;
    SgVULightParallel *parallelp;

    if (pmatC == NULL)
    {
        return;
    }

    base = (qword *)MikuPan_GetHostPointer(pmatC->tagd_addr);

    if (pmatC->Spot.num != 0)
    {
        spotp = (SgVULightSpot *)base;
        base += 8;
    }

    if (pmatC->Point.num != 0)
    {
        pointp = (SgVULightPoint *)base;
        base += 8;
    }

    parallelp = (SgVULightParallel *)base;

    MikuPan_UploadSgMaterialLighting(pmatC,
                                     parallelp,
                                     pointp,
                                     pmatC->Point.num,
                                     pmatC->Point.lnum,
                                     spotp,
                                     pmatC->Spot.num,
                                     pmatC->Spot.lnum);
}

void MikuPan_ApplyBlackWhiteToBaseVertexColor(sceVu0FVECTOR col)
{
    /* On PS2, B/W mode is effectively resolved after world lighting.
     * MikuPan's native mesh path seems to cache authored vertex colour
     * before that resolve, so run the same B/W light conversion used
     * by the original lighting helpers!
     */
    SetBWLight(col);
}


void MikuPan_OnSgPreRenderBegin(void *sgd_top)
{
    MikuPan_MeshCache_InvalidateSgd(sgd_top);
}

void MikuPan_ApplyShadowStrengthFromPs2Alpha(int alpha)
{
    MikuPan_SetShadowStrengthFromPs2Alpha(alpha);
}
