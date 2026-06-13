#include "mirror.h"
#include "cglm/mat4.h"
#include "cglm/vec4.h"
#include "common.h"
#include "typedefs.h"

#include "ee/eestruct.h"
#include "sce/libvu0.h"

#include "graphics/graph3d/gra3d.h"
#include "graphics/graph3d/libsg.h"
#include "graphics/graph3d/sgcam.h"
#include "graphics/graph3d/sgdma.h"
#include "graphics/graph3d/sglib.h"
#include "graphics/graph3d/sglight.h"
#include "graphics/graph3d/sgsu.h"
#include "graphics/graph3d/sgsup.h"
#include "mikupan/rendering/mikupan_renderer.h"
#include "mikupan/rendering/mikupan_shader.h"
#include "os/system.h"

#include <math.h>

int mirror_points = 0;

static int mxmax;
static int mxmin;
static int mymax;
static int mymin;
static float mzmax;
static float mzmin;

sceVu0FMATRIX mir_mtx = {0};
sceVu0FVECTOR mir_norm = {0};
sceVu0FVECTOR mir_center = {0};
sceVu0FVECTOR mirror_lpos[5] = {0};
sceVu0FVECTOR mirror_cval[5] = {0};

#define GET_MESH_TYPE(intpointer) (((char *) intpointer)[13])
#define GET_MESH_GLOOPS(intpointer) ((int) ((char *) intpointer)[14])
#define GET_MESH_UNK(intpointer) (((short *) intpointer)[11])

#define SCRATCHPAD ((u_char *) ps2_virtual_scratchpad)

#define VU0_CLIP_X_POS (1 << 0)
#define VU0_CLIP_X_NEG (1 << 1)
#define VU0_CLIP_Y_POS (1 << 2)
#define VU0_CLIP_Y_NEG (1 << 3)
#define VU0_CLIP_Z_POS (1 << 4)
#define VU0_CLIP_Z_NEG (1 << 5)

sceVu0FMATRIX work_mirror = {0}; // in [vf25:vf28]

static inline void inline_asm__mirror_c_line_38(sceVu0FMATRIX m0)
{
    memcpy(work_mirror, m0, sizeof(sceVu0FMATRIX));
}

static inline void inline_asm__mirror_c_line_47(sceVu0FVECTOR v0,
                                                sceVu0FVECTOR v1)
{
    sceVu0FVECTOR *wk = work_mirror; // in [vf25:vf28]
    vec4 out = {0};

    out[0] = (wk[0][0] * v1[0]) + (wk[1][0] * v1[1]) + (wk[2][0] * v1[2]) + (wk[3][0] *  1.0f);
    out[1] = (wk[0][1] * v1[0]) + (wk[1][1] * v1[1]) + (wk[2][1] * v1[2]) + (wk[3][1] *  1.0f);
    out[2] = (wk[0][2] * v1[0]) + (wk[1][2] * v1[1]) + (wk[2][2] * v1[2]) + (wk[3][2] *  1.0f);
    out[3] = (wk[0][3] * v1[0]) + (wk[1][3] * v1[1]) + (wk[2][3] * v1[2]) + (wk[3][3] *  1.0f);

    glm_vec4_copy(out, v0);
}

static inline void inline_asm__mirror_c_line_62(MNODE *nm, sceVu0FVECTOR vp)
{
    sceVu0FVECTOR *wk = work_mirror; // in [vf25:vf28]
    sceVu0FVECTOR clip;

    clip[0] = (wk[0][0] * vp[0]) + (wk[1][0] * vp[1]) + (wk[2][0] * vp[2]) + (wk[3][0] *  1.0f);
    clip[1] = (wk[0][1] * vp[0]) + (wk[1][1] * vp[1]) + (wk[2][1] * vp[2]) + (wk[3][1] *  1.0f);
    clip[2] = (wk[0][2] * vp[0]) + (wk[1][2] * vp[1]) + (wk[2][2] * vp[2]) + (wk[3][2] *  1.0f);
    clip[3] = (wk[0][3] * vp[0]) + (wk[1][3] * vp[1]) + (wk[2][3] * vp[2]) + (wk[3][3] *  1.0f);

    memcpy(nm->vp, vp, sizeof(sceVu0FVECTOR));
    memcpy(nm->clip, clip, sizeof(sceVu0FVECTOR));
}

