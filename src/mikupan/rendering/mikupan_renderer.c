#include "mikupan_renderer.h"
#include "../mikupan_types.h"
#include "SDL3/SDL_hints.h"
#include "SDL3/SDL_init.h"
#include "cglm/cglm.h"
#include "graphics/graph2d/message.h"
#include "graphics/graph3d/sgcam.h"
#include "graphics/graph3d/sglight.h"
#include "graphics/graph3d/sgsu.h"
#include "mikupan/gs/gs_server_c.h"
#include "mikupan/gs/texture_manager_c.h"
#include "mikupan/mikupan_logging_c.h"
#include "mikupan/ui/mikupan_ui_c.h"
#include "mikupan_shader.h"
#include <stdlib.h>

#define GLAD_GL_IMPLEMENTATION
#include "main/glob.h"
#include "mikupan/mikupan_utils.h"
#include "mikupan_pipeline.h"
#include <glad/gl.h>

int window_width = 640;
int window_height = 448;

SDL_Window *window = NULL;
MikuPan_TextureInfo *fnt_texture[6] = {0};
MikuPan_TextureInfo *curr_fnt_texture = NULL;

mat4 WorldScreen = {0};
mat4 WorldView = {0};

SDL_AppResult MikuPan_Init()
{
#ifdef SDL_PLATFORM_LINUX
    setenv("SDL_VIDEO_WAYLAND_WMCLASS", "MikuPan", 0);
    setenv("SDL_VIDEO_X11_WMCLASS",     "MikuPan", 0);
#endif

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_DEBUG_FLAG);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,
                        SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 32);
    SDL_SetAppMetadata("MikuPan", "1.0", "MikuPan");

    info_log("Initializing SDL");

    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD | SDL_INIT_JOYSTICK
                  | SDL_INIT_HAPTIC))
    {
        info_log("Couldn't initialize SDL: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    SDL_SetHint(SDL_HINT_MAIN_CALLBACK_RATE, "60");

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2);

    info_log("Loading SDL_GameControllerDB");

    if (!SDL_AddGamepadMappingsFromFile("resources/gamecontrollerdb.txt"))
    {
        info_log(SDL_GetError());
        return SDL_APP_FAILURE;
    }

    info_log("Creating SDL Window");

    window = SDL_CreateWindow("MikuPan", window_width, window_height,
                              SDL_WINDOW_RESIZABLE | SDL_WINDOW_OPENGL);

    if (window == NULL)
    {
        info_log(SDL_GetError());
        return SDL_APP_FAILURE;
    }

    SDL_Surface* iconSurface = SDL_LoadPNG("resources/mikupan.png");
    if (!SDL_SetWindowIcon(window, iconSurface))
    {
        info_log(SDL_GetError());
    }

    SDL_DestroySurface(iconSurface);

    info_log("Creating OpenGL Context");

    SDL_GLContext gl_context = SDL_GL_CreateContext(window);

    if (gl_context == NULL)
    {
        info_log(SDL_GetError());
        return SDL_APP_FAILURE;
    }

    SDL_GL_MakeCurrent(window, gl_context);

    info_log("GLad version loaded %d", gladLoadGLLoader((void*)SDL_GL_GetProcAddress));

    glad_glDepthMask(GL_TRUE);

    MikuPan_InitUi(window, gl_context);
    MikuPan_InitShaders();
    MikuPan_InitPipeline();

    return SDL_APP_CONTINUE;
}

void MikuPan_Clear()
{
    MikuPan_RenderSetDebugValues();
    glad_glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glad_glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT
                 | GL_STENCIL_BUFFER_BIT);
}

void MikuPan_UpdateWindowSize(int width, int height)
{
    window_width = width;
    window_height = height;
}

int MikuPan_GetWindowWidth()
{
    return window_width;
}

int MikuPan_GetWindowHeight()
{
    return window_height;
}

int MikuPan_GetRenderMode()
{
    return MikuPan_IsWireframeRendering() ? GL_LINE_STRIP : GL_TRIANGLE_STRIP;
}

void MikuPan_RenderSetDebugValues()
{
    MikuPan_SetUniform1iToAllShaders(MikuPan_IsNormalsRendering(), "renderNormals");
}

