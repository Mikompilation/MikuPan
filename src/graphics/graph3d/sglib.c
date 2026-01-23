#include "sglib.h"
#include "cglm/mat4.h"
#include "common.h"
#include "typedefs.h"

#include <math.h>
#include <stdio.h>

#include "ee/eestruct.h"
#include "sce/libdma.h"
#include "sce/libvu0.h"

#include "graphics/graph3d/sg_dat.h"
#include "graphics/graph3d/sgcam.h"
#include "graphics/graph3d/sgdma.h"
#include "graphics/graph3d/shadow.h"
#include "mikupan/mikupan_logging_c.h"

#define PI 3.1415925f
#define POW2(x) ((x) * (x))

sceVu0FVECTOR work_vf01;
sceVu0FVECTOR work_vf02;

sceVu0FVECTOR vf12reg[2] = {
    {1.0f, 1.0f, 1.0f, -1.0f},
    {1.0f, 0.0f, 0.0f,  0.0f},
};
static sceVu0FVECTOR trad = {0.0f, 1 / (PI * 2), -2 * PI, PI / 180.0f};
static sceVu0FVECTOR tgsinf[2] = {
    {           0.0f,     -1.0f / 6.0f, 1.0f / 120.0f, 0.0f},
    {-1.0f / 5040.0f, 1.0f / 362880.0f,          1.0f, 0.0f},
};
sceVu0FMATRIX SgWSMtx = {0};
sceVu0FMATRIX SgCMtx = {0};
sceVu0FMATRIX SgCMVtx = {0};
sceVu0FMATRIX DLightMtx = {0};
sceVu0FMATRIX SLightMtx = {0};
sceVu0FMATRIX DColorMtx = {0};
sceVu0FMATRIX SColorMtx = {0};
sceVu0FVECTOR TAmbient = {0};
SgPOINTGROUP SgPointGroup[1] = {0};
sceVu0FVECTOR ieye = {0};
SgSPOTGROUP SgSpotGroup[1] = {0};
CoordCache ccahe = {0};
sceVu0FVECTOR fog_value = {0};
sceVu0IVECTOR fog_color = {0};
ShadowTexture shadowtex = {0};
sceVu0FVECTOR clip_value = {0};
sceVu0FVECTOR wscissor_upper = {0};
sceVu0FVECTOR wscissor_lower = {0};

SgLIGHT *SgInfiniteLight = NULL;
SgLIGHT *SgPointLight = NULL;
SgLIGHT *SgSpotLight = NULL;
int model_buffer_size = 0;
int model_tag_size = 0;
sceVu0FVECTOR *vertex_buffer = NULL;
sceVu0FVECTOR *normal_buffer = NULL;
int vnbuf_size = 0;
int loadtri2_flg = 1;
int loadbw_flg = 0;
int set_bw_color = 0;

static inline float asm_1(float rad)
{
    return rad + (truncf(trad[1] * rad)) * trad[2];
}

static inline float asm_2(float rad)
{
    float t, z, ret;

    t = rad;
    z = POW2(rad);

    ret = t;
    t *= z;
    ret += t * tgsinf[0][1];
    t *= z;
    ret += t * tgsinf[0][2];
    t *= z;
    ret += t * tgsinf[1][0];
    t *= z;
    ret += t * tgsinf[1][1];

    if (ret > 1.0f)
        ret = 1.0f;
    if (ret < -1.0f)
        ret = -1.0f;

    return ret;
}

static inline float asm_3(float degree)
{
    float rad = degree * trad[3];
    return rad + (truncf(rad * trad[1]) * trad[2]);
}

void _GetNormalVectorFromVector(sceVu0FVECTOR norm, sceVu0FVECTOR p0,
                                sceVu0FVECTOR p1)
{
    // Compute cross product p1 × p0
    norm[0] = p0[1] * p1[2] - p1[1] * p0[2];// X
    norm[1] = p0[2] * p1[0] - p1[2] * p0[0];// Y
    norm[2] = p0[0] * p1[1] - p1[0] * p0[1];// Z
    // norm[3] = 0.0f;                           // PS2 VU typically keeps W = 0 for direction vectors
}

void WaitVU1()
{
    /// Instant on modern PC
    /*
    asm volatile("\n\
        WVU1:\n\
        bc2t WVU1\n\
        nop\n\
    ");
    */
}

float _TransPPower(float scale)
{
    return scale * 60.0f;
}

float _TransSPower(float scale)
{
    return scale * 200.0f;
}