static int MirrorLineClip(float *v0, float *v1)
{
    int ret = 0;

    if (v0[0] > +fabsf(v0[3])) ret |= VU0_CLIP_X_POS;
    if (v0[0] < -fabsf(v0[3])) ret |= VU0_CLIP_X_NEG;
    if (v0[1] > +fabsf(v0[3])) ret |= VU0_CLIP_Y_POS;
    if (v0[1] < -fabsf(v0[3])) ret |= VU0_CLIP_Y_NEG;
    if (v0[2] > +fabsf(v0[3])) ret |= VU0_CLIP_Z_POS;
    if (v0[2] < -fabsf(v0[3])) ret |= VU0_CLIP_Z_NEG;

    ret <<= 6;

    if (v1[0] > +fabsf(v1[3])) ret |= VU0_CLIP_X_POS;
    if (v1[0] < -fabsf(v1[3])) ret |= VU0_CLIP_X_NEG;
    if (v1[1] > +fabsf(v1[3])) ret |= VU0_CLIP_Y_POS;
    if (v1[1] < -fabsf(v1[3])) ret |= VU0_CLIP_Y_NEG;
    if (v1[2] > +fabsf(v1[3])) ret |= VU0_CLIP_Z_POS;
    if (v1[2] < -fabsf(v1[3])) ret |= VU0_CLIP_Z_NEG;

    ret <<= 6;

    return ret;
}

static int work_clip;

// Man, why?, just keep them in a single function...
// I assume it's always paired with `GetClipValue`
static inline void inline_asm__mirror_c_line_96(sceVu0FVECTOR v0)
{
    int ret = 0;

    if (v0[0] > +fabsf(v0[3])) ret |= VU0_CLIP_X_POS;
    if (v0[0] < -fabsf(v0[3])) ret |= VU0_CLIP_X_NEG;
    if (v0[1] > +fabsf(v0[3])) ret |= VU0_CLIP_Y_POS;
    if (v0[1] < -fabsf(v0[3])) ret |= VU0_CLIP_Y_NEG;
    if (v0[2] > +fabsf(v0[3])) ret |= VU0_CLIP_Z_POS;
    if (v0[2] < -fabsf(v0[3])) ret |= VU0_CLIP_Z_NEG;

    work_clip = ret;
}

static int GetClipValue()
{
    return work_clip;
}

#define FLT_TO_FIX4(_val) ((int)((_val) * 16.0f))
static inline void inline_asm__mirror_c_line_120(Q_WORDDATA *base, sceVu0FVECTOR vp)
{
    sceVu0FVECTOR *wk0 = work_matrix_0; // in [vf4:vf7]
    sceVu0FVECTOR *wk1 = work_matrix_1; // in [vf8:vf11]
    sceVu0FVECTOR tmp0, tmp1;
    float r;

    tmp0[0] = (wk1[0][0] * vp[0]) + (wk1[1][0] * vp[1]) + (wk1[2][0] * vp[2]) + (wk1[3][0] *  1.0f);
    tmp0[1] = (wk1[0][1] * vp[0]) + (wk1[1][1] * vp[1]) + (wk1[2][1] * vp[2]) + (wk1[3][1] *  1.0f);
    tmp0[2] = (wk1[0][2] * vp[0]) + (wk1[1][2] * vp[1]) + (wk1[2][2] * vp[2]) + (wk1[3][2] *  1.0f);
    tmp0[3] = (wk1[0][3] * vp[0]) + (wk1[1][3] * vp[1]) + (wk1[2][3] * vp[2]) + (wk1[3][3] *  1.0f);

    r = 1.0f / tmp0[3];

    tmp0[0] *= r;
    tmp0[1] *= r;
    tmp0[2] *= r;

    base[0].ui32[0] = FLT_TO_FIX4(tmp0[0]);
    base[0].ui32[1] = FLT_TO_FIX4(tmp0[1]);
    base[0].ui32[2] = FLT_TO_FIX4(tmp0[2]);

    // Gets skipped, actually so the value should be unused as it's undefined
    // base.i[3] = FLT_TO_FIX4(tmp0[3] * r);

    base[1].fl32[0] = (wk0[0][0] * vp[0]) + (wk0[1][0] * vp[1]) + (wk0[2][0] * vp[2]) + (wk0[3][0] * vp[3]);
    base[1].fl32[1] = (wk0[0][1] * vp[0]) + (wk0[1][1] * vp[1]) + (wk0[2][1] * vp[2]) + (wk0[3][1] * vp[3]);
    base[1].fl32[2] = (wk0[0][2] * vp[0]) + (wk0[1][2] * vp[1]) + (wk0[2][2] * vp[2]) + (wk0[3][2] * vp[3]);
    base[1].fl32[3] = (wk0[0][3] * vp[0]) + (wk0[1][3] * vp[1]) + (wk0[2][3] * vp[2]) + (wk0[3][3] * vp[3]);

    base[2].fl32[0] = tmp0[0];
    base[2].fl32[1] = tmp0[1];
    base[2].fl32[2] = tmp0[2];
    base[2].fl32[3] = tmp0[3];
}

static float ClampMirrorNdc(float value)
{
    if (value < -1.0f) return -1.0f;
    if (value > 1.0f) return 1.0f;
    return value;
}

