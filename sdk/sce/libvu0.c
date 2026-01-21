#include "libvu0.h"

#include <math.h>

void sceVu0ScaleVectorXYZ(sceVu0FVECTOR v0, sceVu0FVECTOR v1, float s)
{
    v0[0] = v1[0] * s;  // X
    v0[1] = v1[1] * s;  // Y
    v0[2] = v1[2] * s;  // Z
    v0[3] = v1[3];      // W (unchanged)
}

void sceVu0AddVector(sceVu0FVECTOR v0, sceVu0FVECTOR v1, sceVu0FVECTOR v2)
{
    v0[0] = v1[0] + v2[0];
    v0[1] = v1[1] + v2[1];
    v0[2] = v1[2] + v2[2];
    v0[3] = v1[3] + v2[3];
}

void sceVu0SubVector(sceVu0FVECTOR v0, sceVu0FVECTOR v1, sceVu0FVECTOR v2)
{
    v0[0] = v1[0] - v2[0];
    v0[1] = v1[1] - v2[1];
    v0[2] = v1[2] - v2[2];
    v0[3] = v1[3] - v2[3];
}

void sceVu0MulVector(sceVu0FVECTOR v0, sceVu0FVECTOR v1, sceVu0FVECTOR v2)
{
    v0[0] = v1[0] * v2[0];
    v0[1] = v1[1] * v2[1];
    v0[2] = v1[2] * v2[2];
    v0[3] = v1[3] * v2[3];
}

void sceVu0CopyVector(sceVu0FVECTOR v0, sceVu0FVECTOR v1)
{
    v0[0] = v1[0];
    v0[1] = v1[1];
    v0[2] = v1[2];
    v0[3] = v1[3];
}

void sceVu0Normalize(sceVu0FVECTOR v0, sceVu0FVECTOR v1)
{
    float len = sqrtf(v1[0]*v1[0] + v1[1]*v1[1] + v1[2]*v1[2]);

    if (len != 0.0f) {
        float inv_len = 1.0f / len;
        v0[0] = v1[0] * inv_len;
        v0[1] = v1[1] * inv_len;
        v0[2] = v1[2] * inv_len;
    } else {
        v0[0] = v0[1] = v0[2] = 0.0f;
    }

    v0[3] = v1[3];  // Preserve W
}

static u_char vectorIsEqual(sceVu0FVECTOR v0, sceVu0FVECTOR v1)
{
    int len = memcmp(v0, v1, sizeof(v0));
    if (!len)
    {
        return 1;
    }
    else
    {
        return 0;
    }
}

float sceVu0InnerProduct(sceVu0FVECTOR v0, sceVu0FVECTOR v1)
{
    if (vectorIsEqual(v0, v1))
    {
        return 0.0f;
    }

    return v0[0] * v1[0] +
       v0[1] * v1[1] +
       v0[2] * v1[2] +
       v0[3] * v1[3];
}

void sceVu0OuterProduct(sceVu0FVECTOR v0, sceVu0FVECTOR v1, sceVu0FVECTOR v2)
{
    v0[0] = v1[1] * v2[2] - v1[2] * v2[1]; // X = (y1*z2 - z1*y2)
    v0[1] = v1[2] * v2[0] - v1[0] * v2[2]; // Y = (z1*x2 - x1*z2)
    v0[2] = v1[0] * v2[1] - v1[1] * v2[0]; // Z = (x1*y2 - y1*x2)
    v0[3] = 0.0f;                          // W = 0
}

void sceVu0UnitMatrix(sceVu0FMATRIX m)
{
    m[0][0] = 1.0f;  m[0][1] = 0.0f;  m[0][2] = 0.0f;  m[0][3] = 0.0f;
    m[1][0] = 0.0f;  m[1][1] = 1.0f;  m[1][2] = 0.0f;  m[1][3] = 0.0f;
    m[2][0] = 0.0f;  m[2][1] = 0.0f;  m[2][2] = 1.0f;  m[2][3] = 0.0f;
    m[3][0] = 0.0f;  m[3][1] = 0.0f;  m[3][2] = 0.0f;  m[3][3] = 1.0f;
}

