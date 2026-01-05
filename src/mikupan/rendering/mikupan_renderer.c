#include "mikupan_renderer.h"
#include "../mikupan_types.h"
#include "SDL3/SDL_hints.h"
#include "SDL3/SDL_init.h"
#include "SDL3/SDL_log.h"
#include "common/utility.h"
#include "graphics/graph2d/message.h"
#include "graphics/ui/imgui_window_c.h"
#include "gs/gs_server_c.h"
#include "gs/texture_manager_c.h"
#include "mikupan/logging_c.h"

#include <SDL3/SDL_gpu.h>
#include <mikupan/mikupan_memory.h>
#include <stdlib.h>

#define PS2_RESOLUTION_X_FLOAT 640.0f
#define PS2_RESOLUTION_X_INT 640
#define PS2_RESOLUTION_Y_FLOAT 448.0f
#define PS2_RESOLUTION_Y_INT 448

float game_aspect_ratio = 4.0f/3.0f;
int window_width = 640;
int window_height = 448;
SDL_Window *window = NULL;
SDL_Renderer *renderer = NULL;
SDL_GPUDevice* device = NULL;
SDL_GPURenderPass* render_pass = NULL;
SDL_GPUCommandBuffer* command_buffer = NULL;
SDL_GPUTexture* swapchain_texture = NULL;

SDL_Texture* fnt_texture[6] = {0};
SDL_Texture *curr_fnt_texture = NULL;

