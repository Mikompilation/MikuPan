#include "sgsu.h"
#include "common.h"
#include "enums.h"
#include "typedefs.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ee/eestruct.h"
#include "sgsup.h"

#include "graphics/graph3d/libsg.h"
#include "graphics/graph3d/sgcam.h"
#include "graphics/graph3d/sgdma.h"
#include "graphics/graph3d/sglib.h"
#include "graphics/graph3d/sglight.h"
#include "main/glob.h"
#include "mikupan/debug/mikupan_logging_c.h"
#include "mikupan/rendering/mikupan_profiler.h"
#include "mikupan/rendering/mikupan_renderer.h"

#define min(x, y) (((x) > (y))? (y): (x))

extern void DRAWTYPE2();
extern void DRAWTYPE2W();
extern void DRAWTYPE0();
extern void MULTI_PROLOGUE();

extern SgSourceChainTag SgSu_dma_start();
extern SgSourceChainTag SgSuP0_dma_start();
extern SgSourceChainTag SgSuP2_dma_start();
extern SgSourceChainTag SgSu_dma_starts();

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

    //if (vli == NULL)
    if (lphead->pWeightedList == 0)
    {
        return;
    }

    //vps = lphead->pWeightedVertex;
    vps = (sceVu0FVECTOR *) MikuPan_GetHostPointer(lphead->pWeightedVertex);
    vpd = vertex_buffer;

    for (i = 0; i < vli->list_num; i++)
    {
        load_matrix_0(lcp[vli->lists[i].cn0].lwmtx);
        load_matrix_1(lcp[vli->lists[i].cn1].lwmtx);
        //load_matrix_0(lcp[vli->lists[i].cn0].workm);
        //load_matrix_1(lcp[vli->lists[i].cn1].workm);

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
        load_matrix_0(lcp[vli->lists[i].cn0].lwmtx);
        load_matrix_1(lcp[vli->lists[i].cn1].lwmtx);
        //load_matrix_0(lcp[vli->lists[i].cn0].workm);
        //load_matrix_1(lcp[vli->lists[i].cn1].workm);

        for (j = 0; j < vli->lists[i].vIdx; j++, vpd += 1, vps += 2)
        {
            calc_skinned_normal(*vpd, vps);
        }
    }
}

extern int MikuPan_IsShadowPassActive(void);
extern int MikuPan_IsShadowReceiverPassActive(void);

static int g_gpu_skin_enabled = 1;
static int g_gpu_weighted_skin_enabled = 1;
static int g_skin_mode = 0;
static int g_skin_vcount = 0;
static int g_skin_vtype = 0;
static int g_skin_mesh_b0 = 0;
static int g_skin_mesh_b1 = 0;
static int g_skin_bind_valid = 0;
static int g_skin_weighted = 0;
static int g_skin_palette_bones = 0;
static int g_skin_weighted_fail_reason = MIKUPAN_PERF_SKIN_REJECT_WEIGHTED_PREP_FAILED;
static u_int *g_skin_loop_prim = NULL;
static unsigned char *g_skin_staging = NULL;
static long long g_skin_staging_cap = 0;
static unsigned char *g_skin_palette = NULL;
static long long g_skin_palette_cap = 0;
static SgCOORDUNIT *g_skin_palette_last_lcp = NULL;
static unsigned long long g_skin_palette_hash = 0;
static sceVu0FVECTOR *g_skin_weighted_vertices = NULL;
static sceVu0FVECTOR *g_skin_weighted_normals = NULL;
static VERTEXLIST *g_skin_weighted_position_list = NULL;
static VERTEXLIST *g_skin_weighted_normal_list = NULL;

#define MIKUPAN_SKIN_VERTEX_STRIDE 64
#define MIKUPAN_SKIN_MATRIX_BYTES 64

