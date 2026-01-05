#ifndef MIKUPAN_SDL_RENDERER_H
#define MIKUPAN_SDL_RENDERER_H
#include <ingame/camera/camera.h>

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
void MikuPan_Render2DTexture(DISP_SPRT* sprite);
void MikuPan_Render2DMessage(DISP_SPRT* sprite);
void MikuPan_RenderSquare(float x1, float y1, float x2, float y2, float x3, float y3, float x4, float y4, u_char r, u_char g, u_char b, u_char a);
void MikuPan_RenderLine(float x1, float y1, float x2, float y2, u_char r, u_char g, u_char b, u_char a);
void MikuPan_SetupFntTexture();
SDL_Texture* MikuPan_CreateTexture(sceGsTex0* tex0);
void MikuPan_SetFontTexture(int fnt);
void MikuPan_DeleteTexture(void* texture);
void MikuPan_Camera(const SgCAMERA *camera);
void MikuPan_Shutdown();
void MikuPan_EndFrame();
void MikuPan_RenderIMTF_2();
SDL_GPUShader* MikuPan_LoadShader(
    SDL_GPUDevice* device,
    const char* shaderFilename,
    u_int samplerCount,
    u_int uniformBufferCount,
    u_int storageBufferCount,
    u_int storageTextureCount
);

#endif //MIKUPAN_SDL_RENDERER_H