#include "mikupan/gameplay/mikupan_photo_capture.h"

#include "common.h"
#include "typedefs.h"

#include "graphics/graph2d/effect_sub.h"
#include "graphics/graph2d/tim2.h"
#include "ingame/photo/pht_make.h"
#include "main/glob.h"
#include "mikupan/gs/mikupan_texture_manager_c.h"
#include "mikupan/debug/mikupan_logging_c.h"
#include "mikupan/mikupan_memory.h"
#include "mikupan/mikupan_utils.h"
#include "mikupan/rendering/mikupan_renderer.h"

#include <stdlib.h>
#include <string.h>

static u_short MikuPan_PhotoPackRgb555(int r, int g, int b)
{
    return (u_short)(0x8000 | ((b & 0x1f) << 10) |
                     ((g & 0x1f) << 5) | (r & 0x1f));
}

enum
{
    PHOTO_CAPTURE_X = 128,
    PHOTO_CAPTURE_Y = 80,
    PHOTO_CAPTURE_W = 384,
    PHOTO_CAPTURE_H = 256,
    PHOTO_CAPTURE_FIELD_H = PHOTO_CAPTURE_H / 2,
    PHOTO_CAPTURE_PITCH = 384,
    PHOTO_NEGATIVE_PROTECT_X = PHOTO_CAPTURE_X,
    PHOTO_NEGATIVE_PROTECT_Y = PHOTO_CAPTURE_Y,
    PHOTO_NEGATIVE_PROTECT_W = PHOTO_CAPTURE_W,
    PHOTO_NEGATIVE_PROTECT_H = PHOTO_CAPTURE_H,
};

static u_short g_photo_capture_555[PHOTO_CAPTURE_PITCH * PHOTO_CAPTURE_FIELD_H];
static unsigned char *g_photo_preview_rgba = NULL;
static int g_photo_preview_w = 0;
static int g_photo_preview_h = 0;
static int g_photo_capture_valid = 0;
static int g_photo_hint3d_base_valid = 0;
static int g_photo_frame_overlay_active = 0;
static int g_photo_preview_drawn_this_game_frame = 0;

static void MikuPan_ActivateResolvedPhotoNegative(void);
static void MikuPan_ClearResolvedPhotoHint3DBase(void);
static void MikuPan_StageResolvedPhotoCaptureAsHint3DBase(void);
static int MikuPan_EnsurePhotoPreviewBuffer(int width, int height)
{
    const size_t bytes = (size_t)width * (size_t)height * 4u;

    if (width <= 0 || height <= 0)
    {
        return 0;
    }

    if (g_photo_preview_rgba != NULL &&
        g_photo_preview_w == width &&
        g_photo_preview_h == height)
    {
        return 1;
    }

    free(g_photo_preview_rgba);
    g_photo_preview_rgba = (unsigned char *)malloc(bytes);
    if (g_photo_preview_rgba == NULL)
    {
        g_photo_preview_w = 0;
        g_photo_preview_h = 0;
        return 0;
    }

    g_photo_preview_w = width;
    g_photo_preview_h = height;
    return 1;
}

