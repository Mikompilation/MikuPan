#include "mikupan_renderer_internal.h"
#include "graphics/graph2d/message.h"
#include "mikupan/gs/mikupan_gs_c.h"
#include "mikupan/gs/mikupan_texture_manager_c.h"
#include "mikupan/mikupan_utils.h"
#include "mikupan/ui/mikupan_ui.h"
#include "mikupan_pipeline.h"
#include "mikupan_gpu.h"
#include "mikupan_profiler.h"
#include "mikupan_shader.h"
#include <stdlib.h>

static MikuPan_TextureInfo *fnt_texture[6] = {0};
static MikuPan_TextureInfo *curr_fnt_texture = NULL;
static MikuPan_TextureInfo g_photo_preview_texture = {0};
static MikuPan_TextureInfo g_photo_base_preview_texture = {0};
static MikuPan_TextureInfo g_photo_negative_source_texture = {0};
static int g_photo_preview_queued = 0;
static int g_photo_base_preview_active = 0;
static int g_photo_base_preview_drawn_in_game = 0;
static int g_photo_base_preview_x = 0;
static int g_photo_base_preview_y = 0;
static int g_photo_base_preview_w = 0;
static int g_photo_base_preview_h = 0;
static u_char g_photo_base_preview_alpha = 0x80;
static int g_photo_preview_x = 0;
static int g_photo_preview_y = 0;
static int g_photo_preview_w = 0;
static int g_photo_preview_h = 0;
static u_char g_photo_preview_alpha = 0x80;
static float g_photo_preview_negative_strength = 0.0f;
static MikuPan_PhotoDebugInfo g_photo_debug = {
    .force_negative_preview_strength = 1.0f,
};

MikuPan_TextureInfo *MikuPan_GetCurrentFontTexture(void)
{
    return curr_fnt_texture;
}

const MikuPan_PhotoDebugInfo *MikuPan_GetPhotoDebugInfo(void)
{
    return &g_photo_debug;
}

void MikuPan_SetPhotoDebugForceOpaquePreviewEnabled(int enabled)
{
    g_photo_debug.force_opaque_preview_enabled = enabled != 0;
}

int MikuPan_IsPhotoDebugForceOpaquePreviewEnabled(void)
{
    return g_photo_debug.force_opaque_preview_enabled;
}

void MikuPan_SetPhotoDebugForceNegativePreviewEnabled(int enabled)
{
    g_photo_debug.force_negative_preview_enabled = enabled != 0;
}

int MikuPan_IsPhotoDebugForceNegativePreviewEnabled(void)
{
    return g_photo_debug.force_negative_preview_enabled;
}

void MikuPan_SetPhotoDebugForceNegativePreviewStrength(float strength)
{
    if (strength < 0.0f)
    {
        strength = 0.0f;
    }
    else if (strength > 1.0f)
    {
        strength = 1.0f;
    }

    g_photo_debug.force_negative_preview_strength = strength;
}

float MikuPan_GetPhotoDebugForceNegativePreviewStrength(void)
{
    return g_photo_debug.force_negative_preview_strength;
}

void MikuPan_SetPhotoDebugTargetRectEnabled(int enabled)
{
    g_photo_debug.target_rect_enabled = enabled != 0;
}

int MikuPan_IsPhotoDebugTargetRectEnabled(void)
{
    return g_photo_debug.target_rect_enabled;
}

void MikuPan_SetPhotoDebugNegativeLayerEnabled(int enabled)
{
    g_photo_debug.negative_debug_layer_enabled = enabled != 0;
}

int MikuPan_IsPhotoDebugNegativeLayerEnabled(void)
{
    return g_photo_debug.negative_debug_layer_enabled;
}

