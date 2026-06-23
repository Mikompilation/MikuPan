#ifndef MIKUPAN_IMGUI_WINDOW_H
#define MIKUPAN_IMGUI_WINDOW_H

#include "SDL3/SDL_video.h"
#include "SDL3/SDL_events.h"
#include "mikupan/mikupan_basictypes.h"

void MikuPan_InitUi(SDL_Window *window);
SDL_Window *MikuPan_GetUiWindow(void);
int MikuPan_UiIsMenuOpen(void);
void MikuPan_RenderUi(void);
void MikuPan_StartFrameUi(void);
void MikuPan_DrawUi(void);
void MikuPan_DrawMissingDataUi(const char *missing_file);
void MikuPan_ShutDownUi(void);
void MikuPan_ProcessEventUi(SDL_Event *event);
void MikuPan_RequestQuit(void);
void MikuPan_UiHandleShortcuts(void);
void MikuPan_UiMenuBar(void);
int MikuPan_GetRenderResolutionWidth(void);
int MikuPan_GetRenderResolutionHeight(void);
int MikuPan_GetMSAA(void);
int MikuPan_ShowControllerRemapWindow(void);
float MikuPan_GetBrightness(void);
float MikuPan_GetGamma(void);
const MikuPan_ConfigCrt *MikuPan_GetCrtSettings(void);
int MikuPan_IsFullScreen(void);
int MikuPan_GetWindowMode(void);
int MikuPan_IsVsync(void);

#endif // MIKUPAN_IMGUI_WINDOW_H
