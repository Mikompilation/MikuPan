#include "sgsu.h"
#include "common.h"
#include "enums.h"
#include "typedefs.h"

#include <stdio.h>

#include "ee/eestruct.h"
#include "sgsup.h"

#include "graphics/graph3d/libsg.h"
#include "graphics/graph3d/sgcam.h"
#include "graphics/graph3d/sgdma.h"
#include "graphics/graph3d/sglib.h"
#include "graphics/graph3d/sglight.h"
#include "mikupan/mikupan_logging_c.h"
#include "mikupan/rendering/mikupan_renderer.h"

#define min(x, y) (((x) > (y))? (y): (x))

extern void DRAWTYPE2() __attribute__((section(".vutext")));
extern void DRAWTYPE2W() __attribute__((section(".vutext")));
extern void DRAWTYPE0() __attribute__((section(".vutext")));
extern void MULTI_PROLOGUE() __attribute__((section(".vutext")));

extern SgSourceChainTag SgSu_dma_start() __attribute__((section(".vutext")));
extern SgSourceChainTag SgSuP0_dma_start() __attribute__((section(".vutext")));
extern SgSourceChainTag SgSuP2_dma_start() __attribute__((section(".vutext")));
extern SgSourceChainTag SgSu_dma_starts() __attribute__((section(".vutext")));

#define Q12_4(i, f) (((i) << 4) | ((f) & 0xF))
#define SCRATCHPAD ((u_int *) ps2_virtual_scratchpad)

static int write_flg = 0;
static int write_counter = 0;
static int dbg_flg = 0;
static int debug_sign = 0;
static int o0 = 0;
static int o1 = 0;
sceVu0FVECTOR work_vf18;
sceVu0FVECTOR work_vf19;

void _AddColor(float *v)
{
    v[0] = work_vf18[0] + v[0];
    v[1] = work_vf18[1] + v[1];
    v[2] = work_vf18[2] + v[2];
}

void SgSuDebugOn()
{
    dbg_flg = 1;
}

void SgSuDebugOff()
{
    dbg_flg = 0;
}

void SetDebugSign(int num)
{
    debug_sign = num;
}

void PutDebugSign()
{
    static int frame = 0;
    qword *pedraw_buf;

    if (debug_sign != 0)
    {
        pedraw_buf = getObjWrk() + 1;

        ((u_long *) pedraw_buf)[0] = SCE_GIF_SET_TAG(
            2, SCE_GS_TRUE, SCE_GS_TRUE,
            SCE_GS_SET_PRIM(SCE_GS_PRIM_SPRITE, 1, 0, 0, 1, 0, 0, 0, 0),
            SCE_GIF_PACKED, 2);
        ((u_long *) pedraw_buf)[1] =
            0 | SCE_GS_RGBAQ << (0 * 4) | SCE_GS_XYZF2 << (1 * 4);

        pedraw_buf[1][0] = 0x80;
        pedraw_buf[1][1] = 0x80;
        pedraw_buf[1][2] = 0x80;
        pedraw_buf[1][3] = 0x80;

        pedraw_buf[2][0] = Q12_4(2048, 0);
        pedraw_buf[2][1] = Q12_4(2048, 0);
        pedraw_buf[2][2] = 400000;
        pedraw_buf[2][3] = 0;

        pedraw_buf[3][0] = 0x80;
        pedraw_buf[3][1] = 0x80;
        pedraw_buf[3][2] = 0x80;
        pedraw_buf[3][3] = 0x80;

        pedraw_buf[4][0] = Q12_4(2368, 0);
        pedraw_buf[4][1] = Q12_4(2160, 0);
        pedraw_buf[4][2] = 400000;
        pedraw_buf[4][3] = 0;

        AppendDmaBufferFromEndAddress(&pedraw_buf[5]);

        frame++;

        if (frame >= 20)
        {
            frame = 0;
            debug_sign = 0;
        }
    }
}

void SgSetVNBuffer(sceVu0FVECTOR *vnarray, int size)
{
    vertex_buffer = vnarray;
    normal_buffer = vnarray + size / 2;
    vnbuf_size = size / 2;
}

