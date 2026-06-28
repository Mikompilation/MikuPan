#include "mikupan_renderer_internal.h"
#include "graphics/graph2d/message.h"
#include "mikupan/mikupan_logging_c.h"
#include "mikupan/mikupan_utils.h"
#include "mikupan/ui/mikupan_ui.h"
#include "mikupan/ui/mikupan_ui_debug.h"
#include "mikupan_pipeline.h"
#include "mikupan_gpu.h"
#include "mikupan_profiler.h"
#include "mikupan_shader.h"
#include "main/glob.h"
#include <math.h>
#include <string.h>

#define TEXTURED_SPRITE_BATCH_MAX 4096
#define MESSAGE_SPRITE_QUEUE_MAX 32768
#define LATE_2D_OVERLAY_QUEUE_MAX 8192

typedef struct
{
    int active;
    int sprite_count;
    GLuint texture_id;
    int depth_enabled;
} MikuPan_TexturedSpriteBatch;

typedef struct
{
    MikuPan_Rect src;
    MikuPan_Rect dst;
    u_char r;
    u_char g;
    u_char b;
    u_char a;
    float depth;
    int depth_enabled;
    MikuPan_TextureInfo *texture_info;
} MikuPan_QueuedMessageSprite;

typedef enum
{
    MIKUPAN_LATE_2D_TEXTURED,
    MIKUPAN_LATE_2D_UNTEXTURED,
    MIKUPAN_LATE_2D_MESSAGE,
} MikuPan_Late2DOverlayKind;

typedef struct
{
    MikuPan_Late2DOverlayKind kind;
    sceGsTex0 tex;
    union
    {
        float textured[4][12];
        float untextured[4][8];
        MikuPan_QueuedMessageSprite message;
    } vertices;
    int depth_enabled;
    int depth_write;
    unsigned int depth_func;
} MikuPan_Late2DOverlayPrimitive;

static MikuPan_TexturedSpriteBatch g_textured_sprite_batch = {0};
static float g_textured_sprite_cpu_buf[TEXTURED_SPRITE_BATCH_MAX * 6 * 12];
static MikuPan_QueuedMessageSprite
    g_message_sprite_queue[MESSAGE_SPRITE_QUEUE_MAX];
static int g_message_sprite_queue_count = 0;
static MikuPan_Late2DOverlayPrimitive
    g_late_2d_overlay_queue[LATE_2D_OVERLAY_QUEUE_MAX];
static int g_late_2d_overlay_queue_count = 0;
static int g_late_2d_overlay_queue_depth = 0;
static int g_late_2d_overlay_queue_flushing = 0;
static GLuint g_screen_copy_texture = 0;
static int g_screen_copy_w = 0;
static int g_screen_copy_h = 0;
static float g_screen_copy_uv_offset[2] = {0.0f, 0.0f};
static float g_screen_copy_uv_scale[2] = {1.0f, 1.0f};
static float g_screen_copy_content_uv_max[2] = {1.0f, 1.0f};
static MikuPan_ScreenCopyDebugInfo g_screen_copy_debug = {0};

static float MikuPan_NormalizeSpriteDepthValue(float z);
static void MikuPan_CopyAndNormalizeSpriteDepth(float dst[4][12], const float *src);
static void MikuPan_NormalizeUntexturedTriangleDepths(float *buffer, int vertex_count);
static void MikuPan_RenderSpriteInternal(MikuPan_Rect src, MikuPan_Rect dst,
                                         u_char r, u_char g, u_char b,
                                         u_char a,
                                         MikuPan_TextureInfo *texture_info,
                                         float depth, int depth_enabled);

static int MikuPan_IsLate2DOverlayQueueActive(void)
{
    return g_late_2d_overlay_queue_depth > 0 &&
           !g_late_2d_overlay_queue_flushing;
}

static int MikuPan_QueueLate2DTexturedOverlay(sceGsTex0 *tex,
                                              const float *buffer,
                                              int depth_enabled,
                                              int depth_write,
                                              unsigned int depth_func)
{
    if (tex == NULL || buffer == NULL ||
        g_late_2d_overlay_queue_count >= LATE_2D_OVERLAY_QUEUE_MAX)
    {
        return 0;
    }

    MikuPan_Late2DOverlayPrimitive *queued =
        &g_late_2d_overlay_queue[g_late_2d_overlay_queue_count++];
    queued->kind = MIKUPAN_LATE_2D_TEXTURED;
    queued->tex = *tex;
    memcpy(queued->vertices.textured, buffer, sizeof(queued->vertices.textured));
    queued->depth_enabled = depth_enabled;
    queued->depth_write = depth_write;
    queued->depth_func = depth_func;
    return 1;
}

static int MikuPan_QueueLate2DUntexturedOverlay(const float *buffer,
                                                int depth_enabled,
                                                int depth_write,
                                                unsigned int depth_func)
{
    if (buffer == NULL ||
        g_late_2d_overlay_queue_count >= LATE_2D_OVERLAY_QUEUE_MAX)
    {
        return 0;
    }

    MikuPan_Late2DOverlayPrimitive *queued =
        &g_late_2d_overlay_queue[g_late_2d_overlay_queue_count++];
    queued->kind = MIKUPAN_LATE_2D_UNTEXTURED;
    queued->depth_enabled = depth_enabled;
    queued->depth_write = depth_write;
    queued->depth_func = depth_func;
    memcpy(queued->vertices.untextured,
           buffer,
           sizeof(queued->vertices.untextured));
    return 1;
}

static int MikuPan_QueueLate2DMessageOverlay(MikuPan_Rect src,
                                             MikuPan_Rect dst,
                                             u_char r,
                                             u_char g,
                                             u_char b,
                                             u_char a,
                                             float depth,
                                             int depth_enabled,
                                             MikuPan_TextureInfo *texture_info)
{
    if (texture_info == NULL ||
        g_late_2d_overlay_queue_count >= LATE_2D_OVERLAY_QUEUE_MAX)
    {
        return 0;
    }

    MikuPan_Late2DOverlayPrimitive *queued =
        &g_late_2d_overlay_queue[g_late_2d_overlay_queue_count++];
    queued->kind = MIKUPAN_LATE_2D_MESSAGE;
    queued->depth_enabled = depth_enabled;
    queued->depth_write = depth_enabled ? 1 : 0;
    queued->depth_func = GL_LEQUAL;
    queued->vertices.message.src = src;
    queued->vertices.message.dst = dst;
    queued->vertices.message.r = r;
    queued->vertices.message.g = g;
    queued->vertices.message.b = b;
    queued->vertices.message.a = a;
    queued->vertices.message.depth = depth;
    queued->vertices.message.depth_enabled = depth_enabled;
    queued->vertices.message.texture_info = texture_info;
    return 1;
}