static void MikuPan_GetPhotoSourceRect(int src_w, int src_h,
                                        int *crop_x, int *crop_y,
                                        int *crop_w, int *crop_h)
{
    float viewport_x = 0.0f;
    float viewport_y = 0.0f;
    float viewport_w = 0.0f;
    float viewport_h = 0.0f;
    float viewport_scale = 1.0f;
    int x0;
    int y0;
    int x1;
    int y1;

    MikuPan_GetPS2Viewport(src_w, src_h,
                           &viewport_x, &viewport_y,
                           &viewport_w, &viewport_h,
                           &viewport_scale);

    x0 = (int)(viewport_x + (float)PHOTO_CAPTURE_X * viewport_scale + 0.5f);
    y0 = (int)(viewport_y + (float)PHOTO_CAPTURE_Y * viewport_scale + 0.5f);
    x1 = (int)(viewport_x + (float)(PHOTO_CAPTURE_X + PHOTO_CAPTURE_W) * viewport_scale + 0.5f);
    y1 = (int)(viewport_y + (float)(PHOTO_CAPTURE_Y + PHOTO_CAPTURE_H) * viewport_scale + 0.5f);

    x0 = MikuPan_ClampInt(x0, 0, src_w - 1);
    y0 = MikuPan_ClampInt(y0, 0, src_h - 1);
    x1 = MikuPan_ClampInt(x1, x0 + 1, src_w);
    y1 = MikuPan_ClampInt(y1, y0 + 1, src_h);

    *crop_x = x0;
    *crop_y = y0;
    *crop_w = x1 - x0;
    *crop_h = y1 - y0;
}

static void MikuPan_CopyPhotoSourceToPreview(const unsigned char *src_rgba,
                                             int src_w,
                                             int src_h)
{
    int crop_x = 0;
    int crop_y = 0;
    int crop_w = src_w;
    int crop_h = src_h;

    if (!MikuPan_EnsurePhotoPreviewBuffer(PHOTO_CAPTURE_W, PHOTO_CAPTURE_H))
    {
        return;
    }

    MikuPan_GetPhotoSourceRect(src_w, src_h, &crop_x, &crop_y, &crop_w, &crop_h);

    for (int y = 0; y < PHOTO_CAPTURE_H; y++)
    {
        int sy = crop_y + (int)(((long long)y * crop_h) / PHOTO_CAPTURE_H);
        if (sy < 0) sy = 0;
        if (sy >= src_h) sy = src_h - 1;

        for (int x = 0; x < PHOTO_CAPTURE_W; x++)
        {
            int sx = crop_x + (int)(((long long)x * crop_w) / PHOTO_CAPTURE_W);
            if (sx < 0) sx = 0;
            if (sx >= src_w) sx = src_w - 1;

            memcpy(g_photo_preview_rgba +
                       ((size_t)y * PHOTO_CAPTURE_W + x) * 4u,
                   src_rgba + ((size_t)sy * src_w + sx) * 4u,
                   4);
        }
    }
}

static void MikuPan_UpdatePhotoSaveBufferFromPreview(void)
{
    if (g_photo_preview_rgba == NULL ||
        g_photo_preview_w != PHOTO_CAPTURE_W ||
        g_photo_preview_h != PHOTO_CAPTURE_H)
    {
        return;
    }

    for (int y = 0; y < PHOTO_CAPTURE_FIELD_H; y++)
    {
        for (int x = 0; x < PHOTO_CAPTURE_W; x++)
        {
            const unsigned char *p = g_photo_preview_rgba +
                ((size_t)(y * 2) * PHOTO_CAPTURE_W + x) * 4u;
            g_photo_capture_555[x + y * PHOTO_CAPTURE_PITCH] =
                MikuPan_PhotoPackRgb555(p[0] >> 3, p[1] >> 3, p[2] >> 3);
        }
    }
}