int CheckCoordCache(int cn)
{
    int i;

    if (ccahe.cache_on != -1 && ccahe.edge_check == edge_check)
    {
        if (ccahe.Point.num == SgPointGroupNum
            && ccahe.Spot.num == SgSpotGroupNum)
        {
            if (SgPointGroupNum != 0)
            {
                for (i = 0; i < 3; i++)
                {
                    if (ccahe.Point.lnum[i] != SgPointGroup[0].lnum[i])
                    {
                        goto label;
                    }
                }
            }

            if (SgSpotGroupNum != 0)
            {
                for (i = 0; i < 3; i++)
                {
                    if (ccahe.Spot.lnum[i] != SgSpotGroup[0].lnum[i])
                    {
                        goto label;
                    }
                }
            }

            for (i = 0; i < 4; i++)
            {
                if (lcp[cn].lwmtx[i][0] != lcp[ccahe.cn0].lwmtx[i][0]
                    || lcp[cn].lwmtx[i][1] != lcp[ccahe.cn0].lwmtx[i][1]
                    || lcp[cn].lwmtx[i][2] != lcp[ccahe.cn0].lwmtx[i][2]
                    || lcp[cn].lwmtx[i][3] != lcp[ccahe.cn0].lwmtx[i][3])
                {
                    goto label;
                }
            }

            return 1;
        }
    }
label:
    ccahe.cache_on = 1;
    ccahe.edge_check = edge_check;
    ccahe.cn0 = cn;

    if (SgPointGroupNum != 0)
    {
        ccahe.Point.lnum[0] = SgPointGroup[0].lnum[0];
        ccahe.Point.lnum[1] = SgPointGroup[0].lnum[1];
        ccahe.Point.lnum[2] = SgPointGroup[0].lnum[2];
    }

    if (SgSpotGroupNum != 0)
    {
        ccahe.Spot.lnum[0] = SgSpotGroup[0].lnum[0];
        ccahe.Spot.lnum[1] = SgSpotGroup[0].lnum[1];
        ccahe.Spot.lnum[2] = SgSpotGroup[0].lnum[2];
    }

    return 0;
}

void SetVU1Header()
{
    sceVu0FVECTOR *svec;
    sceVu0FVECTOR *dvec;
    int i;

    svec = (sceVu0FVECTOR *) SCRATCHPAD;
    dvec = (sceVu0FVECTOR *) getObjWrk();

    for (i = 0; i < 13; i++)
    {
        //__asm__ __volatile__("\n\
        //    lq $6,0x0(%0)\n\
        //    lq $7,0x10(%0)\n\
        //    ": : "r" (svec) : "$6", "$7"
        //);

        svec += 2;

        //__asm__ __volatile__("\n\
        //    sq $6,0x0(%0)\n\
        //    sq $7,0x10(%0)\n\
        //    ": : "r" (dvec) : "$6", "$7"
        //);

        dvec += 2;
    }

    AppendDmaBuffer(47);
    FlushModel(0);
}

void CalcVertexBuffer(u_int *prim)
{
    int i;
    int j;
    sceVu0FVECTOR *vpd;
    sceVu0FVECTOR *vps;
    VERTEXLIST *vli;

    if (prim[3] == 0)
    {
        return;
    }

    //vli = (VERTEXLIST *)lphead->pWeightedList;
    vli = (VERTEXLIST *) MikuPan_GetHostPointer(lphead->pWeightedList);

    if (vli == NULL)
    {
        return;
    }

    //vps = lphead->pWeightedVertex;
    vps = (sceVu0FVECTOR *) MikuPan_GetHostPointer(lphead->pWeightedVertex);
    vpd = vertex_buffer;

    for (i = 0; i < vli->list_num; i++)
    {
        load_matrix_0(lcp[vli->lists[i].cn0].workm);
        load_matrix_1(lcp[vli->lists[i].cn1].workm);

        for (j = 0; j < vli->lists[i].vIdx; j++, vpd += 1, vps += 2)
        {
            calc_skinned_position(*vpd, vps);
        }
    }

    //vps = lphead->pWeightedNormal;
    vps = (sceVu0FVECTOR *) MikuPan_GetHostPointer(lphead->pWeightedNormal);
    vpd = normal_buffer;

    vli = (VERTEXLIST *) &vli->lists[vli->list_num];

    for (i = 0; i < vli->list_num; i++)
    {
        load_matrix_0(lcp[vli->lists[i].cn0].workm);
        load_matrix_1(lcp[vli->lists[i].cn1].workm);

        for (j = 0; j < vli->lists[i].vIdx; j++, vpd += 1, vps += 2)
        {
            calc_skinned_normal(*vpd, vps);
        }
    }
}

