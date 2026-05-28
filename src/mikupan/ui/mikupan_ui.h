#ifndef MIKUPAN_IMGUI_WINDOW_H
#define MIKUPAN_IMGUI_WINDOW_H

#include "SDL3/SDL_video.h"
#include "SDL3/SDL_events.h"
#include "mikupan/mikupan_basictypes.h"

extern int show_fps;
extern int show_menu_bar;

void MikuPan_InitUi(SDL_Window *window, SDL_GLContext renderer);
void MikuPan_RenderUi(void);
void MikuPan_StartFrameUi(void);
void MikuPan_DrawUi(void);
void MikuPan_ShutDownUi(void);
void MikuPan_ProcessEventUi(SDL_Event *event);
float MikuPan_GetFrameRate(void);
int MikuPan_IsWireframeRendering(void);
int MikuPan_IsNormalsRendering(void);
void MikuPan_ShowTextureList(void);
void MikuPan_UiHandleShortcuts(void);
void MikuPan_UiMenuBar(void);
void MikuPan_UiShaderReloadWindow(void);
void MikuPan_UiDrawCallInspector(void);
int MikuPan_IsBoundingBoxRendering(void);
int MikuPan_ShowCameraDebug(void);
int MikuPan_IsMesh0x82Rendering(void);
int MikuPan_IsMesh0x32Rendering(void);
int MikuPan_IsMesh0x10Rendering(void);
int MikuPan_IsMesh0x12Rendering(void);
int MikuPan_IsMesh0x2Rendering(void);
int MikuPan_IsLightingDisabled(void);
int MikuPan_IsGsUploadsDisabled(void);
int MikuPan_ShowStaticLighting(void);
int MikuPan_GetMeshLightingMode(void);
float MikuPan_GetNormalLength(void);
int MikuPan_GetRenderResolutionWidth(void);
int MikuPan_GetRenderResolutionHeight(void);
int MikuPan_GetMSAA(void);
int MikuPan_ShowControllerRemapWindow(void);
float MikuPan_GetBrightness(void);
float MikuPan_GetGamma(void);
const MikuPan_ConfigCrt *MikuPan_GetCrtSettings(void);
int MikuPan_IsFullScreen(void);
int MikuPan_IsVsync(void);

#endif // MIKUPAN_IMGUI_WINDOW_H
