#include "mikupan/gameplay/mikupan_item_icon_hud.h"

#include "common.h"
#include "typedefs.h"
#include "enums.h"

#include "graphics/graph2d/tim2.h"
#include "ingame/plyr/plyr_ctl.h"
#include "main/glob.h"
#include "mikupan/mikupan_utils.h"
#include "mikupan/rendering/mikupan_gpu.h"
#include "mikupan/rendering/mikupan_renderer.h"
#include "mikupan/ui/mikupan_ui.h"
#include "os/eeiop/cdvd/eecdvd.h"
#include "os/eeiop/eese.h"
#include "os/key_cnf.h"

#include <stdlib.h>
#include <string.h>

struct MikuPanItemIconSlot
{
    int item_no;
    void* data;
    int load_id;
    MikuPan_TextureInfo* texture;
    MikuPan_TextureInfo* dim_texture;
};

static MikuPanItemIconSlot mikupan_item_icon_slots[8] = {};
static int mikupan_finder_film_icon_current = -1;
static int mikupan_finder_film_icon_previous = -1;
static int mikupan_finder_film_icon_direction = 0;
static int mikupan_finder_film_icon_timer = 0;
static int mikupan_finder_film_counter_snap = 0;
static int mikupan_mirror_stone_hud_enabled = 0;
static int mikupan_item_icon_slots_initialised = 0;

static constexpr int MIKUPAN_FINDER_FILM_ICON_SWAP_FRAMES = 18;
static constexpr int MIKUPAN_ITEM_ICON_COUNT = (int)(sizeof(mikupan_item_icon_slots) / sizeof(mikupan_item_icon_slots[0]));

static void MikuPanNotifyFinderFilmSwitch(int old_film_no, int new_film_no, int direction);

static void MikuPanEnsureItemIconSlotsInitialised()
{
    if (mikupan_item_icon_slots_initialised != 0)
    {
        return;
    }

    for (int i = 0; i < MIKUPAN_ITEM_ICON_COUNT; i++)
    {
        mikupan_item_icon_slots[i].item_no = -1;
        mikupan_item_icon_slots[i].load_id = -1;
    }

    mikupan_item_icon_slots_initialised = 1;
}

static u_char MikuPanItemIconAlpha(float alpha)
{
    return (u_char)MikuPan_ClampInt((int)alpha, 0, 128);
}

static int MikuPanFilmNoToItemIconNo(int film_no)
{
    if (film_no < 0 || film_no > 4)
    {
        return -1;
    }

    return film_no;
}

static int MikuPanFindItemIconSlot(int item_no)
{
    for (int i = 0; i < MIKUPAN_ITEM_ICON_COUNT; i++)
    {
        if (mikupan_item_icon_slots[i].item_no == item_no)
        {
            return i;
        }
    }

    return -1;
}

static int MikuPanAllocItemIconSlot(int item_no)
{
    for (int i = 0; i < MIKUPAN_ITEM_ICON_COUNT; i++)
    {
        if (mikupan_item_icon_slots[i].item_no < 0
            || mikupan_item_icon_slots[i].data == NULL)
        {
            mikupan_item_icon_slots[i].item_no = item_no;
            mikupan_item_icon_slots[i].load_id = -1;
            return i;
        }
    }

    return -1;
}