u_int *SetVUVNData(u_int *prim)
{
    int i;
    VUVN_PRIM *vh;
    sceVu0FVECTOR *vp;
    sceVu0FVECTOR tmpvec;// unused

    vh = (VUVN_PRIM *) &prim[2];
    vp = (sceVu0FVECTOR *) getObjWrk();

    copy_skinned_data(vp, (float *) &prim[4], (float *) &prim[8]);

    vp += 2;
    prim += 12;

    for (i = 0; i < vh->vnum; i++, vp += 2, prim += 2)
    {
        copy_skinned_data(vp, (float *) MikuPan_GetHostPointer(prim[0]), (float *) MikuPan_GetHostPointer(prim[1]));
    }

    return (u_int *) vp;
}

u_int *SetVUVNDataPost(u_int *prim)
{
    int i;
    VUVN_PRIM *vh;
    sceVu0FVECTOR *vp;
    sceVu0FVECTOR tmpvec;
    char *cn;

    vh = (VUVN_PRIM *) &prim[2];
    vp = (sceVu0FVECTOR *) getObjWrk();

    copy_skinned_data(vp, (float *) &prim[4], (float *) &prim[8]);

    vp += 2;
    prim += 12;

    switch (vh->vtype)
    {
        case 2:

            if (MikuPan_GetHostAddress(lphead->pWeightedList) != 0)
            {
                for (i = 0; i < vh->vnum; i++, vp += 2, prim += 2)
                {
                    copy_skinned_data(vp, (float *) MikuPan_GetHostPointer(prim[0]), (float *) MikuPan_GetHostPointer(prim[1]));
                }
            }
            else
            {
                cn = (char *) MikuPan_GetHostPointer(prim[0]);

                load_matrix_0(lcp[cn[0x1c]].workm);
                load_matrix_1(lcp[cn[0x1d]].workm);

                for (i = 0; i < vh->vnum; vp += 2, prim += 2, i++)
                {
                    calc_skinned_data(vp, (float *) MikuPan_GetHostPointer(prim[0]), (float *) MikuPan_GetHostPointer(prim[1]));
                }
            }
            break;
        case 3:
            if (MikuPan_GetHostAddress(lphead->pWeightedList) != 0)
            {
                for (i = 0; i < vh->vnum; i++, vp += 2, prim += 2)
                {
                    copy_skinned_data(vp, (float *) MikuPan_GetHostPointer(prim[0]), (float *) MikuPan_GetHostPointer(prim[1]));
                }
            }
            else
            {
                for (i = 0; i < vh->vnum; i++, vp += 2, prim += 2)
                {
                    cn = (char *) MikuPan_GetHostPointer(prim[0]);

                    load_matrix_0(lcp[cn[0x1c]].workm);
                    load_matrix_1(lcp[cn[0x1d]].workm);
                    calc_skinned_data(vp, (float *) MikuPan_GetHostPointer(prim[0]), (float *) MikuPan_GetHostPointer(prim[1]));
                }
            }
            break;
        default:
            for (i = 0; i < vh->vnum; i++, vp += 2, prim += 2)
            {
                copy_skinned_data(vp, (float *) MikuPan_GetHostPointer(prim[0]), (float *) MikuPan_GetHostPointer(prim[1]));
            }
    }

    //MikuPan_RenderVertices((float*) getObjWrk(), vh->vnum);

    return (u_int *) vp;
}

