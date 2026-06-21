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

typedef struct
{
    int texture_valid;
    unsigned int texture_id;
    int texture_width;
    int texture_height;
    int texture_update_count;
    int negative_source_texture_valid;
    unsigned int negative_source_texture_id;
    int negative_source_texture_width;
    int negative_source_texture_height;
    int negative_source_texture_update_count;

    int queued;
    int queue_count;
    int queue_x;
    int queue_y;
    int queue_w;
    int queue_h;
    int queue_alpha;

    int last_draw_valid;
    int draw_count;
    int draw_x;
    int draw_y;
    int draw_w;
    int draw_h;
    int draw_alpha;

    int effect_overlay_active;
    int effect_overlay_count;
    int effect_overlay_drawn_in_game;
    int effect_overlay_in_game_count;
    int negative_overlay_active;
    int negative_overlay_count;
    int negative_overlay_drawn_in_game;
    int negative_overlay_in_game_count;
    int negative_x;
    int negative_y;
    int negative_w;
    int negative_h;
    float negative_strength;
    int force_opaque_preview_enabled;
    int force_negative_preview_enabled;
    float force_negative_preview_strength;
    int target_rect_enabled;
    int target_rect_drawn;
    int negative_debug_layer_enabled;
    int negative_debug_layer_drawn;
    int negative_debug_layer_count;
} MikuPan_PhotoDebugInfo;

typedef struct
{
    int enabled;
    int fbo_initialized;
    int fbo_complete;
    unsigned int fbo_status;
    int matrix_valid;
    unsigned int texture_id;
    int texture_size;

    int caster_passes;
    int receiver_passes;
    int caster_draws;
    int caster_indices;
    int receiver_draws;
    int receiver_indices;

    int caster_type_0;
    int caster_type_2;
    int caster_type_80;
    int caster_type_82;
    int caster_type_other;

    int caster_draw_type_0;
    int caster_draw_type_2;
    int caster_draw_type_80;
    int caster_draw_type_82;
    int caster_draw_type_other;

    int receiver_type_0;
    int receiver_type_10;
    int receiver_type_12;
    int receiver_type_32;
    int receiver_type_other;

    int probe_valid;
    int probe_nonzero_pixels;
    int probe_max_value;
    float probe_coverage;
    float probe_average;
} MikuPan_ShadowDebugInfo;

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
void MikuPan_Flush2DMessageQueue(void);
void MikuPan_BeginLate2DOverlayQueue(void);
void MikuPan_EndLate2DOverlayQueue(void);
void MikuPan_FlushLate2DOverlayQueue(void);
void MikuPan_RenderLine(float x1, float y1, float x2, float y2, u_char r, u_char g, u_char b, u_char a);
void MikuPan_RenderBoundingBox(sceVu0FVECTOR* vertices);
void MikuPan_RenderCameraDebug(void);
void MikuPan_RenderSprite(MikuPan_Rect src, MikuPan_Rect dst, u_char r, u_char g, u_char b, u_char a, MikuPan_TextureInfo* texture_info);
void MikuPan_UpdatePhotoPreviewTextureRGBA(int width, int height,
                                           const unsigned char *rgba);
void MikuPan_UpdatePhotoNegativeSourceTextureRGBA(int width, int height,
                                                  const unsigned char *rgba);
void MikuPan_QueuePhotoPreviewTexture(int x, int y, int w, int h,
                                      u_char alpha);
void MikuPan_SetPhotoPreviewOverlayActiveForFrame(int x, int y, int w, int h,
                                                  u_char alpha);
void MikuPan_SetPhotoNegativeOverlayActiveForFrame(int x, int y, int w, int h,
                                                   float strength);
void MikuPan_ClearPhotoPreviewOverlay(void);
void MikuPan_ClearPhotoNegativeOverlay(void);
void MikuPan_RenderPhotoPreviewTexture(int x, int y, int w, int h,
                                       u_char alpha);