static void MikuPanDrawHudRect(float x, float y, float w, float h, int rgb, u_char alpha)
{
    if (w <= 0.0f || h <= 0.0f || alpha == 0)
    {
        return;
    }

    float ndc[4] = {0};
    MikuPan_ConvertPs2ScreenCoordToNDCMaintainAspectRatio(
        ndc,
        (float)MikuPan_GetRenderResolutionWidth(),
        (float)MikuPan_GetRenderResolutionHeight(),
        x,
        y);
    MikuPan_ConvertPs2ScreenCoordToNDCMaintainAspectRatio(
        &ndc[2],
        (float)MikuPan_GetRenderResolutionWidth(),
        (float)MikuPan_GetRenderResolutionHeight(),
        x + w,
        y + h);

    const float r = MikuPan_ConvertColorFloat((u_char)((rgb >> 16) & 0xff));
    const float g = MikuPan_ConvertColorFloat((u_char)((rgb >> 8) & 0xff));
    const float b = MikuPan_ConvertColorFloat((u_char)(rgb & 0xff));
    const float a = MikuPan_ConvertColorFloat(MikuPan_AdjustPS2Alpha(alpha));

    float vertices[4][8] =
    {
        {r, g, b, a, ndc[0], ndc[1], 0.0f, 1.0f},
        {r, g, b, a, ndc[0], ndc[3], 0.0f, 1.0f},
        {r, g, b, a, ndc[2], ndc[1], 0.0f, 1.0f},
        {r, g, b, a, ndc[2], ndc[3], 0.0f, 1.0f},
    };

    MikuPan_BeginLate2DOverlayQueue();
    MikuPan_RenderUntexturedSprite(&vertices[0][0]);
    MikuPan_EndLate2DOverlayQueue();
}

static MikuPan_TextureInfo* MikuPanCreateItemIconTexture(void* data, int desaturate)
{
    if (data == NULL)
    {
        return NULL;
    }

    TIM2_PICTUREHEADER* header = Tim2GetPictureHeader(data, 0);
    if (header == NULL || header->ImageWidth <= 0 || header->ImageHeight <= 0)
    {
        return NULL;
    }

    const int width = header->ImageWidth;
    const int height = header->ImageHeight;
    unsigned char* rgba = (unsigned char*)malloc((size_t)width * (size_t)height * 4);
    if (rgba == NULL)
    {
        return NULL;
    }

    for (int y = 0; y < height; y++)
    {
        for (int x = 0; x < width; x++)
        {
            const u_int colour = Tim2GetTextureColor(header, 0, 0, x, y);
            unsigned char* out = rgba + ((size_t)y * (size_t)width + (size_t)x) * 4;
            unsigned char r = (unsigned char)(colour & 0xff);
            unsigned char g = (unsigned char)((colour >> 16) & 0xff);
            unsigned char b = (unsigned char)((colour >> 8) & 0xff);

            if (desaturate)
            {
                unsigned char luma = (unsigned char)(((int)r * 30 + (int)g * 59 + (int)b * 11) / 100);
                r = luma;
                g = luma;
                b = luma;
            }

            out[0] = r;
            out[1] = g;
            out[2] = b;
            out[3] = (unsigned char)MikuPan_AdjustPS2Alpha((u_char)((colour >> 24) & 0xff));
        }
    }

    const unsigned int texture_id = MikuPan_GPUCreateTextureRGBA8(
        width, height, rgba, width * 4, 0, 0);
    free(rgba);

    if (texture_id == 0)
    {
        return NULL;
    }

    MikuPan_TextureInfo* texture = (MikuPan_TextureInfo*)malloc(sizeof(MikuPan_TextureInfo));
    if (texture == NULL)
    {
        MikuPan_GPUReleaseTexture(texture_id);
        return NULL;
    }

    memset(texture, 0, sizeof(MikuPan_TextureInfo));
    texture->id = texture_id;
    texture->width = width;
    texture->height = height;
    return texture;
}

