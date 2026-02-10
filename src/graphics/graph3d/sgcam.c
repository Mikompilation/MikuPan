#include "sgcam.h"
#include "cglm/call/mat4.h"
#include "common.h"
#include "sgsup.h"
#include "typedefs.h"

#include <stdio.h>

#include "ee/eeregs.h"
#include "ee/eestruct.h"
#include "sce/libvu0.h"

#include "graphics/graph3d/libsg.h"
#include "graphics/graph3d/sgdma.h"
#include "graphics/graph3d/sglib.h"
#include "graphics/graph3d/sgsu.h"
#include "mikupan/mikupan_logging_c.h"
#include "mikupan/rendering/mikupan_renderer.h"

#include <math.h>

sceVu0FVECTOR clip_volume = {1920.0f, 896.0f, 256.0f, 16777000.0f};
static sceVu0FVECTOR clip_volumev = {320.0f, 112.0f, 0.1f, 16777000.0f};

#define VU0_CLIP_X_POS (1 << 0)
#define VU0_CLIP_X_NEG (1 << 1)
#define VU0_CLIP_Y_POS (1 << 2)
#define VU0_CLIP_Y_NEG (1 << 3)
#define VU0_CLIP_Z_POS (1 << 4)
#define VU0_CLIP_Z_NEG (1 << 5)

#define SCRATCHPAD ((u_char *) ps2_virtual_scratchpad)

void SetViewScreenClipMatrix(SgCAMERA *camera, float scrz)
{
    float az;
    float cz;
    float gsx;
    float gsy;
    float gsxv;
    float gsyv;
    float rscrz;
    float zmin;
    float zmax;
    float farz;
    float nearz;
    sceVu0FMATRIX mt = {0};
    mat4 vs_copy = {0};

    zmin = camera->zmin;
    zmax = camera->zmax;
    farz = camera->farz;
    nearz = camera->nearz;

    rscrz = nearz / scrz;

    gsx = clip_volume[0] * rscrz;
    gsy = clip_volume[1] * rscrz;

    gsxv = clip_volumev[0] * rscrz;
    gsyv = clip_volumev[1] * rscrz;

    cz = (-zmax * nearz + zmin * farz) / (-nearz + farz);
    az = (farz * nearz) * (-zmin + zmax) / (-nearz + farz);

    sceVu0UnitMatrix(camera->vs);

    camera->vs[0][0] = scrz;
    camera->vs[1][1] = scrz;
    camera->vs[2][2] = 0.0f;
    camera->vs[2][3] = camera->vs[3][2] = 1.0f;
    camera->vs[3][3] = 0.0f;

    sceVu0UnitMatrix(mt);

    mt[0][0] = camera->ax;
    mt[1][1] = camera->ay;
    mt[2][2] = az;

    mt[3][0] = camera->cx;
    mt[3][1] = camera->cy;
    mt[3][2] = cz;

    glm_mat4_copy(camera->vs, vs_copy);

    sceVu0MulMatrix(camera->vs, mt, vs_copy);

    sceVu0UnitMatrix(camera->vc);

    camera->vc[0][0] = ((nearz + nearz) * camera->ax) / (gsx + gsx);
    camera->vc[1][1] = ((nearz + nearz) * camera->ay) / (gsy + gsy);
    camera->vc[2][2] = (farz + nearz) / (farz - nearz);
    camera->vc[2][3] = 1.0f;
    camera->vc[3][2] = ((farz * nearz) * -2.0f) / (farz - nearz);
    camera->vc[3][3] = 0.0f;

    sceVu0UnitMatrix(camera->vcv);

    camera->vcv[0][0] = ((nearz + nearz) * camera->ax) / (gsxv + gsxv);
    camera->vcv[1][1] = ((nearz + nearz) * camera->ay) / (gsyv + gsyv);
    camera->vcv[2][2] = (farz + nearz) / (farz - nearz);
    camera->vcv[3][2] = ((farz * nearz) * -2.0f) / (farz - nearz);
    camera->vcv[2][3] = 1.0f;
    camera->vcv[3][3] = 0.0f;
}

void SgSetRefCamera(SgCAMERA *camera)
{
    sceVu0FMATRIX tmpmat;
    sceVu0FMATRIX clip_screen;
    float scrz;

    nowcamera = camera;

    if (camera->fov == 0.0f)
    {
        camera->fov = 90.0f;
    }

    scrz = clip_volumev[1] / tanf(camera->fov * 0.5f) * 2;

    sceVu0SubVector(camera->zd, camera->i, camera->p);
    sceVu0SubVector(ieye, camera->p, camera->i);

    camera->yd[0] = 0.0f;
    camera->yd[1] = -1.0f;
    camera->yd[2] = 0.0f;
    camera->yd[3] = 0.0f;

    sceVu0UnitMatrix(tmpmat);
    sceVu0RotMatrixZ(tmpmat, tmpmat, -camera->roll);
    sceVu0ApplyMatrix(camera->yd, tmpmat, camera->yd);
    SetViewScreenClipMatrix(camera, scrz);
    sceVu0CameraMatrix(camera->wv, camera->p, camera->zd, camera->yd);
    sceVu0MulMatrix(camera->ws, camera->vs, camera->wv);
    sceVu0MulMatrix(camera->wc, camera->vc, camera->wv);
    sceVu0MulMatrix(camera->wcv, camera->vcv, camera->wv);

    SgSetWsMtx(camera->ws);
    SgSetClipMtx(camera->wc);
    SgSetClipVMtx(camera->wcv);

    MikuPan_Camera(camera);
}

