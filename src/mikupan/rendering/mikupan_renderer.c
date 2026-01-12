#include "mikupan_renderer.h"
#include "../mikupan_types.h"
#include "SDL3/SDL_hints.h"
#include "SDL3/SDL_init.h"
#include "common/utility.h"
#include "graphics/graph2d/message.h"
#include "mikupan/ui/mikupan_ui_c.h"
#include "mikupan/gs/gs_server_c.h"
#include "mikupan/gs/texture_manager_c.h"
#include "mikupan/logging_c.h"
#include <mikupan/mikupan_memory.h>
#include <stdlib.h>
#include "cglm/cglm.h"

#define GLAD_GL_IMPLEMENTATION
#include "graphics/graph3d/sgcam.h"
#include "graphics/graph3d/sglib.h"
#include "graphics/graph3d/sglight.h"
#include "graphics/graph3d/sgsu.h"

#include <glad/gl.h>

#define PS2_RESOLUTION_X_FLOAT 640.0f
#define PS2_RESOLUTION_X_INT 640
#define PS2_RESOLUTION_Y_FLOAT 448.0f
#define PS2_RESOLUTION_Y_INT 448

// Vertex Shader source code
const char *vertexShaderSource =
    "#version 330 core\n"
    "layout (location = 0) in vec3 aPos;\n"
    "uniform mat4 model;\n"
    "uniform mat4 view;\n"
    "uniform mat4 projection;\n"
    "void main()\n"
    "{\n"
    "   gl_Position = projection * view * model * vec4(aPos, 1.0f);\n"
    "}\0";

//Fragment Shader source code
const char *fragmentShaderSource =
    "#version 330 core\n"
    "out vec4 FragColor;\n"
    "void main()\n"
    "{\n"
    "   FragColor = vec4(1.0f, 1.0f, 1.0f, 1.0f);\n"
    "}\n\0";

float game_aspect_ratio = 4.0f / 3.0f;
int window_width = 640;
int window_height = 448;

GLuint shaderProgram;

SDL_Window *window = NULL;
SDL_Renderer *renderer = NULL;
SDL_Texture *fnt_texture[6] = {0};
SDL_Texture *curr_fnt_texture = NULL;

SDL_AppResult MikuPan_Init()
{
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD | SDL_INIT_JOYSTICK
                  | SDL_INIT_HAPTIC))
    {
        info_log("Couldn't initialize SDL: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    SDL_SetAppMetadata("MikuPan", "1.0", "mikupan");
    SDL_SetHint(SDL_HINT_MAIN_CALLBACK_RATE, "60");

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,
                        SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_DEBUG_FLAG);

    int num_rend = SDL_GetNumRenderDrivers();

    for (int i = 0; i < num_rend; i++)
    {
        info_log("Available Renderer: %s", SDL_GetRenderDriver(i));
    }

    window = SDL_CreateWindow("MikuPan", window_width, window_height,
                              SDL_WINDOW_RESIZABLE | SDL_WINDOW_OPENGL);

    if (window == NULL)
    {
        info_log(SDL_GetError());
        return SDL_APP_FAILURE;
    }

    renderer = SDL_CreateRenderer(window, "opengl");

    if (renderer == NULL)
    {
        info_log(SDL_GetError());
        return SDL_APP_FAILURE;
    }

    int version = gladLoadGL(SDL_GL_GetProcAddress);

    SDL_SetRenderLogicalPresentation(renderer, window_width, window_height,
                                     SDL_LOGICAL_PRESENTATION_DISABLED);

    InitImGuiWindow(window, renderer);

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderVSync(renderer, 1);

    GLuint vertexShader = glad_glCreateShader(GL_VERTEX_SHADER);
    glad_glShaderSource(vertexShader, 1, &vertexShaderSource, NULL);

    // Compile the Vertex Shader into machine code
    glad_glCompileShader(vertexShader);

    // Create Fragment Shader Object and get its reference
    GLuint fragmentShader = glad_glCreateShader(GL_FRAGMENT_SHADER);

    // Attach Fragment Shader source to the Fragment Shader Object
    glad_glShaderSource(fragmentShader, 1, &fragmentShaderSource, NULL);

    // Compile the Vertex Shader into machine code
    glad_glCompileShader(fragmentShader);

    // Create Shader Program Object and get its reference
    shaderProgram = glad_glCreateProgram();

    // Attach the Vertex and Fragment Shaders to the Shader Program
    glad_glAttachShader(shaderProgram, vertexShader);
    glad_glAttachShader(shaderProgram, fragmentShader);

    // Wrap-up/Link all the shaders together into the Shader Program
    glad_glLinkProgram(shaderProgram);

    // Delete the now useless Vertex and Fragment Shader objects
    glad_glDeleteShader(vertexShader);
    glad_glDeleteShader(fragmentShader);

    // Tell OpenGL which Shader Program we want to use
    glad_glUseProgram(shaderProgram);

    return SDL_APP_CONTINUE;
}