void Set12Register()
{
    sceVu0FVECTOR vf[2] = {0};

    work_vf01[0] = 1.0f;
    work_vf01[1] = 1.0f;
    work_vf01[2] = 1.0f;
    work_vf01[3] = -1.0f;

    work_vf02[0] = 1.0f;
    work_vf02[1] = 0.0f;
    work_vf02[2] = 0.0f;
    work_vf02[3] = 0.0f;
}

void SetVF2Register(sceVu0FVECTOR vf2reg)
{
    Vu0CopyVector(vf12reg[1], vf2reg);
}

void GetVF2Register(float *vf2reg)
{
    Vu0CopyVector(vf2reg, vf12reg[1]);
}

void printVector(sceVu0FVECTOR v)
{
    info_log("(%f %f %f %f)", v[0], v[1], v[2], v[3]);
}

void printVectorC(sceVu0FVECTOR v, char *s)
{
    info_log("%s (%ff,%ff,%ff,%ff)", s, v[0], v[1], v[2], v[3]);
}

void printMat(sceVu0FMATRIX m0)
{
    int i;

    for (i = 0; i < 4; i++)
    {
        info_log("%f %f %f %f", m0[0][i], m0[1][i], m0[2][i], m0[3][i]);
    }
}

void printMatC(sceVu0FMATRIX m0, char *s)
{
    info_log("%s", s);
    printMat(m0);
}

void printLMatC(sceVu0FMATRIX m0, char *str)
{
    int i;

    info_log("%s", str);

    for (i = 0; i < 3; i++)
    {
        info_log("%f %f %f", m0[0][i], m0[1][i], m0[2][i]);
    }
}

sceVu0FVECTOR *objwork = NULL;
SgSourceChainTag *cachetag = NULL;
int vu1tag_num = 0;
int sbuffer_p = 0;
int edge_check = 0;
int ptag = 0;
qword *SgWorkBase = NULL;
SgVULightCoord *SgLightCoordp = NULL;
SgVULightParallel *SgLightParallelp = NULL;
SgVULightSpot *SgLightSpotp = NULL;
SgVULightPoint *SgLightPointp = NULL;
int SgInfiniteNum = 0;
int SgPointNum = 0;
int SgPointGroupNum = 0;
int SgSpotNum = 0;
int SgSpotGroupNum = 0;
void *sgd_top_addr = NULL;
sceDmaChan *dvif0 = NULL;
sceDmaChan *dvif1 = NULL;
sceDmaChan *dgif = NULL;
sceDmaChan *dfspr = NULL;
SgSourceChainTag *cachetagdbuf[2] = {0};
sceVu0FVECTOR *objworkdbuf[2] = {0};
int tagswap = 0;
int vu_prog_num = 0;
int clip_value_check = 0;
int wscissor_flg = 0;
u_int *pGroupPacket = NULL;

void SgSetWsMtx(sceVu0FMATRIX m0)
{
    Vu0CopyMatrix(SgWSMtx, m0);
}

void SgSetClipMtx(sceVu0FMATRIX m0)
{
    Vu0CopyMatrix(SgCMtx, m0);
}

void SgSetClipVMtx(sceVu0FMATRIX m0)
{
    Vu0CopyMatrix(SgCMVtx, m0);
}

void SgSetWorkBase(qword *base)
{
    SgWorkBase = base;
}

qword *SgGetWorkBase()
{
    return SgWorkBase;
}

void SgSetProjection(float scrz)
{
    // Avoid division by zero
    if (scrz == 0.0f)
    {
        return;
    }

    // On PS2, vf1.w would now hold this value globally;
    // here we just simulate setting it in __work_vf01.
    work_vf01[3] = 1.0f / scrz;
}

float SgGetProjection()
{
    return work_vf01[3];
}

void SgInit3D()
{
    u_int prim[4];
    SgSourceChainTag ftag;

    SgSpotLight = NULL;
    SgPointLight = NULL;
    SgSpotNum = 0;
    SgPointNum = 0;

    loadtri2_flg = 1;
    loadbw_flg = 0;

    dvif0 = sceDmaGetChan(0);
    dvif1 = sceDmaGetChan(1);
    dgif = sceDmaGetChan(2);
    dfspr = sceDmaGetChan(8);

    dfspr->chcr.TTE = 0;
    dgif->chcr.TTE = 1;
    dvif1->chcr.TTE = 0;

    vu_prog_num = -1;
    wscissor_flg = 0;

    SetClipValue(-1.0f, 1.0f, -1.0f, 1.0f);
    InitialDmaBuffer();
    Set12Register();
    ShadowInit();
}

