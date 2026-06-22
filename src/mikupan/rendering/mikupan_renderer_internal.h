#ifndef MIKUPAN_RENDERER_INTERNAL_H
#define MIKUPAN_RENDERER_INTERNAL_H

#include "mikupan_renderer.h"
#include "cglm/cglm.h"
#include "SDL3/SDL_video.h"
#include <glad/gl.h>

typedef struct
{
    SDL_Window *window;
    int width;
    int height;
} MikuPan_RenderWindow;

extern MikuPan_RenderWindow mikupan_render;
extern MikuPan_MsaaBufferObject render_back_msaa;

extern mat4 WorldClipView;
extern mat4 WorldView;
extern mat4 projection;
extern mat4 ViewClip;
extern mat4 WorldClip;

void MikuPan_StreamUploadFull(GLenum target, GLuint buffer,
                              GLsizeiptr size, const void *data);

MikuPan_TextureInfo *MikuPan_GetCurrentFontTexture(void);
void MikuPan_TextureShutdown(void);

int MikuPan_IsShadowPassActive(void);
int MikuPan_IsShadowReceiverPassActive(void);
int MikuPan_IsMirrorReflectionPass(void);
void MikuPan_SetViewProjectionMatrices(float *view_matrix,
                                       float *projection_matrix);
void MikuPan_SetWorldSpaceModelTransform(void);
void MikuPan_InvalidateModelTransformCache(void);
void MikuPan_ShadowDebugBeginFrame(void);
void MikuPan_ShadowDebugRecordCasterMeshType(int mesh_type);
void MikuPan_ShadowDebugRecordReceiverMeshType(int mesh_type);
void MikuPan_ShadowDebugRecordCasterDraw(int mesh_type, int index_count);
void MikuPan_ShadowDebugRecordReceiverDraw(int mesh_type, int index_count);

#endif // MIKUPAN_RENDERER_INTERNAL_H