void MikuPan_Clear()
{
    //SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0);
    //SDL_RenderClear(renderer);
    glad_glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glad_glClear(GL_COLOR_BUFFER_BIT);
}

void MikuPan_UpdateWindowSize(int width, int height)
{
    window_width = width;
    window_height = height;
}

void MikuPan_Render2DTexture(DISP_SPRT *sprite)
{
    if (!IsFirstUploadDone())
    {
        return;
    }

    sceGsTex0 *tex0 = (sceGsTex0 *) &sprite->tex0;

    SDL_Texture *texture = (SDL_Texture *) GetSDLTexture(tex0);

    if (texture == NULL)
    {
        texture = MikuPan_CreateTexture(tex0);
    }

    SDL_FRect dst_rect;
    SDL_FRect src_rect;

    src_rect.x = sprite->u;
    src_rect.y = sprite->v;

    src_rect.w = sprite->w;
    src_rect.h = sprite->h;

    dst_rect.x = (float) window_width * (sprite->x / PS2_RESOLUTION_X_FLOAT);
    dst_rect.y = (float) window_height * (sprite->y / PS2_RESOLUTION_Y_FLOAT);

    dst_rect.w = (float) window_width * (sprite->w / PS2_RESOLUTION_X_FLOAT);
    dst_rect.h = (float) window_height * (sprite->h / PS2_RESOLUTION_Y_FLOAT);

    SDL_SetTextureAlphaMod(texture, AdjustAlpha(sprite->alpha));
    SDL_SetTextureColorMod(texture, AdjustAlpha(sprite->r),
                           AdjustAlpha(sprite->g), AdjustAlpha(sprite->b));

    SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);
    //SDL_RenderTextureRotated(renderer, texture, &src_rect, &dst_rect, sprite->rot, NULL, false);
    SDL_RenderTexture(renderer, texture, &src_rect, &dst_rect);
}

int GetTextureIndex(int fnt)
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
    SDL_FRect dst_rect;
    SDL_FRect src_rect;

    src_rect.x = sprite->u;
    src_rect.y = sprite->v;

    src_rect.w = sprite->w;
    src_rect.h = sprite->h;

    dst_rect.x = (float) window_width * (sprite->x / PS2_RESOLUTION_X_FLOAT);
    dst_rect.y = (float) window_height * (sprite->y / PS2_RESOLUTION_Y_FLOAT);

    dst_rect.w = (float) window_width * (sprite->w / PS2_RESOLUTION_X_FLOAT);
    dst_rect.h = (float) window_height * (sprite->h / PS2_RESOLUTION_Y_FLOAT);

    SDL_Texture *texture = curr_fnt_texture;

    SDL_SetTextureAlphaMod(texture, AdjustAlpha(sprite->alpha));
    SDL_SetTextureColorMod(texture, AdjustAlpha(sprite->r),
                           AdjustAlpha(sprite->g), AdjustAlpha(sprite->b));
    SDL_RenderTexture(renderer, texture, &src_rect, &dst_rect);
}