void SetViewScreenClipMatrixOrtho(SgCAMERA *camera, float scrz)
{
    float az;
    float cz;
    float zmin;
    float zmax;
    float farz;
    float nearz;
    /* 0x0(sp) */ sceVu0FMATRIX mt;
    float f24, f25, f26, f27;// not in STAB

    zmin = camera->zmin;
    zmax = camera->zmax;
    farz = camera->farz;
    nearz = camera->nearz;

    f24 = clip_volume[0];
    f25 = clip_volume[1];

    f26 = clip_volumev[0];
    f27 = clip_volumev[1];

    cz = ((-zmax * nearz) + (+zmin * farz)) / (-nearz + farz);
    az = ((+farz * nearz) * (-zmin + zmax)) / (-nearz + farz);

    sceVu0UnitMatrix(camera->vs);
    sceVu0UnitMatrix(mt);

    mt[0][0] = camera->ax;
    mt[1][1] = camera->ay;
    mt[2][2] = az;

    mt[3][0] = camera->cx;
    mt[3][1] = camera->cy;
    mt[3][2] = cz;

    sceVu0MulMatrix(camera->vs, mt, camera->vs);
    sceVu0UnitMatrix(camera->vc);

    camera->vc[0][0] = camera->ax / f24;
    camera->vc[1][1] = camera->ay / f25;
    camera->vc[2][2] = -2.0f / (farz - nearz);
    camera->vc[3][2] = (farz + nearz) / (farz - nearz);

    sceVu0UnitMatrix(camera->vcv);

    camera->vcv[0][0] = camera->ax / f26;
    camera->vcv[1][1] = camera->ay / f27;
    camera->vcv[2][2] = -2.0f / (farz - nearz);
    camera->vcv[3][2] = (farz + nearz) / (farz - nearz);
}

void SgSetRefCameraOrtho(SgCAMERA *camera)
{
    sceVu0FMATRIX tmpmat;
    sceVu0FMATRIX clip_screen;
    float scrz;

    nowcamera = camera;

    scrz = (clip_volumev[1] / tanf(camera->fov * 0.5f)) * 2;

    sceVu0SubVector(camera->zd, camera->i, camera->p);
    sceVu0SubVector(ieye, camera->p, camera->i);

    camera->yd[0] = 0.0f;
    camera->yd[1] = -1.0f;
    camera->yd[2] = 0.0f;
    camera->yd[3] = 0.0f;

    sceVu0UnitMatrix(tmpmat);
    sceVu0RotMatrixZ(tmpmat, tmpmat, -camera->roll);
    sceVu0ApplyMatrix(camera->yd, tmpmat, camera->yd);

    SetViewScreenClipMatrixOrtho(camera, scrz);

    sceVu0CameraMatrix(camera->wv, camera->p, camera->zd, camera->yd);
    sceVu0MulMatrix(camera->ws, camera->vs, camera->wv);
    sceVu0MulMatrix(camera->wc, camera->vc, camera->wv);
    sceVu0MulMatrix(camera->wcv, camera->vcv, camera->wv);

    SgSetWsMtx(camera->ws);
    SgSetClipMtx(camera->wc);
    SgSetClipVMtx(camera->wcv);
}

void SetClipValue(float minx, float maxx, float miny, float maxy)
{
    clip_value[0] = minx;
    clip_value[1] = maxx;
    clip_value[2] = miny;
    clip_value[3] = maxy;

    if (minx == -1.0f && maxx == 1.0f && miny == -1.0f && maxy == 1.0f)
    {
        clip_value_check = 0;
    }
    else
    {
        clip_value_check = 1;
    }
}

void printClipValue()
{
    info_log("clip_value_check %d\n", clip_value_check);

    printVectorC(clip_value, "clip_value");
}

