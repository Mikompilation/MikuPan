#include "mikupan_renderer.h"
#include "../mikupan_types.h"
#include "SDL3/SDL_hints.h"
#include "SDL3/SDL_init.h"
#include "cglm/cglm.h"
#include "graphics/graph2d/message.h"
#include "graphics/graph3d/sgsu.h"
#include "mikupan/gs/gs_server_c.h"
#include "mikupan/gs/mikupan_texture_manager_c.h"
#include "mikupan/mikupan_logging_c.h"
#include "mikupan/ui/mikupan_ui_c.h"
#include "mikupan_shader.h"
#include <stdlib.h>

#define GLAD_GL_IMPLEMENTATION
#include "graphics/graph3d/sglib.h"
#include "mikupan/mikupan_utils.h"
#include "mikupan_pipeline.h"

#include <glad/gl.h>

int window_width = 640;
int window_height = 448;
int vertex_index[1024 * 1024] = {0};
float temp_render_buffer[1024 * 1024] = {0};
int state_changes = 0;
int draw_calls = 0;

SDL_Window *window = NULL;
MikuPan_TextureInfo *fnt_texture[6] = {0};
MikuPan_TextureInfo *curr_fnt_texture = NULL;

/// SgCMVtx or camera->wcv
mat4 WorldClipView = {0};

/// camera->wv
mat4 WorldView = {0};
mat4 projection = {0};

/// camera->vc
mat4 ViewClip = {0};

/// SgCMtx or camera->wc
mat4 WorldClip = {0};

SDL_AppResult MikuPan_Init()
{
    MikuPan_SetupOpenGLContext();
    SDL_SetAppMetadata("MikuPan", "1.0", "MikuPan");

    info_log("Initializing SDL");

    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD | SDL_INIT_JOYSTICK
                  | SDL_INIT_HAPTIC))
    {
        info_log("Couldn't initialize SDL: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    SDL_SetHint(SDL_HINT_MAIN_CALLBACK_RATE, "60");

    MikuPan_SetupOpenGLContext();

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

    MikuPan_InitUi(window, gl_context);
    MikuPan_InitShaders();
    MikuPan_InitPipeline();
    MikuPan_Setup3D();

    return SDL_APP_CONTINUE;
}

void MikuPan_SetupOpenGLContext()
{
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_DEBUG_FLAG);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 1);
    SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, 4);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
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

void MikuPan_SetupAmbientLighting()
{
    for (int i = 0; i < MAX_SHADER_PROGRAMS; i++)
    {
        u_int curr = MikuPan_SetCurrentShaderProgram(i);
        float* f_light = MikuPan_GetLightColor();

        glad_glUniform3fv(
            glad_glGetUniformLocation(curr, "lightColor"),
            1,
            f_light
            /* MikuPan_GetLightColor() */
            /* TAmbient */);

        glad_glUniform1f(
            glad_glGetUniformLocation(curr, "ambientStrength"),
            f_light[3]);
    }
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

    //GLfloat maxAniso = 0.0f;
    //glad_glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &maxAniso);
    //glad_glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, maxAniso);

    glad_glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
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

    if (texture_info != NULL)
    {
        glad_glActiveTexture(GL_TEXTURE0);
        glad_glBindTexture(GL_TEXTURE_2D, texture_info->id);
    }
}

