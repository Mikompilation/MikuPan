#include "sglight.h"
#include "common.h"
#include "typedefs.h"

#include "sce/libvu0.h"

#include "graphics/graph3d/libsg.h"
#include "graphics/graph3d/sgdma.h"
#include "graphics/graph3d/sglib.h"
#include "graphics/graph3d/sglight.h"

#include "cglm/mat4.h"
#include "graphics/graph3d/sgsu.h"

#include "mikupan/mikupan_logging_c.h"
#include "mikupan/rendering/mikupan_profiler.h"
#include "mikupan/rendering/mikupan_renderer.h"
#include "mikupan/mikupan_graph3d_compat.h"
#include "mikupan/ui/mikupan_ui.h"

#include <stdio.h>

static int stack_num = 0;
static int dbg_flg = 0;

static int write_counter;

static SgLIGHT *stack_light[9];
static sceVu0FVECTOR stack_eye[3];
static int stack_light_num[9];

#define GET_MESH_TYPE(intpointer) (char) ((char *) intpointer)[13]
#define GET_MESH_GLOOPS(intpointer) (int) ((char *) intpointer)[14]
#define POW2(x) ((x) * (x))
#define MIN(x, y) (x) < (y) ? (x) : (y)
#define MAX(x, y) (x) < (y) ? (y) : (x)

sceVu0FVECTOR vf0 = {0.0f, 0.0f, 0.0f, 1.0f};
sceVu0FVECTOR vf1 = {1.0f, 1.0f, 1.0f, -1.0f};
LMATRIX __work_matrix_0; // in [vf4:vf6], set in SetMaterialDataPrerender (from SPAD[0x110])
sceVu0FMATRIX __work_matrix_1; // in [vf7:vf10], set in SetMaterialDataPrerender (from SPAD[0x150])
sceVu0FMATRIX __work_matrix_2; // in [vf14:vf16], set in _CalcPointA
sceVu0FMATRIX __work_matrix_3; // in [vf20:vf22], set in asm_CalcSpotlight and read in _ReadDLightMtx and _CalcPointB

sceVu0FMATRIX __work_matrix_4; // in [vf23:vf25], read in _ReadSLightMtx which is unused so can be treated as a local in the remaining functions
sceVu0FMATRIX __work_matrix_5; // in [vf26:vf28], read in _ReadDColor which is unused so can be treated as a local in the remaining functions

sceVu0FVECTOR __work_vf12; // in vf12, set in _SetSpotPos (pos)
sceVu0FVECTOR __work_vf13; // in vf13, set in _SetSpotPos (dir)
sceVu0FVECTOR __work_vf17; // in vf17, set in _CalcPointA
sceVu0FVECTOR __work_vf18; // in vf18, set in asm_2__SetPreRenderTYPE0, asm_CalcSpotLight and _CalcPointB
sceVu0FVECTOR __work_vf19; // in vf19, set in load_abmient

//#define SCRATCHPAD ((u_char *)0x70000000)
#define SCRATCHPAD ((u_char *) ps2_virtual_scratchpad)

void SgPreRenderDbgOn()
{
    dbg_flg = 1;
}

void SgPreRenderDbgOff()
{
    dbg_flg = 0;
}

void QueSetMatrix(sceVu0FMATRIX m, int que, sceVu0FVECTOR v)
{
    sceVu0FVECTOR tmpv;

    _NormalizeVector(tmpv, v);

    m[0][que] = tmpv[0];
    m[1][que] = tmpv[1];
    m[2][que] = tmpv[2];
}

void QueSetMatrixNonNormalized(sceVu0FMATRIX m, int que, sceVu0FVECTOR v)
{
    m[0][que] = v[0];
    m[1][que] = v[1];
    m[2][que] = v[2];
}

void SgSetAmbient(sceVu0FVECTOR ambient)
{
    Vu0CopyVector(TAmbient, ambient);
}

void SgSetDefaultLight(sceVu0FVECTOR eye, SgLIGHT *p0, SgLIGHT *p1, SgLIGHT *p2)
{
    sceVu0FVECTOR nl;
    sceVu0FVECTOR tmpv;
    SgLIGHT *po[3];
    int i;

    _NormalizeVector(ieye, eye);

    Vu0MulVectorX(ieye, ieye, -1.0f);

    for (i = 0; i < 3; i++)
    {
        Vu0ZeroVector(DLightMtx[i]);
        Vu0ZeroVector(SLightMtx[i]);
        Vu0ZeroVector(DColorMtx[i]);
        Vu0ZeroVector(SColorMtx[i]);
    }

    po[0] = p0;
    po[1] = p1;
    po[2] = p2;

    for (i = 0; i < 3; i++)
    {
        if (po[i] != NULL)
        {
            _NormalizeVector(nl, po[i]->direction);
            Vu0CopyVector(DColorMtx[i], po[i]->diffuse);
            Vu0CopyVector(SColorMtx[i], po[i]->specular);
            QueSetMatrix(DLightMtx, i, nl);
            Vu0AddVector(tmpv, ieye, nl);
            _NormalizeVector(tmpv, tmpv);
            QueSetMatrix(SLightMtx, i, tmpv);
        }
        else
        {
            Vu0ZeroVector(DColorMtx[i]);
            Vu0ZeroVector(SColorMtx[i]);
        }
    }

    DColorMtx[0][3] = 1.0f;
}

static void _SetColorMtx(sceVu0FMATRIX dc, sceVu0FMATRIX sc, sceVu0FVECTOR am, sceVu0FVECTOR* v)
{
    sceVu0FVECTOR vf12, vf13, vf14, vf19;

    vf19[0] = (v[0][0] * am[0] * am[3]) + (v[3][0] * dc[0][3]);
    vf19[1] = (v[0][1] * am[1] * am[3]) + (v[3][1] * dc[0][3]);
    vf19[2] = (v[0][2] * am[2] * am[3]) + (v[3][2] * dc[0][3]);
    vf19[3] = dc[0][3];

    __work_matrix_5[0][0] = dc[0][0] * v[1][0] * dc[0][3];
    __work_matrix_5[0][1] = dc[0][1] * v[1][1] * dc[0][3];
    __work_matrix_5[0][2] = dc[0][2] * v[1][2] * dc[0][3];

    __work_matrix_5[1][0] = dc[1][0] * v[1][0] * dc[0][3];
    __work_matrix_5[1][1] = dc[1][1] * v[1][1] * dc[0][3];
    __work_matrix_5[1][2] = dc[1][2] * v[1][2] * dc[0][3];
    __work_matrix_5[1][3] = v[1][3];

    __work_matrix_5[2][0] = dc[2][0] * v[1][0] * dc[0][3];
    __work_matrix_5[2][1] = dc[2][1] * v[1][1] * dc[0][3];
    __work_matrix_5[2][2] = dc[2][2] * v[1][2] * dc[0][3];

    vf12[0] = sc[0][0] * v[2][0] * sc[1][3];
    vf12[1] = sc[0][1] * v[2][1] * sc[1][3];
    vf12[2] = sc[0][2] * v[2][2] * sc[1][3];
    vf12[3] = v[2][3];

    vf13[0] = sc[1][0] * v[2][0] * sc[1][3];
    vf13[1] = sc[1][1] * v[2][1] * sc[1][3];
    vf13[2] = sc[1][2] * v[2][2] * sc[1][3];
    vf13[3] = v[1][3];

    vf14[0] = sc[2][0] * v[2][0] * sc[1][3];
    vf14[1] = sc[2][1] * v[2][1] * sc[1][3];
    vf14[2] = sc[2][2] * v[2][2] * sc[1][3];
    vf14[3] = sc[2][3];

    vf19[3] = sc[2][3]; // ?

    // Move __work_matrix_5 into VU MEM+0x1A0
    // Move vf12 into VU MEM+0x1D0
    // Move vf13 into VU MEM+0x1E0
    // Move vf14 into VU MEM+0x1F0
    // Move vf19 into VU MEM+0x200
}

void _ReadDLightMtx(sceVu0FMATRIX tmp)
{
    memcpy(tmp, __work_matrix_3, sizeof(LMATRIX));
}

static void _ReadSLightMtx(sceVu0FMATRIX tmp)
{
    memcpy(tmp, __work_matrix_4, sizeof(LMATRIX));
}

void _ReadDColor(sceVu0FMATRIX tmp)
{
    memcpy(tmp, __work_matrix_5, sizeof(LMATRIX));
}

static int Tim2CalcBufWidth(int psm, int w)
{
    switch (psm)
    {
        case SCE_GS_PSMT8H:
        case SCE_GS_PSMT4HL:
        case SCE_GS_PSMT4HH:
        case SCE_GS_PSMCT32:
        case SCE_GS_PSMCT24:
        case SCE_GS_PSMZ32:
        case SCE_GS_PSMZ24:
        case SCE_GS_PSMCT16:
        case SCE_GS_PSMCT16S:
        case SCE_GS_PSMZ16:
        case SCE_GS_PSMZ16S:
            return (w + 63) / 64;
            break;
        case SCE_GS_PSMT8:
        case SCE_GS_PSMT4:
            w = (w + 63) / 64;

            if (w & 1)
            {
                w++;
            }
            return w;
            break;
    }

    return 0;
}