static void ConvertMirrorClipToGsBounds(sceVu0FVECTOR clip,
                                        int *xmin, int *ymin,
                                        int *xmax, int *ymax)
{
    float minx = ClampMirrorNdc(clip[0]);
    float maxx = ClampMirrorNdc(clip[1]);
    float miny = ClampMirrorNdc(clip[2]);
    float maxy = ClampMirrorNdc(clip[3]);

    if (maxx < minx)
    {
        float tmp = minx;
        minx = maxx;
        maxx = tmp;
    }

    if (maxy < miny)
    {
        float tmp = miny;
        miny = maxy;
        maxy = tmp;
    }

    *xmin = (int)((1728.0f + (minx + 1.0f) * 320.0f) * 16.0f);
    *xmax = (int)((1728.0f + (maxx + 1.0f) * 320.0f) * 16.0f);
    *ymin = (int)((1936.0f + (1.0f - maxy) * 112.0f) * 16.0f);
    *ymax = (int)((1936.0f + (1.0f - miny) * 112.0f) * 16.0f);
}

static void SetMirrorBoundsFromClipValue(sceVu0FVECTOR clip)
{
    ConvertMirrorClipToGsBounds(clip, &mxmin, &mymin, &mxmax, &mymax);
}

static void UnionMirrorClipBounds(sceVu0FVECTOR dst, const sceVu0FVECTOR src)
{
    if (src[0] < dst[0]) dst[0] = src[0];
    if (src[1] > dst[1]) dst[1] = src[1];
    if (src[2] < dst[2]) dst[2] = src[2];
    if (src[3] > dst[3]) dst[3] = src[3];
}

static void UnionMirrorGsBounds(int *xmin, int *ymin, int *xmax, int *ymax,
                                int other_xmin, int other_ymin,
                                int other_xmax, int other_ymax)
{
    if (other_xmin < *xmin) *xmin = other_xmin;
    if (other_ymin < *ymin) *ymin = other_ymin;
    if (other_xmax > *xmax) *xmax = other_xmax;
    if (other_ymax > *ymax) *ymax = other_ymax;
}

static int GetMirrorClipFromCurrentGlCamera(sceVu0FVECTOR out_clip)
{
    mat4 model;
    mat4 world_clip_view;
    int valid = 0;

    if (mirror_points <= 0)
    {
        return 0;
    }

    glm_mat4_make((float *) &SCRATCHPAD[0x430], model);
    glm_mat4_make(MikuPan_GetWorldClipView(), world_clip_view);

    out_clip[0] =  1.0f;
    out_clip[1] = -1.0f;
    out_clip[2] =  1.0f;
    out_clip[3] = -1.0f;

    for (int i = 0; i < mirror_points; i++)
    {
        vec4 local = {
            mirror_lpos[i][0],
            mirror_lpos[i][1],
            mirror_lpos[i][2],
            1.0f
        };
        vec4 world = {0};
        vec4 clip = {0};
        float w;
        float x;
        float y;

        glm_mat4_mulv(model, local, world);
        glm_mat4_mulv(world_clip_view, world, clip);

        w = fabsf(clip[3]);
        if (w < 0.0001f)
        {
            continue;
        }

        x = clip[0] / w;
        y = clip[1] / w;

        if (x < out_clip[0]) out_clip[0] = x;
        if (x > out_clip[1]) out_clip[1] = x;
        if (y < out_clip[2]) out_clip[2] = y;
        if (y > out_clip[3]) out_clip[3] = y;
        valid = 1;
    }

    if (!valid)
    {
        return 0;
    }

    return 1;
}

int CheckMirrorModel(void *sgd_top)
{
    u_int *prim;
    HeaderSection *hs;

    hs = (HeaderSection *) sgd_top;

    //prim = (u_int *)hs->primitives;

    prim = GetTopProcUnitHeaderPtr(hs, 0);

    if (prim == NULL)
    {
        return 0;
    }

    while (prim[0])
    {
        if (prim[1] == 0x40)
        {
            return prim[2];
        }

        //prim = (u_int *)prim[0];
        prim = GetNextProcUnitHeaderPtr(prim);
    }

    return 0;
}

static inline void inline_asm__mirror_c_line_207(MNODE *dst, MNODE *src)
{
    Vu0CopyVector(dst->vp, src->vp);
    Vu0CopyVector(dst->clip, src->clip);
}

void MirrorInterPNode(MNODE *dst, MNODE *inner, MNODE *outer, ClipData *cldata)
{
    float alpha;
    float ialpha;

    float lower = ((outer->clip[cldata->xyz] - cldata->sgn * outer->clip[3])
               - (inner->clip[cldata->xyz] - cldata->sgn * inner->clip[3]));

    if (lower <= 0.0f)
    {
        lower = 1.0f;
    }

    alpha = (inner->clip[cldata->xyz] - cldata->sgn * inner->clip[3])
            / lower;

    alpha = __builtin_fabsf(alpha);

    ialpha = 1.0f - alpha;

    dst->vp[0] = inner->vp[0] * ialpha + outer->vp[0] * alpha;
    dst->vp[1] = inner->vp[1] * ialpha + outer->vp[1] * alpha;
    dst->vp[2] = inner->vp[2] * ialpha + outer->vp[2] * alpha;
    dst->vp[3] = 1.0f;

    inline_asm__mirror_c_line_47(dst->clip, dst->vp);
}

