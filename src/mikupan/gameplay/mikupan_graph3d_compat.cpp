#include "mikupan/gameplay/mikupan_graph3d_compat.h"

#include <stdint.h>

#include "sce/libgraph.h"

#include "graphics/graph3d/sglib.h"
#include "graphics/graph3d/sglight.h"
#include "mikupan/mikupan_memory.h"
#include "mikupan/gs/mikupan_gs_c.h"
#include "mikupan/gs/mikupan_texture_manager_c.h"
#include "mikupan/mikupan_types.h"
#include "mikupan/rendering/mikupan_meshcache.h"
#include "mikupan/rendering/mikupan_renderer.h"

void MikuPan_OnBlackWhiteClutTransition(u_int **previous_tri2_prim)
{
    /* The same TEX0 can name colour and monochrome TRI2/CLUT uploads across
     * a B/W transition. Reset the original TRI2 packet shortcut, then drop the
     * host-side TEX0 and mesh caches so the next packet is mirrored with the
     * correct CLUT state.
     */
    if (previous_tri2_prim != NULL)
    {
        *previous_tri2_prim = NULL;
    }

    MikuPan_InvalidateTex0Cache();
    MikuPan_MeshCache_Flush();
}

void MikuPan_MirrorTri2PacketToHostGs(u_int *prim)
{
    if (prim == NULL)
    {
        return;
    }

    /* Mirror the selected TRI2 image into the host GS texture cache before the
     * original DMA packet is appended. Animated TRI2 streams must upload the
     * chosen frame after the frame-skip loop, otherwise frame 0 can reuse stale
     * texture/CLUT data when an animation wraps.
     */
    SGDTRI2FILEHEADER *tri2 = (SGDTRI2FILEHEADER *)prim;
    MikuPan_GsUpload(&tri2->gsli, (u_char *)&tri2[1]);

    /* Need to upload the CLUT data too, only if it is not PSMCT32. */
    if (tri2->gsli.bitbltbuf.DPSM != 0 /* PSMCT32 */)
    {
        uint8_t *img = (uint8_t *)&tri2[1];
        sceGsLoadImage *image_load = (sceGsLoadImage *)&img[tri2->gsli.giftag1.NLOOP * 0x10];
        uint8_t *image_color_data = (uint8_t *)(&image_load[1]);
        MikuPan_GsUpload(image_load, image_color_data);
    }
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