int GetDecay()
{
    u_int tmp = 0;

    //__asm__ volatile("\n\
    //    cfc2    %0,$vi13\n\
    //    ": :"r"(tmp)
    //);

    return tmp;
}

static inline void _SetDevay_asm(u_int decay)
{
    //__asm__ volatile("\n\
    //    ctc2    %0,$vi13\n\
    //    ": :"r"(decay)
    //);
}

static void SetDecay(u_int decay)
{
    int count;
    // u_int decay;

    count = 0;

    while (decay != 0)
    {
        decay >>= 1;
        count++;
    }

    _SetDevay_asm(count);
}

float SetMaxColor255(sceVu0FVECTOR dcol, sceVu0FVECTOR col)
{
    float div;

    if (col[0] > col[1])
    {
        if (col[0] > col[2])
        {
            div = col[0] / 255.0f;
        }
        else
        {
            div = col[2] / 255.0f;
        }
    }
    else if (col[1] > col[2])
    {
        div = col[1] / 255.0f;
    }
    else
    {
        div = col[2] / 255.0f;
    }

    if (div == 0.0f)
    {
        div = 1.0f;
    }

    sceVu0DivVectorXYZ(dcol, col, div);

    return div;
}

void SetBWLight(sceVu0FVECTOR col)
{
    float bwcolor;

    if (loadbw_flg != 0)
    {
        bwcolor = (col[0] + col[1] + col[2]) / 3.0f;
        col[0] = col[1] = col[2] = bwcolor;
    }
}

void SetMaterialPointer()
{
    SgLightParallelp = (SgVULightParallel *) &SCRATCHPAD[0x300];
    SgLightSpotp = (SgVULightSpot *) &SCRATCHPAD[0x370];
    SgLightPointp = (SgVULightPoint *) &SCRATCHPAD[0x3f0];
}

void ClearMaterialCache(HeaderSection *hs)
{
    int i;

    for (i = 0; i < hs->materials; i++)
    {
        GetMaterialPtr(hs, i)->vifcode[0] = -1;
    }
}

void SetMaterialDataVU(u_int *prim)
{
    static int old_tag_buf = -1;
    qword *base;
    SgMaterialC *pmatC;
    static SgMaterialC *old_pmatC = NULL;
    int i;
    int qwc;

    if (old_tag_buf != tagswap)
    {
        ClearMaterialCache((HeaderSection *) sgd_top_addr);
        old_tag_buf = tagswap;
    }

    pmatC = (SgMaterialC *) MikuPan_GetHostPointer(prim[2]);

    if (pmatC->cache_on >= 0)
    {
        if (pmatC->Point.num == SgPointGroupNum
            && pmatC->Spot.num == SgSpotGroupNum)
        {
            if (pmatC->Point.num != 0)
            {
                for (i = 0; i < 3; i++)
                {
                    if (pmatC->Point.lnum[i] != SgPointGroup[0].lnum[i])
                    {
                        goto label;
                    }
                }
            }

            if (SgSpotGroupNum != 0)
            {
                for (i = 0; i < 3; i++)
                {
                    if (pmatC->Spot.lnum[i] != SgSpotGroup[0].lnum[i])
                    {
                        goto label;
                    }
                }
            }

            if (pmatC == old_pmatC)
            {
                MikuPan_UploadCachedSgMaterialLighting(pmatC);
                return;
            }

            old_pmatC = pmatC;

            MikuPan_UploadCachedSgMaterialLighting(pmatC);

            AppendDmaTag(pmatC->tagd_addr, pmatC->qwc);
            FlushModel(0);

            return;
        }
    }

label:
    base = (qword *) getObjWrk();

    old_pmatC = pmatC;
    pmatC->tagd_addr =
        (u_int) MikuPan_GetPs2OffsetFromHostPointer(base) & 0x0fffffff;
    qwc = 0;

    if (SgSpotGroupNum != 0)
    {
        SgLightSpotp = (SgVULightSpot *) base;
        base += 8;
        qwc += 8;
    }

    if (SgPointGroupNum != 0)
    {
        SgLightPointp = (SgVULightPoint *) base;
        base += 8;
        qwc += 8;
    }

    SgLightParallelp = (SgVULightParallel *) base;
    qwc += 8;

    pmatC->qwc = qwc;

    SetMaterialData(prim);

    if (SgSpotGroupNum != 0)
    {
        SgLightSpotp->SpotVif[0] = SCE_VIF1_SET_NOP(0);
        SgLightSpotp->SpotVif[1] = SCE_VIF1_SET_NOP(0);
        SgLightSpotp->SpotVif[2] = SCE_VIF1_SET_STCYCL(4, 4, 0);
        SgLightSpotp->SpotVif[3] = SCE_VIF1_SET_UNPACK(
            0x35, 7, V4_32, 0);// VIF1_TOPS=0x35, count=7, V4_32, irq=0

        pmatC->Spot.lnum[0] = SgSpotGroup[0].lnum[0];
        pmatC->Spot.lnum[1] = SgSpotGroup[0].lnum[1];
        pmatC->Spot.lnum[2] = SgSpotGroup[0].lnum[2];
    }

    pmatC->Spot.num = SgSpotGroupNum;

    if (SgPointGroupNum != 0)
    {
        SgLightPointp->PointVif[0] = SCE_VIF1_SET_NOP(0);
        SgLightPointp->PointVif[1] = SCE_VIF1_SET_NOP(0);
        SgLightPointp->PointVif[2] = SCE_VIF1_SET_STCYCL(4, 4, 0);
        SgLightPointp->PointVif[3] = SCE_VIF1_SET_UNPACK(
            0x3c, 7, V4_32, 0);// VIF1_TOPS=0x3c, count=7, V4_32, irq=0

        pmatC->Point.lnum[0] = SgPointGroup[0].lnum[0];
        pmatC->Point.lnum[1] = SgPointGroup[0].lnum[1];
        pmatC->Point.lnum[2] = SgPointGroup[0].lnum[2];
    }

    pmatC->Point.num = SgPointGroupNum;

    SgLightParallelp->ParallelVif[0] = SCE_VIF1_SET_NOP(0);
    SgLightParallelp->ParallelVif[1] = SCE_VIF1_SET_NOP(0);
    SgLightParallelp->ParallelVif[2] = SCE_VIF1_SET_STCYCL(4, 4, 0);
    SgLightParallelp->ParallelVif[3] = SCE_VIF1_SET_UNPACK(
        0x2e, 7, V4_32, 0);// VIF1_TOPS=0x2e, count=7, V4_32, irq=0

    AppendDmaBuffer(qwc);
    FlushModel(0);
}

void SetMaterialPointerCoord()
{
    SgLightCoordp = (SgVULightCoord *) &SCRATCHPAD[0x1a0];
}

void SetMaterialPointerCoordVU()
{
    SgLightCoordp = (SgVULightCoord *) &((u_char *) getObjWrk())[0x1a0];
}

inline static void load_abmient(sceVu0FVECTOR amb)
{
    memcpy(__work_vf19, amb, sizeof(sceVu0FVECTOR));
}

void SetMaterialDataPrerender()
{
    memcpy(__work_matrix_0, &SCRATCHPAD[0x110], sizeof(LMATRIX));
    memcpy(__work_matrix_1, &SCRATCHPAD[0x150], sizeof(sceVu0FMATRIX));

    load_abmient(SgLightParallelp->Parallel_Ambient);
}

