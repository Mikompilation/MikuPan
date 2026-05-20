#include "mikupan_renderer.h"
#include "../mikupan_types.h"
#include "SDL3/SDL_hints.h"
#include "SDL3/SDL_init.h"
#include "cglm/cglm.h"
#include "graphics/graph2d/message.h"
#include "graphics/graph3d/sgsu.h"
#include "mikupan/gs/mikupan_gs_c.h"
#include "mikupan/gs/mikupan_texture_manager_c.h"
#include "mikupan/mikupan_logging_c.h"
#include "mikupan/mikupan_screenshot.h"
#include "mikupan/ui/mikupan_ui.h"
#include "mikupan_profiler.h"
#include "mikupan_shader.h"
#include "SDL3/SDL_timer.h"
#include <stdlib.h>
#include <string.h>

#define GLAD_GL_IMPLEMENTATION
#include "graphics/graph3d/sglib.h"
#include "main/glob.h"
#include "mikupan/mikupan_config.h"
#include "mikupan/mikupan_utils.h"
#include "mikupan_meshcache.h"
#include "mikupan_pipeline.h"

#include <glad/gl.h>

typedef struct
{
    int active;
    int sprite_count;
    GLuint texture_id;
} MikuPan_TexturedSpriteBatch;

static MikuPan_MeshBuffers0x32 g_mesh_buffers_0x32 = {0};
static MikuPan_MeshBuffers0x82 g_mesh_buffers_0x82 = {0};
static MikuPan_MeshBuffers0x2  g_mesh_buffers_0x2  = {0};
static float g_zero_floats[MIKUPAN_MESH_BUFFER_CAPACITY] = {0};
static MikuPan_TexturedSpriteBatch g_textured_sprite_batch = {0};
static float g_textured_sprite_cpu_buf[TEXTURED_SPRITE_BATCH_MAX * 6 * 12];
MikuPan_RenderWindow mikupan_render = {0};
MikuPan_MsaaBufferObject render_back_msaa = {0};
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
static mat4 g_cached_model_matrix;
static int  g_has_cached_model = 0;
static GLfloat g_max_aniso = -1.0f;
#define SHADOW_FBO_SIZE 256
static GLuint g_shadow_fbo  = 0;
static GLuint g_shadow_tex  = 0;
static int    g_shadow_init = 0;
static int    g_shadow_enabled = 0;
static GLint  g_shadow_saved_fbo      = 0;
static int    g_shadow_saved_viewport[4] = {0};
static mat4 g_shadow_world_clip_view;
static int  g_shadow_matrix_valid = 0;
static int g_shadow_pass_active = 0;
MikuPan_LightData    mikupan_light_data    = {0};
MikuPan_MaterialData mikupan_material_data = {
    {1.0f, 1.0f, 1.0f, 1.0f}, // Ambient — identity until first SetMaterial
    {1.0f, 1.0f, 1.0f, 1.0f}, // Diffuse
    {1.0f, 1.0f, 1.0f, 1.0f}, // Specular (.w used as shininess in the shader)
    {0.0f, 0.0f, 0.0f, 0.0f}, // Emission
};

static int mikupan_material_dirty = 1;

int MikuPan_IsShadowPassActive(void)
{
    return g_shadow_pass_active;
}

static inline void MikuPan_StreamUploadFull(GLenum target, GLuint buffer,
                                            GLsizeiptr size, const void *data)
{
    if (size <= 0)
    {
        return;
    }

    Uint64 t0 = MikuPan_PerfBegin();

    MikuPan_BindBufferCached(target, buffer);

    void *ptr = glad_glMapBufferRange(target, 0, size, GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT);

    if (ptr != NULL)
    {
        memcpy(ptr, data, (size_t)size);
        glad_glUnmapBuffer(target);
    }
    else
    {
        glad_glBufferSubData(target, 0, size, data);
    }

    MikuPan_PerfEnd(PERF_SECT_BUFFER_UPLOAD, t0);
}

