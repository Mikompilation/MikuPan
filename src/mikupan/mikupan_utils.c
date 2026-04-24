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

    for (int i = 0; i < num; i++)
    {
        if (*((int *) &uvf[i].v) == 1)
        {
            uvf[i].v = uvf[i - 2].v;
        }
    }
}

// Builds a degenerate-triangle-stitched index run for triangle strips.
// SDL_GPU has no primitive restart, so multiple strips are joined with two
// degenerate (zero-area) indices: {last_of_prev, first_of_curr}.
//
// Index buffer layout for strip i starting at vertex_offset V with N vertices:
//   base = V + mesh_index * 2
//   if mesh_index > 0: buf[base-2] = V-1 (last of previous), buf[base-1] = V (first of current)
//   buf[base .. base+N-1] = V .. V+N-1
//
// Total draw count: sum_of_all_vertices + 2 * (num_meshes - 1)
void MikuPan_SetTriangleIndex(int *triangle_index, int vertex_count,
                              int vertex_offset, int mesh_index)
{
    int idx_base = vertex_offset + mesh_index * 2;

    if (mesh_index > 0)
    {
        // Stitch to previous strip with two degenerate vertices
        triangle_index[idx_base - 2] = vertex_offset - 1; // last vertex of previous strip
        triangle_index[idx_base - 1] = vertex_offset;     // first vertex of this strip
    }

    for (int j = 0; j < vertex_count; j++)
    {
        triangle_index[idx_base + j] = vertex_offset + j;
    }
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