void SliceMirrorPolygon(MFlipNode *fn, ClipData *cldata)
{
    int i;
    int clip;
    int newnodes;
    int currmask;
    int nextmask;
    MNODE *currN;
    MNODE *nextN;
    MNODE *swap;
    MNODE interN;

    newnodes = 0;
    nextmask = cldata->mask;
    currmask = nextmask << 6;

    for (i = 0; i < fn->nodes; i++)
    {
        currN = &fn->in[i];
        nextN = &fn->in[i + 1];

        clip = MirrorLineClip(currN->clip, nextN->clip);

        if ((clip & (nextmask | currmask)) == 0)
        {
            inline_asm__mirror_c_line_207(&fn->out[newnodes++], currN);
        }
        else if ((clip & currmask) == 0 && (clip & nextmask) != 0)
        {
            inline_asm__mirror_c_line_207(&fn->out[newnodes++], currN);
            MirrorInterPNode(&fn->out[newnodes++], currN, nextN, cldata);
        }
        else if ((clip & currmask) != 0 && (clip & nextmask) == 0)
        {
            MirrorInterPNode(&fn->out[newnodes++], nextN, currN, cldata);
        }
    }

    inline_asm__mirror_c_line_207(&fn->out[newnodes], &fn->out[0]);

    fn->nodes = newnodes;

    swap = fn->in;
    fn->in = fn->out;
    fn->out = swap;
}

#define FIX4_TO_FLT(_val) ((_val) / 16.0f)
static void CalcOuterProduct(float *out, int *p0)
{
    sceVu0FVECTOR tmp0, tmp1, tmp2 = {0};
    float t;

    // This can simplify further, actually, as
    // [2] is 0 then the XY components of the product are 0
    tmp0[0] = FIX4_TO_FLT(p0[0]) - FIX4_TO_FLT(p0[12]);
    tmp0[1] = FIX4_TO_FLT(p0[1]) - FIX4_TO_FLT(p0[13]);
    tmp0[2] = 0.0f;
    tmp0[3] = 0.0f;

    tmp1[0] = FIX4_TO_FLT(p0[12]) - FIX4_TO_FLT(p0[24]);
    tmp1[1] = FIX4_TO_FLT(p0[13]) - FIX4_TO_FLT(p0[25]);
    tmp1[2] = 0.0f;
    tmp1[3] = 0.0f;

    tmp2[0] = tmp0[1] * tmp1[2] - tmp1[1] * tmp0[2];
    tmp2[1] = tmp0[2] * tmp1[0] - tmp1[2] * tmp0[0];
    tmp2[2] = tmp0[0] * tmp1[1] - tmp1[0] * tmp0[1];

    t = out[3];

    out[0] = tmp2[0] * t;
    out[1] = tmp2[1] * t;
    out[2] = tmp2[2] * t;
}

