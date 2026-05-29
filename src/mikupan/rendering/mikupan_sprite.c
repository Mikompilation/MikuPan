#include "mikupan_renderer_internal.h"
#include "graphics/graph2d/message.h"
#include "mikupan/mikupan_logging_c.h"
#include "mikupan/mikupan_utils.h"
#include "mikupan/ui/mikupan_ui.h"
#include "mikupan_pipeline.h"
#include "mikupan_profiler.h"
#include "mikupan_shader.h"
#include <string.h>

#define TEXTURED_SPRITE_BATCH_MAX 4096

typedef struct
{
    int active;
    int sprite_count;
    GLuint texture_id;
} MikuPan_TexturedSpriteBatch;

static MikuPan_TexturedSpriteBatch g_textured_sprite_batch = {0};
static float g_textured_sprite_cpu_buf[TEXTURED_SPRITE_BATCH_MAX * 6 * 12];
static GLuint g_screen_copy_texture = 0;
static GLuint g_screen_copy_fbo = 0;
static int g_screen_copy_w = 0;
static int g_screen_copy_h = 0;
static MikuPan_ScreenCopyDebugInfo g_screen_copy_debug = {0};

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

    MikuPan_RenderSprite(src_rect, dst_rect, sprite->r, sprite->g, sprite->b, sprite->alpha, MikuPan_GetCurrentFontTexture());
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

void MikuPan_RenderTexturedTriangles3D(sceGsTex0 *tex, float *buffer, int vertex_count)
{
    if (vertex_count <= 0)
    {
        return;
    }

    MIKUPAN_PERF_SCOPE(PERF_SECT_SPRITE_RENDER);
    MikuPan_FlushTexturedSpriteBatch();
    MikuPan_SetCurrentShaderProgram(SPRITE_SHADER);
    MikuPan_PipelineInfo *pipeline = MikuPan_GetPipelineInfo(UV4_COLOUR4_POSITION4);

    MikuPan_BindVAO(pipeline->vao);
    MikuPan_SetTexture(tex);
    MikuPan_SetRenderStateSprite3D();

    MikuPan_StreamUploadFull(
        GL_ARRAY_BUFFER,
        pipeline->buffers[0].id,
        (GLsizeiptr)(vertex_count * 12 * (int)sizeof(float)),
        buffer);

    MikuPan_TimedDrawArrays(GL_TRIANGLES, 0, vertex_count);
    MikuPan_PerfDrawCall();
}

static void MikuPan_ApplyParticleTriangleState(int depth_always, int additive_blend)
{
    MikuPan_SetRenderStateSprite3D();
    glad_glDepthMask(GL_FALSE);
    glad_glDepthFunc(depth_always ? GL_ALWAYS : GL_LEQUAL);
    glad_glBlendEquation(GL_FUNC_ADD);
    glad_glBlendFunc(GL_SRC_ALPHA, additive_blend ? GL_ONE : GL_ONE_MINUS_SRC_ALPHA);
}

static void MikuPan_ApplyHeatHazeTriangleState(int depth_always)
{
    MikuPan_SetRenderStateSprite3D();
    glad_glDepthMask(GL_FALSE);
    glad_glDepthFunc(depth_always ? GL_ALWAYS : GL_LEQUAL);
    glad_glBlendEquation(GL_FUNC_ADD);
    glad_glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}

static void MikuPan_RestoreParticleTriangleState(void)
{
    glad_glBlendEquation(GL_FUNC_ADD);
    glad_glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glad_glDepthFunc(GL_LEQUAL);
    glad_glDepthMask(GL_TRUE);
    MikuPan_ResetRenderStateCache();
}

