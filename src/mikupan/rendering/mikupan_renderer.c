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
#define PS2_CENTER_X 320.0f
#define PS2_CENTER_Y 224.0f

int window_width = 640;
int window_height = 448;

SDL_Window *window = NULL;
MikuPan_TextureInfo *fnt_texture[6] = {0};
MikuPan_TextureInfo *curr_fnt_texture = NULL;
GLuint VAO, VBO = 0;
GLuint uvVBO = 0;
GLuint gSpriteVAO, gSpriteVBO = 0;
GLuint gShapeVAO, gShapeVBO = 0;
GLuint gBBVAO, gBBVBO = 0;

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

    info_log("GLad version loaded %d", version);

    MikuPan_InitShaders();

    ///////// DEFAULT_SHADER /////////
    glad_glGenVertexArrays(1, &VAO);
    glad_glGenBuffers(1, &VBO);
    glad_glGenBuffers(1, &uvVBO);

    glad_glBindVertexArray(VAO);
    glad_glBindBuffer(GL_ARRAY_BUFFER, VBO);

    // Allocate max size once (no data yet)
    glad_glBufferData(GL_ARRAY_BUFFER, 1024 * 32, NULL, GL_DYNAMIC_DRAW);

    // Position attribute
    glad_glEnableVertexAttribArray(0);
    glad_glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float),
                               (void *) 0);

    glad_glBindBuffer(GL_ARRAY_BUFFER, uvVBO);

    // Allocate once, update per draw
    glad_glBufferData(GL_ARRAY_BUFFER,
                      1024 * 32,// max size
                      NULL, GL_DYNAMIC_DRAW);

    // Texcoord attribute
    glad_glEnableVertexAttribArray(1);
    glad_glVertexAttribPointer(1,// location
                               2, GL_FLOAT, GL_FALSE,
                               sizeof(float[2]),// tightly packed vec2
                               (void *) 0);

    glad_glBindVertexArray(0);

    ///////// UI_SPRITE_SHADER /////////
    glad_glGenVertexArrays(1, &gSpriteVAO);
    glad_glGenBuffers(1, &gSpriteVBO);

    glad_glBindVertexArray(gSpriteVAO);
    glad_glBindBuffer(GL_ARRAY_BUFFER, gSpriteVBO);

    glad_glBufferData(GL_ARRAY_BUFFER, sizeof(float) * 6 * 4, NULL,
                      GL_DYNAMIC_DRAW);

    // position
    glad_glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float),
                               (void *) 0);
    glad_glEnableVertexAttribArray(0);

    glad_glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float),
                               (void *) (2 * sizeof(float)));
    glad_glEnableVertexAttribArray(1);

    glad_glBindVertexArray(0);

    ///////// UNTEXTURED_SPRITE_SHADER /////////
    /// Buffers for line and square
    glad_glGenVertexArrays(1, &gShapeVAO);
    glad_glGenBuffers(1, &gShapeVBO);

    glad_glBindVertexArray(gShapeVAO);
    glad_glBindBuffer(GL_ARRAY_BUFFER, gShapeVBO);
    glad_glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), 0);
    glad_glEnableVertexAttribArray(0);

    ///////// BOUNDING_BOX_SHADER /////////
    glad_glGenVertexArrays(1, &gBBVAO);
    glad_glGenBuffers(1, &gBBVBO);

    glad_glBindVertexArray(gBBVAO);
    glad_glBindBuffer(GL_ARRAY_BUFFER, gBBVBO);

    glad_glBufferData(GL_ARRAY_BUFFER, sizeof(float[4]) * 24, NULL,
                      GL_DYNAMIC_DRAW);

    // position only
    glad_glEnableVertexAttribArray(0);
    glad_glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, sizeof(float[4]),
                               (void *) 0);

    return SDL_APP_CONTINUE;
}

void MikuPan_Clear()
{
    glad_glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glad_glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT
                 | GL_STENCIL_BUFFER_BIT);
}

void MikuPan_UpdateWindowSize(int width, int height)
{
    window_width = width;
    window_height = height;
}