int MikuPan_CapturePhotoForLater(void)
{
    int source_w;
    int source_h;
    unsigned char *rgba;

    g_photo_capture_valid = 0;
    g_photo_frame_overlay_active = 0;

    if (MikuPan_EnsurePhotoPreviewBuffer(PHOTO_CAPTURE_W, PHOTO_CAPTURE_H) &&
        MikuPan_ReadCurrentSceneCaptureRectRGBA8TopLeft(
            PHOTO_CAPTURE_X, PHOTO_CAPTURE_Y,
            PHOTO_CAPTURE_W, PHOTO_CAPTURE_H,
            PHOTO_CAPTURE_W, PHOTO_CAPTURE_H,
            g_photo_preview_rgba))
    {
        MikuPan_CapturePhotoFramebuffer();
        MikuPan_SetPhotoNegativeSourceTextureReference(
            MikuPan_GetPhotoFramebufferTextureId(),
            MikuPan_GetPhotoFramebufferWidth(),
            MikuPan_GetPhotoFramebufferHeight());
        MikuPan_UpdatePhotoSaveBufferFromPreview();
        g_photo_capture_valid = 1;
        MikuPan_UpdatePhotoPreviewTextureRGBA(
            g_photo_preview_w,
            g_photo_preview_h,
            g_photo_preview_rgba);
        MikuPan_SetPhotoCaptureScreenCopyEffectsSuppressed(0);
        return 1;
    }

    MikuPan_CapturePhotoFramebuffer();

    source_w = MikuPan_GetPhotoFramebufferWidth();
    source_h = MikuPan_GetPhotoFramebufferHeight();
    if (source_w <= 0 || source_h <= 0)
    {
        source_w = MikuPan_GetWindowWidth();
        source_h = MikuPan_GetWindowHeight();
    }

    if (source_w <= 0 || source_h <= 0)
    {
        MikuPan_SetPhotoCaptureScreenCopyEffectsSuppressed(0);
        return 0;
    }

    if (MikuPan_EnsurePhotoPreviewBuffer(PHOTO_CAPTURE_W, PHOTO_CAPTURE_H) &&
        MikuPan_ReadPhotoCaptureRectRGBA8TopLeft(
            PHOTO_CAPTURE_X, PHOTO_CAPTURE_Y,
            PHOTO_CAPTURE_W, PHOTO_CAPTURE_H,
            PHOTO_CAPTURE_W, PHOTO_CAPTURE_H,
            g_photo_preview_rgba))
    {
        MikuPan_SetPhotoNegativeSourceTextureReference(
            MikuPan_GetPhotoFramebufferTextureId(),
            MikuPan_GetPhotoFramebufferWidth(),
            MikuPan_GetPhotoFramebufferHeight());
        MikuPan_UpdatePhotoSaveBufferFromPreview();
        g_photo_capture_valid = 1;
        MikuPan_UpdatePhotoPreviewTextureRGBA(
            g_photo_preview_w,
            g_photo_preview_h,
            g_photo_preview_rgba);
        MikuPan_SetPhotoCaptureScreenCopyEffectsSuppressed(0);
        return 1;
    }

    rgba = (unsigned char *)malloc((size_t)source_w * (size_t)source_h * 4u);
    if (rgba == NULL)
    {
        MikuPan_SetPhotoCaptureScreenCopyEffectsSuppressed(0);
        return 0;
    }

    if (!MikuPan_ReadPhotoFramebufferRGBA8TopLeft(source_w, source_h, rgba))
    {
        free(rgba);
        MikuPan_SetPhotoCaptureScreenCopyEffectsSuppressed(0);
        return 0;
    }

    MikuPan_UpdatePhotoNegativeSourceTextureRGBA(source_w, source_h, rgba);

    MikuPan_CopyPhotoSourceToPreview(rgba, source_w, source_h);
    free(rgba);

    if (g_photo_preview_rgba == NULL)
    {
        MikuPan_SetPhotoCaptureScreenCopyEffectsSuppressed(0);
        return 0;
    }

    MikuPan_UpdatePhotoSaveBufferFromPreview();
    g_photo_capture_valid = 1;
    MikuPan_UpdatePhotoPreviewTextureRGBA(
        g_photo_preview_w,
        g_photo_preview_h,
        g_photo_preview_rgba);
    MikuPan_SetPhotoCaptureScreenCopyEffectsSuppressed(0);
    return 1;
}

static void MikuPan_ClearResolvedPhotoHint3DBase(void)
{
    g_photo_hint3d_base_valid = 0;
    MikuPan_ClearPhotoBasePreviewOverlay();
}