static void SkinReset(void)
{
    g_skin_mode = 0;
    g_skin_vcount = 0;
    g_skin_vtype = 0;
    g_skin_mesh_b0 = 0;
    g_skin_mesh_b1 = 0;
    g_skin_bind_valid = 0;
    g_skin_weighted = 0;
    g_skin_loop_prim = NULL;
    g_skin_weighted_vertices = NULL;
    g_skin_weighted_normals = NULL;
    g_skin_weighted_position_list = NULL;
    g_skin_weighted_normal_list = NULL;
}

static unsigned char *SkinEnsure(unsigned char **buf, long long *cap, long long need)
{
    if (need > *cap)
    {
        unsigned char *grown = (unsigned char *) realloc(*buf, (size_t) need);
        if (grown == NULL)
        {
            return NULL;
        }
        *buf = grown;
        *cap = need;
    }
    return *buf;
}

static unsigned long long SkinFnv1a(const void *data, long long size)
{
    const unsigned char *p = (const unsigned char *) data;
    unsigned long long h = 1469598103934665603ULL;
    for (long long i = 0; i < size; i++)
    {
        h ^= (unsigned long long) p[i];
        h *= 1099511628211ULL;
    }
    return h;
}

static int SkinAllowed(void)
{
    return g_gpu_skin_enabled
        && !MikuPan_IsShadowPassActive()
        && !MikuPan_IsShadowReceiverPassActive();
}

static int SkinAllowedForMeshType(int mtype)
{
    const int raw_type = mtype & 0xff;

    if (raw_type == 0x2)
    {
        return 1;
    }
    if (raw_type == 0xA && realtime_scene_flg == 0)
    {
        return 1;
    }
    return 0;
}

static int SkinGpuRejectReason(int mesh_allowed, int weighted, int prepare_failed)
{
    if (!mesh_allowed)
    {
        return MIKUPAN_PERF_SKIN_REJECT_MESH_TYPE;
    }
    if (!g_gpu_skin_enabled)
    {
        return MIKUPAN_PERF_SKIN_REJECT_GPU_DISABLED;
    }
    if (MikuPan_IsShadowPassActive())
    {
        return MIKUPAN_PERF_SKIN_REJECT_SHADOW_PASS;
    }
    if (MikuPan_IsShadowReceiverPassActive())
    {
        return MIKUPAN_PERF_SKIN_REJECT_RECEIVER_PASS;
    }
    if (weighted && !g_gpu_weighted_skin_enabled)
    {
        return MIKUPAN_PERF_SKIN_REJECT_WEIGHTED_DISABLED;
    }
    if (prepare_failed)
    {
        return g_skin_weighted_fail_reason;
    }
    return MIKUPAN_PERF_SKIN_REJECT_MESH_TYPE;
}

static int SkinPackBonePair(int b0, int b1)
{
    return (b0 & 0xFF) | ((b1 & 0xFF) << 8);
}

static void SkinStorePackedBones(unsigned char *dst, int pos_b0, int pos_b1, int nrm_b0, int nrm_b1, int weighted)
{
    float pos_packed = (float) SkinPackBonePair(pos_b0, pos_b1);
    int nrm_packed_int = SkinPackBonePair(nrm_b0, nrm_b1);
    float nrm_packed = weighted ? -((float) nrm_packed_int + 1.0f) : (float) nrm_packed_int;
    memcpy(dst + 28, &pos_packed, sizeof(pos_packed));
    memcpy(dst + 60, &nrm_packed, sizeof(nrm_packed));
}

static ONELIST *SkinFindWeightedList(VERTEXLIST *list, int index)
{
    if (list == NULL || index < 0)
    {
        return NULL;
    }

    for (int i = 0; i < list->list_num; i++)
    {
        int start = list->lists[i].vOff;
        int end = start + list->lists[i].vIdx;
        if (index >= start && index < end)
        {
            return &list->lists[i];
        }
    }

    return NULL;
}