MikuPan_TextureInfo *MikuPan_CreateGLTexture(sceGsTex0 *tex0)
{
    GLuint tex = 0;

    int width = 1 << tex0->TW;
    int height = 1 << tex0->TH;

    void *pixels = DownloadGsTexture(tex0);

    if (!pixels)
    {
        return NULL;
    }

    glad_glGenTextures(1, &tex);
    glad_glBindTexture(GL_TEXTURE_2D, tex);

    glad_glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

    glad_glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8,
                      width, height, 0, GL_RGBA,
                      GL_UNSIGNED_BYTE, pixels);

    GLfloat maxAniso = 0.0f;
    glad_glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &maxAniso);
    glad_glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, maxAniso);

    glad_glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glad_glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glad_glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glad_glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

    glad_glGenerateMipmap(GL_TEXTURE_2D);

    glad_glBindTexture(GL_TEXTURE_2D, 0);

    MikuPan_TextureInfo *texture_info = malloc(sizeof(MikuPan_TextureInfo));
    texture_info->height = height;
    texture_info->width = width;
    texture_info->id = tex;
    texture_info->data = pixels;

    MikuPan_AddTexture(tex0, texture_info);

    return texture_info;
}

void MikuPan_SetTexture(sceGsTex0 *tex0)
{
    MikuPan_TextureInfo *texture_info = MikuPan_GetTextureInfo(tex0);

    if (texture_info == NULL)
    {
        texture_info = MikuPan_CreateGLTexture(tex0);
    }

    glad_glActiveTexture(GL_TEXTURE0);

    if (texture_info != NULL)
    {
        glad_glBindTexture(GL_TEXTURE_2D, texture_info->id);
    }
}

void MikuPan_Render2DTexture(DISP_SPRT *sprite)
{
    if (!IsFirstUploadDone())
    {
        return;
    }

    sceGsTex0 *tex0 = (sceGsTex0 *) &sprite->tex0;

    MikuPan_TextureInfo *texture_info = MikuPan_GetTextureInfo(tex0);

    if (texture_info == NULL)
    {
        texture_info = MikuPan_CreateGLTexture(tex0);
    }

    MikuPan_Rect dst_rect = {0};
    MikuPan_Rect src_rect = {0};

    src_rect.x = (float) sprite->u;
    src_rect.y = (float) sprite->v;

    if (sprite->scw != 1.0f)
    {
        src_rect.w = (float) sprite->w * sprite->scw;
    }
    else
    {
        src_rect.w = (float) sprite->w;
    }

    if (sprite->sch != 1.0f)
    {
        src_rect.h = (float) sprite->h * sprite->sch;
    }
    else
    {
        src_rect.h = (float) sprite->h;
    }

    dst_rect.x = (float) sprite->x;
    dst_rect.y = (float) sprite->y;

    dst_rect.w = (float) sprite->w;
    dst_rect.h = (float) sprite->h;

    MikuPan_RenderSprite(src_rect, dst_rect, sprite->r, sprite->g, sprite->b,
                         sprite->alpha, sprite->rot, sprite->att & 0x2, sprite->att & 0x1, texture_info);
}

int MikuPan_GetTextureIndex(int fnt)
{
    if (fnt == 1)
    {
        return 0;
    }

    if (fnt == 0)
    {
        return 1;
    }

    return fnt;
}

void MikuPan_Render2DMessage(DISP_SPRT *sprite)
{
    MikuPan_Rect dst_rect;
    MikuPan_Rect src_rect;

    src_rect.x = (float) sprite->u;
    src_rect.y = (float) sprite->v;

    src_rect.w = (float) sprite->w;
    src_rect.h = (float) sprite->h;

    dst_rect.x = (float) sprite->x;
    dst_rect.y = (float) sprite->y;

    dst_rect.w = (float) sprite->w;
    dst_rect.h = (float) sprite->h;

    MikuPan_RenderSprite(src_rect, dst_rect, sprite->r, sprite->g, sprite->b,
                         sprite->alpha, sprite->rot, sprite->att & 0x2, sprite->att & 0x1, curr_fnt_texture);
}