void MikuPan_Render2DMessage(DISP_SPRT *sprite)
{
    MikuPan_Rect dst_rect = {0};
    MikuPan_Rect src_rect = {0};

    src_rect.x = (float) sprite->u;
    src_rect.y = (float) sprite->v;

    src_rect.w = (float) sprite->w;
    src_rect.h = (float) sprite->h;

    dst_rect.x = (float) sprite->x;
    dst_rect.y = (float) sprite->y;

    dst_rect.w = (float) sprite->w;
    dst_rect.h = (float) sprite->h;

    MikuPan_RenderSprite(src_rect, dst_rect, sprite->r, sprite->g, sprite->b, sprite->alpha, curr_fnt_texture);
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

    float colours[4] = {
        MikuPan_ConvertScaleColor(r),
        MikuPan_ConvertScaleColor(g),
        MikuPan_ConvertScaleColor(b),
        MikuPan_ConvertScaleColor(a)
    };

    float vertices[2][8] =
    {
        { colours[0], colours[1], colours[2], colours[3], ndc_x1, ndc_y1, 0.0f, 1.0f},
        { colours[0], colours[1], colours[2], colours[3], ndc_x2, ndc_y2, 0.0f, 1.0f},
    };

    MikuPan_SetCurrentShaderProgram(UNTEXTURED_COLOURED_SPRITE_SHADER);
    MikuPan_PipelineInfo* pipeline = MikuPan_GetPipelineInfo(COLOUR4_POSITION4);

    glad_glBindVertexArray(pipeline->vao);
    glad_glBindBuffer(GL_ARRAY_BUFFER, pipeline->buffers[0].id);

    glad_glBufferSubData(
        GL_ARRAY_BUFFER, pipeline->buffers[0].attributes[0].offset,
        sizeof(vertices), vertices);

    MikuPan_SetRenderState2D();

    glad_glDrawArrays(GL_LINES, 0, 2);
}

void MikuPan_RenderBoundingBox(sceVu0FVECTOR *vertices)
{
    if (!MikuPan_IsBoundingBoxRendering())
    {
        return;
    }

    float bb_color[4] = {0.0f, 1.0f, 0.0f, 1.0f};

    MikuPan_SetCurrentShaderProgram(BOUNDING_BOX_SHADER);
    MikuPan_PipelineInfo* pipeline = MikuPan_GetPipelineInfo(POSITION4);
    MikuPan_SetRenderState3D();

    MikuPan_SetUniform4fvToCurrentShader(bb_color, "uColor");

    glad_glBindVertexArray(pipeline->vao);
    glad_glBindBuffer(GL_ARRAY_BUFFER, pipeline->buffers[0].id);
    glad_glBufferSubData(
        GL_ARRAY_BUFFER,
        pipeline->buffers[0].attributes[0].offset,
        pipeline->buffers[0].buffer_length,
        vertices);

    for (int i = 0; i < 6; i++)
    {
        glad_glDrawArrays(GL_LINE_LOOP, i * 4, 4);
    }
}

void MikuPan_RenderSprite(MikuPan_Rect src, MikuPan_Rect dst, u_char r,
                          u_char g, u_char b, u_char a,
                          MikuPan_TextureInfo *texture_info)
{
    if (texture_info == NULL)
    {
        info_log("Cannot render texture due texture_info being NULL");
        return;
    }

    float ndc[4] = {0};

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

    float color[4] = {
        MikuPan_ConvertScaleColor(r),
        MikuPan_ConvertScaleColor(g),
        MikuPan_ConvertScaleColor(b),
        MikuPan_ConvertScaleColor(a)
    };

    float vertices[4][12] =
    {
        {
            u0, v0, 0.0f, 0.0f,
            color[0], color[1], color[2], color[3],
            ndc[0], ndc[1], 0.0f, 1.0f
        },
        {
            u1, v0, 0.0f, 0.0f,
            color[0], color[1], color[2], color[3],
            ndc[2], ndc[1], 0.0f, 1.0f
        },
        {
            u0, v1, 0.0f, 0.0f,
            color[0], color[1], color[2], color[3],
            ndc[0], ndc[3], 0.0f, 1.0f
        },
        {
            u1, v1, 0.0f, 0.0f,
            color[0], color[1], color[2], color[3],
            ndc[2], ndc[3], 0.0f, 1.0f
        }
    };

    u_int current_program = MikuPan_SetCurrentShaderProgram(SPRITE_SHADER);
    MikuPan_PipelineInfo* pipeline = MikuPan_GetPipelineInfo(UV4_COLOUR4_POSITION4);

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

    glad_glDrawArrays(MikuPan_GetRenderMode(), 0, 4);
}

void MikuPan_RenderSprite2D(sceGsTex0 *tex, float *buffer)
{
    MikuPan_SetCurrentShaderProgram(SPRITE_SHADER);
    MikuPan_PipelineInfo* pipeline = MikuPan_GetPipelineInfo(UV4_COLOUR4_POSITION4);

    glad_glBindVertexArray(pipeline->vao);
    MikuPan_SetTexture(tex);

    MikuPan_SetRenderState2D();

    glad_glBindBuffer(GL_ARRAY_BUFFER, pipeline->buffers[0].id);
    glad_glBufferSubData(GL_ARRAY_BUFFER, 0, pipeline->buffers[0].buffer_length, buffer);

    glad_glDrawArrays(MikuPan_GetRenderMode(), 0, 4);
}