MikuPan_TextureInfo *MikuPan_CreateGLTexture(const sceGsTex0 *tex0)
{
    GLuint tex = 0;

    /* ----------------------------------
       Get decoded texture data
       ---------------------------------- */

    int width = 1 << tex0->TW;
    int height = 1 << tex0->TH;

    /* This MUST return CPU-side RGBA8888 pixels */
    /* You already have this for SDL textures */
    void *pixels = DownloadGsTexture(tex0);

    if (!pixels)
    {
        return NULL;
    }

    /* ----------------------------------
       Create OpenGL texture
       ---------------------------------- */

    glad_glGenTextures(1, &tex);
    glad_glBindTexture(GL_TEXTURE_2D, tex);

    /* Pixel storage (PS2 textures are tightly packed) */
    glad_glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

    /* Upload texture */
    glad_glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, /* internal format */
                      width, height, 0, GL_RGBA,  /* input format */
                      GL_UNSIGNED_BYTE, pixels);

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
    MikuPan_TextureInfo *texture_info = malloc(sizeof(MikuPan_TextureInfo));
    texture_info->height = height;
    texture_info->width = width;
    texture_info->id = tex;

    MikuPan_AddTexture(tex0, texture_info);

    return texture_info;
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
                         sprite->alpha, texture_info);
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
                         sprite->alpha, curr_fnt_texture);
}

void MikuPan_RenderSquare(float x1, float y1, float x2, float y2, float x3,
                          float y3, float x4, float y4, u_char r, u_char g,
                          u_char b, u_char a)
{
    MikuPan_Rect rect;

    rect.x = (float) window_width * (x1 / PS2_RESOLUTION_X_FLOAT);
    rect.y = (float) window_height * (y1 / PS2_RESOLUTION_Y_FLOAT);

    rect.w = (float) window_width * ((x4 - x1) / PS2_RESOLUTION_X_FLOAT);
    rect.h = (float) window_height * ((y4 - y1) / PS2_RESOLUTION_Y_FLOAT);

    /* 1. Apply PS2 screen center offset */
    x1 += PS2_CENTER_X;
    y1 += PS2_CENTER_Y;
    x2 += PS2_CENTER_X;
    y2 += PS2_CENTER_Y;
    x3 += PS2_CENTER_X;
    y3 += PS2_CENTER_Y;
    x4 += PS2_CENTER_X;
    y4 += PS2_CENTER_Y;

    /* 2. Collect vertices */
    float px[4] = {x1, x2, x3, x4};
    float py[4] = {y1, y2, y3, y4};

    /* 3. Find quad center */
    float cx = (px[0] + px[1] + px[2] + px[3]) * 0.25f;
    float cy = (py[0] + py[1] + py[2] + py[3]) * 0.25f;

    /* 4. Classify into TL / TR / BR / BL */
    float tlx, tly, trx, try_, brx, bry, blx, bly;

    for (int i = 0; i < 4; i++)
    {
        if (px[i] <= cx && py[i] <= cy)
        {// top-left
            tlx = px[i];
            tly = py[i];
        }
        else if (px[i] > cx && py[i] <= cy)
        {// top-right
            trx = px[i];
            try_ = py[i];
        }
        else if (px[i] > cx && py[i] > cy)
        {// bottom-right
            brx = px[i];
            bry = py[i];
        }
        else
        {// bottom-left
            blx = px[i];
            bly = py[i];
        }
    }

    /* 5. Normalize */
    float stlx = tlx / PS2_RESOLUTION_X_FLOAT;
    float stly = tly / PS2_RESOLUTION_Y_FLOAT;
    float strx = trx / PS2_RESOLUTION_X_FLOAT;
    float stry = try_ / PS2_RESOLUTION_Y_FLOAT;
    float sbrx = brx / PS2_RESOLUTION_X_FLOAT;
    float sbry = bry / PS2_RESOLUTION_Y_FLOAT;
    float sblx = blx / PS2_RESOLUTION_X_FLOAT;
    float sbly = bly / PS2_RESOLUTION_Y_FLOAT;

    /* 6. Convert to NDC and build triangles */
    float vtx[] = {/* Triangle 1 */
                   stlx * 2.0f - 1.0f, 1.0f - stly * 2.0f, strx * 2.0f - 1.0f,
                   1.0f - stry * 2.0f, sbrx * 2.0f - 1.0f, 1.0f - sbry * 2.0f,

                   /* Triangle 2 */
                   stlx * 2.0f - 1.0f, 1.0f - stly * 2.0f, sbrx * 2.0f - 1.0f,
                   1.0f - sbry * 2.0f, sblx * 2.0f - 1.0f, 1.0f - sbly * 2.0f};

    MikuPan_SetCurrentShaderProgram(UNTEXTURED_SPRITE_SHADER);
    glad_glUseProgram(MikuPan_GetCurrentShaderProgram());// same shader as lines
    glad_glBindVertexArray(gShapeVAO);
    glad_glBindBuffer(GL_ARRAY_BUFFER, gShapeVBO);

    glad_glBufferData(GL_ARRAY_BUFFER, sizeof(vtx), vtx, GL_DYNAMIC_DRAW);

    glad_glUniform4f(
        glad_glGetUniformLocation(MikuPan_GetCurrentShaderProgram(), "uColor"),
        AdjustAlpha(r) / 255.0f, AdjustAlpha(g) / 255.0f,
        AdjustAlpha(b) / 255.0f, AdjustAlpha(a) / 255.0f);

    glad_glEnable(GL_BLEND);
    glad_glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glad_glDrawArrays(GL_TRIANGLES, 0, 6);

    glad_glBindVertexArray(0);
    glad_glUseProgram(0);
}

