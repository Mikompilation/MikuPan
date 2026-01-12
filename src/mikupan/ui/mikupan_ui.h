#ifndef MIKUPAN_IMGUI_WINDOW_H
#define MIKUPAN_IMGUI_WINDOW_H
#include "SDL3/SDL_render.h"
#include "SDL3/SDL_video.h"

extern bool show_fps;
extern bool show_menu_bar;

extern "C"
{
    void MikuPan_InitUi(SDL_Window *window, SDL_Renderer *renderer);
    void MikuPan_RenderUi(SDL_Renderer *renderer);
    void MikuPan_StartFrameUi();
    void MikuPan_DrawUi();
    void MikuPan_ShutDownUi();
    void MikuPan_ProcessEventUi(SDL_Event *event);
    float MikuPan_GetFrameRate();
    int MikuPan_IsWireframeRendering();
}
#endif //MIKUPAN_IMGUI_WINDOW_H