SDL_AppResult MikuPan_Init()
{
    MikuPan_SetupOpenGLContext();
    SDL_SetAppMetadata("MikuPan", "1.0", "MikuPan");

    info_log("Initializing SDL");

    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD | SDL_INIT_JOYSTICK | SDL_INIT_HAPTIC))
    {
        info_log("Couldn't initialize SDL: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    SDL_SetHint(SDL_HINT_MAIN_CALLBACK_RATE, "60");

    MikuPan_SetupOpenGLContext();

    info_log("Loading SDL_GameControllerDB");

    if (!SDL_AddGamepadMappingsFromFile("./resources/gamecontrollerdb.txt"))
    {
        info_log("Error adding the controller mapping, bindings might be wrong %s", SDL_GetError());
    }

    info_log("Creating SDL Window");

    SDL_DisplayID primary = SDL_GetPrimaryDisplay();
    const SDL_DisplayMode *mode = SDL_GetCurrentDisplayMode(primary);

    int desired_window_width = mikupan_configuration.renderer.window.width;
    int desired_window_height = mikupan_configuration.renderer.window.height;

    if (desired_window_width <= 0 || desired_window_width > mode->w)
    {
        desired_window_width = mode->w;
        mikupan_configuration.renderer.window.width = desired_window_width;
    }

    if (desired_window_height <= 0 || desired_window_height > mode->h)
    {
        desired_window_height = mode->h;
        mikupan_configuration.renderer.window.height = desired_window_height;
    }

    mikupan_render.window = SDL_CreateWindow(
        "MikuPan",
        desired_window_width,
        desired_window_height,
        SDL_WINDOW_RESIZABLE | SDL_WINDOW_OPENGL
        );

    if (mikupan_render.window == NULL)
    {
        info_log("Error creating the SDL window %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    SDL_GetWindowSize(mikupan_render.window, &mikupan_render.width, &mikupan_render.height);

    SDL_Surface* iconSurface = SDL_LoadPNG("resources/mikupan.png");
    if (!SDL_SetWindowIcon(mikupan_render.window, iconSurface))
    {
        info_log("Error creating the icon %s", SDL_GetError());
    }

    if (iconSurface != NULL)
    {
        SDL_DestroySurface(iconSurface);
    }

    info_log("Creating OpenGL Context");

    SDL_GLContext gl_context = SDL_GL_CreateContext(mikupan_render.window);

    if (gl_context == NULL)
    {
        info_log("Error creating the OpenGL context %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    SDL_GL_MakeCurrent(mikupan_render.window, gl_context);

    info_log("GLad version loaded %d", gladLoadGLLoader((void*)SDL_GL_GetProcAddress));

    /* Disable vsync */
    if (!SDL_GL_SetSwapInterval(0))
    {
        info_log("Failed to disable GL vsync: %s", SDL_GetError());
    }

    int desired_render_width = mikupan_configuration.renderer.render.width;
    int desired_render_height = mikupan_configuration.renderer.render.height;
    int desired_msaa = mikupan_configuration.renderer.msaa_index;

    const int msaa_list[] = {0, 2, 4, 8, 16, 32};

    if (desired_msaa < 0 || desired_msaa > 5)
    {
        desired_msaa = 4;
        mikupan_configuration.renderer.msaa_index = desired_msaa;
    }

    if (desired_render_width <= 0 || desired_render_width > mode->w)
    {
        desired_render_width = mode->w;
        mikupan_configuration.renderer.render.width = desired_render_width;
    }

    if (desired_render_height <= 0 || desired_render_height > mode->h)
    {
        desired_render_height = mode->h;
        mikupan_configuration.renderer.render.height = desired_render_height;
    }

    MikuPan_InitUi(mikupan_render.window, gl_context);
    MikuPan_CreateInternalBuffer(desired_render_width, desired_render_height, msaa_list[desired_msaa]);
    MikuPan_InitShaders();
    MikuPan_InitPipeline();
    MikuPan_MeshCache_Init();

    for (int i = 0; i < MAX_SHADER_PROGRAMS; i++)
    {
        u_int program = MikuPan_SetCurrentShaderProgram(i);

        u_int lightIdx = glad_glGetUniformBlockIndex(program, "LightBlock");
        if (lightIdx != GL_INVALID_INDEX)
        {
            glad_glUniformBlockBinding(program, lightIdx, LightBlock);
        }

        u_int materialIdx = glad_glGetUniformBlockIndex(program, "MaterialBlock");
        if (materialIdx != GL_INVALID_INDEX)
        {
            glad_glUniformBlockBinding(program, materialIdx, MaterialBlock);
        }
    }

    MikuPan_Setup3D();

    return SDL_APP_CONTINUE;
}

void MikuPan_DestroyInternalBuffer()
{
    if (render_back_msaa.colour.id)
    {
        glad_glDeleteRenderbuffers(1, &render_back_msaa.colour.id);
        render_back_msaa.colour.id = 0;
    }

    if (render_back_msaa.depth.id)
    {
        glad_glDeleteRenderbuffers(1, &render_back_msaa.depth.id);
        render_back_msaa.depth.id = 0;
    }

    if (render_back_msaa.framebuffer_readback.id)
    {
        glad_glDeleteFramebuffers(1, &render_back_msaa.framebuffer_readback.id);
        render_back_msaa.framebuffer_readback.id = 0;
    }

    if (render_back_msaa.texture.id)
    {
        glad_glDeleteTextures(1, &render_back_msaa.texture.id);
        render_back_msaa.texture.id = 0;
    }

    if (render_back_msaa.framebuffer.id)
    {
        glad_glDeleteFramebuffers(1, &render_back_msaa.framebuffer.id);
        render_back_msaa.framebuffer.id = 0;
    }
}

static void MikuPan_EnsureShadowFbo(void)
{
    if (g_shadow_init)
    {
        return;
    }

    glad_glGenTextures(1, &g_shadow_tex);
    MikuPan_BindTexture2DCached(g_shadow_tex);
    glad_glTexImage2D(GL_TEXTURE_2D, 0, GL_R8,
                      SHADOW_FBO_SIZE, SHADOW_FBO_SIZE, 0,
                      GL_RED, GL_UNSIGNED_BYTE, NULL);
    glad_glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glad_glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glad_glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glad_glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    float border[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    glad_glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, border);

    glad_glGenFramebuffers(1, &g_shadow_fbo);
    glad_glBindFramebuffer(GL_FRAMEBUFFER, g_shadow_fbo);
    glad_glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                                GL_TEXTURE_2D, g_shadow_tex, 0);

    if (glad_glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
    {
        info_log("Shadow FBO incomplete!");
    }

    glad_glBindFramebuffer(GL_FRAMEBUFFER, 0);
    g_shadow_init = 1;
}

void MikuPan_BeginShadowPass(float *world_clip_view)
{
    MikuPan_EnsureShadowFbo();

    // Snapshot the previous render target + viewport so EndShadowPass can
    // hand the renderer back to the main scene pass without it noticing.
    glad_glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &g_shadow_saved_fbo);
    glad_glGetIntegerv(GL_VIEWPORT, g_shadow_saved_viewport);

    glad_glBindFramebuffer(GL_FRAMEBUFFER, g_shadow_fbo);
    MikuPan_SetViewportCached(0, 0, SHADOW_FBO_SIZE, SHADOW_FBO_SIZE);

    glad_glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glad_glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    if (world_clip_view != NULL)
    {
        memcpy(g_shadow_world_clip_view, world_clip_view, sizeof(g_shadow_world_clip_view));
        g_shadow_matrix_valid = 1;
    }

    // Override every shader bind for the duration of the pass — the mesh
    // renderers call MikuPan_SetCurrentShaderProgram(MESH_*_SHADER) and the
    // override silently routes them to SHADOW_SILHOUETTE_SHADER. Cleared in
    // MikuPan_EndShadowPass so subsequent main-pass draws are unaffected.
    MikuPan_SetShaderOverride(SHADOW_SILHOUETTE_SHADER);
    g_shadow_pass_active = 1;
}

void MikuPan_DrawShadowSilhouetteEllipse(void)
{
    // Reuse the fullscreen sprite VAO — the same one the postprocess pass
    // uses. The shadow caster shader ignores the vertex attributes (all
    // computation is done from gl_FragCoord) so any layout works as long as
    // we issue 4 vertices for a fullscreen TRIANGLE_STRIP.
    MikuPan_PipelineInfo *pi = MikuPan_GetPipelineInfo(UV4_COLOUR4_POSITION4);
    if (pi == NULL) return;

    MikuPan_BindVAO(pi->vao);
    MikuPan_SetCurrentShaderProgram(SHADOW_BLOB_SHADER);

    GLuint prog = MikuPan_GetCurrentShaderProgram();
    GLint sz_loc = glad_glGetUniformLocation(prog, "uShadowSize");
    GLint dk_loc = glad_glGetUniformLocation(prog, "uShadowDarkness");
    if (sz_loc >= 0) glad_glUniform2f(sz_loc, (float)SHADOW_FBO_SIZE, (float)SHADOW_FBO_SIZE);
    if (dk_loc >= 0) glad_glUniform1f(dk_loc, 0.6f);

    // Need additive/normal blending so the ellipse alpha isn't clobbered.
    glad_glDisable(GL_DEPTH_TEST);
    glad_glEnable(GL_BLEND);
    glad_glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    MikuPan_TimedDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    glad_glEnable(GL_DEPTH_TEST);
    // The render-state cache is now out of sync with the GL we just toggled;
    // force the scene pass to reissue depth/blend on its next state change.
    MikuPan_ResetRenderStateCache();
}

void MikuPan_EndShadowPass(void)
{
    // Drop the override + active flag *before* we start pushing scene-side
    // uniforms below — those uniform pushes go through SetCurrentShaderProgram
    // internally and would otherwise route into the silhouette shader.
    MikuPan_SetShaderOverride(-1);
    g_shadow_pass_active = 0;

    glad_glBindFramebuffer(GL_FRAMEBUFFER, (GLuint)g_shadow_saved_fbo);
    MikuPan_SetViewportCached(g_shadow_saved_viewport[0],
                              g_shadow_saved_viewport[1],
                              g_shadow_saved_viewport[2],
                              g_shadow_saved_viewport[3]);

    // The clear colour above clobbered the renderer's; restore the value the
    // scene clear path expects. The actual main-pass clear runs next frame so
    // a single push here is enough.
    glad_glClearColor(0.0f, 0.0f, 0.0f, 1.0f);

    // Push shadow uniforms to every shader that declares them. The mesh
    // fragment shader gates on uShadowEnabled and ignores uShadowTex /
    // uShadowMatrix when the toggle is off — but the texture unit must be
    // bound either way so a stale binding from a previous frame can't be
    // sampled. Texture unit 1 is reserved for the shadow map; unit 0 stays
    // the diffuse path so uTexture continues to work without changes.
    int enabled = (g_shadow_enabled && g_shadow_matrix_valid) ? 1 : 0;
    MikuPan_SetUniform1iToAllShaders(enabled,                                          "uShadowEnabled");
    MikuPan_SetUniform1iToAllShaders(1,                                                "uShadowTex");
    MikuPan_SetUniform1fToAllShaders(0.6f,                                             "uShadowStrength");
    MikuPan_SetUniformMatrix4fvToAllShaders((float *)g_shadow_world_clip_view,         "uShadowMatrix");

    MikuPan_ActiveTextureCached(GL_TEXTURE1);
    glad_glBindTexture(GL_TEXTURE_2D, g_shadow_tex);
    MikuPan_ActiveTextureCached(GL_TEXTURE0);
}

unsigned int MikuPan_GetShadowTexture(void)
{
    return g_shadow_tex;
}

float* MikuPan_GetShadowMatrix(void)
{
    return g_shadow_matrix_valid ? (float *)g_shadow_world_clip_view : NULL;
}

int MikuPan_IsShadowEnabled(void)
{
    return g_shadow_enabled;
}

void MikuPan_SetShadowEnabled(int enabled)
{
    g_shadow_enabled = 0;
    g_shadow_enabled = enabled ? 1 : 0;
}

int MikuPan_ReadFramebufferRGBA8TopLeft(int width, int height, unsigned char *out_rgba)
{
    if (width <= 0 || height <= 0 || out_rgba == NULL)
    {
        return 0;
    }

    // Save caller's FBO bindings — we briefly rebind to do the resolve and
    // readback, and the rest of the frame must see the same state on return.
    GLint prev_read_fbo = 0, prev_draw_fbo = 0;
    glad_glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &prev_read_fbo);
    glad_glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &prev_draw_fbo);

    // Source = the caller's draw FBO (the MSAA scene buffer mid-frame).
    // Query the colour attachment's dimensions so we know what rectangle to
    // blit — the internal render resolution is user-configurable so we
    // can't just use a fixed value.
    GLint src_w = width, src_h = height;
    glad_glBindFramebuffer(GL_READ_FRAMEBUFFER, (GLuint)prev_draw_fbo);
    GLint attach_type = 0;
    glad_glGetFramebufferAttachmentParameteriv(
        GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
        GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE, &attach_type);
    if (attach_type == GL_RENDERBUFFER)
    {
        GLint rb = 0;
        glad_glGetFramebufferAttachmentParameteriv(
            GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
            GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME, &rb);
        glad_glBindRenderbuffer(GL_RENDERBUFFER, (GLuint)rb);
        glad_glGetRenderbufferParameteriv(GL_RENDERBUFFER, GL_RENDERBUFFER_WIDTH,  &src_w);
        glad_glGetRenderbufferParameteriv(GL_RENDERBUFFER, GL_RENDERBUFFER_HEIGHT, &src_h);
        glad_glBindRenderbuffer(GL_RENDERBUFFER, 0);
    }
    else if (attach_type == GL_TEXTURE)
    {
        GLint tex = 0;
        glad_glGetFramebufferAttachmentParameteriv(
            GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
            GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME, &tex);
        MikuPan_BindTexture2DCached((GLuint)tex);
        glad_glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH,  &src_w);
        glad_glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_HEIGHT, &src_h);
    }
    else
    {
        // No usable colour attachment — bail without touching out_rgba so
        // the caller can detect the failure and skip its upload.
        glad_glBindFramebuffer(GL_READ_FRAMEBUFFER, (GLuint)prev_read_fbo);
        return 0;
    }

    // Temp non-MSAA receive surface at the requested PS2 native size: GL
    // resolves the multisamples and downsamples in one blit, then a single
    // glReadPixels gives us linear RGBA8.
    GLuint tmp_tex = 0, tmp_fbo = 0;
    glad_glGenTextures(1, &tmp_tex);
    MikuPan_BindTexture2DCached(tmp_tex);
    glad_glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0,
                      GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    glad_glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glad_glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    glad_glGenFramebuffers(1, &tmp_fbo);
    glad_glBindFramebuffer(GL_DRAW_FRAMEBUFFER, tmp_fbo);
    glad_glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                                GL_TEXTURE_2D, tmp_tex, 0);

    // Linear filter — the captured freeze frame would look aliased
    // otherwise when the internal buffer is much larger than 640×224.
    glad_glBlitFramebuffer(0, 0, src_w, src_h,
                           0, 0, width, height,
                           GL_COLOR_BUFFER_BIT, GL_LINEAR);

    glad_glBindFramebuffer(GL_READ_FRAMEBUFFER, tmp_fbo);
    glad_glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glad_glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, out_rgba);

    // GL framebuffer origin is bottom-left; PS2 GS framebuffer origin is
    // top-left. Flip rows in place so the caller can hand the buffer
    // straight to UploadPSMCT32 without doing the swap themselves.
    {
        const int row_bytes = width * 4;
        // Stack scratch is fine — row_bytes for a 640-wide buffer is 2560,
        // well within typical stack limits and avoids the malloc cost on a
        // function that may run multiple times per second during a freeze.
        unsigned char swap_row[4096];
        unsigned char *swap_ptr = (row_bytes <= (int)sizeof(swap_row)) ? swap_row : NULL;
        unsigned char *heap_row = NULL;
        if (swap_ptr == NULL)
        {
            heap_row = (unsigned char *)malloc((size_t)row_bytes);
            swap_ptr = heap_row;
        }
        if (swap_ptr != NULL)
        {
            for (int y = 0; y < height / 2; ++y)
            {
                unsigned char *a = out_rgba + (size_t)y * (size_t)row_bytes;
                unsigned char *b = out_rgba + (size_t)(height - 1 - y) * (size_t)row_bytes;
                memcpy(swap_ptr, a, (size_t)row_bytes);
                memcpy(a, b, (size_t)row_bytes);
                memcpy(b, swap_ptr, (size_t)row_bytes);
            }
        }
        if (heap_row != NULL) free(heap_row);
    }

    // Restore caller's FBO state and clean up temp resources.
    glad_glBindFramebuffer(GL_READ_FRAMEBUFFER, (GLuint)prev_read_fbo);
    glad_glBindFramebuffer(GL_DRAW_FRAMEBUFFER, (GLuint)prev_draw_fbo);
    glad_glDeleteFramebuffers(1, &tmp_fbo);
    glad_glDeleteTextures(1, &tmp_tex);
    return 1;
}

void MikuPan_SetupOpenGLContext()
{
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
#ifndef NDEBUG
    // Debug GL contexts route every call through driver-side validation and
    // can add 30–80% overhead per call. Only request one in debug builds —
    // release builds get a regular context for full performance.
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_DEBUG_FLAG);
#endif
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_FRAMEBUFFER_SRGB_CAPABLE, 0);
}

