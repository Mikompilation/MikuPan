#include "libvu0.h"

#include "cglm/call/mat4.h"
#include "graphics/graph3d/sglib.h"
#include <math.h>

void sceVu0ScaleVectorXYZ(sceVu0FVECTOR v0, sceVu0FVECTOR v1, float s)
{
    glm_vec4_scale(v1, s, v0);
    v0[3] = v1[3];// W (unchanged)
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
    float b = v1[3];
    glm_vec3_normalize_to(v1, v0);
    v0[3] = b;  // Preserve W
}

float sceVu0InnerProduct(sceVu0FVECTOR v0, sceVu0FVECTOR v1)
{
    return glm_vec4_dot(v0, v1);
}

void sceVu0OuterProduct(sceVu0FVECTOR v0, sceVu0FVECTOR v1, sceVu0FVECTOR v2)
{
    glm_vec3_cross(v1, v2, v0);
    v0[3] = 0.0f;// W = 0
}

void sceVu0UnitMatrix(sceVu0FMATRIX m)
{
    glm_mat4_identity(m);
}

void sceVu0TransposeMatrix(sceVu0FMATRIX m0, sceVu0FMATRIX m1)
{
    glmc_mat4_transpose_to(m1, m0);
}

void sceVu0CameraMatrix(sceVu0FMATRIX m, sceVu0FVECTOR p, sceVu0FVECTOR zd,
                        sceVu0FVECTOR yd)
{
    sceVu0FVECTOR xdir, ydir, zdir = {0};

    // Normalize Z direction (camera forward)
    sceVu0Normalize(zdir, zd);

    // Compute X direction = Y × Z
    sceVu0OuterProduct(xdir, yd, zdir);
    sceVu0Normalize(xdir, xdir);

    // Compute Y direction = Z × X
    sceVu0OuterProduct(ydir, zdir, xdir);

    // Build rotation part of matrix (basis vectors)
    m[0][0] = xdir[0];
    m[0][1] = xdir[1];
    m[0][2] = xdir[2];
    m[0][3] = 0.0f;

    m[1][0] = ydir[0];
    m[1][1] = ydir[1];
    m[1][2] = ydir[2];
    m[1][3] = 0.0f;

    m[2][0] = zdir[0];
    m[2][1] = zdir[1];
    m[2][2] = zdir[2];
    m[2][3] = 0.0f;

    // Compute translation (negative dot of basis with position)
    m[3][0] = -sceVu0InnerProduct(xdir, p);
    m[3][1] = -sceVu0InnerProduct(ydir, p);
    m[3][2] = -sceVu0InnerProduct(zdir, p);
    m[3][3] = 1.0f;
}

void sceVu0MulMatrix(sceVu0FMATRIX m0, sceVu0FMATRIX m1, sceVu0FMATRIX m2)
{
    glmc_mat4_mul(m1, m2, m0);
}

void sceVu0InversMatrix(sceVu0FMATRIX m0, sceVu0FMATRIX m1)
{
    //glm_mat4_inv(m1, m0);

    sceVu0FMATRIX rotT = {0};// temporary transpose of rotation part

    // Transpose the upper-left 3x3 rotation part
    rotT[0][0] = m1[0][0];
    rotT[0][1] = m1[1][0];
    rotT[0][2] = m1[2][0];

    rotT[1][0] = m1[0][1];
    rotT[1][1] = m1[1][1];
    rotT[1][2] = m1[2][1];

    rotT[2][0] = m1[0][2];
    rotT[2][1] = m1[1][2];
    rotT[2][2] = m1[2][2];

    // Set the bottom row
    rotT[3][0] = rotT[3][1] = rotT[3][2] = 0.0f;
    rotT[3][3] = 1.0f;

    // Copy rotation to output
    for (int i = 0; i < 3; i++)
    {
        for (int j = 0; j < 3; j++)
        {
            m0[i][j] = rotT[i][j];
        }
    }

    // Compute new translation = -R^T * T
    m0[0][3] = -(rotT[0][0] * m1[0][3] + rotT[0][1] * m1[1][3]
                 + rotT[0][2] * m1[2][3]);
    m0[1][3] = -(rotT[1][0] * m1[0][3] + rotT[1][1] * m1[1][3]
                 + rotT[1][2] * m1[2][3]);
    m0[2][3] = -(rotT[2][0] * m1[0][3] + rotT[2][1] * m1[1][3]
                 + rotT[2][2] * m1[2][3]);

    // Last row
    m0[3][0] = 0.0f;
    m0[3][1] = 0.0f;
    m0[3][2] = 0.0f;
    m0[3][3] = 1.0f;
}