void printTEX0(sceGsTex0 *tex0)
{
    info_log("TBP0 %x TBW %d PSM %x TW %d TH %d TCC %d\n", tex0->TBP0,
             tex0->TBW, tex0->PSM, tex0->TW, tex0->TH, tex0->TCC);

    info_log("TFX %d CBP %x CPSM %x CSM %d CSA %d CLD %x\n", tex0->TFX,
             tex0->CBP, tex0->CPSM, tex0->CSM, tex0->CSA, tex0->CLD);
}

void SetVUMeshData(u_int *prim)
{
    int mtype;
    u_int *read_p;

    mtype = ((char *) prim)[13];

    switch (mtype & (0x1 | 0x2 | 0x10 | 0x40 | 0x80))// 0xd3
    {
        case 0:
            read_p = SetVUVNData(vuvnprim);

            read_p[0] = 0x14000000 | ((u_int) DRAWTYPE0 >> 3);
            read_p[1] = 0x17000000;
            read_p[2] = 0x11000000;
            read_p[3] = 0x17000000;

            AppendDmaTag((u_int) &prim[4], prim[2]);
            AppendDmaBuffer(((u_char *) vuvnprim)[12]);
            FlushModel(0);
            break;
        case 2:
            read_p = SetVUVNData(vuvnprim);
            MikuPan_RenderMeshType0x2((struct SGDPROCUNITHEADER*)vuvnprim, (struct SGDPROCUNITHEADER*)prim);
            //MikuPan_RenderVertices((int64_t)getObjWrk() + 0x20, 20);

            read_p[0] = 0x14000000 | ((u_int) DRAWTYPE2 >> 3);
            read_p[1] = 0x17000000;
            read_p[2] = 0x11000000;
            read_p[3] = 0x17000000;

            AppendDmaTag((u_int) &prim[4], prim[2]);
            AppendDmaBuffer(((u_char *) vuvnprim)[12]);
            FlushModel(0);
            break;
        case 0x80:
            AppendDmaTag((u_int) &prim[4], prim[2]);
            AppendDmaTag((u_int) & ((u_char *) vuvnprim)[16],
                         ((u_char *) vuvnprim)[12]);

            read_p = (u_int *) getObjWrk();

            read_p[0] = 0x14000000 | ((u_int) DRAWTYPE0 >> 3);
            read_p[1] = 0x17000000;
            read_p[2] = 0x11000000;
            read_p[3] = 0x17000000;

            AppendDmaBuffer(1);
            FlushModel(0);
            break;
        case 0x82:
            MikuPan_RenderMeshType0x82(vuvnprim, prim);

            AppendDmaTag((u_int) &prim[4], prim[2]);
            AppendDmaTag((u_int) & ((u_char *) vuvnprim)[16],
                         ((u_char *) vuvnprim)[12]);

            read_p = (u_int *) getObjWrk();

            read_p[0] = 0x14000000 | ((u_int) DRAWTYPE2 >> 3);
            read_p[1] = 0x17000000;
            read_p[2] = 0x11000000;
            read_p[3] = 0x17000000;

            AppendDmaBuffer(1);
            FlushModel(0);
            break;
        case 0x42:
            AppendDmaTag((u_int) &prim[4], prim[2]);

            read_p = (u_int *) getObjWrk();

            read_p[0] = 0x14000000 | ((u_int) MULTI_PROLOGUE >> 3);
            read_p[1] = 0x17000000;
            read_p[2] = 0x11000000;
            read_p[3] = 0x17000000;

            AppendDmaBuffer(1);
            FlushModel(0);
            break;

        default:
            printf("Illegal Packet %d\n", mtype);
            break;
    }
}