int MikuPan_RenderPhotoPreviewOverlayForFrame(void);
int MikuPan_RenderPhotoNegativePreviewOverlayForFrame(void);
void MikuPan_RenderQueuedPhotoPreviewTexture(void);
const MikuPan_PhotoDebugInfo *MikuPan_GetPhotoDebugInfo(void);
void MikuPan_SetPhotoDebugForceOpaquePreviewEnabled(int enabled);
int MikuPan_IsPhotoDebugForceOpaquePreviewEnabled(void);
void MikuPan_SetPhotoDebugForceNegativePreviewEnabled(int enabled);
int MikuPan_IsPhotoDebugForceNegativePreviewEnabled(void);
void MikuPan_SetPhotoDebugForceNegativePreviewStrength(float strength);
float MikuPan_GetPhotoDebugForceNegativePreviewStrength(void);
void MikuPan_SetPhotoDebugTargetRectEnabled(int enabled);
int MikuPan_IsPhotoDebugTargetRectEnabled(void);
void MikuPan_SetPhotoDebugNegativeLayerEnabled(int enabled);
int MikuPan_IsPhotoDebugNegativeLayerEnabled(void);
void MikuPan_RenderSprite2D(sceGsTex0 *tex, float* buffer);
void MikuPan_RenderSprite2DDepth(sceGsTex0 *tex, float* buffer);
void MikuPan_RenderSprite2DDepthState(sceGsTex0 *tex, float *buffer,
                                      int depth_test, int depth_write,
                                      unsigned int depth_func);
void MikuPan_RenderUntexturedSprite(float* buffer);
void MikuPan_RenderUntexturedSpriteDepth(float* buffer);
void MikuPan_RenderUntexturedSpriteDepthState(float *buffer, int depth_test,
                                              int depth_write,
                                              unsigned int depth_func);
void MikuPan_RenderSprite3D(sceGsTex0 *tex, float* buffer);
void MikuPan_RenderSprite3DWithState(sceGsTex0* tex, float* buffer, int additive_blend);
void MikuPan_RenderTexturedTriangles3D(sceGsTex0 *tex, float *buffer, int vertex_count);
void MikuPan_RenderUntexturedTriangles3D(float *buffer, int vertex_count, int depth_mode, int additive_blend);
void MikuPan_RenderSolidUntexturedTriangles3D(float *buffer, int vertex_count, int depth_mode);
void MikuPan_RenderTexturedTriangles3DWithState(sceGsTex0 *tex, float *buffer, int vertex_count, int depth_mode, int additive_blend);
void MikuPan_RenderScreenCopyTriangles3D(sceGsTex0 *tex, float *buffer, int vertex_count, int depth_mode, int additive_blend);
void MikuPan_RenderScreenCopyTriangles3DScreenPos(sceGsTex0 *tex, float *buffer, int vertex_count, int depth_mode);
void MikuPan_RenderScreenCopyTriangles3DSTQ(sceGsTex0 *tex, float *buffer, int vertex_count, int depth_mode);
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
void MikuPan_EnableMirrorScissorFromGsBounds(int xmin, int ymin, int xmax, int ymax);
void MikuPan_EnableMirrorScissorFromNdcBounds(float minx, float miny, float maxx, float maxy);
void MikuPan_ClearMirrorScissorDepth(void);
void MikuPan_DisableMirrorScissor(void);
/// Brackets the reflected-geometry render in MirrorRender so the mesh
/// render-state setup knows to cull front faces (the reflection flips winding).
void MikuPan_SetMirrorReflectionPass(int active);
void MikuPan_Setup3D();
void MikuPan_Shutdown();
void MikuPan_EndFrame();
void MikuPan_SetModelTransformMatrix(sceVu0FVECTOR* m);
void MikuPan_RenderMeshType0x32(SGDPROCUNITHEADER *pVUVN, SGDPROCUNITHEADER *pPUHead);
void MikuPan_RenderMeshType0x82(unsigned int* pVUVN, unsigned int *pPUHead);
void MikuPan_RenderShadowSilhouettePrepared(unsigned int* pVUVN, unsigned int *pPUHead,
                                            const float *shadow_positions);
void MikuPan_RenderShadowSilhouette0x80(unsigned int* pVUVN, unsigned int *pPUHead);
void MikuPan_RenderMeshType0x2(SGDPROCUNITHEADER* pVUVN, SGDPROCUNITHEADER *pPUHead, float* vertices);
void MikuPan_FlushTexturedSpriteBatch(void);