int BoundClip(sceVu0FVECTOR ed, sceVu0FVECTOR v)
{
    int ret = 0;

    sceVu0FVECTOR *wk0 = work_matrix_0;// in [vf4:vf7]
    sceVu0FVECTOR *wk1 = work_matrix_1;// in [vf8:vf11]
    sceVu0FVECTOR v0 = {0};

    glm_mat4_mulv(wk0, v, v0);
    glm_mat4_mulv(wk1, v, ed);

    /*
    v0[0] = (wk0[0][0] * v[0]) + (wk0[1][0] * v[1]) + (wk0[2][0] * v[2])
            + (wk0[3][0] * 1.0f);
    v0[1] = (wk0[0][1] * v[0]) + (wk0[1][1] * v[1]) + (wk0[2][1] * v[2])
            + (wk0[3][1] * 1.0f);
    v0[2] = (wk0[0][2] * v[0]) + (wk0[1][2] * v[1]) + (wk0[2][2] * v[2])
            + (wk0[3][2] * 1.0f);
    v0[3] = (wk0[0][3] * v[0]) + (wk0[1][3] * v[1]) + (wk0[2][3] * v[2])
            + (wk0[3][3] * 1.0f);

    ed[0] = (wk1[0][0] * v[0]) + (wk1[1][0] * v[1]) + (wk1[2][0] * v[2])
            + (wk1[3][0] * 1.0f);
    ed[1] = (wk1[0][1] * v[0]) + (wk1[1][1] * v[1]) + (wk1[2][1] * v[2])
            + (wk1[3][1] * 1.0f);
    ed[2] = (wk1[0][2] * v[0]) + (wk1[1][2] * v[1]) + (wk1[2][2] * v[2])
            + (wk1[3][2] * 1.0f);
    ed[3] = (wk1[0][3] * v[0]) + (wk1[1][3] * v[1]) + (wk1[2][3] * v[2])
            + (wk1[3][3] * 1.0f);
    */

    if (v0[0] > +fabsf(v0[3]))
        ret |= VU0_CLIP_X_POS;
    if (v0[0] < -fabsf(v0[3]))
        ret |= VU0_CLIP_X_NEG;
    if (v0[1] > +fabsf(v0[3]))
        ret |= VU0_CLIP_Y_POS;
    if (v0[1] < -fabsf(v0[3]))
        ret |= VU0_CLIP_Y_NEG;
    if (v0[2] > +fabsf(v0[3]))
        ret |= VU0_CLIP_Z_POS;
    if (v0[2] < -fabsf(v0[3]))
        ret |= VU0_CLIP_Z_NEG;

    return ret;
}

int BoundClipQ(sceVu0FVECTOR ed, sceVu0FVECTOR v0, sceVu0FVECTOR v1)
{
    int ret = 0;

    sceVu0FVECTOR *wk0 = work_matrix_0;// in [vf4:vf7]
    sceVu0FVECTOR *wk1 = work_matrix_1;// in [vf8:vf11]

    glm_mat4_mulv(wk0, v1, v0);
    glm_mat4_mulv(wk1, v1, ed);

    /*
    v0[0] = (wk0[0][0] * v1[0]) + (wk0[1][0] * v1[1]) + (wk0[2][0] * v1[2])
            + (wk0[3][0] * 1.0f);
    v0[1] = (wk0[0][1] * v1[0]) + (wk0[1][1] * v1[1]) + (wk0[2][1] * v1[2])
            + (wk0[3][1] * 1.0f);
    v0[2] = (wk0[0][2] * v1[0]) + (wk0[1][2] * v1[1]) + (wk0[2][2] * v1[2])
            + (wk0[3][2] * 1.0f);
    v0[3] = (wk0[0][3] * v1[0]) + (wk0[1][3] * v1[1]) + (wk0[2][3] * v1[2])
            + (wk0[3][3] * 1.0f);

    ed[0] = (wk1[0][0] * v1[0]) + (wk1[1][0] * v1[1]) + (wk1[2][0] * v1[2])
            + (wk1[3][0] * 1.0f);
    ed[1] = (wk1[0][1] * v1[0]) + (wk1[1][1] * v1[1]) + (wk1[2][1] * v1[2])
            + (wk1[3][1] * 1.0f);
    ed[2] = (wk1[0][2] * v1[0]) + (wk1[1][2] * v1[1]) + (wk1[2][2] * v1[2])
            + (wk1[3][2] * 1.0f);
    ed[3] = (wk1[0][3] * v1[0]) + (wk1[1][3] * v1[1]) + (wk1[2][3] * v1[2])
            + (wk1[3][3] * 1.0f);
    */

    if (v0[0] > +fabsf(v0[3]))
        ret |= VU0_CLIP_X_POS;
    if (v0[0] < -fabsf(v0[3]))
        ret |= VU0_CLIP_X_NEG;
    if (v0[1] > +fabsf(v0[3]))
        ret |= VU0_CLIP_Y_POS;
    if (v0[1] < -fabsf(v0[3]))
        ret |= VU0_CLIP_Y_NEG;
    if (v0[2] > +fabsf(v0[3]))
        ret |= VU0_CLIP_Z_POS;
    if (v0[2] < -fabsf(v0[3]))
        ret |= VU0_CLIP_Z_NEG;

    v0[0] = v0[0] * (1.0f / v0[3]);
    v0[1] = v0[1] * (1.0f / v0[3]);
    v0[2] = v0[2] * (1.0f / v0[3]);

    return ret;
}

