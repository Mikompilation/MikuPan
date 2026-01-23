#include "sgsgd.h"
#include "common.h"
#include "typedefs.h"

#include "../../mikupan/mikupan_logging_c.h"

#include <stdio.h>

#include "sce/libgraph.h"

#include "graphics/graph3d/sg_dat.h"
#include "graphics/graph3d/sgdma.h"
#include "graphics/graph3d/sglib.h"

#include <mikupan/mikupan_memory.h>

static int post_process = 0;
static int PresetChk;

#define SVA_UNIQUE 0
#define SVA_COMMON 1
#define SVA_WEIGHTED 2

// Mesh Types
#define iMT_0 0x10
#define iMT_2 0x12
#define iMT_2F 0x32

// ProcUnitType
#define VUVN 0
#define MESH 1
#define MATERIAL 2
#define COORDINATE 3
#define BOUNDING_BOX 4
#define GS_IMAGE 5
#define TRI2 10
#define END 11
#define INVALID 12
#define MonotoneTRI2 13
#define StackTRI2 14

#define GET_MESH_TYPE(intpointer) (char)((char*)intpointer)[13]
#define GET_MESH_GLOOPS(intpointer) (int)((char*)intpointer)[14]

void MappingVUVNData(u_int *intpointer, HeaderSection *hs)
{
    int i;
    VUVN_PRIM *vh;
    PHEAD *ph;
    sceVu0FVECTOR *vp;
    sceVu0FVECTOR *np;

    // ph = (PHEAD *)hs->phead
    ph = (PHEAD *)GetPHead(hs);
    vh = (VUVN_PRIM *)&intpointer[2];

    intpointer += 12;

    switch (vh->vtype)
    {
    case SVA_UNIQUE:
        //vp = ph->pUniqVertex;
        //np = ph->pUniqNormal;
        vp = MikuPan_GetHostPointer(ph->pUniqVertex);
        np = MikuPan_GetHostPointer(ph->pUniqNormal);

        for (i = 0; i < vh->vnum; i++)
        {
            //intpointer[0] = intpointer[0] * 16 + (int)vp; intpointer++;
            //intpointer[0] = intpointer[0] * 16 + (int)np; intpointer++;

            /// In order to make it 32bits compatible, the base reference needs to removed
            /// so now instead if will contain the offset from its vp/np
            intpointer[0] = intpointer[0] * 16 + MikuPan_GetPs2OffsetFromHostPointer(vp); intpointer++;
            intpointer[0] = intpointer[0] * 16 + MikuPan_GetPs2OffsetFromHostPointer(np); intpointer++;
        }
    break;
    case SVA_COMMON:    break;
    case SVA_WEIGHTED:
    default:
        if (ph->pWeightedList)
        {
            vp = vertex_buffer;
            np = normal_buffer;

            for (i = 0; i < vh->vnum; i++)
            {
                //intpointer[0] = intpointer[0] * 16 + (int)vp; intpointer++;
                //intpointer[0] = intpointer[0] * 16 + (int)np; intpointer++;

                /// In order to make it 32bits compatible, the base reference needs to removed
                /// so now instead if will contain the offset from its vp/np
                intpointer[0] = intpointer[0] * 16 + MikuPan_GetPs2OffsetFromHostPointer(vp); intpointer++;
                intpointer[0] = intpointer[0] * 16 + MikuPan_GetPs2OffsetFromHostPointer(np); intpointer++;
            }
        }
        else
        {
            //vp = ph->pWeightedVertex;
            //np = ph->pWeightedNormal;
            vp = MikuPan_GetHostPointer(ph->pWeightedVertex);
            np = MikuPan_GetHostPointer(ph->pWeightedNormal);

            for (i = 0; i < vh->vnum; i++)
            {
                //intpointer[0] = intpointer[0] * 32 + (int)vp; intpointer++;
                //intpointer[0] = intpointer[0] * 32 + (int)np; intpointer++;

                /// In order to make it 32bits compatible, the base reference needs to removed
                /// so now instead if will contain the offset from its vp/np
                intpointer[0] = intpointer[0] * 32 + MikuPan_GetPs2OffsetFromHostPointer(vp); intpointer++;
                intpointer[0] = intpointer[0] * 32 + MikuPan_GetPs2OffsetFromHostPointer(np); intpointer++;
            }
        }
    break;
    }
}