static int SkinWeightedVertexIndex(u_int ps2_offset)
{
    sceVu0FVECTOR *ptr = (sceVu0FVECTOR *) MikuPan_GetHostPointer(ps2_offset);
    if (ptr == NULL || vertex_buffer == NULL || ptr < vertex_buffer || ptr >= vertex_buffer + vnbuf_size)
    {
        return -1;
    }
    return (int)(ptr - vertex_buffer);
}

static int SkinWeightedNormalIndex(u_int ps2_offset)
{
    sceVu0FVECTOR *ptr = (sceVu0FVECTOR *) MikuPan_GetHostPointer(ps2_offset);
    if (ptr == NULL || normal_buffer == NULL || ptr < normal_buffer || ptr >= normal_buffer + vnbuf_size)
    {
        return -1;
    }
    return (int)(ptr - normal_buffer);
}

static void SkinGatherPalette(void)
{
    if (lcp == g_skin_palette_last_lcp)
    {
        return; /* same model as the previous skinned mesh — already gathered */
    }

    int bones = blocksm;
    if (bones <= 0)
    {
        bones = 1;
    }

    if (SkinEnsure(&g_skin_palette, &g_skin_palette_cap,
                   (long long) bones * MIKUPAN_SKIN_MATRIX_BYTES) == NULL)
    {
        g_skin_palette_bones = 0;
        g_skin_palette_last_lcp = NULL;
        return;
    }

    for (int b = 0; b < bones; b++)
    {
        memcpy(g_skin_palette + (long long) b * MIKUPAN_SKIN_MATRIX_BYTES,
               lcp[b].lwmtx, MIKUPAN_SKIN_MATRIX_BYTES);
    }

    g_skin_palette_bones = bones;
    g_skin_palette_hash = SkinFnv1a(g_skin_palette, (long long) bones * MIKUPAN_SKIN_MATRIX_BYTES);
    g_skin_palette_last_lcp = lcp;
}

void MikuPan_SkinFrameReset(void)
{
    g_skin_palette_last_lcp = NULL;
}

unsigned long long MikuPan_SkinPaletteHash(void)
{
    return g_skin_palette_hash;
}

static int SkinPreparePlain(u_int *loop_prim, int vnum, int vtype)
{
    SkinReset();
    if (loop_prim == NULL || vnum <= 0 || (vtype != 2 && vtype != 3))
    {
        return 0;
    }

    g_skin_loop_prim = loop_prim;
    g_skin_vtype = vtype;
    g_skin_vcount = vnum;

    if (vtype == 2)
    {
        unsigned char *cn = (unsigned char *) MikuPan_GetHostPointer(loop_prim[0]);
        if (cn == NULL)
        {
            SkinReset();
            return 0;
        }
        g_skin_mesh_b0 = cn[0x1c];
        g_skin_mesh_b1 = cn[0x1d];
    }

    SkinGatherPalette();
    if (g_skin_palette_bones <= 0 || g_skin_palette == NULL)
    {
        SkinReset();
        return 0;
    }

    g_skin_mode = vtype;
    return 1;
}