void GetMatrixFromQuaternion(sceVu0FMATRIX quat, sceVu0FVECTOR qvert)
{
    sceVu0FMATRIX rmat;
    sceVu0FMATRIX lmat;

    rmat[0][0] = +qvert[3];
    rmat[0][1] = +qvert[2];
    rmat[0][2] = -qvert[1];
    rmat[0][3] = +qvert[0];
    rmat[1][0] = -qvert[2];
    rmat[1][1] = +qvert[3];
    rmat[1][2] = +qvert[0];
    rmat[1][3] = +qvert[1];
    rmat[2][0] = +qvert[1];
    rmat[2][1] = -qvert[0];
    rmat[2][2] = +qvert[3];
    rmat[2][3] = +qvert[2];
    rmat[3][0] = -qvert[0];
    rmat[3][1] = -qvert[1];
    rmat[3][2] = -qvert[2];
    rmat[3][3] = +qvert[3];

    lmat[0][0] = +qvert[3];
    lmat[0][1] = +qvert[2];
    lmat[0][2] = -qvert[1];
    lmat[0][3] = -qvert[0];
    lmat[1][0] = -qvert[2];
    lmat[1][1] = +qvert[3];
    lmat[1][2] = +qvert[0];
    lmat[1][3] = -qvert[1];
    lmat[2][0] = +qvert[1];
    lmat[2][1] = -qvert[0];
    lmat[2][2] = +qvert[3];
    lmat[2][3] = -qvert[2];
    lmat[3][0] = +qvert[0];
    lmat[3][1] = +qvert[1];
    lmat[3][2] = +qvert[2];
    lmat[3][3] = +qvert[3];

    for (int i = 0; i < 4; ++i)
    {
        quat[i][0] = (rmat[0][0] * lmat[i][0]) + (rmat[1][0] * lmat[i][1])
                     + (rmat[2][0] * lmat[i][2]) + (rmat[3][0] * lmat[i][3]);
        quat[i][1] = (rmat[0][1] * lmat[i][0]) + (rmat[1][1] * lmat[i][1])
                     + (rmat[2][1] * lmat[i][2]) + (rmat[3][1] * lmat[i][3]);
        quat[i][2] = (rmat[0][2] * lmat[i][0]) + (rmat[1][2] * lmat[i][1])
                     + (rmat[2][2] * lmat[i][2]) + (rmat[3][2] * lmat[i][3]);
        quat[i][3] = (rmat[0][3] * lmat[i][0]) + (rmat[1][3] * lmat[i][1])
                     + (rmat[2][3] * lmat[i][2]) + (rmat[3][3] * lmat[i][3]);
    }
}

void GetMatrixRotateAxis(sceVu0FMATRIX quat, float *axis, float theta)
{
    sceVu0FVECTOR qvert;
    float theta2;

    theta2 = theta * 0.5f;
    sceVu0ScaleVector(qvert, axis, SgSinf(theta2));

    qvert[3] = SgCosf(theta2);
    GetMatrixFromQuaternion(quat, qvert);
}

float SgSinf(float rad)
{
    rad = asm_1(rad);

    if (rad < 0.0f)
    {
        rad += (PI * 2);
    }

    if (((PI * 3) / 2) < rad)
    {
        rad -= (PI * 2);
    }
    else if ((PI / 2) < rad)
    {
        rad = PI - rad;
    }

    rad = asm_2(rad);

    return rad;
}

float SgCosf(float rad)
{
    return SgSinf(rad + (PI / 2));
}

float SgSinfd(float degree)
{
    float rad;

    rad = asm_3(degree);

    if (rad < 0.0f)
    {
        rad += (PI * 2);
    }

    if (((PI * 3) / 2) < rad)
    {
        rad -= (PI * 2);
    }
    else if ((PI / 2) < rad)
    {
        rad = PI - rad;
    }

    rad = asm_2(rad);

    return rad;
}

float SgCosfd(float degree)
{
    return SgSinfd(degree + 90.0f);
}

float SgACosf(float ccos)
{
    float sqrt;

    if (isnan(1.0f - ccos * ccos))
    {
        sqrt = 0.0f;
    }
    else
    {
        sqrt = SgSqrtf(1.0f - ccos * ccos);
    }

    return SgAtan2f(sqrt, ccos);
}

