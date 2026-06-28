#include "mikupan_renderer_internal.h"
#include "mikupan_pipeline.h"
#include "mikupan_gpu.h"
#include "mikupan_profiler.h"
#include "mikupan_shader.h"
#include "mikupan/mikupan_logging_c.h"
#include <string.h>

/* Shadow map is square. The runtime size is g_shadow_fbo_size (changeable from
 * the UI); SHADOW_FBO_MAX_SIZE bounds it and sizes the probe readback buffer. */
#define SHADOW_FBO_MAX_SIZE 2048
static int g_shadow_fbo_size = 256;
static int g_shadow_resize_pending = 0;

static GLuint g_shadow_fbo  = 0;
static GLuint g_shadow_tex  = 0;
static GLuint g_shadow_depth = 0;
static int    g_shadow_init = 0;
static GLenum g_shadow_fbo_status = 0;
static int    g_shadow_enabled = 1;
static GLint  g_shadow_saved_fbo      = 0;
static int    g_shadow_saved_viewport[4] = {0};
static mat4   g_shadow_saved_world_view;
static mat4   g_shadow_saved_projection;
static mat4   g_shadow_world_clip_view;
static int    g_shadow_matrix_valid = 0;
static int    g_shadow_pass_active = 0;
static int    g_shadow_receiver_pass_active = 0;
static int    g_shadow_receiver_debug_view = 0;
static float  g_shadow_strength = 0x50 / 128.0f;

/* Caster-model inspection: override the shadow projector with a free orbit
 * camera so the silhouette can be viewed from any angle in the preview. */
static int    g_shadow_inspect = 0;
static int    g_shadow_inspect_wireframe = 0;
static float  g_shadow_inspect_yaw   = 0.6f;
static float  g_shadow_inspect_pitch = 0.6f;

static MikuPan_ShadowDebugInfo g_shadow_debug = {0};

static void MikuPan_UpdateShadowDebugStaticFields(void)
{
    g_shadow_debug.enabled = g_shadow_enabled;
    g_shadow_debug.fbo_initialized = g_shadow_init;
    g_shadow_debug.fbo_complete = (g_shadow_fbo_status == GL_FRAMEBUFFER_COMPLETE);
    g_shadow_debug.fbo_status = g_shadow_fbo_status;
    g_shadow_debug.matrix_valid = g_shadow_matrix_valid;
    g_shadow_debug.texture_id = g_shadow_tex;
    g_shadow_debug.texture_size = g_shadow_fbo_size;
}

void MikuPan_ShadowDebugBeginFrame(void)
{
    const int probe_valid = g_shadow_debug.probe_valid;
    const int probe_nonzero_pixels = g_shadow_debug.probe_nonzero_pixels;
    const int probe_max_value = g_shadow_debug.probe_max_value;
    const float probe_coverage = g_shadow_debug.probe_coverage;
    const float probe_average = g_shadow_debug.probe_average;

    memset(&g_shadow_debug, 0, sizeof(g_shadow_debug));
    g_shadow_debug.probe_valid = probe_valid;
    g_shadow_debug.probe_nonzero_pixels = probe_nonzero_pixels;
    g_shadow_debug.probe_max_value = probe_max_value;
    g_shadow_debug.probe_coverage = probe_coverage;
    g_shadow_debug.probe_average = probe_average;
    MikuPan_UpdateShadowDebugStaticFields();
}

static int MikuPan_ShadowBucketOf(int mesh_type)
{
    switch (mesh_type & 0xff)
    {
        case 0x00: return MIKUPAN_SHADOW_BUCKET_0;
        case 0x02: return MIKUPAN_SHADOW_BUCKET_2;
        case 0x0A: return MIKUPAN_SHADOW_BUCKET_A;
        case 0x10: return MIKUPAN_SHADOW_BUCKET_10;
        case 0x12: return MIKUPAN_SHADOW_BUCKET_12;
        case 0x32: return MIKUPAN_SHADOW_BUCKET_32;
        case 0x42: return MIKUPAN_SHADOW_BUCKET_42;
        case 0x80: return MIKUPAN_SHADOW_BUCKET_80;
        case 0x82: return MIKUPAN_SHADOW_BUCKET_82;
        default:   return MIKUPAN_SHADOW_BUCKET_OTHER;
    }
}