void MikuPan_CreateInternalBuffer(int w, int h, int msaa)
{
    render_back_msaa.texture.width = w;
    render_back_msaa.texture.height = h;
    render_back_msaa.msaa = msaa;

    glad_glGenFramebuffers(1, &render_back_msaa.framebuffer.id);
    glad_glBindFramebuffer(GL_FRAMEBUFFER, render_back_msaa.framebuffer.id);

    glad_glGenTextures(1, &render_back_msaa.texture.id);
    MikuPan_BindTexture2DCached(render_back_msaa.texture.id);

    glad_glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8,
                      render_back_msaa.texture.width, render_back_msaa.texture.height, 0,
                      GL_RGBA, GL_UNSIGNED_BYTE, NULL);

    glad_glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glad_glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glad_glFramebufferTexture2D(GL_FRAMEBUFFER,
                                GL_COLOR_ATTACHMENT0,
                                GL_TEXTURE_2D,
                                render_back_msaa.texture.id, 0);

    if (glad_glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
    {
        info_log("Resolve FBO failed!");
    }

    glad_glGenFramebuffers(1, &render_back_msaa.framebuffer_readback.id);
    glad_glBindFramebuffer(GL_FRAMEBUFFER, render_back_msaa.framebuffer_readback.id);

    glad_glGenRenderbuffers(1, &render_back_msaa.colour.id);
    glad_glBindRenderbuffer(GL_RENDERBUFFER, render_back_msaa.colour.id);

    glad_glRenderbufferStorageMultisample(
        GL_RENDERBUFFER,
       render_back_msaa.msaa,
        GL_RGBA8,
        render_back_msaa.texture.width, render_back_msaa.texture.height
    );

    glad_glFramebufferRenderbuffer(
        GL_FRAMEBUFFER,
        GL_COLOR_ATTACHMENT0,
        GL_RENDERBUFFER,
        render_back_msaa.colour.id
    );

    glad_glGenRenderbuffers(1, &render_back_msaa.depth.id);
    glad_glBindRenderbuffer(GL_RENDERBUFFER, render_back_msaa.depth.id);

    glad_glRenderbufferStorageMultisample(
        GL_RENDERBUFFER,
        render_back_msaa.msaa,
        GL_DEPTH24_STENCIL8,
        render_back_msaa.texture.width, render_back_msaa.texture.height
    );

    glad_glFramebufferRenderbuffer(
        GL_FRAMEBUFFER,
        GL_DEPTH_STENCIL_ATTACHMENT,
        GL_RENDERBUFFER,
        render_back_msaa.depth.id
    );

    if (glad_glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
    {
        info_log("MSAA FBO failed!");
    }

    glad_glBindFramebuffer(GL_FRAMEBUFFER, 0);
    MikuPan_SetViewportCached(0, 0, render_back_msaa.texture.width, render_back_msaa.texture.height);
}

void MikuPan_Clear()
{
    // Start the per-frame CPU/GPU timing window — we measure from here through
    // the moment right before SDL_GL_SwapWindow, which captures all of our
    // command-submission work.
    MikuPan_PerfBeginFrame();

    // Roll the per-frame GS instrumentation counters: the values currently in
    // g_gs_frame_metrics become "last frame" for the perf graph to read, and
    // the live counters reset to zero for the new frame.
    MikuPan_GsResetFrameMetrics();
    MikuPan_PerfResetFrame();

    // ImGui (and other GL-touching code outside our wrappers) leaves the
    // shader/texture/render-state caches potentially stale at the end of the
    // previous frame. Force-invalidate them so the first call into our state
    // setters this frame actually executes the underlying GL calls.
    MikuPan_ResetShaderCache();
    MikuPan_ResetRenderStateCache();
    MikuPan_ResetGLBindCache();

    MikuPan_RenderSetDebugValues();

    int curr_render_width = MikuPan_GetRenderResolutionWidth();
    int curr_render_height = MikuPan_GetRenderResolutionHeight();
    int curr_msaa = MikuPan_GetMSAA();

    if (render_back_msaa.texture.width != curr_render_width || render_back_msaa.texture.height != curr_render_height || render_back_msaa.msaa != curr_msaa)
    {
        MikuPan_DestroyInternalBuffer();
        MikuPan_CreateInternalBuffer(curr_render_width, curr_render_height, curr_msaa);
    }

    glad_glBindFramebuffer(GL_FRAMEBUFFER, render_back_msaa.framebuffer_readback.id);

    MikuPan_SetViewportCached(0, 0, render_back_msaa.texture.width, render_back_msaa.texture.height);

    glad_glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
}

void MikuPan_EndFrame()
{
    glad_glBindFramebuffer(GL_READ_FRAMEBUFFER, render_back_msaa.framebuffer_readback.id);
    glad_glBindFramebuffer(GL_DRAW_FRAMEBUFFER, render_back_msaa.framebuffer.id);

    glad_glBlitFramebuffer(
        0, 0, render_back_msaa.texture.width, render_back_msaa.texture.height,
        0, 0, render_back_msaa.texture.width, render_back_msaa.texture.height,
        GL_COLOR_BUFFER_BIT,
        GL_LINEAR
    );

    glad_glBindFramebuffer(GL_FRAMEBUFFER, 0);
    SDL_GetWindowSize(mikupan_render.window, &mikupan_render.width, &mikupan_render.height);

    float quad[] = {
        0,0,0,0,   1,1,1,1,   -1,-1,0,1,
        1,0,0,0,   1,1,1,1,    1,-1,0,1,
        0,1,0,0,   1,1,1,1,   -1, 1,0,1,
        1,1,0,0,   1,1,1,1,    1, 1,0,1
    };

    int offset_output[4];

    MikuPan_ConvertScreenToNDCCoord(
        offset_output,
        (float)mikupan_render.width, (float)mikupan_render.height,
        (float)render_back_msaa.texture.width, (float)render_back_msaa.texture.height
        );

    MikuPan_SetViewportCached(
        offset_output[0], offset_output[1],
        offset_output[2], offset_output[3]
        );
    glad_glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    MikuPan_BindTexture2DCached(render_back_msaa.texture.id);

    MikuPan_SetRenderState2D();

    MikuPan_SetCurrentShaderProgram(POSTPROCESS_SHADER);
    MikuPan_SetUniform1fToCurrentShader(MikuPan_GetBrightness(), "uBrightness");
    MikuPan_SetUniform1fToCurrentShader(MikuPan_GetGamma(),      "uGamma");
    MikuPan_PipelineInfo* pipeline = MikuPan_GetPipelineInfo(UV4_COLOUR4_POSITION4);
    MikuPan_BindVAO(pipeline->vao);

    MikuPan_StreamUploadFull(GL_ARRAY_BUFFER, pipeline->buffers[0].id,
                             (GLsizeiptr)sizeof(quad), quad);

    glad_glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    // Capture the post-processed scene before the debug UI is composited so
    // screenshots show the game without ImGui overlays.
    {
        int gl_w = 0, gl_h = 0;
        SDL_GetWindowSizeInPixels(mikupan_render.window, &gl_w, &gl_h);
        MikuPan_ScreenshotCaptureIfRequested(gl_w, gl_h);
    }

    //info_log("Frame: state_changes=%d, draw_calls=%d, mesh_cache hits=%d misses_new=%d misses_full=%d",
    //    MikuPan_PerfGetStateChangesCurrent(),
    //    MikuPan_PerfGetDrawCallsCurrent(),
    //    MikuPan_PerfGetMeshCacheHitsCurrent(),
    //    MikuPan_PerfGetMeshCacheMissesNewCurrent(),
    //    MikuPan_PerfGetMeshCacheMissesFullCurrent());

    {
        Uint64 _t0 = MikuPan_PerfBegin();
        MikuPan_DrawUi();

        MikuPan_PerfEnd(PERF_SECT_DRAWUI, _t0);
    }

    MikuPan_FlushTexturedSpriteBatch();

    {
        Uint64 _t0 = MikuPan_PerfBegin();
        MikuPan_RenderUi();
        MikuPan_PerfEnd(PERF_SECT_RENDERUI, _t0);
    }

    MikuPan_PerfEndFrame();

    SDL_GL_SwapWindow(mikupan_render.window);
}

void MikuPan_UpdateWindowSize(int width, int height)
{
    mikupan_render.width = width;
    mikupan_render.height = height;
}

int MikuPan_GetWindowWidth()
{
    return render_back_msaa.texture.width;
}

int MikuPan_GetWindowHeight()
{
    return render_back_msaa.texture.height;
}

int MikuPan_GetRenderMode()
{
    return MikuPan_IsWireframeRendering() ? GL_LINE_STRIP : GL_TRIANGLE_STRIP;
}

static int MikuPan_GetMeshRenderMode()
{
    return MikuPan_IsWireframeRendering() ? GL_LINES : GL_TRIANGLES;
}

void MikuPan_SetupAmbientLighting(const LIGHT_PACK *lp, float *eyevec)
{
#define MAX_LIGHTS 3
    // Populate the entire LightBlock UBO straight from the LIGHT_PACK input.
    // The previous design relied on MikuPan_SetupAmbientLighting2() reading
    // colors from SgLightParallelp, but that pointer is updated per-material
    // by SetMaterialData (sglight.c:473) and is stale/zero at the SetLightData
    // call site — which is why the scene was so dark: the shader was getting
    // empty diffuse colors. Source the canonical values directly here.

    // Scene ambient.
    mikupan_light_data.uAmbient[0] = lp->ambient[0];
    mikupan_light_data.uAmbient[1] = lp->ambient[1];
    mikupan_light_data.uAmbient[2] = lp->ambient[2];
    mikupan_light_data.uAmbient[3] = lp->ambient[3];

    mat3 view3;
    glm_mat4_pick3(WorldView, view3);

    // Parallel (directional) lights. Compute the same Blinn-Phong halfway
    // vector that SgSetDefaultLight (sglight.c:118-120) bakes into SLightMtx:
    //     halfway = normalize(ieye + dir)   where ieye = -normalize(eye)
    // Both vectors live in view space here so we don't need an `eye` parameter
    // — the camera sits at the origin in view space, so ieye is just (0,0,1).
    const vec3 ieye_vs = {eyevec[0], eyevec[1], eyevec[2]};

    int parCount =
        lp->parallel_num > MAX_LIGHTS ? MAX_LIGHTS : lp->parallel_num;
    mikupan_light_data.uParCount[0] = parCount;

    for (int i = 0; i < parCount; i++)
    {
        // Direction is in world space; rotate (no translation) into view space
        // so the shader can dot it against view-space normals directly.
        vec4 dirWS = {
            lp->parallel[i].direction[0], lp->parallel[i].direction[1],
            lp->parallel[i].direction[2], lp->parallel[i].direction[3]};
        vec4 dirVS;
        glm_mat3_mulv(view3, dirWS, dirVS);
        vec3 dirVSn = {dirVS[0], dirVS[1], dirVS[2]};
        glm_vec3_normalize(dirVSn);

        mikupan_light_data.uParDir[i][0] = dirVSn[0];
        mikupan_light_data.uParDir[i][1] = dirVSn[1];
        mikupan_light_data.uParDir[i][2] = dirVSn[2];
        mikupan_light_data.uParDir[i][3] = 1.0f;

        mikupan_light_data.uParDiffuse[i][0] = lp->parallel[i].diffuse[0];
        mikupan_light_data.uParDiffuse[i][1] = lp->parallel[i].diffuse[1];
        mikupan_light_data.uParDiffuse[i][2] = lp->parallel[i].diffuse[2];
        mikupan_light_data.uParDiffuse[i][3] = lp->parallel[i].diffuse[3];

        // light_pack carries no separate specular field for parallel lights;
        // gra3d.c:1083 mirrors specular = diffuse on the SgLIGHT side, so
        // match that convention here.
        mikupan_light_data.uParSpecular[i][0] = lp->parallel[i].diffuse[0];
        mikupan_light_data.uParSpecular[i][1] = lp->parallel[i].diffuse[1];
        mikupan_light_data.uParSpecular[i][2] = lp->parallel[i].diffuse[2];
        mikupan_light_data.uParSpecular[i][3] = lp->parallel[i].diffuse[3];

        // Halfway vector for Blinn-Phong — matches sglight.c:118-120 exactly.
        vec3 halfway = {ieye_vs[0] + dirVSn[0], ieye_vs[1] + dirVSn[1],
                        ieye_vs[2] + dirVSn[2]};
        glm_vec3_normalize(halfway);
        mikupan_light_data.uParHalfway[i][0] = halfway[0];
        mikupan_light_data.uParHalfway[i][1] = halfway[1];
        mikupan_light_data.uParHalfway[i][2] = halfway[2];
        mikupan_light_data.uParHalfway[i][3] = 1.0f;
    }

    // Point lights.
    int pointCount = lp->point_num > MAX_LIGHTS ? MAX_LIGHTS : lp->point_num;
    mikupan_light_data.uPointCount[0] = pointCount;

    for (int i = 0; i < pointCount; i++)
    {
        vec4 posWS = {lp->point[i].pos[0], lp->point[i].pos[1],
                      lp->point[i].pos[2], lp->point[i].pos[3]};
        vec4 posVS;
        glm_mat4_mulv(WorldView, posWS, posVS);

        mikupan_light_data.uPointPos[i][0] = posVS[0];
        mikupan_light_data.uPointPos[i][1] = posVS[1];
        mikupan_light_data.uPointPos[i][2] = posVS[2];
        mikupan_light_data.uPointPos[i][3] = 1.0f;

        mikupan_light_data.uPointDiffuse[i][0] = lp->point[i].diffuse[0];
        mikupan_light_data.uPointDiffuse[i][1] = lp->point[i].diffuse[1];
        mikupan_light_data.uPointDiffuse[i][2] = lp->point[i].diffuse[2];
        mikupan_light_data.uPointDiffuse[i][3] = lp->point[i].diffuse[3];

        // Specular = diffuse, mirroring gra3d.c:1101.
        mikupan_light_data.uPointSpecular[i][0] = lp->point[i].diffuse[0];
        mikupan_light_data.uPointSpecular[i][1] = lp->point[i].diffuse[1];
        mikupan_light_data.uPointSpecular[i][2] = lp->point[i].diffuse[2];
        mikupan_light_data.uPointSpecular[i][3] = lp->point[i].diffuse[3];

        mikupan_light_data.uPointPower[i][0] = lp->point[i].power;
    }

    // Spot lights.
    int spotCount = lp->spot_num > MAX_LIGHTS ? MAX_LIGHTS : lp->spot_num;
    mikupan_light_data.uSpotCount[0] = spotCount;

    for (int i = 0; i < spotCount; i++)
    {
        vec4 posWS = {lp->spot[i].pos[0], lp->spot[i].pos[1],
                      lp->spot[i].pos[2], lp->spot[i].pos[3]};
        vec4 posVS;
        glm_mat4_mulv(WorldView, posWS, posVS);

        mikupan_light_data.uSpotPos[i][0] = posVS[0];
        mikupan_light_data.uSpotPos[i][1] = posVS[1];
        mikupan_light_data.uSpotPos[i][2] = posVS[2];
        mikupan_light_data.uSpotPos[i][3] = 1.0f;

        vec4 dirWS = {lp->spot[i].direction[0], lp->spot[i].direction[1],
                      lp->spot[i].direction[2], lp->spot[i].direction[3]};
        vec4 dirVS;
        glm_mat3_mulv(view3, dirWS, dirVS);
        glm_vec3_normalize(dirVS);

        mikupan_light_data.uSpotDir[i][0] = dirVS[0];
        mikupan_light_data.uSpotDir[i][1] = dirVS[1];
        mikupan_light_data.uSpotDir[i][2] = dirVS[2];
        mikupan_light_data.uSpotDir[i][3] = 1.0f;

        mikupan_light_data.uSpotDiffuse[i][0] = lp->spot[i].diffuse[0];
        mikupan_light_data.uSpotDiffuse[i][1] = lp->spot[i].diffuse[1];
        mikupan_light_data.uSpotDiffuse[i][2] = lp->spot[i].diffuse[2];
        mikupan_light_data.uSpotDiffuse[i][3] = lp->spot[i].diffuse[3];

        // Specular = diffuse, mirroring gra3d.c:1122.
        mikupan_light_data.uSpotSpecular[i][0] = lp->spot[i].diffuse[0];
        mikupan_light_data.uSpotSpecular[i][1] = lp->spot[i].diffuse[1];
        mikupan_light_data.uSpotSpecular[i][2] = lp->spot[i].diffuse[2];
        mikupan_light_data.uSpotSpecular[i][3] = lp->spot[i].diffuse[3];

        mikupan_light_data.uSpotPower[i][0] = lp->spot[i].power;//1250

        // Cone gate parameters. Match SgSetSpotLights (sglight.c:914):
        //   intens   = cos²(half-angle), the inner-cone threshold
        //   intens_b = 1/(1 - intens), the per-spot reciprocal used by
        //              asm_CalcSpotLight (sglight.c:1125-1127) to normalize
        //              cone² ∈ [intens..1] back to a 0..1 falloff ramp.
        // intens == 1.0 is degenerate (zero-width cone) — clamp away from it
        // so we don't divide by zero. The shader ignores the spot anyway when
        // cone² <= intens, so the clamp is purely a numerical guard.
        float intens = lp->spot[i].intens;
        float intens_b = 0.0f;
        if (intens < 0.9999f)
        {
            intens_b = 1.0f / (1.0f - intens);
        }
        mikupan_light_data.uSpotIntens[i][0] = intens;
        mikupan_light_data.uSpotIntens[i][1] = intens_b;
    }

    // The LightBlock UBO is bound to indexed binding point 0 via
    // glBindBufferBase at init, but glBufferSubData(GL_UNIFORM_BUFFER, ...)
    // reads from the *generic* GL_UNIFORM_BUFFER target — which was left at
    // 0 (unbound) after pipeline init. Without this bind the upload was a
    // silent no-op and the shader saw zeros for diffuse colors / counts,
    // leaving the scene effectively unlit.
    MikuPan_PipelineInfo *p = MikuPan_GetPipelineInfo(LIGHTING_DATA);
    glad_glBindBuffer(GL_UNIFORM_BUFFER, p->buffers[0].id);

    // Data was already populated by MikuPan_SetupAmbientLighting from the
    // canonical LIGHT_PACK; this just publishes the staged buffer to the GPU.
    glad_glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(MikuPan_LightData),
                         &mikupan_light_data);
}