void SetMaterialData(u_int *prim)
{
    int i;
    int j;
    SgMaterialC *pmatC;
    sceVu0FVECTOR *dvec;
    float max_color;

    //pmatC = (SgMaterialC *)prim[2];
    pmatC = (SgMaterialC *) MikuPan_GetHostAddress(prim[2]);
    pmatC->cache_on = 1;

    if (pmatC->primtype != 0)
    {
        TAmbient[3] = 128.0f;
        DColorMtx[0][3] = 192.0f;
        SColorMtx[1][3] =
            (pmatC->Specular[0] + pmatC->Specular[1] + pmatC->Specular[2])
            * 43.0f;
    }
    else
    {
        TAmbient[3] = 255.0f;
        DColorMtx[0][3] = 255.0f;
        SColorMtx[1][3] =
            (pmatC->Specular[0] + pmatC->Specular[1] + pmatC->Specular[2])
            * 86.0f;
    }

    SColorMtx[2][3] = 255.0f;
    dvec = (sceVu0FVECTOR *) &SCRATCHPAD[0x620];

    Vu0MulVectorXYZ(*dvec, TAmbient, pmatC->Ambient);
    Vu0AddVector(*dvec, *dvec, pmatC->Emission);
    SetBWLight(*dvec);
    Vu0MulVectorX(SgLightParallelp->Parallel_Ambient, *dvec, TAmbient[3]);

    SgLightParallelp->Parallel_Ambient[3] = SColorMtx[2][3];

    for (i = 0; i < SgInfiniteNum; i++)
    {
        Vu0MulVectorXYZ(*dvec, DColorMtx[i], pmatC->Diffuse);
        SetBWLight(*dvec);
        Vu0MulVectorX(SgLightParallelp->Parallel_DColor[i], *dvec,
                      DColorMtx[0][3]);
        Vu0MulVectorXYZ(*dvec, SColorMtx[i], pmatC->Specular);
        SetBWLight(*dvec);
        Vu0MulVectorX(SgLightParallelp->Parallel_SColor[i], *dvec,
                      SColorMtx[1][3]);
    }

    for (; i < 3; i++)
    {
        Vu0ZeroVector(SgLightParallelp->Parallel_DColor[i]);
        Vu0ZeroVector(SgLightParallelp->Parallel_SColor[i]);
    }

    SgLightParallelp->Parallel_DColor[0][3] = pmatC->Diffuse[3];
    SgLightParallelp->Parallel_SColor[0][3] = pmatC->Diffuse[3];

    pmatC->Point.num = SgPointGroupNum;

    for (i = 0; i < SgPointGroupNum; i++)
    {
        SgPOINTGROUP *ppg = &SgPointGroup[i];

        for (j = 0; j < 3; j++)
        {
            Vu0MulVectorXYZW(*dvec, ppg->DColorMtx[j], pmatC->Diffuse);
            SetBWLight(*dvec);
            Vu0MulVectorX(*dvec, *dvec, DColorMtx[0][3]);

            max_color = SetMaxColor255(SgLightPointp->Point_DColor[j], *dvec);

            Vu0MulVectorXYZW(*dvec, ppg->SColorMtx[j], pmatC->Specular);
            SetBWLight(*dvec);
            Vu0MulVectorX(SgLightPointp->Point_SColor[j], *dvec,
                          SColorMtx[1][3] / max_color);

            SgLightPointp->Point_btimes[j] = ppg->btimes[j] * max_color;
            pmatC->Point.lnum[j] = ppg->lnum[j];
        }
    }

    pmatC->Spot.num = SgSpotGroupNum;

    for (i = 0; i < SgSpotGroupNum; i++)
    {
        SgSPOTGROUP *spg = &SgSpotGroup[i];

        for (j = 0; j < 3; j++)
        {
            Vu0MulVectorXYZW(*dvec, spg->DColorMtx[j], pmatC->Diffuse);
            SetBWLight(*dvec);
            Vu0MulVectorX(*dvec, *dvec, DColorMtx[0][3]);

            max_color = SetMaxColor255(SgLightSpotp->Spot_DColor[j], *dvec);

            Vu0MulVectorXYZW(*dvec, spg->SColorMtx[j], pmatC->Specular);
            SetBWLight(*dvec);
            Vu0MulVectorX(SgLightSpotp->Spot_SColor[j], *dvec,
                          SColorMtx[1][3] / max_color);

            SgLightSpotp->Spot_btimes[j] = spg->btimes[j] * max_color;
            pmatC->Spot.lnum[j] = spg->lnum[j];
        }
    }

    MikuPan_UploadSgMaterialLighting(pmatC,
                                      SgLightParallelp,
                                      SgLightPointp,
                                      SgPointGroupNum,
                                      SgPointGroup[0].lnum,
                                      SgLightSpotp,
                                      SgSpotGroupNum,
                                      SgSpotGroup[0].lnum);
}

static void _SetDLight(sceVu0FMATRIX m0)
{
    memcpy(__work_matrix_3, m0, sizeof(LMATRIX));
    /* Sends m0 to VU mem+0x140 */
}

static void _SetSLight(sceVu0FMATRIX m0)
{
    memcpy(__work_matrix_4, m0, sizeof(LMATRIX));
    /* Sends m0 to VU mem+0x170 */
}

void SetPointGroup()
{
    int i;
    int gnum;
    int gcount;
    SgLIGHT *TmpLight;
    SgPOINTGROUP *TmpGroup;

    if (SgPointNum == 0)
    {
        SgPointGroupNum = 0;
        return;
    }

    for (i = 0; i < SgPointNum; i++)
    {
        TmpLight = &SgPointLight[i];

        if (TmpLight->Enable == 0 && TmpLight->SEnable == 0)
        {
            Vu0CopyVector(SgLightCoordp->Point_pos[1], TmpLight->pos);
            Vu0CopyVector(SgLightCoordp->Point_pos[2], TmpLight->pos);

            SgLightCoordp->Point_pos[1][3] = SgLightCoordp->Point_pos[2][3] =
                10000000.0f;
            break;
        }
    }

    Vu0ZeroVector(SgPointGroup[0].DColorMtx[1]);
    Vu0ZeroVector(SgPointGroup[0].DColorMtx[2]);

    Vu0ZeroVector(SgPointGroup[0].SColorMtx[1]);
    Vu0ZeroVector(SgPointGroup[0].SColorMtx[2]);

    SgPointGroup[0].btimes[1] = 0.0f;
    SgPointGroup[0].btimes[2] = 0.0f;

    gnum = gcount = 0;

    for (i = 0; i < SgPointNum; i++)
    {
        TmpLight = &SgPointLight[i];
        TmpGroup = &SgPointGroup[gnum];

        if (TmpLight->Enable != 0 || TmpLight->SEnable != 0)
        {
            continue;
        }

        Vu0CopyVector(SgLightCoordp->Point_pos[gcount], TmpLight->pos);
        Vu0CopyVector(SgLightCoordp->Point_eyevec, ieye);

        if (TmpLight->intens != 0.0f)
        {
            SgLightCoordp->Point_pos[gcount][3] =
                1.0f
                / (TmpLight->power * TmpLight->intens
                   * (TmpLight->diffuse[0] + TmpLight->diffuse[1]
                      + TmpLight->diffuse[2]));
        }
        else
        {
            SgLightCoordp->Point_pos[gcount][3] = 0.0f;
        }

        TmpGroup->btimes[gcount] = TmpLight->power;

        Vu0CopyVector(TmpGroup->DColorMtx[gcount], TmpLight->diffuse);
        Vu0CopyVector(TmpGroup->SColorMtx[gcount], TmpLight->specular);

        TmpGroup->lnum[gcount] = i;

        gcount++;

        if (gcount > 2)
        {
            gcount = 0;
            gnum++;

            if (gnum > 0)
            {
                i = SgPointNum + 100;
            }
        }
    }
}

void SetSpotGroup(sceVu0FMATRIX wlmtx)
{
    int i;
    int gnum;
    int gcount;
    sceVu0FVECTOR dtmp;
    sceVu0FVECTOR stmp;
    SgLIGHT *TmpLight;
    SgSPOTGROUP *TmpGroup;

    if (SgSpotNum == 0)
    {
        SgSpotGroupNum = 0;
    }
    else
    {
        for (i = 0; i < 1; i++)
        {
            SgSPOTGROUP *TmpGroup = &SgSpotGroup[i];

            SgLightCoordp->Spot_intens[1] = 100.0f;
            SgLightCoordp->Spot_intens[2] = 100.0f;
            SgLightCoordp->Spot_intens_b[1] = 1.0f;
            SgLightCoordp->Spot_intens_b[2] = 1.0f;

            Vu0CopyVector(SgLightCoordp->Spot_pos[1], SgSpotLight->pos);
            Vu0CopyVector(SgLightCoordp->Spot_pos[2], SgSpotLight->pos);

            Vu0ZeroVector(SgLightCoordp->Spot_WDLightMtx[1]);
            Vu0ZeroVector(SgLightCoordp->Spot_WDLightMtx[2]);
            Vu0ZeroVector(SgLightCoordp->Spot_SLightMtx[0]);
            Vu0ZeroVector(SgLightCoordp->Spot_SLightMtx[1]);
            Vu0ZeroVector(SgLightCoordp->Spot_SLightMtx[2]);

            TmpGroup->btimes[0] = 1.0f;
            TmpGroup->btimes[1] = 1.0f;
            TmpGroup->btimes[2] = 1.0f;

            Vu0ZeroVector(TmpGroup->DColorMtx[1]);
            Vu0ZeroVector(TmpGroup->DColorMtx[2]);
            Vu0ZeroVector(TmpGroup->SColorMtx[1]);
            Vu0ZeroVector(TmpGroup->SColorMtx[2]);
        }

        gnum = gcount = 0;

        for (i = 0; i < SgSpotNum; i++)
        {
            TmpLight = &SgSpotLight[i];
            TmpGroup = &SgSpotGroup[gnum];

            if (TmpLight->Enable != 0 || TmpLight->SEnable != 0)
            {
                continue;
            }

            Vu0CopyVector(SgLightCoordp->Spot_pos[gcount], TmpLight->pos);

            SgLightCoordp->Spot_intens[gcount] = TmpLight->intens;
            SgLightCoordp->Spot_intens_b[gcount] = TmpLight->intens_b;

            Vu0CopyVector(SgLightCoordp->Spot_WDLightMtx[gcount],
                          TmpLight->direction);

            if (wlmtx != NULL)
            {
                Vu0LoadMatrix(wlmtx);
                Vu0ApplyVectorInline(dtmp, TmpLight->direction);
                Vu0ApplyVectorInline(stmp, ieye);
            }
            else
            {
                Vu0CopyVector(dtmp, TmpLight->direction);
                Vu0CopyVector(stmp, ieye);
            }

            Vu0SubVector(stmp, dtmp, stmp);
            QueSetMatrix(SgLightCoordp->Spot_SLightMtx, gcount, dtmp);

            TmpGroup->btimes[gcount] = TmpLight->btimes;

            Vu0CopyVector(TmpGroup->DColorMtx[gcount], TmpLight->diffuse);
            Vu0CopyVector(TmpGroup->SColorMtx[gcount], TmpLight->specular);

            TmpGroup->lnum[gcount] = i;

            gcount++;

            if (gcount > 2)
            {
                gcount = 0;
                gnum++;

                if (gnum > 0)
                {
                    i = SgSpotNum + 100;
                }
            }
        }
    }
}