void MikuPan_ShadowDebugRecordCasterMeshType(int mesh_type)
{
    g_shadow_debug.caster_seen[MikuPan_ShadowBucketOf(mesh_type)]++;
}

void MikuPan_ShadowDebugRecordReceiverMeshType(int mesh_type)
{
    g_shadow_debug.receiver_seen[MikuPan_ShadowBucketOf(mesh_type)]++;
}

void MikuPan_ShadowDebugRecordCasterDraw(int mesh_type, int index_count)
{
    g_shadow_debug.caster_draws++;
    g_shadow_debug.caster_indices += index_count;
    g_shadow_debug.caster_drawn[MikuPan_ShadowBucketOf(mesh_type)]++;
}

void MikuPan_ShadowDebugRecordReceiverDraw(int mesh_type, int index_count)
{
    g_shadow_debug.receiver_draws++;
    g_shadow_debug.receiver_indices += index_count;
    g_shadow_debug.receiver_drawn[MikuPan_ShadowBucketOf(mesh_type)]++;
}

const MikuPan_ShadowDebugInfo *MikuPan_GetShadowDebugInfo(void)
{
    MikuPan_UpdateShadowDebugStaticFields();
    return &g_shadow_debug;
}

int MikuPan_IsShadowPassActive(void)
{
    return g_shadow_pass_active;
}

int MikuPan_IsShadowReceiverPassActive(void)
{
    return g_shadow_receiver_pass_active;
}

void MikuPan_SetShadowStrengthFromPs2Alpha(int alpha)
{
    if (alpha < 0)
    {
        alpha = 0;
    }
    else if (alpha > 0x80)
    {
        alpha = 0x80;
    }

    g_shadow_strength = (float) alpha / 128.0f;
}

static void MikuPan_EnsureShadowFbo(void)
{
    if (g_shadow_init && !g_shadow_resize_pending)
    {
        return;
    }

    /* Resolution changed: tear down the old objects so they're rebuilt below. */
    if (g_shadow_init)
    {
        MikuPan_GPUReleaseTexture(g_shadow_tex);
        g_shadow_fbo = 0;
        g_shadow_tex = 0;
        g_shadow_depth = 0;
        g_shadow_init = 0;
    }
    g_shadow_resize_pending = 0;

    const int size = g_shadow_fbo_size;

    g_shadow_tex = MikuPan_GPUCreateTextureR8Target(size, size);
    if (g_shadow_tex == 0)
    {
        g_shadow_fbo_status = 0;
        info_log("Shadow texture creation failed");
        return;
    }

    g_shadow_fbo = 1;
    g_shadow_depth = 0;
    g_shadow_fbo_status = GL_FRAMEBUFFER_COMPLETE;
    g_shadow_init = 1;
}

/*
 * Build the shadow projector's world->clip matrix in OpenGL conventions.
 *
 * The PS2 scamera.* matrices map into GS screen space (origin 2048, sub-pixel
 * fixed point, etc.) and are NOT valid as GL clip matrices. Instead we build a
 * proper orthographic light-view-projection here with cglm — the same column-
 * major, NDC [-1,1] convention MikuPan uses for the main camera — from purely
 * world-space inputs:
 *   center       : caster bounding-box centre (world space)
 *   light_dir    : source-side direction, matching shadow.c's ndirection
 *   world_bbox8  : 8 caster bounding-box corners (world space, stride 4 floats)
 *
 * The result is used both as the caster silhouette's view matrix (projection is
 * identity in BeginShadowPass) and, via g_shadow_world_clip_view, as the
 * receiver's uShadowMatrix — so caster and receiver stay perfectly consistent.
 */