void sceVu0TransposeMatrix(sceVu0FMATRIX m0, sceVu0FMATRIX m1)
{
    m0[0][0] = m1[0][0];
    m0[0][1] = m1[1][0];
    m0[0][2] = m1[2][0];
    m0[0][3] = m1[3][0];

    m0[1][0] = m1[0][1];
    m0[1][1] = m1[1][1];
    m0[1][2] = m1[2][1];
    m0[1][3] = m1[3][1];

    m0[2][0] = m1[0][2];
    m0[2][1] = m1[1][2];
    m0[2][2] = m1[2][2];
    m0[2][3] = m1[3][2];

    m0[3][0] = m1[0][3];
    m0[3][1] = m1[1][3];
    m0[3][2] = m1[2][3];
    m0[3][3] = m1[3][3];
}

void sceVu0CameraMatrix(sceVu0FMATRIX m, sceVu0FVECTOR p, sceVu0FVECTOR zd, sceVu0FVECTOR yd)
{
    sceVu0FVECTOR xdir, ydir, zdir;

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
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            m0[i][j] =
                m1[i][0] * m2[0][j] +
                m1[i][1] * m2[1][j] +
                m1[i][2] * m2[2][j] +
                m1[i][3] * m2[3][j];
        }
    }
}

void sceVu0InversMatrix(sceVu0FMATRIX m0, sceVu0FMATRIX m1)
{
    sceVu0FMATRIX rotT;  // temporary transpose of rotation part

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
    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 3; j++) {
            m0[i][j] = rotT[i][j];
        }
    }

    // Compute new translation = -R^T * T
    m0[0][3] = -(rotT[0][0]*m1[0][3] + rotT[0][1]*m1[1][3] + rotT[0][2]*m1[2][3]);
    m0[1][3] = -(rotT[1][0]*m1[0][3] + rotT[1][1]*m1[1][3] + rotT[1][2]*m1[2][3]);
    m0[2][3] = -(rotT[2][0]*m1[0][3] + rotT[2][1]*m1[1][3] + rotT[2][2]*m1[2][3]);

    // Last row
    m0[3][0] = 0.0f;
    m0[3][1] = 0.0f;
    m0[3][2] = 0.0f;
    m0[3][3] = 1.0f;
}

void sceVu0CopyMatrix(sceVu0FMATRIX m0, sceVu0FMATRIX m1)
{
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            m0[i][j] = m1[i][j];
        }
    }
}

void sceVu0RotMatrixZ(sceVu0FMATRIX m0, sceVu0FMATRIX m1, float rz)
{
    float cos_r = cosf(rz);
    float sin_r = sinf(rz);

    for (int i = 0; i < 4; i++) {
        m0[i][0] = m1[i][0] * cos_r - m1[i][1] * sin_r;
        m0[i][1] = m1[i][0] * sin_r + m1[i][1] * cos_r;
        m0[i][2] = m1[i][2];
        m0[i][3] = m1[i][3];
    }
}

void sceVu0ApplyMatrix(sceVu0FVECTOR v0, sceVu0FMATRIX m, sceVu0FVECTOR v1)
{
    v0[0] = m[0][0]*v1[0] + m[0][1]*v1[1] + m[0][2]*v1[2] + m[0][3]*v1[3];
    v0[1] = m[1][0]*v1[0] + m[1][1]*v1[1] + m[1][2]*v1[2] + m[1][3]*v1[3];
    v0[2] = m[2][0]*v1[0] + m[2][1]*v1[1] + m[2][2]*v1[2] + m[2][3]*v1[3];
    v0[3] = m[3][0]*v1[0] + m[3][1]*v1[1] + m[3][2]*v1[2] + m[3][3]*v1[3];
}

void sceVu0RotMatrixX(sceVu0FMATRIX m0, sceVu0FMATRIX m1, float rx)
{
    float cos_r = cosf(rx);
    float sin_r = sinf(rx);

    for (int i = 0; i < 4; i++) {
        m0[i][0] = m1[i][0];                            // X unchanged
        m0[i][1] = m1[i][1] * cos_r - m1[i][2] * sin_r; // Y
        m0[i][2] = m1[i][1] * sin_r + m1[i][2] * cos_r; // Z
        m0[i][3] = m1[i][3];                            // W unchanged
    }
}