void MappingVUVNDataPreset(u_int *intpointer, int mtype, int gloops, int hsize)
{
    return;
}

void MappingTYPE0(int gloops, u_int *prim)
{
    int j;
    int loops;

    loops = gloops;

    for (j = 0; j < loops; j++)
    {
        prim[1] |= 0x100000;
        prim = prim + ((prim[0] & 0x7fff) * 8 + 4);
    }
}

int MappingTYPE2(int gloops, u_int *prim)
{
    int j;
    int loops;
    int hsize;

    hsize = 0;

    if ((prim[2] & 0xf) == SCE_GIF_PACKED_AD)
    {
        hsize = ((sceGifTag *)prim)->NREG + 1;
    }

    prim = &prim[hsize*4];

    for(j = 0; j < gloops; j++)
    {
        loops = prim[0];
        prim[1] |= 0x100000;
        prim += (loops & 0x7fff) * 12 + 4;
    }

    return hsize;
}

void MappingMeshData(u_int *intpointer, u_int *vuvnprim, HeaderSection *hs)
{
    int mtype;
    int gloops;
    int hsize;
    static u_int *old_mesh_p;

    mtype = GET_MESH_TYPE(intpointer);
    gloops = GET_MESH_GLOOPS(intpointer);

    if (mtype & iMT_0)
    {
        PresetChk = 1;

        if ((mtype & 0x40) == 0)
        {
            MappingVUVNDataPreset(vuvnprim, mtype, gloops, hsize);
            return;
        }
    }
    else
    {
        if ((mtype & 0xc0) == 0)
        {
            MappingVUVNData(vuvnprim, hs);
            return;
        }
    }
}

void MappingCoordinateData(u_int *intpointer, HeaderSection *hs)
{
    return;
}

void MappingVertexList(VERTEXLIST *vli, PHEAD *ph)
{
    int i;
    int size;
    int vnnum;

    for (i = 0, size = 0; i < vli->list_num; i++)
    {
        vnnum = *(int *)&vli->lists[i].vIdx;
        vli->lists[i].vOff = size;
        size += vnnum;
    }

    if (vnbuf_size < size)
    {
        info_log("VNBuffer Over size %d needs %d", vnbuf_size, size);
        ph->pUniqList = ph->pWeightedList = 0;
    }
}

void SgMapRebuld(void *sgd_top)
{
    u_int *prim;
    u_int *nextprim;
    HeaderSection *hs;

    hs = (HeaderSection *)sgd_top;

    if (hs->primitives == 0)
    {
        return;
    }

    prim = (u_int *)((int)hs->primitives + (int)sgd_top);

    while (prim[0] != 0)
    {
        nextprim = (u_int *)(prim[0] + (int)prim);

        // check that always evaluates to false ???
        // most likely `prim == 0` that got optimized out
        if (0)
        {
            break;
        }

        switch (prim[1])
        {
        case TRI2:
            RebuildTRI2Files(prim);
        break;
        }

        prim = nextprim;
    }
}

