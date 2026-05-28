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