void MikuPan_Render2DMessage(DISP_SPRT *sprite)
{
    MikuPan_Rect dst_rect = {0};
    MikuPan_Rect src_rect = {0};
    MikuPan_TextureInfo *texture_info = MikuPan_GetCurrentFontTexture();

    src_rect.x = (float) sprite->u;
    src_rect.y = (float) sprite->v;

    src_rect.w = (float) sprite->w;
    src_rect.h = (float) sprite->h;

    dst_rect.x = (float) sprite->x;
    dst_rect.y = (float) sprite->y;

    dst_rect.w = (float) sprite->w;
    dst_rect.h = (float) sprite->h;

    if (texture_info == NULL)
    {
        return;
    }

    float depth = MikuPan_NormalizeSpriteDepthValue((float)sprite->z);

    if (MikuPan_IsLate2DOverlayQueueActive())
    {
        if (!MikuPan_QueueLate2DMessageOverlay(
                src_rect,
                dst_rect,
                sprite->r,
                sprite->g,
                sprite->b,
                sprite->alpha,
                depth,
                1,
                texture_info))
        {
            info_log("Late 2D message overlay queue overflow");
        }
        return;
    }

    if (g_message_sprite_queue_count >= MESSAGE_SPRITE_QUEUE_MAX)
    {
        info_log("2D message queue overflow; drawing queued text early");
        MikuPan_Flush2DMessageQueue();
    }

    if (g_message_sprite_queue_count < MESSAGE_SPRITE_QUEUE_MAX)
    {
        MikuPan_QueuedMessageSprite *queued =
            &g_message_sprite_queue[g_message_sprite_queue_count++];
        queued->src = src_rect;
        queued->dst = dst_rect;
        queued->r = sprite->r;
        queued->g = sprite->g;
        queued->b = sprite->b;
        queued->a = sprite->alpha;
        queued->depth = depth;
        queued->depth_enabled = 1;
        queued->texture_info = texture_info;
    }
}

void MikuPan_Flush2DMessageQueue(void)
{
    for (int i = 0; i < g_message_sprite_queue_count; i++)
    {
        const MikuPan_QueuedMessageSprite *queued =
            &g_message_sprite_queue[i];

        MikuPan_RenderSpriteInternal(queued->src, queued->dst,
                                     queued->r, queued->g, queued->b,
                                     queued->a, queued->texture_info,
                                     queued->depth,
                                     queued->depth_enabled);
    }

    g_message_sprite_queue_count = 0;
    MikuPan_FlushTexturedSpriteBatch();
}

void MikuPan_BeginLate2DOverlayQueue(void)
{
    g_late_2d_overlay_queue_depth++;
}

void MikuPan_EndLate2DOverlayQueue(void)
{
    if (g_late_2d_overlay_queue_depth > 0)
    {
        g_late_2d_overlay_queue_depth--;
    }
}

void MikuPan_FlushLate2DOverlayQueue(void)
{
    g_late_2d_overlay_queue_flushing = 1;

    for (int i = 0; i < g_late_2d_overlay_queue_count; i++)
    {
        MikuPan_Late2DOverlayPrimitive *queued =
            &g_late_2d_overlay_queue[i];

        if (queued->kind == MIKUPAN_LATE_2D_TEXTURED)
        {
            if (queued->depth_enabled)
            {
                MikuPan_RenderSprite2DDepthState(
                    &queued->tex,
                    &queued->vertices.textured[0][0],
                    queued->depth_enabled,
                    queued->depth_write,
                    queued->depth_func);
            }
            else
            {
                MikuPan_RenderSprite2D(&queued->tex,
                                       &queued->vertices.textured[0][0]);
            }
        }
        else if (queued->kind == MIKUPAN_LATE_2D_MESSAGE)
        {
            MikuPan_QueuedMessageSprite *message =
                &queued->vertices.message;

            MikuPan_RenderSpriteInternal(message->src, message->dst,
                                         message->r, message->g, message->b,
                                         message->a, message->texture_info,
                                         message->depth,
                                         message->depth_enabled);
        }
        else
        {
            if (queued->depth_enabled)
            {
                MikuPan_RenderUntexturedSpriteDepthState(
                    &queued->vertices.untextured[0][0],
                    queued->depth_enabled,
                    queued->depth_write,
                    queued->depth_func);
            }
            else
            {
                MikuPan_RenderUntexturedSprite(
                    &queued->vertices.untextured[0][0]);
            }
        }
    }

    g_late_2d_overlay_queue_count = 0;
    g_late_2d_overlay_queue_flushing = 0;
    MikuPan_FlushTexturedSpriteBatch();
}

void MikuPan_RenderLine(float x1, float y1, float x2, float y2, u_char r,
                        u_char g, u_char b, u_char a)
{
    MikuPan_FlushTexturedSpriteBatch();
    const float render_w = (float)MikuPan_GetRenderResolutionWidth();
    const float render_h = (float)MikuPan_GetRenderResolutionHeight();
    const float dx = x2 - x1;
    const float dy = y2 - y1;
    const float len = sqrtf(dx * dx + dy * dy);
    float ox = 0.5f;
    float oy = 0.5f;
    float ndc[4][2];

    float colours[4] = {
        MikuPan_ConvertScaleColor(r),
        MikuPan_ConvertScaleColor(g),
        MikuPan_ConvertScaleColor(b),
        MikuPan_ConvertScaleColor(a)
    };

    if (len > 0.00001f)
    {
        ox = (-dy / len) * 0.5f;
        oy = ( dx / len) * 0.5f;
    }

    MikuPan_ConvertPs2HalfScreenCoordToNDCMaintainAspectRatio(
        ndc[0], render_w, render_h, x1 - ox, y1 - oy);
    MikuPan_ConvertPs2HalfScreenCoordToNDCMaintainAspectRatio(
        ndc[1], render_w, render_h, x1 + ox, y1 + oy);
    MikuPan_ConvertPs2HalfScreenCoordToNDCMaintainAspectRatio(
        ndc[2], render_w, render_h, x2 - ox, y2 - oy);
    MikuPan_ConvertPs2HalfScreenCoordToNDCMaintainAspectRatio(
        ndc[3], render_w, render_h, x2 + ox, y2 + oy);

    float vertices[6][8] =
    {
        { colours[0], colours[1], colours[2], colours[3], ndc[0][0], ndc[0][1], 0.0f, 1.0f},
        { colours[0], colours[1], colours[2], colours[3], ndc[1][0], ndc[1][1], 0.0f, 1.0f},
        { colours[0], colours[1], colours[2], colours[3], ndc[2][0], ndc[2][1], 0.0f, 1.0f},
        { colours[0], colours[1], colours[2], colours[3], ndc[2][0], ndc[2][1], 0.0f, 1.0f},
        { colours[0], colours[1], colours[2], colours[3], ndc[1][0], ndc[1][1], 0.0f, 1.0f},
        { colours[0], colours[1], colours[2], colours[3], ndc[3][0], ndc[3][1], 0.0f, 1.0f},
    };

    MikuPan_SetCurrentShaderProgram(UNTEXTURED_COLOURED_SPRITE_SHADER);
    MikuPan_PipelineInfo* pipeline = MikuPan_GetPipelineInfo(COLOUR4_POSITION4);

    MikuPan_BindVAO(pipeline->vao);
    MikuPan_StreamUploadFull(
        GL_ARRAY_BUFFER, pipeline->buffers[0].id,
        (GLsizeiptr)sizeof(vertices), vertices);

    MikuPan_SetRenderState2D();

    MikuPan_TimedDrawArrays(GL_TRIANGLES, 0, 6);
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
    MikuPan_StreamUploadFull(
        GL_ARRAY_BUFFER, pipeline->buffers[0].id,
        (GLsizeiptr)pipeline->buffers[0].buffer_length,
        vertices);

    for (int i = 0; i < 6; i++)
    {
        MikuPan_TimedDrawArrays(GL_LINE_LOOP, i * 4, 4);
    }
}