void MikuPan_RenderUntexturedSprite(float *buffer)
{
    MikuPan_SetCurrentShaderProgram(UNTEXTURED_COLOURED_SPRITE_SHADER);
    MikuPan_PipelineInfo* pipeline = MikuPan_GetPipelineInfo(COLOUR4_POSITION4);

    glad_glBindVertexArray(pipeline->vao);

    MikuPan_SetRenderState2D();

    glad_glBindBuffer(GL_ARRAY_BUFFER, pipeline->buffers[0].id);
    glad_glBufferSubData(GL_ARRAY_BUFFER, 0, pipeline->buffers[0].buffer_length, buffer);

    glad_glDrawArrays(MikuPan_GetRenderMode(), 0, 4);
}

void MikuPan_RenderSprite3D(sceGsTex0 *tex, float* buffer)
{
    MikuPan_SetCurrentShaderProgram(SPRITE_SHADER);
    MikuPan_PipelineInfo* pipeline = MikuPan_GetPipelineInfo(UV4_COLOUR4_POSITION4);

    glad_glBindVertexArray(pipeline->vao);
    MikuPan_SetTexture(tex);

    MikuPan_SetRenderStateSprite3D();

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
            fnt_texture[i] = MikuPan_CreateGLTexture((sceGsTex0 *) &fntdat[i].tex0);
        }
    }

    curr_fnt_texture = fnt_texture[0];
}

float* MikuPan_GetWorldClipView()
{
    return (float*)&WorldClipView;
}

void MikuPan_SetWorldClipView()
{
    MikuPan_SetModelTransformMatrix(WorldClipView);
}

void MikuPan_SetModelTransformMatrix(sceVu0FVECTOR *m)
{
    state_changes++;
    MikuPan_SetupAmbientLighting();
    MikuPan_SetUniformMatrix4fvToAllShaders((float*)m, "model");
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
    SDL_DestroyWindow(window);
}

void MikuPan_EndFrame()
{
    //info_log("Total state changes and draw calls this frame: %d, %d", state_changes, draw_calls);
    draw_calls = 0;
    state_changes = 0;

    MikuPan_DrawUi();
    MikuPan_RenderUi();
    SDL_GL_SwapWindow(window);
}

void MikuPan_SetupCamera(MikuPan_Camera *mikupan_camera)
{
    // View -> camera->wv
    vec3 center = {0};
    glm_vec3_add(mikupan_camera->p, mikupan_camera->zd, center);
    vec3 up = {0};
    mat4 roll = {0};
    vec3 axis = {0.0f, 0.0f, 1.0f};
    glm_mat4_identity(roll);
    glm_rotate(roll, -mikupan_camera->roll, axis);
    glm_mat4_mulv3(roll, mikupan_camera->yd, 1.0f, up);
    glm_lookat(mikupan_camera->p, center, up, WorldView);

    // Projection -> camera->vcv
    float aspect = (float) window_width / (float) window_height;
    glm_perspective(mikupan_camera->fov, aspect, 10.0f, mikupan_camera->farz, projection);


    glm_mat4_mul(projection, WorldView, WorldClipView);
    MikuPan_SetUniformMatrix4fvToAllShaders((float*)WorldView, "view");
    MikuPan_SetUniformMatrix4fvToAllShaders((float*)projection, "projection");

    float nearz = mikupan_camera->nearz;
    float farz  = mikupan_camera->farz;

    float gs_width  = 4096.0f;
    float gs_height = 4096.0f;

    float scaleX = window_width  / gs_width;
    float scaleY = window_height / gs_height;

    mat4 vc;
    glm_mat4_identity(vc);

    vc[0][0] = (((nearz + nearz) * mikupan_camera->ax) / (gs_width)) * scaleX;
    vc[1][1] = (((nearz + nearz) * mikupan_camera->ay) / (gs_height)) * scaleY;

    vc[2][2] = (farz + nearz) / (farz - nearz);
    vc[2][3] = 1.0f;

    vc[3][2] = ((farz * nearz) * -2.0f) / (farz - nearz);
    vc[3][3] = 0.0f;

    glm_mat4_mul(vc, WorldView, WorldClip);

    SgSetClipMtx(WorldClip);
    SgSetClipVMtx(WorldClipView);
}

