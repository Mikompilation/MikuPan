#include "mikupan_renderer.h"
#include "../mikupan_types.h"
#include "SDL3/SDL_hints.h"
#include "SDL3/SDL_init.h"
#include "cglm/cglm.h"
#include "common/utility.h"
#include "graphics/graph2d/message.h"
#include "graphics/graph3d/sgcam.h"
#include "graphics/graph3d/sglight.h"
#include "graphics/graph3d/sgsu.h"
#include "mikupan/gs/gs_server_c.h"
#include "mikupan/gs/texture_manager_c.h"
#include "mikupan/mikupan_logging_c.h"
#include "mikupan/ui/mikupan_ui_c.h"
#include "mikupan_shader.h"
#include <mikupan/mikupan_memory.h>
#include <stdlib.h>

#define GLAD_GL_IMPLEMENTATION
#include <glad/gl.h>

#define PS2_RESOLUTION_X_FLOAT 640.0f
#define PS2_RESOLUTION_X_INT 640
#define PS2_RESOLUTION_Y_FLOAT 448.0f
#define PS2_RESOLUTION_Y_INT 448

int window_width = 640;
int window_height = 448;

SDL_Window *window = NULL;
SDL_Texture *fnt_texture[6] = {0};
SDL_Texture *curr_fnt_texture = NULL;
GLuint VAO, VBO = 0;
GLuint gSpriteVAO, gSpriteVBO = 0;

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

    SDL_GLContext gl_context = SDL_GL_CreateContext(window);
    SDL_GL_MakeCurrent(window, gl_context);

    MikuPan_InitUi(window, gl_context);

    int version = gladLoadGL(SDL_GL_GetProcAddress);

    MikuPan_InitShaders();

    glad_glGenVertexArrays(1, &VAO);
    glad_glGenBuffers(1, &VBO);

    glad_glBindVertexArray(VAO);
    glad_glBindBuffer(GL_ARRAY_BUFFER, VBO);

    // Allocate max size once (no data yet)
    glad_glBufferData(GL_ARRAY_BUFFER,
                      1024*1024,
                      NULL,
                      GL_DYNAMIC_DRAW);

    // Position attribute
    glad_glEnableVertexAttribArray(0);
    glad_glVertexAttribPointer(
        0, 3, GL_FLOAT, GL_FALSE,
        3 * sizeof(float),
        (void*)0
    );

    glad_glBindVertexArray(0);

    glad_glGenVertexArrays(1, &gSpriteVAO);
    glad_glGenBuffers(1, &gSpriteVBO);

    glad_glBindVertexArray(gSpriteVAO);
    glad_glBindBuffer(GL_ARRAY_BUFFER, gSpriteVBO);

    glad_glBufferData(GL_ARRAY_BUFFER, sizeof(float) * 6 * 4, NULL, GL_DYNAMIC_DRAW);

    // position
    glad_glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glad_glEnableVertexAttribArray(0);

    glad_glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
    glad_glEnableVertexAttribArray(1);

    glad_glBindVertexArray(0);

    return SDL_APP_CONTINUE;
}

void MikuPan_Clear()
{
    glad_glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glad_glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
}

void MikuPan_UpdateWindowSize(int width, int height)
{
    window_width = width;
    window_height = height;
}

GLuint MikuPan_CreateGLTexture(const sceGsTex0 *tex0)
{
    GLuint tex = 0;

    /* ----------------------------------
       Get decoded texture data
       ---------------------------------- */

    int width  = 1 << tex0->TW;
    int height = 1 << tex0->TH;

    /* This MUST return CPU-side RGBA8888 pixels */
    /* You already have this for SDL textures */
    void *pixels = DownloadGsTexture(tex0);

    if (!pixels)
    {
        return 0;
    }

    /* ----------------------------------
       Create OpenGL texture
       ---------------------------------- */

    glad_glGenTextures(1, &tex);
    glad_glBindTexture(GL_TEXTURE_2D, tex);

    /* Pixel storage (PS2 textures are tightly packed) */
    glad_glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

    /* Upload texture */
    glad_glTexImage2D(
        GL_TEXTURE_2D,
        0,
        GL_RGBA8,            /* internal format */
        width,
        height,
        0,
        GL_RGBA,             /* input format */
        GL_UNSIGNED_BYTE,
        pixels
    );

    /* ----------------------------------
       Sampler state (matches SDL defaults)
       ---------------------------------- */

    glad_glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glad_glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glad_glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glad_glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

    /* ----------------------------------
       Optional mipmaps (disable if unwanted)
       ---------------------------------- */
    // glGenerateMipmap(GL_TEXTURE_2D);

    glad_glBindTexture(GL_TEXTURE_2D, 0);

    /* ----------------------------------
       Cache mapping: tex0 → GL texture
       ---------------------------------- */

    //MikuPan_RegisterGLTexture(tex0, tex);

    return tex;
}