void MikuPan_RenderSquare(float x1, float y1, float x2, float y2,
                          float x3, float y3, float x4, float y4,
                          u_char r, u_char g, u_char b, u_char a)
{
    float vx, vy, vw, vh, scale = 0.0f;
    MikuPan_GetPS2Viewport(window_width, window_height, &vx, &vy, &vw, &vh, &scale);

    x1 += PS2_CENTER_X; y1 += PS2_CENTER_Y;
    x2 += PS2_CENTER_X; y2 += PS2_CENTER_Y;
    x3 += PS2_CENTER_X; y3 += PS2_CENTER_Y;
    x4 += PS2_CENTER_X; y4 += PS2_CENTER_Y;

    float px[4] = { x1, x2, x3, x4 };
    float py[4] = { y1, y2, y3, y4 };

    float cx = (px[0] + px[1] + px[2] + px[3]) * 0.25f;
    float cy = (py[0] + py[1] + py[2] + py[3]) * 0.25f;

    float tlx, tly, trx, try_, brx, bry, blx, bly;

    for (int i = 0; i < 4; i++)
    {
        if (px[i] <= cx && py[i] <= cy)      { tlx = px[i]; tly = py[i]; }
        else if (px[i] > cx && py[i] <= cy) { trx = px[i]; try_ = py[i]; }
        else if (px[i] > cx && py[i] > cy)  { brx = px[i]; bry = py[i]; }
        else                                { blx = px[i]; bly = py[i]; }
    }

    float wx_tl = vx + tlx * scale;
    float wy_tl = vy + tly * scale;

    float wx_tr = vx + trx * scale;
    float wy_tr = vy + try_ * scale;

    float wx_br = vx + brx * scale;
    float wy_br = vy + bry * scale;

    float wx_bl = vx + blx * scale;
    float wy_bl = vy + bly * scale;

    float ndc_tl_x = (wx_tl / window_width) * 2.0f - 1.0f;
    float ndc_tl_y = 1.0f - (wy_tl / window_height) * 2.0f;

    float ndc_tr_x = (wx_tr / window_width) * 2.0f - 1.0f;
    float ndc_tr_y = 1.0f - (wy_tr / window_height) * 2.0f;

    float ndc_br_x = (wx_br / window_width) * 2.0f - 1.0f;
    float ndc_br_y = 1.0f - (wy_br / window_height) * 2.0f;

    float ndc_bl_x = (wx_bl / window_width) * 2.0f - 1.0f;
    float ndc_bl_y = 1.0f - (wy_bl / window_height) * 2.0f;

    float vtx[12] = {
        /* Triangle 1 */
        ndc_tl_x, ndc_tl_y,
        ndc_tr_x, ndc_tr_y,
        ndc_br_x, ndc_br_y,

        /* Triangle 2 */
        ndc_tl_x, ndc_tl_y,
        ndc_br_x, ndc_br_y,
        ndc_bl_x, ndc_bl_y
    };

    u_int current_program = MikuPan_SetCurrentShaderProgram(UNTEXTURED_SPRITE_SHADER);
    MikuPan_PipelineInfo* pipeline = MikuPan_GetPipelineInfo(POSITION2);

    glad_glBindVertexArray(pipeline->vao);
    glad_glBindBuffer(GL_ARRAY_BUFFER, pipeline->buffers[0].id);

    glad_glBufferSubData(
        GL_ARRAY_BUFFER, 0,
        sizeof(vtx), vtx);

    glad_glUniform4f(
        glad_glGetUniformLocation(current_program, "uColor"),
        MikuPan_ConvertScaleColor(r),
        MikuPan_ConvertScaleColor(g),
        MikuPan_ConvertScaleColor(b),
        MikuPan_ConvertScaleColor(a));

    MikuPan_SetRenderState2D();

    glad_glDrawArrays(GL_TRIANGLES, 0, 6);
}

