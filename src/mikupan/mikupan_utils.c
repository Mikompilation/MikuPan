#include "mikupan_utils.h"

#include <math.h>
#include <stdlib.h>

void MikuPan_ConvertPs2ScreenCoordToNDCMaintainAspectRatio(float *out,
                                                           float screen_width,
                                                           float screen_height,
                                                           float x, float y)
{
    float scale_x = screen_width / PS2_RESOLUTION_X_FLOAT;
    float scale_y = screen_height / PS2_RESOLUTION_Y_FLOAT;
    float scale = (scale_x < scale_y) ? scale_x : scale_y;

    float viewport_w = PS2_RESOLUTION_X_FLOAT * scale;
    float viewport_h = PS2_RESOLUTION_Y_FLOAT * scale;

    float viewport_x = (screen_width - viewport_w) * 0.5f;
    float viewport_y = (screen_height - viewport_h) * 0.5f;

    // Compute destination rectangle in screen space
    float x0 = viewport_x + x * scale;
    float y0 = viewport_y + y * scale;

    // Convert screen space to OpenGL NDC (-1 to 1)
    out[0] = (x0 / screen_width) * 2.0f - 1.0f;
    out[1] = 1.0f - (y0 / screen_height) * 2.0f;
}

void MikuPan_ConvertPs2HalfScreenCoordToNDCMaintainAspectRatio(
    float *out, float screen_width, float screen_height, float x, float y)
{
    float scale_x = screen_width / PS2_RESOLUTION_X_FLOAT;
    float scale_y = screen_height / PS2_RESOLUTION_Y_FLOAT;
    float scale = (scale_x < scale_y) ? scale_x : scale_y;

    float viewport_w = PS2_RESOLUTION_X_FLOAT * scale;
    float viewport_h = PS2_RESOLUTION_Y_FLOAT * scale;

    float viewport_x = (screen_width - viewport_w) * 0.5f;
    float viewport_y = (screen_height - viewport_h) * 0.5f;

    // Compute destination rectangle in screen space
    float x0 = viewport_x + (x + (PS2_RESOLUTION_X_FLOAT / 2)) * scale;
    float y0 = viewport_y + (y + (PS2_RESOLUTION_Y_FLOAT / 2)) * scale;

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

    return (float) color_fragment / 255.0f;
}

float MikuPan_ConvertColorFloat(unsigned char color_fragment)
{
    return (float) color_fragment / 255.0f;
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

    if (sdl_axis > 32767)
    {
        sdl_axis = 32767;
    }

    int ps2 = (sdl_axis + 32768) * 255 / 65535;
    return (unsigned char) ps2;
}

void MikuPan_GetPS2Viewport(int width, int height, float *vx, float *vy,
                            float *vw, float *vh, float *scale)
{
    float sx = (float) width / PS2_RESOLUTION_X_FLOAT;
    float sy = (float) height / PS2_RESOLUTION_Y_FLOAT;

    *scale = (sx < sy) ? sx : sy;

    *vw = PS2_RESOLUTION_X_FLOAT * (*scale);
    *vh = PS2_RESOLUTION_Y_FLOAT * (*scale);

    *vx = ((float) width - *vw) * 0.5f;
    *vy = ((float) height - *vh) * 0.5f;
}

void MikuPan_FixUV(float *uv, int num)
{
    typedef struct
    {
        float u;
        float v;
    } v2;

    v2 *uvf = (v2 *) uv;

    for (int i = 2; i < num; i++)
    {
        if (*((int *) &uvf[i].v) == 1)
        {
            uvf[i].v = uvf[i - 2].v;
        }
    }
}

void MikuPan_FixColors(float *color_buf, int num)
{
    typedef struct
    {
        float r;
        float g;
        float b;
    } colour;

    colour *uvf = (colour *) color_buf;

    for (int i = 2; i < num; i++)
    {
        if (*((int *) &uvf[i].r) == 1)
        {
            uvf[i].r = uvf[i - 2].r;
            uvf[i].g = uvf[i - 2].g;
            uvf[i].b = uvf[i - 2].b;
        }
    }
}

int MikuPan_SetTriangleIndex(int *triangle_index, int vertex_count,
                             int vertex_offset, int index_write_offset)
{
    int idx = index_write_offset;
    for (int j = 0; j < vertex_count - 2; j++)
    {
        if (j % 2 == 0)
        {
            triangle_index[idx++] = vertex_offset + j;
            triangle_index[idx++] = vertex_offset + j + 1;
            triangle_index[idx++] = vertex_offset + j + 2;
        }
        else
        {
            triangle_index[idx++] = vertex_offset + j + 1;
            triangle_index[idx++] = vertex_offset + j;
            triangle_index[idx++] = vertex_offset + j + 2;
        }
    }
    return idx - index_write_offset;
}

unsigned int *MikuPan_GetNextUnpackAddr(unsigned int *prim)
{
    while (1)
    {
        if ((*prim & 0x60000000) == 0x60000000)
        {
            return prim;
        }

        prim++;
    }
}

