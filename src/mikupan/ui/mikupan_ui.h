#ifndef MIKUPAN_IMGUI_WINDOW_H
#define MIKUPAN_IMGUI_WINDOW_H

#include "SDL3/SDL_video.h"
#include "SDL3/SDL_events.h"
#include "mikupan/mikupan_basictypes.h"

#ifdef __cplusplus
extern "C" {
#endif

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
int MikuPan_GetRenderResolutionOptionCount(void);
int MikuPan_GetSelectedRenderResolutionOption(void);
const char *MikuPan_GetRenderResolutionOptionLabel(int index);
int MikuPan_SelectRenderResolutionOption(int index);
int MikuPan_GetMSAA(void);
int MikuPan_GetMSAAOptionCount(void);
int MikuPan_GetSelectedMSAAOption(void);
const char *MikuPan_GetMSAAOptionLabel(int index);
int MikuPan_SelectMSAAOption(int index);
int MikuPan_ShowControllerRemapWindow(void);
void MikuPan_SetControllerRemapWindowVisible(int visible);
void MikuPan_ResetControllerBindingsFromUi(void);
float MikuPan_GetBrightness(void);
void MikuPan_SetBrightness(float value);
float MikuPan_GetGamma(void);
void MikuPan_SetGamma(float value);
const MikuPan_ConfigCrt *MikuPan_GetCrtSettings(void);
void MikuPan_SetCrtEnabled(int enabled);
int MikuPan_GetCrtFieldCount(void);
const char *MikuPan_GetCrtFieldLabel(int index);
float MikuPan_GetCrtFieldValue(int index);
float MikuPan_GetCrtFieldMin(int index);
float MikuPan_GetCrtFieldMax(int index);
float MikuPan_GetCrtFieldStep(int index);
void MikuPan_SetCrtFieldValue(int index, float value);
void MikuPan_ResetCrtSettings(void);
float MikuPan_GetAudioMasterVolume(void);
void MikuPan_SetAudioMasterVolume(float value);
float MikuPan_GetAudioAmbientBgmVolume(void);
void MikuPan_SetAudioAmbientBgmVolume(float value);
float MikuPan_GetAudioBattleBgmVolume(void);
void MikuPan_SetAudioBattleBgmVolume(float value);
float MikuPan_GetAudioAmbientSeVolume(void);
void MikuPan_SetAudioAmbientSeVolume(float value);
float MikuPan_GetAudioBattleSeVolume(void);
void MikuPan_SetAudioBattleSeVolume(float value);
int MikuPan_IsFullScreen(void);
int MikuPan_GetWindowMode(void);
void MikuPan_SetWindowMode(int mode);
int MikuPan_IsVsync(void);
void MikuPan_SetVsync(int enabled);
int MikuPan_GetGpuDriverOptionCount(void);
int MikuPan_GetSelectedGpuDriverOption(void);
const char *MikuPan_GetGpuDriverOptionLabel(int index);
int MikuPan_IsGpuDriverOptionSupported(int index);
int MikuPan_SelectGpuDriverOption(int index);
const char *MikuPan_GetActiveGpuDriverLabel(void);
int MikuPan_IsGpuDriverRestartPending(void);
int MikuPan_GetShadowResolutionOptionCount(void);
int MikuPan_GetSelectedShadowResolutionOption(void);
const char *MikuPan_GetShadowResolutionOptionLabel(int index);
int MikuPan_SelectShadowResolutionOption(int index);
int MikuPan_GetLightingMode(void);
void MikuPan_SetLightingMode(int mode);
int MikuPan_GetThemeOptionCount(void);
int MikuPan_GetSelectedThemeOption(void);
const char *MikuPan_GetThemeOptionLabel(int index);
int MikuPan_SelectThemeOption(int index);
int MikuPan_GetFontOptionCount(void);
int MikuPan_GetSelectedFontOption(void);
const char *MikuPan_GetFontOptionLabel(int index);
int MikuPan_SelectFontOption(int index);
float MikuPan_GetUiFontScale(void);
void MikuPan_SetUiFontScale(float scale);
const char *MikuPan_GetDataFolder(void);
void MikuPan_SetDataFolder(const char *folder);
void MikuPan_BrowseDataFolderFromUi(void);
int MikuPan_SaveConfigurationFromUi(void);
const char *MikuPan_GetConfigSaveStatus(void);

#ifdef __cplusplus
}
#endif

#endif // MIKUPAN_IMGUI_WINDOW_H