void MikuPan_RenderLine(float x1, float y1, float x2, float y2, u_char r,
                        u_char g, u_char b, u_char a)
{
    float sx1 = (300.0f + x1) / PS2_RESOLUTION_X_FLOAT;
    float sy1 = (200.0f + y1) / PS2_RESOLUTION_Y_FLOAT;
    float sx2 = (300.0f + x2) / PS2_RESOLUTION_X_FLOAT;
    float sy2 = (200.0f + y2) / PS2_RESOLUTION_Y_FLOAT;

    float ndc_x1 = sx1 * 2.0f - 1.0f;
    float ndc_y1 = 1.0f - sy1 * 2.0f;
    float ndc_x2 = sx2 * 2.0f - 1.0f;
    float ndc_y2 = 1.0f - sy2 * 2.0f;

    float vertices[4] =
    {
        ndc_x1,
        ndc_y1,
        ndc_x2,
        ndc_y2
    };

    u_int current_program = MikuPan_SetCurrentShaderProgram(UNTEXTURED_SPRITE_SHADER);
    MikuPan_PipelineInfo* pipeline = MikuPan_GetPipelineInfo(POSITION2);

    glad_glBindVertexArray(pipeline->vao);
    glad_glBindBuffer(GL_ARRAY_BUFFER, pipeline->buffers[0].id);

    glad_glBufferSubData(
        GL_ARRAY_BUFFER, pipeline->buffers[0].attributes[0].offset,
        sizeof(vertices), vertices);

    glad_glUniform4f(
        glad_glGetUniformLocation(current_program, "uColor"),
        MikuPan_ConvertScaleColor(r),
        MikuPan_ConvertScaleColor(g),
        MikuPan_ConvertScaleColor(b),
        MikuPan_ConvertScaleColor(a)
        );

    MikuPan_SetRenderState2D();

    glad_glDrawArrays(GL_LINES, 0, 2);
}

void MikuPan_RenderBoundingBox(sceVu0FVECTOR *vertices)
{
    if (!MikuPan_IsBoundingBoxRendering())
    {
        return;
    }

    MikuPan_SetShaderProgramWithBackup(BOUNDING_BOX_SHADER);
    MikuPan_PipelineInfo* pipeline = MikuPan_GetPipelineInfo(POSITION4);

    glad_glBindVertexArray(pipeline->vao);
    glad_glBindBuffer(GL_ARRAY_BUFFER, pipeline->buffers[0].id);
    glad_glBufferSubData(
        GL_ARRAY_BUFFER,
        pipeline->buffers[0].attributes[0].offset,
        pipeline->buffers[0].buffer_length,
        vertices);

    MikuPan_SetRenderState3D();

    for (int i = 0; i < 6; i++)
    {
        glad_glDrawArrays(GL_LINE_LOOP, i * 4, 4);
    }
}

void MikuPan_RenderSprite(MikuPan_Rect src, MikuPan_Rect dst, u_char r,
                          u_char g, u_char b, u_char a, float rotation, bool flipX, bool flipY,
                          MikuPan_TextureInfo *texture_info)
{
    if (texture_info == NULL)
    {
        info_log("Cannot render texture due texture_info being NULL");
        return;
    }

    float ndc[4];

    MikuPan_ConvertPs2ScreenCoordToNDCMaintainAspectRatio(ndc, (float)window_width, (float)window_height, dst.x, dst.y);
    MikuPan_ConvertPs2ScreenCoordToNDCMaintainAspectRatio(&ndc[2], (float)window_width, (float)window_height, dst.x + src.w, dst.y + src.h);

    // Container size (PS2 texture memory size)
    float texW = (float) (texture_info->width);
    float texH = (float) (texture_info->height);

    // Half-texel offsets (PS2-style snapping)
    float halfU = 0.5f / texW;
    float halfV = 0.5f / texH;

    // Normalize sprite rectangle (pixel-accurate)
    float u0 = (src.x / texW) + halfU;
    float v0 = (src.y / texH) + halfV;
    float u1 = ((src.x + dst.w) / texW) - halfU;
    float v1 = ((src.y + dst.h) / texH) - halfV;

    if (flipX == 1)
    {
        float tmp = u0;
        u0 = u1;
        u1 = tmp;
    }

    if (flipY == 1)
    {
        float tmp = v0;
        v0 = v1;
        v1 = tmp;
    }

    float vertices[6][4] =
    {
        {ndc[0], ndc[3], u0, v1}, /// bottom-left
        {ndc[2], ndc[3], u1, v1}, /// bottom-right
        {ndc[2], ndc[1], u1, v0}, /// top-right
        {ndc[0], ndc[3], u0, v1}, /// bottom-left
        {ndc[2], ndc[1], u1, v0}, /// top-right
        {ndc[0], ndc[1], u0, v0}  /// top-left
    };

    u_int current_program = MikuPan_SetCurrentShaderProgram(UI_SPRITE_SHADER);
    MikuPan_PipelineInfo* pipeline = MikuPan_GetPipelineInfo(POSITION2_UV);

    glad_glBindVertexArray(pipeline->vao);
    glad_glBindBuffer(GL_ARRAY_BUFFER, pipeline->buffers[0].id);
    glad_glBufferSubData(
        GL_ARRAY_BUFFER,
        pipeline->buffers[0].attributes[0].offset,
        pipeline->buffers[0].buffer_length,
        vertices);

    glad_glActiveTexture(GL_TEXTURE0);
    glad_glBindTexture(GL_TEXTURE_2D, texture_info->id);
    MikuPan_SetRenderState2D();

    float color[4] =
    {
        MikuPan_ConvertScaleColor(r),
        MikuPan_ConvertScaleColor(g),
        MikuPan_ConvertScaleColor(b),
        MikuPan_ConvertScaleColor(a)
    };

    glad_glUniform4fv(
        glad_glGetUniformLocation(current_program, "uColor"),
        1,
        color);

    glad_glDrawArrays(GL_TRIANGLES, 0, 6);
}