static void MikuPan_StageResolvedPhotoCaptureAsHint3DBase(void)
{
    if (!g_photo_capture_valid || g_photo_preview_rgba == NULL)
    {
        return;
    }

    g_photo_hint3d_base_valid = 1;
    MikuPan_UpdatePhotoBasePreviewTextureRGBA(
        g_photo_preview_w,
        g_photo_preview_h,
        g_photo_preview_rgba);
}

void MikuPan_StagePhotoCaptureForSave(void)
{
    MikuPan_ClearResolvedPhotoHint3DBase();
    MikuPan_CapturePhotoForLater();
}

int MikuPan_HasPhotoCapture(void)
{
    return g_photo_capture_valid != 0;
}

static void MikuPan_ClearStagedPhotoCaptureForSave(void)
{
    g_photo_capture_valid = 0;
    MikuPan_ClearPhotoFrameOverlay();
}

void MikuPan_OnHiddenFurniturePhotoStart(void)
{
    /* The first resolved capture is the visible-furniture photo. Before the
     * hidden-furniture render runs, preserve that frame as the 3D hint base,
     * then clear it as the save target so PhotoMake() captures the hidden
     * version. Renderer history/screen-copy suppression stays in this MikuPan
     * hook instead of being spelled out in pht_main.c.
     */
    MikuPan_ClearSceneFeedbackFrameHistory();
    MikuPan_SuppressNextSceneFeedbackFrameCapture();
    MikuPan_SetPhotoCaptureScreenCopyEffectsSuppressed(1);
    MikuPan_StageResolvedPhotoCaptureAsHint3DBase();
    MikuPan_ClearStagedPhotoCaptureForSave();
}

void MikuPan_TakePhotoFromResolvedScreen(void)
{
    if (!g_photo_capture_valid && !MikuPan_CapturePhotoForLater())
    {
        TakePhotoFromScreen();
        return;
    }

    *(int *)MikuPan_GetHostAddress(EVENT_ADDRESS) = 0x18000;
    memcpy((u_char *)MikuPan_GetHostAddress(EVENT_ADDRESS) + 16,
           g_photo_capture_555,
           sizeof(g_photo_capture_555));

    MikuPan_UpdatePhotoPreviewTextureRGBA(
        g_photo_preview_w,
        g_photo_preview_h,
        g_photo_preview_rgba);
}

void MikuPan_QueueResolvedPhotoPreview(void)
{
    if (!g_photo_preview_drawn_this_game_frame)
    {
        MikuPan_SetPhotoPreviewOverlayActiveForFrame(
            PHOTO_CAPTURE_X, PHOTO_CAPTURE_Y,
            PHOTO_CAPTURE_W, PHOTO_CAPTURE_H,
            0x80);
        MikuPan_QueuePhotoPreviewTexture(
            PHOTO_CAPTURE_X, PHOTO_CAPTURE_Y,
            PHOTO_CAPTURE_W, PHOTO_CAPTURE_H,
            0x80);
        MikuPan_ActivateResolvedPhotoNegative();
    }
}

void MikuPan_ActivatePhotoPreviewWithAlpha(u_char alpha)
{
    if (g_photo_capture_valid)
    {
        if (hint_3d != 0 && g_photo_hint3d_base_valid)
        {
            MikuPan_SetPhotoBasePreviewOverlayActiveForFrame(
                PHOTO_CAPTURE_X, PHOTO_CAPTURE_Y,
                PHOTO_CAPTURE_W, PHOTO_CAPTURE_H,
                0x80);
        }

        MikuPan_SetPhotoPreviewOverlayActiveForFrame(
            PHOTO_CAPTURE_X, PHOTO_CAPTURE_Y,
            PHOTO_CAPTURE_W, PHOTO_CAPTURE_H,
            alpha);
    }
}

static void MikuPan_ActivateResolvedPhotoPreview(void)
{
    MikuPan_ActivatePhotoPreviewWithAlpha(0x80);
}