void MikuPan_RenderLine(float x1, float y1, float x2, float y2, u_char r,
                        u_char g, u_char b, u_char a)
{
    float dst_x1 =
        (float) window_width * (300.0f + x1) / PS2_RESOLUTION_X_FLOAT;
    float dst_y1 =
        (float) window_height * (200.0f + y1) / PS2_RESOLUTION_Y_FLOAT;
    float dst_x2 =
        (float) window_width * (300.0f + x2) / PS2_RESOLUTION_X_FLOAT;
    float dst_y2 =
        (float) window_height * (200.0f + y2) / PS2_RESOLUTION_Y_FLOAT;

    // Convert PS2 space → screen space → NDC
    float sx1 = (300.0f + x1) / PS2_RESOLUTION_X_FLOAT;
    float sy1 = (200.0f + y1) / PS2_RESOLUTION_Y_FLOAT;
    float sx2 = (300.0f + x2) / PS2_RESOLUTION_X_FLOAT;
    float sy2 = (200.0f + y2) / PS2_RESOLUTION_Y_FLOAT;

    float ndc_x1 = sx1 * 2.0f - 1.0f;
    float ndc_y1 = 1.0f - sy1 * 2.0f;
    float ndc_x2 = sx2 * 2.0f - 1.0f;
    float ndc_y2 = 1.0f - sy2 * 2.0f;

    float vertices[4] = {ndc_x1, ndc_y1, ndc_x2, ndc_y2};

    MikuPan_SetCurrentShaderProgram(UNTEXTURED_SPRITE_SHADER);
    u_int program = MikuPan_GetCurrentShaderProgram();
    glad_glUseProgram(program);
    glad_glBindVertexArray(gShapeVAO);
    glad_glBindBuffer(GL_ARRAY_BUFFER, gShapeVBO);

    glad_glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices,
                      GL_DYNAMIC_DRAW);

    // Color + alpha (SDL-compatible)
    glad_glUniform4f(glad_glGetUniformLocation(program, "uColor"),
                     AdjustAlpha(r) / 255.0f, AdjustAlpha(g) / 255.0f,
                     AdjustAlpha(b) / 255.0f, AdjustAlpha(a) / 255.0f);

    glad_glEnable(GL_BLEND);
    glad_glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glad_glDrawArrays(GL_LINES, 0, 2);

    glad_glBindVertexArray(0);
    glad_glUseProgram(0);
}