void MikuPan_RenderSprite2D(sceGsTex0 *tex, float *buffer)
{
    MikuPan_SetCurrentShaderProgram(SPRITE_3D_SHADER);
    MikuPan_PipelineInfo* pipeline = MikuPan_GetPipelineInfo(POSITION3_UV_3D);

    glad_glBindVertexArray(pipeline->vao);
    MikuPan_SetTexture(tex);

    MikuPan_SetRenderState2D();

    glad_glBindBuffer(GL_ARRAY_BUFFER, pipeline->buffers[0].id);
    glad_glBufferSubData(GL_ARRAY_BUFFER, 0, pipeline->buffers[0].buffer_length, buffer);

    glad_glDrawArrays(MikuPan_GetRenderMode(), 0, 4);
}

void MikuPan_RenderSprite3D(sceGsTex0 *tex, float* buffer)
{
    MikuPan_SetCurrentShaderProgram(SPRITE_3D_SHADER);
    MikuPan_PipelineInfo* pipeline = MikuPan_GetPipelineInfo(POSITION3_UV_3D);

    glad_glBindVertexArray(pipeline->vao);
    MikuPan_SetTexture(tex);

    MikuPan_SetRenderState3D();

    glad_glBindBuffer(GL_ARRAY_BUFFER, pipeline->buffers[0].id);
    glad_glBufferSubData(GL_ARRAY_BUFFER, 0, pipeline->buffers[0].buffer_length, buffer);

    glad_glDrawArrays(MikuPan_GetRenderMode(), 0, 4);
}

void MikuPan_SetupFntTexture()
{
    for (int i = 0; i < 6; i++)
    {
        if (fnt_texture[i] == NULL)
        {
            fnt_texture[i] =
                MikuPan_CreateGLTexture((sceGsTex0 *) &fntdat[i].tex0);
        }
    }

    curr_fnt_texture = fnt_texture[0];
}

float* MikuPan_GetWorldScreenMatrix()
{
    return (float*)&WorldScreen;
}

void MikuPan_SetWeightedMesh(int value)
{
    MikuPan_SetUniform1iToAllShaders(value, "isWeighted");
}

void MikuPan_SetWorldScreen()
{
    MikuPan_SetModelTransformMatrix(WorldScreen);
}

void MikuPan_SetFontTexture(int fnt)
{
    curr_fnt_texture = fnt_texture[fnt];
}

void MikuPan_DeleteTexture(MikuPan_TextureInfo *texture_info)
{
    for (int i = 0; i < 6; i++)
    {
        if (fnt_texture[i]->id == texture_info->id)
        {
            return;
        }
    }

    glad_glDeleteTextures(1, (const GLuint *) &texture_info->id);
    free(texture_info);
}