void MikuPan_TextureShutdown(void)
{
    for (int i = 0; i < 6; i++)
    {
        if (fnt_texture[i] != NULL)
        {
            MikuPan_GPUReleaseTexture(fnt_texture[i]->id);
            free(fnt_texture[i]);
            fnt_texture[i] = NULL;
        }
    }

    curr_fnt_texture = NULL;

    if (g_photo_preview_texture.id != 0)
    {
        MikuPan_GPUReleaseTexture(g_photo_preview_texture.id);
        g_photo_preview_texture.id = 0;
    }

    if (g_photo_base_preview_texture.id != 0)
    {
        MikuPan_GPUReleaseTexture(g_photo_base_preview_texture.id);
        g_photo_base_preview_texture.id = 0;
    }

    if (g_photo_negative_source_texture.id != 0)
    {
        MikuPan_GPUReleaseTexture(g_photo_negative_source_texture.id);
        g_photo_negative_source_texture.id = 0;
    }

    g_photo_debug.texture_valid = 0;
    g_photo_debug.texture_id = 0;
    g_photo_debug.negative_source_texture_valid = 0;
    g_photo_debug.negative_source_texture_id = 0;
}

MikuPan_TextureInfo *MikuPan_CreateGLTexture(sceGsTex0 *tex0)
{
    int width = 1 << tex0->TW;
    int height = 1 << tex0->TH;

    uint64_t hash = 0;
    void *pixels = MikuPan_GsDownloadTexture(tex0, &hash);

    if (hash == 0)
    {
        return NULL;
    }

    unsigned int tex = MikuPan_GPUCreateTextureRGBA8(
        width, height, pixels, width * 4, 1, 0);

    MikuPan_TextureInfo *texture_info = (MikuPan_TextureInfo *)malloc(sizeof(MikuPan_TextureInfo));
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
        MikuPan_ActiveTextureCached(GL_TEXTURE0);
        MikuPan_BindTexture2DCached(texture_info->id);
        MikuPan_PerfEnd(PERF_SECT_TEX_BIND, _t);
    }
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

void MikuPan_SetFontTexture(int fnt)
{
    curr_fnt_texture = fnt_texture[fnt];
}

void MikuPan_UpdatePhotoPreviewTextureRGBA(int width, int height,
                                           const unsigned char *rgba)
{
    if (width <= 0 || height <= 0 || rgba == NULL)
    {
        return;
    }

    if (g_photo_preview_texture.id == 0 ||
        g_photo_preview_texture.width != width ||
        g_photo_preview_texture.height != height)
    {
        if (g_photo_preview_texture.id != 0)
        {
            MikuPan_GPUReleaseTexture(g_photo_preview_texture.id);
        }

        g_photo_preview_texture.id = MikuPan_GPUCreateTextureRGBA8(
            width, height, rgba, width * 4, 0, 0);
        g_photo_preview_texture.width = width;
        g_photo_preview_texture.height = height;
        g_photo_preview_texture.tex0 = 0;
        g_photo_preview_texture.hash = 0;

        g_photo_debug.texture_valid = 1;
        g_photo_debug.texture_id = g_photo_preview_texture.id;
        g_photo_debug.texture_width = width;
        g_photo_debug.texture_height = height;
        g_photo_debug.texture_update_count++;
        return;
    }

    MikuPan_GPUUploadTextureRGBA8(
        g_photo_preview_texture.id, width, height, rgba, width * 4);

    g_photo_debug.texture_valid = 1;
    g_photo_debug.texture_id = g_photo_preview_texture.id;
    g_photo_debug.texture_width = width;
    g_photo_debug.texture_height = height;
    g_photo_debug.texture_update_count++;
}


void MikuPan_UpdatePhotoBasePreviewTextureRGBA(int width, int height,
                                               const unsigned char *rgba)
{
    if (width <= 0 || height <= 0 || rgba == NULL)
    {
        return;
    }

    if (g_photo_base_preview_texture.id == 0 ||
        g_photo_base_preview_texture.width != width ||
        g_photo_base_preview_texture.height != height)
    {
        if (g_photo_base_preview_texture.id != 0)
        {
            MikuPan_GPUReleaseTexture(g_photo_base_preview_texture.id);
        }

        g_photo_base_preview_texture.id = MikuPan_GPUCreateTextureRGBA8(
            width, height, rgba, width * 4, 0, 0);
        g_photo_base_preview_texture.width = width;
        g_photo_base_preview_texture.height = height;
        g_photo_base_preview_texture.tex0 = 0;
        g_photo_base_preview_texture.hash = 0;
        return;
    }

    MikuPan_GPUUploadTextureRGBA8(
        g_photo_base_preview_texture.id, width, height, rgba, width * 4);
}