void printBoundingBox(u_int *prim)
{
    sceVu0FVECTOR tmpvec[8];

    printVectorC(*(sceVu0FVECTOR *) &prim[4], "min");
    printVectorC(*(sceVu0FVECTOR *) &prim[32], "max");
}

SgCAMERA *nowcamera = NULL;

int ClipCheck(sceVu0FVECTOR *vec)
{
    int ret;

    if (vec[0][0] > +fabsf(vec[0][3]))
        ret |= VU0_CLIP_X_POS;
    if (vec[0][0] < -fabsf(vec[0][3]))
        ret |= VU0_CLIP_X_NEG;
    if (vec[0][1] > +fabsf(vec[0][3]))
        ret |= VU0_CLIP_Y_POS;
    if (vec[0][1] < -fabsf(vec[0][3]))
        ret |= VU0_CLIP_Y_NEG;
    if (vec[0][2] > +fabsf(vec[0][3]))
        ret |= VU0_CLIP_Z_POS;
    if (vec[0][2] < -fabsf(vec[0][3]))
        ret |= VU0_CLIP_Z_NEG;

    ret <<= 6;

    if (vec[1][0] > +fabsf(vec[1][3]))
        ret |= VU0_CLIP_X_POS;
    if (vec[1][0] < -fabsf(vec[1][3]))
        ret |= VU0_CLIP_X_NEG;
    if (vec[1][1] > +fabsf(vec[1][3]))
        ret |= VU0_CLIP_Y_POS;
    if (vec[1][1] < -fabsf(vec[1][3]))
        ret |= VU0_CLIP_Y_NEG;
    if (vec[1][2] > +fabsf(vec[1][3]))
        ret |= VU0_CLIP_Z_POS;
    if (vec[1][2] < -fabsf(vec[1][3]))
        ret |= VU0_CLIP_Z_NEG;

    ret <<= 6;

    if (vec[2][0] > +fabsf(vec[2][3]))
        ret |= VU0_CLIP_X_POS;
    if (vec[2][0] < -fabsf(vec[2][3]))
        ret |= VU0_CLIP_X_NEG;
    if (vec[2][1] > +fabsf(vec[2][3]))
        ret |= VU0_CLIP_Y_POS;
    if (vec[2][1] < -fabsf(vec[2][3]))
        ret |= VU0_CLIP_Y_NEG;
    if (vec[2][2] > +fabsf(vec[2][3]))
        ret |= VU0_CLIP_Z_POS;
    if (vec[2][2] < -fabsf(vec[2][3]))
        ret |= VU0_CLIP_Z_NEG;

    ret <<= 6;

    if (vec[3][0] > +fabsf(vec[3][3]))
        ret |= VU0_CLIP_X_POS;
    if (vec[3][0] < -fabsf(vec[3][3]))
        ret |= VU0_CLIP_X_NEG;
    if (vec[3][1] > +fabsf(vec[3][3]))
        ret |= VU0_CLIP_Y_POS;
    if (vec[3][1] < -fabsf(vec[3][3]))
        ret |= VU0_CLIP_Y_NEG;
    if (vec[3][2] > +fabsf(vec[3][3]))
        ret |= VU0_CLIP_Z_POS;
    if (vec[3][2] < -fabsf(vec[3][3]))
        ret |= VU0_CLIP_Z_NEG;

    return ret;
}

