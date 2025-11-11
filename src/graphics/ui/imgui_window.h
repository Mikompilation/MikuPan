#ifndef MIKUPAN_IMGUI_WINDOW_H
#define MIKUPAN_IMGUI_WINDOW_H
#include "SDL3/SDL_render.h"
#include "SDL3/SDL_video.h"

extern bool show_fps;
extern bool ingame_debug_menu;
extern bool show_menu_bar;

extern "C"
{
    void InitImGuiWindow(SDL_Window *window, SDL_Renderer *renderer);
    void RenderImGuiWindow(SDL_Renderer *renderer);
    void NewFrameImGuiWindow();
    void DrawImGuiWindow();
    void ShutDownImGuiWindow();
    void ProcessEventImGui(SDL_Event *event);
    float GetFrameRate();
}
#endif //MIKUPAN_IMGUI_WINDOW_H