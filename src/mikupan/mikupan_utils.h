#ifndef MIKUPAN_MIKUPAN_UTILS_H
#define MIKUPAN_MIKUPAN_UTILS_H

#define PS2_RESOLUTION_X_FLOAT 640.0f
#define PS2_RESOLUTION_X_INT 640
#define PS2_RESOLUTION_Y_FLOAT 448.0f
#define PS2_RESOLUTION_Y_INT 448
#define PS2_CENTER_X 320.0f
#define PS2_CENTER_Y 224.0f

void MikuPan_ConvertPs2ScreenCoordToNDCMaintainAspectRatio(float* out, float screen_width, float screen_height, float x, float y);
float MikuPan_ConvertScaleColor(unsigned char color_fragment);
unsigned char MikuPan_GamePadAxisToPS2(int sdl_axis, int deadzone);
void MikuPan_GetPS2Viewport(int width, int height,
    float *vx, float *vy, float *vw, float *vh, float *scale);
void MikuPan_FixUV(float* uv, int num);
#endif//MIKUPAN_MIKUPAN_UTILS_H