void CalcScreenMirror(sceVu0FVECTOR vp0, sceVu0FVECTOR vp1, sceVu0FVECTOR vp2,
                      float sgn)
{
    int i;
    int allclip;
    qword screen_xyz[20];
    qword *pbase;
    MFlipNode *fn;
    sceVu0FVECTOR out;
    sceVu0FVECTOR v0;
    sceVu0FVECTOR v1;
    sceVu0FVECTOR v2;
    static ClipData cldata[6] = {
        {16, 2,  1.0f},
        {32, 2, -1.0f},
        { 1, 0,  1.0f},
        { 2, 0, -1.0f},
        { 4, 1,  1.0f},
        { 8, 1, -1.0f},
    };

    fn = (MFlipNode *) &SCRATCHPAD[0x6c0];

    fn->in = (MNODE *) fn;
    fn->out = (MNODE *) &SCRATCHPAD[0x800];

    v0[0] = vp0[0];
    v0[1] = vp0[1];
    v0[2] = vp0[2];
    v0[3] = 1.0f;

    v1[0] = vp1[0];
    v1[1] = vp1[1];
    v1[2] = vp1[2];
    v1[3] = 1.0f;

    v2[0] = vp2[0];
    v2[1] = vp2[1];
    v2[2] = vp2[2];
    v2[3] = 1.0f;

    inline_asm__mirror_c_line_62(&fn->in[0], v0);
    inline_asm__mirror_c_line_62(&fn->in[1], v1);
    inline_asm__mirror_c_line_62(&fn->in[2], v2);
    inline_asm__mirror_c_line_62(&fn->in[3], v0);

    fn->nodes = 3;

    inline_asm__mirror_c_line_96(fn->in[0].clip);
    allclip = GetClipValue() & 0x3f;

    inline_asm__mirror_c_line_96(fn->in[1].clip);
    allclip |= GetClipValue() & 0x3f;

    inline_asm__mirror_c_line_96(fn->in[2].clip);
    allclip |= GetClipValue() & 0x3f;

    if (allclip & (0x20 | 0x10))
    {
        if (allclip & cldata[0].mask)
        {
            SliceMirrorPolygon(fn, &cldata[0]);
        }

        if (allclip & cldata[1].mask)
        {
            SliceMirrorPolygon(fn, &cldata[1]);
        }

        for (i = 0; i < fn->nodes; i++)
        {
            inline_asm__mirror_c_line_96(fn->in[i].clip);
            allclip |= GetClipValue() & 0x3f;
        }
    }

    for (i = 2; i < 6; i++)
    {
        if (allclip & cldata[i].mask)
        {
            SliceMirrorPolygon(fn, &cldata[i]);
        }
    }

    if (fn->nodes > 2)
    {
        pbase = screen_xyz;

        for (i = 0; i < 3; i++)
        {
            inline_asm__mirror_c_line_120((Q_WORDDATA*)pbase, fn->in[i].vp);
            pbase += 3;
        }

        out[2] = sgn;

        CalcOuterProduct(out, screen_xyz[0]);

        if (out[2] >= 0.0f)
        {
            mzmax = 2000000.0f;
        }
        else
        {
            pbase = screen_xyz;

            for (i = 0; i < fn->nodes; i++)
            {
                inline_asm__mirror_c_line_120((Q_WORDDATA*)pbase, fn->in[i].vp);

                if (mxmax < pbase[0][0])
                {
                    mxmax = pbase[0][0];
                }

                if (pbase[0][0] < mxmin)
                {
                    mxmin = pbase[0][0];
                }

                if (mymax < pbase[0][1])
                {
                    mymax = pbase[0][1];
                }

                if (pbase[0][1] < mymin)
                {
                    mymin = pbase[0][1];
                }

                if (*(float *) &pbase[1][2] < mzmax)
                {
                    mzmax = *(float *) &pbase[1][2];
                }

                if (mzmin < *(float *) &pbase[1][2])
                {
                    mzmin = *(float *) &pbase[1][2];
                    Vu0CopyVector(mir_center, fn->in[i].vp);
                }

                pbase += 3;
            }
        }
    }
}

void AppendLocalMPos(sceVu0FVECTOR vp)
{
    if (mirror_points >= 5)
    {
        return;
    }

    Vu0CopyVector(mirror_lpos[mirror_points], vp);

    inline_asm__mirror_c_line_47(mirror_cval[mirror_points], vp);

    mirror_points++;
}

int MakeMirrorEnvironment(u_int *prim)
{
    int i;
    int j;
    int loops;
    int gloops;
    int mtype;
    int disp_flg;
    sceVu0FVECTOR cp;
    float sgn;
    float *vp;
    sceVu0FVECTOR vertex = {0};

    mzmin = 0.0f;
    mzmax = 1000000.0f;
    mxmin = 0xa800;
    mxmax = 0;
    mymin = 0xa800;
    mymax = 0;

    mtype = GET_MESH_TYPE(prim);
    gloops = GET_MESH_GLOOPS(prim);

    i = GET_MESH_UNK(prim);

    if (i == 0)
    {
        return 0;
    }

    prim += i;

    vertex[3] = 1.0f;

    disp_flg = 1;

    switch (mtype)
    {
        case 0x12:
            vp = (float *) &vuvnprim[14];

            for (j = 0; j < gloops; j++)
            {
                prim = GetNextUnpackAddr(prim);

                loops = ((u_char *) prim)[2];
                loops -= 2;

                prim++;

                vertex[0] = vp[0];
                vertex[1] = vp[1];
                vertex[2] = vp[2];
                AppendLocalMPos(vertex);

                vertex[0] = vp[6];
                vertex[1] = vp[7];
                vertex[2] = vp[8];
                AppendLocalMPos(vertex);

                prim += 6;
                vp += 12;
                sgn = 1.0f;

                for (i = 0; i < loops; i++)
                {
                    if (prim[0] != 1)
                    {
                        vertex[0] = vp[0];
                        vertex[1] = vp[1];
                        vertex[2] = vp[2];
                        AppendLocalMPos(vertex);

                        CalcScreenMirror(&vp[-12], &vp[-6], &vp[0], sgn);

                        if (mzmax > 1000000.0f)
                        {
                            disp_flg = 0;
                        }
                    }

                    prim += 3;
                    vp += 6;
                    sgn = -sgn;
                }
            }
            break;
        case 0x32:
            vp = (float *) &vuvnprim[14];
            vp = (float *) ((int64_t) vp + ((short *) vuvnprim)[5] * 12);

            for (j = 0; j < gloops; j++)
            {
                prim = GetNextUnpackAddr(prim);

                loops = ((u_char *) prim)[2];
                loops -= 2;

                prim++;

                vertex[0] = vp[0];
                vertex[1] = vp[1];
                vertex[2] = vp[2];
                AppendLocalMPos(vertex);

                vertex[0] = vp[3];
                vertex[1] = vp[4];
                vertex[2] = vp[5];
                AppendLocalMPos(vertex);

                prim += 6;
                vp += 6;
                sgn = 1.0f;

                for (i = 0; i < loops; i++)
                {
                    if (prim[0] != 1)
                    {
                        vertex[0] = vp[0];
                        vertex[1] = vp[1];
                        vertex[2] = vp[2];
                        AppendLocalMPos(vertex);

                        CalcScreenMirror(&vp[-6], &vp[-3], &vp[0], sgn);

                        if (mzmax > 1000000.0f)
                        {
                            disp_flg = 0;
                        }
                    }

                    prim += 3;
                    vp += 3;
                    sgn = -sgn;
                }
            }
            break;
        default:
            return 0;
    }

    mxmax += 16;
    mxmin -= 16;
    mymax += 16;
    mymin -= 16;

    if (0x280 < (mxmax / 16) - 0x6c0)
    {
        mxmax = 0x9400;
    }

    if ((mxmin / 16) + -0x6c0 < 0)
    {
        mxmin = 0x6c00;
    }

    if (0xe0 < (mymax / 16) - 0x790)
    {
        mymax = 0x8700;
    }

    if ((mymin / 16) - 0x790 < 0)
    {
        mymin = 0x7900;
    }

    return 1;
}

