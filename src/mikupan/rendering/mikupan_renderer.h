#ifndef MIKUPAN_SDL_RENDERER_H
#define MIKUPAN_SDL_RENDERER_H

#include "SDL3/SDL_init.h"
#include "SDL3/SDL_video.h"
#include "SDL3/SDL_gpu.h"
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
    SDL_Window    *window;
    SDL_GPUDevice *device;
    int            width;
    int            height;
} MikuPan_RenderWindow;

SDL_AppResult MikuPan_Init(void);
void MikuPan_SetupGpuDevice(void);
void MikuPan_Clear(void);
void MikuPan_CreateInternalBuffer(int w, int h, int msaa);
void MikuPan_DestroyInternalBuffer(void);
void MikuPan_UpdateWindowSize(int width, int height);
int  MikuPan_GetWindowWidth(void);
int  MikuPan_GetWindowHeight(void);
bool MikuPan_GetRenderWireframe(void);
void MikuPan_RenderSetDebugValues(void);
void MikuPan_Render2DMessage(DISP_SPRT *sprite);
void MikuPan_RenderLine(float x1, float y1, float x2, float y2,
                        u_char r, u_char g, u_char b, u_char a);
void MikuPan_RenderBoundingBox(sceVu0FVECTOR *vertices);
void MikuPan_RenderSprite(MikuPan_Rect src, MikuPan_Rect dst,
                          u_char r, u_char g, u_char b, u_char a,
                          MikuPan_TextureInfo *texture_info);
void MikuPan_RenderSprite2D(sceGsTex0 *tex, float *buffer);
void MikuPan_RenderUntexturedSprite(float *buffer);
void MikuPan_RenderSprite3D(sceGsTex0 *tex, float *buffer);
void MikuPan_SetupFntTexture(void);
void MikuPan_SetWorldClipView(void);
float *MikuPan_GetWorldClipView(void);
float *MikuPan_GetWorldClip(void);
void MikuPan_SetupAmbientLighting(const LIGHT_PACK *lp);
void MikuPan_SetFontTexture(int fnt);
void MikuPan_DeleteTexture(MikuPan_TextureInfo *texture_info);
bool MikuPan_IsFontTexture(MikuPan_TextureInfo *texture_info);
MikuPan_TextureInfo *MikuPan_CreateGPUTexture(sceGsTex0 *tex0);
void MikuPan_SetTexture(sceGsTex0 *tex0);
void MikuPan_SetupCamera(MikuPan_Camera *camera);
void MikuPan_SetupMirrorMtx(float *wv);
void MikuPan_Setup3D(void);
void MikuPan_Shutdown(void);
void MikuPan_EndFrame(void);
void MikuPan_SetModelTransformMatrix(sceVu0FVECTOR *m);
void MikuPan_RenderMeshType0x32(SGDPROCUNITHEADER *pVUVN, SGDPROCUNITHEADER *pPUHead);
void MikuPan_RenderMeshType0x82(unsigned int *pVUVN, unsigned int *pPUHead);
void MikuPan_RenderMeshType0x2(SGDPROCUNITHEADER *pVUVN, SGDPROCUNITHEADER *pPUHead,
                               float *vertices);

#endif /* MIKUPAN_SDL_RENDERER_H */