void MikuPan_Shutdown()
{
    // destroy the window
    SDL_DestroyWindow(window);
}

void MikuPan_EndFrame()
{
    MikuPan_DrawUi();
    MikuPan_RenderUi();
    SDL_GL_SwapWindow(window);
}

void MikuPan_SetModelTransformMatrix(sceVu0FVECTOR *m)
{
    MikuPan_SetUniformMatrix4fvToAllShaders((float*)m, "model");
}

void MikuPan_SetModelTransform(unsigned int *prim)
{
    SgCOORDUNIT* cp = &lcp[prim[1]];
    MikuPan_SetModelTransformMatrix(cp->lwmtx);
}

void MikuPan_Camera(SgCAMERA *camera)
{
    // View -> camera->wv
    vec3 center = {0};
    glm_vec3_add(camera->p, camera->zd, center);
    vec3 up = {0};
    mat4 roll = {0};
    vec3 axis = {0.0f, 0.0f, 1.0f};
    glm_mat4_identity(roll);
    glm_rotate(roll, -camera->roll, axis);
    glm_mat4_mulv3(roll, camera->yd, 1.0f, up);
    glm_lookat(camera->p, center, up, WorldView);

    // Projection -> camera->vcv
    mat4 projection = {0};
    float aspect = (float) window_width / (float) window_height;
    glm_perspective(camera->fov, aspect, 125.0f, camera->farz,
                    projection);

    glm_mat4_mul(projection, WorldView, WorldScreen);

    MikuPan_SetUniformMatrix4fvToAllShaders((float*)WorldView, "view");
    MikuPan_SetUniformMatrix4fvToAllShaders((float*)projection, "projection");
}

void MikuPan_SetupMirrorMtx(float* mtx)
{
    mat4 out = {0};
    mat4 m = {0};

    glm_mat4_make(mtx, m);
    glm_mat4_mul(WorldView, m, out);

    MikuPan_SetUniformMatrix4fvToAllShaders((float*)out, "view");
}

void MikuPan_RenderMeshType0x32(struct SGDPROCUNITHEADER *pVUVN,
                                struct SGDPROCUNITHEADER *pPUHead)
{
    MikuPan_PipelineInfo* pipeline;
    if (GET_MESH_TYPE(pPUHead) == 0x32 && MikuPan_IsMesh0x32Rendering())
    {
        MikuPan_SetShaderProgramWithBackup(SIMPLE_TEXTURED_SHADER);
        pipeline = MikuPan_GetPipelineInfo(POSITION3_UV);
    }
    else if ((GET_MESH_TYPE(pPUHead) == 0x12  || GET_MESH_TYPE(pPUHead) == 0x10) && MikuPan_IsMesh0x12Rendering())
    {
        MikuPan_SetShaderProgramWithBackup(MESH_0x12_SHADER);
        pipeline = MikuPan_GetPipelineInfo(POSITION3_NORMAL3_UV);
    }
    else
    {
        return;
    }

    u_int current_program = MikuPan_GetCurrentShaderProgram();

    union SGDPROCUNITDATA *pVUVNData = (union SGDPROCUNITDATA *) &pVUVN[1];
    union SGDPROCUNITDATA *pProcData = (union SGDPROCUNITDATA *) &pPUHead[1];

    struct SGDVUMESHPOINTNUM *pMeshInfo =
        (struct SGDVUMESHPOINTNUM *) &pPUHead[4];

    struct SGDVUMESHSTDATA *sgdMeshData = (struct SGDVUMESHSTDATA *)
    ((int64_t) pProcData + (pProcData->VUMeshData_Preset.sOffsetToST - 1) * 4);

    struct _SGDVUMESHCOLORDATA *pVMCD =
        (struct _SGDVUMESHCOLORDATA
             *) (&pPUHead->pNext + pProcData->VUMeshData_Preset.sOffsetToPrim);

    int vertexOffset = 0;

    glad_glBindVertexArray(pipeline->vao);