void MikuPan_SetMaterial(const sceVu0FVECTOR *ambient,
                         const sceVu0FVECTOR *diffuse,
                         const sceVu0FVECTOR *specular,
                         const sceVu0FVECTOR *emission)
{
    memcpy(mikupan_material_data.uMatAmbient,  *ambient,  sizeof(float[4]));
    memcpy(mikupan_material_data.uMatDiffuse,  *diffuse,  sizeof(float[4]));
    memcpy(mikupan_material_data.uMatSpecular, *specular, sizeof(float[4]));
    memcpy(mikupan_material_data.uMatEmission, *emission, sizeof(float[4]));
    mikupan_material_dirty = 1;

    MikuPan_PipelineInfo *p = MikuPan_GetPipelineInfo(MATERIAL_DATA);
    glad_glBindBuffer(GL_UNIFORM_BUFFER, p->buffers[0].id);
    glad_glBufferSubData(GL_UNIFORM_BUFFER, 0,
                         sizeof(MikuPan_MaterialData),
                         &mikupan_material_data);
}

void MikuPan_RenderSetDebugValues()
{
    static int   last_render_normals   = -1;
    static float last_normal_length    = -1.0f;
    static int   last_disable_lighting = -1;
    static int   last_static_lighting  = -1;

    int   render_normals   = MikuPan_IsNormalsRendering();
    float normal_length    = MikuPan_GetNormalLength();
    int   disable_lighting = MikuPan_IsLightingDisabled();
    int   static_lighting  = MikuPan_ShowStaticLighting();

    if (render_normals != last_render_normals)
    {
        MikuPan_SetUniform1iToAllShaders(render_normals, "renderNormals");
        last_render_normals = render_normals;
    }

    if (normal_length != last_normal_length)
    {
        MikuPan_SetUniform1fToAllShaders(normal_length, "uNormalLength");
        last_normal_length = normal_length;
    }

    if (disable_lighting != last_disable_lighting)
    {
        MikuPan_SetUniform1iToAllShaders(disable_lighting, "disableLighting");
        last_disable_lighting = disable_lighting;
    }

    if (static_lighting != last_static_lighting)
    {
        MikuPan_SetUniform1iToAllShaders(static_lighting, "staticLighting");
        last_static_lighting = static_lighting;
    }
}

static GLfloat MikuPan_GetMaxAniso(void)
{
    if (g_max_aniso < 0.0f)
    {
        glad_glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &g_max_aniso);
    }

    return g_max_aniso;
}

MikuPan_TextureInfo *MikuPan_CreateGLTexture(sceGsTex0 *tex0)
{
    GLuint tex = 0;

    int width = 1 << tex0->TW;
    int height = 1 << tex0->TH;

    uint64_t hash = 0;
    void *pixels = MikuPan_GsDownloadTexture(tex0, &hash);

    if (hash == 0)
    {
        return NULL;
    }

    glad_glGenTextures(1, &tex);
    MikuPan_BindTexture2DCached(tex);
    glad_glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glad_glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8,
                      width, height, 0, GL_RGBA,
                      GL_UNSIGNED_BYTE, pixels);
    glad_glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, MikuPan_GetMaxAniso());
    glad_glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glad_glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glad_glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glad_glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glad_glGenerateMipmap(GL_TEXTURE_2D);
    MikuPan_BindTexture2DCached(0);

    MikuPan_TextureInfo *texture_info = malloc(sizeof(MikuPan_TextureInfo));
    texture_info->height = height;
    texture_info->width = width;
    texture_info->id = tex;
    texture_info->tex0 = *(uint64_t*)tex0;
    texture_info->hash = hash;

    MikuPan_AddTexture(hash, texture_info);

    free(pixels);

    return texture_info;
}