void MikuPan_RenderSquare(float x1, float y1, float x2, float y2, float x3,
                          float y3, float x4, float y4, u_char r, u_char g,
                          u_char b, u_char a)
{
    SDL_SetRenderDrawColor(renderer, AdjustAlpha(r), AdjustAlpha(g),
                           AdjustAlpha(b), AdjustAlpha(a));

    SDL_FRect rect;

    rect.x = (float) window_width * (x1 / PS2_RESOLUTION_X_FLOAT);
    rect.y = (float) window_height * (y1 / PS2_RESOLUTION_Y_FLOAT);

    rect.w = (float) window_width * ((x4 - x1) / PS2_RESOLUTION_X_FLOAT);
    rect.h = (float) window_height * ((y4 - y1) / PS2_RESOLUTION_Y_FLOAT);
    SDL_RenderFillRect(renderer, &rect);
}

void MikuPan_RenderLine(float x1, float y1, float x2, float y2, u_char r,
                        u_char g, u_char b, u_char a)
{
    SDL_SetRenderDrawColor(renderer, r, g, b, AdjustAlpha(a));

    float dst_x1 =
        (float) window_width * (300.0f + x1) / PS2_RESOLUTION_X_FLOAT;
    float dst_y1 =
        (float) window_height * (200.0f + y1) / PS2_RESOLUTION_Y_FLOAT;
    float dst_x2 =
        (float) window_width * (300.0f + x2) / PS2_RESOLUTION_X_FLOAT;
    float dst_y2 =
        (float) window_height * (200.0f + y2) / PS2_RESOLUTION_Y_FLOAT;

    SDL_RenderLine(renderer, dst_x1, dst_y1, dst_x2, dst_y2);
}

void MikuPan_SetupFntTexture()
{
    for (int i = 0; i < 6; i++)
    {
        if (fnt_texture[i] == NULL)
        {
            fnt_texture[i] =
                MikuPan_CreateTexture((sceGsTex0 *) &fntdat[i].tex0);
        }
    }
}

SDL_Texture *MikuPan_CreateTexture(sceGsTex0 *tex0)
{
    int texture_width = 1 << tex0->TW;
    int texture_height = 1 << tex0->TH;

    unsigned char *image = DownloadGsTexture(tex0);

    SDL_Surface *surface =
        SDL_CreateSurfaceFrom(texture_width, texture_height,
                              SDL_PIXELFORMAT_RGBA32, image, texture_width * 4);

    SDL_Texture *texture = SDL_CreateTextureFromSurface(renderer, surface);
    SDL_DestroySurface(surface);

    AddSDLTexture(tex0, texture);

    return texture;
}

void MikuPan_SetFontTexture(int fnt)
{
    curr_fnt_texture = fnt_texture[fnt];
}

void MikuPan_DeleteTexture(void *texture)
{
    for (int i = 0; i < 6; i++)
    {
        if (fnt_texture[i] == texture)
        {
            return;
        }
    }

    SDL_DestroyTexture((SDL_Texture *) texture);
}

void MikuPan_Shutdown()
{
    SDL_DestroyRenderer(renderer);

    // destroy the window
    SDL_DestroyWindow(window);
}

void MikuPan_EndFrame()
{
    DrawImGuiWindow();
    RenderImGuiWindow(renderer);
    SDL_RenderPresent(renderer);
}

void MikuPan_SetModelTransform(unsigned int *prim)
{
    GLint id;
    glGetIntegerv(GL_CURRENT_PROGRAM, &id);

    glad_glUseProgram(shaderProgram);
    int modelLoc =
        glad_glGetUniformLocation(shaderProgram, "model");

    glad_glUniformMatrix4fv(
        modelLoc, 1, GL_FALSE,
        (float*)&lcp[prim[2]].lwmtx[0]);

    glad_glUseProgram(id);
}