void sceVu0RotMatrixY(sceVu0FMATRIX m0, sceVu0FMATRIX m1, float ry)
{
    float cos_r = cosf(ry);
    float sin_r = sinf(ry);

    for (int i = 0; i < 4; i++) {
        m0[i][0] = m1[i][0] * cos_r + m1[i][2] * sin_r; // X
        m0[i][1] = m1[i][1];                             // Y unchanged
        m0[i][2] = -m1[i][0] * sin_r + m1[i][2] * cos_r; // Z
        m0[i][3] = m1[i][3];                             // W unchanged
    }
}

void sceVu0TransMatrix(sceVu0FMATRIX m0, sceVu0FMATRIX m1, sceVu0FVECTOR tv)
{
    // Copy rotation/scale part unchanged
    for (int i = 0; i < 3; i++) {
        m0[i][0] = m1[i][0];
        m0[i][1] = m1[i][1];
        m0[i][2] = m1[i][2];
    }

    // Compute new translation = original translation + rotated tv
    m0[0][3] = m1[0][3] + tv[0]*m1[0][0] + tv[1]*m1[0][1] + tv[2]*m1[0][2];
    m0[1][3] = m1[1][3] + tv[0]*m1[1][0] + tv[1]*m1[1][1] + tv[2]*m1[1][2];
    m0[2][3] = m1[2][3] + tv[0]*m1[2][0] + tv[1]*m1[2][1] + tv[2]*m1[2][2];

    // Last row remains unchanged
    m0[3][0] = 0.0f;
    m0[3][1] = 0.0f;
    m0[3][2] = 0.0f;
    m0[3][3] = 1.0f;
}

void sceVu0RotTransPers(sceVu0IVECTOR v0, sceVu0FMATRIX m0, sceVu0FVECTOR v1, int mode)
{
}

void sceVu0ScaleVector(sceVu0FVECTOR v0, sceVu0FVECTOR v1, float s)
{
    v0[0] = v1[0] * s; // X
    v0[1] = v1[1] * s; // Y
    v0[2] = v1[2] * s; // Z
    v0[3] = v1[3] * s; // W
}

void sceVu0RotTransPersN(sceVu0IVECTOR* v0, sceVu0FMATRIX m0, sceVu0FVECTOR* v1, int n, int mode)
{
}

void sceVu0DivVector(sceVu0FVECTOR v0, sceVu0FVECTOR v1, float q)
{
    if (q == 0.0f)
    {
        return;
    }

    float inv_q = 1.0f / q;

    v0[0] = v1[0] * inv_q; // X
    v0[1] = v1[1] * inv_q; // Y
    v0[2] = v1[2] * inv_q; // Z
    v0[3] = v1[3] * inv_q; // W
}

void sceVu0DivVectorXYZ(sceVu0FVECTOR v0, sceVu0FVECTOR v1, float q)
{
    if (q == 0.0f)
    {
        return;
    }

    float inv_q = 1.0f / q;

    v0[0] = v1[0] * inv_q; // X
    v0[1] = v1[1] * inv_q; // Y
    v0[2] = v1[2] * inv_q; // Z
    v0[3] = v1[3];         // W
}

void sceVu0ClampVector(sceVu0FVECTOR v0, sceVu0FVECTOR v1, float min, float max)
{
    for (int i = 0; i < 4; i++) {
        if (v1[i] < min)
            v0[i] = min;
        else if (v1[i] > max)
            v0[i] = max;
        else
            v0[i] = v1[i];
    }
}

void sceVu0InterVector(sceVu0FVECTOR v0, sceVu0FVECTOR v1, sceVu0FVECTOR v2, float r)
{
    float inv_r = 1.0f - r;

    v0[0] = v1[0] * inv_r + v2[0] * r;
    v0[1] = v1[1] * inv_r + v2[1] * r;
    v0[2] = v1[2] * inv_r + v2[2] * r;
    v0[3] = v1[3] * inv_r + v2[3] * r;
}