void MikuPan_Render2DTexture(DISP_SPRT *sprite)
{
    if (!IsFirstUploadDone())
        return;

    sceGsTex0 *tex0 = (sceGsTex0 *)&sprite->tex0;

    // GetSDLTexture()
    GLuint glTex = 0; //(GLuint)GetGLTexture(tex0);

    if (glTex == 0)
    {
        glTex = MikuPan_CreateGLTexture(tex0);
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

    int container_width  = 1 << tex0->TW;
    int container_height = 1 << tex0->TH;

    // Compute destination rectangle in screen space
    float x0 = (float)window_width * (sprite->x / PS2_RESOLUTION_X_FLOAT);
    float y0 = (float)window_height * (sprite->y / PS2_RESOLUTION_Y_FLOAT);
    float x1 = x0 + (float)window_width * (sprite->w / PS2_RESOLUTION_X_FLOAT);
    float y1 = y0 + (float)window_height * (sprite->h / PS2_RESOLUTION_Y_FLOAT);

    // Convert screen space to OpenGL NDC (-1 to 1)
    float ndc_x0 = (x0 / window_width)  * 2.0f - 1.0f;
    float ndc_x1 = (x1 / window_width)  * 2.0f - 1.0f;
    float ndc_y0 = 1.0f - (y0 / window_height) * 2.0f; // Flip Y
    float ndc_y1 = 1.0f - (y1 / window_height) * 2.0f;

    // Container size (PS2 texture memory size)
    float texW = (float)(1 << tex0->TW);
    float texH = (float)(1 << tex0->TH);

    // Half-texel offsets (PS2-style snapping)
    float halfU = 0.5f / texW;
    float halfV = 0.5f / texH;

    // Normalize sprite rectangle (pixel-accurate)
    float u0 = (sprite->u / texW) + halfU;
    float v0 = (sprite->v / texH) + halfV;
    float u1 = ((sprite->u + sprite->w) / texW) - halfU;
    float v1 = ((sprite->v + sprite->h) / texH) - halfV;

    // Vertex array: {pos_x, pos_y, uv_u, uv_v}
    float vertices[6][4] = {
        { ndc_x0, ndc_y1, u0, v1 }, // bottom-left
        { ndc_x1, ndc_y1, u1, v1 }, // bottom-right
        { ndc_x1, ndc_y0, u1, v0 }, // top-right
        { ndc_x0, ndc_y1, u0, v1 }, // bottom-left
        { ndc_x1, ndc_y0, u1, v0 }, // top-right
        { ndc_x0, ndc_y0, u0, v0 }  // top-left
    };

    /* ---------------------------
       Upload quad (dynamic)
       --------------------------- */

    glad_glBindBuffer(GL_ARRAY_BUFFER, gSpriteVBO);
    glad_glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), NULL, GL_DYNAMIC_DRAW);
    glad_glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(vertices), vertices);

    /* ---------------------------
       Render state
       --------------------------- */
    MikuPan_SetCurrentShaderProgram(HUD_SHADER);
    glad_glUseProgram(MikuPan_GetCurrentShaderProgram());

    glad_glActiveTexture(GL_TEXTURE0);
    glad_glBindTexture(GL_TEXTURE_2D, glTex);

    glad_glEnable(GL_BLEND);
    glad_glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    float color[4] = {
        AdjustAlpha(sprite->r) / 255.0f,
        AdjustAlpha(sprite->g) / 255.0f,
        AdjustAlpha(sprite->b) / 255.0f,
        AdjustAlpha(sprite->alpha) / 255.0f
    };

    int gSpriteColorLoc = glad_glGetUniformLocation(MikuPan_GetCurrentShaderProgram(), "uColor");
    glad_glUniform4fv(gSpriteColorLoc, 1, color);

    glad_glBindVertexArray(gSpriteVAO);
    glad_glDrawArrays(GL_TRIANGLES, 0, 6);
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
    //SDL_RenderTexture(renderer, texture, &src_rect, &dst_rect);
}

void MikuPan_RenderSquare(float x1, float y1, float x2, float y2, float x3,
                          float y3, float x4, float y4, u_char r, u_char g,
                          u_char b, u_char a)
{
    //SDL_SetRenderDrawColor(renderer, AdjustAlpha(r), AdjustAlpha(g),
    //                       AdjustAlpha(b), AdjustAlpha(a));

    SDL_FRect rect;

    rect.x = (float) window_width * (x1 / PS2_RESOLUTION_X_FLOAT);
    rect.y = (float) window_height * (y1 / PS2_RESOLUTION_Y_FLOAT);

    rect.w = (float) window_width * ((x4 - x1) / PS2_RESOLUTION_X_FLOAT);
    rect.h = (float) window_height * ((y4 - y1) / PS2_RESOLUTION_Y_FLOAT);
    //SDL_RenderFillRect(renderer, &rect);
}