void MikuPan_Setup3D()
{
    MikuPan_SetRenderState3D();
    glad_glCullFace(GL_BACK);
}

void MikuPan_SetupMirrorMtx(float* mtx)
{
    MikuPan_SetRenderState3DMirror();

    mat4 out = {0};
    mat4 m = {0};

    glm_mat4_make(mtx, m);
    glm_mat4_mul(WorldView, m, out);

    glm_mat4_mul(projection, out, WorldClipView);

    MikuPan_SetUniformMatrix4fvToAllShaders((float*)out, "view");
}

void MikuPan_RenderMeshType0x32(SGDPROCUNITHEADER *pVUVN, SGDPROCUNITHEADER *pPUHead)
{
    char mesh_type = GET_MESH_TYPE(pPUHead);

    if ((mesh_type != 0x12 && mesh_type != 0x10 && mesh_type != 0x32) ||
        ((mesh_type == 0x12 || mesh_type == 0x10) && !MikuPan_IsMesh0x12Rendering()) ||
        (mesh_type == 0x32 && !MikuPan_IsMesh0x32Rendering()))
    {
        return;
    }

    MikuPan_SetCurrentShaderProgram(MESH_0x12_SHADER);
    MikuPan_PipelineInfo* pipeline = MikuPan_GetPipelineInfo(POSITION3_NORMAL3_UV2);

    SGDPROCUNITDATA *pVUVNData = (SGDPROCUNITDATA *) &pVUVN[1];
    SGDPROCUNITDATA *pProcData = (SGDPROCUNITDATA *) &pPUHead[1];
    SGDVUMESHPOINTNUM *pMeshInfo = (SGDVUMESHPOINTNUM *) &pPUHead[4];
    SGDVUMESHSTDATA *sgdMeshData = (SGDVUMESHSTDATA *)((int64_t) pProcData + (pProcData->VUMeshData_Preset.sOffsetToST - 1) * 4);
    _SGDVUMESHCOLORDATA *pVMCD = (_SGDVUMESHCOLORDATA*) (&pPUHead->pNext + pProcData->VUMeshData_Preset.sOffsetToPrim);

    glad_glBindVertexArray(pipeline->vao);

    sceGsTex0 *mesh_tex_reg = (sceGsTex0 *) ((int64_t) pProcData + 0x28);
    MikuPan_SetTexture(mesh_tex_reg);
    MikuPan_SetRenderState3D();

    GLfloat *vertices = NULL;
    GLfloat *normals = NULL;

    if (mesh_type == 0x32)
    {
        vertices =
            (GLfloat *) (pVUVNData->VUVNData_Preset.aui
                         + (pVUVN->VUVNDesc.sNumNormal) * 3
                         + 10);
    }
    else if (mesh_type == 0x12 || mesh_type == 0x10)
    {
        vertices = ((float *) &(((int *) pVUVN)[14]));
    }

    size_t byte_size = pVUVN->VUVNDesc.sNumVertex * pipeline->buffers[0].attributes[0].stride;
    glad_glBindBuffer(GL_ARRAY_BUFFER, pipeline->buffers[1].id);

    int vertex_offset = 0;

    MikuPan_SetTriangleStripRestart();

    if (GET_NUM_MESH(pPUHead) * pVUVN->VUVNDesc.sNumVertex * 4 > (1024 * 1024))
    {
        info_log("OVERFLOW!");
    }

    SGDMESHVERTEXDATA_TYPE2* buf = (SGDMESHVERTEXDATA_TYPE2*)temp_render_buffer;

    for (int i = 0; i < GET_NUM_MESH(pPUHead); i++)
    {
        pVMCD = (_SGDVUMESHCOLORDATA *) MikuPan_GetNextUnpackAddr((u_int *) pVMCD);
        int vertex_count = pVMCD->VifUnpack.NUM;

        MikuPan_FixUV((float*)&sgdMeshData->astData, vertex_count);
        MikuPan_SetTriangleIndex(vertex_index, vertex_count, vertex_offset, i);

        glad_glBufferSubData(
            GL_ARRAY_BUFFER,
            vertex_offset * pipeline->buffers[1].attributes[0].stride,
            vertex_count * pipeline->buffers[1].attributes[0].stride,
            sgdMeshData->astData);

        if (mesh_type == 0x32)
        {
            normals = (GLfloat *) (pVUVNData->VUVNData_Preset.aui
                               + (i * 3)
                               + 10);

            for (int j = 0; j < vertex_count; j++)
            {
                buf[vertex_offset + j].vVertex[0] = vertices[(vertex_offset + j) * 3 + 0];
                buf[vertex_offset + j].vVertex[1] = vertices[(vertex_offset + j) * 3 + 1];
                buf[vertex_offset + j].vVertex[2] = vertices[(vertex_offset + j) * 3 + 2];

                buf[vertex_offset + j].vNormal[0] = normals[0];
                buf[vertex_offset + j].vNormal[1] = normals[1];
                buf[vertex_offset + j].vNormal[2] = normals[2];
            }
        }

        vertex_offset += vertex_count;
        sgdMeshData = (SGDVUMESHSTDATA *) &sgdMeshData->astData[vertex_count];
        pVMCD = (_SGDVUMESHCOLORDATA *) &pVMCD->avColor[vertex_count];
    }

    glad_glBindBuffer(GL_ARRAY_BUFFER, pipeline->buffers[0].id);

    if (mesh_type == 0x32)
    {
        glad_glBufferSubData(GL_ARRAY_BUFFER, 0, (GLsizeiptr)byte_size, buf);
    }
    else
    {
        glad_glBufferSubData(GL_ARRAY_BUFFER, 0, (GLsizeiptr)byte_size, vertices);
    }

    draw_calls++;
    glad_glDrawElements(MikuPan_GetRenderMode(), pVUVN->VUVNDesc.sNumVertex + GET_NUM_MESH(pPUHead), GL_UNSIGNED_INT, vertex_index);
}

