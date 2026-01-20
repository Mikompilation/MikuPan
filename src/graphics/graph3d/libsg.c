#include "libsg.h"
#include "graphics/graph3d/sglib.h"
#include "typedefs.h"
#include <string.h>

u32 g_vu0_r = 0;
sceVu0FMATRIX work_matrix_0 = {0};
sceVu0FMATRIX work_matrix_1 = {0};

u32 rnext(void)
{
    u32 r = g_vu0_r;
    s32 x = (r >> 4) & 1;
    s32 y = (r >> 22) & 1;
    r <<= 1;
    r ^= (x ^ y);
    r = 0x3f800000 | (r & 0x7fffff); /*23bit*/
    g_vu0_r = r;
    return r;
}

// line 26
void load_matrix_0(sceVu0FMATRIX m0)
{
    memcpy(work_matrix_0, m0, sizeof(sceVu0FMATRIX));
}

// line 36
void load_matrix_1(sceVu0FMATRIX m0)
{
    memcpy(work_matrix_1, m0, sizeof(sceVu0FMATRIX));
}

// Line 73
// Can't be 100% sure on the name, but the code suggests this runs instead
// of `calc_skinned_data`, so it makes sense for it to be a simple verbatim
// copy of pre-skinned data.
// Then again, it's also a trivial copy of 2 qwords into a buffer
// This way, `s0` has pre-skinned pos and `s1` pre-skinned normals
// `vb` is then the destination buffer, like `dp` in the "calc" version
void copy_skinned_data(sceVu0FVECTOR *vb, float *s0, float *s1)
{
    memcpy(vb[0], s0, sizeof(sceVu0FVECTOR));
    memcpy(vb[1], s1, sizeof(sceVu0FVECTOR));
}

// Line 151
float vu0Rand()
{
    u_int rnd = rnext();
    float r = (*(float *) &rnd) - 1.0f;
    //info_log("Random value: %f", r);
    return r;
}

// Line 233
// Basically same LBS calculations `CalcVertexBuffer` asm blobs
// do but in one step, so small tweaks are present
// `m` has the position verts and `n` has the normals
// then results are saved into dp[0] and dp[1] respectively
void calc_skinned_data(sceVu0FVECTOR *dp, float *_m, float *_n)
{
    sceVu0FVECTOR *m = (sceVu0FVECTOR *) _m;// position
    sceVu0FVECTOR *n = (sceVu0FVECTOR *) _n;// normal

    sceVu0FVECTOR *m0 = work_matrix_0;// in [vf4:vf7]
    sceVu0FVECTOR *m1 = work_matrix_1;// in [vf8:vf11]

    sceVu0FVECTOR vf13, vf14;

    vf13[0] = (m0[0][0] * m[0][0]) + (m0[1][0] * m[0][1]) + (m0[2][0] * m[0][2])
              + m0[3][0];
    vf13[1] = (m0[0][1] * m[0][0]) + (m0[1][1] * m[0][1]) + (m0[2][1] * m[0][2])
              + m0[3][1];
    vf13[2] = (m0[0][2] * m[0][0]) + (m0[1][2] * m[0][1]) + (m0[2][2] * m[0][2])
              + m0[3][2];
    vf13[3] = m[0][3];

    vf14[0] = (m1[0][0] * m[1][0]) + (m1[1][0] * m[1][1]) + (m1[2][0] * m[1][2])
              + m1[3][0];
    vf14[1] = (m1[0][1] * m[1][0]) + (m1[1][1] * m[1][1]) + (m1[2][1] * m[1][2])
              + m1[3][1];
    vf14[2] = (m1[0][2] * m[1][0]) + (m1[1][2] * m[1][1]) + (m1[2][2] * m[1][2])
              + m1[3][2];
    vf14[3] = 1.0f - m[0][3];

    dp[0][0] = (vf13[0] * vf13[3]) + (vf14[0] * vf14[3]);
    dp[0][1] = (vf13[1] * vf13[3]) + (vf14[1] * vf14[3]);
    dp[0][2] = (vf13[2] * vf13[3]) + (vf14[2] * vf14[3]);
    dp[0][3] = 1.0f;

    vf13[0] =
        (m0[0][0] * n[0][0]) + (m0[1][0] * n[0][1]) + (m0[2][0] * n[0][2]);
    vf13[1] =
        (m0[0][1] * n[0][0]) + (m0[1][1] * n[0][1]) + (m0[2][1] * n[0][2]);
    vf13[2] =
        (m0[0][2] * n[0][0]) + (m0[1][2] * n[0][1]) + (m0[2][2] * n[0][2]);
    vf13[3] = m[0][3];

    vf14[0] =
        (m1[0][0] * n[1][0]) + (m1[1][0] * n[1][1]) + (m1[2][0] * n[1][2]);
    vf14[1] =
        (m1[0][1] * n[1][0]) + (m1[1][1] * n[1][1]) + (m1[2][1] * n[1][2]);
    vf14[2] =
        (m1[0][2] * n[1][0]) + (m1[1][2] * n[1][1]) + (m1[2][2] * n[1][2]);
    vf14[3] = 1.0f - n[0][3];

    dp[1][0] = ((vf13[0] * vf13[3]) + (vf14[0] * vf14[3])) * m0[3][3];
    dp[1][1] = ((vf13[1] * vf13[3]) + (vf14[1] * vf14[3])) * m0[3][3];
    dp[1][2] = ((vf13[2] * vf13[3]) + (vf14[2] * vf14[3])) * m0[3][3];
    dp[1][3] = 1.0f;
}