static inline void asm_1__CheckBoundingBox(sceVu0FMATRIX m0, sceVu0FMATRIX m1,
                                           sceVu0FMATRIX lw)
{
    sceVu0FVECTOR *wk0 = work_matrix_0;// in [vf4:vf7]
    sceVu0FVECTOR *wk1 = work_matrix_1;// in [vf8:vf11]

    glm_mat4_mul(m0, lw, wk0);
    glm_mat4_mul(m0, m1, wk1);

    /*
    wk0[0][0] = (m0[0][0] * lw[0][0]) + (m0[1][0] * lw[0][1])
                + (m0[2][0] * lw[0][2]) + (m0[3][0] * lw[0][3]);
    wk0[0][1] = (m0[0][1] * lw[0][0]) + (m0[1][1] * lw[0][1])
                + (m0[2][1] * lw[0][2]) + (m0[3][1] * lw[0][3]);
    wk0[0][2] = (m0[0][2] * lw[0][0]) + (m0[1][2] * lw[0][1])
                + (m0[2][2] * lw[0][2]) + (m0[3][2] * lw[0][3]);
    wk0[0][3] = (m0[0][3] * lw[0][0]) + (m0[1][3] * lw[0][1])
                + (m0[2][3] * lw[0][2]) + (m0[3][3] * lw[0][3]);

    wk0[1][0] = (m0[0][0] * lw[1][0]) + (m0[1][0] * lw[1][1])
                + (m0[2][0] * lw[1][2]) + (m0[3][0] * lw[1][3]);
    wk0[1][1] = (m0[0][1] * lw[1][0]) + (m0[1][1] * lw[1][1])
                + (m0[2][1] * lw[1][2]) + (m0[3][1] * lw[1][3]);
    wk0[1][2] = (m0[0][2] * lw[1][0]) + (m0[1][2] * lw[1][1])
                + (m0[2][2] * lw[1][2]) + (m0[3][2] * lw[1][3]);
    wk0[1][3] = (m0[0][3] * lw[1][0]) + (m0[1][3] * lw[1][1])
                + (m0[2][3] * lw[1][2]) + (m0[3][3] * lw[1][3]);

    wk0[2][0] = (m0[0][0] * lw[2][0]) + (m0[1][0] * lw[2][1])
                + (m0[2][0] * lw[2][2]) + (m0[3][0] * lw[2][3]);
    wk0[2][1] = (m0[0][1] * lw[2][0]) + (m0[1][1] * lw[2][1])
                + (m0[2][1] * lw[2][2]) + (m0[3][1] * lw[2][3]);
    wk0[2][2] = (m0[0][2] * lw[2][0]) + (m0[1][2] * lw[2][1])
                + (m0[2][2] * lw[2][2]) + (m0[3][2] * lw[2][3]);
    wk0[2][3] = (m0[0][3] * lw[2][0]) + (m0[1][3] * lw[2][1])
                + (m0[2][3] * lw[2][2]) + (m0[3][3] * lw[2][3]);

    wk0[3][0] = (m0[0][0] * lw[3][0]) + (m0[1][0] * lw[3][1])
                + (m0[2][0] * lw[3][2]) + (m0[3][0] * lw[3][3]);
    wk0[3][1] = (m0[0][1] * lw[3][0]) + (m0[1][1] * lw[3][1])
                + (m0[2][1] * lw[3][2]) + (m0[3][1] * lw[3][3]);
    wk0[3][2] = (m0[0][2] * lw[3][0]) + (m0[1][2] * lw[3][1])
                + (m0[2][2] * lw[3][2]) + (m0[3][2] * lw[3][3]);
    wk0[3][3] = (m0[0][3] * lw[3][0]) + (m0[1][3] * lw[3][1])
                + (m0[2][3] * lw[3][2]) + (m0[3][3] * lw[3][3]);

    wk1[0][0] = (m0[0][0] * m1[0][0]) + (m0[1][0] * m1[0][1])
                + (m0[2][0] * m1[0][2]) + (m0[3][0] * m1[0][3]);
    wk1[0][1] = (m0[0][1] * m1[0][0]) + (m0[1][1] * m1[0][1])
                + (m0[2][1] * m1[0][2]) + (m0[3][1] * m1[0][3]);
    wk1[0][2] = (m0[0][2] * m1[0][0]) + (m0[1][2] * m1[0][1])
                + (m0[2][2] * m1[0][2]) + (m0[3][2] * m1[0][3]);
    wk1[0][3] = (m0[0][3] * m1[0][0]) + (m0[1][3] * m1[0][1])
                + (m0[2][3] * m1[0][2]) + (m0[3][3] * m1[0][3]);

    wk1[1][0] = (m0[0][0] * m1[1][0]) + (m0[1][0] * m1[1][1])
                + (m0[2][0] * m1[1][2]) + (m0[3][0] * m1[1][3]);
    wk1[1][1] = (m0[0][1] * m1[1][0]) + (m0[1][1] * m1[1][1])
                + (m0[2][1] * m1[1][2]) + (m0[3][1] * m1[1][3]);
    wk1[1][2] = (m0[0][2] * m1[1][0]) + (m0[1][2] * m1[1][1])
                + (m0[2][2] * m1[1][2]) + (m0[3][2] * m1[1][3]);
    wk1[1][3] = (m0[0][3] * m1[1][0]) + (m0[1][3] * m1[1][1])
                + (m0[2][3] * m1[1][2]) + (m0[3][3] * m1[1][3]);

    wk1[2][0] = (m0[0][0] * m1[2][0]) + (m0[1][0] * m1[2][1])
                + (m0[2][0] * m1[2][2]) + (m0[3][0] * m1[2][3]);
    wk1[2][1] = (m0[0][1] * m1[2][0]) + (m0[1][1] * m1[2][1])
                + (m0[2][1] * m1[2][2]) + (m0[3][1] * m1[2][3]);
    wk1[2][2] = (m0[0][2] * m1[2][0]) + (m0[1][2] * m1[2][1])
                + (m0[2][2] * m1[2][2]) + (m0[3][2] * m1[2][3]);
    wk1[2][3] = (m0[0][3] * m1[2][0]) + (m0[1][3] * m1[2][1])
                + (m0[2][3] * m1[2][2]) + (m0[3][3] * m1[2][3]);

    wk1[3][0] = (m0[0][0] * m1[3][0]) + (m0[1][0] * m1[3][1])
                + (m0[2][0] * m1[3][2]) + (m0[3][0] * m1[3][3]);
    wk1[3][1] = (m0[0][1] * m1[3][0]) + (m0[1][1] * m1[3][1])
                + (m0[2][1] * m1[3][2]) + (m0[3][1] * m1[3][3]);
    wk1[3][2] = (m0[0][2] * m1[3][0]) + (m0[1][2] * m1[3][1])
                + (m0[2][2] * m1[3][2]) + (m0[3][2] * m1[3][3]);
    wk1[3][3] = (m0[0][3] * m1[3][0]) + (m0[1][3] * m1[3][1])
                + (m0[2][3] * m1[3][2]) + (m0[3][3] * m1[3][3]);
    */
}

