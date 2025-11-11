#include "sdl_renderer.h"

#include "SDL3/SDL_init.h"
#include "SDL3/SDL_log.h"
#include "gs/texture_manager_c.h"

#define PS2_RESOLUTION_X_FLOAT 640.0f
#define PS2_RESOLUTION_X_INT 640
#define PS2_RESOLUTION_Y_FLOAT 448.0f
#define PS2_RESOLUTION_Y_INT 448

SDL_Window *window = NULL;
SDL_Renderer *renderer = NULL;

SDL_AppResult InitSDL()
{
    SDL_SetAppMetadata("MikuPan", "1.0", "mikupan");

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        SDL_Log("Couldn't initialize SDL: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    if (!SDL_CreateWindowAndRenderer("MikuPan", WINDOW_WIDTH, WINDOW_HEIGHT, SDL_WINDOW_RESIZABLE, &window, &renderer)) {
        SDL_Log("Couldn't create window/renderer: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    SDL_SetRenderLogicalPresentation(renderer, WINDOW_WIDTH, WINDOW_HEIGHT, SDL_LOGICAL_PRESENTATION_DISABLED);
    InitImGuiWindow(window, renderer);

    return SDL_APP_CONTINUE;
}

void SDL_Clear()
{
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, SDL_ALPHA_OPAQUE);
    SDL_RenderClear(renderer);
}

float ConvertPixelToNdcX(float x)
{
    float adjustmentX = PS2_RESOLUTION_X_FLOAT / 2;
    return (x - adjustmentX) / adjustmentX;
}

float ConvertPixelToNdcY(float y)
{
    return 1.0f - (((2.0f * y)) / PS2_RESOLUTION_Y_FLOAT);
}

void SDL_Render2DTexture(DISP_SPRT* sprite, unsigned char* image)
{
    sceGsTex0 tex0 = *(sceGsTex0*)&sprite->tex0;
    int texture_width = 1<<tex0.TW;
    int texture_height = 1<<tex0.TH;

    if (!IsFirstUploadDone())
    {
        return;
    }

    SDL_Texture* texture = (SDL_Texture*)GetSDLTexture(tex0.TBP0);

    if (texture == NULL)
    {
        SDL_Surface *surface = SDL_CreateSurfaceFrom(
        texture_width, texture_height,
        SDL_PIXELFORMAT_ABGR8888, image,
        texture_width * 4);

        texture = SDL_CreateTextureFromSurface(renderer, surface);
        SDL_DestroySurface(surface);

        AddSDLTexture(tex0.TBP0, (void*)texture);
    }

    SDL_FRect dst_rect;
    SDL_FRect src_rect;

    src_rect.x = sprite->u;
    src_rect.y = sprite->v;

    src_rect.w = sprite->w;
    src_rect.h = sprite->h;

    dst_rect.x = sprite->x;
    dst_rect.y = sprite->y;

    dst_rect.w = sprite->w;
    dst_rect.h = sprite->h;

    SDL_SetTextureAlphaMod(texture, (char)(255.0f * (sprite->alpha / 128.0f)));
    SDL_SetTextureColorMod(texture, sprite->r, sprite->g, sprite->b);
    SDL_RenderTexture(renderer, texture, &src_rect, &dst_rect);
}

void SDL_Render2DTexture2(DISP_SQAR *sprite, unsigned char *image)
{
}

void SDL_RenderSquare(float x1, float y1, float x2, float y2, float x3,
    float y3, float x4, float y4, u_char r, u_char g, u_char b, u_char a)
{
    SDL_SetRenderDrawColor(renderer, r, g, b, 255.0f * a / 128.0f);

    SDL_FRect rect;
    rect.x = 300 + x1;
    rect.y = 200 + y1;
    rect.w = x4 - x1;
    rect.h = y4 - y1;
    SDL_RenderFillRect(renderer, &rect);
}

void MIKUPAN_RenderLine(float x1, float y1, float x2, float y2, u_char r, u_char g, u_char b, u_char a)
{
    SDL_SetRenderDrawColor(renderer, r, g, b, 255.0f * a / 128.0f);

    SDL_RenderLine(renderer, 300.0f+x1, 200.0f+y1, 300.0f+x2, 200.0f+y2);
}