void MikuPan_SetTexture(sceGsTex0 *tex0)
{
    // L1: lookup by raw tex0 register value. Avoids the per-call XXH3 over
    // GS memory (which can run into many MB/frame for textured scenes).
    uint64_t tex0_value = *(uint64_t *)tex0;

    // Sub-section breakdown of PERF_SECT_SC_TEXTURE — each step times itself
    // separately so the perf graph can show whether the cost is the L1 hash
    // probe, the XXH3 over GS memory, the L2 lookup, an allocation/upload,
    // or just glBindTexture.
    Uint64 _t = MikuPan_PerfBegin();
    MikuPan_TextureInfo *texture_info = MikuPan_LookupTextureByTex0(tex0_value);
    MikuPan_PerfEnd(PERF_SECT_TEX_L1_LOOKUP, _t);

    if (texture_info == NULL)
    {
        MikuPan_PerfTexL1Miss();
        // L2: hash-based lookup. Same texture data may be referenced by
        // different tex0 values; the GS-memory hash unifies them.
        _t = MikuPan_PerfBegin();
        uint64_t hash = MikuPan_GetTextureHash(tex0);
        MikuPan_PerfEnd(PERF_SECT_TEX_HASH, _t);

        _t = MikuPan_PerfBegin();
        texture_info = MikuPan_GetTextureInfo(hash);
        MikuPan_PerfEnd(PERF_SECT_TEX_L2_LOOKUP, _t);

        if (texture_info == NULL)
        {
            _t = MikuPan_PerfBegin();
            texture_info = MikuPan_CreateGLTexture(tex0);
            MikuPan_PerfEnd(PERF_SECT_TEX_CREATE, _t);
        }

        if (texture_info != NULL)
        {
            MikuPan_RegisterTextureForTex0(tex0_value, tex0, texture_info);
        }
    }
    else
    {
        MikuPan_PerfTexL1Hit();
    }

    if (texture_info != NULL)
    {
        _t = MikuPan_PerfBegin();
        MikuPan_BindTexture2DCached(texture_info->id);
        MikuPan_PerfEnd(PERF_SECT_TEX_BIND, _t);
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
    MikuPan_FlushTexturedSpriteBatch();
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

    MikuPan_BindVAO(pipeline->vao);
    MikuPan_BindBufferCached(GL_ARRAY_BUFFER, pipeline->buffers[0].id);

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

    MikuPan_FlushTexturedSpriteBatch();

    float bounding_box_color[4] = {0.0f, 1.0f, 0.0f, 1.0f};

    MikuPan_SetCurrentShaderProgram(BOUNDING_BOX_SHADER);
    MikuPan_PipelineInfo* pipeline = MikuPan_GetPipelineInfo(POSITION4);
    MikuPan_SetRenderState3D();

    MikuPan_SetUniform4fvToCurrentShader(bounding_box_color, "uColor");

    MikuPan_BindVAO(pipeline->vao);
    MikuPan_BindBufferCached(GL_ARRAY_BUFFER, pipeline->buffers[0].id);
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

void MikuPan_FlushTexturedSpriteBatch(void)
{
    MIKUPAN_PERF_SCOPE(PERF_SECT_BATCH_FLUSH);

    if (!g_textured_sprite_batch.active || g_textured_sprite_batch.sprite_count == 0)
    {
        g_textured_sprite_batch.active = 0;
        g_textured_sprite_batch.sprite_count = 0;
        return;
    }

    MikuPan_SetCurrentShaderProgram(SPRITE_SHADER);
    MikuPan_PipelineInfo *pipeline = MikuPan_GetPipelineInfo(UV4_COLOUR4_POSITION4);
    MikuPan_BindVAO(pipeline->vao);
    MikuPan_SetRenderState2D();

    MikuPan_BindTexture2DCached(g_textured_sprite_batch.texture_id);

    int n_verts = g_textured_sprite_batch.sprite_count * 6;
    MikuPan_StreamUploadFull(GL_ARRAY_BUFFER, pipeline->buffers[0].id,
        (GLsizeiptr)(n_verts * 12 * sizeof(float)),
        g_textured_sprite_cpu_buf);

    int mode = MikuPan_IsWireframeRendering() ? GL_LINES : GL_TRIANGLES;
    MikuPan_TimedDrawArrays(mode, 0, n_verts);
    MikuPan_PerfDrawCall();

    g_textured_sprite_batch.active = 0;
    g_textured_sprite_batch.sprite_count = 0;
}

void MikuPan_RenderSprite(MikuPan_Rect src, MikuPan_Rect dst, u_char r,
                          u_char g, u_char b, u_char a,
                          MikuPan_TextureInfo *texture_info)
{
    MIKUPAN_PERF_SCOPE(PERF_SECT_SPRITE_RENDER);

    if (texture_info == NULL)
    {
        info_log("Cannot render texture due texture_info being NULL");
        return;
    }

    float ndc[4] = {0};
    MikuPan_ConvertPs2ScreenCoordToNDCMaintainAspectRatio(ndc, (float)MikuPan_GetWindowWidth(), (float)MikuPan_GetWindowHeight(), dst.x, dst.y);
    MikuPan_ConvertPs2ScreenCoordToNDCMaintainAspectRatio(&ndc[2], (float)MikuPan_GetWindowWidth(), (float)MikuPan_GetWindowHeight(), dst.x + src.w, dst.y + src.h);

    float texW = (float) (texture_info->width);
    float texH = (float) (texture_info->height);

    float halfU = 0.5f / texW;
    float halfV = 0.5f / texH;

    float u0 = (src.x / texW) + halfU;
    float v0 = (src.y / texH) + halfV;
    float u1 = ((src.x + dst.w) / texW) - halfU;
    float v1 = ((src.y + dst.h) / texH) - halfV;

    const float c0 = MikuPan_ConvertScaleColor(r);
    const float c1 = MikuPan_ConvertScaleColor(g);
    const float c2 = MikuPan_ConvertScaleColor(b);
    const float c3 = MikuPan_ConvertScaleColor(a);

    // Flush if the texture changes or batch is full.
    if (g_textured_sprite_batch.active &&
        (g_textured_sprite_batch.texture_id != texture_info->id ||
         g_textured_sprite_batch.sprite_count >= TEXTURED_SPRITE_BATCH_MAX))
    {
        MikuPan_FlushTexturedSpriteBatch();
    }

    if (!g_textured_sprite_batch.active)
    {
        g_textured_sprite_batch.active = 1;
        g_textured_sprite_batch.texture_id = texture_info->id;
        g_textured_sprite_batch.sprite_count = 0;
    }

    // Append 6 vertices (two triangles forming the quad: TL, BL, TR, TR, BL, BR).
    float *vp = g_textured_sprite_cpu_buf + g_textured_sprite_batch.sprite_count * 6 * 12;

    #define WRITE_VERT(idx, vu, vv, vx, vy) do { \
        float *p = vp + (idx) * 12; \
        p[0] = (vu); p[1] = (vv); p[2] = 0.0f; p[3] = 0.0f; \
        p[4] = c0;   p[5] = c1;   p[6] = c2;   p[7] = c3;   \
        p[8] = (vx); p[9] = (vy); p[10] = 0.0f; p[11] = 1.0f; \
    } while (0)

    WRITE_VERT(0, u0, v0, ndc[0], ndc[1]);  // TL
    WRITE_VERT(1, u0, v1, ndc[0], ndc[3]);  // BL
    WRITE_VERT(2, u1, v0, ndc[2], ndc[1]);  // TR
    WRITE_VERT(3, u1, v0, ndc[2], ndc[1]);  // TR
    WRITE_VERT(4, u0, v1, ndc[0], ndc[3]);  // BL
    WRITE_VERT(5, u1, v1, ndc[2], ndc[3]);  // BR

    #undef WRITE_VERT

    g_textured_sprite_batch.sprite_count++;
}

void MikuPan_RenderSprite2D(sceGsTex0 *tex, float *buffer)
{
    MIKUPAN_PERF_SCOPE(PERF_SECT_SPRITE_RENDER);
    MikuPan_FlushTexturedSpriteBatch();
    MikuPan_SetCurrentShaderProgram(SPRITE_SHADER);
    MikuPan_PipelineInfo* pipeline = MikuPan_GetPipelineInfo(UV4_COLOUR4_POSITION4);

    MikuPan_BindVAO(pipeline->vao);
    MikuPan_SetTexture(tex);

    MikuPan_SetRenderState2D();

    // Caller passes a 4-vert quad worth of data (UV4_COLOUR4_POSITION4 layout).
    MikuPan_BindBufferCached(GL_ARRAY_BUFFER, pipeline->buffers[0].id);
    glad_glBufferSubData(GL_ARRAY_BUFFER, 0, (GLsizeiptr)sizeof(float[4][12]), buffer);

    glad_glDrawArrays(MikuPan_GetRenderMode(), 0, 4);
}

void MikuPan_RenderUntexturedSprite(float *buffer)
{
    MIKUPAN_PERF_SCOPE(PERF_SECT_SPRITE_RENDER);
    MikuPan_FlushTexturedSpriteBatch();
    MikuPan_SetCurrentShaderProgram(UNTEXTURED_COLOURED_SPRITE_SHADER);
    MikuPan_PipelineInfo* pipeline = MikuPan_GetPipelineInfo(COLOUR4_POSITION4);

    MikuPan_BindVAO(pipeline->vao);

    MikuPan_SetRenderState2D();

    // Caller passes a 4-vert quad worth of data (COLOUR4_POSITION4 layout: 8 floats / vert).
    MikuPan_BindBufferCached(GL_ARRAY_BUFFER, pipeline->buffers[0].id);
    glad_glBufferSubData(GL_ARRAY_BUFFER, 0, (GLsizeiptr)sizeof(float[4][8]), buffer);

    glad_glDrawArrays(MikuPan_GetRenderMode(), 0, 4);
}

void MikuPan_RenderSprite3D(sceGsTex0 *tex, float* buffer)
{
    MIKUPAN_PERF_SCOPE(PERF_SECT_SPRITE_RENDER);
    MikuPan_FlushTexturedSpriteBatch();
    MikuPan_SetCurrentShaderProgram(SPRITE_SHADER);
    MikuPan_PipelineInfo* pipeline = MikuPan_GetPipelineInfo(UV4_COLOUR4_POSITION4);

    MikuPan_BindVAO(pipeline->vao);
    MikuPan_SetTexture(tex);

    MikuPan_SetRenderStateSprite3D();

    MikuPan_BindBufferCached(GL_ARRAY_BUFFER, pipeline->buffers[0].id);
    glad_glBufferSubData(GL_ARRAY_BUFFER, 0, (GLsizeiptr)sizeof(float[4][12]), buffer);

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

float* MikuPan_GetWorldClip()
{
    return (float*)&WorldView;
}

void MikuPan_SetWorldClipView()
{
    MikuPan_SetModelTransformMatrix(WorldClipView);
}

static void MikuPan_RecomputeAndUploadDerived(void)
{
    mat4 mv, mvp, vp;
    mat3 mv3, view3, normalMatrix3, viewNormalMatrix3;

    // mv = view * model
    if (g_has_cached_model)
    {
        glm_mat4_mul(WorldView, g_cached_model_matrix, mv);
    }
    else
    {
        glm_mat4_copy(WorldView, mv);
    }

    // mvp = projection * mv
    glm_mat4_mul(projection, mv, mvp);

    // vp = projection * view (used by shaders that don't apply model — e.g. 0xA)
    glm_mat4_mul(projection, WorldView, vp);

    // normalMatrix3 = transpose(inverse(mv_3x3))
    glm_mat4_pick3(mv, mv3);
    glm_mat3_inv(mv3, normalMatrix3);
    glm_mat3_transpose(normalMatrix3);

    // viewNormalMatrix3 = transpose(inverse(view_3x3))
    glm_mat4_pick3(WorldView, view3);
    glm_mat3_inv(view3, viewNormalMatrix3);
    glm_mat3_transpose(viewNormalMatrix3);

    MikuPan_SetUniformMatrix4fvToAllShaders((float*)mvp,               "mvp");
    MikuPan_SetUniformMatrix4fvToAllShaders((float*)mv,                "modelView");
    MikuPan_SetUniformMatrix4fvToAllShaders((float*)vp,                "viewProj");
    MikuPan_SetUniformMatrix3fvToAllShaders((float*)normalMatrix3,     "normalMatrix");
    MikuPan_SetUniformMatrix3fvToAllShaders((float*)viewNormalMatrix3, "viewNormalMatrix");
}

void MikuPan_SetModelTransformMatrix(sceVu0FVECTOR *m)
{
    if (g_has_cached_model && memcmp(g_cached_model_matrix, m, sizeof(g_cached_model_matrix)) == 0)
    {
        return;
    }

    memcpy(g_cached_model_matrix, m, sizeof(g_cached_model_matrix));
    g_has_cached_model = 1;

    MikuPan_PerfStateChange();

    MikuPan_SetUniformMatrix4fvToAllShaders((float*)m, "model");
    MikuPan_RecomputeAndUploadDerived();
}

void MikuPan_SetFontTexture(int fnt)
{
    curr_fnt_texture = fnt_texture[fnt];
}

void MikuPan_DeleteTexture(MikuPan_TextureInfo *texture_info)
{
    if (texture_info == NULL)
    {
        return;
    }

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
    for (int i = 0; i < 6; i++)
    {
        if (fnt_texture[i] != NULL)
        {
            glad_glDeleteTextures(1, (const GLuint *) &fnt_texture[i]->id);
            free(fnt_texture[i]);

            fnt_texture[i] = NULL;
        }
    }

    SDL_DestroyWindow(mikupan_render.window);
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
    glm_mat4_zero(projection);
    //glm_perspective(mikupan_camera->fov, (float)MikuPan_GetWindowWidth() / (float)MikuPan_GetWindowHeight(), 10.0f, mikupan_camera->farz, projection);
    float nearz = 10.0f;
    float farz  = mikupan_camera->farz;
    float halfW = (float) MikuPan_GetWindowWidth()  / 2.0f;
    float halfH = (float) MikuPan_GetWindowHeight() / 4.0f;
    float scrz = halfH / tanf(mikupan_camera->fov * 0.5f) * 2.0f;
    float rscrz = nearz / scrz;
    float gsxv = halfW * rscrz;
    float gsyv = halfH * rscrz;
    float l = -gsxv;
    float r =  gsxv;
    float b = -gsyv;
    float t =  gsyv;

    projection[0][0] = (2.0f * nearz) / (r - l);
    projection[1][1] = (2.0f * nearz) / (t - b);
    projection[0][2] = (r + l) / (r - l);
    projection[1][2] = (t + b) / (t - b);
    projection[2][2] = -(farz + nearz) / (farz - nearz);
    projection[2][3] = -1.0f;
    projection[3][2] = -(2.0f * farz * nearz) / (farz - nearz);
    projection[3][3] = 0.0f;
    projection[0][0] *= mikupan_camera->ax;
    projection[1][1] *= mikupan_camera->ay;

    glm_mat4_mul(projection, WorldView, WorldClipView);
    MikuPan_SetUniformMatrix4fvToAllShaders((float*)WorldView, "view");
    MikuPan_SetUniformMatrix4fvToAllShaders((float*)projection, "projection");

    // View / projection just changed — refresh the precomputed mvp/modelView/etc.
    MikuPan_RecomputeAndUploadDerived();

    nearz = mikupan_camera->nearz;
    float gs_width  = 4096.0f;
    float gs_height = 4096.0f;

    float scaleX = (float)MikuPan_GetWindowWidth()  / gs_width;
    float scaleY = (float)MikuPan_GetWindowHeight() / gs_height;

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

    mat4 mv, mvp, vp;
    mat3 mv3, view3, normalMatrix3, viewNormalMatrix3;

    if (g_has_cached_model)
    {
        glm_mat4_mul(out, g_cached_model_matrix, mv);
    }
    else
    {
        glm_mat4_copy(out, mv);
    }

    glm_mat4_mul(projection, mv, mvp);
    glm_mat4_mul(projection, out, vp);

    glm_mat4_pick3(mv, mv3);
    glm_mat3_inv(mv3, normalMatrix3);
    glm_mat3_transpose(normalMatrix3);

    glm_mat4_pick3(out, view3);
    glm_mat3_inv(view3, viewNormalMatrix3);
    glm_mat3_transpose(viewNormalMatrix3);

    MikuPan_SetUniformMatrix4fvToAllShaders((float*)mvp,               "mvp");
    MikuPan_SetUniformMatrix4fvToAllShaders((float*)mv,                "modelView");
    MikuPan_SetUniformMatrix4fvToAllShaders((float*)vp,                "viewProj");
    MikuPan_SetUniformMatrix3fvToAllShaders((float*)normalMatrix3,     "normalMatrix");
    MikuPan_SetUniformMatrix3fvToAllShaders((float*)viewNormalMatrix3, "viewNormalMatrix");
}

/// 0x10 -> Type0   -> [vertex.x, vertex.y, vertex.z, normal.x, normal.y, normal.z]
/// 0x12 -> Type2   ->
/// 0x32 -> Type2F  ->
/// @param pVUVN
/// @param pPUHead
void MikuPan_RenderMeshType0x32(SGDPROCUNITHEADER *pVUVN, SGDPROCUNITHEADER *pPUHead)
{
    MIKUPAN_PERF_SCOPE(PERF_SECT_MESH_RENDER);

    char mesh_type = GET_MESH_TYPE(pPUHead);

    if (pPUHead->VUMeshDesc.MeshType.TEX == 0)
    {
        return;
    }

    if (mesh_type != 0x12 && mesh_type != 0x10 && mesh_type != 0x32)
    {
        return;
    }

    if (!g_shadow_pass_active)
    {
        if (((mesh_type == 0x12) && !MikuPan_IsMesh0x12Rendering()) || ((mesh_type == 0x10) && !MikuPan_IsMesh0x10Rendering()) ||
            (mesh_type == 0x32 && !MikuPan_IsMesh0x32Rendering()))
        {
            return;
        }
    }

    int per_type_sect = (mesh_type == 0x32) ? PERF_SECT_MESH_0x32 :
                        (mesh_type == 0x12) ? PERF_SECT_MESH_0x12 :
                                              PERF_SECT_MESH_0x10;

    MIKUPAN_PERF_SCOPE(per_type_sect);

    SGDPROCUNITDATA *pProcData = (SGDPROCUNITDATA *) &pPUHead[1];
    SGDPROCUNITDATA *pVUVNData = (SGDPROCUNITDATA *) &pVUVN[1];

    sceGsTex0 *mesh_tex_reg = (sceGsTex0 *) ((int64_t) pProcData + 0x28);

    SGDVUMESHSTDATA *sgdMeshData = (SGDVUMESHSTDATA *)((int64_t) pProcData + (pProcData->VUMeshData_Preset.sOffsetToST - 1) * 4);
    _SGDVUMESHCOLORDATA *pVMCD = (_SGDVUMESHCOLORDATA*) (&pPUHead->pNext + pProcData->VUMeshData_Preset.sOffsetToPrim);

    int desired_pipeline = (mesh_type == 0x32) ? POSITION3_NORMAL3_UV2_SOA : POSITION3_NORMAL3_UV2;

    MikuPan_PipelineInfo *pipeline = MikuPan_GetPipelineInfo(desired_pipeline);

    MikuPan_FlushTexturedSpriteBatch();

    /// Per-frame state setup. Shader/texture/render-state can change between
    /// frames even when the underlying geometry doesn't, so they stay outside
    /// the mesh-buffer cache and run on every draw.
    Uint64 _sc_t0 = SDL_GetPerformanceCounter();
    Uint64 _t = MikuPan_PerfBegin();
    MikuPan_SetCurrentShaderProgram(MESH_0x12_SHADER);
    MikuPan_PerfEnd(PERF_SECT_SC_SHADER, _t);

    if ((int64_t)mesh_tex_reg < (int64_t)pVMCD)
    {
        _t = MikuPan_PerfBegin();
        MikuPan_SetTexture(mesh_tex_reg);
        MikuPan_PerfEnd(PERF_SECT_SC_TEXTURE, _t);
    }

    _t = MikuPan_PerfBegin();
    MikuPan_SetRenderState3D();
    MikuPan_PerfEnd(PERF_SECT_SC_RS3D, _t);
    MikuPan_PerfEnd(PERF_SECT_STATE_CHANGE, _sc_t0);

    /// ── Cache fast path ──
    /// Static furniture / map geometry: the SGD bytes don't change between
    /// frames, so on every draw after the first we just bind the cached VAO
    /// (which already has all attribute pointers + IBO baked in) and issue
    /// the draw. No CPU staging, no buffer uploads.
    const int cache_on = MikuPan_MeshCache_IsEnabled();
    MikuPan_MeshCacheEntry *cache_entry = NULL;

    if (cache_on)
    {
        cache_entry = MikuPan_MeshCache_Lookup(pPUHead);
        if (cache_entry != NULL && cache_entry->sgd_top == sgd_top_addr)
        {
            _t = MikuPan_PerfBegin();
            MikuPan_BindVAO(cache_entry->vao);
            MikuPan_PerfEnd(PERF_SECT_SC_VAO, _t);

            MikuPan_PerfDrawCall();
            MikuPan_TimedDrawElements(MikuPan_GetMeshRenderMode(),
                                      cache_entry->index_count, GL_UNSIGNED_INT, (void *)0);

            if (MikuPan_IsNormalsRendering())
            {
                MikuPan_SetCurrentShaderProgram(NORMALS_0x12_SHADER);
                MikuPan_TimedDrawElements(GL_TRIANGLES, cache_entry->index_count,
                                          GL_UNSIGNED_INT, (void *)0);
            }

            MikuPan_PerfMeshCacheHit();
            return;
        }

        /// Miss — allocate a per-mesh entry now and target its static buffers
        /// instead of the shared streaming buffers during the upload phase.
        MikuPan_PerfMeshCacheMissNew();
        cache_entry = MikuPan_MeshCache_Insert(pPUHead, sgd_top_addr, desired_pipeline);
    }
    else
    {
        /// Cache disabled — use the shared streaming VAO so the upload phase
        /// targets the per-frame DYNAMIC buffers (the A/B-comparison baseline).
        _t = MikuPan_PerfBegin();
        MikuPan_BindVAO(pipeline->vao);
        MikuPan_PerfEnd(PERF_SECT_SC_VAO, _t);
    }

    // ── Vertex source ──
    GLfloat *vertices = NULL;
    if (mesh_type == 0x32)
    {
        vertices = (GLfloat *)(pVUVNData->VUVNData_Preset.aui + (pVUVN->VUVNDesc.sNumNormal) * 3 + 10);
        memcpy(g_mesh_buffers_0x32.positions, vertices, (size_t)pVUVN->VUVNDesc.sNumVertex * 3 * sizeof(float));
    }
    else /* 0x12 / 0x10 */
    {
        vertices = (float *)&(((int *)pVUVN)[14]);
        if (!cache_on)
        {
            size_t byte_size = pVUVN->VUVNDesc.sNumVertex * pipeline->buffers[0].attributes[0].stride;
            MikuPan_StreamUploadFull(GL_ARRAY_BUFFER, pipeline->buffers[0].id, (GLsizeiptr)byte_size, vertices);
        }
    }

    // ── Build CPU staging buffers (UVs, colors, normals (SoA), indices) ──
    const int uv_buf_idx    = (mesh_type == 0x32) ? 2 : 1;
    const int color_buf_idx = (mesh_type == 0x32) ? 3 : 2;
    const int uv_stride    = pipeline->buffers[uv_buf_idx].attributes[0].stride;
    const int color_stride = pipeline->buffers[color_buf_idx].attributes[0].stride;

    int vertex_offset = 0;
    int index_write_offset = 0;

    for (int i = 0; i < GET_NUM_MESH(pPUHead); i++)
    {
        pVMCD = (_SGDVUMESHCOLORDATA *) MikuPan_GetNextUnpackAddr((u_int *) pVMCD);
        int vertex_count = pVMCD->VifUnpack.NUM;

        if (pPUHead->VUMeshDesc.MeshType.TEX == 1)
        {
            MikuPan_FixUV((float*)&sgdMeshData->astData, vertex_count);
        }

        if (pPUHead->VUMeshDesc.MeshType.PRE == 1)
        {
            MikuPan_FixColors((float*)&pVMCD->avColor, vertex_count);
        }

        index_write_offset += MikuPan_SetTriangleIndex(g_mesh_buffers_0x32.indices, vertex_count, vertex_offset, index_write_offset);

        memcpy((char *)g_mesh_buffers_0x32.uvs + vertex_offset * uv_stride, sgdMeshData->astData, (size_t)vertex_count * uv_stride);
        memcpy((char *)g_mesh_buffers_0x32.colors + vertex_offset * color_stride, pVMCD->avColor, (size_t)vertex_count * color_stride);

        if (mesh_type == 0x32)
        {
            // Per-submesh normal: tight contiguous fill (compiler auto-vectorizes).
            const GLfloat *normals = (GLfloat *)(pVUVNData->VUVNData_Preset.aui + (i * 3) + 10);
            const float n0 = normals[0], n1 = normals[1], n2 = normals[2];
            float *ndst = g_mesh_buffers_0x32.normals + vertex_offset * 3;

            for (int j = 0; j < vertex_count; j++)
            {
                ndst[j * 3 + 0] = n0;
                ndst[j * 3 + 1] = n1;
                ndst[j * 3 + 2] = n2;
            }
        }

        vertex_offset += vertex_count;
        sgdMeshData = (SGDVUMESHSTDATA *) &sgdMeshData->astData[vertex_count];
        pVMCD = (_SGDVUMESHCOLORDATA *) &pVMCD->avColor[vertex_count];
    }

    const int N = vertex_offset;
    const int pos_stride = pipeline->buffers[0].attributes[0].stride;
    if (cache_on)
    {
        if (desired_pipeline == POSITION3_NORMAL3_UV2_SOA)
        {
            MikuPan_MeshCache_UploadVbo(cache_entry, 0, (long)(N * pos_stride), g_mesh_buffers_0x32.positions);
            MikuPan_MeshCache_UploadVbo(cache_entry, 1, (long)(N * pipeline->buffers[1].attributes[0].stride), g_mesh_buffers_0x32.normals);
            MikuPan_MeshCache_UploadVbo(cache_entry, 2, (long)(N * pipeline->buffers[2].attributes[0].stride), g_mesh_buffers_0x32.uvs);
            MikuPan_MeshCache_UploadVbo(cache_entry, 3, (long)(N * pipeline->buffers[3].attributes[0].stride), g_mesh_buffers_0x32.colors);
        }
        else
        {
            /// 0x12 / 0x10: pos+norm AoS in buffer 0, UVs in 1, colors in 2.
            MikuPan_MeshCache_UploadVbo(cache_entry, 0, (long)(pVUVN->VUVNDesc.sNumVertex * pos_stride), vertices);
            MikuPan_MeshCache_UploadVbo(cache_entry, 1, (long)(N * uv_stride),    g_mesh_buffers_0x32.uvs);
            MikuPan_MeshCache_UploadVbo(cache_entry, 2, (long)(N * color_stride), g_mesh_buffers_0x32.colors);
        }
        MikuPan_MeshCache_UploadIbo(cache_entry, (long)(index_write_offset * (int)sizeof(u_int)), g_mesh_buffers_0x32.indices);

        cache_entry->index_count = index_write_offset;

        /// Bind the freshly-populated VAO. The bind-cache shadow was reset by
        /// MeshCache_Insert, so this issues a real glBindVertexArray.
        _t = MikuPan_PerfBegin();
        MikuPan_BindVAO(cache_entry->vao);
        MikuPan_PerfEnd(PERF_SECT_SC_VAO, _t);
    }
    else
    {
        if (desired_pipeline == POSITION3_NORMAL3_UV2_SOA)
        {
            MikuPan_StreamUploadFull(GL_ARRAY_BUFFER, pipeline->buffers[0].id, (GLsizeiptr)(N * pos_stride), g_mesh_buffers_0x32.positions);
            MikuPan_StreamUploadFull(GL_ARRAY_BUFFER, pipeline->buffers[1].id, (GLsizeiptr)(N * pipeline->buffers[1].attributes[0].stride), g_mesh_buffers_0x32.normals);
            MikuPan_StreamUploadFull(GL_ARRAY_BUFFER, pipeline->buffers[2].id, (GLsizeiptr)(N * pipeline->buffers[2].attributes[0].stride), g_mesh_buffers_0x32.uvs);
            MikuPan_StreamUploadFull(GL_ARRAY_BUFFER, pipeline->buffers[3].id, (GLsizeiptr)(N * pipeline->buffers[3].attributes[0].stride), g_mesh_buffers_0x32.colors);
        }
        else
        {
            MikuPan_StreamUploadFull(GL_ARRAY_BUFFER, pipeline->buffers[1].id, (GLsizeiptr)(N * uv_stride),    g_mesh_buffers_0x32.uvs);
            MikuPan_StreamUploadFull(GL_ARRAY_BUFFER, pipeline->buffers[2].id, (GLsizeiptr)(N * color_stride), g_mesh_buffers_0x32.colors);
        }
        MikuPan_StreamUploadFull(GL_ELEMENT_ARRAY_BUFFER, pipeline->ibo, (GLsizeiptr)(index_write_offset * (int)sizeof(u_int)), g_mesh_buffers_0x32.indices);
    }

    MikuPan_PerfDrawCall();
    MikuPan_TimedDrawElements(MikuPan_GetMeshRenderMode(), index_write_offset, GL_UNSIGNED_INT, (void *)0);

    if (MikuPan_IsNormalsRendering())
    {
        MikuPan_SetCurrentShaderProgram(NORMALS_0x12_SHADER);
        MikuPan_TimedDrawElements(GL_TRIANGLES, index_write_offset, GL_UNSIGNED_INT, (void *)0);
    }
}

void MikuPan_RenderMeshType0x82(unsigned int *pVUVN, unsigned int *pPUHead)
{
    MIKUPAN_PERF_SCOPE(PERF_SECT_MESH_RENDER);

    if (!g_shadow_pass_active && !MikuPan_IsMesh0x82Rendering())
    {
        return;
    }

    MIKUPAN_PERF_SCOPE(PERF_SECT_MESH_0x82);

    SGDVUVNDATA_PRESET *pVUVNData = (SGDVUVNDATA_PRESET *) &(((SGDPROCUNITHEADER *) pVUVN)[1]);
    SGDVUMESHPOINTNUM *pMeshInfo = (SGDVUMESHPOINTNUM *) &(((SGDPROCUNITHEADER *) pPUHead)[4]);
    SGDPROCUNITDATA *pProcData = (SGDPROCUNITDATA *) &(((SGDPROCUNITHEADER *) pPUHead)[1]);
    SGDVUMESHSTREGSET* sgdVuMeshStRegSet = (SGDVUMESHSTREGSET *) &pMeshInfo[GET_NUM_MESH(pPUHead)];
    SGDVUMESHSTDATA* sgdMeshData = (SGDVUMESHSTDATA *) &sgdVuMeshStRegSet->auiVifCode[3];
    VUVN_PRIM *v = ((VUVN_PRIM *) &((int*)pVUVN)[2]);
    sceGsTex0 *mesh_tex_reg = (sceGsTex0 *) ((int64_t) pProcData + 0x18);

    MikuPan_PipelineInfo *pipeline = MikuPan_GetPipelineInfo(POSITION3_NORMAL3_UV2);

    // ── State setup ──
    MikuPan_FlushTexturedSpriteBatch();
    Uint64 _sc_t0 = SDL_GetPerformanceCounter();
    Uint64 _t = MikuPan_PerfBegin();
    MikuPan_SetCurrentShaderProgram(MESH_0x12_SHADER);
    MikuPan_PerfEnd(PERF_SECT_SC_SHADER, _t);

    _t = MikuPan_PerfBegin();
    MikuPan_SetTexture(mesh_tex_reg);
    MikuPan_PerfEnd(PERF_SECT_SC_TEXTURE, _t);

    _t = MikuPan_PerfBegin();
    MikuPan_SetRenderState3D();
    MikuPan_PerfEnd(PERF_SECT_SC_RS3D, _t);
    MikuPan_PerfEnd(PERF_SECT_STATE_CHANGE, _sc_t0);

    /// ── Cache fast path ──
    const int cache_on = MikuPan_MeshCache_IsEnabled();
    MikuPan_MeshCacheEntry *cache_entry = NULL;

    if (cache_on)
    {
        cache_entry = MikuPan_MeshCache_Lookup(pPUHead);
        if (cache_entry != NULL && cache_entry->sgd_top == sgd_top_addr)
        {
            _t = MikuPan_PerfBegin();
            MikuPan_BindVAO(cache_entry->vao);
            MikuPan_PerfEnd(PERF_SECT_SC_VAO, _t);

            MikuPan_PerfDrawCall();
            MikuPan_TimedDrawElements(MikuPan_GetMeshRenderMode(),
                                      cache_entry->index_count, GL_UNSIGNED_INT, (void *)0);

            if (MikuPan_IsNormalsRendering())
            {
                MikuPan_SetCurrentShaderProgram(NORMALS_0x12_SHADER);
                MikuPan_TimedDrawElements(GL_TRIANGLES, cache_entry->index_count,
                                          GL_UNSIGNED_INT, (void *)0);
            }

            MikuPan_PerfMeshCacheHit();
            return;
        }

        MikuPan_PerfMeshCacheMissNew();
        cache_entry = MikuPan_MeshCache_Insert(pPUHead, sgd_top_addr, POSITION3_NORMAL3_UV2);
    }
    else
    {
        _t = MikuPan_PerfBegin();
        MikuPan_BindVAO(pipeline->vao);
        MikuPan_PerfEnd(PERF_SECT_SC_VAO, _t);
    }

    // ── Build UV / color (zeros for NVL) / index staging ──
    int vertex_offset = 0;
    int index_write_offset = 0;
    for (int i = 0; i < GET_NUM_MESH(pPUHead); i++)
    {
        int vertex_count = pMeshInfo[i].uiPointNum;
        if (vertex_count == 0) continue;

        MikuPan_FixUV((float*)sgdMeshData->astData, vertex_count);
        index_write_offset += MikuPan_SetTriangleIndex(
            g_mesh_buffers_0x82.indices, vertex_count, vertex_offset, index_write_offset);

        memcpy((char *)g_mesh_buffers_0x82.uvs + vertex_offset * pipeline->buffers[1].attributes[0].stride,
               sgdMeshData->astData,
               (size_t)vertex_count * pipeline->buffers[1].attributes[0].stride);

        memcpy((char *)g_mesh_buffers_0x82.colors + vertex_offset * pipeline->buffers[2].attributes[0].stride,
               g_zero_floats,
               (size_t)vertex_count * pipeline->buffers[2].attributes[0].stride);

        sgdMeshData = (SGDVUMESHSTDATA*) &sgdMeshData->astData[vertex_count];
        vertex_offset += vertex_count;
    }

    // ── Upload pos+norm + UV + color + indices ──
    const int N = vertex_offset;
    if (cache_on)
    {
        MikuPan_MeshCache_UploadVbo(cache_entry, 0,
            (long)(v->vnum * pipeline->buffers[0].attributes[0].stride),
            pVUVNData->avt2);
        MikuPan_MeshCache_UploadVbo(cache_entry, 1,
            (long)(N * pipeline->buffers[1].attributes[0].stride),
            g_mesh_buffers_0x82.uvs);
        MikuPan_MeshCache_UploadVbo(cache_entry, 2,
            (long)(N * pipeline->buffers[2].attributes[0].stride),
            g_mesh_buffers_0x82.colors);
        MikuPan_MeshCache_UploadIbo(cache_entry,
            (long)(index_write_offset * (int)sizeof(u_int)),
            g_mesh_buffers_0x82.indices);

        cache_entry->index_count = index_write_offset;

        _t = MikuPan_PerfBegin();
        MikuPan_BindVAO(cache_entry->vao);
        MikuPan_PerfEnd(PERF_SECT_SC_VAO, _t);
    }
    else
    {
        MikuPan_StreamUploadFull(GL_ARRAY_BUFFER, pipeline->buffers[0].id,
            (GLsizeiptr)(v->vnum * pipeline->buffers[0].attributes[0].stride),
            pVUVNData->avt2);
        MikuPan_StreamUploadFull(GL_ARRAY_BUFFER, pipeline->buffers[1].id,
            (GLsizeiptr)(N * pipeline->buffers[1].attributes[0].stride),
            g_mesh_buffers_0x82.uvs);
        MikuPan_StreamUploadFull(GL_ARRAY_BUFFER, pipeline->buffers[2].id,
            (GLsizeiptr)(N * pipeline->buffers[2].attributes[0].stride),
            g_mesh_buffers_0x82.colors);
        MikuPan_StreamUploadFull(GL_ELEMENT_ARRAY_BUFFER, pipeline->ibo,
            (GLsizeiptr)(index_write_offset * (int)sizeof(u_int)),
            g_mesh_buffers_0x82.indices);
    }

    // ── Draw ──
    MikuPan_PerfDrawCall();
    MikuPan_TimedDrawElements(MikuPan_GetMeshRenderMode(),
                              index_write_offset, GL_UNSIGNED_INT, (void *)0);

    if (MikuPan_IsNormalsRendering())
    {
        MikuPan_SetCurrentShaderProgram(NORMALS_0x12_SHADER);
        MikuPan_TimedDrawElements(GL_TRIANGLES,
                                  index_write_offset, GL_UNSIGNED_INT, (void *)0);
    }
}

void MikuPan_RenderMeshType0x2(SGDPROCUNITHEADER *pVUVN, SGDPROCUNITHEADER *pPUHead, float* vertices)
{
    MIKUPAN_PERF_SCOPE(PERF_SECT_MESH_RENDER);

    // Visibility filter is skipped during the shadow pass so the silhouette
    // captures all of the caster regardless of UI debug toggles.
    if (!g_shadow_pass_active && !MikuPan_IsMesh0x2Rendering())
    {
        return;
    }

    char mesh_type = GET_MESH_TYPE(pPUHead);
    int shader;
    if (mesh_type == 0x2)      shader = MESH_0x2_SHADER;
    else if (mesh_type == 0xA) shader = MESH_0xA_SHADER;
    else return;

    int per_type_sect = (mesh_type == 0x2) ? PERF_SECT_MESH_0x2
                                           : PERF_SECT_MESH_0xA;
    MIKUPAN_PERF_SCOPE(per_type_sect);

    VUVN_PRIM *v = ((VUVN_PRIM *) &((int*)pVUVN)[2]);
    SGDVUMESHPOINTNUM *pMeshInfo = (SGDVUMESHPOINTNUM *) &pPUHead[4];
    SGDVUMESHSTREGSET *sgdVuMeshStRegSet = (SGDVUMESHSTREGSET *) &pMeshInfo[GET_NUM_MESH(pPUHead)];
    SGDVUMESHSTDATA *sgdMeshData = (SGDVUMESHSTDATA *) &sgdVuMeshStRegSet->auiVifCode[3];
    SGDPROCUNITDATA *pProcData = (SGDPROCUNITDATA *) &pPUHead[1];
    sceGsTex0 *mesh_tex_reg = (sceGsTex0 *) ((int64_t) pProcData + 0x18);

    MikuPan_PipelineInfo *pipeline = MikuPan_GetPipelineInfo(POSITION4_NORMAL4_UV2);

    // ── State setup (per-frame, always) ──
    MikuPan_FlushTexturedSpriteBatch();
    Uint64 _sc_t0 = SDL_GetPerformanceCounter();
    Uint64 _t = MikuPan_PerfBegin();
    MikuPan_SetCurrentShaderProgram(shader);
    MikuPan_PerfEnd(PERF_SECT_SC_SHADER, _t);

    _t = MikuPan_PerfBegin();
    MikuPan_SetTexture(mesh_tex_reg);
    MikuPan_PerfEnd(PERF_SECT_SC_TEXTURE, _t);

    _t = MikuPan_PerfBegin();
    MikuPan_SetRenderState3D();
    MikuPan_PerfEnd(PERF_SECT_SC_RS3D, _t);
    MikuPan_PerfEnd(PERF_SECT_STATE_CHANGE, _sc_t0);

    /// ── Partial cache ──
    /// 0x2/0xA meshes are CPU-skinned: positions+normals change every frame,
    /// but the triangle topology and UVs are immutable. Cache the static
    /// streams (UV VBO + IBO) and the VAO; stream only pos+norm.
    const int cache_on = MikuPan_MeshCache_IsEnabled();
    MikuPan_MeshCacheEntry *cache_entry = NULL;
    int need_static_upload = 0;

    if (cache_on)
    {
        cache_entry = MikuPan_MeshCache_Lookup(pPUHead);
        if (cache_entry == NULL || cache_entry->sgd_top != sgd_top_addr)
        {
            cache_entry = MikuPan_MeshCache_Insert(pPUHead, sgd_top_addr, POSITION4_NORMAL4_UV2);
            need_static_upload = 1;
            MikuPan_PerfMeshCacheMissNew();
        }
        else
        {
            MikuPan_PerfMeshCacheHit();
        }

        _t = MikuPan_PerfBegin();
        MikuPan_BindVAO(cache_entry->vao);
        MikuPan_PerfEnd(PERF_SECT_SC_VAO, _t);
    }
    else
    {
        _t = MikuPan_PerfBegin();
        MikuPan_BindVAO(pipeline->vao);
        MikuPan_PerfEnd(PERF_SECT_SC_VAO, _t);
    }

    // ── Pos+norm AoS streamed every frame (CPU-skinned) ──
    if (cache_on)
    {
        MikuPan_MeshCache_StreamVbo(cache_entry, 0,
            (long)(v->vnum * pipeline->buffers[0].attributes[0].stride),
            vertices);
    }
    else
    {
        MikuPan_StreamUploadFull(GL_ARRAY_BUFFER, pipeline->buffers[0].id,
            (GLsizeiptr)(v->vnum * pipeline->buffers[0].attributes[0].stride),
            vertices);
    }

    /// ── Static streams (UV + indices) — only built on cache miss, OR every
    /// frame when the cache is disabled. With caching on, the steady-state
    /// hot path skips the entire per-submesh loop, the UV memcpys, and the
    /// FixUV / SetTriangleIndex calls.
    int index_count;
    if (need_static_upload || !cache_on)
    {
        int vertex_offset = 0;
        int index_write_offset = 0;
        for (int i = 0; i < GET_NUM_MESH(pPUHead); i++)
        {
            int vertex_count = pMeshInfo[i].uiPointNum;

            if (pPUHead->VUMeshDesc.MeshType.TEX == 1)
            {
                MikuPan_FixUV((float*)&sgdMeshData->astData, vertex_count);
            }

            index_write_offset += MikuPan_SetTriangleIndex(
                g_mesh_buffers_0x2.indices, vertex_count, vertex_offset, index_write_offset);

            memcpy((char *)g_mesh_buffers_0x2.uvs + vertex_offset * pipeline->buffers[1].attributes[0].stride,
                   sgdMeshData->astData,
                   (size_t)vertex_count * pipeline->buffers[1].attributes[0].stride);

            sgdMeshData = (SGDVUMESHSTDATA *) &sgdMeshData->astData[vertex_count];
            vertex_offset += vertex_count;
        }

        const int N = vertex_offset;
        if (cache_on)
        {
            MikuPan_MeshCache_UploadVbo(cache_entry, 1,
                (long)(N * pipeline->buffers[1].attributes[0].stride),
                g_mesh_buffers_0x2.uvs);
            MikuPan_MeshCache_UploadIbo(cache_entry,
                (long)(index_write_offset * (int)sizeof(u_int)),
                g_mesh_buffers_0x2.indices);
            cache_entry->index_count = index_write_offset;

            /// MeshCache_Insert unbound the VAO and reset the bind cache, so
            /// rebind to capture the streamed pos+norm + freshly-uploaded UV
            /// bindings into the next draw.
            MikuPan_BindVAO(cache_entry->vao);
        }
        else
        {
            MikuPan_StreamUploadFull(GL_ARRAY_BUFFER, pipeline->buffers[1].id,
                (GLsizeiptr)(N * pipeline->buffers[1].attributes[0].stride),
                g_mesh_buffers_0x2.uvs);
            MikuPan_StreamUploadFull(GL_ELEMENT_ARRAY_BUFFER, pipeline->ibo,
                (GLsizeiptr)(index_write_offset * (int)sizeof(u_int)),
                g_mesh_buffers_0x2.indices);
        }

        index_count = index_write_offset;
    }
    else
    {
        index_count = cache_entry->index_count;
    }

    // ── Draw ──
    MikuPan_PerfDrawCall();
    MikuPan_TimedDrawElements(MikuPan_GetMeshRenderMode(),
                              index_count, GL_UNSIGNED_INT, (void *)0);

    if (MikuPan_IsNormalsRendering())
    {
        MikuPan_SetCurrentShaderProgram(NORMALS_0x2_SHADER);
        MikuPan_TimedDrawElements(GL_TRIANGLES,
                                  index_count, GL_UNSIGNED_INT, (void *)0);
    }
}