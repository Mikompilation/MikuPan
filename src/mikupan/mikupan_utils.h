#ifndef MIKUPAN_MIKUPAN_UTILS_H
#define MIKUPAN_MIKUPAN_UTILS_H

#define PS2_RESOLUTION_X_FLOAT 640.0f
#define PS2_RESOLUTION_X_INT 640
#define PS2_RESOLUTION_Y_FLOAT 448.0f
#define PS2_RESOLUTION_Y_INT 448
#define PS2_CENTER_X 320.0f
#define PS2_CENTER_Y 224.0f
#include "typedefs.h"

void MikuPan_ConvertPs2ScreenCoordToNDCMaintainAspectRatio(float* out, float screen_width, float screen_height, float x, float y);
void MikuPan_ConvertPs2HalfScreenCoordToNDCMaintainAspectRatio(float* out, float screen_width, float screen_height, float x, float y);
float MikuPan_ConvertScaleColor(unsigned char color_fragment);
float MikuPan_ConvertColorFloat(unsigned char color_fragment);
unsigned char MikuPan_GamePadAxisToPS2(int sdl_axis, int deadzone);
void MikuPan_GetPS2Viewport(int width, int height, float *vx, float *vy, float *vw, float *vh, float *scale);
void MikuPan_FixUV(float* uv, int num);
void MikuPan_SetTriangleIndex(int* triangle_index, int vertex_count, int vertex_offset, int mesh_offset);
unsigned int *MikuPan_GetNextUnpackAddr(unsigned int *prim);
unsigned char* MikuPan_ConvertImageAlpha(unsigned char* img, int width, int height);
unsigned char MikuPan_AdjustPS2Alpha(unsigned char alpha);
int MikuPan_IsVisibleOnScreen(const sceVu0FVECTOR* vector);
void MikuPan_GSToNDC(int Xgs, int Ygs, int Zgs, float* x, float* y, float* z, float window_width, float window_height);
#endif//MIKUPAN_MIKUPAN_UTILS_H