void MikuPan_FlushTexturedSpriteBatch(void)
{
    MIKUPAN_PERF_SCOPE(PERF_SECT_BATCH_FLUSH);

    if (!g_textured_sprite_batch.active || g_textured_sprite_batch.sprite_count == 0)
    {
        g_textured_sprite_batch.active = 0;
        g_textured_sprite_batch.sprite_count = 0;
        g_textured_sprite_batch.depth_enabled = 0;
        return;
    }

    MikuPan_SetCurrentShaderProgram(SPRITE_SHADER);
    MikuPan_PipelineInfo *pipeline = MikuPan_GetPipelineInfo(UV4_COLOUR4_POSITION4);
    MikuPan_BindVAO(pipeline->vao);
    if (g_textured_sprite_batch.depth_enabled)
    {
        MikuPan_SetRenderState2DDepth();
    }
    else
    {
        MikuPan_SetRenderState2D();
    }

    MikuPan_ActiveTextureCached(GL_TEXTURE0);
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
    g_textured_sprite_batch.depth_enabled = 0;
}

static void MikuPan_RenderSpriteInternal(MikuPan_Rect src, MikuPan_Rect dst,
                                         u_char r, u_char g, u_char b,
                                         u_char a,
                                         MikuPan_TextureInfo *texture_info,
                                         float depth, int depth_enabled)
{
    MIKUPAN_PERF_SCOPE(PERF_SECT_SPRITE_RENDER);

    if (texture_info == NULL)
    {
        info_log("Cannot render texture due texture_info being NULL");
        return;
    }

    float ndc[4] = {0};
    MikuPan_ConvertPs2ScreenCoordToNDCMaintainAspectRatio(ndc, (float)MikuPan_GetRenderResolutionWidth(), (float)MikuPan_GetRenderResolutionHeight(), dst.x, dst.y);
    MikuPan_ConvertPs2ScreenCoordToNDCMaintainAspectRatio(&ndc[2], (float)MikuPan_GetRenderResolutionWidth(), (float)MikuPan_GetRenderResolutionHeight(), dst.x + src.w, dst.y + src.h);

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
         g_textured_sprite_batch.depth_enabled != depth_enabled ||
         g_textured_sprite_batch.sprite_count >= TEXTURED_SPRITE_BATCH_MAX))
    {
        MikuPan_FlushTexturedSpriteBatch();
    }

    if (!g_textured_sprite_batch.active)
    {
        g_textured_sprite_batch.active = 1;
        g_textured_sprite_batch.texture_id = texture_info->id;
        g_textured_sprite_batch.depth_enabled = depth_enabled;
        g_textured_sprite_batch.sprite_count = 0;
    }

    // Append 6 vertices (two triangles forming the quad: TL, BL, TR, TR, BL, BR).
    float *vp = g_textured_sprite_cpu_buf + g_textured_sprite_batch.sprite_count * 6 * 12;

    #define WRITE_VERT(idx, vu, vv, vx, vy) do { \
        float *p = vp + (idx) * 12; \
        p[0] = (vu); p[1] = (vv); p[2] = 0.0f; p[3] = 0.0f; \
        p[4] = c0;   p[5] = c1;   p[6] = c2;   p[7] = c3;   \
        p[8] = (vx); p[9] = (vy); p[10] = (depth); p[11] = 1.0f; \
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

void MikuPan_RenderSprite(MikuPan_Rect src, MikuPan_Rect dst, u_char r,
                          u_char g, u_char b, u_char a,
                          MikuPan_TextureInfo *texture_info)
{
    MikuPan_RenderSpriteInternal(src, dst, r, g, b, a, texture_info, 0.0f, 0);
}

static void MikuPan_RenderSprite2DInternal(sceGsTex0 *tex, float *buffer,
                                           int depth_enabled,
                                           int depth_write,
                                           unsigned int depth_func,
                                           int shader,
                                           MikuPan_GPUBlendMode blend_mode,
                                           int nearest_sampler)
{
    float upload_buffer[4][12];

    MIKUPAN_PERF_SCOPE(PERF_SECT_SPRITE_RENDER);

    if (MikuPan_IsLate2DOverlayQueueActive()
        && shader == SPRITE_SHADER
        && blend_mode == MIKUPAN_GPU_BLEND_NORMAL
        && nearest_sampler == 0)
    {
        if (!MikuPan_QueueLate2DTexturedOverlay(tex, buffer, depth_enabled,
                                                depth_write, depth_func))
        {
            info_log("Late 2D textured overlay queue overflow");
        }
        return;
    }

    MikuPan_FlushTexturedSpriteBatch();
    MikuPan_SetCurrentShaderProgram(shader);
    MikuPan_PipelineInfo* pipeline = MikuPan_GetPipelineInfo(UV4_COLOUR4_POSITION4);

    MikuPan_BindVAO(pipeline->vao);
    MikuPan_SetTexture(tex);

    if (depth_enabled)
    {
        MikuPan_CopyAndNormalizeSpriteDepth(upload_buffer, buffer);
        MikuPan_SetRenderState2DDepth();
        MikuPan_GPUSetDepthWrite(depth_write);
        MikuPan_GPUSetDepthFunc(depth_func);
    }
    else
    {
        MikuPan_SetRenderState2D();
    }

    MikuPan_GPUSetBlendMode(1, blend_mode);
    // Caller passes a 4-vert quad worth of data (UV4_COLOUR4_POSITION4 layout).
    MikuPan_StreamUploadFull(GL_ARRAY_BUFFER, pipeline->buffers[0].id,
                             (GLsizeiptr)sizeof(float[4][12]),
                             depth_enabled ? &upload_buffer[0][0] : buffer);

    MikuPan_GPUSetSamplerNearestOverride(nearest_sampler);
    MikuPan_TimedDrawArrays(MikuPan_GetRenderMode(), 0, 4);
    MikuPan_GPUSetSamplerNearestOverride(0);

    if (shader != SPRITE_SHADER || blend_mode != MIKUPAN_GPU_BLEND_NORMAL)
    {
        MikuPan_GPUSetBlendMode(1, MIKUPAN_GPU_BLEND_NORMAL);
        MikuPan_SetCurrentShaderProgram(SPRITE_SHADER);
    }
}