void SetLightData(SgCOORDUNIT *cp0, SgCOORDUNIT *cp1)
{
    MIKUPAN_PERF_SCOPE(PERF_SECT_LIGHT_SETUP);
    int i;
    sceVu0FMATRIX tmp = {0};
    sceVu0FMATRIX tmp2;
    sceVu0FVECTOR tmpv = {0};
    sceVu0FVECTOR tmpv2;
    sceVu0FVECTOR scale = {0};

    if (cp1 == NULL)
    {
        for (i = 0; i < 3; i++)
        {
            tmpv[0] = cp0->lwmtx[0][i];
            tmpv[1] = cp0->lwmtx[1][i];
            tmpv[2] = cp0->lwmtx[2][i];

            scale[i] = SgRSqrtf(tmpv[0] * tmpv[0] + tmpv[1] * tmpv[1]
                                + tmpv[2] * tmpv[2]);

            _NormalizeVector(tmpv, tmpv);

            tmp[0][i] = tmpv[0];
            tmp[1][i] = tmpv[1];
            tmp[2][i] = tmpv[2];
        }

        Vu0CopyMatrix(*(sceVu0FMATRIX *) &SCRATCHPAD[0x150], cp0->lwmtx);
        Vu0CopyMatrix(*(sceVu0FMATRIX *) &SCRATCHPAD[0x110], tmp);

        _MulRotMatrix(SgLightCoordp->Parallel_DLight, DLightMtx, tmp);
        _MulRotMatrix(SgLightCoordp->Parallel_SLight, SLightMtx, tmp);

        SetPointGroup();
        sceVu0UnitMatrix(tmp);

        for (i = 0; i < 3; i++)
        {
            tmp[i][0] = cp0->lwmtx[0][i] * scale[i];
            tmp[i][1] = cp0->lwmtx[1][i] * scale[i];
            tmp[i][2] = cp0->lwmtx[2][i] * scale[i];
        }

        SetSpotGroup(tmp);
    }
    else
    {
        for (i = 0; i < SgSpotNum; i++)
        {
            SgSpotLight[i].SEnable = 0;
        }

        for (i = 0; i < SgPointNum; i++)
        {
            SgPointLight[i].SEnable = 0;
        }

        sceVu0UnitMatrix(tmp);
        Vu0CopyMatrix(*(sceVu0FMATRIX *) &SCRATCHPAD[0x150], tmp);
        Vu0CopyMatrix(*(sceVu0FMATRIX *) &SCRATCHPAD[0x110], tmp);
        Vu0CopyMatrix(SgLightCoordp->Parallel_DLight, DLightMtx);
        Vu0CopyMatrix(SgLightCoordp->Parallel_SLight, SLightMtx);
        SetPointGroup();
        SetSpotGroup(NULL);
    }

    *((int *) &SCRATCHPAD[0x64]) = SgSpotGroupNum;
    *((int *) &SCRATCHPAD[0x68]) = SgPointGroupNum;
}

void SgSetInfiniteLights(sceVu0FVECTOR eye, SgLIGHT *lights, int num)
{
    SgInfiniteLight = lights;
    SgInfiniteNum = num;

    switch (num)
    {
        case 0:
            SgSetDefaultLight(eye, NULL, NULL, NULL);
            break;
        case 1:
            SgSetDefaultLight(eye, &lights[0], NULL, NULL);
            break;
        case 2:
            SgSetDefaultLight(eye, &lights[0], &lights[1], NULL);
            break;
        default:
            SgSetDefaultLight(eye, &lights[0], &lights[1], &lights[2]);
            break;
    }
}

void SgSetPointLights(SgLIGHT *lights, int num)
{
    int i;

    SgPointLight = lights;
    SgPointNum = num;

    for (i = 0; i < num; i++)
    {
        if (lights[i].Enable == 0)
        {
            lights[i].SEnable = 0;
        }
    }

    MikuPan_OnSgSetPointLights(lights, num);
}

void SgSetSpotLights(SgLIGHT *lights, int num)
{
    int i;

    SgSpotLight = lights;
    SgSpotNum = num;

    for (i = 0; i < num; i++)
    {
        if (lights[i].Enable == 0)
        {
            _NormalizeVector(lights[i].direction, lights[i].direction);

            lights[i].intens_b = 1.0f / (1.0f - lights[i].intens);
            lights[i].btimes = lights[i].power;
            lights[i].SEnable = 0;
        }
    }

    MikuPan_OnSgSetSpotLights(lights, num);
}

void PushLight()
{
    sceVu0FVECTOR eye;

    if (stack_num < 3)
    {
        stack_light[stack_num * 3 + 0] = SgInfiniteLight;
        stack_light[stack_num * 3 + 1] = SgPointLight;
        stack_light[stack_num * 3 + 2] = SgSpotLight;

        Vu0MulVectorX(eye, ieye, -1.0f);
        Vu0CopyVector(stack_eye[stack_num], eye);

        stack_light_num[stack_num * 3 + 0] = SgInfiniteNum;
        stack_light_num[stack_num * 3 + 1] = SgPointNum;
        stack_light_num[stack_num * 3 + 2] = SgSpotNum;

        stack_num++;
    }
}

void PopLight()
{
    sceVu0FVECTOR eye;

    if (stack_num != 0)
    {
        stack_num--;

        SgInfiniteLight = stack_light[stack_num * 3 + 0];
        SgPointLight = stack_light[stack_num * 3 + 1];
        SgSpotLight = stack_light[stack_num * 3 + 2];

        Vu0CopyVector(eye, stack_eye[stack_num]);

        SgInfiniteNum = stack_light_num[stack_num * 3 + 0];
        SgPointNum = stack_light_num[stack_num * 3 + 1];
        SgSpotNum = stack_light_num[stack_num * 3 + 2];

        SgSetInfiniteLights(eye, SgInfiniteLight, SgInfiniteNum);
        SgSetPointLights(SgPointLight, SgPointNum);
        SgSetSpotLights(SgSpotLight, SgSpotNum);
    }
}

void ClearLightStack()
{
    stack_num = 0;
}