static int MikuPan_EnsureScreenCopyTexture(int width, int height)
{
    if (width <= 0 || height <= 0)
    {
        return 0;
    }

    if (g_screen_copy_texture != 0 &&
        g_screen_copy_fbo != 0 &&
        g_screen_copy_w == width &&
        g_screen_copy_h == height)
    {
        return 1;
    }

    if (g_screen_copy_texture != 0)
    {
        glad_glDeleteTextures(1, &g_screen_copy_texture);
        g_screen_copy_texture = 0;
        MikuPan_ResetGLBindCache();
    }

    if (g_screen_copy_fbo != 0)
    {
        glad_glDeleteFramebuffers(1, &g_screen_copy_fbo);
        g_screen_copy_fbo = 0;
    }

    g_screen_copy_w = 0;
    g_screen_copy_h = 0;

    glad_glGenTextures(1, &g_screen_copy_texture);
    MikuPan_BindTexture2DCached(g_screen_copy_texture);
    glad_glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8,
                      width, height, 0,
                      GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    glad_glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glad_glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glad_glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glad_glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glad_glGenFramebuffers(1, &g_screen_copy_fbo);
    glad_glBindFramebuffer(GL_FRAMEBUFFER, g_screen_copy_fbo);
    glad_glFramebufferTexture2D(GL_FRAMEBUFFER,
                                GL_COLOR_ATTACHMENT0,
                                GL_TEXTURE_2D,
                                g_screen_copy_texture, 0);

    if (glad_glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
    {
        glad_glDeleteFramebuffers(1, &g_screen_copy_fbo);
        glad_glDeleteTextures(1, &g_screen_copy_texture);
        g_screen_copy_texture = 0;
        g_screen_copy_fbo = 0;
        MikuPan_ResetGLBindCache();
        return 0;
    }

    g_screen_copy_w = width;
    g_screen_copy_h = height;
    return 1;
}

static int MikuPan_UpdateScreenCopyTexture(sceGsTex0 *tex)
{
    int texture_w;
    int texture_h;
    int render_w;
    int render_h;
    int copy_w;
    int copy_h;
    int src_x0;
    int src_x1;
    int src_y0;
    int src_y1;
    float viewport_x;
    float viewport_y;
    float viewport_w;
    float viewport_h;
    float viewport_scale;
    GLint prev_read_fbo = 0;
    GLint prev_draw_fbo = 0;
    GLint prev_viewport[4] = {0};
    GLfloat prev_clear_color[4] = {0.0f};
    GLboolean prev_color_mask[4] = {GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE};
    int ok = 0;

    if (tex == NULL ||
        render_back_msaa.framebuffer_readback.id == 0 ||
        render_back_msaa.framebuffer.id == 0)
    {
        return 0;
    }

    texture_w = 1 << tex->TW;
    texture_h = 1 << tex->TH;
    render_w = render_back_msaa.texture.width;
    render_h = render_back_msaa.texture.height;

    glad_glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &prev_read_fbo);
    glad_glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &prev_draw_fbo);
    glad_glGetIntegerv(GL_VIEWPORT, prev_viewport);
    glad_glGetFloatv(GL_COLOR_CLEAR_VALUE, prev_clear_color);
    glad_glGetBooleanv(GL_COLOR_WRITEMASK, prev_color_mask);

    if (!MikuPan_EnsureScreenCopyTexture(texture_w, texture_h))
    {
        goto restore;
    }

    glad_glBindFramebuffer(GL_READ_FRAMEBUFFER, render_back_msaa.framebuffer_readback.id);
    glad_glBindFramebuffer(GL_DRAW_FRAMEBUFFER, render_back_msaa.framebuffer.id);
    glad_glBlitFramebuffer(0, 0, render_w, render_h,
                           0, 0, render_w, render_h,
                           GL_COLOR_BUFFER_BIT,
                           GL_NEAREST);

    MikuPan_GetPS2Viewport(render_w, render_h,
                           &viewport_x, &viewport_y,
                           &viewport_w, &viewport_h,
                           &viewport_scale);

    copy_w = texture_w < PS2_RESOLUTION_X_INT ? texture_w : PS2_RESOLUTION_X_INT;
    copy_h = texture_h < (int)PS2_CENTER_Y ? texture_h : (int)PS2_CENTER_Y;

    src_x0 = (int)(viewport_x + 0.5f);
    src_x1 = (int)(viewport_x + viewport_w + 0.5f);
    src_y0 = (int)((float)render_h - viewport_y + 0.5f);
    src_y1 = (int)((float)render_h - (viewport_y + viewport_h) + 0.5f);

    glad_glBindFramebuffer(GL_DRAW_FRAMEBUFFER, g_screen_copy_fbo);
    MikuPan_SetViewportCached(0, 0, texture_w, texture_h);

    glad_glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glad_glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glad_glClear(GL_COLOR_BUFFER_BIT);

    glad_glBindFramebuffer(GL_READ_FRAMEBUFFER, render_back_msaa.framebuffer.id);
    glad_glBlitFramebuffer(src_x0, src_y0, src_x1, src_y1,
                           0, 0, copy_w, copy_h,
                           GL_COLOR_BUFFER_BIT,
                           GL_LINEAR);

    glad_glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_TRUE);
    glad_glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glad_glClear(GL_COLOR_BUFFER_BIT);

    ok = 1;

