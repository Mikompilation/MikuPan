#include "mikupan_utils.h"
#include <stdlib.h>

void MikuPan_ConvertPs2ScreenCoordToNDCMaintainAspectRatio(float* out, float screen_width, float screen_height, float x, float y)
{
    float scale_x = screen_width  / PS2_RESOLUTION_X_FLOAT;
    float scale_y = screen_height / PS2_RESOLUTION_Y_FLOAT;
    float scale   = (scale_x < scale_y) ? scale_x : scale_y;

    float viewport_w = PS2_RESOLUTION_X_FLOAT * scale;
    float viewport_h = PS2_RESOLUTION_Y_FLOAT * scale;

    float viewport_x = (screen_width  - viewport_w) * 0.5f;
    float viewport_y = (screen_height - viewport_h) * 0.5f;

    // Compute destination rectangle in screen space
    float x0 = viewport_x + x * scale;
    float y0 = viewport_y + y * scale;

    // Convert screen space to OpenGL NDC (-1 to 1)
    out[0] = (x0 / screen_width) * 2.0f - 1.0f;
    out[1] = 1.0f - (y0 / screen_height) * 2.0f;
}

float MikuPan_ConvertScaleColor(unsigned char color_fragment)
{
    if (color_fragment <= 127)
    {
        color_fragment = color_fragment << 1;
    }
    else
    {
        color_fragment = 0xFF;
    }

    return (float)color_fragment / 255.0f;
}

unsigned char MikuPan_GamePadAxisToPS2(int sdl_axis, int deadzone)
{
    if (abs(sdl_axis) < deadzone)
    {
        sdl_axis = 0;
    }

    if (sdl_axis < -32768)
    {
        sdl_axis = -32768;
    }

    if (sdl_axis >  32767)
    {
        sdl_axis =  32767;
    }

    int ps2 = (sdl_axis + 32768) * 255 / 65535;
    return (unsigned char)ps2;
}

void MikuPan_GetPS2Viewport(int width, int height,
    float *vx, float *vy, float *vw, float *vh, float *scale)
{
    float sx = (float)width  / PS2_RESOLUTION_X_FLOAT;
    float sy = (float)height / PS2_RESOLUTION_Y_FLOAT;

    *scale = (sx < sy) ? sx : sy;

    *vw = PS2_RESOLUTION_X_FLOAT * (*scale);
    *vh = PS2_RESOLUTION_Y_FLOAT * (*scale);

    *vx = ((float)width  - *vw) * 0.5f;
    *vy = ((float)height - *vh) * 0.5f;
}

void MikuPan_FixUV(float* uv, int num)
{
    typedef struct
    {
        float u;
        float v;
    } v2;

    v2* uvf = (v2*)uv;

    for (int i = 0; i < num; i++)
    {
        if (*((int*)&uvf[i].v) == 1)
        {
            uvf[i].v = uvf[i - 2].v;
        }
    }
}