static int SkinPrepareWeighted(u_int *loop_prim, int vnum, int vtype)
{
    SkinReset();
    g_skin_weighted_fail_reason = MIKUPAN_PERF_SKIN_REJECT_WEIGHTED_PREP_FAILED;

    if (!g_gpu_weighted_skin_enabled)
    {
        g_skin_weighted_fail_reason = MIKUPAN_PERF_SKIN_REJECT_WEIGHTED_DISABLED;
        return 0;
    }
    if (loop_prim == NULL || vnum <= 0 || (vtype != 2 && vtype != 3))
    {
        g_skin_weighted_fail_reason = MIKUPAN_PERF_SKIN_REJECT_WEIGHTED_INVALID_ARGS;
        return 0;
    }
    if (lphead->pWeightedList == 0)
    {
        g_skin_weighted_fail_reason = MIKUPAN_PERF_SKIN_REJECT_WEIGHTED_NO_LIST;
        return 0;
    }
    if (lphead->pWeightedVertex == 0)
    {
        g_skin_weighted_fail_reason = MIKUPAN_PERF_SKIN_REJECT_WEIGHTED_NO_VERTEX_SOURCE;
        return 0;
    }
    if (lphead->pWeightedNormal == 0)
    {
        g_skin_weighted_fail_reason = MIKUPAN_PERF_SKIN_REJECT_WEIGHTED_NO_NORMAL_SOURCE;
        return 0;
    }

    VERTEXLIST *position_list = (VERTEXLIST *) MikuPan_GetHostPointer(lphead->pWeightedList);
    if (position_list == NULL)
    {
        g_skin_weighted_fail_reason = MIKUPAN_PERF_SKIN_REJECT_WEIGHTED_LIST_PTR_NULL;
        return 0;
    }

    VERTEXLIST *normal_list = (VERTEXLIST *) &position_list->lists[position_list->list_num];
    sceVu0FVECTOR *weighted_vertices = (sceVu0FVECTOR *) MikuPan_GetHostPointer(lphead->pWeightedVertex);
    sceVu0FVECTOR *weighted_normals = (sceVu0FVECTOR *) MikuPan_GetHostPointer(lphead->pWeightedNormal);
    if (weighted_vertices == NULL)
    {
        g_skin_weighted_fail_reason = MIKUPAN_PERF_SKIN_REJECT_WEIGHTED_VERTEX_PTR_NULL;
        return 0;
    }
    if (weighted_normals == NULL)
    {
        g_skin_weighted_fail_reason = MIKUPAN_PERF_SKIN_REJECT_WEIGHTED_NORMAL_PTR_NULL;
        return 0;
    }

    u_int *p = loop_prim;
    for (int i = 0; i < vnum; i++, p += 2)
    {
        int pos_index = SkinWeightedVertexIndex(p[0]);
        int nrm_index = SkinWeightedNormalIndex(p[1]);
        if (pos_index < 0)
        {
            g_skin_weighted_fail_reason = MIKUPAN_PERF_SKIN_REJECT_WEIGHTED_POS_INDEX_INVALID;
            return 0;
        }
        if (nrm_index < 0)
        {
            g_skin_weighted_fail_reason = MIKUPAN_PERF_SKIN_REJECT_WEIGHTED_NRM_INDEX_INVALID;
            return 0;
        }
        if (SkinFindWeightedList(position_list, pos_index) == NULL)
        {
            g_skin_weighted_fail_reason = MIKUPAN_PERF_SKIN_REJECT_WEIGHTED_POS_LIST_MISS;
            return 0;
        }
        if (SkinFindWeightedList(normal_list, nrm_index) == NULL)
        {
            g_skin_weighted_fail_reason = MIKUPAN_PERF_SKIN_REJECT_WEIGHTED_NRM_LIST_MISS;
            return 0;
        }
    }

    g_skin_loop_prim = loop_prim;
    g_skin_vtype = vtype;
    g_skin_vcount = vnum;
    g_skin_weighted = 1;
    g_skin_weighted_vertices = weighted_vertices;
    g_skin_weighted_normals = weighted_normals;
    g_skin_weighted_position_list = position_list;
    g_skin_weighted_normal_list = normal_list;

    SkinGatherPalette();
    if (g_skin_palette_bones <= 0 || g_skin_palette == NULL)
    {
        g_skin_weighted_fail_reason = MIKUPAN_PERF_SKIN_REJECT_WEIGHTED_PREP_FAILED;
        SkinReset();
        return 0;
    }

    g_skin_mode = vtype;
    return 1;
}

static void SkinCopyLoop(sceVu0FVECTOR *vp, u_int *prim, int vnum)
{
    for (int i = 0; i < vnum; i++, vp += 2, prim += 2)
    {
        copy_skinned_data(vp, (float *) MikuPan_GetHostPointer(prim[0]), (float *) MikuPan_GetHostPointer(prim[1]));
    }
}