void MikuPan_RenderLine(float x1, float y1, float x2, float y2, u_char r,
                        u_char g, u_char b, u_char a)
{
    //SDL_SetRenderDrawColor(renderer, r, g, b, AdjustAlpha(a));

    float dst_x1 =
        (float) window_width * (300.0f + x1) / PS2_RESOLUTION_X_FLOAT;
    float dst_y1 =
        (float) window_height * (200.0f + y1) / PS2_RESOLUTION_Y_FLOAT;
    float dst_x2 =
        (float) window_width * (300.0f + x2) / PS2_RESOLUTION_X_FLOAT;
    float dst_y2 =
        (float) window_height * (200.0f + y2) / PS2_RESOLUTION_Y_FLOAT;

    //SDL_RenderLine(renderer, dst_x1, dst_y1, dst_x2, dst_y2);
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

    //SDL_Texture *texture = SDL_CreateTextureFromSurface(renderer, surface);
    //SDL_DestroySurface(surface);

    //AddSDLTexture(tex0, texture);

    //return texture;
    return NULL;
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
    //SDL_DestroyRenderer(renderer);

    // destroy the window
    SDL_DestroyWindow(window);
}

void MikuPan_EndFrame()
{
    MikuPan_DrawUi();
    MikuPan_RenderUi();
    SDL_GL_SwapWindow(window);
}

void MikuPan_SetModelTransform(unsigned int *prim)
{
    MikuPan_SetShaderProgramWithBackup(DEFAULT_SHADER);
    u_int current_program = MikuPan_GetCurrentShaderProgram();

    int modelLoc = glad_glGetUniformLocation(current_program, "model");

    glad_glUniformMatrix4fv(modelLoc, 1, GL_FALSE,
                            (float *) &lcp[prim[2]].lwmtx[0]);

    MikuPan_RestoreCurrentShaderProgram();
}

void MikuPan_Camera(const SgCAMERA *camera)
{
    struct GRA3DSCRATCHPADLAYOUT *scratchpad =
        (struct GRA3DSCRATCHPADLAYOUT *) ps2_virtual_scratchpad;

    MikuPan_SetShaderProgramWithBackup(DEFAULT_SHADER);

    mat4 mtx = {0};
    vec3 cam = {camera->p[0], camera->p[1], camera->p[2]};

    // camera->zd
    vec3 zd = {camera->i[0] - cam[0], camera->i[1] - cam[1],
               camera->i[2] - cam[2]};
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
    glm_lookat(cam, center, up, mtx);

    u_int current_program = MikuPan_GetCurrentShaderProgram();

    int viewLoc = glad_glGetUniformLocation(current_program, "view");

    glad_glUniformMatrix4fv(viewLoc, 1, GL_FALSE, (float *) &mtx);

    // Projection
    mat4 projection = {0};
    float aspect = (float) window_width / (float) window_height;
    glm_perspective(camera->fov, aspect, camera->nearz, camera->farz,
                    projection);

    int projectionLoc =
        glad_glGetUniformLocation(current_program, "projection");

    glad_glUniformMatrix4fv(projectionLoc, 1, GL_FALSE, (float *) &projection);

    MikuPan_RestoreCurrentShaderProgram();
}

void MikuPan_RenderMeshType0x32(struct SGDPROCUNITHEADER *pVUVN,
                                struct SGDPROCUNITHEADER *pPUHead)
{
    MikuPan_SetShaderProgramWithBackup(DEFAULT_SHADER);

    union SGDPROCUNITDATA *pVUVNData = (union SGDPROCUNITDATA *) &pVUVN[1];
    union SGDPROCUNITDATA *pProcData = (union SGDPROCUNITDATA *) &pPUHead[1];

    struct SGDVUMESHPOINTNUM *pMeshInfo =
        (struct SGDVUMESHPOINTNUM *) &pPUHead[4];

    struct SGDVUMESHSTDATA *sgdMeshData =
        (struct SGDVUMESHSTDATA *) ((int64_t) pVUVNData
                                    + (pVUVNData->VUMeshData_Preset.sOffsetToST
                                       - 1)
                                          * 4);

    struct _SGDVUMESHCOLORDATA *pVMCD =
        (struct _SGDVUMESHCOLORDATA
             *) (&pPUHead->pNext + pProcData->VUMeshData_Preset.sOffsetToPrim);

    int vertexOffset = 0;

    sceGsTex0* mesh_tex_reg = (sceGsTex0*)((int64_t)pProcData + 0x18);

    if (pProcData->VUMeshData.GifTag.NREG == 6)
    {
        mesh_tex_reg = (sceGsTex0*)((int64_t)pProcData + 0x28);
    }