static MikuPan_TextureInfo* MikuPanGetItemIconTexture(int item_no)
{
    MikuPanEnsureItemIconSlotsInitialised();

    if (item_no < 0 || item_no > 70)
    {
        return NULL;
    }

    int slot_index = MikuPanFindItemIconSlot(item_no);
    if (slot_index < 0)
    {
        slot_index = MikuPanAllocItemIconSlot(item_no);
    }

    if (slot_index < 0)
    {
        return NULL;
    }

    MikuPanItemIconSlot* slot = &mikupan_item_icon_slots[slot_index];
    const int file_no = ITEM_00_TM2 + item_no;

    if (slot->texture != NULL)
    {
        return slot->texture;
    }

    if (slot->load_id != -1)
    {
        if (IsLoadEnd(slot->load_id) == 0)
        {
            return NULL;
        }

        slot->load_id = -1;
        slot->texture = MikuPanCreateItemIconTexture(slot->data, 0);
        return slot->texture;
    }

    if (slot->data == NULL)
    {
        IMG_ARRANGEMENT* img = GetImgArrangementP(file_no);
        if (img == NULL || img->size <= 0)
        {
            return NULL;
        }

        const int size = img->size;
        slot->data = malloc(size);
        if (slot->data == NULL)
        {
            return NULL;
        }

        memset(slot->data, 0, size);
    }

    slot->load_id = LoadReqToHostPointer(file_no, slot->data);
    if (slot->load_id == -1)
    {
        return NULL;
    }

    return NULL;
}

static MikuPan_TextureInfo* MikuPanGetDimmedItemIconTexture(int item_no)
{
    if (MikuPanGetItemIconTexture(item_no) == NULL)
    {
        return NULL;
    }

    int slot_index = MikuPanFindItemIconSlot(item_no);
    if (slot_index < 0)
    {
        return NULL;
    }

    MikuPanItemIconSlot* slot = &mikupan_item_icon_slots[slot_index];
    if (slot->dim_texture == NULL)
    {
        slot->dim_texture = MikuPanCreateItemIconTexture(slot->data, 1);
    }

    return slot->dim_texture;
}

static void MikuPanDrawItemIconTextureClipped(MikuPan_TextureInfo* texture, float x, float y, float scale, u_char r, u_char g, u_char b, u_char alpha, float clip_x, float clip_y, float clip_w, float clip_h)
{
    if (texture == NULL || texture->id == 0 || alpha == 0 || scale <= 0.0f)
    {
        return;
    }

    MikuPan_Rect src = {0};
    MikuPan_Rect dst = {0};
    src.x = 0.0f;
    src.y = 0.0f;
    src.w = (float)texture->width;
    src.h = (float)texture->height;
    dst.x = x;
    dst.y = y;
    dst.w = (float)texture->width * scale;
    dst.h = (float)texture->height * scale;

    const float clip_r = clip_x + clip_w;
    const float clip_b = clip_y + clip_h;
    const float dst_r = dst.x + dst.w;
    const float dst_b = dst.y + dst.h;

    if (dst.x >= clip_r || dst.y >= clip_b || dst_r <= clip_x || dst_b <= clip_y)
    {
        return;
    }

    if (dst.x < clip_x)
    {
        float delta = clip_x - dst.x;
        src.x += delta / scale;
        src.w -= delta / scale;
        dst.x = clip_x;
        dst.w -= delta;
    }

    if (dst.y < clip_y)
    {
        float delta = clip_y - dst.y;
        src.y += delta / scale;
        src.h -= delta / scale;
        dst.y = clip_y;
        dst.h -= delta;
    }

    if (dst.x + dst.w > clip_r)
    {
        float delta = dst.x + dst.w - clip_r;
        src.w -= delta / scale;
        dst.w -= delta;
    }

    if (dst.y + dst.h > clip_b)
    {
        float delta = dst.y + dst.h - clip_b;
        src.h -= delta / scale;
        dst.h -= delta;
    }

    if (src.w <= 0.0f || src.h <= 0.0f || dst.w <= 0.0f || dst.h <= 0.0f)
    {
        return;
    }

    MikuPan_BeginLate2DOverlayQueue();
    MikuPan_RenderSprite(src, dst, r, g, b, alpha, texture);
    MikuPan_EndLate2DOverlayQueue();
}