void MikuPan_UpdatePhotoNegativeSourceTextureRGBA(int width, int height,
                                                  const unsigned char *rgba)
{
    if (width <= 0 || height <= 0 || rgba == NULL)
    {
        return;
    }

    if (g_photo_negative_source_texture.id == 0 ||
        g_photo_negative_source_texture.width != width ||
        g_photo_negative_source_texture.height != height)
    {
        if (g_photo_negative_source_texture.id != 0)
        {
            MikuPan_GPUReleaseTexture(g_photo_negative_source_texture.id);
        }

        g_photo_negative_source_texture.id = MikuPan_GPUCreateTextureRGBA8(
            width, height, rgba, width * 4, 0, 0);
        g_photo_negative_source_texture.width = width;
        g_photo_negative_source_texture.height = height;
        g_photo_negative_source_texture.tex0 = 0;
        g_photo_negative_source_texture.hash = 0;

        g_photo_debug.negative_source_texture_valid = 1;
        g_photo_debug.negative_source_texture_id =
            g_photo_negative_source_texture.id;
        g_photo_debug.negative_source_texture_width = width;
        g_photo_debug.negative_source_texture_height = height;
        g_photo_debug.negative_source_texture_update_count++;
        return;
    }

    MikuPan_GPUUploadTextureRGBA8(
        g_photo_negative_source_texture.id, width, height, rgba, width * 4);

    g_photo_debug.negative_source_texture_valid = 1;
    g_photo_debug.negative_source_texture_id = g_photo_negative_source_texture.id;
    g_photo_debug.negative_source_texture_width = width;
    g_photo_debug.negative_source_texture_height = height;
    g_photo_debug.negative_source_texture_update_count++;
}

void MikuPan_QueuePhotoPreviewTexture(int x, int y, int w, int h,
                                      u_char alpha)
{
    if (g_photo_preview_texture.id == 0 || w <= 0 || h <= 0)
    {
        return;
    }

    g_photo_preview_x = x;
    g_photo_preview_y = y;
    g_photo_preview_w = w;
    g_photo_preview_h = h;
    g_photo_preview_alpha = alpha;
    g_photo_preview_queued = 1;

    g_photo_debug.queued = 1;
    g_photo_debug.queue_count++;
    g_photo_debug.queue_x = x;
    g_photo_debug.queue_y = y;
    g_photo_debug.queue_w = w;
    g_photo_debug.queue_h = h;
    g_photo_debug.queue_alpha = alpha;
}

void MikuPan_SetPhotoPreviewOverlayActiveForFrame(int x, int y, int w, int h,
                                                  u_char alpha)
{
    if (w <= 0 || h <= 0)
    {
        return;
    }

    g_photo_preview_x = x;
    g_photo_preview_y = y;
    g_photo_preview_w = w;
    g_photo_preview_h = h;
    g_photo_preview_alpha = alpha;

    g_photo_debug.effect_overlay_active = 1;
    g_photo_debug.effect_overlay_count++;
    g_photo_debug.queue_x = x;
    g_photo_debug.queue_y = y;
    g_photo_debug.queue_w = w;
    g_photo_debug.queue_h = h;
    g_photo_debug.queue_alpha = alpha;
}

void MikuPan_SetPhotoBasePreviewOverlayActiveForFrame(int x, int y, int w, int h,
                                                      u_char alpha)
{
    if (g_photo_base_preview_texture.id == 0 || w <= 0 || h <= 0)
    {
        return;
    }

    g_photo_base_preview_x = x;
    g_photo_base_preview_y = y;
    g_photo_base_preview_w = w;
    g_photo_base_preview_h = h;
    g_photo_base_preview_alpha = alpha;
    g_photo_base_preview_active = 1;
}

void MikuPan_ClearPhotoBasePreviewOverlay(void)
{
    g_photo_base_preview_active = 0;
    g_photo_base_preview_drawn_in_game = 0;
}

