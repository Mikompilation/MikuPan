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

#endif // MIKUPAN_RENDERER_INTERNAL_H
