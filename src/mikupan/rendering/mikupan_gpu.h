#ifndef MIKUPAN_GPU_H
#define MIKUPAN_GPU_H

#include "SDL3/SDL_gpu.h"
#include "SDL3/SDL_video.h"
#include "mikupan/mikupan_basictypes.h"
#include "mikupan/mikupan_types.h"
#include "mikupan_shader.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum MikuPan_GPUTarget
{
    MIKUPAN_GPU_TARGET_SCENE = 0,
    MIKUPAN_GPU_TARGET_WINDOW,
    MIKUPAN_GPU_TARGET_SHADOW,
    MIKUPAN_GPU_TARGET_SCREEN_COPY
} MikuPan_GPUTarget;

typedef enum MikuPan_GPUBufferKind
{
    MIKUPAN_GPU_BUFFER_VERTEX = 0,
    MIKUPAN_GPU_BUFFER_INDEX,
    MIKUPAN_GPU_BUFFER_UNIFORM_CPU
} MikuPan_GPUBufferKind;

typedef struct MikuPan_GPUUniformBlock
{
    float model[16];
    float view[16];
    float projection[16];
    float mvp[16];
    float modelView[16];
    float viewProj[16];
    float uShadowMatrix[16];
    float uWorldClipView[16];

    float normalMatrix[12];
    float viewNormalMatrix[12];

    float uColor[4];
    float uFog[4];
    float uFogColor[4];
    float uShadowSize[4];
    float uTextureSize[4];
    float uOutputSize[4];
    float uPhotoNegativeContentRect[4];
    float uPhotoNegativeRect[4];
    float uFramebufferUvOffset[4];
    float uFramebufferUvScale[4];
    float uFramebufferContentUvMax[4];
    float uRenderSize[4];

    float uParams0[4];
    float uCrt0[4];
    float uCrt1[4];
    float uCrt2[4];
    float uCrt3[4];
    float uParams1[4];

    int uFlags0[4];
    int uFlags1[4];
    int uFlags2[4];
    int uPadFlags[4];
} MikuPan_GPUUniformBlock;

int  MikuPan_GPUInit(SDL_Window *window, int vsync);
void MikuPan_GPUShutdown(void);
void MikuPan_GPUWaitIdle(void);
void MikuPan_GPUSetVsync(int vsync);

SDL_GPUDevice *MikuPan_GPUGetDevice(void);
SDL_GPUCommandBuffer *MikuPan_GPUGetCommandBuffer(void);
SDL_GPURenderPass *MikuPan_GPUGetRenderPass(void);
SDL_GPUTextureFormat MikuPan_GPUGetSwapchainFormat(void);

void MikuPan_GPUBeginFrame(void);
void MikuPan_GPUEndFrame(void);
void MikuPan_GPUFlushRenderPass(void);
void MikuPan_GPUBeginRenderPass(void);

void MikuPan_GPUCreateInternalBuffer(int width, int height, int msaa);
void MikuPan_GPUDestroyInternalBuffer(void);
unsigned int MikuPan_GPUGetSceneTextureId(void);
unsigned int MikuPan_GPUGetSceneWidth(void);
unsigned int MikuPan_GPUGetSceneHeight(void);

unsigned int MikuPan_GPUCreateBuffer(unsigned int size,
                                     MikuPan_GPUBufferKind kind);
unsigned int MikuPan_GPUCreateUniformCPUBuffer(unsigned int size);
void MikuPan_GPUReleaseBuffer(unsigned int id);
void MikuPan_GPUUploadBuffer(unsigned int id, unsigned int size,
                             const void *data);
void MikuPan_GPUUpdateUniformCPUBuffer(unsigned int id, unsigned int size,
                                       const void *data);

unsigned int MikuPan_GPUCreateTextureRGBA8(int width, int height,
                                           const void *pixels,
                                           int pitch,
                                           int repeat,
                                           int mipmaps);