void MikuPan_SetPhotoNegativeOverlayActiveForFrame(int x, int y, int w, int h,
                                                   float strength)
{
    if (w <= 0 || h <= 0)
    {
        return;
    }

    g_photo_debug.negative_overlay_active = 1;
    g_photo_debug.negative_overlay_count++;
    g_photo_debug.negative_x = x;
    g_photo_debug.negative_y = y;
    g_photo_debug.negative_w = w;
    g_photo_debug.negative_h = h;
    g_photo_debug.negative_strength = strength;
}

void MikuPan_SetScreenNegativeOverlayActiveForFrame(float r, float g, float b,
                                                    float strength)
{
    if (strength <= 0.0f)
    {
        return;
    }

    if (strength > 1.0f)
    {
        strength = 1.0f;
    }

    g_photo_debug.screen_negative_overlay_active = 1;
    g_photo_debug.screen_negative_overlay_count++;
    g_photo_debug.screen_negative_r = r;
    g_photo_debug.screen_negative_g = g;
    g_photo_debug.screen_negative_b = b;
    g_photo_debug.screen_negative_strength = strength;
}

void MikuPan_ClearScreenNegativeOverlay(void)
{
    g_photo_debug.screen_negative_overlay_active = 0;
    g_photo_debug.screen_negative_strength = 0.0f;
}

void MikuPan_ClearPhotoPreviewOverlay(void)
{
    g_photo_preview_queued = 0;
    g_photo_preview_negative_strength = 0.0f;
    g_photo_debug.queued = 0;
    g_photo_debug.effect_overlay_active = 0;
    g_photo_debug.effect_overlay_drawn_in_game = 0;
    g_photo_debug.negative_overlay_drawn_in_game = 0;
}

void MikuPan_ClearPhotoNegativeOverlay(void)
{
    g_photo_debug.negative_overlay_active = 0;
    g_photo_debug.negative_strength = 0.0f;
}

static void MikuPan_RenderPhotoTextureInfo(MikuPan_TextureInfo *texture_info,
                                             int x, int y, int w, int h,
                                             u_char alpha,
                                             float negative_strength)
{
    if (texture_info == NULL || texture_info->id == 0 || w <= 0 || h <= 0)
    {
        return;
    }

    MikuPan_FlushTexturedSpriteBatch();

    float tl[4] = {0};
    float br[4] = {0};
    MikuPan_ConvertPs2ScreenCoordToNDCMaintainAspectRatio(
        tl,
        (float)MikuPan_GetRenderResolutionWidth(),
        (float)MikuPan_GetRenderResolutionHeight(),
        (float)x,
        (float)y);
    MikuPan_ConvertPs2ScreenCoordToNDCMaintainAspectRatio(
        br,
        (float)MikuPan_GetRenderResolutionWidth(),
        (float)MikuPan_GetRenderResolutionHeight(),
        (float)(x + w),
        (float)(y + h));

    const float a = MikuPan_ConvertScaleColor(alpha);
    const float verts[6][12] = {
        {0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f, a, tl[0], tl[1], 0.0f, 1.0f},
        {0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f, a, tl[0], br[1], 0.0f, 1.0f},
        {1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f, a, br[0], tl[1], 0.0f, 1.0f},
        {1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f, a, br[0], tl[1], 0.0f, 1.0f},
        {0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f, a, tl[0], br[1], 0.0f, 1.0f},
        {1.0f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f, a, br[0], br[1], 0.0f, 1.0f},
    };

    MikuPan_SetCurrentShaderProgram(SPRITE_SHADER);
    MikuPan_SetUniform1iToCurrentShader(0, "uTexture");
    MikuPan_SetUniform1fToCurrentShader(negative_strength,
                                        "uPhotoNegativeStrength");
    MikuPan_SetRenderState2D();

    MikuPan_PipelineInfo *pipeline = MikuPan_GetPipelineInfo(UV4_COLOUR4_POSITION4);
    MikuPan_BindVAO(pipeline->vao);
    MikuPan_BindTexture2DCached(texture_info->id);
    MikuPan_StreamUploadFull(GL_ARRAY_BUFFER, pipeline->buffers[0].id,
                             (GLsizeiptr)sizeof(verts), verts);

    MikuPan_TimedDrawArrays(GL_TRIANGLES, 0, 6);
    MikuPan_PerfDrawCall();

    if (negative_strength != 0.0f)
    {
        MikuPan_SetUniform1fToCurrentShader(0.0f, "uPhotoNegativeStrength");
    }
}

