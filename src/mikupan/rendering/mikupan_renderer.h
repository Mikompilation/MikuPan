#ifndef MIKUPAN_SDL_RENDERER_H
#define MIKUPAN_SDL_RENDERER_H
#include "mikupan/mikupan_basictypes.h"

#include <ingame/camera/camera.h>

struct SGDPROCUNITHEADER;
extern int window_width;
extern int window_height;
#include "SDL3/SDL_init.h"
#include "SDL3/SDL_video.h"
#include "ee/eestruct.h"
#include "graphics/graph2d/sprt.h"

extern SDL_Window *window;

SDL_AppResult MikuPan_Init();
void MikuPan_Clear();
void MikuPan_UpdateWindowSize(int width, int height);
int MikuPan_GetWindowWidth();
int MikuPan_GetWindowHeight();
void MikuPan_Render2DTexture(DISP_SPRT* sprite);
void MikuPan_Render2DMessage(DISP_SPRT* sprite);
void MikuPan_RenderSquare(float x1, float y1, float x2, float y2, float x3, float y3, float x4, float y4, u_char r, u_char g, u_char b, u_char a);
void MikuPan_RenderLine(float x1, float y1, float x2, float y2, u_char r, u_char g, u_char b, u_char a);
void MikuPan_RenderBoundingBox(sceVu0FVECTOR* vertices);
void MikuPan_RenderSprite(MikuPan_Rect src, MikuPan_Rect dst, u_char r, u_char g, u_char b, u_char a, float rotation, bool flipX, bool flipY, MikuPan_TextureInfo* texture_info);
void MikuPan_SetupFntTexture();
int MikuPan_GetTextureIndex(int fnt);
void MikuPan_SetFontTexture(int fnt);
void MikuPan_DeleteTexture(MikuPan_TextureInfo* texture_info);
MikuPan_TextureInfo* MikuPan_CreateGLTexture(const sceGsTex0 *tex0);
void MikuPan_Camera(SgCAMERA *camera);
void MikuPan_Shutdown();
void MikuPan_EndFrame();
void MikuPan_SetModelTransformMatrix(sceVu0FVECTOR* m);
void MikuPan_SetModelTransform(unsigned int *prim);
void MikuPan_RenderVertices(float* vertices, int num);
void MikuPan_RenderMeshType0x32(struct SGDPROCUNITHEADER *pVUVN, struct SGDPROCUNITHEADER *pPUHead);
void MikuPan_RenderMeshType0x82(unsigned int* pVUVN, unsigned int *pPUHead);
void MikuPan_RenderMeshType0x2(struct SGDPROCUNITHEADER* pVUVN, struct SGDPROCUNITHEADER *pPUHead);

#endif //MIKUPAN_SDL_RENDERER_H