#include "imgui_window.h"

#include "imgui_impl_sdl3.h"
#include "imgui_impl_sdlrenderer3.h"

extern "C"
{
#include "graphics/graph2d/g2d_debug.h"
#include "graphics/graph2d/message.h"
}

bool show_fps = true;
bool ingame_debug_menu = true;
bool show_menu_bar = false;

void InitImGuiWindow(SDL_Window *window, SDL_Renderer *renderer)
{
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;

    ImGui_ImplSDL3_InitForSDLRenderer(window, renderer);
    ImGui_ImplSDLRenderer3_Init(renderer);
}

void RenderImGuiWindow(SDL_Renderer* renderer)
{
    ImGui::Render();
    ImGui_ImplSDLRenderer3_RenderDrawData(ImGui::GetDrawData(), renderer);
}

void NewFrameImGuiWindow()
{
    ImGui_ImplSDLRenderer3_NewFrame();
    ImGui_ImplSDL3_NewFrame();
    ImGui::NewFrame();
}

void DrawImGuiWindow()
{
    if (ImGui::IsKeyPressed(ImGuiKey_F1))
    {
        show_menu_bar = !show_menu_bar;
    }

    if (ImGui::IsKeyPressed(ImGuiKey_F2))
    {
        ingame_debug_menu = !ingame_debug_menu;
    }

    if (show_menu_bar && ImGui::BeginMainMenuBar()) {
        if (ImGui::BeginMenu("Stats")) {
            ImGui::Checkbox("Show FPS", &show_fps);
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Debug")) {
            ImGui::Checkbox("Show Ingame Debug Menu", &ingame_debug_menu);

            if (ImGui::MenuItem("Create")) {
            }

            ImGui::EndMenu();
        }
        ImGui::EndMainMenuBar();
    }

    if (ingame_debug_menu)
    {
        gra2dDrawDbgMenu();
    }

    if (show_fps)
    {
        SetString2(0x10, 0.0f, 420.0f, 0, 0x80, 0x80, 0x80, (char*)"FPS %d", (int)GetFrameRate());

        // You can choose to make the window static or toggle it via a bool
        //ImGui::Begin("Performance");

        // ImGui provides a ready-made function for FPS:
        //ImGui::Text("FPS: %.1f", ImGui::GetIO().Framerate);

        // You can also show frame time in ms:
        //ImGui::Text("Frame time: %.3f ms/frame", 1000.0f / ImGui::GetIO().Framerate);

        //ImGui::End();
    }
}

void ShutDownImGuiWindow()
{
    ImGui_ImplSDLRenderer3_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();
}

void ProcessEventImGui(SDL_Event *event)
{
    ImGui_ImplSDL3_ProcessEvent(event);
}

float GetFrameRate()
{
    return ImGui::GetIO().Framerate;
}