    /// TYPE0 (0x10) Requires
    sceGsTex0 *mesh_tex_reg = (sceGsTex0 *) ((int64_t) pProcData + 0x28);
    MikuPan_SetTexture(mesh_tex_reg);

    MikuPan_SetRenderState3D();

    for (int i = 0; i < GET_NUM_MESH(pPUHead); i++)
    {
        pVMCD =
            (struct _SGDVUMESHCOLORDATA *) GetNextUnpackAddr((u_int *) pVMCD);

        GLfloat *vertices = NULL;
        GLfloat *normals = NULL;

        size_t vertexCount = pVMCD->VifUnpack.NUM;
        size_t byteSize = vertexCount * pipeline->buffers[0].attributes[0].stride;
        MikuPan_FixUV((float*)&sgdMeshData->astData, vertexCount);

        if (GET_MESH_TYPE(pPUHead) == 0x32)
        {
            vertices =
                (GLfloat *) (pVUVNData->VUVNData_Preset.aui
                             + (vertexOffset + pVUVN->VUVNDesc.sNumNormal) * 3
                             + 10);

            normals = (GLfloat *) (pVUVNData->VUVNData_Preset.aui
                                   + (i * 3)
                                   + 10);
        }
        else if (GET_MESH_TYPE(pPUHead) == 0x12 || GET_MESH_TYPE(pPUHead) == 0x10)
        {
            vertices = &((float *) &(((int *) pVUVN)[14]))[vertexOffset * 6];
        }

        glad_glBindBuffer(GL_ARRAY_BUFFER, pipeline->buffers[0].id);
        glad_glBufferSubData(GL_ARRAY_BUFFER, 0, byteSize, vertices);

        glad_glBindBuffer(GL_ARRAY_BUFFER, pipeline->buffers[1].id);
        glad_glBufferSubData(
            GL_ARRAY_BUFFER,
            pipeline->buffers[1].attributes[0].offset,
            vertexCount * pipeline->buffers[1].attributes[0].stride,
            sgdMeshData->astData);

        u_int loc = glad_glGetUniformLocation(current_program, "aNormal");

        if (GET_MESH_TYPE(pPUHead) == 0x32)
        {
            glad_glUniform3fv(loc, 1, (float *) normals);
        }

        /// LIGHT DIRECTION
        loc = glad_glGetUniformLocation(current_program, "uLighDirection");
        glad_glUniform3fv(loc, 1, (float *) plyr_wrk.mylight.parallel[0].direction);

        glad_glDrawArrays(MikuPan_GetRenderMode(), 0, vertexCount);

        sgdMeshData = (struct SGDVUMESHSTDATA *) &sgdMeshData
                          ->astData[pVMCD->VifUnpack.NUM];
        vertexOffset += pVMCD->VifUnpack.NUM;
        pVMCD = (struct _SGDVUMESHCOLORDATA *) &pVMCD
                    ->avColor[pVMCD->VifUnpack.NUM];
    }
}