// line 285
void calc_skinned_position(sceVu0FVECTOR dp, sceVu0FVECTOR *v)
{
    sceVu0FVECTOR *m0 = work_matrix_0;// in [vf4:vf7]
    sceVu0FVECTOR *m1 = work_matrix_1;// in [vf8:vf11]

    sceVu0FVECTOR vf13, vf14;

    vf13[0] = (m0[0][0] * v[0][0]) + (m0[1][0] * v[0][1]) + (m0[2][0] * v[0][2])
              + m0[3][0];
    vf13[1] = (m0[0][1] * v[0][0]) + (m0[1][1] * v[0][1]) + (m0[2][1] * v[0][2])
              + m0[3][1];
    vf13[2] = (m0[0][2] * v[0][0]) + (m0[1][2] * v[0][1]) + (m0[2][2] * v[0][2])
              + m0[3][2];
    vf13[3] = 1.0f - v[0][3];

    vf14[0] = (m1[0][0] * v[1][0]) + (m1[1][0] * v[1][1]) + (m1[2][0] * v[1][2])
              + m1[3][0];
    vf14[1] = (m1[0][1] * v[1][0]) + (m1[1][1] * v[1][1]) + (m1[2][1] * v[1][2])
              + m1[3][1];
    vf14[2] = (m1[0][2] * v[1][0]) + (m1[1][2] * v[1][1]) + (m1[2][2] * v[1][2])
              + m1[3][2];

    dp[0] = (vf13[0] * v[0][3]) + (vf14[0] * vf13[3]);
    dp[1] = (vf13[1] * v[0][3]) + (vf14[1] * vf13[3]);
    dp[2] = (vf13[2] * v[0][3]) + (vf14[2] * vf13[3]);
    dp[3] = 1.0f;
}

// Line 314
void calc_skinned_normal(sceVu0FVECTOR dp, sceVu0FVECTOR *v)
{
    sceVu0FVECTOR *m0 = work_matrix_0;// in [vf4:vf7]
    sceVu0FVECTOR *m1 = work_matrix_1;// in [vf8:vf11]

    sceVu0FVECTOR vf13, vf14;
    float vf12_w;

    vf13[0] =
        (m0[0][0] * v[0][0]) + (m0[1][0] * v[0][1]) + (m0[2][0] * v[0][2]);
    vf13[1] =
        (m0[0][1] * v[0][0]) + (m0[1][1] * v[0][1]) + (m0[2][1] * v[0][2]);
    vf13[2] =
        (m0[0][2] * v[0][0]) + (m0[1][2] * v[0][1]) + (m0[2][2] * v[0][2]);
    vf13[3] = 1.0f - v[0][3];

    vf12_w = v[0][3] * m0[3][3];
    vf13[3] *= m0[3][3];

    vf14[0] =
        (m1[0][0] * v[1][0]) + (m1[1][0] * v[1][1]) + (m1[2][0] * v[1][2]);
    vf14[1] =
        (m1[0][1] * v[1][0]) + (m1[1][1] * v[1][1]) + (m1[2][1] * v[1][2]);
    vf14[2] =
        (m1[0][2] * v[1][0]) + (m1[1][2] * v[1][1]) + (m1[2][2] * v[1][2]);

    dp[0] = (vf13[0] * vf12_w) + (vf14[0] * vf13[3]);
    dp[1] = (vf13[1] * vf12_w) + (vf14[1] * vf13[3]);
    dp[2] = (vf13[2] * vf12_w) + (vf14[2] * vf13[3]);
    dp[3] = 1.0f;
}

