#ifndef MIKUPAN_SDL_RENDERER_H
#define MIKUPAN_SDL_RENDERER_H

#define WINDOW_WIDTH 640
#define WINDOW_HEIGHT 448
#include "SDL3/SDL_init.h"
#include "SDL3/SDL_render.h"
#include "SDL3/SDL_video.h"
#include "ee/eestruct.h"
#include "graphics/graph2d/sprt.h"
#include "graphics/graph2d/tim2.h"

/* We will use this renderer to draw into this window every frame. */
extern SDL_Window *window;
extern SDL_Renderer *renderer;

SDL_AppResult InitSDL();
void SDL_Clear();
void SDL_Render2DTexture(DISP_SPRT* sprite, unsigned char* image);
void SDL_Render2DTexture2(DISP_SQAR* sprite, unsigned char* image);
void SDL_RenderSquare(float x1, float y1, float x2, float y2, float x3, float y3, float x4, float y4, u_char r, u_char g, u_char b, u_char a);

#endif //MIKUPAN_SDL_RENDERER_H