static void GetATanf(sceVu0FVECTOR *tmpv)
{
    float t, z, p;
    t = (tmpv[0][0] - 1.0f) / (tmpv[0][0] + 1.0f);
    z = POW2(t);

    p = t * tmpv[0][1];
    t *= z;
    p += t * tmpv[0][2];
    t *= z;
    p += t * tmpv[0][3];
    t *= z;
    p += t * tmpv[1][0];
    t *= z;
    p += t * tmpv[1][1];
    t *= z;
    p += t * tmpv[1][2];
    t *= z;
    p += t * tmpv[1][3];
    t *= z;
    p += t * tmpv[2][0];
    tmpv[0][0] = p + tmpv[2][1];
}

float SgAtanf(float x)
{
    sceVu0FVECTOR tmpv[3] = {0};

    // the followings are most likely polynomial coefficients
    // used to approximate arctan(x)

    if (x < 0.0f)
    {
        return -SgAtanf(-x);
    }

    if (x > 1.0f)
    {
        tmpv[0][0] = 1.0f / x;
        tmpv[0][1] = +0.99999934f;
        tmpv[0][2] = -0.33329856f;
        tmpv[0][3] = +0.19946536f;
        tmpv[1][0] = -0.13908534f;
        tmpv[1][1] = +0.09642004f;
        tmpv[1][2] = -0.055909887f;
        tmpv[1][3] = +0.021861229f;
        tmpv[2][0] = -0.0040540579f;
        tmpv[2][1] = +(PI / 4);

        GetATanf(tmpv);

        return (PI / 2) - tmpv[0][0];
    }

    tmpv[0][0] = x;
    tmpv[0][1] = +0.99999934f;
    tmpv[0][2] = -0.33329856f;
    tmpv[0][3] = +0.19946536f;
    tmpv[1][0] = -0.13908534f;
    tmpv[1][1] = +0.09642004f;
    tmpv[1][2] = -0.055909887f;
    tmpv[1][3] = +0.021861229f;
    tmpv[2][0] = -0.0040540579f;
    tmpv[2][1] = +(PI / 4);

    GetATanf(tmpv);

    return tmpv[0][0];
}

float SgAtan2f(float y, float x)
{
    float rad;

    if (x == 0.0f)
    {
        if (y < 0.0f)
        {
            return -PI / 2;
        }
        else
        {
            return +PI / 2;
        }
    }

    rad = SgAtanf(y / x);

    if (x < 0.0f)
    {
        rad += PI;

        if (PI < rad)
        {
            rad -= PI * 2;
        }
    }

    return rad;
}

float SgSqrtf(float len)
{
    return sqrt(fabs(len));
}

float SgRSqrtf(float len)
{
    if (len == 0.0f)
    {
        return 0.0f;// handle divide by zero
    }

    return 1.0f / sqrtf(len);
}

static void GetSgCalclen(sceVu0FVECTOR tmpv)
{
    tmpv[0] = sqrtf(POW2(tmpv[0]) + POW2(tmpv[1]) + POW2(tmpv[2]));
}

float SgCalcLen(float x, float y, float z)
{
    sceVu0FVECTOR tmpv;

    tmpv[0] = x;
    tmpv[1] = y;
    tmpv[2] = z;

    GetSgCalclen(tmpv);

    return tmpv[0];
}

float _CalcLen(sceVu0FVECTOR v0, sceVu0FVECTOR v1)
{
    sceVu0FVECTOR tmpv;
    // basically an exact copy of _CalcLenASM,
    // this one does just return the first
    // element of the vector however

    return sqrtf(POW2(v0[0] - v1[0]) + POW2(v0[1] - v1[1])
                 + POW2(v0[2] - v1[2]));
}

void _MulRotMatrix(sceVu0FMATRIX ans, sceVu0FMATRIX m0, sceVu0FMATRIX m1)
{
    for (int i = 0; i < 3; ++i)
    {
        ans[i][0] = (m0[0][0] * m1[i][0]) + (m0[1][0] * m1[i][1])
                    + (m0[2][0] * m1[i][2]);
        ans[i][1] = (m0[0][1] * m1[i][0]) + (m0[1][1] * m1[i][1])
                    + (m0[2][1] * m1[i][2]);
        ans[i][2] = (m0[0][2] * m1[i][0]) + (m0[1][2] * m1[i][1])
                    + (m0[2][2] * m1[i][2]);
    }
}