// Line 351
void Vu0CopyVector(sceVu0FVECTOR v0, sceVu0FVECTOR v1)
{
    memcpy(v0, v1, sizeof(sceVu0FVECTOR));
}

// Line 366
void Vu0ZeroVector(sceVu0FVECTOR v)
{
    v[0] = 0.0f;
    v[1] = 0.0f;
    v[2] = 0.0f;
    v[3] = 1.0f;
}

// Line 373
void Vu0AddVector(sceVu0FVECTOR v, sceVu0FVECTOR v0, sceVu0FVECTOR v1)
{
    v[0] = v0[0] + v1[0];
    v[1] = v0[1] + v1[1];
    v[2] = v0[2] + v1[2];
    v[3] = v0[3] + v1[3];
}

// Line 383
void Vu0SubVector(sceVu0FVECTOR v, sceVu0FVECTOR v0, sceVu0FVECTOR v1)
{
    v[0] = v0[0] - v1[0];
    v[1] = v0[1] - v1[1];
    v[2] = v0[2] - v1[2];
    v[3] = v0[3] - v1[3];
}

// Line 393
void Vu0MulVectorXYZW(sceVu0FVECTOR v, sceVu0FVECTOR v0, sceVu0FVECTOR v1)
{
    v[0] = v0[0] * v1[0];
    v[1] = v0[1] * v1[1];
    v[2] = v0[2] * v1[2];
    v[3] = v0[3] * v1[3];
}

// Line 403
void Vu0MulVectorXYZ(sceVu0FVECTOR v, sceVu0FVECTOR v0, sceVu0FVECTOR v1)
{
    v[0] = v0[0] * v1[0];
    v[1] = v0[1] * v1[1];
    v[2] = v0[2] * v1[2];
}

// Line 412
void Vu0CopyMatrix(sceVu0FMATRIX m0, sceVu0FMATRIX m1)
{
    memcpy(m0, m1, sizeof(sceVu0FMATRIX));
}

// Line 438
void Vu0LoadMatrix(sceVu0FMATRIX m0)
{
    memcpy(work_matrix_0, m0, sizeof(sceVu0FMATRIX));
}

// Line 448
void Vu0ApplyVectorInline(sceVu0FVECTOR v0, sceVu0FVECTOR v1)
{
    sceVu0FVECTOR *m0 = work_matrix_0;// in [vf4:vf7]

    v0[0] = (m0[0][0] * v1[0]) + (m0[1][0] * v1[1]) + (m0[2][0] * v1[2])
            + (m0[3][0] * 1.0f);
    v0[1] = (m0[0][1] * v1[0]) + (m0[1][1] * v1[1]) + (m0[2][1] * v1[2])
            + (m0[3][1] * 1.0f);
    v0[2] = (m0[0][2] * v1[0]) + (m0[1][2] * v1[1]) + (m0[2][2] * v1[2])
            + (m0[3][2] * 1.0f);
    v0[3] = (m0[0][3] * v1[0]) + (m0[1][3] * v1[1]) + (m0[2][3] * v1[2])
            + (m0[3][3] * 1.0f);
}

// Line 463
float inline_asm__libsg_g_line_463(float *v0, float *v1)
{
    float ret;

    ret = (v0[0] * v1[0]) + (v0[1] * v1[1]) + (v0[2] * v1[2]);

    return ret;
}

// Line 478
void Vu0MulVectorX(sceVu0FVECTOR v0, sceVu0FVECTOR v1, float sc)
{
    v0[0] = v1[0] * sc;
    v0[1] = v1[1] * sc;
    v0[2] = v1[2] * sc;
}

// Line 494
qword *getObjWrk()
{
    return (qword *) &objwork[sbuffer_p];
}