void sceVu0CopyMatrix(sceVu0FMATRIX m0, sceVu0FMATRIX m1)
{
    glm_mat4_copy(m1, m0);
}

void sceVu0RotMatrixZ(sceVu0FMATRIX m0, sceVu0FMATRIX m1, float rz)
{
    //glm_rotate_z(m1, rz, m0);

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
        m0[i][0] = m1[i][0] * cos_r - m1[i][1] * sin_r;
        m0[i][1] = m1[i][0] * sin_r + m1[i][1] * cos_r;
        m0[i][2] = m1[i][2];
        m0[i][3] = m1[i][3];
    }
}

void sceVu0ApplyMatrix(sceVu0FVECTOR v0, sceVu0FMATRIX m, sceVu0FVECTOR v1)
{
    glm_mat4_mulv(m, v1, v0);
}

void sceVu0RotMatrixX(sceVu0FMATRIX m0, sceVu0FMATRIX m1, float rx)
{
    //glm_rotate_x(m1, rx, m0);
    float cos_r = SgCosf(rx);
    float sin_r = SgSinf(rx);

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
        m0[i][0] = m1[i][0];                           // X unchanged
        m0[i][1] = m1[i][1] * cos_r - m1[i][2] * sin_r;// Y
        m0[i][2] = m1[i][1] * sin_r + m1[i][2] * cos_r;// Z
        m0[i][3] = m1[i][3];                           // W unchanged
    }
}

void sceVu0RotMatrixY(sceVu0FMATRIX m0, sceVu0FMATRIX m1, float ry)
{
    //glm_rotated_y(m1, ry, m0);

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
        m0[i][0] = m1[i][0] * cos_r + m1[i][2] * sin_r; // X
        m0[i][1] = m1[i][1];                            // Y unchanged
        m0[i][2] = -m1[i][0] * sin_r + m1[i][2] * cos_r;// Z
        m0[i][3] = m1[i][3];                            // W unchanged
    }
}

void sceVu0TransMatrix(sceVu0FMATRIX m0, sceVu0FMATRIX m1, sceVu0FVECTOR tv)
{
    glm_translate_to(m1, tv, m0);

    // Copy rotation/scale part unchanged
    //for (int i = 0; i < 3; i++) {
    //    m0[i][0] = m1[i][0];
    //    m0[i][1] = m1[i][1];
    //    m0[i][2] = m1[i][2];
    //}
    //// Compute new translation = original translation + rotated tv
    //m0[0][3] = m1[0][3] + tv[0]*m1[0][0] + tv[1]*m1[0][1] + tv[2]*m1[0][2];
    //m0[1][3] = m1[1][3] + tv[0]*m1[1][0] + tv[1]*m1[1][1] + tv[2]*m1[1][2];
    //m0[2][3] = m1[2][3] + tv[0]*m1[2][0] + tv[1]*m1[2][1] + tv[2]*m1[2][2];
    //// Last row remains unchanged
    //m0[3][0] = 0.0f;
    //m0[3][1] = 0.0f;
    //m0[3][2] = 0.0f;
    //m0[3][3] = 1.0f;
}

void sceVu0RotTransPers(sceVu0IVECTOR v0, sceVu0FMATRIX m0, sceVu0FVECTOR v1,
                        int mode)
{
}

void sceVu0ScaleVector(sceVu0FVECTOR v0, sceVu0FVECTOR v1, float s)
{
    glm_vec4_scale(v1, s, v0);
    //v0[0] = v1[0] * s; // X
    //v0[1] = v1[1] * s; // Y
    //v0[2] = v1[2] * s; // Z
    //v0[3] = v1[3] * s; // W
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
    v0[3] = v1[3];

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