void MikuPan_Camera(const SgCAMERA *camera)
{
    struct GRA3DSCRATCHPADLAYOUT *scratchpad =
        (struct GRA3DSCRATCHPADLAYOUT *) ps2_virtual_scratchpad;

    GLint id;
    glGetIntegerv(GL_CURRENT_PROGRAM,&id);
    glad_glUseProgram(shaderProgram);

    mat4 mtx = {0};

    vec3 cam = {camera->p[0], camera->p[1], camera->p[2]};

    // camera->zd
    vec3 zd = {camera->i[0] - cam[0], camera->i[1] - cam[1], camera->i[2] - cam[2]};
    vec3 center = {0};
    glm_vec3_add(cam, zd, center);

    // === Default up vector (PS2 uses Y-down) ===
    vec3 default_up = {0.0f, 1.0f, 0.0f};
    vec3 up = {0};

    // === Apply roll (rotation around Z) ===
    mat4 roll = {0};
    vec3 axis = {0.0f, 0.0f, 1.0f};
    glm_mat4_identity(roll);
    glm_rotate(roll, -camera->roll, axis);
    glm_mat4_mulv3(roll, default_up, 0.0f, up);

    // === View matrix (equivalent to sceVu0CameraMatrix) ===
    glm_lookat(cam,
        center,
        up,
        mtx);

    int viewLoc =
        glad_glGetUniformLocation(shaderProgram, "view");

    glad_glUniformMatrix4fv(
        viewLoc, 1, GL_FALSE,
        (float*)&mtx);

    // Projection
    mat4 projection = {0};
    float aspect = (float)window_width / (float)window_height;
    glm_perspective(camera->fov, aspect, camera->nearz, camera->farz, projection);

    int projectionLoc =
        glad_glGetUniformLocation(shaderProgram, "projection");

    glad_glUniformMatrix4fv(
        projectionLoc, 1, GL_FALSE,
        (float*)&projection);

    glad_glUseProgram(id);
}

void MikuPan_RenderMeshType0x32(struct SGDPROCUNITHEADER *pVUVN, struct SGDPROCUNITHEADER *pPUHead)
{
    //glad_glEnable(GL_DEPTH_TEST);

    GLint id;
    glad_glGetIntegerv(GL_CURRENT_PROGRAM, &id);

    glad_glUseProgram(shaderProgram);

    union SGDPROCUNITDATA *pVUVNData = (union SGDPROCUNITDATA *) &pVUVN[1];
    union SGDPROCUNITDATA * pProcData = (union SGDPROCUNITDATA *) &pPUHead[1];

    struct SGDVUMESHPOINTNUM *pMeshInfo =
        (struct SGDVUMESHPOINTNUM *) &pPUHead[4];

    struct SGDVUMESHSTDATA *sgdMeshData = (struct SGDVUMESHSTDATA *)((int64_t)pVUVNData + (pVUVNData->VUMeshData_Preset.sOffsetToST - 1) * 4);

    struct _SGDVUMESHCOLORDATA * pVMCD =
        (struct _SGDVUMESHCOLORDATA *) (&pPUHead->pNext
                                 + pProcData->VUMeshData_Preset.sOffsetToPrim);

    int vertexOffset = 0;

    for (int i = 0; i < GET_NUM_MESH(pPUHead); i++)
    {
        pVMCD = (struct _SGDVUMESHCOLORDATA *) GetNextUnpackAddr((u_int *) pVMCD);

        GLfloat *vertices =
            (GLfloat *) (pVUVNData->VUVNData_Preset.aui
                         + (vertexOffset + pVUVN->VUVNDesc.sNumNormal) * 3
                         + 10);

        GLuint VAO, VBO;

        glad_glGenVertexArrays(1, &VAO);
        glad_glGenBuffers(1, &VBO);

        // Make the VAO the current Vertex Array Object by binding it
        glad_glBindVertexArray(VAO);

        // Bind the VBO specifying it's a GL_ARRAY_BUFFER
        glad_glBindBuffer(GL_ARRAY_BUFFER, VBO);

        // Introduce the vertices into the VBO
        glad_glBufferData(GL_ARRAY_BUFFER,
                                 pVMCD->VifUnpack.NUM * sizeof(float[3]),
                                 vertices, GL_STATIC_DRAW);

        // Configure the Vertex Attribute so that OpenGL knows how to read the VBO
        glad_glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, NULL);

        // Enable the Vertex Attribute so that OpenGL knows to use it
        glad_glEnableVertexAttribArray(0);

        // Bind both the VBO and VAO to 0 so that we don't accidentally modify the VAO and VBO we created
        glad_glBindBuffer(GL_ARRAY_BUFFER, 0);
        glad_glBindVertexArray(0);

        // Bind the VAO so OpenGL knows to use it
        glad_glBindVertexArray(VAO);

        // Draw the triangle using the GL_TRIANGLE_STRIP/GL_LINE_STRIP primitive

        glad_glDrawArrays(IsWireframeRendering() ? GL_LINE_STRIP : GL_TRIANGLE_STRIP, 0, pVMCD->VifUnpack.NUM);

        glad_glDeleteVertexArrays(1, &VAO);
        glad_glDeleteBuffers(1, &VBO);

        vertexOffset += pVMCD->VifUnpack.NUM;
        pVMCD = (struct _SGDVUMESHCOLORDATA *) &pVMCD->avColor[pVMCD->VifUnpack.NUM];
    }

    glad_glUseProgram(id);
}