restore:
    glad_glColorMask(prev_color_mask[0], prev_color_mask[1],
                     prev_color_mask[2], prev_color_mask[3]);
    glad_glClearColor(prev_clear_color[0], prev_clear_color[1],
                      prev_clear_color[2], prev_clear_color[3]);
    glad_glBindFramebuffer(GL_READ_FRAMEBUFFER, (GLuint)prev_read_fbo);
    glad_glBindFramebuffer(GL_DRAW_FRAMEBUFFER, (GLuint)prev_draw_fbo);
    MikuPan_SetViewportCached(prev_viewport[0], prev_viewport[1],
                              prev_viewport[2], prev_viewport[3]);
    return ok;
}

void MikuPan_RenderUntexturedTriangles3D(float *buffer, int vertex_count, int depth_always, int additive_blend)
{
    if (vertex_count <= 0)
    {
        return;
    }

    MIKUPAN_PERF_SCOPE(PERF_SECT_SPRITE_RENDER);
    MikuPan_FlushTexturedSpriteBatch();
    MikuPan_SetCurrentShaderProgram(UNTEXTURED_COLOURED_SPRITE_SHADER);
    MikuPan_PipelineInfo *pipeline = MikuPan_GetPipelineInfo(COLOUR4_POSITION4);

    MikuPan_BindVAO(pipeline->vao);
    MikuPan_ApplyParticleTriangleState(depth_always, additive_blend);

    MikuPan_StreamUploadFull(
        GL_ARRAY_BUFFER,
        pipeline->buffers[0].id,
        (GLsizeiptr)(vertex_count * 8 * (int)sizeof(float)),
        buffer);

    MikuPan_TimedDrawArrays(GL_TRIANGLES, 0, vertex_count);
    MikuPan_PerfDrawCall();
    MikuPan_RestoreParticleTriangleState();
}

