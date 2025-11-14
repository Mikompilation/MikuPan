#ifndef MIKUPAN_IMGUI_WINDOW_C_H
#define MIKUPAN_IMGUI_WINDOW_C_H

void InitImGuiWindow(SDL_Window *window, SDL_Renderer *renderer);
float GetFrameRate();
void ShutDownImGuiWindow();
void RenderImGuiWindow(SDL_Renderer *renderer);
void DrawImGuiWindow();
void NewFrameImGuiWindow();
void ProcessEventImGui(SDL_Event *event);

#endif //MIKUPAN_IMGUI_WINDOW_C_H