static void MikuPanDrawItemIconTextureCenteredClipped(MikuPan_TextureInfo* texture, float center_x, float center_y, float scale, u_char r, u_char g, u_char b, u_char alpha, float clip_x, float clip_y, float clip_w, float clip_h)
{
    if (texture == NULL)
    {
        return;
    }

    float x = center_x - (float)texture->width * scale * 0.5f;
    float y = center_y - (float)texture->height * scale * 0.5f;
    MikuPanDrawItemIconTextureClipped(texture, x - 1.6f, y + 1.6f, scale, 0, 0, 0, MikuPanItemIconAlpha(alpha * 0.42f), clip_x, clip_y, clip_w, clip_h);
    MikuPanDrawItemIconTextureClipped(texture, x, y, scale, r, g, b, alpha, clip_x, clip_y, clip_w, clip_h);
}

static void MikuPanDrawItemIconCenteredClipped(int item_no, float center_x, float center_y, float scale, u_char alpha, float clip_x, float clip_y, float clip_w, float clip_h)
{
    MikuPanDrawItemIconTextureCenteredClipped(
        MikuPanGetItemIconTexture(item_no),
        center_x,
        center_y,
        scale,
        0x80,
        0x80,
        0x80,
        alpha,
        clip_x,
        clip_y,
        clip_w,
        clip_h);
}

static int MikuPanLerpRgb(int top_rgb, int bottom_rgb, float t)
{
    int tr = (top_rgb >> 16) & 0xff;
    int tg = (top_rgb >> 8) & 0xff;
    int tb = top_rgb & 0xff;
    int br = (bottom_rgb >> 16) & 0xff;
    int bg = (bottom_rgb >> 8) & 0xff;
    int bb = bottom_rgb & 0xff;
    int r = tr + (int)((br - tr) * t + 0.5f);
    int g = tg + (int)((bg - tg) * t + 0.5f);
    int b = tb + (int)((bb - tb) * t + 0.5f);
    return (r << 16) | (g << 8) | b;
}

static void MikuPanDrawItemIconSlotGradient(float x, float y, float w, float h, float alpha)
{
    const int steps = 12;
    const float step_h = h / (float)steps;
    const u_char grad_alpha = MikuPanItemIconAlpha(alpha);

    for (int i = 0; i < steps; i++)
    {
        float t = steps > 1 ? i / (float)(steps - 1) : 0.0f;
        int rgb = MikuPanLerpRgb(0x010101, 0x0f0e0c, t);
        float draw_y = y + step_h * i;
        float draw_h = i == steps - 1 ? y + h - draw_y : step_h + 0.25f;
        MikuPanDrawHudRect(x, draw_y, w, draw_h, rgb, grad_alpha);
    }
}

static void MikuPanDrawItemIconSlotFrame(float x, float y, float outer_w, float outer_h, float alpha, float* clip_rect)
{
    if (outer_w <= 4.0f || outer_h <= 4.0f)
    {
        return;
    }

    const float inner_w = outer_w - 2.0f;
    const float inner_h = outer_h - 2.0f;
    const float slot_w = outer_w - 4.0f;
    const float slot_h = outer_h - 4.0f;
    const u_char frame_alpha = MikuPanItemIconAlpha(alpha);

    MikuPanDrawHudRect(x, y, outer_w, outer_h, 0x1b1513, frame_alpha);
    MikuPanDrawHudRect(x + 1.0f, y + 1.0f, inner_w, inner_h, 0x0a0807, frame_alpha);
    MikuPanDrawItemIconSlotGradient(x + 2.0f, y + 2.0f, slot_w, slot_h, alpha);

    if (clip_rect != NULL)
    {
        clip_rect[0] = x + 2.0f;
        clip_rect[1] = y + 2.0f;
        clip_rect[2] = slot_w;
        clip_rect[3] = slot_h;
    }
}

static int MikuPanFinderFilmSwapInputDirection()
{
    if (!MikuPan_FinderDpadFilmSwapEnabled())
    {
        return 0;
    }

    u_short up = DPAD_UP_PRESSED();
    u_short down = DPAD_DOWN_PRESSED();

    if (up == 1 || (up > 25 && up % 8 == 1))
    {
        return -1;
    }

    if (down == 1 || (down > 25 && down % 8 == 1))
    {
        return 1;
    }

    return 0;
}

