#include "sdl_renderer.h"

#include "SDL3/SDL_init.h"
#include "SDL3/SDL_log.h"
#include "gs/texture_manager_c.h"
#include "graphics/ui/imgui_window_c.h"

#define PS2_RESOLUTION_X_FLOAT 640.0f
#define PS2_RESOLUTION_X_INT 640
#define PS2_RESOLUTION_Y_FLOAT 448.0f
#define PS2_RESOLUTION_Y_INT 448

float game_aspect_ratio = 4.0f/3.0f;
int window_width = 640;
int window_height = 448;
SDL_Window *window = NULL;
SDL_Renderer *renderer = NULL;

SDL_AppResult MikuPan_Init()
{
    SDL_SetAppMetadata("MikuPan", "1.0", "mikupan");

    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD)) {
        SDL_Log("Couldn't initialize SDL: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    if (!SDL_CreateWindowAndRenderer("MikuPan", window_width, window_height, SDL_WINDOW_RESIZABLE, &window, &renderer)) {
        SDL_Log("Couldn't create window/renderer: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    SDL_SetRenderLogicalPresentation(renderer, window_width, window_height, SDL_LOGICAL_PRESENTATION_DISABLED);
    InitImGuiWindow(window, renderer);

    return SDL_APP_CONTINUE;
}

void MikuPan_Clear()
{
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, SDL_ALPHA_OPAQUE);
    SDL_RenderClear(renderer);
}

void MikuPan_UpdateWindowSize(int width, int height)
{
    window_width = width;
    window_height = height;
}

void MikuPan_Render2DTexture(DISP_SPRT* sprite, unsigned char* image)
{
    sceGsTex0 tex0 = *(sceGsTex0*)&sprite->tex0;
    int texture_width = 1<<tex0.TW;
    int texture_height = 1<<tex0.TH;

    if (!IsFirstUploadDone())
    {
        return;
    }

    SDL_Texture* texture = (SDL_Texture*)GetSDLTexture(&tex0);

    if (texture == NULL)
    {
        SDL_Surface *surface = SDL_CreateSurfaceFrom(
        texture_width, texture_height,
        SDL_PIXELFORMAT_ABGR8888, image,
        texture_width * 4);

        texture = SDL_CreateTextureFromSurface(renderer, surface);
        SDL_DestroySurface(surface);

        AddSDLTexture(&tex0, (void*)texture);
    }

    SDL_FRect dst_rect;
    SDL_FRect src_rect;

    src_rect.x = sprite->u;
    src_rect.y = sprite->v;

    src_rect.w = sprite->w;
    src_rect.h = sprite->h;

    dst_rect.x = (float)window_width * (sprite->x / PS2_RESOLUTION_X_FLOAT);
    dst_rect.y = (float)window_height * (sprite->y / PS2_RESOLUTION_Y_FLOAT);

    dst_rect.w = (float)window_width * (sprite->w / PS2_RESOLUTION_X_FLOAT);
    dst_rect.h = (float)window_height * (sprite->h / PS2_RESOLUTION_Y_FLOAT);

    SDL_SetTextureAlphaMod(texture, (char)(255.0f * (sprite->alpha / 128.0f)));
    SDL_SetTextureColorMod(texture, sprite->r, sprite->g, sprite->b);
    SDL_RenderTexture(renderer, texture, &src_rect, &dst_rect);
}

void MikuPan_Render2DTexture2(DISP_SQAR *sprite, unsigned char *image)
{
}

void MikuPan_RenderSquare(float x1, float y1, float x2, float y2, float x3,
    float y3, float x4, float y4, u_char r, u_char g, u_char b, u_char a)
{
    SDL_SetRenderDrawColor(renderer, r, g, b, 255.0f * a / 128.0f);
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

    SDL_FRect rect;

    rect.x = (float)window_width * (x1 / PS2_RESOLUTION_X_FLOAT);
    rect.y = (float)window_height * (y1 / PS2_RESOLUTION_Y_FLOAT);

    rect.w = (float)window_width * ((x4 - x1) / PS2_RESOLUTION_X_FLOAT);
    rect.h = (float)window_height * ((y4 - y1) / PS2_RESOLUTION_Y_FLOAT);
    SDL_RenderFillRect(renderer, &rect);
}

void MikuPan_RenderLine(float x1, float y1, float x2, float y2, u_char r, u_char g, u_char b, u_char a)
{
    SDL_SetRenderDrawColor(renderer, r, g, b, 255.0f * a / 128.0f);

    SDL_RenderLine(renderer, 300.0f+x1, 200.0f+y1, 300.0f+x2, 200.0f+y2);
}