static void SkinCalcLoopType2(sceVu0FVECTOR *vp, u_int *prim, int vnum)
{
    char *cn = (char *) MikuPan_GetHostPointer(prim[0]);
    load_matrix_0(lcp[cn[0x1c]].lwmtx);
    load_matrix_1(lcp[cn[0x1d]].lwmtx);

    for (int i = 0; i < vnum; i++, vp += 2, prim += 2)
    {
        calc_skinned_data(vp, (float *) MikuPan_GetHostPointer(prim[0]), (float *) MikuPan_GetHostPointer(prim[1]));
    }
}

static void SkinCalcLoopType3(sceVu0FVECTOR *vp, u_int *prim, int vnum)
{
    for (int i = 0; i < vnum; i++, vp += 2, prim += 2)
    {
        char *cn = (char *) MikuPan_GetHostPointer(prim[0]);
        load_matrix_0(lcp[cn[0x1c]].lwmtx);
        load_matrix_1(lcp[cn[0x1d]].lwmtx);
        calc_skinned_data(vp, (float *) MikuPan_GetHostPointer(prim[0]), (float *) MikuPan_GetHostPointer(prim[1]));
    }
}

static int SkinTryGpuWeighted(u_int *prim, int vnum, int vtype, int mesh_allowed)
{
    if (mesh_allowed && SkinAllowed() && SkinPrepareWeighted(prim, vnum, vtype))
    {
        MikuPan_PerfRecordSkinPath(vtype, MIKUPAN_PERF_SKIN_GPU, mesh_allowed, vnum);
        return 1;
    }

    MikuPan_PerfRecordSkinPath(vtype, MIKUPAN_PERF_SKIN_WEIGHTED_COPY, mesh_allowed, vnum);
    MikuPan_PerfRecordSkinReject(vtype, SkinGpuRejectReason(mesh_allowed, 1, mesh_allowed && SkinAllowed()), vnum);
    return 0;
}

static int SkinTryGpuPlain(u_int *prim, int vnum, int vtype, int mesh_allowed)
{
    if (mesh_allowed && SkinAllowed() && SkinPreparePlain(prim, vnum, vtype))
    {
        MikuPan_PerfRecordSkinPath(vtype, MIKUPAN_PERF_SKIN_GPU, mesh_allowed, vnum);
        return 1;
    }

    MikuPan_PerfRecordSkinPath(vtype, MIKUPAN_PERF_SKIN_CPU_CALC, mesh_allowed, vnum);
    MikuPan_PerfRecordSkinReject(vtype, SkinGpuRejectReason(mesh_allowed, 0, 0), vnum);
    return 0;
}

int MikuPan_GpuSkinningEnabled(void)
{
    return g_gpu_skin_enabled;
}

void MikuPan_SetGpuSkinningEnabled(int enabled)
{
    g_gpu_skin_enabled = enabled ? 1 : 0;
    if (!g_gpu_skin_enabled)
    {
        SkinReset();
    }
}

int MikuPan_GpuWeightedSkinningEnabled(void)
{
    return g_gpu_weighted_skin_enabled;
}

void MikuPan_SetGpuWeightedSkinningEnabled(int enabled)
{
    g_gpu_weighted_skin_enabled = enabled ? 1 : 0;
    if (!g_gpu_weighted_skin_enabled)
    {
        SkinReset();
    }
}

int MikuPan_SkinMode(void)
{
    return g_skin_mode;
}

void MikuPan_ClearSkinMode(void)
{
    SkinReset();
}

int MikuPan_SkinVertexCount(void)
{
    return g_skin_vcount;
}