int CheckBoundingBox(u_int *prim)
{
    sceVu0FVECTOR *pvec;
    sceVu0FVECTOR *tmpvec;
    int i;
    int clip1;
    int xmax_flg;
    int xmin_flg;
    int ymin_flg;
    int ymax_flg;
    // float *lw[4];
    // float *m0[4];
    // float *v1;
    sceVu0FVECTOR *ed;
    sceVu0FVECTOR *vec_690;
    sceVu0FVECTOR *vec_6e0;

    tmpvec = (sceVu0FVECTOR *) &SCRATCHPAD[0x620];
    vec_690 = (sceVu0FVECTOR *) &SCRATCHPAD[0x690];
    ed = (sceVu0FVECTOR *) &SCRATCHPAD[0x6a0];
    vec_6e0 = (sceVu0FVECTOR *) &SCRATCHPAD[0x6e0];

    MikuPan_SetModelTransform(prim);
    //DrawBoundingBox((sceVu0FVECTOR*)&prim[4]);

    lcp[prim[2]].camin = 0;

    asm_1__CheckBoundingBox(SgCMVtx, SgCMtx, lcp[prim[2]].lwmtx);

    // Re-enable this line to have stuff render, right now, nothing is within BoundingBox
    return 1;

    pvec = (sceVu0FVECTOR *) &prim[4];

    if (clip_value_check != 0)
    {
        clip1 = 0x3f;

        for (i = 0; i < 8; i++, pvec++)
        {
            clip1 &= BoundClipQ(ed[i], tmpvec[i], *pvec);
        }

        if (clip1 != 0)
        {
            return 0;
        }

        ymax_flg = 1;
        ymin_flg = 1;
        xmax_flg = 1;
        xmin_flg = 1;

        for (i = 0; i < 8; i++)
        {
            if (clip_value[0] < tmpvec[i][0])
            {
                xmin_flg = 0;
            }

            if (tmpvec[i][0] < clip_value[1])
            {
                xmax_flg = 0;
            }

            if (clip_value[2] < tmpvec[i][1])
            {
                ymin_flg = 0;
            }

            if (tmpvec[i][1] < clip_value[3])
            {
                ymax_flg = 0;
            }
        }

        if (xmin_flg | xmax_flg | ymin_flg | ymax_flg)
        {
            return 0;
        }
    }
    else
    {
        pvec = (sceVu0FVECTOR *) &prim[4];

        clip1 = 0x3f;

        for (i = 0; i < 8; i++, pvec++)
        {
            clip1 &= BoundClip(ed[i], *pvec);
        }

        if (clip1 != 0)
        {
            return 0;
        }
    }

    if (wscissor_flg != 0)
    {
        Vu0LoadMatrix(lcp[prim[2]].lwmtx);

        pvec = (sceVu0FVECTOR *) &prim[4];

        for (i = 0; i < 8; i++, pvec++)
        {
            Vu0ApplyVectorInline(tmpvec[i], *pvec);
        }

        if (wscissor_upper[1] != 0.0f && wscissor_upper[1] < tmpvec[0][1]
            && wscissor_upper[1] < vec_690[0][1])
        {
            return 0;
        }

        if (wscissor_lower[1] != 0.0f && wscissor_lower[1] > tmpvec[0][1]
            && wscissor_lower[1] > vec_690[0][1])
        {
            return 0;
        }
    }

    lcp[prim[2]].camin = 1;

    clip1 = ClipCheck(ed);
    clip1 |= ClipCheck(vec_6e0);

    if (clip1 & 0xffffff)
    {
        edge_check = 1;
    }
    else
    {
        edge_check = 0;
    }

    return 1;
}