void MirrorPrim(u_int *prim)
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
                SetVUMeshDataP(prim);
                break;
            case 2:
                SetMaterialDataVU(prim);
                break;
            case 3:
                Vu0CopyMatrix(*(sceVu0FMATRIX *) &SCRATCHPAD[0x430],
                              lcp[prim[2]].lwmtx);
                _MulMatrix(*(sceVu0FMATRIX *) &SCRATCHPAD[0x90], SgWSMtx,
                           *(sceVu0FMATRIX *) &SCRATCHPAD[0x430]);
                break;
            case 4:
                if (BoundingBoxCalcP(prim) == 0)
                {
                    return;
                }
                break;
            case 5:
                GsImageProcess(prim);
                break;
            case 10:
                LoadTRI2Files(prim);
        }

        prim = GetNextProcUnitHeaderPtr(prim);
    }
}

int PreMirrorPrim(SgCAMERA *camera, u_int *prim)
{
    sceVu0FMATRIX tmpmat;
    int mir_flag = 0;

    if (prim == NULL)
    {
        return 0;
    }

    while (prim[0] != 0)
    {
        switch (prim[1])
        {
            case 0:
                vuvnprim = prim;
                break;
            case 1:
                mirror_points = 0;

                _MulMatrix(tmpmat, camera->wv,
                           *(sceVu0FMATRIX *) &SCRATCHPAD[0x430]);
                Vu0LoadMatrix(tmpmat);

                inline_asm__mirror_c_line_38(*(sceVu0FMATRIX *) &SCRATCHPAD[0xd0]);
                _SetRotTransPersMatrix(*(sceVu0FMATRIX *) &SCRATCHPAD[0x90]);

                return MakeMirrorEnvironment(prim);
            case 3:
                Vu0CopyMatrix(*(sceVu0FMATRIX *) &SCRATCHPAD[0x430], lcp[prim[2]].lwmtx);
                _MulMatrix(*(sceVu0FMATRIX *) &SCRATCHPAD[0x90], SgWSMtx, *(sceVu0FMATRIX *) &SCRATCHPAD[0x430]);
                break;
            case 4:
                mir_flag = CheckBoundingBox(prim);

                if (mir_flag == 0)
                {
                    return 0;
                }

                _MulMatrix(*(sceVu0FMATRIX *) &SCRATCHPAD[0xd0], SgCMVtx, *(sceVu0FMATRIX *) &SCRATCHPAD[0x430]);
                break;
        }

        prim = GetNextProcUnitHeaderPtr(prim);
    }

    return mir_flag;
}

void MirrorBufferFlush(int tlen)
{
    u_int *datap;

    datap = (u_int *) getObjWrk();

    datap[0] = 0x11000000;
    datap[1] = 0;
    datap[2] = 0;
    datap[3] = DMAcall | tlen;

    AppendDmaBuffer(tlen + 1);

    FlushModel(0);
}

static inline void inline_asm__mirror_c_line_639(sceVu0FVECTOR v0,
                                                 sceVu0FVECTOR v1,
                                                 sceVu0FVECTOR v2,
                                                 sceVu0FVECTOR v3)
{
    sceVu0FVECTOR tmp0, tmp1;

    tmp0[0] = v2[0] - v1[0];
    tmp0[1] = v2[1] - v1[1];
    tmp0[2] = v2[2] - v1[2];
    tmp0[3] = v2[3] - v1[3];

    tmp1[0] = v3[0] - v2[0];
    tmp1[1] = v3[1] - v2[1];
    tmp1[2] = v3[2] - v2[2];
    tmp1[3] = v3[3] - v2[3];

    v0[0] = tmp0[1] * tmp1[2] - tmp1[1] * tmp0[2];
    v0[1] = tmp0[2] * tmp1[0] - tmp1[2] * tmp0[0];
    v0[2] = tmp0[0] * tmp1[1] - tmp1[0] * tmp0[1];
}