static void MikuPan_RenderPhotoBasePreviewTexture(int x, int y, int w, int h,
                                                  u_char alpha)
{
    MikuPan_RenderPhotoTextureInfo(
        &g_photo_base_preview_texture, x, y, w, h, alpha, 0.0f);
}

void MikuPan_RenderPhotoPreviewTexture(int x, int y, int w, int h,
                                       u_char alpha)
{
    if (g_photo_preview_texture.id == 0 || w <= 0 || h <= 0)
    {
        return;
    }

    MikuPan_RenderPhotoTextureInfo(
        &g_photo_preview_texture,
        x, y, w, h, alpha, g_photo_preview_negative_strength);

    g_photo_preview_negative_strength = 0.0f;
    MikuPan_SetUniform1fToCurrentShader(0.0f, "uPhotoNegativeStrength");

    g_photo_debug.last_draw_valid = 1;
    g_photo_debug.draw_count++;
    g_photo_debug.draw_x = x;
    g_photo_debug.draw_y = y;
    g_photo_debug.draw_w = w;
    g_photo_debug.draw_h = h;
    g_photo_debug.draw_alpha = alpha;
}

int MikuPan_RenderPhotoPreviewOverlayForFrame(void)
{
    int rendered = 0;

    if (g_photo_base_preview_active &&
        g_photo_base_preview_texture.id != 0 &&
        g_photo_base_preview_w > 0 &&
        g_photo_base_preview_h > 0)
    {
        MikuPan_RenderPhotoBasePreviewTexture(
            g_photo_base_preview_x,
            g_photo_base_preview_y,
            g_photo_base_preview_w,
            g_photo_base_preview_h,
            g_photo_base_preview_alpha);

        g_photo_base_preview_drawn_in_game = 1;
        rendered = 1;
    }

    if (g_photo_debug.effect_overlay_active &&
        g_photo_preview_texture.id != 0 &&
        g_photo_preview_w > 0 &&
        g_photo_preview_h > 0)
    {
        MikuPan_RenderPhotoPreviewTexture(
            g_photo_preview_x,
            g_photo_preview_y,
            g_photo_preview_w,
            g_photo_preview_h,
            g_photo_preview_alpha);

        g_photo_debug.effect_overlay_drawn_in_game = 1;
        g_photo_debug.effect_overlay_in_game_count++;
        rendered = 1;
    }

    return rendered;
}

int MikuPan_RenderPhotoNegativePreviewOverlayForFrame(void)
{
    /*
     * The photo negative is an outside-the-photo full-frame effect. It cannot
     * be represented by drawing the captured photo texture again, because that
     * targets the exclusion rect. The final postprocess pass consumes
     * negative_overlay_active/negative_strength and applies the effect to the
     * composed framebuffer outside negative_x/y/w/h.
     */
    return 0;
}