static int MikuPanFindNextFinderFilm(int current, int direction)
{
    if (direction == 0)
    {
        return current;
    }

    if (current < 1 || current > 4)
    {
        current = direction > 0 ? 0 : 5;
    }

    for (int i = 1; i <= 4; i++)
    {
        int film = current + direction * i;
        while (film < 1)
        {
            film += 4;
        }
        while (film > 4)
        {
            film -= 4;
        }

        if (poss_item[film] != 0)
        {
            return film;
        }
    }

    return current;
}

void MikuPan_QuickSwapFinderFilm(void)
{
    int direction = MikuPanFinderFilmSwapInputDirection();
    if (direction == 0)
    {
        return;
    }

    int old_film = plyr_wrk.film_no;
    int new_film = MikuPanFindNextFinderFilm(old_film, direction);

    if (new_film == old_film || new_film < 1 || new_film > 4)
    {
        return;
    }

    plyr_wrk.film_no = new_film;
    PlyrChargeCtrl(0xff, NULL);
    MikuPanNotifyFinderFilmSwitch(old_film, new_film, direction);
    mikupan_finder_film_counter_snap = 1;
    SeStartFix(SE_SEALING, 0, 0x1000, 0xa00, 0);
}

static void MikuPanNotifyFinderFilmSwitch(int old_film_no, int new_film_no, int direction)
{
    if (new_film_no < 1 || new_film_no > 4)
    {
        return;
    }

    if (old_film_no < 1 || old_film_no > 4)
    {
        old_film_no = new_film_no;
    }

    mikupan_finder_film_icon_previous = old_film_no;
    mikupan_finder_film_icon_current = new_film_no;
    mikupan_finder_film_icon_direction = direction < 0 ? -1 : direction > 0 ? 1 : 0;
    mikupan_finder_film_icon_timer =
        mikupan_finder_film_icon_direction != 0
            ? MIKUPAN_FINDER_FILM_ICON_SWAP_FRAMES
            : 0;
}

int MikuPan_ConsumeFinderFilmCounterSnap(void)
{
    int value = mikupan_finder_film_counter_snap;
    mikupan_finder_film_counter_snap = 0;
    return value;
}

void MikuPan_DrawFinderFilmIcon(short int pos_x, short int pos_y, u_char fade_alpha)
{
    if (!MikuPan_FinderDpadFilmSwapEnabled())
    {
        return;
    }

    int film_no = plyr_wrk.film_no;
    if (film_no < 1 || film_no > 4)
    {
        return;
    }

    if (mikupan_finder_film_icon_current != film_no
        && mikupan_finder_film_icon_timer == 0)
    {
        int direction = 0;
        if (mikupan_finder_film_icon_current >= 1
            && mikupan_finder_film_icon_current <= 4)
        {
            direction = film_no > mikupan_finder_film_icon_current ? 1 : -1;
        }
        MikuPanNotifyFinderFilmSwitch(
            mikupan_finder_film_icon_current, film_no, direction);
    }

    static constexpr float film_slot_x = 103.0f;
    static constexpr float film_slot_y = 385.0f;
    static constexpr float film_icon_scale = 0.180f;

    float base_alpha = fade_alpha * 128.0f / 100.0f;
    float slot_x = pos_x + film_slot_x;
    float slot_y = pos_y + film_slot_y;
    float icon_center_x = slot_x + 30.0f;
    float icon_center_y = slot_y + 20.0f;
    float scale = film_icon_scale;
    float clip_rect[4] = {0.0f, 0.0f, 0.0f, 0.0f};

    MikuPanDrawItemIconSlotFrame(slot_x, slot_y, 60.0f, 40.0f, base_alpha, clip_rect);

    if (mikupan_finder_film_icon_timer > 0
        && mikupan_finder_film_icon_previous >= 1
        && mikupan_finder_film_icon_previous <= 4
        && mikupan_finder_film_icon_direction != 0)
    {
        float progress =
            (MIKUPAN_FINDER_FILM_ICON_SWAP_FRAMES
             - mikupan_finder_film_icon_timer)
            / (float)MIKUPAN_FINDER_FILM_ICON_SWAP_FRAMES;
        float offset = 32.0f;
        float direction = (float)mikupan_finder_film_icon_direction;

        MikuPanDrawItemIconCenteredClipped(
            MikuPanFilmNoToItemIconNo(mikupan_finder_film_icon_previous),
            icon_center_x,
            icon_center_y - direction * offset * progress,
            scale,
            MikuPanItemIconAlpha(base_alpha * (1.0f - progress)),
            clip_rect[0], clip_rect[1], clip_rect[2], clip_rect[3]);
        MikuPanDrawItemIconCenteredClipped(
            MikuPanFilmNoToItemIconNo(mikupan_finder_film_icon_current),
            icon_center_x,
            icon_center_y + direction * offset * (1.0f - progress),
            scale,
            MikuPanItemIconAlpha(base_alpha * progress),
            clip_rect[0], clip_rect[1], clip_rect[2], clip_rect[3]);

        mikupan_finder_film_icon_timer--;
        return;
    }

    mikupan_finder_film_icon_timer = 0;
    mikupan_finder_film_icon_current = film_no;
    mikupan_finder_film_icon_previous = film_no;
    MikuPanDrawItemIconCenteredClipped(
        MikuPanFilmNoToItemIconNo(film_no),
        icon_center_x,
        icon_center_y,
        scale,
        MikuPanItemIconAlpha(base_alpha),
        clip_rect[0], clip_rect[1], clip_rect[2], clip_rect[3]);
}