unsigned char *MikuPan_ConvertImageAlpha(unsigned char *img, int width,
                                         int height)
{
    unsigned int *image_data = (unsigned int *) malloc(width * height * 4);
    unsigned int *raw_pixel = (unsigned int *) img;

    typedef struct
    {
        unsigned char r;
        unsigned char g;
        unsigned char b;
        unsigned char a;
    } RGBA;

    for (int i = 0; i < height; i++)
    {
        for (int k = 0; k < width; k++)
        {
            RGBA *pixel = (RGBA *) &raw_pixel[(i * width) + k];
            pixel->a = MikuPan_AdjustPS2Alpha(pixel->a);

            image_data[(i * width) + k] = *(unsigned int *) pixel;
        }
    }

    return (unsigned char *) image_data;
}

unsigned char MikuPan_AdjustPS2Alpha(unsigned char alpha)
{
    if (alpha <= 127)
    {
        return alpha << 1;
    }

    return 0xFF;
}

int MikuPan_IsVisibleOnScreen(const sceVu0FVECTOR *vector)
{
    int w = 0;

    for (int i = 0; i < 4; i++)
    {
        if (vector[i][0] < -vector[i][3] || vector[i][0] > vector[i][3])
        {
            w = 1;
        }

        if (vector[i][1] < -vector[i][3] || vector[i][1] > vector[i][3])
        {
            w = 1;
        }

        if (vector[i][2] < 0.0 || vector[i][2] > vector[i][3])
        {
            w = 1;
        }
    }


    return w;
}

void MikuPan_GSToNDC(int Xgs, int Ygs, int Zgs, float* x, float* y, float* z, float window_width, float window_height)
{
    /*
    float px = ((float)Xgs / 16.0f) - 1728.0f;
    float py = ((float)Ygs / 16.0f) - 1936.0f;

    float scale_x = window_width / PS2_RESOLUTION_X_FLOAT;
    float scale_y = window_height / PS2_RESOLUTION_Y_FLOAT;
    float scale = (scale_x < scale_y) ? scale_x : scale_y;

    float viewport_w = PS2_RESOLUTION_X_FLOAT * scale;
    float viewport_h = PS2_RESOLUTION_Y_FLOAT * scale;

    float viewport_x = (window_width - viewport_w) * 0.5f;
    float viewport_y = (window_height - viewport_h) * 0.5f;

    float x0 = viewport_x + px * scale;
    float y0 = viewport_y + py * scale;

    *x = (x0 / window_width) * 2.0f - 1.0f;
    *y = 1.0f - (y0 / window_height) * 2.0f;
    **/

    float screenX = ((float)Xgs / 16.0f) - 1728.0f;
    float screenY = ((float)Ygs / 16.0f) - 1936.0f;

    /*
    float out[2];
    MikuPan_ConvertPs2ScreenCoordToNDCMaintainAspectRatio(out, window_width, window_height, screenX, screenY);
    *x = out[0];

    MikuPan_ConvertPs2HalfScreenCoordToNDCMaintainAspectRatio(out, window_width, window_height, screenX, screenY);
    *y = out[1];
    **/

    *x = (screenX / PS2_RESOLUTION_X_FLOAT) * 2.0f - 1.0f;
    *y = -(screenY / PS2_CENTER_Y) * 2.0f + 1.0f;

    float z01 = (float)((float)Zgs - 255.0f) / (32768.0f - 255.0f);
    *z = z01 * 2.0f - 1.0f;
}

void MikuPan_ConvertPs2GSCoordToNDC(float *out,
                                    float window_width, float window_height,
                                    float gs_x, float gs_y)
{
    float screen_x       = gs_x - (2048.0f - PS2_CENTER_X);            // gs_x - 1728
    float screen_y_field = gs_y - (2048.0f - PS2_CENTER_Y * 0.5f);     // gs_y - 1936
    float screen_y_frame = screen_y_field * (PS2_RESOLUTION_Y_FLOAT / PS2_CENTER_Y); // ×2

    MikuPan_ConvertPs2ScreenCoordToNDCMaintainAspectRatio(
        out, window_width, window_height, screen_x, screen_y_frame);
}

void MikuPan_ConvertPs2GSSubPixelToNDC(float *out,
                                       float window_width, float window_height,
                                       int gs_sub_x, int gs_sub_y)
{
    // GS wire-format coordinates are sub-pixel fixed-point at 1/16 px (see
    // FLT_TO_FIX4 in sdk/sce/libvu0.c) — that's why values around the
    // framebuffer origin land near 32768 instead of 2048. Recover pixel space
    // and forward to the shared pixel-space converter so both paths use the
    // exact same letterboxing / NDC mapping.
    MikuPan_ConvertPs2GSCoordToNDC(out, window_width, window_height,
                                   (float)gs_sub_x / 16.0f,
                                   (float)gs_sub_y / 16.0f);
}

void MikuPan_ConvertScreenToNDCCoord(int *out, float ref_width,
                                     float ref_height, float target_width,
                                     float target_height)
{
    float target_aspect = target_width / target_height;
    float window_aspect = (float)ref_width / (float)ref_height;

    if (window_aspect > target_aspect)
    {
        out[3] = (int)ref_height;
        out[2] = (int)(ref_height * target_aspect);
        out[0] = (int) ((ref_width - (float)out[2]) / 2);
        out[1] = 0;
    }
    else
    {
        out[2] = (int)ref_width;
        out[3] = (int)(ref_width / target_aspect);
        out[0] = 0;
        out[1] = (int) ((ref_height - (float)out[3]) / 2);
    }
}