static void MikuPan_RenderPhotoDebugRect(int x, int y, int w, int h,
                                         float r, float g, float b, float a)
{
    float tl[4] = {0};
    float br[4] = {0};

    if (w <= 0 || h <= 0)
    {
        return;
    }

    MikuPan_ConvertPs2ScreenCoordToNDCMaintainAspectRatio(
        tl,
        (float)MikuPan_GetRenderResolutionWidth(),
        (float)MikuPan_GetRenderResolutionHeight(),
        (float)x,
        (float)y);
    MikuPan_ConvertPs2ScreenCoordToNDCMaintainAspectRatio(
        br,
        (float)MikuPan_GetRenderResolutionWidth(),
        (float)MikuPan_GetRenderResolutionHeight(),
        (float)(x + w),
        (float)(y + h));

    const float verts[4][8] = {
        {r, g, b, a, tl[0], tl[1], 0.0f, 1.0f},
        {r, g, b, a, tl[0], br[1], 0.0f, 1.0f},
        {r, g, b, a, br[0], tl[1], 0.0f, 1.0f},
        {r, g, b, a, br[0], br[1], 0.0f, 1.0f},
    };

    MikuPan_FlushTexturedSpriteBatch();
    MikuPan_SetCurrentShaderProgram(UNTEXTURED_COLOURED_SPRITE_SHADER);
    MikuPan_SetRenderState2D();

    MikuPan_PipelineInfo *pipeline = MikuPan_GetPipelineInfo(COLOUR4_POSITION4);
    MikuPan_BindVAO(pipeline->vao);
    MikuPan_StreamUploadFull(GL_ARRAY_BUFFER, pipeline->buffers[0].id,
                             (GLsizeiptr)sizeof(verts), verts);

    MikuPan_TimedDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    MikuPan_PerfDrawCall();
}

static int MikuPan_GetPhotoPreviewDebugRect(int *x, int *y, int *w, int *h)
{
    if (g_photo_preview_w > 0 && g_photo_preview_h > 0)
    {
        *x = g_photo_preview_x;
        *y = g_photo_preview_y;
        *w = g_photo_preview_w;
        *h = g_photo_preview_h;
        return 1;
    }

    if (g_photo_debug.last_draw_valid &&
        g_photo_debug.draw_w > 0 &&
        g_photo_debug.draw_h > 0)
    {
        *x = g_photo_debug.draw_x;
        *y = g_photo_debug.draw_y;
        *w = g_photo_debug.draw_w;
        *h = g_photo_debug.draw_h;
        return 1;
    }

    return 0;
}

static int MikuPan_GetPhotoNegativeDebugRect(int *x, int *y, int *w, int *h)
{
    if (g_photo_debug.negative_w > 0 && g_photo_debug.negative_h > 0)
    {
        *x = g_photo_debug.negative_x;
        *y = g_photo_debug.negative_y;
        *w = g_photo_debug.negative_w;
        *h = g_photo_debug.negative_h;
        return 1;
    }

    return MikuPan_GetPhotoPreviewDebugRect(x, y, w, h);
}

static void MikuPan_RenderPhotoDebugTargetRect(void)
{
    int x = 0;
    int y = 0;
    int w = 0;
    int h = 0;

    g_photo_debug.target_rect_drawn = 0;

    if (!g_photo_debug.target_rect_enabled ||
        !MikuPan_GetPhotoPreviewDebugRect(&x, &y, &w, &h))
    {
        return;
    }

    MikuPan_RenderPhotoDebugRect(x, y, w, h, 1.0f, 0.0f, 1.0f, 0.22f);
    g_photo_debug.target_rect_drawn = 1;
}

static void MikuPan_RenderPhotoNegativeDebugLayer(void)
{
    int x = 0;
    int y = 0;
    int w = 0;
    int h = 0;

    g_photo_debug.negative_debug_layer_drawn = 0;

    if (!g_photo_debug.negative_debug_layer_enabled ||
        !MikuPan_GetPhotoNegativeDebugRect(&x, &y, &w, &h))
    {
        return;
    }

    MikuPan_RenderPhotoDebugRect(0, 0, 640, y, 0.0f, 0.85f, 1.0f, 0.22f);
    MikuPan_RenderPhotoDebugRect(0, y + h, 640, 448 - (y + h),
                                 0.0f, 0.85f, 1.0f, 0.22f);
    MikuPan_RenderPhotoDebugRect(0, y, x, h, 0.0f, 0.85f, 1.0f, 0.22f);
    MikuPan_RenderPhotoDebugRect(x + w, y, 640 - (x + w), h,
                                 0.0f, 0.85f, 1.0f, 0.22f);
    g_photo_debug.negative_debug_layer_drawn = 1;
    g_photo_debug.negative_debug_layer_count++;
}