static void MikuPan_ActivateResolvedPhotoNegative(void)
{
    if (g_photo_capture_valid)
    {
        MikuPan_SetPhotoNegativeOverlayActiveForFrame(
            PHOTO_NEGATIVE_PROTECT_X, PHOTO_NEGATIVE_PROTECT_Y,
            PHOTO_NEGATIVE_PROTECT_W, PHOTO_NEGATIVE_PROTECT_H,
            1.0f);
    }
}

void MikuPan_ActivatePhotoFramePreview(void)
{
    if (hint_3d != 0 && g_photo_hint3d_base_valid)
    {
        MikuPan_ActivatePhotoPreviewWithAlpha(0);
    }
    else
    {
        MikuPan_ActivateResolvedPhotoPreview();
    }
}

void MikuPan_ActivatePhotoFrameOverlays(void)
{
    g_photo_preview_drawn_this_game_frame = 0;
    g_photo_frame_overlay_active = g_photo_capture_valid != 0;

    MikuPan_ActivatePhotoFramePreview();
    MikuPan_ActivateResolvedPhotoNegative();
}

void MikuPan_ClearPhotoFrameOverlay(void)
{
    g_photo_frame_overlay_active = 0;
    g_photo_preview_drawn_this_game_frame = 0;
    MikuPan_ClearPhotoPreviewOverlay();
    MikuPan_ClearPhotoBasePreviewOverlay();
    MikuPan_ClearPhotoNegativeOverlay();
}

void MikuPan_ResetPhotoFrameOverlay(void)
{
    MikuPan_SetPhotoCaptureScreenCopyEffectsSuppressed(0);
    g_photo_capture_valid = 0;
    MikuPan_ClearResolvedPhotoHint3DBase();
    MikuPan_ClearPhotoFrameOverlay();
}

void MikuPan_RenderPhotoPreviewInFrame(void)
{
    if (g_photo_preview_drawn_this_game_frame)
    {
        return;
    }

    DrawAll2D();

    if (MikuPan_RenderPhotoPreviewOverlayForFrame())
    {
        g_photo_preview_drawn_this_game_frame = 1;
    }
}

void MikuPan_RenderPhotoNegativeInFrame(void)
{
    DrawAll2D();
    MikuPan_RenderPhotoNegativePreviewOverlayForFrame();
}

int MikuPan_UsePhotoFrameOverlay(void)
{
    return g_photo_capture_valid != 0 && g_photo_frame_overlay_active != 0;
}

void MikuPan_ActivatePhotoNegativeFromSetNegaFilter(int type, float y12,
                                                    float y22)
{
    float coverage;

    if (type == 3)
    {
        MikuPan_SetPhotoNegativeOverlayActiveForFrame(
            PHOTO_NEGATIVE_PROTECT_X, PHOTO_NEGATIVE_PROTECT_Y,
            PHOTO_NEGATIVE_PROTECT_W, PHOTO_NEGATIVE_PROTECT_H,
            1.0f);
        return;
    }

    if (type != 1 && type != 2 && type != 4 && type != 5)
    {
        MikuPan_ClearPhotoNegativeOverlay();
        return;
    }

    if (y12 < 0.0f)
    {
        y12 = -y12;
    }

    if (y22 < 0.0f)
    {
        y22 = -y22;
    }

    coverage = y12 > y22 ? y12 : y22;
    coverage /= 128.0f;

    if (coverage <= 0.0f)
    {
        MikuPan_ClearPhotoNegativeOverlay();
        return;
    }

    if (coverage > 1.0f)
    {
        coverage = 1.0f;
    }

    MikuPan_SetPhotoNegativeOverlayActiveForFrame(
        PHOTO_NEGATIVE_PROTECT_X, PHOTO_NEGATIVE_PROTECT_Y,
        PHOTO_NEGATIVE_PROTECT_W, PHOTO_NEGATIVE_PROTECT_H,
        coverage);
}
