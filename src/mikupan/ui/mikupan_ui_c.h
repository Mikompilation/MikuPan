#ifndef MIKUPAN_IMGUI_WINDOW_C_H
#define MIKUPAN_IMGUI_WINDOW_C_H

void MikuPan_InitUi(SDL_Window *window, SDL_GLContext renderer);
void MikuPan_RenderUi();
void MikuPan_StartFrameUi();
void MikuPan_DrawUi();
void MikuPan_ShutDownUi();
void MikuPan_ProcessEventUi(SDL_Event *event);
float MikuPan_GetFrameRate();
int MikuPan_IsWireframeRendering();
int MikuPan_IsNormalsRendering();
int MikuPan_IsBoundingBoxRendering();
int MikuPan_IsMesh0x82Rendering();
int MikuPan_IsMesh0x32Rendering();
int MikuPan_IsMesh0x12Rendering();
int MikuPan_IsMesh0x2Rendering();
float* MikuPan_GetLightColor();
int MikuPan_GetRenderResolutionWidth();
int MikuPan_GetRenderResolutionHeight();
int MikuPan_GetMSAA();

#endif //MIKUPAN_IMGUI_WINDOW_C_H