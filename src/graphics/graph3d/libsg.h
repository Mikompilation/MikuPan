#ifndef GRAPHICS_GRAPH3D_LIBSG_H
#define GRAPHICS_GRAPH3D_LIBSG_H
#include "typedefs.h"
#include "common.h"


extern u32 g_vu0_r;
extern sceVu0FMATRIX work_matrix_0;
extern sceVu0FMATRIX work_matrix_1;

u32 rnext(void);
void load_matrix_0(sceVu0FMATRIX m0);
void load_matrix_1(sceVu0FMATRIX m0);
void copy_skinned_data(sceVu0FVECTOR *vb, float *s0, float *s1);
float vu0Rand();
void calc_skinned_data(sceVu0FVECTOR *dp, float *_m, float *_n);
void calc_skinned_position(sceVu0FVECTOR dp, sceVu0FVECTOR *v);
void calc_skinned_normal(sceVu0FVECTOR dp, sceVu0FVECTOR *v);
void Vu0CopyVector(sceVu0FVECTOR v0, sceVu0FVECTOR v1);
void Vu0ZeroVector(sceVu0FVECTOR v);
void Vu0AddVector(sceVu0FVECTOR v, sceVu0FVECTOR v0, sceVu0FVECTOR v1);
void Vu0SubVector(sceVu0FVECTOR v, sceVu0FVECTOR v0, sceVu0FVECTOR v1);
void Vu0MulVectorXYZW(sceVu0FVECTOR v, sceVu0FVECTOR v0, sceVu0FVECTOR v1);
void Vu0MulVectorXYZ(sceVu0FVECTOR v, sceVu0FVECTOR v0, sceVu0FVECTOR v1);
void Vu0CopyMatrix(sceVu0FMATRIX m0, sceVu0FMATRIX m1);
void Vu0LoadMatrix(sceVu0FMATRIX m0);
void Vu0ApplyVectorInline(sceVu0FVECTOR v0, sceVu0FVECTOR v1);
float inline_asm__libsg_g_line_463(float *v0, float*v1);
void Vu0MulVectorX(sceVu0FVECTOR v0, sceVu0FVECTOR v1, float sc);
qword* getObjWrk();

#endif //  GRAPHICS_GRAPH3D_LIBSG_H