static void MikuPan_RenderPhotoPreviewFrameBorder(int enabled)
{
    const int border = 6;
    const float r = 0xcf / 255.0f;
    const float g = 0xbd / 255.0f;
    const float b = 0xa1 / 255.0f;

    if (!enabled ||
        g_photo_preview_texture.id == 0 ||
        g_photo_preview_w <= 0 ||
        g_photo_preview_h <= 0)
    {
        return;
    }

    MikuPan_RenderPhotoDebugRect(
        g_photo_preview_x - border,
        g_photo_preview_y - border,
        g_photo_preview_w + border * 2,
        border,
        r, g, b, 1.0f);
    MikuPan_RenderPhotoDebugRect(
        g_photo_preview_x - border,
        g_photo_preview_y + g_photo_preview_h,
        g_photo_preview_w + border * 2,
        border,
        r, g, b, 1.0f);
    MikuPan_RenderPhotoDebugRect(
        g_photo_preview_x - border,
        g_photo_preview_y,
        border,
        g_photo_preview_h,
        r, g, b, 1.0f);
    MikuPan_RenderPhotoDebugRect(
        g_photo_preview_x + g_photo_preview_w,
        g_photo_preview_y,
        border,
        g_photo_preview_h,
        r, g, b, 1.0f);
}

void MikuPan_RenderQueuedPhotoPreviewTexture(void)
{
    int rendered_preview = 0;
    int base_context =
        g_photo_base_preview_active || g_photo_base_preview_drawn_in_game;
    int preview_context =
        base_context || g_photo_preview_queued ||
        g_photo_debug.effect_overlay_active ||
        g_photo_debug.effect_overlay_drawn_in_game;

    if (base_context &&
        g_photo_base_preview_texture.id != 0 &&
        g_photo_base_preview_w > 0 &&
        g_photo_base_preview_h > 0)
    {
        MikuPan_RenderPhotoBasePreviewTexture(
            g_photo_base_preview_x,
            g_photo_base_preview_y,
            g_photo_base_preview_w,
            g_photo_base_preview_h,
            g_photo_base_preview_alpha);
    }

    if (g_photo_preview_queued &&
        !g_photo_debug.effect_overlay_drawn_in_game)
    {
        g_photo_preview_queued = 0;
        g_photo_debug.queued = 0;
        MikuPan_RenderPhotoPreviewTexture(
            g_photo_preview_x,
            g_photo_preview_y,
            g_photo_preview_w,
            g_photo_preview_h,
            g_photo_preview_alpha);
        rendered_preview = 1;
    }
    else if (g_photo_preview_queued)
    {
        g_photo_preview_queued = 0;
        g_photo_debug.queued = 0;
    }

    if (!rendered_preview &&
        !g_photo_debug.force_negative_preview_enabled &&
        preview_context &&
        g_photo_preview_texture.id != 0 &&
        g_photo_preview_w > 0 &&
        g_photo_preview_h > 0)
    {
        MikuPan_RenderPhotoPreviewTexture(
            g_photo_preview_x,
            g_photo_preview_y,
            g_photo_preview_w,
            g_photo_preview_h,
            g_photo_preview_alpha);
    }

    MikuPan_RenderPhotoPreviewFrameBorder(preview_context || rendered_preview);
    MikuPan_RenderPhotoNegativeDebugLayer();
    MikuPan_RenderPhotoDebugTargetRect();
    g_photo_base_preview_active = 0;
    g_photo_base_preview_drawn_in_game = 0;
    g_photo_debug.effect_overlay_active = 0;
    g_photo_debug.effect_overlay_drawn_in_game = 0;
    g_photo_debug.negative_overlay_drawn_in_game = 0;
    MikuPan_ClearScreenNegativeOverlay();
}

void MikuPan_DeleteTexture(MikuPan_TextureInfo *texture_info)
{
    if (texture_info == NULL)
    {
        return;
    }

    for (int i = 0; i < 6; i++)
    {
        if (fnt_texture[i] != NULL && fnt_texture[i]->id == texture_info->id)
        {
            return;
        }
    }

    MikuPan_GPUReleaseTexture(texture_info->id);
    free(texture_info);
}
