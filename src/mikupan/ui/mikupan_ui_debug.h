#ifndef MIKUPAN_MIKUPAN_UI_DEBUG_H
#define MIKUPAN_MIKUPAN_UI_DEBUG_H

#include "mikupan/mikupan_basictypes.h"

void CollectTexture(MikuPan_TextureInfo* tex, void* ud);
int CompareTextureById(const void* a, const void* b);
void MikuPan_UiDrawCallInspector(void);
const char* MikuPan_CameraDebugKindName(int kind);
void MikuPan_UiCameraDebugWindow(void);
void MikuPan_UiShaderReloadWindow(void);
void MikuPan_UiEffectDebugWindow(void);
void MikuPan_UiShadowDebugWindow(void);
void MikuPan_UiPhotoDebugWindow(void);
void MikuPan_UiShadowResolutionCombo(const char* label);
void MikuPan_ShowTextureList(void);
float MikuPan_GetFrameRate(void);
int MikuPan_IsWireframeRendering(void);
int MikuPan_IsNormalsRendering(void);
void MikuPan_UiDebugMenuRender(void);
void MikuPan_UiDebugWindowsRender(void);
int MikuPan_IsBoundingBoxRendering(void);
int MikuPan_IsEventHitboxRendering(void);
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

#endif// MIKUPAN_MIKUPAN_UI_DEBUG_H
