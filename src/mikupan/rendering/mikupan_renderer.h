#ifndef MIKUPAN_SDL_RENDERER_H
#define MIKUPAN_SDL_RENDERER_H
#include "SDL3/SDL_init.h"
#include "ee/eestruct.h"
#include "graphics/graph2d/sprt.h"
#include "graphics/graph3d/light_types.h"
#include "mikupan/mikupan_basictypes.h"
#include "mikupan/mikupan_types.h"

typedef struct {
    sceVu0FVECTOR p;
    sceVu0FVECTOR i;
    float roll;
    float fov;
    float nearz;
    float farz;
    float ax;
    float ay;
    float cx;
    float cy;
    float zmin;
    float zmax;
    float pad[2];
    sceVu0FMATRIX vs;
    sceVu0FMATRIX vc;
    sceVu0FMATRIX vcv;
    sceVu0FMATRIX wv;
    sceVu0FMATRIX ws;
    sceVu0FMATRIX wc;
    sceVu0FMATRIX wcv;
    sceVu0FVECTOR zd;
    sceVu0FVECTOR yd;
} MikuPan_Camera;

typedef struct
{
    int vertex_count;
    int depth_mode;
    int additive_blend;
    float uv_min[2];
    float uv_max[2];
    float ndc_min[2];
    float ndc_max[2];
} MikuPan_ScreenCopyDebugInfo;

#define MIKUPAN_DEPTH_LEQUAL 0
#define MIKUPAN_DEPTH_ALWAYS 1
#define MIKUPAN_DEPTH_GEQUAL 2

SDL_AppResult MikuPan_Init();
void MikuPan_SetupOpenGLContext();
void MikuPan_Clear();
void MikuPan_CreateInternalBuffer(int w, int h, int msaa);
void MikuPan_DestroyInternalBuffer();
void MikuPan_UpdateWindowSize(int width, int height);
int MikuPan_GetWindowWidth();
int MikuPan_GetWindowHeight();
void MikuPan_GetFullScreenHalfExtent(float *half_w, float *half_h);
int MikuPan_GetRenderMode();
void MikuPan_RenderSetDebugValues();
void MikuPan_Render2DMessage(DISP_SPRT* sprite);
void MikuPan_RenderLine(float x1, float y1, float x2, float y2, u_char r, u_char g, u_char b, u_char a);
void MikuPan_RenderBoundingBox(sceVu0FVECTOR* vertices);
void MikuPan_RenderCameraDebug(void);
void MikuPan_RenderSprite(MikuPan_Rect src, MikuPan_Rect dst, u_char r, u_char g, u_char b, u_char a, MikuPan_TextureInfo* texture_info);
void MikuPan_RenderSprite2D(sceGsTex0 *tex, float* buffer);
void MikuPan_RenderUntexturedSprite(float* buffer);
void MikuPan_RenderSprite3D(sceGsTex0 *tex, float* buffer);
void MikuPan_RenderTexturedTriangles3D(sceGsTex0 *tex, float *buffer, int vertex_count);
void MikuPan_RenderUntexturedTriangles3D(float *buffer, int vertex_count, int depth_mode, int additive_blend);
void MikuPan_RenderTexturedTriangles3DWithState(sceGsTex0 *tex, float *buffer, int vertex_count, int depth_mode, int additive_blend);
void MikuPan_RenderScreenCopyTriangles3D(sceGsTex0 *tex, float *buffer, int vertex_count, int depth_mode, int additive_blend);
void MikuPan_RenderScreenCopyTriangles3DScreenPos(sceGsTex0 *tex, float *buffer, int vertex_count, int depth_mode);
const MikuPan_TextureInfo *MikuPan_GetScreenCopyTextureInfo(void);
const MikuPan_ScreenCopyDebugInfo *MikuPan_GetScreenCopyDebugInfo(void);
void MikuPan_SetupFntTexture();
void MikuPan_SetWorldClipView();
float* MikuPan_GetWorldClipView();
float* MikuPan_GetWorldClip();
void MikuPan_SetupAmbientLighting(const LIGHT_PACK* lp, float *eyevec);
void MikuPan_SetBakedLighting(int parallel_count,
                              const sceVu0FVECTOR parallel_ambient,
                              const float (*parallel_dcolor)[4],
                              const float (*parallel_scolor)[4],
                              int point_group_count,
                              const int point_lnum[3],
                              const float (*point_dcolor)[4],
                              const float (*point_scolor)[4],
                              const sceVu0FVECTOR point_btimes,
                              int spot_group_count,
                              const int spot_lnum[3],
                              const float (*spot_dcolor)[4],
                              const float (*spot_scolor)[4],
                              const sceVu0FVECTOR spot_btimes,
                              const sceVu0FVECTOR spot_intens,
                              const sceVu0FVECTOR spot_intens_b);