static void _CalcPointA(sceVu0FMATRIX grc, float *grm, float *len)
{
    sceVu0FVECTOR* wk0 = __work_matrix_0; // in [vf4:vf6]
    sceVu0FVECTOR* grm1 = (sceVu0FVECTOR*)grm; // in [vf4:vf6]
    float *lPos = __work_vf12; // in vf12
    float *lDir = __work_vf13; // in vf13
    sceVu0FVECTOR vf14, vf15, vf16, vf17;
    sceVu0FVECTOR vf23, vf24, vf25, vf26;

    vf14[0] = grc[0][0] - lPos[0];
    vf14[1] = grc[0][1] - lPos[1];
    vf14[2] = grc[0][2] - lPos[2];

    vf15[0] = grc[1][0] - lPos[0];
    vf15[1] = grc[1][1] - lPos[1];
    vf15[2] = grc[1][2] - lPos[2];

    vf16[0] = grc[2][0] - lPos[0];
    vf16[1] = grc[2][1] - lPos[1];
    vf16[2] = grc[2][2] - lPos[2];

    vf26[0] = (wk0[0][0] * lDir[0]) + (wk0[1][0] * lDir[1]) + (wk0[2][0] * lDir[2]);
    vf26[1] = (wk0[0][1] * lDir[0]) + (wk0[1][1] * lDir[1]) + (wk0[2][1] * lDir[2]);
    vf26[2] = (wk0[0][2] * lDir[0]) + (wk0[1][2] * lDir[1]) + (wk0[2][2] * lDir[2]);

    // These 2 actually use vf01, but it's set to [1, 1, 1, -1]
    // in SgPreRender, by calling Set12Register
    vf23[0] = POW2(vf14[0]) + POW2(vf14[1]) + POW2(vf14[2]);
    vf23[1] = POW2(vf15[0]) + POW2(vf15[1]) + POW2(vf15[2]);
    vf23[2] = POW2(vf16[0]) + POW2(vf16[1]) + POW2(vf16[2]);

    vf17[0] = (vf26[0] * vf14[0]) + (vf26[1] * vf14[1]) + (vf26[2] * vf14[2]);
    vf17[1] = (vf26[0] * vf15[0]) + (vf26[1] * vf15[1]) + (vf26[2] * vf15[2]);
    vf17[2] = (vf26[0] * vf16[0]) + (vf26[1] * vf16[1]) + (vf26[2] * vf16[2]);

    __work_vf17[0] = MAX(vf17[0] * grm1[0][0], 0.0f);
    __work_vf17[1] = MAX(vf17[1] * grm1[0][1], 0.0f);
    __work_vf17[2] = MAX(vf17[2] * grm1[0][2], 0.0f);

    memcpy(__work_matrix_3, &grm1[1][0], sizeof(LMATRIX));
    memcpy(__work_matrix_2, &grm1[4][0], sizeof(LMATRIX));

    //vf27[0] = 1.0f / vf23[0];
    //vf27[1] = 1.0f / vf23[1];
    //vf27[2] = 1.0f / vf23[2];

    // vf27 is also set by this
    len[0] = 1.0f / vf23[0];
    len[1] = 1.0f / vf23[1];
    len[2] = 1.0f / vf23[2];
    // len[3] = vf27[3]; // undefined, vf27[3] is not set here ever
}

static void _CalcPointB(sceVu0FVECTOR len)
{
    sceVu0FVECTOR* wk0 = __work_matrix_3; // in [vf20:vf22]
    sceVu0FVECTOR* wk1 = __work_matrix_2; // in [vf14:vf16]
    float *wk18 = __work_vf18; // in vf18
    float x0, y0, z0, x1, y1, z1;

    x0 = MIN(len[0] * __work_vf17[0], 1.0f);
    y0 = MIN(len[1] * __work_vf17[1], 1.0f);
    z0 = MIN(len[2] * __work_vf17[2], 1.0f);

    // pow8
    x1 = POW2(x0);
    y1 = POW2(x0);
    z1 = POW2(x0);
    x1 *= x1;
    y1 *= y1;
    z1 *= z1;
    x1 *= x1;
    y1 *= y1;
    z1 *= z1;

    wk18[0] = (wk0[0][0] * x0) + (wk0[1][0] * y0) + (wk0[2][0] * z0) + (wk1[0][0] * x1) + (wk1[1][0] * y1) + (wk1[2][0] * z1) + wk18[0];
    wk18[1] = (wk0[0][1] * x0) + (wk0[1][1] * y0) + (wk0[2][1] * z0) + (wk1[0][1] * x1) + (wk1[1][1] * y1) + (wk1[2][1] * z1) + wk18[1];
    wk18[2] = (wk0[0][2] * x0) + (wk0[1][2] * y0) + (wk0[2][2] * z0) + (wk1[0][2] * x1) + (wk1[1][2] * y1) + (wk1[2][2] * z1) + wk18[2];
}

void CalcPointLight()
{
    sceVu0FVECTOR len = {0};
    static float max_len = 0.0f;

    if (SgPointGroupNum > 0)
    {
        _CalcPointA(SgLightCoordp->Point_pos, SgLightPointp->Point_btimes, len);

        if (len[0] < SgLightCoordp->Point_pos[0][3])
        {
            len[0] = 0.0f;
        }

        if (len[1] < SgLightCoordp->Point_pos[1][3])
        {
            len[1] = 0.0f;
        }

        if (len[2] < SgLightCoordp->Point_pos[2][3])
        {
            len[2] = 0.0f;
        }

        _CalcPointB(len);
    }
}

inline static void asm_CalcSpotLight(LMATRIX cdata, sceVu0FMATRIX mdata)
{
    sceVu0FVECTOR *wk0 = __work_matrix_0; // in [vf4:vf6]
    float *lPos = __work_vf12; // in vf12
    float *lDir = __work_vf13; // in vf13
    float *wk18 = __work_vf18; // in vf18
    sceVu0FVECTOR vf14, vf15, vf16, vf17;
    sceVu0FVECTOR vf23, vf24, vf25, vf26;
    LMATRIX dLgtMtx;

    vf14[0] = cdata[0][0] - lPos[0];
    vf14[1] = cdata[0][1] - lPos[1];
    vf14[2] = cdata[0][2] - lPos[2];

    vf15[0] = cdata[1][0] - lPos[0];
    vf15[1] = cdata[1][1] - lPos[1];
    vf15[2] = cdata[1][2] - lPos[2];

    vf16[0] = cdata[2][0] - lPos[0];
    vf16[1] = cdata[2][1] - lPos[1];
    vf16[2] = cdata[2][2] - lPos[2];

    vf26[0] = (wk0[0][0] * lDir[0]) + (wk0[1][0] * lDir[1]) + (wk0[2][0] * lDir[2]);
    vf26[1] = (wk0[0][1] * lDir[0]) + (wk0[1][1] * lDir[1]) + (wk0[2][1] * lDir[2]);
    vf26[2] = (wk0[0][2] * lDir[0]) + (wk0[1][2] * lDir[1]) + (wk0[2][2] * lDir[2]);

    dLgtMtx[0][0] = cdata[5][0] * vf14[0];
    dLgtMtx[0][1] = cdata[5][1] * vf14[1];
    dLgtMtx[0][2] = cdata[5][2] * vf14[2];

    dLgtMtx[1][0] = cdata[6][0] * vf15[0];
    dLgtMtx[1][1] = cdata[6][1] * vf15[1];
    dLgtMtx[1][2] = cdata[6][2] * vf15[2];

    dLgtMtx[2][0] = cdata[7][0] * vf16[0];
    dLgtMtx[2][1] = cdata[7][1] * vf16[1];
    dLgtMtx[2][2] = cdata[7][2] * vf16[2];

    vf23[0] = 1.0f / (POW2(vf14[0]) + POW2(vf14[1]) + POW2(vf14[2]));
    vf23[1] = 1.0f / (POW2(vf15[0]) + POW2(vf15[1]) + POW2(vf15[2]));
    vf23[2] = 1.0f / (POW2(vf16[0]) + POW2(vf16[1]) + POW2(vf16[2]));

    vf17[0] = MAX(dLgtMtx[0][0] + dLgtMtx[0][1] + dLgtMtx[0][2], 0.0f);
    vf17[1] = MAX(dLgtMtx[1][0] + dLgtMtx[1][1] + dLgtMtx[1][2], 0.0f);
    vf17[2] = MAX(dLgtMtx[2][0] + dLgtMtx[2][1] + dLgtMtx[2][2], 0.0f);

    vf17[0] = MAX((POW2(vf17[0]) * vf23[0]) - cdata[3][0], 0.0f) * cdata[4][0];
    vf17[1] = MAX((POW2(vf17[1]) * vf23[1]) - cdata[3][1], 0.0f) * cdata[4][1];
    vf17[2] = MAX((POW2(vf17[2]) * vf23[2]) - cdata[3][2], 0.0f) * cdata[4][2];

    vf24[0] = MAX((vf26[0] * vf14[0]) + (vf26[1] * vf14[1]) + (vf26[2] * vf14[2]), 0.0f);
    vf24[1] = MAX((vf26[0] * vf15[0]) + (vf26[1] * vf15[1]) + (vf26[2] * vf15[2]), 0.0f);
    vf24[2] = MAX((vf26[0] * vf16[0]) + (vf26[1] * vf16[1]) + (vf26[2] * vf16[2]), 0.0f);

    vf24[0] = MIN(vf24[0] * vf23[0] * mdata[0][0], 1.0f);
    vf24[1] = MIN(vf24[1] * vf23[1] * mdata[0][1], 1.0f);
    vf24[2] = MIN(vf24[2] * vf23[2] * mdata[0][2], 1.0f);

    // POW8
    vf25[0] = POW2(vf24[0]);
    vf25[1] = POW2(vf24[1]);
    vf25[2] = POW2(vf24[2]);
    vf25[0] *= vf25[0];
    vf25[1] *= vf25[1];
    vf25[2] *= vf25[2];
    vf25[0] *= vf25[0];
    vf25[1] *= vf25[1];
    vf25[2] *= vf25[2];

    vf24[0] *= vf17[0];
    vf24[1] *= vf17[1];
    vf24[2] *= vf17[2];
    vf25[0] *= vf17[0];
    vf25[1] *= vf17[1];
    vf25[2] *= vf17[2];

    wk18[0] += (mdata[1][0] * vf24[0]) + (mdata[2][0] * vf24[1]) + (mdata[3][0] * vf24[2]) + (mdata[4][0] * vf25[0]) + (mdata[5][0] * vf25[1]) + (mdata[6][0] * vf25[2]);
    wk18[1] += (mdata[1][1] * vf24[0]) + (mdata[2][1] * vf24[1]) + (mdata[3][1] * vf24[2]) + (mdata[4][1] * vf25[0]) + (mdata[5][1] * vf25[1]) + (mdata[6][1] * vf25[2]);
    wk18[2] += (mdata[1][2] * vf24[0]) + (mdata[2][2] * vf24[1]) + (mdata[3][2] * vf24[2]) + (mdata[4][2] * vf25[0]) + (mdata[5][2] * vf25[1]) + (mdata[6][2] * vf25[2]);
}

