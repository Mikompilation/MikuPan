#include "libvu0.h"

#include "cglm/call/mat4.h"
#include "graphics/graph3d/sglib.h"
#include <math.h>

void sceVu0ScaleVectorXYZ(sceVu0FVECTOR v0, sceVu0FVECTOR v1, float s)
{
    glm_vec4_scale(v1, s, v0);
}

void sceVu0AddVector(sceVu0FVECTOR v0, sceVu0FVECTOR v1, sceVu0FVECTOR v2)
{
    glm_vec4_add(v1, v2, v0);
}

void sceVu0SubVector(sceVu0FVECTOR v0, sceVu0FVECTOR v1, sceVu0FVECTOR v2)
{
    glm_vec4_sub(v1, v2, v0);
}

void sceVu0MulVector(sceVu0FVECTOR v0, sceVu0FVECTOR v1, sceVu0FVECTOR v2)
{
    glm_vec4_mul(v1, v2, v0);
}

void sceVu0CopyVector(sceVu0FVECTOR v0, sceVu0FVECTOR v1)
{
    glm_vec4_copy(v1, v0);
}

void sceVu0Normalize(sceVu0FVECTOR v0, sceVu0FVECTOR v1)
{
    glm_vec3_normalize_to(v1, v0);
    v0[3] = v1[3];
}

float sceVu0InnerProduct(sceVu0FVECTOR v0, sceVu0FVECTOR v1)
{
    return glm_vec3_dot(v0, v1);
}

void sceVu0OuterProduct(sceVu0FVECTOR v0, sceVu0FVECTOR v1, sceVu0FVECTOR v2)
{
    glm_vec3_cross(v1, v2, v0);
    v0[3] = 0.0f;
}

void sceVu0UnitMatrix(sceVu0FMATRIX m)
{
    glm_mat4_identity(m);
}

void sceVu0TransposeMatrix(sceVu0FMATRIX m0, sceVu0FMATRIX m1)
{
    mat4 out = {0};
    glmc_mat4_transpose_to(m1, out);
    glm_mat4_copy(out, m0);
}

void sceVu0CameraMatrix(sceVu0FMATRIX m, sceVu0FVECTOR p, sceVu0FVECTOR zd,
                        sceVu0FVECTOR yd)
{
    sceVu0FMATRIX m0 = {0};
    sceVu0FVECTOR xd = {0};

    sceVu0UnitMatrix(m0);
    sceVu0OuterProduct(xd, yd, zd);
    sceVu0Normalize(m0[0], xd);
    sceVu0Normalize(m0[2], zd);
    sceVu0OuterProduct(m0[1], m0[2], m0[0]);
    sceVu0TransMatrix(m0, m0, p);
    sceVu0InversMatrix(m, m0);
}

void sceVu0MulMatrix(sceVu0FMATRIX m0, sceVu0FMATRIX m1, sceVu0FMATRIX m2)
{
    mat4 out = {0};
    glmc_mat4_mul(m1, m2, out);

    glm_mat4_copy(out, m0);
}

void sceVu0InversMatrix(sceVu0FMATRIX m0, sceVu0FMATRIX m1)
{
    mat4 out = {0};
    //glm_mat4_inv(m1, out);

    for (int i = 0; i < 3; i++)
    {
        for (int j = 0; j < 3; j++)
        {
            out[i][j] = m1[j][i];
        }
    }
    out[3][0] = -(out[0][0]*m1[3][0] + out[1][0]*m1[3][1] + out[2][0]*m1[3][2]);
    out[3][1] = -(out[0][1]*m1[3][0] + out[1][1]*m1[3][1] + out[2][1]*m1[3][2]);
    out[3][2] = -(out[0][2]*m1[3][0] + out[1][2]*m1[3][1] + out[2][2]*m1[3][2]);
    out[3][3] = 1.0f;

    glm_mat4_copy(out, m0);
}

void sceVu0CopyMatrix(sceVu0FMATRIX m0, sceVu0FMATRIX m1)
{
    glm_mat4_copy(m1, m0);
}

void sceVu0RotMatrixZ(sceVu0FMATRIX m0, sceVu0FMATRIX m1, float rz)
{
    mat4 out = {0};
    float cos_r = SgCosf(rz);
    float sin_r = SgSinf(rz);

    if (fabsf(cos_r) == 1.0f)
    {
        sin_r = 0.0f;
    }
    if (fabsf(sin_r) == 1.0f)
    {
        cos_r = 0.0f;
    }

    for (int i = 0; i < 4; i++)
    {
        out[i][0] = m1[i][0] * cos_r - m1[i][1] * sin_r;
        out[i][1] = m1[i][0] * sin_r + m1[i][1] * cos_r;
        out[i][2] = m1[i][2];
        out[i][3] = m1[i][3];
    }

    sceVu0CopyMatrix(m0, out);
}

