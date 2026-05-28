// C++ wrapper that compiles imgui backends and exposes them with C linkage
// so they can be called from mikupan_ui.c (pure C).
#include "SDL3/SDL.h"
#include "imgui.h"
#include "imgui_impl_sdl3.h"
#include "imgui_impl_opengl3.h"

extern "C"
{

void MikuPan_ImGui_ImplInit(SDL_Window *window, void *gl_context)
{
    ImGui_ImplSDL3_InitForOpenGL(window, gl_context);
    ImGui_ImplOpenGL3_Init("#version 330");
}

void MikuPan_ImGui_ImplShutdown(void)
{
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL3_Shutdown();
}

void MikuPan_ImGui_ImplNewFrame(void)
{
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplSDL3_NewFrame();
}

void MikuPan_ImGui_ImplProcessEvent(SDL_Event *event)
{
    ImGui_ImplSDL3_ProcessEvent(event);
}

void MikuPan_ImGui_ImplRenderDrawData(void)
{
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

}