const void *MikuPan_SkinBindData(void)
{
    int vnum = g_skin_vcount;
    if (g_skin_mode == 0 || vnum <= 0 || g_skin_loop_prim == NULL)
    {
        return NULL;
    }
    if (g_skin_bind_valid)
    {
        return g_skin_staging;
    }

    if (SkinEnsure(&g_skin_staging, &g_skin_staging_cap,
                   (long long) vnum * MIKUPAN_SKIN_VERTEX_STRIDE) == NULL)
    {
        return NULL;
    }

    u_int *p = g_skin_loop_prim;
    for (int i = 0; i < vnum; i++, p += 2)
    {
        unsigned char *dst = g_skin_staging + (long long) i * MIKUPAN_SKIN_VERTEX_STRIDE;

        if (g_skin_weighted)
        {
            int pos_index = SkinWeightedVertexIndex(p[0]);
            int nrm_index = SkinWeightedNormalIndex(p[1]);
            ONELIST *pos_list = SkinFindWeightedList(g_skin_weighted_position_list, pos_index);
            ONELIST *nrm_list = SkinFindWeightedList(g_skin_weighted_normal_list, nrm_index);
            if (pos_list == NULL || nrm_list == NULL)
            {
                return NULL;
            }

            memcpy(dst, g_skin_weighted_vertices + (long long) pos_index * 2, 32);
            memcpy(dst + 32, g_skin_weighted_normals + (long long) nrm_index * 2, 32);
            SkinStorePackedBones(dst, pos_list->cn0, pos_list->cn1, nrm_list->cn0, nrm_list->cn1, 1);
        }
        else
        {
            void *src_pos = MikuPan_GetHostPointer(p[0]);
            void *src_nrm = MikuPan_GetHostPointer(p[1]);
            if (src_pos == NULL || src_nrm == NULL)
            {
                return NULL;
            }

            memcpy(dst, src_pos, 32);
            memcpy(dst + 32, src_nrm, 32);

            int b0;
            int b1;
            if (g_skin_vtype == 2)
            {
                b0 = g_skin_mesh_b0;
                b1 = g_skin_mesh_b1;
            }
            else
            {
                b0 = dst[28];
                b1 = dst[29];
            }

            SkinStorePackedBones(dst, b0, b1, b0, b1, 0);
        }
    }

    g_skin_bind_valid = 1;
    return g_skin_staging;
}

const void *MikuPan_SkinPalette(int *out_bones)
{
    if (out_bones != NULL)
    {
        *out_bones = g_skin_palette_bones;
    }
    return g_skin_palette;
}

u_int *SetVUVNData(u_int *prim)
{
    MIKUPAN_PERF_SCOPE(PERF_SECT_SKIN_PREP);
    int i;
    VUVN_PRIM *vh;
    sceVu0FVECTOR *vp;
    SkinReset();

    vh = (VUVN_PRIM *) &prim[2];
    MikuPan_PerfRecordSkinPath(vh->vtype, MIKUPAN_PERF_SKIN_COPY, 0, vh->vnum);
    MikuPan_PerfRecordSkinReject(vh->vtype, MIKUPAN_PERF_SKIN_REJECT_LEGACY_PREPASS, vh->vnum);
    vp = (sceVu0FVECTOR *) getObjWrk();

    copy_skinned_data(vp, (float *) &prim[4], (float *) &prim[8]);

    vp += 2;
    prim += 12;

    void* bak = vp;
    for (i = 0; i < vh->vnum; i++, vp += 2, prim += 2)
    {
        copy_skinned_data(vp, (float *) MikuPan_GetHostPointer(prim[0]), (float *) MikuPan_GetHostPointer(prim[1]));
    }

    return (u_int *) bak;
}