void CalcSpotLight()
{
    if (SgSpotGroupNum > 0)
    {
        asm_CalcSpotLight(SgLightCoordp->Spot_pos, &SgLightSpotp->Spot_btimes);
    }
}

void SgReadLights(void *sgd_top, void *light_top, float *Ambient,
                  SgLIGHT *Ilight, int imax, SgLIGHT *Plight, int pmax,
                  SgLIGHT *Slight, int smax)
{
    int num;
    int i;
    u_int *prim;
    u_int *pk;
    sceVu0FVECTOR *pvec;
    sceVu0FVECTOR interest;
    sceVu0FVECTOR tmpvec;
    SgCOORDUNIT *cp;
    float scale;
    float cc;

    if (sgd_top == NULL && light_top == NULL)
    {
        return;
    }

    cp = NULL;

    if (sgd_top != NULL)
    {
        //cp = ((HeaderSection *)sgd_top)->coordp;
        cp = GetCoordP((HeaderSection *)sgd_top);
    }

    if (cp != NULL)
    {
        scale =
            SgSqrtf(Vu0DotProduct(cp->lwmtx[0], cp->lwmtx[0]));
        scale +=
            SgSqrtf(Vu0DotProduct(cp->lwmtx[1], cp->lwmtx[1]));
        scale +=
            SgSqrtf(Vu0DotProduct(cp->lwmtx[2], cp->lwmtx[2]));
        scale /= 3.0f;
    }
    else
    {
        scale = 1.0f;
    }

    pk = (u_int *) &((HeaderSection *) light_top)->primitives;
    //pk = GetPrimitiveAddr(light_top, 0);

    if (light_top == NULL)
    {
        pk = (u_int *) &((HeaderSection *) sgd_top)->primitives;
        //pk = GetPrimitiveAddr(sgd_top, 0);
    }

    if (Ilight != NULL)
    {
        Ilight->num = 0;
    }

    if (Plight != NULL)
    {
        Plight->num = 0;
    }

    if (Slight != NULL)
    {
        Slight->num = 0;
    }

    Vu0ZeroVector(Ambient);

    //prim = (u_int *)*pk;
    prim = (u_int *) MikuPan_GetHostPointer((int) *pk);

    while (prim != NULL)
    {
        if (prim[1] == 11)
        {
            num = prim[3];

            switch (prim[2])
            {
                case 0:
                    if (imax < num)
                    {
                        num = imax;
                    }

                    pvec = (sceVu0FVECTOR *) &prim[4];

                    if (Ilight != NULL)
                    {
                        Ilight->num = num;

                        for (i = 0; i < num; i++)
                        {
                            Vu0CopyVector(Ilight[i].diffuse, pvec[0]);
                            Vu0CopyVector(Ilight[i].specular, pvec[0]);

                            Ilight[i].ambient[0] = 0.0f;
                            Ilight[i].ambient[1] = 0.0f;
                            Ilight[i].ambient[2] = 0.0f;
                            Ilight[i].ambient[3] = 0.0f;

                            Vu0CopyVector(Ilight[i].direction, pvec[1]);

                            Ilight[i].Enable = 0;

                            pvec += 2;
                        }
                    }
                    break;
                case 1:
                    if (pmax < num)
                    {
                        num = pmax;
                    }

                    pvec = (sceVu0FVECTOR *) &prim[4];

                    if (Plight != NULL)
                    {
                        Plight->num = num;

                        for (i = 0; i < num; i++)
                        {
                            Vu0CopyVector(Plight[i].diffuse, pvec[0]);
                            Vu0CopyVector(Plight[i].specular, pvec[0]);

                            Plight[i].intens = pvec[0][3] * scale;
                            Plight[i].ambient[0] = 0.0f;
                            Plight[i].ambient[1] = 0.0f;
                            Plight[i].ambient[2] = 0.0f;
                            Plight[i].ambient[3] = 0.0f;

                            if (cp != NULL)
                            {
                                sceVu0ApplyMatrix(Plight[i].pos, cp->lwmtx,
                                                  pvec[1]);
                            }
                            else
                            {
                                Vu0CopyVector(Plight[i].pos, pvec[1]);
                            }

                            Plight[i].power = _TransPPower(scale);
                            Plight[i].ambient[0] = scale;
                            Plight[i].Enable = 0;

                            pvec += 2;
                        }
                    }
                    break;
                case 2:
                    if (smax < num)
                    {
                        num = smax;
                    }

                    pvec = (sceVu0FVECTOR *) &prim[4];

                    if (Slight != NULL)
                    {
                        Slight->num = num;

                        for (i = 0; i < num; i++)
                        {
                            Vu0CopyVector(Slight[i].diffuse, pvec[0]);
                            Vu0CopyVector(Slight[i].specular, pvec[0]);

                            Slight[i].ambient[0] = 0.0f;
                            Slight[i].ambient[1] = 0.0f;
                            Slight[i].ambient[2] = 0.0f;
                            Slight[i].ambient[3] = 0.0f;

                            if (cp != NULL)
                            {
                                Vu0CopyVector(tmpvec, pvec[1]);
                                tmpvec[3] = 1.0f;

                                sceVu0ApplyMatrix(Slight[i].pos, cp->lwmtx,
                                                  tmpvec);

                                Vu0CopyVector(tmpvec, pvec[2]);
                                tmpvec[3] = 1.0f;

                                sceVu0ApplyMatrix(interest, cp->lwmtx, tmpvec);
                                sceVu0SubVector(Slight[i].direction,
                                                Slight[i].pos, interest);
                            }
                            else
                            {
                                Vu0CopyVector(Slight[i].pos, pvec[1]);
                                Slight[i].pos[3] = 1.0f;

                                sceVu0SubVector(Slight[i].direction, pvec[1],
                                                pvec[2]);
                            }

                            cc = SgCosfd(pvec[1][3]);

                            Slight[i].intens = cc * cc;
                            Slight[i].power = _TransSPower(scale);
                            Slight[i].ambient[0] = scale;
                            Slight[i].ambient[1] = pvec[1][3];
                            Slight[i].Enable = 0;

                            pvec += 3;
                        }
                    }
                    break;
                case 3:
                    if (Ambient != NULL)
                    {
                        pvec = (sceVu0FVECTOR *) &prim[4];
                        Vu0CopyVector(Ambient, pvec[0]);
                    }
                    break;
            }
        }

        //prim = (u_int *)*prim;
        prim = GetNextProcUnitHeaderPtr(prim);
    }
}

u_int *GetNextUnpackAddr(u_int *prim)
{
    while (1)
    {
        if ((*prim & 0x60000000) == 0x60000000)
        {
            return prim;
        }

        prim++;
    }
}

inline static void Vu0RowMajorMatrixMultiply(sceVu0FVECTOR normal,
                                            sceVu0FVECTOR vertex)
{
    sceVu0FVECTOR *wk0 = __work_matrix_1; // in [vf7:vf10]

    memcpy(__work_vf13, normal, sizeof(sceVu0FVECTOR));

    __work_vf12[0] = (wk0[0][0] * vertex[0]) + (wk0[1][0] * vertex[1]) + (wk0[2][0] * vertex[2]) + (wk0[3][0] * vertex[3]);
    __work_vf12[1] = (wk0[0][1] * vertex[0]) + (wk0[1][1] * vertex[1]) + (wk0[2][1] * vertex[2]) + (wk0[3][1] * vertex[3]);
    __work_vf12[2] = (wk0[0][2] * vertex[0]) + (wk0[1][2] * vertex[1]) + (wk0[2][2] * vertex[2]) + (wk0[3][2] * vertex[3]);
    __work_vf12[3] = (wk0[0][3] * vertex[0]) + (wk0[1][3] * vertex[1]) + (wk0[2][3] * vertex[2]) + (wk0[3][3] * vertex[3]);
}


inline static void Vu0LoadVectorRegisterVF18(sceVu0FVECTOR first)
{
    memcpy(__work_vf18, first, sizeof(sceVu0FVECTOR));
}