void MikuPan_RenderBoundingBox(sceVu0FVECTOR *vertices)
{
    MikuPan_SetShaderProgramWithBackup(BOUNDING_BOX_SHADER);

    glad_glBindVertexArray(gBBVAO);
    glad_glBindBuffer(GL_ARRAY_BUFFER, gBBVBO);

    glad_glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(float[4]) * 24, vertices);

    glad_glDisable(GL_CULL_FACE);
    glad_glDepthMask(GL_FALSE);
    glad_glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);

    for (int i = 0; i < 6; i++)
    {
        glad_glDrawArrays(GL_LINE_LOOP, i * 4, 4);
    }

    glad_glDepthMask(GL_TRUE);
    glad_glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

    MikuPan_RestoreCurrentShaderProgram();
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

    // Compute destination rectangle in screen space
    float x0 = (float) window_width * (dst.x / PS2_RESOLUTION_X_FLOAT);
    float y0 = (float) window_height * (dst.y / PS2_RESOLUTION_Y_FLOAT);
    float x1 = x0 + (float) window_width * (src.w / PS2_RESOLUTION_X_FLOAT);
    float y1 = y0 + (float) window_height * (src.h / PS2_RESOLUTION_Y_FLOAT);

    // Convert screen space to OpenGL NDC (-1 to 1)
    float ndc_x0 = (x0 / window_width) * 2.0f - 1.0f;
    float ndc_x1 = (x1 / window_width) * 2.0f - 1.0f;
    float ndc_y0 = 1.0f - (y0 / window_height) * 2.0f;// Flip Y
    float ndc_y1 = 1.0f - (y1 / window_height) * 2.0f;

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

    // Vertex array: {pos_x, pos_y, uv_u, uv_v}
    float vertices[6][4] = {
        {ndc_x0, ndc_y1, u0, v1}, // bottom-left
        {ndc_x1, ndc_y1, u1, v1}, // bottom-right
        {ndc_x1, ndc_y0, u1, v0}, // top-right
        {ndc_x0, ndc_y1, u0, v1}, // bottom-left
        {ndc_x1, ndc_y0, u1, v0}, // top-right
        {ndc_x0, ndc_y0, u0, v0}  // top-left
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
    MikuPan_SetCurrentShaderProgram(UI_SPRITE_SHADER);
    glad_glUseProgram(MikuPan_GetCurrentShaderProgram());

    glad_glActiveTexture(GL_TEXTURE0);
    glad_glBindTexture(GL_TEXTURE_2D, texture_info->id);

    glad_glEnable(GL_BLEND);
    glad_glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glad_glDepthMask(GL_FALSE);

    float color[4] = {AdjustAlpha(r) / 255.0f, AdjustAlpha(g) / 255.0f,
                      AdjustAlpha(b) / 255.0f, AdjustAlpha(a) / 255.0f};

    int gSpriteColorLoc =
        glad_glGetUniformLocation(MikuPan_GetCurrentShaderProgram(), "uColor");
    glad_glUniform4fv(gSpriteColorLoc, 1, color);

    glad_glBindVertexArray(gSpriteVAO);
    glad_glDrawArrays(GL_TRIANGLES, 0, 6);

    glad_glDepthMask(GL_TRUE);
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
    for (int i = 0; i < MAX_SHADER_PROGRAMS; i++)
    {
        MikuPan_SetShaderProgramWithBackup(i);

        u_int current_program = MikuPan_GetCurrentShaderProgram();
        glad_glUniformMatrix4fv(
            glad_glGetUniformLocation(current_program, "model"), 1, GL_FALSE,
            (float *) m);
    }
}

void MikuPan_SetModelTransform(unsigned int *prim)
{
    for (int i = 0; i < MAX_SHADER_PROGRAMS; i++)
    {
        MikuPan_SetShaderProgramWithBackup(i);

        u_int current_program = MikuPan_GetCurrentShaderProgram();
        glad_glUniformMatrix4fv(
            glad_glGetUniformLocation(current_program, "model"), 1, GL_FALSE,
            (float *) lcp[prim[1]].lwmtx);
    }
}