void SetVUMeshDataPost(u_int *prim)
{
    int mtype;
    u_int *read_p;

    mtype = ((char *) prim)[13];

    switch (mtype & (0x1 | 0x2 | 0x10 | 0x40))// 0x53
    {
        case 0:
            read_p = SetVUVNDataPost(vuvnprim);

            read_p[0] = 0x14000000 | ((u_int) DRAWTYPE0 >> 3);
            read_p[1] = 0x17000000;
            read_p[2] = 0x11000000;
            read_p[3] = 0x17000000;

            AppendDmaTag((u_int) &prim[4], prim[2]);
            AppendDmaBuffer(((u_char *) vuvnprim)[12]);
            FlushModel(0);
            break;
        case 2:
            read_p = SetVUVNDataPost(vuvnprim);

            read_p[0] = 0x14000000 | ((u_int) DRAWTYPE2W >> 3);
            read_p[1] = 0x17000000;
            read_p[2] = 0x11000000;
            read_p[3] = 0x17000000;

            AppendDmaTag((u_int) &prim[4], prim[2]);
            AppendDmaBuffer(((u_char *) vuvnprim)[12]);
            FlushModel(0);
            break;
        case 0x42:
            AppendDmaTag((u_int) &prim[4], prim[2]);

            read_p = (u_int *) getObjWrk();

            read_p[0] = 0x14000000 | ((u_int) MULTI_PROLOGUE >> 3);
            read_p[1] = 0x17000000;
            read_p[2] = 0x11000000;
            read_p[3] = 0x17000000;

            AppendDmaBuffer(1);
            FlushModel(0);
            break;
        default:
            printf("Illegal Packet %d\n", mtype);
            break;
    }
}

void SetCoordData(u_int *prim)
{
    int j;
    float abs;
    SgCOORDUNIT *llp;

    if (prim[3] == 0)
    {
        return;
    }

    for (j = 1; j < blocksm - 1; j++)
    {
        llp = &lcp[j];

        Vu0CopyMatrix(llp->workm, llp->lwmtx);

        abs = SgCalcLen(llp->workm[0][0], llp->workm[1][0], llp->workm[2][0]);
        abs += SgCalcLen(llp->workm[0][1], llp->workm[1][1], llp->workm[2][1]);
        abs += SgCalcLen(llp->workm[0][2], llp->workm[1][2], llp->workm[2][2]);

        if (abs == 0.0f)
        {
            llp->workm[3][3] = 0.0f;
            continue;
        }

        llp->workm[3][3] = 3.0f / abs;
    }

    SetLightData(&lcp[prim[2]], &lcp[prim[2]]);
}

void SgSortUnitPrim(u_int *prim)
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
                SetVUMeshData(prim);
                break;
            case 2:
                SetMaterialDataVU(prim);
                break;
            case 4:
                if (CheckBoundingBox(prim) == 0)
                {
                    return;
                }

                write_coord++;

                SetMaterialPointerCoordVU();

                if (CheckCoordCache(prim[2]) == 0)
                {
                    SetLightData(&lcp[prim[2]], NULL);
                    SetVU1Header();
                }
                break;
            case 5:
                GsImageProcess(prim);
                break;
        }

        prim = GetNextProcUnitHeaderPtr(prim);
    }
}

void SgSortUnitPrimPost(u_int *prim)
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
                SetVUMeshDataPost(prim);
                break;
            case 2:
                SetMaterialDataVU(prim);
                break;
            case 3:
                SetMaterialPointerCoordVU();
                SetCoordData(prim);

                ccahe.cache_on = -1;

                SetVU1Header();
                CalcVertexBuffer(prim);

                write_coord++;
                break;
            case 5:
                GsImageProcess(prim);
                break;
        }

        prim = GetNextProcUnitHeaderPtr(prim);
    }
}

void SgSortPreProcess(u_int *prim)
{
    static u_int *old_pointer = NULL;
    static sceDmaTag tag[2][3];
    static sceDmaTag *tp;
    static qword base[3];
    static qword base3[6];
    static qword *base2;

    if (prim == NULL)
    {
        return;
    }

    while (prim[0] != 0)
    {
        switch (prim[1])
        {
            case 5:
                GsImageProcess(prim);
                break;
            case 10:
                LoadTRI2Files(prim);
                break;
            case 13:
                if (loadbw_flg != 0)
                {
                    LoadTRI2Files(prim);
                }
                break;
        }

        prim = GetNextProcUnitHeaderPtr(prim);
    }
}

SgCOORDUNIT *lcp = NULL;
PHEAD *lphead = NULL;
u_int nextprim = 0;
u_int *vuvnprim = NULL;
int blocksm = 0;
int write_coord = 0;