void CalcMirrorMatrix(SgCAMERA *camera)
{
    sceVu0FMATRIX tmpmat = {0};
    sceVu0FVECTOR centerpos = {0};
    sceVu0FVECTOR norm = {0};
    float center_dot;

    (void)camera;

    Vu0AddVector(centerpos, mirror_lpos[0], mirror_lpos[1]);
    Vu0AddVector(centerpos, centerpos, mirror_lpos[2]);
    Vu0AddVector(centerpos, centerpos, mirror_lpos[3]);

    sceVu0ScaleVector(centerpos, centerpos, 0.25f);

    Vu0LoadMatrix(*(sceVu0FMATRIX *) &SCRATCHPAD[0x430]);

    Vu0ApplyVectorInline(centerpos, centerpos);

    inline_asm__mirror_c_line_639(norm, mirror_lpos[0], mirror_lpos[1],
                                  mirror_lpos[2]);

    _ApplyMatrixXYZ(norm, *(sceVu0FMATRIX *) &SCRATCHPAD[0x430], norm);
    _NormalizeVector(norm, norm);

    sceVu0UnitMatrix(tmpmat);

    center_dot = sceVu0InnerProduct(norm, centerpos);

    tmpmat[0][0] = 1.0f - 2.0f * norm[0] * norm[0];
    tmpmat[0][1] =       - 2.0f * norm[0] * norm[1];
    tmpmat[0][2] =       - 2.0f * norm[0] * norm[2];

    tmpmat[1][0] =       - 2.0f * norm[1] * norm[0];
    tmpmat[1][1] = 1.0f - 2.0f * norm[1] * norm[1];
    tmpmat[1][2] =       - 2.0f * norm[1] * norm[2];

    tmpmat[2][0] =       - 2.0f * norm[2] * norm[0];
    tmpmat[2][1] =       - 2.0f * norm[2] * norm[1];
    tmpmat[2][2] = 1.0f - 2.0f * norm[2] * norm[2];

    tmpmat[3][0] = 2.0f * center_dot * norm[0];
    tmpmat[3][1] = 2.0f * center_dot * norm[1];
    tmpmat[3][2] = 2.0f * center_dot * norm[2];

    Vu0CopyVector(mir_center, centerpos);
    Vu0CopyVector(mir_norm, norm);

    Vu0CopyMatrix(mir_mtx, tmpmat);
}