int CheckBoundingBoxEdge(u_int *prim)
{
    sceVu0FVECTOR *pvec;
    int clip;
    sceVu0FVECTOR *tmpvec;

    tmpvec = (sceVu0FVECTOR *) &SCRATCHPAD[0x620];

    _SetMulMatrix(SgCMtx, lcp[prim[2]].lwmtx);

    pvec = (sceVu0FVECTOR *) &prim[4];

    Vu0ApplyVectorInline(tmpvec[0], pvec[0]);
    Vu0ApplyVectorInline(tmpvec[1], pvec[1]);
    Vu0ApplyVectorInline(tmpvec[2], pvec[2]);
    Vu0ApplyVectorInline(tmpvec[3], pvec[3]);

    clip = ClipCheck(&tmpvec[0]);

    Vu0ApplyVectorInline(tmpvec[4], pvec[4]);
    Vu0ApplyVectorInline(tmpvec[5], pvec[5]);
    Vu0ApplyVectorInline(tmpvec[6], pvec[6]);
    Vu0ApplyVectorInline(tmpvec[7], pvec[7]);

    clip |= ClipCheck(&tmpvec[4]);

    return (clip & 0x00ffffff) != 0;
}

void SgSetFog(float min, float max, float near, float far, int r, int g, int b)
{
    u_int *datap;

    fog_value[0] = min;
    fog_value[1] = max;
    fog_value[2] =
        (((min - max) * (far + near)) / (far - near) + (min + max)) / 2;
    fog_value[3] = (far * near * (max - min)) / (far - near);

    datap = (u_int *) getObjWrk();

    datap[0] = 0;
    datap[1] = 0;
    datap[2] = VU0_MICRO_ADDR;
    datap[3] = 0x50000002;

    *(u_long *) &datap[4] =
        SCE_GIF_SET_TAG(1, SCE_GS_TRUE, SCE_GS_FALSE, 0, SCE_GIF_PACKED, 1);
    *(u_long *) &datap[6] = SCE_GIF_PACKED_AD;

    *(u_long *) &datap[8] = SCE_GS_SET_FOGCOL(r, g, b);
    *(u_long *) &datap[10] = SCE_GS_FOGCOL;

    AppendDmaBuffer(3);
    FlushModel(0);
}

void SgSetWScissorBox(float ax, float ay, float az, float bx, float by,
                      float bz)
{
    if (ax == 0.0f && ay == 0.0f && az == 0.0f && bx == 0.0f && by == 0.0f
        && bz == 0.0f)
    {
        wscissor_flg = 0;
    }
    else
    {
        wscissor_flg = 1;

        wscissor_upper[0] = ax;
        wscissor_upper[1] = ay;
        wscissor_upper[2] = az;

        wscissor_lower[0] = bx;
        wscissor_lower[1] = by;
        wscissor_lower[2] = bz;
    }
}

