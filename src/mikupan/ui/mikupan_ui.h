#ifndef MIKUPAN_IMGUI_WINDOW_H
#define MIKUPAN_IMGUI_WINDOW_H

#include "SDL3/SDL_video.h"
#include "SDL3/SDL_events.h"

#include <imgui.h>
#include <vector>

extern bool show_fps;
extern bool show_menu_bar;

class FrameTimeGraph
{
public:
    FrameTimeGraph(int max_samples = 300, float ms_scale = -1.0f);
    void update(float dt_sec);
    void draw(const char* label = "Frame Time", ImVec2 size = ImVec2(0,0));
    void setMaxSamples(int max_samples);
    void setManualScaleMs(float ms);
    void clear();

private:
    std::vector<float> times_;
    int max_samples_ = 300;
    float ms_scale_ = -1.0f; // negative means auto scale
    double sum_ms_ = 0.0; // reserved for future use
};

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
    void MikuPan_ShowTextureList();
    void MikuPan_UiHandleShortcuts();
    void MikuPan_UiMenuBar();
    int MikuPan_IsBoundingBoxRendering();
    int MikuPan_IsMesh0x82Rendering();
    int MikuPan_IsMesh0x32Rendering();
    int MikuPan_IsMesh0x12Rendering();
    int MikuPan_IsMesh0x2Rendering();
    float* MikuPan_GetLightColor();
    int MikuPan_GetRenderResolutionWidth();
    int MikuPan_GetRenderResolutionHeight();
    int MikuPan_GetMSAA();
}
#endif //MIKUPAN_IMGUI_WINDOW_H