inline static void Vu0ClampColors(sceVu0FVECTOR pcol)
{
    pcol[0] = __work_vf18[0] = MIN(__work_vf18[0], __work_vf19[3]);
    pcol[1] = __work_vf18[1] = MIN(__work_vf18[1], __work_vf19[3]);
    pcol[2] = __work_vf18[2] = MIN(__work_vf18[2], __work_vf19[3]);
    pcol[3] = __work_vf18[3] = MIN(__work_vf18[3], __work_vf19[3]);
    pcol[0] /= 255.0f;
    pcol[1] /= 255.0f;
    pcol[2] /= 255.0f;
}

void SetPreRenderTYPE0(int gloops, u_int *prim)
{
    int i;
    int j;
    int loops;
    float *vp;
    sceVu0FVECTOR normal;
    sceVu0FVECTOR vertex;
    sceVu0FVECTOR first;
    sceVu0FVECTOR pcol = {0.0f, 0.0f, 0.0f, 0.0f};

    first[0] = 0.0f;
    first[1] = 0.0f;
    first[2] = 0.0f;
    first[3] = 0.0f;
    normal[3] = 1.0f;
    vertex[3] = 1.0f;

    vp = (float *) &vuvnprim[14];

    i = ((short *) prim)[11];

    if (i == 0)
    {
        return;
    }

    prim = &prim[i];

    for (j = 0; j < gloops; j++)
    {
        prim = GetNextUnpackAddr(prim);

        loops = ((u_char *) prim)[2];

        prim = &prim[1];

        for (i = 0; i < loops; i++)
        {
            if (prim[0] != 1)
            {
                vertex[0] = vp[0];
                vertex[1] = vp[1];
                vertex[2] = vp[2];

                normal[0] = vp[3];
                normal[1] = vp[4];
                normal[2] = vp[5];

                Vu0RowMajorMatrixMultiply(normal, vertex);

                first[0] = ((float *) prim)[0];
                first[1] = ((float *) prim)[1];
                first[2] = ((float *) prim)[2];
                MikuPan_ApplyBlackWhiteToBaseVertexColor(first);

                Vu0LoadVectorRegisterVF18(first);

                CalcPointLight();
                CalcSpotLight();

                Vu0ClampColors(pcol);

                ((float *) prim)[0] = pcol[0];
                ((float *) prim)[1] = pcol[1];
                ((float *) prim)[2] = pcol[2];
            }

            vp += 6;
            prim = &prim[3];
        }
    }
}

void SetPreRenderTYPE2(int gloops, u_int *prim)
{
    int i;
    int j;
    int loops;
    float *vp;
    sceVu0FVECTOR normal;
    sceVu0FVECTOR vertex;
    sceVu0FVECTOR first;
    sceVu0FVECTOR pcol;

    first[0] = 0.0f;
    first[1] = 0.0f;
    first[2] = 0.0f;
    first[3] = 0.0f;
    normal[3] = 1.0f;
    vertex[3] = 1.0f;

    vp = (float *) &vuvnprim[14];

    i = ((short *) prim)[11];

    if (i == 0)
    {
        return;
    }

    prim = &prim[i];

    for (j = 0; j < gloops; j++)
    {
        prim = GetNextUnpackAddr(prim);

        loops = ((u_char *) prim)[2];

        prim = &prim[1];

        for (i = 0; i < loops; i++)
        {
            if (prim[0] != 1)
            {
                vertex[0] = vp[0];
                vertex[1] = vp[1];
                vertex[2] = vp[2];

                normal[0] = vp[3];
                normal[1] = vp[4];
                normal[2] = vp[5];

                Vu0RowMajorMatrixMultiply(normal, vertex);

                first[0] = ((float *) prim)[0];
                first[1] = ((float *) prim)[1];
                first[2] = ((float *) prim)[2];
                MikuPan_ApplyBlackWhiteToBaseVertexColor(first);

                Vu0LoadVectorRegisterVF18(first);

                CalcPointLight();
                CalcSpotLight();

                Vu0ClampColors(pcol);

                if (dbg_flg != 0)
                {
                    info_log("%f %f %f", pcol[0], pcol[1], pcol[2]);
                }

                ((float *) prim)[0] = pcol[0];
                ((float *) prim)[1] = pcol[1];
                ((float *) prim)[2] = pcol[2];

                if ((prim[0] & 0xffff) == 1)
                {
                    prim[0] = prim[0] + 1;
                }
            }

            vp += 6;
            prim = &prim[3];
        }
    }
}

void SetPreRenderTYPE2F(int gloops, u_int *prim)
{
    int i;
    int j;
    int loops;
    float *vp;
    float *np;
    sceVu0FVECTOR normal;
    sceVu0FVECTOR vertex;
    sceVu0FVECTOR first;
    sceVu0FVECTOR pcol;

    first[0] = 0.0f;
    first[1] = 0.0f;
    first[2] = 0.0f;
    first[3] = 0.0f;
    vertex[3] = 1.0f;
    normal[3] = 1.0f;

    np = (float *) &vuvnprim[14];

    vp = (float *) ((int64_t) np + ((short *) vuvnprim)[5] * 12);

    i = ((short *) prim)[11];

    if (i == 0)
    {
        return;
    }

    prim = &prim[i];

    for (j = 0; j < gloops; j++)
    {
        prim = GetNextUnpackAddr(prim);

        loops = ((u_char *) prim)[2];

        prim = &prim[1];

        normal[0] = np[0];
        normal[1] = np[1];
        normal[2] = np[2];

        np += 3;

        for (i = 0; i < loops; i++)
        {
            if (prim[0] != 1)
            {
                vertex[0] = vp[0];
                vertex[1] = vp[1];
                vertex[2] = vp[2];

                Vu0RowMajorMatrixMultiply(normal, vertex);

                first[0] = ((float *) prim)[0];
                first[1] = ((float *) prim)[1];
                first[2] = ((float *) prim)[2];
                MikuPan_ApplyBlackWhiteToBaseVertexColor(first);

                Vu0LoadVectorRegisterVF18(first);

                CalcPointLight();
                CalcSpotLight();

                Vu0ClampColors(pcol);

                if (dbg_flg != 0)
                {
                    info_log("%f %f %f", pcol[0], pcol[1], pcol[2]);
                }

                ((float *) prim)[0] = pcol[0];
                ((float *) prim)[1] = pcol[1];
                ((float *) prim)[2] = pcol[2];

                if ((prim[0] & 0xffff) == 1)
                {
                    prim[0] = prim[0] + 1;
                }
            }

            prim = &prim[3];
            vp += 3;
        }
    }
}

void SetPreRenderMeshData(u_int *prim)
{
    int gloops = GET_MESH_GLOOPS(prim);
    int mtype = GET_MESH_TYPE(prim);

    switch (mtype)
    {
        case 0x10:
            SetPreRenderTYPE0(gloops, prim);
            break;
        case 0x12:
            SetPreRenderTYPE2(gloops, prim);
            break;
        case 0x32:
            SetPreRenderTYPE2F(gloops, prim);
            break;
    }
}

static void _SetSpotPos(sceVu0FVECTOR pos, sceVu0FVECTOR dir)
{
    memcpy(__work_vf12, pos, sizeof(sceVu0FVECTOR));
    memcpy(__work_vf13, dir, sizeof(sceVu0FVECTOR));
}

static float _SpotInnerProduct(sceVu0FVECTOR bpos)
{
    float x, y, z;

    x = __work_vf13[0] * (__work_vf12[0] - bpos[0]);
    y = __work_vf13[1] * (__work_vf12[1] - bpos[1]);
    z = __work_vf13[2] * (__work_vf12[2] - bpos[2]);

    return x + y + z;
}