void MikuPan_SetMaterial(const sceVu0FVECTOR* ambient,
                         const sceVu0FVECTOR* diffuse,
                         const sceVu0FVECTOR* specular,
                         const sceVu0FVECTOR* emission);
void MikuPan_SetFontTexture(int fnt);
void MikuPan_DeleteTexture(MikuPan_TextureInfo* texture_info);
MikuPan_TextureInfo* MikuPan_CreateGLTexture(sceGsTex0 *tex0);
void MikuPan_SetTexture(sceGsTex0 *tex0);
void MikuPan_SetupCamera(MikuPan_Camera *camera);
void MikuPan_SetupMirrorMtx(float* wv);
void MikuPan_Setup3D();
void MikuPan_Shutdown();
void MikuPan_EndFrame();
void MikuPan_SetModelTransformMatrix(sceVu0FVECTOR* m);
void MikuPan_RenderMeshType0x32(SGDPROCUNITHEADER *pVUVN, SGDPROCUNITHEADER *pPUHead);
void MikuPan_RenderMeshType0x82(unsigned int* pVUVN, unsigned int *pPUHead);
void MikuPan_RenderMeshType0x2(SGDPROCUNITHEADER* pVUVN, SGDPROCUNITHEADER *pPUHead, float* vertices);
void MikuPan_FlushTexturedSpriteBatch(void);

u_long GSAlphaToOpenGL(int A, int B, int C, int D, int fix);

/// Resolve+downsample the currently-bound DRAW framebuffer (the MSAA scene
/// buffer when called mid-frame) into an RGBA8 buffer at PS2 native size,
/// with a top-left origin so the GS-memory side can stuff the bytes
/// straight into PSMCT32. Returns 1 on success, 0 if the source FBO has no
/// usable colour attachment. Used by mikupan_gs.cpp's
/// MikuPan_GsCaptureFramebuffer (the freeze-frame bridge); kept here
/// because glad and the FBO struct live in this TU.
int MikuPan_ReadFramebufferRGBA8TopLeft(int width, int height, unsigned char *out_rgba);

/// ----- Shadow pass --------------------------------------------------------
/// PS2-style projector shadow. The original engine (graphics/graph3d/shadow.c)
/// renders each shadow caster's silhouette into a small GS texture from the
/// light's POV, then projects that texture onto receiver geometry as a
/// multiplicative decal. This is the GL equivalent: a 256×256 R8 alpha FBO,
/// an orthographic projection lifted from the PS2 shadow camera, and a
/// world-space sampling path baked into the regular mesh fragment shader.
///
/// Lifecycle each frame, called from shadow.c:DrawShadow:
///   1. `BeginShadowPass(world_clip_view)` — bind FBO, save matrices, push
///      the shadow VP onto the mesh shaders' viewProj/mvp uniforms so any
///      caster mesh you draw afterward lands in shadow space.
///   2. Render caster silhouettes (currently a single fitted ellipse — the
///      DrawShadowModel iteration is still on the TODO list).
///   3. `EndShadowPass()` — unbind FBO, restore the main camera matrices.
///   4. Subsequent regular mesh draws sample the shadow texture using the
///      saved `uShadowMatrix` and darken receiver fragments.
void MikuPan_BeginShadowPass(float *world_clip_view);
void MikuPan_EndShadowPass(void);
/// Stub silhouette renderer — fits an ellipse into the shadow camera's
/// frustum (which SetShadowCamera in shadow.c already sized to the bbox).
/// Replace with the real DrawShadowModel-equivalent draw loop once the
/// caster's prim list is wired through MikuPan_RenderMeshType*.
void MikuPan_DrawShadowSilhouetteEllipse(void);
unsigned int MikuPan_GetShadowTexture(void);
float       *MikuPan_GetShadowMatrix(void);
int          MikuPan_IsShadowEnabled(void);
void         MikuPan_SetShadowEnabled(int enabled);

#endif //MIKUPAN_SDL_RENDERER_H