float *MikuPan_ComputeShadowClipView(const float *center,
                                     const float *light_dir,
                                     const float *world_bbox8)
{
    static mat4 clip;

    vec3 c   = { center[0], center[1], center[2] };
    vec3 dir = { light_dir[0], light_dir[1], light_dir[2] };
    glm_vec3_normalize(dir);

    /* One-time diagnostic: verify the source-side direction matches the
     * flashlight. A reversed shadow means this vector is being treated as
     * light travel direction instead of caster->light/source direction. */
    {
        static int logged = 0;
        if (logged < 8)
        {
            logged++;
            info_log("Shadow dir: center=(%.1f,%.1f,%.1f) source_dir=(%.3f,%.3f,%.3f)",
                     c[0], c[1], c[2], dir[0], dir[1], dir[2]);
        }
    }

    /* Match SetShadowCamera(): scamera.p = center + ndirection * 25000.
     * ndirection points from the caster toward the light/source side; the
     * resulting projection casts onto receivers away from that source. */
    const float dist = 5000.0f;
    vec3 eye;
    if (g_shadow_inspect)
    {
        /* Inspection: free orbit around the caster centre instead of the light
         * direction, so the silhouette can be examined from any angle in the
         * preview. (This overrides the real cast direction while enabled.) */
        float cp = cosf(g_shadow_inspect_pitch);
        vec3 odir = { cosf(g_shadow_inspect_yaw) * cp,
                      sinf(g_shadow_inspect_pitch),
                      sinf(g_shadow_inspect_yaw) * cp };
        glm_vec3_scale(odir, dist, eye);
        glm_vec3_add(c, eye, eye);
    }
    else
    {
        glm_vec3_scale(dir, dist, eye);
        glm_vec3_add(c, eye, eye);
    }

    /* Avoid a degenerate up vector for a near-vertical view. */
    vec3 view_dir;
    glm_vec3_sub(c, eye, view_dir);
    glm_vec3_normalize(view_dir);
    vec3 up = { 0.0f, 1.0f, 0.0f };
    float dup = glm_vec3_dot(view_dir, up);
    if (dup > 0.99f || dup < -0.99f)
    {
        up[0] = 0.0f; up[1] = 0.0f; up[2] = 1.0f;
    }

    mat4 view;
    glm_lookat(eye, c, up, view);

    /* Fit an ortho box to the caster bbox in view space. */
    float minx =  1e30f, miny =  1e30f, minz =  1e30f;
    float maxx = -1e30f, maxy = -1e30f, maxz = -1e30f;
    for (int i = 0; i < 8; i++)
    {
        vec4 wp = { world_bbox8[i * 4 + 0], world_bbox8[i * 4 + 1],
                    world_bbox8[i * 4 + 2], 1.0f };
        vec4 vp;
        glm_mat4_mulv(view, wp, vp);
        if (vp[0] < minx) minx = vp[0];
        if (vp[0] > maxx) maxx = vp[0];
        if (vp[1] < miny) miny = vp[1];
        if (vp[1] > maxy) maxy = vp[1];
        if (vp[2] < minz) minz = vp[2];
        if (vp[2] > maxz) maxz = vp[2];
    }

    const float padx = (maxx - minx) * 0.05f + 1.0f;
    const float pady = (maxy - miny) * 0.05f + 1.0f;

    /* View space looks down -Z: near = -maxz, far = -minz. Extend far well past
     * the caster so the floor it drops onto stays inside the depth slab. */
    float near_z = -maxz - 1.0f;
    float far_z  = -minz + 20000.0f;
    if (near_z < 1.0f) near_z = 1.0f;

    mat4 proj;
    glm_ortho(minx - padx, maxx + padx, miny - pady, maxy + pady,
              near_z, far_z, proj);

    glm_mat4_mul(proj, view, clip);
    return (float *) clip;
}