void SgMapUnit(void *sgd_top)
{
    int i;
    int j;
    int size;
    u_int *intpointer;
    u_int *nextprim;
    u_int *pk;
    u_int *vuvnprim;
    HeaderSection *hs;
    SgMaterial *matp;
    u_int *phead;
    SgCOORDUNIT *cp;
    PHEAD *ph;
    VERTEXLIST *vli;

    hs = (HeaderSection *)sgd_top;

    if (hs->MAPFLAG != 0)
    {
        return;
    }

    hs->MAPFLAG = 1;

    PresetChk = 0;

    if ((u_int)hs->coordp - 1 < 0x2fffffff)
    {
        /// Overwrites data for 64bits PTR
        //hs->coordp = (SgCOORDUNIT *)((u_int)hs->coordp + (int)sgd_top);
        hs->coordp = MikuPan_GetPs2OffsetFromHostPointer(sgd_top) + hs->coordp;
    }

    if ((u_int)hs->matp - 1 < 0x2fffffff)
    {
        /// Overwrites data for 64bits PTR
        //hs->matp = (SgMaterial *)((u_int)hs->matp + (int)sgd_top);
        hs->matp = MikuPan_GetPs2OffsetFromHostPointer(sgd_top) + hs->matp;
    }

    /// Overwrites data for 64bits PTR
    //hs->phead = (u_int *)((u_int)hs->phead + (int)sgd_top);
    hs->phead = MikuPan_GetPs2OffsetFromHostPointer(sgd_top) + hs->phead;

    pk = (int *)&hs->primitives;

    for (i = 0; i < hs->blocks; i++)
    {
        if (pk[i] != 0)
        {
            /// Overwrites data for 64bits PTR
            pk[i] = MikuPan_GetPs2OffsetFromHostPointer(sgd_top) + pk[i];
        }
    }

    cp = GetCoordP(hs);

    /// Maps CoordP parents
    //if (cp != 0)
    if (hs->coordp != 0)
    {
        for (i = 0; i < hs->blocks - 1; i++)
        {
            if (cp[i].parent < 0)
            {
                cp[i].parent = 0;
            }
            else if (cp[i].parent < MikuPan_GetPs2OffsetFromHostPointer(cp))
            {
                /// Overwrites data for 64bits PTR
                //cp[i].parent = (int64_t)cp[i].parent + cp;
                cp[i].parent = MikuPan_GetPs2OffsetFromHostPointer(&cp[cp[i].parent]);
            }
        }
    }

    //intpointer = (u_int *)((int64_t)hs->phead);
    intpointer = GetPHead(hs);
    intpointer++;

    for (i = 0; i < GetPHead(hs)[0]; i++)
    {
        size = *intpointer++;

        for (j = 0; j < size - 1; j++, intpointer++)
        {
            if (*intpointer != 0)
            {
                //*intpointer = *intpointer + (int64_t)sgd_top;
                *intpointer = MikuPan_GetPs2OffsetFromHostPointer(sgd_top) + *intpointer;
            }
        }
    }

    //ph = (PHEAD *)hs->phead;
    ph = (PHEAD *)GetPHead(hs);

    if (ph->pUniqList != 0 && ph->UniqHeaderSize == 4)
    {
        if (vnbuf_size == 0)
        {
            ph->pUniqList = 0;
            ph->pCommonList = 0;
            ph->pWeightedList = 0;
        }
        else
        {
            if (ph->pWeightedVertex == 0 && ph->pWeightedNormal == 0)
            {
                ph->pWeightedList = 0;
            }
        }

        ph->pUniqList = 0;
        if (ph->pWeightedList != 0)
        {
            //vli = ((VERTEXLIST *)ph->pWeightedList);
            vli = ((VERTEXLIST *)MikuPan_GetHostPointer(ph->pWeightedList));
            MappingVertexList((VERTEXLIST *)MikuPan_GetHostPointer(ph->pWeightedList), ph);

            vli = (VERTEXLIST *)(&vli->lists[vli->list_num]);
            MappingVertexList(vli, ph);
        }
    }

    for (i = 0; i < hs->blocks; i++)
    {
        if (i == hs->blocks - 1)
        {
            post_process = 1;
        }
        else
        {
            post_process = 0;
        }

        if (pk[i] == NULL)
        {
            continue;
        }

        //intpointer = (u_int *)pk[i];
        intpointer = GetTopProcUnitHeaderPtr(hs, i);

        while (*intpointer != NULL)
        {
            //*intpointer = (int)*intpointer + (int)intpointer;
            // nextprim = intpointer++;
            nextprim = GetNextProcUnitHeaderPtr(intpointer);

            intpointer = &intpointer[1];

            switch(*intpointer)
            {
            case VUVN:
                vuvnprim = intpointer-1;
            break;
            case MESH:
                MappingMeshData(intpointer-1, vuvnprim, hs);
            break;
            case MATERIAL:
                intpointer++;
                //*intpointer = (u_int)&hs->matp[*intpointer];
                *intpointer = MikuPan_GetPs2OffsetFromHostPointer(GetMaterialPtr(hs, *intpointer));
            break;
            case COORDINATE:
                MappingCoordinateData(intpointer-1, hs);
            break;
            case TRI2:
                RebuildTRI2Files(intpointer-1);
            break;
            }

            //intpointer = (u_int *)*nextprim;
            intpointer = nextprim;
        }
    }

    if (hs->materials == 0)
    {
        size = 0;
        matp = GetMaterialPtr(hs, 0);
        phead = GetPHead(hs);

        while ((int64_t)matp < (int64_t)phead)
        {
            size++;
            matp++;
        }

        hs->materials = size;
        hs->kind = (int)PresetChk;
    }
}