void MikuPan_RenderMeshType0x82(unsigned int *pVUVN, unsigned int *pPUHead)
{
    if (!MikuPan_IsMesh0x82Rendering())
    {
        return;
    }

    SGDVUVNDATA_PRESET *pVUVNData = (SGDVUVNDATA_PRESET *) &(((SGDPROCUNITHEADER *) pVUVN)[1]);
    SGDVUMESHPOINTNUM *pMeshInfo = (SGDVUMESHPOINTNUM *) &(((SGDPROCUNITHEADER *) pPUHead)[4]);
    SGDPROCUNITDATA *pProcData = (SGDPROCUNITDATA *) &(((SGDPROCUNITHEADER *) pPUHead)[1]);
    SGDVUMESHSTREGSET* sgdVuMeshStRegSet = (SGDVUMESHSTREGSET *) &pMeshInfo[GET_NUM_MESH(pPUHead)];
    SGDVUMESHSTDATA* sgdMeshData = (SGDVUMESHSTDATA *) &sgdVuMeshStRegSet->auiVifCode[3];
    VUVN_PRIM *v = ((VUVN_PRIM *) &((int*)pVUVN)[2]);

    MikuPan_SetCurrentShaderProgram(MESH_0x12_SHADER);
    MikuPan_PipelineInfo* pipeline = MikuPan_GetPipelineInfo(POSITION3_NORMAL3_UV2);
    glad_glBindVertexArray(pipeline->vao);

    sceGsTex0 *mesh_tex_reg = (sceGsTex0 *) ((int64_t) pProcData + 0x18);
    MikuPan_SetTexture(mesh_tex_reg);

    int vertex_offset = 0;

    MikuPan_SetRenderState3D();

    glad_glBindBuffer(GL_ARRAY_BUFFER, pipeline->buffers[0].id);
    glad_glBufferSubData(GL_ARRAY_BUFFER,
        0,
        v->vnum * pipeline->buffers[0].attributes[0].stride,
        pVUVNData->avt2);

    glad_glBindBuffer(GL_ARRAY_BUFFER, pipeline->buffers[1].id);

    for (int i = 0; i < GET_NUM_MESH(pPUHead); i++)
    {
        int vertex_count = pMeshInfo[i].uiPointNum;

        if (vertex_count == 0)
        {
            continue;
        }

        MikuPan_FixUV((float*)sgdMeshData->astData, vertex_count);
        MikuPan_SetTriangleIndex(vertex_index, vertex_count, vertex_offset, i);

        glad_glBufferSubData(
            GL_ARRAY_BUFFER,
            vertex_offset * pipeline->buffers[1].attributes[0].stride,
            vertex_count * pipeline->buffers[1].attributes[0].stride,
            sgdMeshData->astData);

        sgdMeshData = (SGDVUMESHSTDATA*) &sgdMeshData->astData[vertex_count];
        vertex_offset += vertex_count;
    }

    draw_calls++;
    glad_glDrawElements(MikuPan_GetRenderMode(), v->nnum + GET_NUM_MESH(pPUHead), GL_UNSIGNED_INT, vertex_index);
}