void AppendVUProgTag(u_int *prog)
{
    while (((SgSourceChainTag *) prog)->ID
           != 7) /* so long as it's not DmaEnd */
    {
        AppendDmaTag((u_int) (((SgSourceChainTag *) prog) + 1),
                     ((SgSourceChainTag *) prog)->QWC);
        prog = (u_int *) &(
            ((u_long *) prog)[((SgSourceChainTag *) prog)->QWC * 2 + 2]);
        FlushModel(0);
    }
}

void LoadSgProg(int load_prog)
{
    static u_int pk[4] = {
        0x0,
        0x0,
        0x3000060,
        0x20001D0,
    };
    static SgSourceChainTag starttag;

    if (vu_prog_num == load_prog)
    {
        return;
    }

    switch (load_prog)
    {
        case VUPROG_SG:
            AppendVUProgTag((u_int *) SgSu_dma_start);
            break;
        case VUPROG_SG_PRESET0:
            AppendVUProgTag((u_int *) SgSuP0_dma_start);
            break;
        case VUPROG_SG_PRESET2:
            AppendVUProgTag((u_int *) SgSuP2_dma_start);
            break;
        case VUPROG_SG_SHADOW:
            AppendVUProgTag((u_int *) SgSu_dma_starts);
            break;
    }

    AppendDmaTag((u_int) pk, 1);

    vu_prog_num = load_prog;

    FlushModel(0);
}

void SetUpSortUnit()
{
    u_int *datap;

    ccahe.cache_on = -1;

    if (SgPointNum != 0)
    {
        SgPointGroupNum = 1;
    }
    else
    {
        SgSpotGroupNum = 0;
    }

    if (SgSpotNum != 0)
    {
        SgSpotGroupNum = 1;
    }
    else
    {
        SgSpotGroupNum = 0;
    }

    SCRATCHPAD[0] = 0;
    SCRATCHPAD[1] = 0x11000000;
    SCRATCHPAD[2] = 0x01000404;
    SCRATCHPAD[3] = 0x6c2e0000;

    datap = &SCRATCHPAD[4];
    datap[12] = 0;
    datap[13] = 0x20364000;

    datap[14] = 0x41;
    datap[15] = 0;
    datap[16] = 0;
    datap[17] = 0x303e4000;

    datap[18] = 0x412;
    datap[19] = 0;

    datap[20] = 0x8000;

    datap[24] = 0x8000;
    datap[25] = 0x2036c000;

    datap[26] = 0x41;
    datap[27] = 0;
    datap[28] = 0x8000;
    datap[29] = 0x303ec000;

    datap[30] = 0x412;
    datap[31] = 0;

    Vu0CopyMatrix(*(sceVu0FMATRIX *) &datap[32], SgWSMtx);
    Vu0CopyMatrix(*(sceVu0FMATRIX *) &datap[48], SgCMtx);

    Vu0CopyVector(*(sceVu0FVECTOR *) &SCRATCHPAD[4], vf12reg[0]);
    Vu0CopyVector(*(sceVu0FVECTOR *) &SCRATCHPAD[8], vf12reg[1]);

    datap[8] = 0x60;
    datap[9] = 0x230;

    Vu0CopyVector(*(sceVu0FVECTOR *) &datap[96], fog_value);

    datap = (u_int *) getObjWrk();

    datap[0] = 0x30000000;
    datap[1] = 0;
    datap[2] = 0;
    ((float *) datap)[3] = 1.0f;
    ((float *) datap)[4] = 1.0f;
    datap[5] = 0;
    datap[6] = 0;
    datap[7] = 0;
    AppendDmaBuffer(2);
}