u_int *SetVUVNDataPost(u_int *prim, int allow_gpu_skin)
{
    MIKUPAN_PERF_SCOPE(PERF_SECT_SKIN_PREP);
    VUVN_PRIM *vh;
    sceVu0FVECTOR *vp;

    vh = (VUVN_PRIM *) &prim[2];
    vp = (sceVu0FVECTOR *) getObjWrk();

    copy_skinned_data(vp, (float *) &prim[4], (float *) &prim[8]);

    vp += 2;
    prim += 12;

    void *bak = vp;
    SkinReset();

    switch (vh->vtype)
    {
        case 2:
            if (lphead->pWeightedList != 0)
            {
                if (!SkinTryGpuWeighted(prim, vh->vnum, 2, allow_gpu_skin))
                {
                    SkinCopyLoop(vp, prim, vh->vnum);
                }
            }
            else if (!SkinTryGpuPlain(prim, vh->vnum, 2, allow_gpu_skin))
            {
                SkinCalcLoopType2(vp, prim, vh->vnum);
            }
            break;

        case 3:
            if (lphead->pWeightedList != 0)
            {
                if (!SkinTryGpuWeighted(prim, vh->vnum, 3, allow_gpu_skin))
                {
                    SkinCopyLoop(vp, prim, vh->vnum);
                }
            }
            else if (!SkinTryGpuPlain(prim, vh->vnum, 3, allow_gpu_skin))
            {
                SkinCalcLoopType3(vp, prim, vh->vnum);
            }
            break;

        default:
            MikuPan_PerfRecordSkinPath(vh->vtype, MIKUPAN_PERF_SKIN_COPY, allow_gpu_skin, vh->vnum);
            MikuPan_PerfRecordSkinReject(vh->vtype, MIKUPAN_PERF_SKIN_REJECT_UNSUPPORTED_VTYPE, vh->vnum);
            SkinCopyLoop(vp, prim, vh->vnum);
            break;
    }

    return (u_int *) bak;
}

void printTEX0(sceGsTex0 *tex0)
{
    info_log("TBP0 %x TBW %d PSM %x TW %d TH %d TCC %d", tex0->TBP0,
             tex0->TBW, tex0->PSM, tex0->TW, tex0->TH, tex0->TCC);

    info_log("TFX %d CBP %x CPSM %x CSM %d CSA %d CLD %x", tex0->TFX,
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
            MikuPan_RenderMeshType0x2((SGDPROCUNITHEADER*)vuvnprim, (SGDPROCUNITHEADER*)prim, (float*)read_p);

            read_p[0] = 0x14000000 | ((u_int) DRAWTYPE2 >> 3);
            read_p[1] = 0x17000000;
            read_p[2] = 0x11000000;
            read_p[3] = 0x17000000;

            AppendDmaTag((u_int) &prim[4], prim[2]);
            AppendDmaBuffer(((u_char *) vuvnprim)[12]);
            FlushModel(0);
            break;
        case 0x80:
            //MikuPan_RenderMeshType0x82(vuvnprim, prim);
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
            info_log("Illegal Packet %d", mtype);
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
            read_p = SetVUVNDataPost(vuvnprim, 0);

            //MikuPan_RenderMeshType0x2((SGDPROCUNITHEADER*)vuvnprim, (SGDPROCUNITHEADER*)prim, (float*)read_p);

            read_p[0] = 0x14000000 | ((u_int) DRAWTYPE0 >> 3);
            read_p[1] = 0x17000000;
            read_p[2] = 0x11000000;
            read_p[3] = 0x17000000;

            AppendDmaTag((u_int) &prim[4], prim[2]);
            AppendDmaBuffer(((u_char *) vuvnprim)[12]);
            FlushModel(0);
            break;
        case 2:
            read_p = SetVUVNDataPost(vuvnprim, SkinAllowedForMeshType(mtype));

            /// Needs to be its own function since the actual type is 0x10
            MikuPan_RenderMeshType0x2((SGDPROCUNITHEADER *)vuvnprim, (SGDPROCUNITHEADER *)prim, (float *)read_p);

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
            info_log("Illegal Packet %d", mtype);
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
    return;
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
        info_log("SgSortUnit Data broken. %x lcp %x", sgd_top, lcp);
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
    memcpy(work_matrix_0, m0, sizeof(sceVu0FMATRIX));
}

void _SetLWMatrix1(sceVu0FMATRIX m0)
{
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
