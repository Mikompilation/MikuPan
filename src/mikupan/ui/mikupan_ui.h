#ifndef MIKUPAN_IMGUI_WINDOW_H
#define MIKUPAN_IMGUI_WINDOW_H

#include "SDL3/SDL_video.h"
#include "SDL3/SDL_events.h"

extern bool show_fps;
extern bool show_menu_bar;

extern "C"
{
    void MikuPan_InitUi(SDL_Window *window, SDL_GLContext renderer);
    void MikuPan_RenderUi();
    void MikuPan_StartFrameUi();
    void MikuPan_DrawUi();
    void MikuPan_ShutDownUi();
    void MikuPan_ProcessEventUi(SDL_Event *event);
    float MikuPan_GetFrameRate();
    int MikuPan_IsWireframeRendering();
    int MikuPan_IsNormalsRendering();
}
#endif //MIKUPAN_IMGUI_WINDOW_H