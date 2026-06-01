#include "mikupan_renderer.h"
#include "mikupan_renderer_internal.h"
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

SDL_GLContext gl_context = NULL;
MikuPan_RenderWindow mikupan_render = {0};
MikuPan_MsaaBufferObject render_back_msaa = {0};

void MikuPan_StreamUploadFull(GLenum target, GLuint buffer,
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

int MikuPan_IsBlackWhiteModeActive(void)
{
    return loadbw_flg != 0;
}

static void MikuPan_ConvertRGBA8ToBlackWhite(unsigned char *rgba,
                                             int width,
                                             int height)
{
    const int pixel_count = width * height;

    for (int i = 0; i < pixel_count; i++)
    {
        unsigned char *p = rgba + i * 4;
        const unsigned int gray =
            ((unsigned int)p[0] + (unsigned int)p[1] + (unsigned int)p[2]) / 3;
        p[0] = (unsigned char)gray;
        p[1] = (unsigned char)gray;
        p[2] = (unsigned char)gray;
    }
}

SDL_AppResult MikuPan_Init()
{
    MikuPan_LoadConfiguration(NULL);
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

    int config_window_flags = SDL_WINDOW_RESIZABLE | SDL_WINDOW_OPENGL;

    if (mikupan_configuration.renderer.is_fullscreen > 0 && mikupan_configuration.renderer.is_fullscreen)
    {
        config_window_flags |= SDL_WINDOW_FULLSCREEN;
    }
    else
    {
        mikupan_configuration.renderer.is_fullscreen = 0;
    }

    mikupan_render.window = SDL_CreateWindow(
        "MikuPan",
        desired_window_width,
        desired_window_height,
        config_window_flags
        );

    if (mikupan_render.window == NULL)
    {
        info_log("Error creating the SDL window %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    SDL_GetWindowSize(mikupan_render.window, &mikupan_render.width, &mikupan_render.height);
    mikupan_configuration.renderer.window.width = mikupan_render.width;
    mikupan_configuration.renderer.window.height = mikupan_render.height;

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

    gl_context = SDL_GL_CreateContext(mikupan_render.window);

    if (gl_context == NULL)
    {
        info_log("Error creating the OpenGL context %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    SDL_GL_MakeCurrent(mikupan_render.window, gl_context);
    info_log("GLad version loaded %d", gladLoadGLLoader((void*)SDL_GL_GetProcAddress));

    int desired_render_width = mikupan_configuration.renderer.render.width;
    int desired_render_height = mikupan_configuration.renderer.render.height;
    int desired_msaa = mikupan_configuration.renderer.msaa_index;
    int desired_vsync = mikupan_configuration.renderer.vsync;

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

    if (desired_vsync < 0 || desired_vsync > 1)
    {
        desired_vsync = 1;
        mikupan_configuration.renderer.vsync = desired_vsync;
    }

    if (!SDL_GL_SetSwapInterval(desired_vsync))
    {
        info_log("Failed to disable GL vsync: %s", SDL_GetError());
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

    if (MikuPan_IsBlackWhiteModeActive())
    {
        MikuPan_ConvertRGBA8ToBlackWhite(out_rgba, width, height);
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
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_DEBUG_FLAG);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_FRAMEBUFFER_SRGB_CAPABLE, -1);
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
    glad_glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glad_glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

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
    MikuPan_PerfBeginFrame();

    MikuPan_GsResetFrameMetrics();
    MikuPan_PerfResetFrame();

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

    if (mikupan_configuration.renderer.is_fullscreen != MikuPan_IsFullScreen())
    {
        SDL_SetWindowFullscreen(mikupan_render.window, MikuPan_IsFullScreen());
        mikupan_configuration.renderer.is_fullscreen = MikuPan_IsFullScreen();
    }

    if (mikupan_configuration.renderer.vsync != MikuPan_IsVsync())
    {
        mikupan_configuration.renderer.vsync = MikuPan_IsVsync();
        if (!SDL_GL_SetSwapInterval(mikupan_configuration.renderer.vsync))
        {
            info_log("Failed to disable GL vsync: %s", SDL_GetError());
        }
    }
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
    MikuPan_SetUniform1iToCurrentShader(0, "uTexture");
    /*
     * Black/white mode is applied through the original model CLUT/light paths
     * and to explicit framebuffer copies. Do not apply it here: the final
     * postprocess pass contains already-composited 2D HUD/menu sprites too.
     */
    MikuPan_SetUniform1iToCurrentShader(0, "uBlackWhiteMode");
    MikuPan_SetUniform1fToCurrentShader(MikuPan_GetBrightness(), "uBrightness");
    MikuPan_SetUniform1fToCurrentShader(MikuPan_GetGamma(),      "uGamma");
    {
        const MikuPan_ConfigCrt *crt = MikuPan_GetCrtSettings();
        MikuPan_SetUniform1iToCurrentShader(crt->enabled, "uCrtEnabled");
        MikuPan_SetUniform1fToCurrentShader(crt->strength, "uCrtStrength");
        MikuPan_SetUniform1fToCurrentShader(crt->curvature, "uCrtCurvature");
        MikuPan_SetUniform1fToCurrentShader(crt->overscan, "uCrtOverscan");
        MikuPan_SetUniform1fToCurrentShader(crt->scanline_strength, "uCrtScanlineStrength");
        MikuPan_SetUniform1fToCurrentShader(crt->scanline_scale, "uCrtScanlineScale");
        MikuPan_SetUniform1fToCurrentShader(crt->scanline_thickness, "uCrtScanlineThickness");
        MikuPan_SetUniform1fToCurrentShader(crt->mask_strength, "uCrtMaskStrength");
        MikuPan_SetUniform1fToCurrentShader(crt->mask_scale, "uCrtMaskScale");
        MikuPan_SetUniform1fToCurrentShader(crt->vignette_strength, "uCrtVignetteStrength");
        MikuPan_SetUniform1fToCurrentShader(crt->vignette_size, "uCrtVignetteSize");
        MikuPan_SetUniform1fToCurrentShader(crt->chroma_offset, "uCrtChromaOffset");
        MikuPan_SetUniform1fToCurrentShader(crt->blend_strength, "uCrtBlendStrength");
        MikuPan_SetUniform1fToCurrentShader(crt->blend_radius, "uCrtBlendRadius");
        MikuPan_SetUniform1fToCurrentShader(crt->noise_strength, "uCrtNoiseStrength");
        MikuPan_SetUniform1fToCurrentShader(crt->flicker_strength, "uCrtFlickerStrength");
        MikuPan_SetUniform1fToCurrentShader(crt->glow_strength, "uCrtGlowStrength");
        MikuPan_SetUniform2fToCurrentShader((float) render_back_msaa.texture.width,
                                            (float) render_back_msaa.texture.height,
                                            "uTextureSize");
        MikuPan_SetUniform2fToCurrentShader((float) offset_output[2],
                                            (float) offset_output[3],
                                            "uOutputSize");
        MikuPan_SetUniform1fToCurrentShader((float) ((double) SDL_GetTicks() / 1000.0),
                                            "uTime");
    }

    MikuPan_PipelineInfo* pipeline = MikuPan_GetPipelineInfo(UV4_COLOUR4_POSITION4);
    MikuPan_BindVAO(pipeline->vao);

    MikuPan_StreamUploadFull(GL_ARRAY_BUFFER, pipeline->buffers[0].id,
                             (GLsizeiptr)sizeof(quad), quad);

    glad_glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    int screenshot_width = 0, screenshot_height = 0;
    SDL_GetWindowSizeInPixels(mikupan_render.window, &screenshot_width, &screenshot_height);
    MikuPan_ScreenshotCaptureIfRequested(screenshot_width, screenshot_height);

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

    if (width > 0)
    {
        mikupan_configuration.renderer.window.width = width;
    }

    if (height > 0)
    {
        mikupan_configuration.renderer.window.height = height;
    }
}

int MikuPan_GetWindowWidth()
{
    return render_back_msaa.texture.width;
}

int MikuPan_GetWindowHeight()
{
    return render_back_msaa.texture.height;
}

void MikuPan_GetFullScreenHalfExtent(float *half_w, float *half_h)
{
    float vx, vy, vw, vh, scale;
    MikuPan_GetPS2Viewport(MikuPan_GetWindowWidth(), MikuPan_GetWindowHeight(),
                           &vx, &vy, &vw, &vh, &scale);
    if (scale <= 0.0f)
    {
        *half_w = PS2_CENTER_X;
        *half_h = PS2_CENTER_Y;
        return;
    }
    *half_w = (float) MikuPan_GetWindowWidth() / (2.0f * scale);
    *half_h = (float) MikuPan_GetWindowHeight() / (2.0f * scale);
}

int MikuPan_GetRenderMode()
{
    return MikuPan_IsWireframeRendering() ? GL_LINE_STRIP : GL_TRIANGLE_STRIP;
}

void MikuPan_RenderSetDebugValues()
{
    static int   last_render_normals   = -1;
    static float last_normal_length    = -1.0f;
    static int   last_disable_lighting = -1;
    static int   last_static_lighting  = -1;
    static int   last_mesh_lighting_mode = -1;
    static unsigned int last_shader_generation = 0;

    int   render_normals   = MikuPan_IsNormalsRendering();
    float normal_length    = MikuPan_GetNormalLength();
    int   disable_lighting = MikuPan_IsLightingDisabled();
    int   static_lighting  = MikuPan_ShowStaticLighting();
    int   mesh_lighting_mode = MikuPan_GetMeshLightingMode();
    unsigned int shader_generation = MikuPan_GetShaderGeneration();

    if (shader_generation != last_shader_generation)
    {
        last_render_normals = -1;
        last_normal_length = -1.0f;
        last_disable_lighting = -1;
        last_static_lighting = -1;
        last_mesh_lighting_mode = -1;
        last_shader_generation = shader_generation;
    }

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

    if (mesh_lighting_mode != last_mesh_lighting_mode)
    {
        MikuPan_SetUniform1iToAllShaders(mesh_lighting_mode, "uMeshLightingMode");
        last_mesh_lighting_mode = mesh_lighting_mode;
    }
}

void MikuPan_Shutdown()
{
    MikuPan_TextureShutdown();
    SDL_DestroyWindow(mikupan_render.window);
}