void MikuPan_RenderVertices(float *vertices, int num)
{
    MikuPan_SetShaderProgramWithBackup(DEFAULT_SHADER);

    glad_glBindBuffer(GL_ARRAY_BUFFER, VBO);

    // Optional but recommended: orphan buffer to avoid stalls
    glad_glBufferData(GL_ARRAY_BUFFER, num * sizeof(float[3]), NULL,
                      GL_DYNAMIC_DRAW);

    // Upload new data
    glad_glBufferSubData(GL_ARRAY_BUFFER, 0, num * sizeof(float[3]), vertices);

    // Draw
    glad_glBindVertexArray(VAO);
    glad_glDrawArrays(MikuPan_IsWireframeRendering() ? GL_LINE_STRIP
                                                     : GL_TRIANGLE_STRIP,
                      0, num);
}

void MikuPan_Camera(SgCAMERA *camera)
{
    // View -> camera->wv
    mat4 mtx = {0};
    vec3 center = {0};
    glm_vec3_add(camera->p, camera->zd, center);
    vec3 up = {0};
    mat4 roll = {0};
    vec3 axis = {0.0f, 0.0f, 1.0f};
    glm_mat4_identity(roll);
    glm_rotate(roll, -camera->roll, axis);
    glm_mat4_mulv3(roll, camera->yd, 1.0f, up);
    glm_lookat(camera->p, center, up, mtx);

    // Projection -> camera->vcv
    mat4 projection = {0};
    float aspect = (float) window_width / (float) window_height;
    glm_perspective(camera->fov, aspect, camera->nearz, camera->farz,
                    projection);

    for (int i = 0; i < MAX_SHADER_PROGRAMS; i++)
    {
        MikuPan_SetShaderProgramWithBackup(i);

        u_int current_program = MikuPan_GetCurrentShaderProgram();
        glad_glUniformMatrix4fv(
            glad_glGetUniformLocation(current_program, "view"), 1, GL_FALSE,
            (float *) mtx);
        glad_glUniformMatrix4fv(
            glad_glGetUniformLocation(current_program, "projection"), 1,
            GL_FALSE, (float *) projection);
    }

    MikuPan_RestoreCurrentShaderProgram();
}

void MikuPan_RenderMeshType0x32(struct SGDPROCUNITHEADER *pVUVN,
                                struct SGDPROCUNITHEADER *pPUHead)
{
    MikuPan_SetShaderProgramWithBackup(SIMPLE_TEXTURED_SHADER);

    union SGDPROCUNITDATA *pVUVNData = (union SGDPROCUNITDATA *) &pVUVN[1];
    union SGDPROCUNITDATA *pProcData = (union SGDPROCUNITDATA *) &pPUHead[1];

    struct SGDVUMESHPOINTNUM *pMeshInfo =
        (struct SGDVUMESHPOINTNUM *) &pPUHead[4];

    struct SGDVUMESHSTDATA *sgdMeshData =
        (struct SGDVUMESHSTDATA *) ((int64_t) pProcData
                                    + (pProcData->VUMeshData_Preset.sOffsetToST
                                       - 1)
                                          * 4);

    struct _SGDVUMESHCOLORDATA *pVMCD =
        (struct _SGDVUMESHCOLORDATA
             *) (&pPUHead->pNext + pProcData->VUMeshData_Preset.sOffsetToPrim);

    int vertexOffset = 0;

    sceGsTex0 *mesh_tex_reg = (sceGsTex0 *) ((int64_t) pProcData + 0x28);

    if (pProcData->VUMeshData.GifTag.NREG != 6)
    {
        //return;
        // /mesh_tex_reg = (sceGsTex0 *) ((int64_t) pProcData + 0x18);
    }

    MikuPan_TextureInfo *texture_info = MikuPan_GetTextureInfo(mesh_tex_reg);

    if (texture_info == NULL)
    {
        texture_info = MikuPan_CreateGLTexture(mesh_tex_reg);
    }

    glad_glDepthMask(GL_FALSE);
    glad_glActiveTexture(GL_TEXTURE0);

    if (texture_info != NULL)
    {
        glad_glBindTexture(GL_TEXTURE_2D, texture_info->id);
    }