void sceVu0ApplyMatrix(sceVu0FVECTOR v0, sceVu0FMATRIX m, sceVu0FVECTOR v1)
{
    vec4 out = {0};
    glm_mat4_mulv(m, v1, out);
    glm_vec4_copy(out, v0);
}

void sceVu0RotMatrixX(sceVu0FMATRIX m0, sceVu0FMATRIX m1, float rx)
{
    float cos_r = SgCosf(rx);
    float sin_r = SgSinf(rx);

    mat4 out = {0};

    if (fabsf(cos_r) == 1.0f)
    {
        sin_r = 0.0f;
    }
    if (fabsf(sin_r) == 1.0f)
    {
        cos_r = 0.0f;
    }

    for (int i = 0; i < 4; i++)
    {
        out[i][0] = m1[i][0];                           // X unchanged
        out[i][1] = m1[i][1] * cos_r - m1[i][2] * sin_r;// Y
        out[i][2] = m1[i][1] * sin_r + m1[i][2] * cos_r;// Z
        out[i][3] = m1[i][3];                           // W unchanged
    }

    sceVu0CopyMatrix(m0, out);
}

void sceVu0RotMatrixY(sceVu0FMATRIX m0, sceVu0FMATRIX m1, float ry)
{
    mat4 out = {0};
    float cos_r = SgCosf(ry);
    float sin_r = SgSinf(ry);

    if (fabsf(cos_r) == 1.0f)
    {
        sin_r = 0.0f;
    }
    if (fabsf(sin_r) == 1.0f)
    {
        cos_r = 0.0f;
    }

    for (int i = 0; i < 4; i++)
    {
        out[i][0] = m1[i][0] * cos_r + m1[i][2] * sin_r; // X
        out[i][1] = m1[i][1];                            // Y unchanged
        out[i][2] = -m1[i][0] * sin_r + m1[i][2] * cos_r;// Z
        out[i][3] = m1[i][3];                            // W unchanged
    }

    sceVu0CopyMatrix(m0, out);
}

void sceVu0TransMatrix(sceVu0FMATRIX m0, sceVu0FMATRIX m1, sceVu0FVECTOR tv)
{
    glm_mat4_copy(m1, m0);
    glm_vec4_add(tv, m0[3], m0[3]);
}

void sceVu0RotTransPers(sceVu0IVECTOR v0, sceVu0FMATRIX m0, sceVu0FVECTOR v1,
                        int mode)
{
}

void sceVu0ScaleVector(sceVu0FVECTOR v0, sceVu0FVECTOR v1, float s)
{
    glm_vec4_scale(v1, s, v0);
}

void sceVu0RotTransPersN(sceVu0IVECTOR *v0, sceVu0FMATRIX m0, sceVu0FVECTOR *v1,
                         int n, int mode)
{
}

void sceVu0DivVector(sceVu0FVECTOR v0, sceVu0FVECTOR v1, float q)
{
    if (q == 0.0f)
    {
        glm_vec4_zero(v0);
        return;
    }

    glm_vec4_divs(v1, q, v0);
}

void sceVu0DivVectorXYZ(sceVu0FVECTOR v0, sceVu0FVECTOR v1, float q)
{
    if (q == 0.0f)
    {
        glm_vec3_zero(v0);
        return;
    }

    glm_vec3_divs(v1, q, v0);
}

void sceVu0ClampVector(sceVu0FVECTOR v0, sceVu0FVECTOR v1, float min, float max)
{
    for (int i = 0; i < 4; i++)
    {
        if (v1[i] < min)
        {
            v0[i] = min;
        }
        else if (v1[i] > max)
        {
            v0[i] = max;
        }
        else
        {
            v0[i] = v1[i];
        }
    }
}

void sceVu0InterVector(sceVu0FVECTOR v0, sceVu0FVECTOR v1, sceVu0FVECTOR v2,
                       float r)
{
    float inv_r = 1.0f - r;

    v0[0] = v1[0] * inv_r + v2[0] * r;
    v0[1] = v1[1] * inv_r + v2[1] * r;
    v0[2] = v1[2] * inv_r + v2[2] * r;
    v0[3] = v1[3] * inv_r + v2[3] * r;
}