void MikuPan_RenderMeshType0x82(unsigned int *pVUVN, unsigned int *pPUHead)
{
    if (!MikuPan_IsMesh0x82Rendering())
    {
        return;
    }

    struct SGDVUVNDATA_PRESET *pVUVNData = (struct SGDVUVNDATA_PRESET *) &(((struct SGDPROCUNITHEADER *) pVUVN)[1]);
    struct SGDVUMESHPOINTNUM *pMeshInfo = (struct SGDVUMESHPOINTNUM *) &(((struct SGDPROCUNITHEADER *) pPUHead)[4]);
    union SGDPROCUNITDATA *pProcData = (union SGDPROCUNITDATA *) &(((struct SGDPROCUNITHEADER *) pPUHead)[1]);
    struct SGDVUMESHSTREGSET* sgdVuMeshStRegSet = (struct SGDVUMESHSTREGSET *) &pMeshInfo[GET_NUM_MESH(pPUHead)];
    struct SGDVUMESHSTDATA* sgdMeshData = (struct SGDVUMESHSTDATA *) &sgdVuMeshStRegSet->auiVifCode[3];

    MikuPan_SetShaderProgramWithBackup(MESH_0x12_SHADER);
    MikuPan_PipelineInfo* pipeline = MikuPan_GetPipelineInfo(POSITION3_NORMAL3_UV);
    glad_glBindVertexArray(pipeline->vao);

    sceGsTex0 *mesh_tex_reg = (sceGsTex0 *) ((int64_t) pProcData + 0x18);
    MikuPan_SetTexture(mesh_tex_reg);

    int vertexOffset = 0;

    MikuPan_SetRenderState3D();

    for (int i = 0; i < GET_NUM_MESH(pPUHead); i++)
    {
        int vertexCount = pMeshInfo[i].uiPointNum;

        if (vertexCount == 0)
        {
            continue;
        }

        MikuPan_FixUV((float*)&sgdMeshData->astData, vertexCount);

        glad_glBindBuffer(GL_ARRAY_BUFFER, pipeline->buffers[0].id);
        glad_glBufferSubData(GL_ARRAY_BUFFER, 0, vertexCount * sizeof(struct SGDMESHVERTEXDATA_TYPE2), &pVUVNData->avt2[vertexOffset]);

        glad_glBindBuffer(GL_ARRAY_BUFFER, pipeline->buffers[1].id);
        glad_glBufferSubData(GL_ARRAY_BUFFER, 0, vertexCount * sizeof(float[2]),
                             sgdMeshData->astData);

        glad_glDrawArrays(MikuPan_GetRenderMode(), 0, vertexCount);


        sgdMeshData = (struct SGDVUMESHSTDATA *) &sgdMeshData->astData[vertexCount];
        vertexOffset += vertexCount;
    }
}

void MikuPan_RenderMeshType0x2(struct SGDPROCUNITHEADER *pVUVN,
                               struct SGDPROCUNITHEADER *pPUHead, float* vertices)
{
    if (!MikuPan_IsMesh0x2Rendering())
    {
        return;
    }

    MikuPan_SetShaderProgramWithBackup(MESH_0x2_SHADER);
    MikuPan_PipelineInfo* pipeline = MikuPan_GetPipelineInfo(POSITION4_NORMAL4_UV);
    VUVN_PRIM *v = ((VUVN_PRIM *) &((int*)pVUVN)[2]);

    struct SGDVUMESHPOINTNUM *pMeshInfo =
        (struct SGDVUMESHPOINTNUM *) &pPUHead[4];
    struct SGDVUMESHSTREGSET *sgdVuMeshStRegSet =
        (struct SGDVUMESHSTREGSET *) &pMeshInfo[GET_NUM_MESH(pPUHead)];
    struct SGDVUMESHSTDATA *sgdMeshData =
        (struct SGDVUMESHSTDATA *) &sgdVuMeshStRegSet->auiVifCode[3];
    union SGDPROCUNITDATA *pProcData = (union SGDPROCUNITDATA *) &pPUHead[1];

    glad_glBindVertexArray(pipeline->vao);

    sceGsTex0 *mesh_tex_reg = (sceGsTex0 *) ((int64_t) pProcData + 0x18);
    MikuPan_SetTexture(mesh_tex_reg);

    MikuPan_SetRenderState3D();

    for (int i = 0; i < GET_NUM_MESH(pPUHead); i++)
    {
        int vertexCount = pMeshInfo[i].uiPointNum;

        MikuPan_FixUV((float*)&sgdMeshData->astData, vertexCount);

        glad_glBindBuffer(GL_ARRAY_BUFFER, pipeline->buffers[0].id);
        glad_glBufferSubData(
            GL_ARRAY_BUFFER,
            0,
            vertexCount * pipeline->buffers[0].attributes[0].stride,
            vertices);

        glad_glBindBuffer(GL_ARRAY_BUFFER, pipeline->buffers[1].id);
        glad_glBufferSubData(
            GL_ARRAY_BUFFER,
            pipeline->buffers[1].attributes[0].offset,
            vertexCount * pipeline->buffers[1].attributes[0].stride,
            sgdMeshData->astData);

        vertices = &vertices[vertexCount * 4 * 2];

        sgdMeshData = (struct SGDVUMESHSTDATA *) &sgdMeshData
                          ->astData[vertexCount];

        glad_glDrawArrays(MikuPan_GetRenderMode(), 0, vertexCount);
    }
}