    for (int i = 0; i < GET_NUM_MESH(pPUHead); i++)
    {
        pVMCD =
            (struct _SGDVUMESHCOLORDATA *) GetNextUnpackAddr((u_int *) pVMCD);

        GLfloat *vertices =
            (GLfloat *) (pVUVNData->VUVNData_Preset.aui
                         + (vertexOffset + pVUVN->VUVNDesc.sNumNormal) * 3
                         + 10);

        size_t vertexCount = pVMCD->VifUnpack.NUM;
        size_t byteSize = vertexCount * sizeof(float[3]);

        glad_glBindBuffer(GL_ARRAY_BUFFER, VBO);

        // Optional but recommended: orphan buffer to avoid stalls
        glad_glBufferData(GL_ARRAY_BUFFER, byteSize, NULL, GL_DYNAMIC_DRAW);

        // Upload new data
        glad_glBufferSubData(GL_ARRAY_BUFFER, 0, byteSize, vertices);

        glad_glBindBuffer(GL_ARRAY_BUFFER, uvVBO);
        glad_glBufferData(GL_ARRAY_BUFFER,
                          1024 * 32,// max size
                          NULL, GL_DYNAMIC_DRAW);

        glad_glBufferSubData(GL_ARRAY_BUFFER, 0, vertexCount * sizeof(float[2]),
                             sgdMeshData->astData);

        // Draw
        glad_glBindVertexArray(VAO);
        glad_glDrawArrays(MikuPan_IsWireframeRendering() ? GL_LINE_STRIP
                                                         : GL_TRIANGLE_STRIP,
                          0, vertexCount);

        sgdMeshData = (struct SGDVUMESHSTDATA *) &sgdMeshData
                          ->astData[pVMCD->VifUnpack.NUM];
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
        glad_glDrawArrays(GL_TRIANGLE_STRIP, 0, pMeshInfo[i].uiPointNum);

        glad_glDeleteVertexArrays(1, &VAO);
        glad_glDeleteBuffers(1, &VBO);

        vertexOffset += pMeshInfo[i].uiPointNum;
    }
}

void MikuPan_RenderMeshType0x2(struct SGDPROCUNITHEADER *pVUVN,
                               struct SGDPROCUNITHEADER *pPUHead)
{
    return;
    MikuPan_SetShaderProgramWithBackup(DEFAULT_SHADER);

    VUVN_PRIM *v = ((VUVN_PRIM *) &pVUVN[2]);

    u_int *vector_data = (u_int *) &(((struct SGDPROCUNITHEADER *) pVUVN)[3]);

    struct SGDVUMESHPOINTNUM *pMeshInfo =
        (struct SGDVUMESHPOINTNUM *) &pPUHead[4];
    struct SGDVUMESHSTREGSET *sgdVuMeshStRegSet =
        (struct SGDVUMESHSTREGSET *) &pMeshInfo[GET_NUM_MESH(pPUHead)];
    struct SGDVUMESHSTDATA *sgdMeshData =
        (struct SGDVUMESHSTDATA *) &sgdVuMeshStRegSet->auiVifCode[3];

    int vertex_offset = 0;

    glad_glBindBuffer(GL_ARRAY_BUFFER, VBO);

    for (int i = 0; i < GET_NUM_MESH(pPUHead); i++)
    {
        size_t vertexCount = pMeshInfo[i].uiPointNum;
        size_t byteSize = vertexCount * sizeof(float[4]);

        //for (int j = 0; j < vertexCount; j++)
        {
            GLfloat *vertices =
                (GLfloat *) MikuPan_GetHostPointer(vector_data[i]);

            // Optional but recommended: orphan buffer to avoid stalls
            glad_glBufferData(GL_ARRAY_BUFFER, byteSize, NULL, GL_DYNAMIC_DRAW);

            // Upload new data
            glad_glBufferSubData(GL_ARRAY_BUFFER, 0, byteSize, vertices);
        }

        vertex_offset += vertexCount;

        // Draw
        glad_glBindVertexArray(VAO);
        glad_glDrawArrays(MikuPan_IsWireframeRendering() ? GL_LINE_STRIP
                                                         : GL_TRIANGLE_STRIP,
                          0, vertexCount);
    }
}