void MikuPan_DrawMirrorStoneHudIcon(short int pos_x, short int pos_y, u_char fade_alpha)
{
    if (!mikupan_mirror_stone_hud_enabled || fade_alpha == 0)
    {
        return;
    }

    static constexpr float stone_slot_x = 245.0f;
    static constexpr float stone_slot_y = 50.0f;
    static constexpr float stone_slot_w = 28.0f;
    static constexpr float stone_slot_h = 18.0f;
    static constexpr float stone_icon_scale = 0.100f;

    float base_alpha = fade_alpha;
    float slot_x = pos_x + stone_slot_x;
    float slot_y = pos_y + stone_slot_y;
    float icon_center_x = slot_x + stone_slot_w * 0.5f;
    float icon_center_y = slot_y + stone_slot_h * 0.5f;
    float clip_rect[4] = {0.0f, 0.0f, 0.0f, 0.0f};

    MikuPanDrawItemIconSlotFrame(slot_x, slot_y, stone_slot_w, stone_slot_h, base_alpha, clip_rect);

    if (poss_item[8] == 0)
    {
        MikuPanDrawItemIconTextureCenteredClipped(
            MikuPanGetDimmedItemIconTexture(8),
            icon_center_x,
            icon_center_y,
            stone_icon_scale,
            0x50,
            0x50,
            0x50,
            MikuPanItemIconAlpha(base_alpha * 0.55f),
            clip_rect[0],
            clip_rect[1],
            clip_rect[2],
            clip_rect[3]);
        return;
    }

    MikuPanDrawItemIconCenteredClipped(
        8,
        icon_center_x,
        icon_center_y,
        stone_icon_scale,
        MikuPanItemIconAlpha(base_alpha),
        clip_rect[0],
        clip_rect[1],
        clip_rect[2],
        clip_rect[3]);
}

void MikuPan_SetMirrorStoneHudEnabled(int enabled)
{
    mikupan_mirror_stone_hud_enabled = enabled ? 1 : 0;
}

int MikuPan_MirrorStoneHudEnabled(void)
{
    return mikupan_mirror_stone_hud_enabled;
}

void MikuPan_ResetItemIconHud(void)
{
    mikupan_finder_film_icon_current = -1;
    mikupan_finder_film_icon_previous = -1;
    mikupan_finder_film_icon_direction = 0;
    mikupan_finder_film_icon_timer = 0;
    mikupan_finder_film_counter_snap = 0;
}