void MikuPan_RenderMeshType0x2(SGDPROCUNITHEADER *pVUVN, SGDPROCUNITHEADER *pPUHead, float* vertices)
{
    if (!MikuPan_IsMesh0x2Rendering())
    {
        return;
    }

    if (GET_MESH_TYPE(pPUHead) == 0x2)
    {
        MikuPan_SetCurrentShaderProgram(MESH_0x2_SHADER);
    }
    else if (GET_MESH_TYPE(pPUHead) == 0xA)
    {
        MikuPan_SetCurrentShaderProgram(MESH_0xA_SHADER);
    }
    else
    {
        return;
    }

    MikuPan_PipelineInfo* pipeline = MikuPan_GetPipelineInfo(POSITION4_NORMAL4_UV2);
    VUVN_PRIM *v = ((VUVN_PRIM *) &((int*)pVUVN)[2]);

    SGDVUMESHPOINTNUM *pMeshInfo = (SGDVUMESHPOINTNUM *) &pPUHead[4];
    SGDVUMESHSTREGSET *sgdVuMeshStRegSet = (SGDVUMESHSTREGSET *) &pMeshInfo[GET_NUM_MESH(pPUHead)];
    SGDVUMESHSTDATA *sgdMeshData = (SGDVUMESHSTDATA *) &sgdVuMeshStRegSet->auiVifCode[3];
    SGDPROCUNITDATA *pProcData = (SGDPROCUNITDATA *) &pPUHead[1];

    glad_glBindVertexArray(pipeline->vao);

    sceGsTex0 *mesh_tex_reg = (sceGsTex0 *) ((int64_t) pProcData + 0x18);
    MikuPan_SetTexture(mesh_tex_reg);

    MikuPan_SetRenderState3D();

    glad_glBindBuffer(GL_ARRAY_BUFFER, pipeline->buffers[0].id);
    glad_glBufferSubData(
        GL_ARRAY_BUFFER,
        0,
        v->vnum * pipeline->buffers[0].attributes[0].stride,
        vertices);

    glad_glBindBuffer(GL_ARRAY_BUFFER, pipeline->buffers[1].id);

    int vertex_offset = 0;

    MikuPan_SetTriangleStripRestart();

    if (GET_NUM_MESH(pPUHead) * pVUVN->VUVNDesc.sNumVertex * 4 > (1024 * 1024))
    {
        info_log("OVERFLOW!");
    }

    for (int i = 0; i < GET_NUM_MESH(pPUHead); i++)
    {
        int vertex_count = pMeshInfo[i].uiPointNum;

        if (pPUHead->VUMeshDesc.MeshType.TEX == 1)
        {
            MikuPan_FixUV((float*)&sgdMeshData->astData, vertex_count);
        }

        MikuPan_SetTriangleIndex(vertex_index, vertex_count, vertex_offset, i);

        glad_glBufferSubData(
            GL_ARRAY_BUFFER,
            vertex_offset * pipeline->buffers[1].attributes[0].stride,
            vertex_count * pipeline->buffers[1].attributes[0].stride,
            sgdMeshData->astData);

        sgdMeshData = (SGDVUMESHSTDATA *) &sgdMeshData->astData[vertex_count];
        vertex_offset += vertex_count;
    }

    draw_calls++;
    glad_glDrawElements(MikuPan_GetRenderMode(), v->nnum + GET_NUM_MESH(pPUHead), GL_UNSIGNED_INT, vertex_index);
}