u_int *GetPrimitiveAddr(void *sgd_top, int num)
{
    int blocks;
    u_int *pk;
    HeaderSection *hs;

    hs = (HeaderSection *)sgd_top;

    blocks = hs->blocks;

    if (blocks <= num)
    {
        return (u_int*)0xFFFFFFFF;
    }

    pk = (u_int *)&hs->primitives;

    //return (pk[num] != 0) ? (u_int *)pk[num] : &pk[num];
    return (pk[num] != 0) ? GetTopProcUnitHeaderPtr(hs, num) : &pk[num];
}

u_int *GetStartPacketAddr(void *sgd_top, int num)
{
    int blocks;
    u_int *pk;
    HeaderSection *hs;

    hs = (HeaderSection *)sgd_top;

    blocks = hs->blocks;

    if (blocks <= num)
    {
        return (u_int *)0xFFFFFFFF;
    }

    pk = (u_int *)&hs->primitives;

    return &pk[num];
}

u_int* GetEndPacketAddr(void *sgd_top, int num)
{
    u_int *pk;
    u_int *pkback;

    pk = GetPrimitiveAddr(sgd_top, num);

    if ((u_int)pk == 0xFFFFFFFF || pk == NULL)
    {
        return pk;
    }

    pkback = pk;

    while (*pk != 0)
    {
        pkback = pk;
        pk = (u_int*)*pkback;
    }

    return pkback;
}

void PostChainPacket(u_int *dest, u_int *source)
{
    *source = *dest;
    *dest = (u_int)source;
}

int GetModelKind(void *sgd_top)
{
    return ((HeaderSection*)sgd_top)->kind & 0x1;
}

void SgSetTEX1Prim(u_int *prim, sceGsTex1 *ptex1)
{
    u_int *mesh_prim;
    int i;
    int hsize;

    if (prim == NULL)
    {
        return;
    }

    while (prim[0] != NULL)
    {
        if (prim[1] == 1 && (prim[10] & 0xf) == 14)
        {
            hsize = *(u_long *)&prim[8] >> 60;
            mesh_prim = &prim[12];

            for (i = 0; i < hsize; i++)
            {
                if (mesh_prim[2] == SCE_GS_TEX1_1)
                {
                    sceGsTex1 *tex1 = (sceGsTex1 *)mesh_prim;

                    if (tex1->MXL != 0)
                    {
                        tex1->LCM = ptex1->LCM;

                        if (ptex1->MXL > 0)
                        {
                            tex1->MXL = ptex1->MXL;
                        }

                        tex1->MMAG = ptex1->MMAG;
                        tex1->MMIN = ptex1->MMIN;
                        tex1->MTBA = ptex1->MTBA;
                        tex1->L = ptex1->L;
                        tex1->K = ptex1->K;
                    }
                }

                mesh_prim += 4;
            }
        }

        prim = (u_int *)*prim;
    }
}

void SgSetTEX1(void *sgd_top, sceGsTex1 *ptex1)
{
    int i;
    HeaderSection *hs;
    u_int **pk;

    hs = (HeaderSection *)sgd_top;
    pk = (u_int **)&hs->primitives;


    for (i = 1; i < hs->blocks; i++)
    {
        SgSetTEX1Prim(pk[i], ptex1);
    }
}
