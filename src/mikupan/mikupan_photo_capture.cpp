#include "mikupan/mikupan_photo_capture.h"

#include "common.h"
#include "typedefs.h"

#include "graphics/graph2d/effect_sub.h"
#include "graphics/graph2d/tim2.h"
#include "ingame/photo/pht_make.h"
#include "main/glob.h"
#include "mikupan/gs/mikupan_texture_manager_c.h"
#include "mikupan/mikupan_logging_c.h"
#include "mikupan/mikupan_memory.h"
#include "mikupan/rendering/mikupan_renderer.h"

#include <string.h>

static u_short MikuPan_PhotoPackRgb555(int r, int g, int b)
{
    return (u_short)(0x8000 | ((b & 0x1f) << 10) |
                     ((g & 0x1f) << 5) | (r & 0x1f));
}

enum
{
    PHOTO_CAPTURE_SCREEN_W = 640,
    PHOTO_CAPTURE_SCREEN_H = 224,
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
static unsigned char g_photo_preview_rgba[PHOTO_CAPTURE_W * PHOTO_CAPTURE_H * 4];
static int g_photo_capture_valid = 0;
static int g_photo_hint3d_base_valid = 0;
static int g_photo_frame_overlay_active = 0;
static int g_photo_preview_drawn_this_game_frame = 0;

static void MikuPan_ActivateResolvedPhotoNegative(void);
static void MikuPan_ClearResolvedPhotoHint3DBase(void);
static void MikuPan_StageResolvedPhotoCaptureAsHint3DBase(void);
static void MikuPan_ClearStagedPhotoCaptureForSave(void);

int MikuPan_CapturePhotoForLater(void)
{
    const int myy = PHOTO_CAPTURE_Y / 2;
    unsigned char *rgba = (unsigned char *)GetEmptyBuffer(0);

    g_photo_capture_valid = 0;
    g_photo_frame_overlay_active = 0;

    MikuPan_CapturePhotoFramebuffer();

    if (!MikuPan_ReadPhotoFramebufferRGBA8TopLeft(
            PHOTO_CAPTURE_SCREEN_W,
            PHOTO_CAPTURE_SCREEN_H,
            rgba))
    {
        MikuPan_SetPhotoCaptureScreenCopyEffectsSuppressed(0);
        return 0;
    }

    MikuPan_UpdatePhotoNegativeSourceTextureRGBA(
        PHOTO_CAPTURE_SCREEN_W,
        PHOTO_CAPTURE_SCREEN_H,
        rgba);

    {
        long long dbg_sum = 0;
        int dbg_n = 0;
        int dbg_min = 255;
        int dbg_max = 0;

        for (int dy = 0; dy < PHOTO_CAPTURE_FIELD_H; dy += 8)
        {
            for (int dx = 0; dx < PHOTO_CAPTURE_W; dx += 8)
            {
                const unsigned char *p =
                    rgba + ((long long)((PHOTO_CAPTURE_X + dx) +
                                   (myy + dy) * PHOTO_CAPTURE_SCREEN_W)) * 4;
                const int lum = (p[0] + p[1] + p[2]) / 3;
                dbg_sum += lum;
                if (lum < dbg_min) dbg_min = lum;
                if (lum > dbg_max) dbg_max = lum;
                dbg_n++;
            }
        }

        info_log("[DIAG] photo staged read: avg_rgb=%lld min=%d max=%d region=%d,%d %dx%d count=%d",
                 dbg_n ? dbg_sum / dbg_n : -1,
                 dbg_n ? dbg_min : -1,
                 dbg_n ? dbg_max : -1,
                 PHOTO_CAPTURE_X, PHOTO_CAPTURE_Y,
                 PHOTO_CAPTURE_W, PHOTO_CAPTURE_H,
                 (int)sys_wrk.count);
    }

    for (int y = 0; y < PHOTO_CAPTURE_FIELD_H; y++)
    {
        for (int x = 0; x < PHOTO_CAPTURE_W; x++)
        {
            const unsigned char *p =
                rgba + ((long long)((PHOTO_CAPTURE_X + x) +
                               (myy + y) * PHOTO_CAPTURE_SCREEN_W)) * 4;
            g_photo_capture_555[x + y * PHOTO_CAPTURE_PITCH] =
                MikuPan_PhotoPackRgb555(p[0] >> 3, p[1] >> 3, p[2] >> 3);

            for (int yy = 0; yy < 2; yy++)
            {
                unsigned char *preview =
                    g_photo_preview_rgba +
                    ((long long)(x + (y * 2 + yy) * PHOTO_CAPTURE_W)) * 4;
                preview[0] = p[0];
                preview[1] = p[1];
                preview[2] = p[2];
                preview[3] = 0xff;
            }
        }
    }

    g_photo_capture_valid = 1;
    MikuPan_UpdatePhotoPreviewTextureRGBA(
        PHOTO_CAPTURE_W,
        PHOTO_CAPTURE_H,
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
    if (!g_photo_capture_valid)
    {
        return;
    }

    g_photo_hint3d_base_valid = 1;
    MikuPan_UpdatePhotoBasePreviewTextureRGBA(
        PHOTO_CAPTURE_W,
        PHOTO_CAPTURE_H,
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
        PHOTO_CAPTURE_W,
        PHOTO_CAPTURE_H,
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