void _MulMatrix(sceVu0FMATRIX ans, sceVu0FMATRIX m0, sceVu0FMATRIX m1)
{
    mat4 out = {0};
    glm_mat4_mul(m0, m1, out);
    glm_mat4_copy(out, ans);

    //for (int i = 0; i < 4; ++i)
    //{
    //    ans[i][0] = (m0[0][0] * m1[i][0]) + (m0[1][0] * m1[i][1])
    //                + (m0[2][0] * m1[i][2]) + (m0[3][0] * m1[i][3]);
    //    ans[i][1] = (m0[0][1] * m1[i][0]) + (m0[1][1] * m1[i][1])
    //                + (m0[2][1] * m1[i][2]) + (m0[3][1] * m1[i][3]);
    //    ans[i][2] = (m0[0][2] * m1[i][0]) + (m0[1][2] * m1[i][1])
    //                + (m0[2][2] * m1[i][2]) + (m0[3][2] * m1[i][3]);
    //    ans[i][3] = (m0[0][3] * m1[i][0]) + (m0[1][3] * m1[i][1])
    //                + (m0[2][3] * m1[i][2]) + (m0[3][3] * m1[i][3]);
    //}
}

void _SetMulMatrix(sceVu0FMATRIX m0, sceVu0FMATRIX m1)
{
    // This is the same as _MulMatrix, but it "returns"
    sceVu0FVECTOR *wk0 = work_matrix_0;// in [vf4:vf7]

    glm_mat4_mul(m0, m1, work_matrix_0);

    //for (int i = 0; i < 4; ++i)
    //{
    //    wk0[i][0] = (m0[0][0] * m1[i][0]) + (m0[1][0] * m1[i][1])
    //                + (m0[2][0] * m1[i][2]) + (m0[3][0] * m1[i][3]);
    //    wk0[i][1] = (m0[0][1] * m1[i][0]) + (m0[1][1] * m1[i][1])
    //                + (m0[2][1] * m1[i][2]) + (m0[3][1] * m1[i][3]);
    //    wk0[i][2] = (m0[0][2] * m1[i][0]) + (m0[1][2] * m1[i][1])
    //                + (m0[2][2] * m1[i][2]) + (m0[3][2] * m1[i][3]);
    //    wk0[i][3] = (m0[0][3] * m1[i][0]) + (m0[1][3] * m1[i][1])
    //                + (m0[2][3] * m1[i][2]) + (m0[3][3] * m1[i][3]);
    //}
}

void _CalcLenASM(sceVu0FVECTOR out, sceVu0FVECTOR v0, sceVu0FVECTOR v1)
{
    // Classic Euclidean distance formula, even though the whole vf14
    // is saved into `out` at the end I think it only makes sense for
    // `x` ([0]) to be used after this function returns. If the other
    // values are used (for some reason) they are:
    // out[1] = POW2(v0[1] - v1[1]);
    // out[2] = POW2(v0[2] - v1[2])
    // out[3] = undefined

    out[0] =
        sqrtf(POW2(v0[0] - v1[0]) + POW2(v0[1] - v1[1]) + POW2(v0[2] - v1[2]));
}

void _NormalizeVector(sceVu0FVECTOR v, sceVu0FVECTOR v0)
{
    glm_vec3_normalize_to(v0, v);
    v[3] = v0[3];  // Preserve W
}

void _NormalizeVector2(sceVu0FVECTOR v, sceVu0FVECTOR v0)
{
    glm_vec3_normalize_to(v0, v);
    v[3] = 0.0f;  // Preserve W
}

void _ApplyRotMatrix(sceVu0FVECTOR v0, sceVu0FVECTOR v1)
{
    sceVu0FVECTOR *wk0 = work_matrix_0;// in [vf4:vf7]

    v0[0] = (wk0[0][0] * v1[0]) + (wk0[1][0] * v1[2]) + (wk0[3][0] * v1[3]);
    v0[1] = (wk0[0][1] * v1[0]) + (wk0[1][1] * v1[2]) + (wk0[3][1] * v1[3]);
    v0[2] = (wk0[0][2] * v1[0]) + (wk0[1][2] * v1[2]) + (wk0[3][2] * v1[3]);
    v0[3] = (wk0[0][3] * v1[0]) + (wk0[1][3] * v1[2]) + (wk0[3][3] * v1[3]);
}

void _ApplyMatrixXYZ(sceVu0FVECTOR v0, sceVu0FMATRIX m, sceVu0FVECTOR v1)
{
    v0[0] = (m[0][0] * v1[0]) + (m[1][0] * v1[1]) + (m[2][0] * v1[2]);
    v0[1] = (m[0][1] * v1[0]) + (m[1][1] * v1[1]) + (m[2][1] * v1[2]);
    v0[2] = (m[0][2] * v1[0]) + (m[1][2] * v1[1]) + (m[2][2] * v1[2]);
}