void MirrorDraw(SgCAMERA *camera, void *sgd_top,
                void (*render_func)(/* parameters unknown */))
{
    int i;
    int num;
    u_int *pk;
    sceVu0FVECTOR clip_value;
    sceVu0FVECTOR gl_clip_value;
    sceVu0FVECTOR draw_clip_value;
    sceGsScissor bak_scissor;
    sceVu0FVECTOR tmpv;
    HeaderSection *hs;
    int has_gl_clip;
    int draw_xmin;
    int draw_ymin;
    int draw_xmax;
    int draw_ymax;

    hs = (HeaderSection *) sgd_top;

    SetClipValue(-1.0f, 1.0f, -1.0f, 1.0f);

    lcp = GetCoordP(hs);
    blocksm = hs->blocks;
    pk = (u_int *) &hs->primitives;
    sgd_top_addr = sgd_top;

    num = CheckMirrorModel(sgd_top);

    if (num == 0 || PreMirrorPrim(camera, (u_int *) GetTopProcUnitHeaderPtr(hs, num)) == 0)
    {
        CalcMirrorMatrix(camera);
        return;
    }

    clip_value[0] = 1.0f;
    clip_value[1] = -1.0f;
    clip_value[2] = 1.0f;
    clip_value[3] = clip_value[1];

    for (i = 0; i < mirror_points; i++)
    {
        sceVu0DivVectorXYZ(tmpv, mirror_cval[i],
                           __builtin_fabsf(mirror_cval[i][3]));

        if (tmpv[0] < clip_value[0])
        {
            clip_value[0] = tmpv[0];
        }

        if (tmpv[0] > clip_value[1])
        {
            clip_value[1] = tmpv[0];
        }

        if (tmpv[1] < clip_value[2])
        {
            clip_value[2] = tmpv[1];
        }

        if (tmpv[1] > clip_value[3])
        {
            clip_value[3] = tmpv[1];
        }
    }

    if (mirror_points == 0)
    {
        CalcMirrorMatrix(camera);
        return;
    }

    if (clip_value[0] < -1.0f)
    {
        clip_value[0] = -1.0f;
    }

    if (clip_value[1] > 1.0f)
    {
        clip_value[1] = 1.0f;
    }

    if (clip_value[2] < -1.0f)
    {
        clip_value[2] = -1.0f;
    }

    if (clip_value[3] > 1.0f)
    {
        clip_value[3] = 1.0f;
    }

    if (mxmax <= mxmin || mymax <= mymin)
    {
        SetMirrorBoundsFromClipValue(clip_value);
    }

    has_gl_clip = GetMirrorClipFromCurrentGlCamera(gl_clip_value);

    draw_clip_value[0] = clip_value[0];
    draw_clip_value[1] = clip_value[1];
    draw_clip_value[2] = clip_value[2];
    draw_clip_value[3] = clip_value[3];

    draw_xmin = mxmin;
    draw_ymin = mymin;
    draw_xmax = mxmax;
    draw_ymax = mymax;

    if (has_gl_clip)
    {
        int gl_xmin;
        int gl_ymin;
        int gl_xmax;
        int gl_ymax;

        gl_clip_value[0] = ClampMirrorNdc(gl_clip_value[0]);
        gl_clip_value[1] = ClampMirrorNdc(gl_clip_value[1]);
        gl_clip_value[2] = ClampMirrorNdc(gl_clip_value[2]);
        gl_clip_value[3] = ClampMirrorNdc(gl_clip_value[3]);

        UnionMirrorClipBounds(draw_clip_value, gl_clip_value);

        ConvertMirrorClipToGsBounds(gl_clip_value,
                                    &gl_xmin, &gl_ymin,
                                    &gl_xmax, &gl_ymax);
        UnionMirrorGsBounds(&draw_xmin, &draw_ymin, &draw_xmax, &draw_ymax,
                            gl_xmin, gl_ymin, gl_xmax, gl_ymax);
    }

    SetClipValue(draw_clip_value[0], draw_clip_value[1],
                 draw_clip_value[2], draw_clip_value[3]);

    bak_scissor = pdrawenv->scissor1;

    pdrawenv->scissor1.SCAX0 = (draw_xmin / 16) + -0x6c0;
    pdrawenv->scissor1.SCAX1 = (draw_xmax / 16) + -0x6c0;
    pdrawenv->scissor1.SCAY0 = (draw_ymin / 16) + -0x790;
    pdrawenv->scissor1.SCAY1 = (draw_ymax / 16) + -0x790;

    SetEnvironment();
    MikuPan_EnableMirrorScissorFromGsBounds(draw_xmin, draw_ymin,
                                            draw_xmax, draw_ymax);
    MirrorRender(camera, render_func);
    MikuPan_ClearMirrorScissorDepth();
    MikuPan_DisableMirrorScissor();

    pdrawenv->scissor1 = bak_scissor;

    SetEnvironment();

    lcp = GetCoordP(hs);
    blocksm = hs->blocks;
    sgd_top_addr = sgd_top;

    ClearMaterialCache((HeaderSection *) sgd_top);
    SetUpSortUnit();
    SetClipValue(-1.0f, 1.0f, -1.0f, 1.0f);
    MikuPan_SetUniform1iToAllShaders(1, "uMirrorSurfacePass");
    MirrorPrim((u_int *) GetTopProcUnitHeaderPtr(hs, num));
    MikuPan_SetUniform1iToAllShaders(0, "uMirrorSurfacePass");
}

void MirrorRender(SgCAMERA *camera,
                  void (*render_func)(/* parameters unknown */))
{
    sceVu0FVECTOR rreg;
    SgCAMERA mir_camera;

    mir_camera = *camera;
    mir_camera.nearz = mzmax;

    SgSetRefCamera(&mir_camera);
    CalcMirrorMatrix(camera);

    MikuPan_SetupMirrorMtx((float*)mir_mtx);

    sceVu0MulMatrix(mir_camera.ws, mir_camera.ws, mir_mtx);
    sceVu0MulMatrix(mir_camera.wc, mir_camera.wc, mir_mtx);
    sceVu0MulMatrix(mir_camera.wcv, mir_camera.wcv, mir_mtx);

    SgSetWsMtx(mir_camera.ws);
    SgSetClipMtx(mir_camera.wc);
    SgSetClipVMtx(mir_camera.wcv);

    rreg[0] = -1.0f;
    rreg[1] = 0.0f;
    rreg[2] = 0.0f;
    rreg[3] = 0.0f;

    SetVF2Register(rreg);

    MikuPan_SetMirrorReflectionPass(1);
    render_func();
    MikuPan_SetMirrorReflectionPass(0);

    rreg[0] = 1.0f;
    rreg[1] = 0.0f;
    rreg[2] = 0.0f;
    rreg[3] = 0.0f;

    SetVF2Register(rreg);
    SgSetRefCamera(camera);

    MikuPan_Setup3D();
}