SDL_AppResult MikuPan_Init()
{
    SDL_SetAppMetadata("MikuPan", "1.0", "mikupan");
    SDL_SetHint(SDL_HINT_MAIN_CALLBACK_RATE, "60");

    if (!SDL_Init(
        SDL_INIT_VIDEO |
        SDL_INIT_GAMEPAD |
        SDL_INIT_JOYSTICK |
        SDL_INIT_HAPTIC
        ))
    {
        info_log("Couldn't initialize SDL: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    window = SDL_CreateWindow("MikuPan", window_width, window_height, SDL_WINDOW_RESIZABLE);

    device = SDL_CreateGPUDevice(
        SDL_GPU_SHADERFORMAT_SPIRV | SDL_GPU_SHADERFORMAT_MSL,
        true,
        NULL
        );

    renderer = SDL_CreateGPURenderer(device, window);

    if (window == NULL || renderer == NULL || device == NULL)
    {
        info_log(SDL_GetError());
        return SDL_APP_FAILURE;
    }

    SDL_SetRenderLogicalPresentation(
        renderer,
        window_width,
        window_height,
        SDL_LOGICAL_PRESENTATION_DISABLED);

    InitImGuiWindow(window, renderer);

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderVSync(renderer, 1);

    return SDL_APP_CONTINUE;
}

void MikuPan_Clear()
{
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, SDL_ALPHA_OPAQUE);
    SDL_RenderClear(renderer);
    command_buffer = SDL_AcquireGPUCommandBuffer(device);

}

void MikuPan_UpdateWindowSize(int width, int height)
{
    window_width = width;
    window_height = height;
}

void MikuPan_Render2DTexture(DISP_SPRT* sprite)
{
    if (!IsFirstUploadDone())
    {
        return;
    }

    sceGsTex0* tex0 = (sceGsTex0*)&sprite->tex0;

    SDL_Texture* texture = (SDL_Texture*)GetSDLTexture(tex0);

    if (texture == NULL)
    {
        texture = MikuPan_CreateTexture(tex0);
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

    SDL_SetTextureAlphaMod(texture, AdjustAlpha(sprite->alpha));
    SDL_SetTextureColorMod(texture, AdjustAlpha(sprite->r), AdjustAlpha(sprite->g), AdjustAlpha(sprite->b));

    SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);
    //SDL_RenderTextureRotated(renderer, texture, &src_rect, &dst_rect, sprite->rot, NULL, false);
    SDL_RenderTexture(renderer, texture, &src_rect, &dst_rect);
}

int GetTextureIndex(int fnt)
{
    if (fnt == 1)
    {
        return 0;
    }

    if (fnt == 0)
    {
        return 1;
    }

    return fnt;
}

void MikuPan_Render2DMessage(DISP_SPRT *sprite)
{
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

    SDL_Texture* texture = curr_fnt_texture;

    SDL_SetTextureAlphaMod(texture, AdjustAlpha(sprite->alpha));
    SDL_SetTextureColorMod(texture, AdjustAlpha(sprite->r), AdjustAlpha(sprite->g), AdjustAlpha(sprite->b));
    SDL_RenderTexture(renderer, texture, &src_rect, &dst_rect);
}

void MikuPan_RenderSquare(float x1, float y1, float x2, float y2, float x3,
                          float y3, float x4, float y4, u_char r, u_char g, u_char b, u_char a)
{
    SDL_SetRenderDrawColor(renderer, AdjustAlpha(r), AdjustAlpha(g), AdjustAlpha(b), AdjustAlpha(a));

    SDL_FRect rect;

    rect.x = (float)window_width * (x1 / PS2_RESOLUTION_X_FLOAT);
    rect.y = (float)window_height * (y1 / PS2_RESOLUTION_Y_FLOAT);

    rect.w = (float)window_width * ((x4 - x1) / PS2_RESOLUTION_X_FLOAT);
    rect.h = (float)window_height * ((y4 - y1) / PS2_RESOLUTION_Y_FLOAT);
    SDL_RenderFillRect(renderer, &rect);
}

void MikuPan_RenderLine(float x1, float y1, float x2, float y2, u_char r, u_char g, u_char b, u_char a)
{
    SDL_SetRenderDrawColor(renderer, r, g, b, AdjustAlpha(a));

    float dst_x1 = (float)window_width *  (300.0f+x1) / PS2_RESOLUTION_X_FLOAT;
    float dst_y1 = (float)window_height * (200.0f+y1) / PS2_RESOLUTION_Y_FLOAT;
    float dst_x2 = (float)window_width *  (300.0f+x2) / PS2_RESOLUTION_X_FLOAT;
    float dst_y2 = (float)window_height * (200.0f+y2) / PS2_RESOLUTION_Y_FLOAT;

    SDL_RenderLine(renderer, dst_x1, dst_y1, dst_x2, dst_y2);
}

void MikuPan_SetupFntTexture()
{
    for (int i = 0; i < 6; i++)
    {
        if (fnt_texture[i] == NULL)
        {
            fnt_texture[i] = MikuPan_CreateTexture((sceGsTex0*)&fntdat[i].tex0);
        }
    }
}

SDL_Texture *MikuPan_CreateTexture(sceGsTex0 *tex0)
{
    int texture_width = 1 << tex0->TW;
    int texture_height = 1 << tex0->TH;

    unsigned char *image = DownloadGsTexture(tex0);

    SDL_Surface *surface =
        SDL_CreateSurfaceFrom(texture_width, texture_height,
                              SDL_PIXELFORMAT_RGBA32, image, texture_width * 4);

    SDL_Texture *texture = SDL_CreateTextureFromSurface(renderer, surface);
    SDL_DestroySurface(surface);

    AddSDLTexture(tex0, texture);

    return texture;
}

void MikuPan_SetFontTexture(int fnt)
{
    curr_fnt_texture = fnt_texture[fnt];
}

void MikuPan_DeleteTexture(void *texture)
{
    for (int i = 0; i < 6; i++)
    {
        if (fnt_texture[i] == texture)
        {
            return;
        }
    }

    SDL_DestroyTexture((SDL_Texture*)texture);
}

void MikuPan_Camera(const SgCAMERA *camera)
{
    struct GRA3DSCRATCHPADLAYOUT * scratchpad = (struct GRA3DSCRATCHPADLAYOUT *)ps2_virtual_scratchpad;
    return;
    //unsigned int viewLoc = gl_context->GetUniformLocation(shaderProgram, "view");
    //gl_context->UniformMatrix4fv(viewLoc, 1, GL_FALSE, glm::value_ptr(view));

    // Projection
    //unsigned int projectionLoc = context->GetUniformLocation(shaderProgram, "projection");
    //context->UniformMatrix4fv(projectionLoc, 1, GL_FALSE, glm::value_ptr(projection));
}

void MikuPan_Shutdown()
{
    // destroy the GPU device
    SDL_DestroyGPUDevice(device);

    // destroy the window
    SDL_DestroyWindow(window);
}

void MikuPan_EndFrame()
{
    DrawImGuiWindow();
    RenderImGuiWindow(renderer);
    //SDL_EndGPURenderPass(render_pass);
    SDL_SubmitGPUCommandBuffer(command_buffer);
    SDL_RenderPresent(renderer);
}