void SgSortUnit(void *sgd_top, int pnum)
{
    int i;
    u_int *pk;
    HeaderSection *hs;

    hs = (HeaderSection *) sgd_top;

    wscissor_flg = 0;
    lcp = GetCoordP(hs);

    if (((u_int) lcp & 0xf) != 0)
    {
        printf("SgSortUnit Data broken. %x lcp %x\n", sgd_top, lcp);
        return;
    }

    blocksm = hs->blocks;
    //lphead = (PHEAD *)hs->phead;
    lphead = (PHEAD *) MikuPan_GetHostAddress(hs->phead);

    sgd_top_addr = sgd_top;

    write_flg = 0;
    write_counter = 0;
    write_coord = 0;

    ClearMaterialCache(hs);
    SetUpSortUnit();
    LoadSgProg(VUPROG_SG);

    pk = (u_int *) &hs->primitives;

    if (pnum < 0)
    {
        SgSortPreProcess(GetTopProcUnitHeaderPtr(hs, 0));

        for (i = 1; i < blocksm - 1; i++)
        {
            SgSortUnitPrim(GetTopProcUnitHeaderPtr(hs, i));
        }

        //if ((u_int *)pk[i] != NULL)
        if (GetTopProcUnitHeaderPtr(hs, i) != NULL)
        {
            SgSortUnitPrimPost(GetTopProcUnitHeaderPtr(hs, i));
        }
    }
    else if (pnum == 0)
    {
        SgSortPreProcess(GetTopProcUnitHeaderPtr(hs, pnum));
    }
    else if (pnum == blocksm - 1)
    {
        SgSortUnitPrimPost(GetTopProcUnitHeaderPtr(hs, pnum));
    }
    else
    {
        SgSortUnitPrim(GetTopProcUnitHeaderPtr(hs, pnum));
    }
}

void SgSortUnitKind(void *sgd_top, int num)
{
    HeaderSection *hs;

    hs = (HeaderSection *) sgd_top;

    if (hs->kind & 1)
    {
        SgSortUnitP(sgd_top, num);
    }
    else
    {
        SgSortUnit(sgd_top, num);
    }
}

void _SetLWMatrix0(sceVu0FMATRIX m0)
{
    //__asm__ volatile ("\n\
    //    lqc2    $vf4,0(%0)\n\
    //    lqc2    $vf5,0x10(%0)\n\
    //    lqc2    $vf6,0x20(%0)\n\
    //    lqc2    $vf7,0x30(%0)\n\
    //    ": :"r"(m0)
    //);

    memcpy(work_matrix_0, m0, sizeof(sceVu0FMATRIX));
}

void _SetLWMatrix1(sceVu0FMATRIX m0)
{
    //__asm__ volatile ("\n\
    //    lqc2    $vf8,0(%0)\n\
    //    lqc2    $vf9,0x10(%0)\n\
    //    lqc2    $vf10,0x20(%0)\n\
    //    lqc2    $vf11,0x30(%0)\n\
    //    ": :"r"(m0)
    //);

    memcpy(work_matrix_1, m0, sizeof(sceVu0FMATRIX));
}

void _SetRotTransPersMatrix(sceVu0FMATRIX m0)
{
    memcpy(work_matrix_1, m0, sizeof(sceVu0FMATRIX));
}

void _CalcVertex(sceVu0FVECTOR *dp, float *v, float *n)
{
    sceVu0FVECTOR *wk0 = work_matrix_0; // in [vf4:vf7]

    dp[1][0] = n[0];
    dp[1][1] = n[1];
    dp[1][2] = n[2];
    dp[1][3] = n[3];

    dp[0][0] = (wk0[0][0] * v[0]) + (wk0[1][0] * v[1]) + (wk0[2][0] * v[2]) + (wk0[3][0] * v[3]);
    dp[0][1] = (wk0[0][1] * v[0]) + (wk0[1][1] * v[1]) + (wk0[2][1] * v[2]) + (wk0[3][1] * v[3]);
    dp[0][2] = (wk0[0][2] * v[0]) + (wk0[1][2] * v[1]) + (wk0[2][2] * v[2]) + (wk0[3][2] * v[3]);
    dp[0][3] = (wk0[0][3] * v[0]) + (wk0[1][3] * v[1]) + (wk0[2][3] * v[2]) + (wk0[3][3] * v[3]);
}

void _vfito0(int *v0)
{
    v0[0] = (int)min(work_vf18[0], work_vf19[0]);
    v0[1] = (int)min(work_vf18[1], work_vf19[1]);
    v0[2] = (int)min(work_vf18[2], work_vf19[2]);
    v0[3] = (int)min(work_vf18[3], work_vf19[3]);
}