    //unsigned char* img = DownloadGsTexture(mesh_tex_reg);

    for (int i = 0; i < GET_NUM_MESH(pPUHead); i++)
    {
        pVMCD =
            (struct _SGDVUMESHCOLORDATA *) GetNextUnpackAddr((u_int *) pVMCD);

        GLfloat *vertices =
            (GLfloat *) (pVUVNData->VUVNData_Preset.aui
                         + (vertexOffset + pVUVN->VUVNDesc.sNumNormal) * 3
                         + 10);

        size_t vertexCount = pVMCD->VifUnpack.NUM;
        size_t byteSize    = vertexCount * sizeof(float[3]);

        glad_glBindBuffer(GL_ARRAY_BUFFER, VBO);

        // Optional but recommended: orphan buffer to avoid stalls
        glad_glBufferData(GL_ARRAY_BUFFER,
                          byteSize,
                          NULL,
                          GL_DYNAMIC_DRAW);

        // Upload new data
        glad_glBufferSubData(GL_ARRAY_BUFFER,
                             0,
                             byteSize,
                             vertices);

        // Draw
        glad_glBindVertexArray(VAO);
        glad_glDrawArrays(
            !MikuPan_IsWireframeRendering() ? GL_LINE_STRIP
                                           : GL_TRIANGLE_STRIP,
            0,
            vertexCount
        );

        vertexOffset += pVMCD->VifUnpack.NUM;
        pVMCD = (struct _SGDVUMESHCOLORDATA *) &pVMCD
                    ->avColor[pVMCD->VifUnpack.NUM];
    }

    MikuPan_RestoreCurrentShaderProgram();
}

void MikuPan_RenderMeshType0x82(unsigned int *pVUVN, unsigned int *pPUHead)
{
    return;
    struct SGDVUVNDATA_PRESET *pVUVNData = (struct SGDVUVNDATA_PRESET *) &(
        ((struct SGDPROCUNITHEADER *) pVUVN)[1]);
    struct SGDVUMESHPOINTNUM *pMeshInfo = (struct SGDVUMESHPOINTNUM *) &(
        ((struct SGDPROCUNITHEADER *) pPUHead)[4]);

    int vertexOffset = 0;

    MikuPan_SetShaderProgramWithBackup(DEFAULT_SHADER);

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
        glad_glBufferData(GL_ARRAY_BUFFER,
                          pMeshInfo[i].uiPointNum
                              * sizeof(struct SGDMESHVERTEXDATA_TYPE2),
                          &pVUVNData->avt2[vertexOffset], GL_STATIC_DRAW);

        // Configure the Vertex Attribute so that OpenGL knows how to read the VBO
        glad_glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(float[3]),
                                   NULL);

        // Enable the Vertex Attribute so that OpenGL knows to use it
        glad_glEnableVertexAttribArray(0);

        // Bind both the VBO and VAO to 0 so that we don't accidentally modify the VAO and VBO we created
        glad_glBindBuffer(GL_ARRAY_BUFFER, 0);
        glad_glBindVertexArray(0);

        // Bind the VAO so OpenGL knows to use it
        glad_glBindVertexArray(VAO);

        // Draw the triangle using the GL_TRIANGLE_STRIP primitive
        int render_type =
            MikuPan_IsWireframeRendering() ? GL_LINE_STRIP : GL_TRIANGLE_STRIP;
        glad_glDrawArrays(render_type, 0, pMeshInfo[i].uiPointNum);

        glad_glDeleteVertexArrays(1, &VAO);
        glad_glDeleteBuffers(1, &VBO);

        vertexOffset += pMeshInfo[i].uiPointNum;
    }

    MikuPan_RestoreCurrentShaderProgram();
}

void MikuPan_RenderMeshType0x2(unsigned int *pVUVN, unsigned int *pPUHead)
{
    u_int *vector_data = (u_int *) &(
        ((struct SGDPROCUNITHEADER *) pVUVN)[3]);

    float* vertex = ((sceVu0FVECTOR *)MikuPan_GetHostAddress(VNBufferAddress))[0];
    float* vertex_list = (float*)&vector_data[GET_NUM_MESH(pPUHead) + 2];

    // auto pVectorData = (_VECTORDATA  *) &s_ppuhVUVN[3];
    // const auto pVectorInfo = GetVectorInfoPtr(sgdCurr);
    // const auto pSVAUniqueVertex = RelOffsetToPtr<Vector4>(
    //     sgdCurr, pVectorInfo->aAddress[SVA_UNIQUE].pvVertex);
    // const auto pSVAUniqueNormal = RelOffsetToPtr<Vector4>(
    //     sgdCurr, pVectorInfo->aAddress[SVA_UNIQUE].pvNormal);
    // auto wVertex =
    //     pSVAUniqueVertex[pVectorData[meshIndex].vIndex.uiVertexId];
}