void MikuPan_RenderMeshType0x82(unsigned int *pVUVN, unsigned int *pPUHead)
{
    struct SGDVUVNDATA_PRESET *pVUVNData = (struct SGDVUVNDATA_PRESET *) &(
        ((struct SGDPROCUNITHEADER *) pVUVN)[1]);
    struct SGDVUMESHPOINTNUM *pMeshInfo = (struct SGDVUMESHPOINTNUM *) &(
        ((struct SGDPROCUNITHEADER *) pPUHead)[4]);

    int vertexOffset = 0;

    GLint id;
    glGetIntegerv(GL_CURRENT_PROGRAM,&id);
    glad_glUseProgram(shaderProgram);

    for (int i = 0; i < GET_NUM_MESH(pPUHead); i++)
    {
        if (pMeshInfo[i].uiPointNum == 0)
        {
            continue;
        }

        GLuint VAO, VBO;

        glad_glGenVertexArrays(1, &VAO);
        glad_glGenBuffers(1, &VBO);

        // Make the VAO the current Vertex Array Object by binding it
        glad_glBindVertexArray(VAO);

        // Bind the VBO specifying it's a GL_ARRAY_BUFFER
        glad_glBindBuffer(GL_ARRAY_BUFFER, VBO);

        // Introduce the vertices into the VBO
        glad_glBufferData(
            GL_ARRAY_BUFFER,
            pMeshInfo[i].uiPointNum * sizeof(struct SGDMESHVERTEXDATA_TYPE2),
            &pVUVNData->avt2[vertexOffset], GL_STATIC_DRAW);

        // Configure the Vertex Attribute so that OpenGL knows how to read the VBO
        glad_glVertexAttribPointer(
            0, 3, GL_FLOAT, GL_FALSE, sizeof(float[3]),
            NULL);

        // Enable the Vertex Attribute so that OpenGL knows to use it
        glad_glEnableVertexAttribArray(0);

        // Bind both the VBO and VAO to 0 so that we don't accidentally modify the VAO and VBO we created
        glad_glBindBuffer(GL_ARRAY_BUFFER, 0);
        glad_glBindVertexArray(0);

        // Bind the VAO so OpenGL knows to use it
        glad_glBindVertexArray(VAO);

        // Draw the triangle using the GL_TRIANGLE_STRIP primitive
        int render_type = IsWireframeRendering() ? GL_LINE_STRIP : GL_TRIANGLE_STRIP;
        glad_glDrawArrays(render_type, 0, pMeshInfo[i].uiPointNum);

        glad_glDeleteVertexArrays(1, &VAO);
        glad_glDeleteBuffers(1, &VBO);

        vertexOffset += pMeshInfo[i].uiPointNum;
    }

    glad_glUseProgram(id);
}