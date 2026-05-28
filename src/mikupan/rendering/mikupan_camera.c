#include "mikupan_renderer_internal.h"
#include "graphics/graph3d/sglib.h"
#include "mikupan_pipeline.h"
#include "mikupan_profiler.h"
#include "mikupan_shader.h"
#include <math.h>
#include <string.h>

mat4 WorldClipView = {0};
mat4 WorldView = {0};
mat4 projection = {0};
mat4 ViewClip = {0};
mat4 WorldClip = {0};
static mat4 g_cached_model_matrix;
static int  g_has_cached_model = 0;

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