void MikuPan_BeginShadowPass(float *world_clip_view)
{
    MikuPan_EnsureShadowFbo();
    g_shadow_debug.caster_passes++;

    // Snapshot viewport so EndShadowPass can hand the renderer back to the
    // main scene pass without it noticing.
    g_shadow_saved_viewport[0] = 0;
    g_shadow_saved_viewport[1] = 0;
    g_shadow_saved_viewport[2] = render_back_msaa.texture.width;
    g_shadow_saved_viewport[3] = render_back_msaa.texture.height;
    memcpy(g_shadow_saved_world_view, WorldView, sizeof(g_shadow_saved_world_view));
    memcpy(g_shadow_saved_projection, projection, sizeof(g_shadow_saved_projection));

    MikuPan_GPUSetShadowTarget(g_shadow_tex, g_shadow_fbo_size, 1);
    MikuPan_SetViewportCached(0, 0, g_shadow_fbo_size, g_shadow_fbo_size);
    MikuPan_SetRenderStateShadow();

    /* Inspection wireframe: draw caster silhouettes as edges so the mesh
     * topology (spikes, bridging, winding) is visible in the preview. */
    if (g_shadow_inspect && g_shadow_inspect_wireframe)
    {
        MikuPan_GPUSetFillLine(1);
    }

    if (world_clip_view != NULL)
    {
        mat4 identity_projection;
        glm_mat4_identity(identity_projection);

        memcpy(g_shadow_world_clip_view, world_clip_view, sizeof(g_shadow_world_clip_view));
        MikuPan_SetViewProjectionMatrices(world_clip_view, (float *)identity_projection);
        g_shadow_matrix_valid = 1;
    }

    MikuPan_SetUniform1iToAllShaders(2, "uShadowEnabled");
    MikuPan_InvalidateModelTransformCache();
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

    MikuPan_SetUniform2fToCurrentShader((float)g_shadow_fbo_size,
                                        (float)g_shadow_fbo_size,
                                        "uShadowSize");
    MikuPan_SetUniform1fToCurrentShader(0.6f, "uShadowDarkness");

    // Need additive/normal blending so the ellipse alpha isn't clobbered.
    MikuPan_GPUSetDepthFunc(GL_ALWAYS);
    MikuPan_GPUSetBlendMode(1, MIKUPAN_GPU_BLEND_NORMAL);

    MikuPan_TimedDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    MikuPan_ResetRenderStateCache();
}

void MikuPan_EndShadowPass(void)
{
    MikuPan_GPUSetFillLine(0);

    // Drop the override + active flag *before* we start pushing scene-side
    // uniforms below — those uniform pushes go through SetCurrentShaderProgram
    // internally and would otherwise route into the silhouette shader.
    MikuPan_SetShaderOverride(-1);
    g_shadow_pass_active = 0;
    MikuPan_SetViewProjectionMatrices((float *)g_shadow_saved_world_view,
                                      (float *)g_shadow_saved_projection);

    MikuPan_GPUSetTarget(MIKUPAN_GPU_TARGET_SCENE, 0);
    MikuPan_SetViewportCached(g_shadow_saved_viewport[0],
                              g_shadow_saved_viewport[1],
                              g_shadow_saved_viewport[2],
                              g_shadow_saved_viewport[3]);

    // Push shadow uniforms for the receiver decal pass. The forward mesh
    // shader's built-in sampler is kept disabled so the shadow is applied
    // only through AssignShadow's receiver traversal, not order-dependent
    // later mesh draws. Texture unit 1 is reserved for the shadow map; unit 0
    // stays the diffuse path so uTexture continues to work without changes.
    MikuPan_SetUniform1iToAllShaders(0,                                                "uShadowEnabled");
    MikuPan_SetUniform1iToAllShaders(1,                                                "uShadowTex");
    MikuPan_SetUniform1fToAllShaders(g_shadow_strength,                                "uShadowStrength");
    MikuPan_SetUniformMatrix4fvToAllShaders((float *)g_shadow_world_clip_view,         "uShadowMatrix");

    MikuPan_ActiveTextureCached(GL_TEXTURE1);
    MikuPan_BindTexture2DCached(g_shadow_tex);
    MikuPan_ActiveTextureCached(GL_TEXTURE0);
}

