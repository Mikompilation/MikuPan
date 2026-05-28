#include "mikupan_renderer_internal.h"
#include "mikupan_pipeline.h"
#include "mikupan_profiler.h"
#include "mikupan_shader.h"
#include "mikupan/mikupan_logging_c.h"
#include <string.h>

#define SHADOW_FBO_SIZE 256

static GLuint g_shadow_fbo  = 0;
static GLuint g_shadow_tex  = 0;
static int    g_shadow_init = 0;
static int    g_shadow_enabled = 0;
static GLint  g_shadow_saved_fbo      = 0;
static int    g_shadow_saved_viewport[4] = {0};
static mat4   g_shadow_world_clip_view;
static int    g_shadow_matrix_valid = 0;
static int    g_shadow_pass_active = 0;

int MikuPan_IsShadowPassActive(void)
{
    return g_shadow_pass_active;
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