unsigned int MikuPan_GPUCreateTextureR8Target(int width, int height);
unsigned int MikuPan_GPUCreateRenderTextureRGBA8(int width, int height);
void MikuPan_GPUReleaseTexture(unsigned int id);
void MikuPan_GPUUploadTextureRGBA8(unsigned int id, int width, int height,
                                   const void *pixels, int pitch);
int MikuPan_GPUReadTextureRGBA8(unsigned int texture_id, int width, int height,
                                unsigned char *out_rgba);
int MikuPan_GPUReadTextureR8(unsigned int texture_id, int size,
                             unsigned char *out_r8);
void MikuPan_GPUCopyTexture(unsigned int src_texture_id,
                            unsigned int dst_texture_id,
                            int width,
                            int height);

unsigned int MikuPan_GPURegisterVertexArray(int pipeline_type,
                                            unsigned int num_buffers,
                                            const unsigned int *buffers,
                                            unsigned int ibo);
void MikuPan_GPUUpdateVertexArrayBuffers(unsigned int vao,
                                         unsigned int num_buffers,
                                         const unsigned int *buffers,
                                         unsigned int ibo);
void MikuPan_GPUBindVertexArray(unsigned int vao);
unsigned int MikuPan_GPUGetBoundVertexArray(void);

void MikuPan_GPUSetCurrentShader(int shader_index);
int  MikuPan_GPUGetCurrentShader(void);
void MikuPan_GPUInvalidatePipelines(void);

void MikuPan_GPUSetRenderState3D(void);
void MikuPan_GPUSetRenderState3DMirror(void);
void MikuPan_GPUSetRenderState2D(void);
void MikuPan_GPUSetRenderStateSprite3D(void);
void MikuPan_GPUSetRenderStateShadow(void);
void MikuPan_GPUSetRenderStateShadowReceiver(void);
void MikuPan_GPUSetDepthWrite(int enabled);
void MikuPan_GPUSetDepthFunc(unsigned int gl_func);
void MikuPan_GPUSetBlend(int enabled, int additive);
void MikuPan_GPUSetCullBack(void);
void MikuPan_GPUSetCullFront(void);
void MikuPan_GPUSetCullNone(void);
void MikuPan_GPUSetColorWriteMask(int r, int g, int b, int a);
void MikuPan_GPUSetFillLine(int enabled);

void MikuPan_GPUSetViewport(int x, int y, int w, int h);
void MikuPan_GPUSetScissor(int x, int y, int w, int h);
void MikuPan_GPUDisableScissor(void);
void MikuPan_GPUClearDepth(void);

void MikuPan_GPUSetTarget(MikuPan_GPUTarget target, int clear);
void MikuPan_GPUSetShadowTarget(unsigned int texture_id, int size,
                                int clear);
void MikuPan_GPUSetScreenCopyTarget(unsigned int texture_id, int width,
                                    int height, int clear);

void MikuPan_GPUBindTextureSlot(int slot, unsigned int texture_id);
unsigned int MikuPan_GPUGetBoundTexture0(void);

void MikuPan_GPUDrawArrays(unsigned int gl_mode, int first, int count);
void MikuPan_GPUDrawElements(unsigned int gl_mode, int count,
                             unsigned int gl_index_type,
                             const void *indices);

MikuPan_GPUUniformBlock *MikuPan_GPUUniforms(void);
MikuPan_LightData *MikuPan_GPULightData(void);
MikuPan_MaterialData *MikuPan_GPUMaterialData(void);
void MikuPan_GPUSetMatrix4(const char *name, const float *mat);
void MikuPan_GPUSetMatrix3(const char *name, const float *mat);
void MikuPan_GPUSetVec4(const char *name, const float *vec);
void MikuPan_GPUSetInt(const char *name, int value);
void MikuPan_GPUSetFloat(const char *name, float value);
void MikuPan_GPUSetVec2(const char *name, float x, float y);

#ifdef __cplusplus
}
#endif

#endif // MIKUPAN_GPU_H