u_long GSAlphaToOpenGL(int A, int B, int C, int D, int fix);

/// Resolve+downsample the currently-bound DRAW framebuffer into an RGBA8 buffer
/// at PS2 native size, with a top-left origin so the GS-memory side can stuff
/// the bytes straight into PSMCT32. Returns 1 on success, 0 if the source FBO
/// has no usable colour attachment.
int MikuPan_ReadFramebufferRGBA8TopLeft(int width, int height, unsigned char *out_rgba);
/// Same format as MikuPan_ReadFramebufferRGBA8TopLeft, but reads the latest
/// resolved scene framebuffer. Use this for GS freeze-frame copies requested
/// before the current GL frame has been rendered.
int MikuPan_ReadResolvedFramebufferRGBA8TopLeft(int width, int height,
                                                unsigned char *out_rgba);
int MikuPan_IsBlackWhiteModeActive(void);

/// ----- Shadow pass --------------------------------------------------------
/// PS2-style projector shadow. The original engine (graphics/graph3d/shadow.c)
/// renders each shadow caster's silhouette into a small GS texture from the
/// light's POV, then projects that texture onto receiver geometry as a
/// multiplicative decal. This is the GL equivalent: a 256×256 R8 alpha FBO,
/// an orthographic projection lifted from the PS2 shadow camera, and a
/// receiver-decal pass that reuses the original AssignShadow traversal.
///
/// Lifecycle each frame, called from shadow.c:DrawShadow:
///   1. `BeginShadowPass(world_clip_view)` — bind FBO, save matrices, push
///      the shadow VP onto the mesh shaders' viewProj/mvp uniforms so any
///      caster mesh you draw afterward lands in shadow space.
///   2. Render caster silhouettes through DrawShadowModel using the
///      regular mesh paths redirected to SHADOW_SILHOUETTE_SHADER.
///   3. `EndShadowPass()` — unbind FBO, restore the main camera matrices.
///   4. `BeginShadowReceiverPass()` / `EndShadowReceiverPass()` bracket the
///      AssignShadow receiver traversal and draw transparent black decals.
void MikuPan_BeginShadowPass(float *world_clip_view);
/// Build a GL-convention orthographic light view-projection (NDC [-1,1]) for the
/// shadow projector from world-space inputs. Returns a pointer to a static mat4.
float *MikuPan_ComputeShadowClipView(const float *center,
                                     const float *light_dir,
                                     const float *world_bbox8);
void MikuPan_EndShadowPass(void);
void MikuPan_BeginShadowReceiverPass(void);
void MikuPan_EndShadowReceiverPass(void);
/// Debug fallback silhouette renderer. The normal path draws the caster mesh
/// through DrawShadowModel; this remains useful for checking receiver sampling.
void MikuPan_DrawShadowSilhouetteEllipse(void);
unsigned int MikuPan_GetShadowTexture(void);
float       *MikuPan_GetShadowMatrix(void);
int          MikuPan_IsShadowEnabled(void);
int          MikuPan_IsShadowReceiverPassActive(void);
void         MikuPan_SetShadowEnabled(int enabled);
int          MikuPan_IsShadowInspectEnabled(void);
void         MikuPan_SetShadowInspectEnabled(int enabled);
void         MikuPan_GetShadowInspectAngles(float *yaw, float *pitch);
void         MikuPan_SetShadowInspectAngles(float yaw, float pitch);
int          MikuPan_IsShadowInspectWireframe(void);
void         MikuPan_SetShadowInspectWireframe(int enabled);
int          MikuPan_GetShadowResolution(void);
void         MikuPan_SetShadowResolution(int size);
int          MikuPan_IsShadowReceiverDebugViewEnabled(void);
void         MikuPan_SetShadowReceiverDebugViewEnabled(int enabled);
const MikuPan_ShadowDebugInfo *MikuPan_GetShadowDebugInfo(void);
void         MikuPan_ShadowDebugProbeTexture(void);
void         MikuPan_ShadowDebugRecordCasterMeshType(int mesh_type);
void         MikuPan_ShadowDebugRecordReceiverMeshType(int mesh_type);

#endif //MIKUPAN_SDL_RENDERER_H
