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
int MikuPan_GetWindowSizeOptionCount(void);
int MikuPan_GetSelectedWindowSizeOption(void);
const char *MikuPan_GetWindowSizeOptionLabel(int index);
int MikuPan_SelectWindowSizeOption(int index);
int MikuPan_GetRenderResolutionWidth(void);
int MikuPan_GetRenderResolutionHeight(void);
int MikuPan_GetRenderResolutionOptionCount(void);
int MikuPan_GetSelectedRenderResolutionOption(void);
const char *MikuPan_GetRenderResolutionOptionLabel(int index);
int MikuPan_SelectRenderResolutionOption(int index);
int MikuPan_GetMSAA(void);
int MikuPan_GetMSAAOptionCount(void);
int MikuPan_GetSelectedMSAAOption(void);
int MikuPan_IsSuperSamplingEnabled(void);
const char *MikuPan_GetMSAAOptionLabel(int index);
int MikuPan_SelectMSAAOption(int index);
int MikuPan_ShowControllerRemapWindow(void);
void MikuPan_SetControllerRemapWindowVisible(int visible);
void MikuPan_ResetControllerBindingsFromUi(void);
float MikuPan_GetBrightness(void);
void MikuPan_SetBrightness(float value);
float MikuPan_GetGamma(void);
void MikuPan_SetGamma(float value);
float MikuPan_GetContrast(void);
void MikuPan_SetContrast(float value);
float MikuPan_GetShadowDepth(void);
void MikuPan_SetShadowDepth(float value);
int MikuPan_IsHdrEnabled(void);
void MikuPan_SetHdrEnabled(int enabled);
float MikuPan_GetHdrPaperWhite(void);
void MikuPan_SetHdrPaperWhite(float value);
float MikuPan_GetHdrPeakLuminance(void);
void MikuPan_SetHdrPeakLuminance(float value);
const MikuPan_ConfigCrt *MikuPan_GetCrtSettings(void);
void MikuPan_SetCrtSettings(const MikuPan_ConfigCrt *settings);
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
int MikuPan_IsSoftShadowsEnabled(void);
void MikuPan_SetSoftShadowsEnabled(int enabled);
float MikuPan_GetSoftShadowRadius(void);
void MikuPan_SetSoftShadowRadius(float value);
int MikuPan_GetLightingMode(void);
void MikuPan_SetLightingMode(int mode);
int MikuPan_GetDitherModeOptionCount(void);
int MikuPan_GetSelectedDitherModeOption(void);
const char *MikuPan_GetDitherModeOptionLabel(int index);
int MikuPan_SelectDitherModeOption(int index);
int MikuPan_IsSsaoEnabled(void);
void MikuPan_SetSsaoEnabled(int enabled);
float MikuPan_GetSsaoStrength(void);
void MikuPan_SetSsaoStrength(float value);
float MikuPan_GetSsaoRadius(void);
void MikuPan_SetSsaoRadius(float value);
float MikuPan_GetSsaoBias(void);
void MikuPan_SetSsaoBias(float value);
int MikuPan_IsVolumetricShaftsEnabled(void);
void MikuPan_SetVolumetricShaftsEnabled(int enabled);
float MikuPan_GetVolumetricShaftsStrength(void);
void MikuPan_SetVolumetricShaftsStrength(float value);
float MikuPan_GetVolumetricShaftsRadius(void);
void MikuPan_SetVolumetricShaftsRadius(float value);
float MikuPan_GetVolumetricShaftsDensity(void);
void MikuPan_SetVolumetricShaftsDensity(float value);
int MikuPan_IsBloomEnabled(void);
void MikuPan_SetBloomEnabled(int enabled);
float MikuPan_GetBloomStrength(void);
void MikuPan_SetBloomStrength(float value);
float MikuPan_GetBloomThreshold(void);
void MikuPan_SetBloomThreshold(float value);
float MikuPan_GetBloomRadius(void);
void MikuPan_SetBloomRadius(float value);
int MikuPan_IsColorGradeEnabled(void);
void MikuPan_SetColorGradeEnabled(int enabled);
float MikuPan_GetColorGradeStrength(void);
void MikuPan_SetColorGradeStrength(float value);
float MikuPan_GetColorGradeTemperature(void);
void MikuPan_SetColorGradeTemperature(float value);
float MikuPan_GetColorGradeSaturation(void);
void MikuPan_SetColorGradeSaturation(float value);
int MikuPan_IsAtmosphericFogEnabled(void);
void MikuPan_SetAtmosphericFogEnabled(int enabled);
float MikuPan_GetAtmosphericFogStrength(void);
void MikuPan_SetAtmosphericFogStrength(float value);
float MikuPan_GetAtmosphericFogDensity(void);
void MikuPan_SetAtmosphericFogDensity(float value);
float MikuPan_GetAtmosphericFogHeight(void);
void MikuPan_SetAtmosphericFogHeight(float value);
int MikuPan_IsMaterialHighlightsEnabled(void);
void MikuPan_SetMaterialHighlightsEnabled(int enabled);
float MikuPan_GetMaterialHighlightsStrength(void);
void MikuPan_SetMaterialHighlightsStrength(float value);
float MikuPan_GetMaterialHighlightsRoughness(void);
void MikuPan_SetMaterialHighlightsRoughness(float value);
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