void MikuPan_RenderSprite2D(sceGsTex0 *tex, float *buffer)
{
    MikuPan_RenderSprite2DInternal(tex, buffer, 0, 0, GL_LEQUAL,
                                   SPRITE_SHADER,
                                   MIKUPAN_GPU_BLEND_NORMAL, 0);
}

void MikuPan_RenderSprite2DDepth(sceGsTex0 *tex, float *buffer)
{
    MikuPan_RenderSprite2DInternal(tex, buffer, 1, 1, GL_LEQUAL,
                                   SPRITE_SHADER,
                                   MIKUPAN_GPU_BLEND_NORMAL, 0);
}

void MikuPan_RenderSprite2DDepthState(sceGsTex0 *tex, float *buffer,
                                      int depth_test, int depth_write,
                                      unsigned int depth_func)
{
    MikuPan_RenderSprite2DInternal(tex, buffer, depth_test, depth_write,
                                   depth_func, SPRITE_SHADER,
                                   MIKUPAN_GPU_BLEND_NORMAL, 0);
}

void MikuPan_RenderSprite2DDepthStateFiltered(sceGsTex0 *tex, float *buffer,
                                             int depth_test, int depth_write,
                                             unsigned int depth_func,
                                             int nearest_sampler)
{
    MikuPan_RenderSprite2DInternal(tex, buffer, depth_test, depth_write,
                                   depth_func, SPRITE_SHADER,
                                   MIKUPAN_GPU_BLEND_NORMAL,
                                   nearest_sampler);
}

static void MikuPan_RenderSprite2DGSAlphaInternal(
    sceGsTex0 *tex, float *buffer, int depth_test, int depth_write,
    unsigned int depth_func, int nearest_sampler, unsigned long gs_alpha)
{
    switch (gs_alpha & 0xff)
    {
        case MIKUPAN_GS_ALPHA_DST_MINUS_SRC:
            /*
             * GS ALPHA 0x41:
             *   (Cd - Cs) * As + Cd
             * = Cd + Cd * As - Cs * As
             *
             * Pass 1: Cd + Cd * As
             * Pass 2: Cd - Cs * As
             */
            MikuPan_RenderSprite2DInternal(
                tex, buffer, depth_test, depth_write, depth_func,
                SPRITE_ALPHA_AS_RGB_SHADER, MIKUPAN_GPU_BLEND_SRC_TIMES_DST_ADD,
                nearest_sampler);
            MikuPan_RenderSprite2DInternal(
                tex, buffer, depth_test, depth_write, depth_func, SPRITE_SHADER,
                MIKUPAN_GPU_BLEND_SUBTRACTIVE, nearest_sampler);
            break;

        case MIKUPAN_GS_ALPHA_SUBTRACTIVE:
            /* Cd - Cs * As */
            MikuPan_RenderSprite2DInternal(
                tex, buffer, depth_test, depth_write, depth_func, SPRITE_SHADER,
                MIKUPAN_GPU_BLEND_SUBTRACTIVE, nearest_sampler);
            break;

        case MIKUPAN_GS_ALPHA_ADDITIVE:
            /* Cs * As + Cd */
            MikuPan_RenderSprite2DInternal(
                tex, buffer, depth_test, depth_write, depth_func, SPRITE_SHADER,
                MIKUPAN_GPU_BLEND_ADDITIVE, nearest_sampler);
            break;

        case MIKUPAN_GS_ALPHA_DST_BOOST:
            /* Cd * As + Cd */
            MikuPan_RenderSprite2DInternal(
                tex, buffer, depth_test, depth_write, depth_func,
                SPRITE_ALPHA_AS_RGB_SHADER, MIKUPAN_GPU_BLEND_SRC_TIMES_DST_ADD,
                nearest_sampler);
            break;

        case MIKUPAN_GS_ALPHA_NORMAL:
        default:
            MikuPan_RenderSprite2DInternal(
                tex, buffer, depth_test, depth_write, depth_func, SPRITE_SHADER,
                MIKUPAN_GPU_BLEND_NORMAL, nearest_sampler);
            break;
    }
}

void MikuPan_RenderSprite2DGSAlpha(sceGsTex0 *tex, float *buffer,
                                   unsigned long gs_alpha)
{
    MikuPan_RenderSprite2DGSAlphaInternal(tex, buffer, 0, 0, GL_LEQUAL, 0,
                                          gs_alpha);
}

void MikuPan_RenderSprite2DFilteredGSAlpha(sceGsTex0 *tex, float *buffer,
                                           int nearest_sampler,
                                           unsigned long gs_alpha)
{
    MikuPan_RenderSprite2DGSAlphaInternal(tex, buffer, 0, 0, GL_LEQUAL,
                                          nearest_sampler, gs_alpha);
}

void MikuPan_RenderSprite2DDepthStateFilteredGSAlpha(
    sceGsTex0 *tex, float *buffer, int depth_test, int depth_write,
    unsigned int depth_func, int nearest_sampler, unsigned long gs_alpha)
{
    MikuPan_RenderSprite2DGSAlphaInternal(tex, buffer, depth_test, depth_write,
                                          depth_func, nearest_sampler,
                                          gs_alpha);
}

static float MikuPan_NormalizeSpriteDepthValue(float z)
{
    if (z >= -1.0f && z <= 1.0f)
    {
        return z;
    }

    return MikuPan_ConvertGsDepthToNDC(z);
}