void _SetMulMatrixBB(sceVu0FMATRIX m0, sceVu0FMATRIX m1, sceVu0FMATRIX lw)
{
    sceVu0FVECTOR *wk0 = work_matrix_0;// in [vf4:vf7]
    sceVu0FVECTOR *wk1 = work_matrix_1;// in [vf8:vf11]

    glm_mat4_mul(m0, lw, wk0);
    glm_mat4_mul(m0, m1, wk1);

    /*
    wk0[0][0] = (m0[0][0] * lw[0][0]) + (m0[1][0] * lw[0][1])
                + (m0[2][0] * lw[0][2]) + (m0[3][0] * lw[0][3]);
    wk0[0][1] = (m0[0][1] * lw[0][0]) + (m0[1][1] * lw[0][1])
                + (m0[2][1] * lw[0][2]) + (m0[3][1] * lw[0][3]);
    wk0[0][2] = (m0[0][2] * lw[0][0]) + (m0[1][2] * lw[0][1])
                + (m0[2][2] * lw[0][2]) + (m0[3][2] * lw[0][3]);
    wk0[0][3] = (m0[0][3] * lw[0][0]) + (m0[1][3] * lw[0][1])
                + (m0[2][3] * lw[0][2]) + (m0[3][3] * lw[0][3]);

    wk0[1][0] = (m0[0][0] * lw[1][0]) + (m0[1][0] * lw[1][1])
                + (m0[2][0] * lw[1][2]) + (m0[3][0] * lw[1][3]);
    wk0[1][1] = (m0[0][1] * lw[1][0]) + (m0[1][1] * lw[1][1])
                + (m0[2][1] * lw[1][2]) + (m0[3][1] * lw[1][3]);
    wk0[1][2] = (m0[0][2] * lw[1][0]) + (m0[1][2] * lw[1][1])
                + (m0[2][2] * lw[1][2]) + (m0[3][2] * lw[1][3]);
    wk0[1][3] = (m0[0][3] * lw[1][0]) + (m0[1][3] * lw[1][1])
                + (m0[2][3] * lw[1][2]) + (m0[3][3] * lw[1][3]);

    wk0[2][0] = (m0[0][0] * lw[2][0]) + (m0[1][0] * lw[2][1])
                + (m0[2][0] * lw[2][2]) + (m0[3][0] * lw[2][3]);
    wk0[2][1] = (m0[0][1] * lw[2][0]) + (m0[1][1] * lw[2][1])
                + (m0[2][1] * lw[2][2]) + (m0[3][1] * lw[2][3]);
    wk0[2][2] = (m0[0][2] * lw[2][0]) + (m0[1][2] * lw[2][1])
                + (m0[2][2] * lw[2][2]) + (m0[3][2] * lw[2][3]);
    wk0[2][3] = (m0[0][3] * lw[2][0]) + (m0[1][3] * lw[2][1])
                + (m0[2][3] * lw[2][2]) + (m0[3][3] * lw[2][3]);

    wk0[3][0] = (m0[0][0] * lw[3][0]) + (m0[1][0] * lw[3][1])
                + (m0[2][0] * lw[3][2]) + (m0[3][0] * lw[3][3]);
    wk0[3][1] = (m0[0][1] * lw[3][0]) + (m0[1][1] * lw[3][1])
                + (m0[2][1] * lw[3][2]) + (m0[3][1] * lw[3][3]);
    wk0[3][2] = (m0[0][2] * lw[3][0]) + (m0[1][2] * lw[3][1])
                + (m0[2][2] * lw[3][2]) + (m0[3][2] * lw[3][3]);
    wk0[3][3] = (m0[0][3] * lw[3][0]) + (m0[1][3] * lw[3][1])
                + (m0[2][3] * lw[3][2]) + (m0[3][3] * lw[3][3]);

    wk1[0][0] = (m0[0][0] * m1[0][0]) + (m0[1][0] * m1[0][1])
                + (m0[2][0] * m1[0][2]) + (m0[3][0] * m1[0][3]);
    wk1[0][1] = (m0[0][1] * m1[0][0]) + (m0[1][1] * m1[0][1])
                + (m0[2][1] * m1[0][2]) + (m0[3][1] * m1[0][3]);
    wk1[0][2] = (m0[0][2] * m1[0][0]) + (m0[1][2] * m1[0][1])
                + (m0[2][2] * m1[0][2]) + (m0[3][2] * m1[0][3]);
    wk1[0][3] = (m0[0][3] * m1[0][0]) + (m0[1][3] * m1[0][1])
                + (m0[2][3] * m1[0][2]) + (m0[3][3] * m1[0][3]);

    wk1[1][0] = (m0[0][0] * m1[1][0]) + (m0[1][0] * m1[1][1])
                + (m0[2][0] * m1[1][2]) + (m0[3][0] * m1[1][3]);
    wk1[1][1] = (m0[0][1] * m1[1][0]) + (m0[1][1] * m1[1][1])
                + (m0[2][1] * m1[1][2]) + (m0[3][1] * m1[1][3]);
    wk1[1][2] = (m0[0][2] * m1[1][0]) + (m0[1][2] * m1[1][1])
                + (m0[2][2] * m1[1][2]) + (m0[3][2] * m1[1][3]);
    wk1[1][3] = (m0[0][3] * m1[1][0]) + (m0[1][3] * m1[1][1])
                + (m0[2][3] * m1[1][2]) + (m0[3][3] * m1[1][3]);

    wk1[2][0] = (m0[0][0] * m1[2][0]) + (m0[1][0] * m1[2][1])
                + (m0[2][0] * m1[2][2]) + (m0[3][0] * m1[2][3]);
    wk1[2][1] = (m0[0][1] * m1[2][0]) + (m0[1][1] * m1[2][1])
                + (m0[2][1] * m1[2][2]) + (m0[3][1] * m1[2][3]);
    wk1[2][2] = (m0[0][2] * m1[2][0]) + (m0[1][2] * m1[2][1])
                + (m0[2][2] * m1[2][2]) + (m0[3][2] * m1[2][3]);
    wk1[2][3] = (m0[0][3] * m1[2][0]) + (m0[1][3] * m1[2][1])
                + (m0[2][3] * m1[2][2]) + (m0[3][3] * m1[2][3]);

    wk1[3][0] = (m0[0][0] * m1[3][0]) + (m0[1][0] * m1[3][1])
                + (m0[2][0] * m1[3][2]) + (m0[3][0] * m1[3][3]);
    wk1[3][1] = (m0[0][1] * m1[3][0]) + (m0[1][1] * m1[3][1])
                + (m0[2][1] * m1[3][2]) + (m0[3][1] * m1[3][3]);
    wk1[3][2] = (m0[0][2] * m1[3][0]) + (m0[1][2] * m1[3][1])
                + (m0[2][2] * m1[3][2]) + (m0[3][2] * m1[3][3]);
    wk1[3][3] = (m0[0][3] * m1[3][0]) + (m0[1][3] * m1[3][1])
                + (m0[2][3] * m1[3][2]) + (m0[3][3] * m1[3][3]);
    */
}