void MikuPan_RenderScreenCopyTriangles3D(sceGsTex0 *tex, float *buffer, int vertex_count, int depth_always, int additive_blend)
{
    int i;

    (void)additive_blend;

    if (vertex_count <= 0)
    {
        return;
    }

    MikuPan_FlushTexturedSpriteBatch();

    if (!MikuPan_UpdateScreenCopyTexture(tex))
    {
        return;
    }

    MIKUPAN_PERF_SCOPE(PERF_SECT_SPRITE_RENDER);
    MikuPan_SetCurrentShaderProgram(HEAT_HAZE_SHADER);
    MikuPan_SetUniform1iToCurrentShader(0, "uTexture");
    MikuPan_PipelineInfo *pipeline = MikuPan_GetPipelineInfo(UV4_COLOUR4_POSITION4);

    MikuPan_BindVAO(pipeline->vao);
    MikuPan_BindTexture2DCached(g_screen_copy_texture);
    MikuPan_ApplyHeatHazeTriangleState(depth_always);

    g_screen_copy_debug.vertex_count = vertex_count;
    g_screen_copy_debug.depth_always = depth_always;
    g_screen_copy_debug.additive_blend = 0;
    g_screen_copy_debug.uv_min[0] = buffer[0];
    g_screen_copy_debug.uv_max[0] = buffer[0];
    g_screen_copy_debug.uv_min[1] = buffer[1];
    g_screen_copy_debug.uv_max[1] = buffer[1];
    g_screen_copy_debug.ndc_min[0] = buffer[8];
    g_screen_copy_debug.ndc_max[0] = buffer[8];
    g_screen_copy_debug.ndc_min[1] = buffer[9];
    g_screen_copy_debug.ndc_max[1] = buffer[9];

    for (i = 1; i < vertex_count; i++)
    {
        float *v = buffer + i * 12;

        if (v[0] < g_screen_copy_debug.uv_min[0]) g_screen_copy_debug.uv_min[0] = v[0];
        if (v[0] > g_screen_copy_debug.uv_max[0]) g_screen_copy_debug.uv_max[0] = v[0];
        if (v[1] < g_screen_copy_debug.uv_min[1]) g_screen_copy_debug.uv_min[1] = v[1];
        if (v[1] > g_screen_copy_debug.uv_max[1]) g_screen_copy_debug.uv_max[1] = v[1];
        if (v[8] < g_screen_copy_debug.ndc_min[0]) g_screen_copy_debug.ndc_min[0] = v[8];
        if (v[8] > g_screen_copy_debug.ndc_max[0]) g_screen_copy_debug.ndc_max[0] = v[8];
        if (v[9] < g_screen_copy_debug.ndc_min[1]) g_screen_copy_debug.ndc_min[1] = v[9];
        if (v[9] > g_screen_copy_debug.ndc_max[1]) g_screen_copy_debug.ndc_max[1] = v[9];
    }

    MikuPan_StreamUploadFull(
        GL_ARRAY_BUFFER,
        pipeline->buffers[0].id,
        (GLsizeiptr)(vertex_count * 12 * (int)sizeof(float)),
        buffer);

    MikuPan_TimedDrawArrays(GL_TRIANGLES, 0, vertex_count);
    MikuPan_PerfDrawCall();
    MikuPan_RestoreParticleTriangleState();
}

const MikuPan_TextureInfo *MikuPan_GetScreenCopyTextureInfo(void)
{
    static MikuPan_TextureInfo info = {0};

    if (g_screen_copy_texture == 0)
    {
        return NULL;
    }

    info.id = g_screen_copy_texture;
    info.width = g_screen_copy_w;
    info.height = g_screen_copy_h;
    info.tex0 = 0;
    info.hash = 0;

    return &info;
}

const MikuPan_ScreenCopyDebugInfo *MikuPan_GetScreenCopyDebugInfo(void)
{
    if (g_screen_copy_debug.vertex_count <= 0)
    {
        return NULL;
    }

    return &g_screen_copy_debug;
}

void MikuPan_RenderTexturedTriangles3DWithState(sceGsTex0 *tex, float *buffer, int vertex_count, int depth_always, int additive_blend)
{
    if (vertex_count <= 0)
    {
        return;
    }

    MIKUPAN_PERF_SCOPE(PERF_SECT_SPRITE_RENDER);
    MikuPan_FlushTexturedSpriteBatch();
    MikuPan_SetCurrentShaderProgram(SPRITE_SHADER);
    MikuPan_PipelineInfo *pipeline = MikuPan_GetPipelineInfo(UV4_COLOUR4_POSITION4);

    MikuPan_BindVAO(pipeline->vao);
    MikuPan_SetTexture(tex);
    MikuPan_ApplyParticleTriangleState(depth_always, additive_blend);

    MikuPan_StreamUploadFull(
        GL_ARRAY_BUFFER,
        pipeline->buffers[0].id,
        (GLsizeiptr)(vertex_count * 12 * (int)sizeof(float)),
        buffer);

    MikuPan_TimedDrawArrays(GL_TRIANGLES, 0, vertex_count);
    MikuPan_PerfDrawCall();
    MikuPan_RestoreParticleTriangleState();
}