static void MikuPan_CopyAndNormalizeSpriteDepth(float dst[4][12], const float *src)
{
    int i;
    int j;

    for (i = 0; i < 4; i++)
    {
        for (j = 0; j < 12; j++)
        {
            dst[i][j] = src[i * 12 + j];
        }

        dst[i][10] = MikuPan_NormalizeSpriteDepthValue(dst[i][10]);
    }
}

static void MikuPan_NormalizeTexturedTriangleDepths(float *buffer, int vertex_count)
{
    int i;

    for (i = 0; i < vertex_count; i++)
    {
        float *vertex = buffer + i * 12;
        vertex[10] = MikuPan_NormalizeSpriteDepthValue(vertex[10]);
    }
}

static void MikuPan_NormalizeUntexturedTriangleDepths(float *buffer, int vertex_count)
{
    int i;

    for (i = 0; i < vertex_count; i++)
    {
        float *vertex = buffer + i * 8;
        vertex[6] = MikuPan_NormalizeSpriteDepthValue(vertex[6]);
    }
}

static void MikuPan_RenderUntexturedSpriteInternal(float *buffer,
                                                   int depth_enabled,
                                                   int depth_write,
                                                   unsigned int depth_func)
{
    float upload_buffer[4][8];

    MIKUPAN_PERF_SCOPE(PERF_SECT_SPRITE_RENDER);

    if (MikuPan_IsLate2DOverlayQueueActive())
    {
        if (!MikuPan_QueueLate2DUntexturedOverlay(buffer, depth_enabled,
                                                  depth_write, depth_func))
        {
            info_log("Late 2D untextured overlay queue overflow");
        }
        return;
    }

    MikuPan_FlushTexturedSpriteBatch();
    MikuPan_SetCurrentShaderProgram(UNTEXTURED_COLOURED_SPRITE_SHADER);
    MikuPan_PipelineInfo* pipeline = MikuPan_GetPipelineInfo(COLOUR4_POSITION4);

    MikuPan_BindVAO(pipeline->vao);

    if (depth_enabled)
    {
        memcpy(upload_buffer, buffer, sizeof(upload_buffer));
        MikuPan_NormalizeUntexturedTriangleDepths(&upload_buffer[0][0], 4);
        MikuPan_SetRenderState2DDepth();
        MikuPan_GPUSetDepthWrite(depth_write);
        MikuPan_GPUSetDepthFunc(depth_func);
    }
    else
    {
        MikuPan_SetRenderState2D();
    }

    // Caller passes a 4-vert quad worth of data (COLOUR4_POSITION4 layout: 8 floats / vert).
    MikuPan_StreamUploadFull(
        GL_ARRAY_BUFFER, pipeline->buffers[0].id,
        (GLsizeiptr)sizeof(float[4][8]),
        depth_enabled ? &upload_buffer[0][0] : buffer);

    MikuPan_TimedDrawArrays(MikuPan_GetRenderMode(), 0, 4);
}

void MikuPan_RenderUntexturedSprite(float *buffer)
{
    MikuPan_RenderUntexturedSpriteInternal(buffer, 0, 0, GL_LEQUAL);
}

void MikuPan_RenderUntexturedSpriteDepth(float *buffer)
{
    MikuPan_RenderUntexturedSpriteInternal(buffer, 1, 1, GL_LEQUAL);
}

void MikuPan_RenderUntexturedSpriteDepthState(float *buffer, int depth_test,
                                              int depth_write,
                                              unsigned int depth_func)
{
    MikuPan_RenderUntexturedSpriteInternal(buffer, depth_test, depth_write,
                                           depth_func);
}

static void MikuPan_RenderSprite3DInternal(sceGsTex0* tex, float* buffer,
                                             MikuPan_GPUBlendMode blend_mode)
{
    float upload_buffer[4][12];

    MIKUPAN_PERF_SCOPE(PERF_SECT_SPRITE_RENDER);
    MikuPan_FlushTexturedSpriteBatch();
    MikuPan_SetCurrentShaderProgram(SPRITE_SHADER);
    MikuPan_PipelineInfo* pipeline = MikuPan_GetPipelineInfo(UV4_COLOUR4_POSITION4);

    MikuPan_CopyAndNormalizeSpriteDepth(upload_buffer, buffer);

    MikuPan_BindVAO(pipeline->vao);
    MikuPan_SetTexture(tex);

    MikuPan_SetRenderStateSprite3D();
    MikuPan_GPUSetDepthWrite(0);
    MikuPan_GPUSetDepthFunc(GL_LEQUAL);
    MikuPan_GPUSetBlendMode(1, blend_mode);

    MikuPan_StreamUploadFull(
        GL_ARRAY_BUFFER, pipeline->buffers[0].id,
        (GLsizeiptr)sizeof(upload_buffer), upload_buffer);

    MikuPan_TimedDrawArrays(MikuPan_GetRenderMode(), 0, 4);
    MikuPan_GPUSetBlendMode(1, MIKUPAN_GPU_BLEND_NORMAL);
    MikuPan_GPUSetDepthWrite(1);
    MikuPan_ResetRenderStateCache();
}

void MikuPan_RenderSprite3D(sceGsTex0* tex, float* buffer)
{
    MikuPan_RenderSprite3DInternal(tex, buffer, MIKUPAN_GPU_BLEND_NORMAL);
}

void MikuPan_RenderSprite3DWithState(sceGsTex0* tex, float* buffer,
                                     MikuPan_GPUBlendMode blend_mode)
{
    MikuPan_RenderSprite3DInternal(tex, buffer, blend_mode);
}

void MikuPan_RenderSprite3DWithStateGSAlpha(sceGsTex0* tex, float* buffer,
                                            unsigned long gs_alpha)
{
    MikuPan_RenderSprite3DWithState(
        tex, buffer, MikuPan_GPUBlendModeFromGSAlpha(gs_alpha));
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
    MikuPan_GPUSetDepthWrite(0);
    MikuPan_GPUSetDepthFunc(GL_LEQUAL);
    MikuPan_NormalizeTexturedTriangleDepths(buffer, vertex_count);

    MikuPan_StreamUploadFull(
        GL_ARRAY_BUFFER,
        pipeline->buffers[0].id,
        (GLsizeiptr)(vertex_count * 12 * (int)sizeof(float)),
        buffer);

    MikuPan_TimedDrawArrays(GL_TRIANGLES, 0, vertex_count);
    MikuPan_PerfDrawCall();
    MikuPan_GPUSetDepthWrite(1);
    MikuPan_ResetRenderStateCache();
}

static GLenum MikuPan_GetDepthFuncForMode(int depth_mode)
{
    switch (depth_mode)
    {
    case MIKUPAN_DEPTH_ALWAYS:
        return GL_ALWAYS;
    case MIKUPAN_DEPTH_GEQUAL:
        return GL_GEQUAL;
    case MIKUPAN_DEPTH_LEQUAL:
    default:
        return GL_LEQUAL;
    }
}