void SelectLight(u_int *prim)
{
    SgLIGHT *TmpLight;
    sceVu0FVECTOR *pvec;
    sceVu0FVECTOR minvec;
    sceVu0FVECTOR maxvec;
    sceVu0FVECTOR ominvec;
    sceVu0FVECTOR omaxvec;
    sceVu0FVECTOR *tmpvec;
    sceVu0FVECTOR plain;
    sceVu0FVECTOR interest;
    float maxpower[4];
    float colscale;
    float spotdir;
    float spotvalue[8];
    int maxnum[4];
    int i;
    int j;
    int k;

    if (SgSpotNum == 0 && SgPointNum == 0)
    {
        return;
    }

    tmpvec = (sceVu0FVECTOR *) &SCRATCHPAD[0x620];
    pvec = (sceVu0FVECTOR *) &prim[4];

    Vu0AddVector(*(sceVu0FVECTOR *) &SCRATCHPAD[0x6a0], *pvec,
                 *(sceVu0FVECTOR *) &prim[32]);
    Vu0MulVectorX(*(sceVu0FVECTOR *) &SCRATCHPAD[0x6a0],
                  *(sceVu0FVECTOR *) &SCRATCHPAD[0x6a0], 0.5f);
    Vu0LoadMatrix(lcp[prim[2]].lwmtx);

    for (i = 0; i < 8; i++)
    {
        Vu0ApplyVectorInline(tmpvec[i], pvec[i]);
    }

    Vu0ApplyVectorInline(*(sceVu0FVECTOR *) &SCRATCHPAD[0x6a0],
                         *(sceVu0FVECTOR *) &SCRATCHPAD[0x6a0]);

    maxpower[0] = maxpower[1] = maxpower[2] = 0.0f;
    maxnum[0] = maxnum[1] = maxnum[2] = -1;

    for (i = 0; i < SgPointNum; i++)
    {
        TmpLight = &SgPointLight[i];

        if (TmpLight->Enable == 0)
        {
            TmpLight->SEnable = 1;
            colscale = (TmpLight->diffuse[0] + TmpLight->diffuse[1]
                        + TmpLight->diffuse[2])
                       * TmpLight->power;
            TmpLight->spower =
                colscale
                / _CalcLen(TmpLight->pos,
                           *(sceVu0FVECTOR *) &SCRATCHPAD[0x6a0]);

            for (j = 0; j < 3; j++)
            {
                if (maxpower[j] < TmpLight->spower)
                {
                    for (k = 3; k > j; k--)
                    {
                        maxpower[k] = maxpower[k - 1];
                        maxnum[k] = maxnum[k - 1];
                    }

                    maxpower[j] = TmpLight->spower;
                    maxnum[j] = i;
                    break;
                }
            }
        }
    }

    SgPointGroupNum = 0;

    for (j = 0; j < 3; j++)
    {
        if (maxnum[j] >= 0)
        {
            SgPointLight[maxnum[j]].SEnable = 0;
            SgPointGroupNum = 1;
        }
    }

    maxpower[0] = maxpower[1] = maxpower[2] = 0.0f;
    maxnum[0] = maxnum[1] = maxnum[2] = -1;

    for (i = 0; i < SgSpotNum; i++)
    {
        TmpLight = &SgSpotLight[i];
        if (TmpLight->Enable == 0)
        {
            TmpLight->SEnable = 1;

            Vu0CopyVector(plain, TmpLight->direction);

            /// plain[3] is the dot product of direction and pos
            plain[3] = -Vu0DotProduct(plain, TmpLight->pos);

            Vu0AddVector(interest, TmpLight->pos, TmpLight->direction);

            spotdir = Vu0DotProduct(plain, interest) + plain[3];

            for (j = 0; j < 8; j++)
            {
                spotvalue[j] =
                    Vu0DotProduct(plain, tmpvec[j]) + plain[3];
                if (spotvalue[j] * spotdir < 0.0f)
                {
                    break;
                }
            }

            if (j != 8)
            {
                colscale = (TmpLight->diffuse[0] + TmpLight->diffuse[1]
                            + TmpLight->diffuse[2])
                           * TmpLight->power;
                TmpLight->spower =
                    colscale
                    / _CalcLen(TmpLight->pos,
                               *(sceVu0FVECTOR *) &SCRATCHPAD[0x6a0]);

                for (j = 0; j < 3; j++)
                {
                    if (maxpower[j] < TmpLight->spower)
                    {
                        for (k = 3; k > j; k--)
                        {
                            maxpower[k] = maxpower[k - 1];
                            maxnum[k] = maxnum[k - 1];
                        }

                        maxpower[j] = TmpLight->spower;
                        maxnum[j] = i;
                        break;
                    }
                }
            }
        }
    }

    SgSpotGroupNum = 0;

    for (j = 0; j < 3; j++)
    {
        if (maxnum[j] >= 0)
        {
            SgSpotLight[maxnum[j]].SEnable = 0;
            SgSpotGroupNum = 1;
        }
    }
}

void SgPreRenderPrim(u_int *prim)
{
    if (prim == NULL)
    {
        return;
    }

    while (prim[0] != 0)
    {
        switch (prim[1])
        {
            case 0:
                vuvnprim = prim;
                break;
            case 1:
                nextprim = prim[0];

                SetPreRenderMeshData(prim);
                break;
            case 2:
                SetMaterialData(prim);
                SetMaterialDataPrerender();

                if (dbg_flg != 0)
                {
                    info_log("PNum %d(%d) SNum %d(%d)", SgPointGroupNum,
                             SgPointNum, SgSpotGroupNum, SgSpotNum);

                    printVectorC(SgLightCoordp->Spot_pos[0], "pos0");
                    printVectorC(SgLightCoordp->Spot_pos[1], "pos1");
                    printVectorC(SgLightCoordp->Spot_pos[2], "pos2");
                    printVectorC(SgLightCoordp->Spot_intens, "intens");
                    printVectorC(SgLightCoordp->Spot_intens_b, "intens_b");
                    printLMatC(SgLightCoordp->Spot_WDLightMtx, "WDLightMtx");
                    printLMatC(SgLightCoordp->Spot_SLightMtx, "SLightMtx");
                    printVectorC(SgLightSpotp->Spot_btimes, "btimes");
                    printLMatC(SgLightSpotp->Spot_DColor, "DColor");
                    printLMatC(SgLightSpotp->Spot_SColor, "SColor");
                }
                break;
            case 3:
                Vu0CopyMatrix(*(sceVu0FMATRIX *) &SCRATCHPAD[0x430],
                              lcp[prim[2]].lwmtx);
                _MulMatrix(*(sceVu0FMATRIX *) &SCRATCHPAD[0x90], SgWSMtx,
                           *(sceVu0FMATRIX *) &SCRATCHPAD[0x430]);
                break;
            case 4:
                SelectLight(prim);
                SetLightData(&lcp[prim[2]], NULL);
                break;
        }

        prim = (u_int *) GetNextProcUnitHeaderPtr(prim);
    }
}

void SgPreRender(void *sgd_top, int pnum)
{
    int i;
    u_int *pk;
    HeaderSection *hs;

    if (sgd_top == NULL)
    {
        return;
    }

    MikuPan_OnSgPreRenderBegin(sgd_top);

    Set12Register();

    write_counter = 0;

    hs = (HeaderSection *) sgd_top;

    //pk = (u_int *)&hs->primitives;
    pk = GetPrimitiveAddr(hs, 0);
    lcp = GetCoordP(hs);
    blocksm = hs->blocks;

    SetMaterialPointer();
    SetMaterialPointerCoord();

    if (pnum < 0)
    {
        for (i = 1; i < blocksm; i++)
        {
            //SgPreRenderPrim((u_int *)pk[i]);
            SgPreRenderPrim(GetTopProcUnitHeaderPtr(hs, i));
        }
    }
    else if (pnum != 0)
    {
        //SgPreRenderPrim((u_int *)pk[pnum]);
        SgPreRenderPrim(GetTopProcUnitHeaderPtr(hs, pnum));
    }
}

void ClearPreRenderMeshData(u_int *prim)
{
    int i;
    int j;
    int loops;
    int gloops;
    int mtype;
    sceVu0FVECTOR first;

    mtype = ((char *) prim)[0xd];
    gloops = ((char *) prim)[0xe];

    i = ((short *) prim)[11];

    if (i == 0)
    {
        return;
    }

    prim = &prim[i];

    switch (mtype)
    {
        case 0x10:
            for (j = 0; j < gloops; j++)
            {
                prim = GetNextUnpackAddr(prim);

                loops = ((u_char *) prim)[2];

                prim = &prim[1];

                for (i = 0; i < loops; i++)
                {
                    if (prim[0] != 1)
                    {
                        prim[0] = 0;
                        prim[1] = 0;
                        prim[2] = 0;
                    }

                    prim = &prim[3];
                }
            }
            break;
        case 0x12:
        case 0x32:
            for (j = 0; j < gloops; j++)
            {
                prim = GetNextUnpackAddr(prim);

                loops = ((u_char *) prim)[2];

                prim = &prim[1];

                for (i = 0; i < loops; i++)
                {
                    if (prim[0] != 1)
                    {
                        prim[0] = 0;
                        prim[1] = 0;
                        prim[2] = 0;
                    }

                    prim = &prim[3];
                }
            }
            break;
    }
}

void SgClearPreRenderPrim(u_int *prim)
{
    if (prim == NULL)
    {
        return;
    }

    while (prim[0] != 0)
    {
        if (prim[1] == 1)
        {
            ClearPreRenderMeshData(prim);
        }

        //prim = (u_int *)prim[0];
        prim = (u_int *) GetNextProcUnitHeaderPtr(prim);
    }
}

void SgClearPreRender(void *sgd_top, int pnum)
{
    int i;
    u_int *pk;
    HeaderSection *hs;

    if (sgd_top == NULL)
    {
        return;
    }

    hs = (HeaderSection *) sgd_top;

    blocksm = hs->blocks;
    pk = (u_int *) &hs->primitives;

    SetMaterialPointer();
    SetMaterialPointerCoord();

    if (pnum < 0)
    {
        for (i = 1; i < blocksm; i++)
        {
            SgClearPreRenderPrim(GetTopProcUnitHeaderPtr(hs, i));
        }
    }
    else if (pnum != 0)
    {
        SgClearPreRenderPrim(GetTopProcUnitHeaderPtr(hs, pnum));
    }
}