void MikuPan_BeginShadowReceiverPass(void)
{
    if (!g_shadow_enabled || !g_shadow_matrix_valid)
    {
        g_shadow_receiver_pass_active = 0;
        return;
    }

    g_shadow_debug.receiver_passes++;
    MikuPan_SetRenderStateShadowReceiver();
    MikuPan_SetShaderOverride(SHADOW_RECEIVER_SHADER);
    MikuPan_SetCurrentShaderProgram(SHADOW_RECEIVER_SHADER);
    MikuPan_SetUniform1iToCurrentShader(g_shadow_receiver_debug_view,
                                        "uShadowDebugView");
    g_shadow_receiver_pass_active = 1;

    MikuPan_ActiveTextureCached(GL_TEXTURE1);
    MikuPan_BindTexture2DCached(g_shadow_tex);
    MikuPan_ActiveTextureCached(GL_TEXTURE0);
    MikuPan_InvalidateModelTransformCache();
}

void MikuPan_EndShadowReceiverPass(void)
{
    if (!g_shadow_receiver_pass_active)
    {
        return;
    }

    MikuPan_SetShaderOverride(-1);
    g_shadow_receiver_pass_active = 0;
    MikuPan_ResetRenderStateCache();
    MikuPan_InvalidateModelTransformCache();
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
    g_shadow_enabled = enabled ? 1 : 0;
}

int MikuPan_IsShadowInspectEnabled(void)
{
    return g_shadow_inspect;
}

void MikuPan_SetShadowInspectEnabled(int enabled)
{
    g_shadow_inspect = enabled ? 1 : 0;
}

void MikuPan_GetShadowInspectAngles(float *yaw, float *pitch)
{
    if (yaw)   *yaw   = g_shadow_inspect_yaw;
    if (pitch) *pitch = g_shadow_inspect_pitch;
}

void MikuPan_SetShadowInspectAngles(float yaw, float pitch)
{
    g_shadow_inspect_yaw   = yaw;
    g_shadow_inspect_pitch = pitch;
}

int MikuPan_IsShadowInspectWireframe(void)
{
    return g_shadow_inspect_wireframe;
}

void MikuPan_SetShadowInspectWireframe(int enabled)
{
    g_shadow_inspect_wireframe = enabled ? 1 : 0;
}

int MikuPan_GetShadowResolution(void)
{
    return g_shadow_fbo_size;
}

void MikuPan_SetShadowResolution(int size)
{
    if (size < 64)                   size = 64;
    if (size > SHADOW_FBO_MAX_SIZE)  size = SHADOW_FBO_MAX_SIZE;

    if (size != g_shadow_fbo_size)
    {
        g_shadow_fbo_size = size;
        /* Rebuilt lazily on the GL thread by MikuPan_EnsureShadowFbo(). */
        g_shadow_resize_pending = 1;
    }
}

int MikuPan_IsShadowReceiverDebugViewEnabled(void)
{
    return g_shadow_receiver_debug_view;
}

void MikuPan_SetShadowReceiverDebugViewEnabled(int enabled)
{
    g_shadow_receiver_debug_view = enabled ? 1 : 0;
}

void MikuPan_ShadowDebugProbeTexture(void)
{
    /* Static (not stack) so the largest resolution doesn't overflow the stack. */
    static unsigned char pixels[SHADOW_FBO_MAX_SIZE * SHADOW_FBO_MAX_SIZE];
    int nonzero = 0;
    int max_value = 0;
    unsigned int sum = 0;

    MikuPan_EnsureShadowFbo();

    const int size = g_shadow_fbo_size;
    const int total = size * size;

    if (!MikuPan_GPUReadTextureR8(g_shadow_tex, size, pixels))
    {
        g_shadow_debug.probe_valid = 0;
        return;
    }

    for (int i = 0; i < total; i++)
    {
        int value = pixels[i];
        if (value != 0)
        {
            nonzero++;
        }
        if (value > max_value)
        {
            max_value = value;
        }
        sum += (unsigned int)value;
    }

    g_shadow_debug.probe_valid = 1;
    g_shadow_debug.probe_nonzero_pixels = nonzero;
    g_shadow_debug.probe_max_value = max_value;
    g_shadow_debug.probe_coverage =
        (float)nonzero / (float)total;
    g_shadow_debug.probe_average =
        (float)sum / (float)(total * 255);
}