static void MikuPan_ApplyParticleTriangleState(int depth_mode, MikuPan_GPUBlendMode blend_mode)
{
    MikuPan_SetRenderStateSprite3D();
    MikuPan_GPUSetDepthWrite(0);
    MikuPan_GPUSetDepthFunc(MikuPan_GetDepthFuncForMode(depth_mode));
    MikuPan_GPUSetBlendMode(1, blend_mode);
}

static void MikuPan_ApplyHeatHazeTriangleState(int depth_mode, MikuPan_GPUBlendMode blend_mode)
{
    MikuPan_SetRenderStateSprite3D();
    MikuPan_GPUSetDepthWrite(0);
    MikuPan_GPUSetDepthFunc(MikuPan_GetDepthFuncForMode(depth_mode));
    MikuPan_GPUSetBlendMode(1, blend_mode);
}

static void MikuPan_RestoreParticleTriangleState(void)
{
    MikuPan_GPUSetBlendMode(1, MIKUPAN_GPU_BLEND_NORMAL);
    MikuPan_GPUSetDepthFunc(GL_LEQUAL);
    MikuPan_GPUSetDepthWrite(1);
    MikuPan_ResetRenderStateCache();
}

static int MikuPan_EnsureScreenCopyTexture(int width, int height)
{
    if (width <= 0 || height <= 0)
    {
        return 0;
    }

    if (g_screen_copy_texture != 0 &&
        g_screen_copy_w == width &&
        g_screen_copy_h == height)
    {
        return 1;
    }

    if (g_screen_copy_texture != 0)
    {
        MikuPan_GPUReleaseTexture(g_screen_copy_texture);
        g_screen_copy_texture = 0;
        MikuPan_ResetGLBindCache();
    }

    g_screen_copy_w = 0;
    g_screen_copy_h = 0;

    g_screen_copy_texture = MikuPan_GPUCreateRenderTextureRGBA8(width, height);
    if (g_screen_copy_texture == 0)
    {
        MikuPan_ResetGLBindCache();
        return 0;
    }

    g_screen_copy_w = width;
    g_screen_copy_h = height;
    return 1;
}

static int MikuPan_IsPs2MainFramebufferAddress(int addr)
{
    return addr == 0 || addr == (224 * (640 / 64));
}

static int MikuPan_IsPs2DisplayedFramebufferAddress(int addr)
{
    return MikuPan_IsPs2MainFramebufferAddress(addr) &&
           addr == (int)((sys_wrk.count & 1) * (224 * (640 / 64)));
}

static int MikuPan_IsPs2NamedFeedbackCopyAddress(int addr)
{
    return addr == 0x1a40;
}

static int g_force_feedback_screen_copy_source = 0;

static unsigned int MikuPan_GetScreenCopySourceTexture(sceGsTex0 *tex)
{
    if (tex != NULL &&
        (g_force_feedback_screen_copy_source ||
         MikuPan_IsPs2DisplayedFramebufferAddress(tex->TBP0)))
    {
        unsigned int feedback = MikuPan_GetSceneFeedbackTextureId(0);
        if (feedback != 0)
        {
            return feedback;
        }
    }

    /* 0x1A40 is the old PS2 scratch framebuffer address, but in MikuPan it
     * is just a named live screen copy used by haze/menu/loading paths.
     * DO NOT assume every 0x1A40 texture is feedback history.
     *
     * Effect-specific feedbacks can opt in with g_force_feedback_screen_copy_source. */
    return render_back_msaa.texture.id;
}

static int MikuPan_UpdateScreenCopyTexture(sceGsTex0 *tex)
{
    int texture_w;
    int texture_h;
    int render_w;
    int render_h;
    int copy_w;
    int copy_h;
    float viewport_x;
    float viewport_y;
    float viewport_w;
    float viewport_h;
    float viewport_scale;
    unsigned int source_texture;

    if (tex == NULL || render_back_msaa.texture.id == 0)
    {
        return 0;
    }

    source_texture = MikuPan_GetScreenCopySourceTexture(tex);
    if (source_texture == 0)
    {
        return 0;
    }

    texture_w = 1 << tex->TW;
    texture_h = 1 << tex->TH;
    render_w = render_back_msaa.texture.width;
    render_h = render_back_msaa.texture.height;

    if (render_w <= 0 || render_h <= 0)
    {
        return 0;
    }

    MikuPan_GetPS2Viewport(render_w, render_h,
                           &viewport_x, &viewport_y,
                           &viewport_w, &viewport_h,
                           &viewport_scale);

    copy_w = render_w;
    copy_h = render_h;

    if (!MikuPan_EnsureScreenCopyTexture(copy_w, copy_h))
    {
        return 0;
    }

    g_screen_copy_uv_offset[0] = viewport_x / (float)render_w;
    g_screen_copy_uv_offset[1] = viewport_y / (float)render_h;
    g_screen_copy_uv_scale[0] =
        (viewport_w / (float)render_w) *
        ((float)texture_w / PS2_RESOLUTION_X_FLOAT);
    g_screen_copy_uv_scale[1] =
        (viewport_h / (float)render_h) *
        ((float)texture_h / PS2_CENTER_Y);

    g_screen_copy_content_uv_max[0] =
        g_screen_copy_uv_offset[0] +
        (PS2_RESOLUTION_X_FLOAT / (float)texture_w) * g_screen_copy_uv_scale[0];
    g_screen_copy_content_uv_max[1] =
        g_screen_copy_uv_offset[1] +
        (PS2_CENTER_Y / (float)texture_h) * g_screen_copy_uv_scale[1];

    MikuPan_GPUCopyTexture(source_texture,
                           g_screen_copy_texture,
                           copy_w,
                           copy_h);
    return 1;
}

void MikuPan_RenderUntexturedTriangles3D(float *buffer, int vertex_count, int depth_mode, MikuPan_GPUBlendMode blend_mode)
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
    MikuPan_ApplyParticleTriangleState(depth_mode, blend_mode);
    MikuPan_NormalizeUntexturedTriangleDepths(buffer, vertex_count);

    MikuPan_StreamUploadFull(
        GL_ARRAY_BUFFER,
        pipeline->buffers[0].id,
        (GLsizeiptr)(vertex_count * 8 * (int)sizeof(float)),
        buffer);

    MikuPan_TimedDrawArrays(GL_TRIANGLES, 0, vertex_count);
    MikuPan_PerfDrawCall();
    MikuPan_RestoreParticleTriangleState();
}

