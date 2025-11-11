#ifndef MIKUPAN_SDL_RENDERER_H
#define MIKUPAN_SDL_RENDERER_H

extern int window_width;
extern int window_height;
#include "SDL3/SDL_init.h"
#include "SDL3/SDL_render.h"
#include "SDL3/SDL_video.h"
#include "ee/eestruct.h"
#include "graphics/graph2d/sprt.h"
#include "graphics/graph2d/tim2.h"

/* We will use this renderer to draw into this window every frame. */
extern SDL_Window *window;
extern SDL_Renderer *renderer;

SDL_AppResult MikuPan_Init();
void MikuPan_Clear();
void MikuPan_UpdateWindowSize(int width, int height);
void MikuPan_Render2DTexture(DISP_SPRT* sprite, unsigned char* image);
void MikuPan_Render2DTexture2(DISP_SQAR* sprite, unsigned char* image);
void MikuPan_RenderSquare(float x1, float y1, float x2, float y2, float x3, float y3, float x4, float y4, u_char r, u_char g, u_char b, u_char a);
void MikuPan_RenderLine(float x1, float y1, float x2, float y2, u_char r, u_char g, u_char b, u_char a);

#endif //MIKUPAN_SDL_RENDERER_H