void MikuPan_RenderSolidUntexturedTriangles3D(float *buffer, int vertex_count, int depth_mode)
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
    MikuPan_SetRenderState3D();
    MikuPan_GPUSetCullNone();
    MikuPan_GPUSetDepthWrite(1);
    MikuPan_GPUSetDepthFunc(MikuPan_GetDepthFuncForMode(depth_mode));
    MikuPan_GPUSetBlendMode(1, MIKUPAN_GPU_BLEND_NORMAL);
    MikuPan_NormalizeUntexturedTriangleDepths(buffer, vertex_count);

    MikuPan_StreamUploadFull(
        GL_ARRAY_BUFFER,
        pipeline->buffers[0].id,
        (GLsizeiptr)(vertex_count * 8 * (int)sizeof(float)),
        buffer);

    MikuPan_TimedDrawArrays(GL_TRIANGLES, 0, vertex_count);
    MikuPan_PerfDrawCall();

    MikuPan_GPUSetDepthFunc(GL_LEQUAL);
    MikuPan_GPUSetDepthWrite(1);
    MikuPan_GPUSetCullBack();
    MikuPan_ResetRenderStateCache();
}

void MikuPan_RenderScreenCopyTriangles3D(sceGsTex0 *tex, float *buffer, int vertex_count, int depth_mode, MikuPan_GPUBlendMode blend_mode)
{
    int i;

    if (vertex_count <= 0)
    {
        return;
    }

    if (MikuPan_ArePhotoCaptureScreenCopyEffectsSuppressed())
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
    /*
     * DO NOT apply the global monochrome flag to framebuffer/effect copies!!
     * The original game handles monochrome through scene/material/CLUT.
     * Late additive effects like the blue save lamp must be able to remain coloured.
     */
    MikuPan_SetUniform1iToCurrentShader(0, "uBlackWhiteMode");
    MikuPan_SetUniform2fToCurrentShader(g_screen_copy_uv_offset[0],
                                        g_screen_copy_uv_offset[1],
                                        "uFramebufferUvOffset");
    MikuPan_SetUniform2fToCurrentShader(g_screen_copy_uv_scale[0],
                                        g_screen_copy_uv_scale[1],
                                        "uFramebufferUvScale");
    MikuPan_SetUniform2fToCurrentShader(g_screen_copy_content_uv_max[0],
                                        g_screen_copy_content_uv_max[1],
                                        "uFramebufferContentUvMax");
    MikuPan_SetUniform1iToCurrentShader(0, "uUseScreenPos");
    MikuPan_PipelineInfo *pipeline = MikuPan_GetPipelineInfo(UV4_COLOUR4_POSITION4);

    MikuPan_BindVAO(pipeline->vao);
    MikuPan_ActiveTextureCached(GL_TEXTURE0);
    MikuPan_BindTexture2DCached(g_screen_copy_texture);
    MikuPan_ApplyHeatHazeTriangleState(depth_mode, blend_mode);
    MikuPan_NormalizeTexturedTriangleDepths(buffer, vertex_count);

    g_screen_copy_debug.vertex_count = vertex_count;
    g_screen_copy_debug.depth_mode = depth_mode;
    g_screen_copy_debug.blend_mode = blend_mode;
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

void MikuPan_RenderScreenCopyTriangles3DGSAlpha(sceGsTex0 *tex,
                                                float *buffer,
                                                int vertex_count,
                                                int depth_mode,
                                                unsigned long gs_alpha)
{
    MikuPan_RenderScreenCopyTriangles3D(
        tex, buffer, vertex_count, depth_mode,
        MikuPan_GPUBlendModeFromGSAlpha(gs_alpha));
}


typedef enum MikuPan_ScreenCopyScreenPosMode
{
    MIKUPAN_SCREEN_COPY_SCREEN_POS_NORMAL = 0,
    MIKUPAN_SCREEN_COPY_SCREEN_POS_MODULATE = 1,
} MikuPan_ScreenCopyScreenPosMode;

static MikuPan_ScreenCopyScreenPosMode MikuPan_ScreenCopyScreenPosModeFromTex(const sceGsTex0 *tex)
{
    if (tex != NULL && tex->TFX == SCE_GS_MODULATE)
    {
        return MIKUPAN_SCREEN_COPY_SCREEN_POS_MODULATE;
    }

    return MIKUPAN_SCREEN_COPY_SCREEN_POS_NORMAL;
}

static void MikuPan_RenderScreenCopyTriangles3DScreenPosMode(sceGsTex0 *tex,
                                                              float *buffer,
                                                              int vertex_count,
                                                              int depth_mode,
                                                              MikuPan_GPUBlendMode blend_mode,
                                                              MikuPan_ScreenCopyScreenPosMode screen_pos_mode)
{
    if (vertex_count <= 0)
    {
        return;
    }

    if (MikuPan_ArePhotoCaptureScreenCopyEffectsSuppressed())
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
    MikuPan_SetUniform1iToCurrentShader(0, "uBlackWhiteMode");
    MikuPan_SetUniform2fToCurrentShader(g_screen_copy_uv_offset[0],
                                        g_screen_copy_uv_offset[1],
                                        "uFramebufferUvOffset");
    MikuPan_SetUniform2fToCurrentShader(g_screen_copy_uv_scale[0],
                                        g_screen_copy_uv_scale[1],
                                        "uFramebufferUvScale");
    MikuPan_SetUniform2fToCurrentShader(g_screen_copy_content_uv_max[0],
                                        g_screen_copy_content_uv_max[1],
                                        "uFramebufferContentUvMax");
    MikuPan_SetUniform1iToCurrentShader(
        screen_pos_mode == MIKUPAN_SCREEN_COPY_SCREEN_POS_MODULATE ? 4 : 1,
        "uUseScreenPos");
    MikuPan_SetUniform2fToCurrentShader((float)g_screen_copy_w,
                                        (float)g_screen_copy_h,
                                        "uRenderSize");
    MikuPan_PipelineInfo *pipeline = MikuPan_GetPipelineInfo(UV4_COLOUR4_POSITION4);

    MikuPan_BindVAO(pipeline->vao);
    MikuPan_ActiveTextureCached(GL_TEXTURE0);
    MikuPan_BindTexture2DCached(g_screen_copy_texture);
    MikuPan_ApplyHeatHazeTriangleState(depth_mode, blend_mode);
    MikuPan_NormalizeTexturedTriangleDepths(buffer, vertex_count);

    MikuPan_StreamUploadFull(
        GL_ARRAY_BUFFER,
        pipeline->buffers[0].id,
        (GLsizeiptr)(vertex_count * 12 * (int)sizeof(float)),
        buffer);

    MikuPan_TimedDrawArrays(GL_TRIANGLES, 0, vertex_count);
    MikuPan_PerfDrawCall();
    MikuPan_RestoreParticleTriangleState();
}

void MikuPan_RenderScreenCopyTriangles3DScreenPos(sceGsTex0 *tex,
                                                   float *buffer,
                                                   int vertex_count,
                                                   int depth_mode,
                                                   MikuPan_GPUBlendMode blend_mode)
{
    MikuPan_RenderScreenCopyTriangles3DScreenPosMode(
        tex, buffer, vertex_count, depth_mode, blend_mode,
        MikuPan_ScreenCopyScreenPosModeFromTex(tex));
}

void MikuPan_RenderScreenCopyTriangles3DScreenPosGSAlpha(sceGsTex0 *tex,
                                                         float *buffer,
                                                         int vertex_count,
                                                         int depth_mode,
                                                         unsigned long gs_alpha)
{
    MikuPan_RenderScreenCopyTriangles3DScreenPos(
        tex, buffer, vertex_count, depth_mode,
        MikuPan_GPUBlendModeFromGSAlpha(gs_alpha));
}


void MikuPan_RenderScreenCopyTriangles3DFeedbackModulate(sceGsTex0 *tex,
                                                         float *buffer,
                                                         int vertex_count,
                                                         int depth_mode,
                                                         MikuPan_GPUBlendMode blend_mode)
{
    if (vertex_count <= 0)
    {
        return;
    }

    if (MikuPan_ArePhotoCaptureScreenCopyEffectsSuppressed())
    {
        return;
    }

    MikuPan_FlushTexturedSpriteBatch();

    g_force_feedback_screen_copy_source = 1;
    if (!MikuPan_UpdateScreenCopyTexture(tex))
    {
        g_force_feedback_screen_copy_source = 0;
        return;
    }
    g_force_feedback_screen_copy_source = 0;

    MIKUPAN_PERF_SCOPE(PERF_SECT_SPRITE_RENDER);
    MikuPan_SetCurrentShaderProgram(HEAT_HAZE_SHADER);
    MikuPan_SetUniform1iToCurrentShader(0, "uTexture");
    MikuPan_SetUniform1iToCurrentShader(0, "uBlackWhiteMode");
    MikuPan_SetUniform2fToCurrentShader(g_screen_copy_uv_offset[0],
                                        g_screen_copy_uv_offset[1],
                                        "uFramebufferUvOffset");
    MikuPan_SetUniform2fToCurrentShader(g_screen_copy_uv_scale[0],
                                        g_screen_copy_uv_scale[1],
                                        "uFramebufferUvScale");
    MikuPan_SetUniform2fToCurrentShader(g_screen_copy_content_uv_max[0],
                                        g_screen_copy_content_uv_max[1],
                                        "uFramebufferContentUvMax");
    MikuPan_SetUniform1iToCurrentShader(3, "uUseScreenPos");
    MikuPan_SetUniform2fToCurrentShader((float)g_screen_copy_w,
                                        (float)g_screen_copy_h,
                                        "uRenderSize");
    MikuPan_PipelineInfo *pipeline = MikuPan_GetPipelineInfo(UV4_COLOUR4_POSITION4);

    MikuPan_BindVAO(pipeline->vao);
    MikuPan_ActiveTextureCached(GL_TEXTURE0);
    MikuPan_BindTexture2DCached(g_screen_copy_texture);
    MikuPan_ApplyHeatHazeTriangleState(depth_mode, blend_mode);
    MikuPan_NormalizeTexturedTriangleDepths(buffer, vertex_count);

    MikuPan_StreamUploadFull(
        GL_ARRAY_BUFFER,
        pipeline->buffers[0].id,
        (GLsizeiptr)(vertex_count * 12 * (int)sizeof(float)),
        buffer);

    MikuPan_TimedDrawArrays(GL_TRIANGLES, 0, vertex_count);
    MikuPan_PerfDrawCall();
    MikuPan_RestoreParticleTriangleState();
}

void MikuPan_RenderScreenCopyTriangles3DSTQ(sceGsTex0 *tex, float *buffer, int vertex_count, int depth_mode)
{
    if (vertex_count <= 0)
    {
        return;
    }

    if (MikuPan_ArePhotoCaptureScreenCopyEffectsSuppressed())
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
    MikuPan_SetUniform1iToCurrentShader(0, "uBlackWhiteMode");
    MikuPan_SetUniform2fToCurrentShader(g_screen_copy_uv_offset[0],
                                        g_screen_copy_uv_offset[1],
                                        "uFramebufferUvOffset");
    MikuPan_SetUniform2fToCurrentShader(g_screen_copy_uv_scale[0],
                                        g_screen_copy_uv_scale[1],
                                        "uFramebufferUvScale");
    MikuPan_SetUniform2fToCurrentShader(g_screen_copy_content_uv_max[0],
                                        g_screen_copy_content_uv_max[1],
                                        "uFramebufferContentUvMax");
    MikuPan_SetUniform1iToCurrentShader(2, "uUseScreenPos");
    MikuPan_SetUniform2fToCurrentShader((float)g_screen_copy_w,
                                        (float)g_screen_copy_h,
                                        "uRenderSize");
    MikuPan_PipelineInfo *pipeline = MikuPan_GetPipelineInfo(UV4_COLOUR4_POSITION4);

    MikuPan_BindVAO(pipeline->vao);
    MikuPan_ActiveTextureCached(GL_TEXTURE0);
    MikuPan_BindTexture2DCached(g_screen_copy_texture);
    MikuPan_ApplyHeatHazeTriangleState(depth_mode, MIKUPAN_GPU_BLEND_NORMAL);
    MikuPan_NormalizeTexturedTriangleDepths(buffer, vertex_count);

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

void MikuPan_RenderTexturedTriangles3DWithState(sceGsTex0 *tex,
                                           float *buffer,
                                           int vertex_count,
                                           int depth_mode,
                                           MikuPan_GPUBlendMode blend_mode)
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
    MikuPan_ApplyParticleTriangleState(depth_mode, blend_mode);
    MikuPan_NormalizeTexturedTriangleDepths(buffer, vertex_count);

    MikuPan_StreamUploadFull(
        GL_ARRAY_BUFFER,
        pipeline->buffers[0].id,
        (GLsizeiptr)(vertex_count * 12 * (int)sizeof(float)),
        buffer);

    MikuPan_TimedDrawArrays(GL_TRIANGLES, 0, vertex_count);
    MikuPan_PerfDrawCall();
    MikuPan_RestoreParticleTriangleState();
}
