#include "mikupan/mikupan_effect_compat.h"

#include <string.h>

#include "sce/libgraph.h"
#include "mikupan/mikupan_utils.h"
#include "mikupan/mikupan_config.h"
#include "mikupan/rendering/mikupan_renderer.h"
#include "mikupan/rendering/mikupan_gpu.h"

#define MIKUPAN_GUS_QUEUE_MAX_VERTICES ((600 + 200 + (4 * 80)) * 6)
#define MIKUPAN_GUS_QUEUE_MAX_BATCHES 16

typedef struct MikuPan_GusDrawBatch
{
    u_long tex0;
    int first_vertex;
    int vertex_count;
} MikuPan_GusDrawBatch;

static float g_mikupan_gus_vertices[MIKUPAN_GUS_QUEUE_MAX_VERTICES][12];
static int g_mikupan_gus_vertex_count = 0;
static MikuPan_GusDrawBatch g_mikupan_gus_batches[MIKUPAN_GUS_QUEUE_MAX_BATCHES];
static int g_mikupan_gus_batch_count = 0;

#define MIKUPAN_SCREEN_DITHER_TBP0 0x37FC
#define MIKUPAN_SCREEN_DITHER_CBP  0x383C
#define MIKUPAN_SCREEN_DITHER_PRESENTATION_SCALE 2.0f

int MikuPan_IsScreenDitherTexture(const sceGsTex0 *tex)
{
    return tex != NULL &&
           tex->TBP0 == MIKUPAN_SCREEN_DITHER_TBP0 &&
           tex->TBW == 2 &&
           tex->PSM == SCE_GS_PSMT8 &&
           tex->TW == 7 &&
           tex->TH == 7 &&
           tex->TCC == 1 &&
           tex->CBP == MIKUPAN_SCREEN_DITHER_CBP;
}

int MikuPan_GetScreenDitherNearestSampler(const sceGsTex0 *tex, int original_nearest_sampler)
{
    if (!MikuPan_IsScreenDitherTexture(tex))
    {
        return original_nearest_sampler;
    }

    return mikupan_configuration.renderer.dither_mode == 0 ? 1 : 0;
}

int MikuPan_IsScreenDitherSoft(const sceGsTex0 *tex)
{
    return MikuPan_IsScreenDitherTexture(tex) &&
           mikupan_configuration.renderer.dither_mode == 1;
}

void MikuPan_ExpandScreenEffectQuadNoStretch(float *vertices, int vertex_count)
{
    int i;
    float min_x;
    float max_x;
    float min_y;
    float max_y;
    float u_at_min_x = 0.0f;
    float u_at_max_x = 0.0f;
    float v_at_min_y = 0.0f;
    float v_at_max_y = 0.0f;
    int min_x_count = 0;
    int max_x_count = 0;
    int min_y_count = 0;
    int max_y_count = 0;
    float du_per_ndc;
    float dv_per_ndc;

    if (vertices == NULL || vertex_count < 4)
    {
        return;
    }

    min_x = max_x = vertices[8];
    min_y = max_y = vertices[9];

    for (i = 1; i < vertex_count; i++)
    {
        const float *vertex = vertices + i * 12;
        const float x = vertex[8];
        const float y = vertex[9];

        if (x < min_x)
        {
            min_x = x;
        }
        if (x > max_x)
        {
            max_x = x;
        }
        if (y < min_y)
        {
            min_y = y;
        }
        if (y > max_y)
        {
            max_y = y;
        }
    }

    if ((max_x - min_x) <= 0.00001f || (max_y - min_y) <= 0.00001f)
    {
        return;
    }

    if (min_x <= -0.999f && max_x >= 0.999f &&
        min_y <= -0.999f && max_y >= 0.999f)
    {
        return;
    }

    for (i = 0; i < vertex_count; i++)
    {
        const float *vertex = vertices + i * 12;
        const float x = vertex[8];
        const float y = vertex[9];

        if ((x - min_x) <= (max_x - x))
        {
            u_at_min_x += vertex[0];
            min_x_count++;
        }
        else
        {
            u_at_max_x += vertex[0];
            max_x_count++;
        }

        if ((y - min_y) <= (max_y - y))
        {
            v_at_min_y += vertex[1];
            min_y_count++;
        }
        else
        {
            v_at_max_y += vertex[1];
            max_y_count++;
        }
    }

    if (min_x_count == 0 || max_x_count == 0 || min_y_count == 0 || max_y_count == 0)
    {
        return;
    }

    u_at_min_x /= (float)min_x_count;
    u_at_max_x /= (float)max_x_count;
    v_at_min_y /= (float)min_y_count;
    v_at_max_y /= (float)max_y_count;

    du_per_ndc = (u_at_max_x - u_at_min_x) / (max_x - min_x);
    dv_per_ndc = (v_at_max_y - v_at_min_y) / (max_y - min_y);

    for (i = 0; i < vertex_count; i++)
    {
        float *vertex = vertices + i * 12;
        const float old_x = vertex[8];
        const float old_y = vertex[9];
        const float target_x = (old_x - min_x) <= (max_x - old_x) ? -1.0f : 1.0f;
        const float target_y = (old_y - min_y) <= (max_y - old_y) ? -1.0f : 1.0f;

        vertex[0] = u_at_min_x + (target_x - min_x) * du_per_ndc;
        vertex[1] = v_at_min_y + (target_y - min_y) * dv_per_ndc;
        vertex[8] = target_x;
        vertex[9] = target_y;
    }
}

void MikuPan_ApplyScreenDitherPresentationScale(const sceGsTex0 *tex, float *vertices, int vertex_count)
{
    int i;
    float base_u;
    float base_v;

    if (!MikuPan_IsScreenDitherTexture(tex) || vertices == NULL || vertex_count <= 0)
    {
        return;
    }

    /*
     * The PS2 dither pass is authored as a 128x128 GS texture tiled over the
     * 640x448 scene. MikuPan's native high-resolution sprite path renders
     * that tile too crisply/small compared with original hardware/capture,
     * where the pattern reads as a coarser crawling darkness over the 3D scene.
     *
     * Keep SubDither3/SubDither4's original UV/clamp math untouched and apply
     * only a presentation-side UV magnification for this specific GS dither
     * texture in the native renderer mirror. Fullscreen coverage is handled by
     * MikuPan_ExpandScreenEffectQuadNoStretch() after this scale is applied,
     * so the extra coverage is tiled at the corrected dither density rather
     * than stretching the original quad.
     */
    base_u = vertices[0];
    base_v = vertices[1];

    for (i = 1; i < vertex_count; i++)
    {
        const float u = vertices[i * 12 + 0];
        const float v = vertices[i * 12 + 1];

        if (u < base_u)
        {
            base_u = u;
        }

        if (v < base_v)
        {
            base_v = v;
        }
    }

    for (i = 0; i < vertex_count; i++)
    {
        float *vertex = vertices + i * 12;
        vertex[0] = base_u + (vertex[0] - base_u) / MIKUPAN_SCREEN_DITHER_PRESENTATION_SCALE;
        vertex[1] = base_v + (vertex[1] - base_v) / MIKUPAN_SCREEN_DITHER_PRESENTATION_SCALE;
    }
}



int MikuPan_TryScreenNegativeOverlay(u_char r, u_char g, u_char b, u_char alp, u_char alp2)
{
    /*
     * Native replacement for EF_NEGA's framebuffer-feedback draw.  The
     * original SubNega() body remains in effect_scr.c as the authoritative PS2
     * path/fallback; this hook is only used when MikuPan can express the same
     * fade data over the composed SDL GPU scene framebuffer.
     */
    const float strength = (float)alp2 / 128.0f;

    (void)alp;

    MikuPan_SetScreenNegativeOverlayActiveForFrame(
        MikuPan_ConvertScaleColor(r),
        MikuPan_ConvertScaleColor(g),
        MikuPan_ConvertScaleColor(b),
        strength);

    return 1;
}

void MikuPan_QueueGusTriangles(u_long tex0, const float *vertices, int vertex_count)
{
    int batch_index;
    int copy_count;

    if (vertices == NULL || vertex_count <= 0)
    {
        return;
    }

    batch_index = -1;

    if (g_mikupan_gus_batch_count > 0 &&
        g_mikupan_gus_batches[g_mikupan_gus_batch_count - 1].tex0 == tex0)
    {
        batch_index = g_mikupan_gus_batch_count - 1;
    }
    else if (g_mikupan_gus_batch_count < MIKUPAN_GUS_QUEUE_MAX_BATCHES)
    {
        batch_index = g_mikupan_gus_batch_count;
        g_mikupan_gus_batches[batch_index].tex0 = tex0;
        g_mikupan_gus_batches[batch_index].first_vertex = g_mikupan_gus_vertex_count;
        g_mikupan_gus_batches[batch_index].vertex_count = 0;
        g_mikupan_gus_batch_count++;
    }

    if (batch_index < 0)
    {
        return;
    }

    copy_count = vertex_count;
    if (copy_count > MIKUPAN_GUS_QUEUE_MAX_VERTICES - g_mikupan_gus_vertex_count)
    {
        copy_count = MIKUPAN_GUS_QUEUE_MAX_VERTICES - g_mikupan_gus_vertex_count;
    }

    if (copy_count <= 0)
    {
        return;
    }

    memcpy(&g_mikupan_gus_vertices[g_mikupan_gus_vertex_count][0],
           vertices,
           (size_t)copy_count * 12 * sizeof(float));

    g_mikupan_gus_vertex_count += copy_count;
    g_mikupan_gus_batches[batch_index].vertex_count += copy_count;
}

void MikuPan_DrawQueuedGusObjects(void)
{
    int i;

    if (g_mikupan_gus_vertex_count <= 0)
    {
        g_mikupan_gus_batch_count = 0;
        return;
    }

    for (i = 0; i < g_mikupan_gus_batch_count; i++)
    {
        if (g_mikupan_gus_batches[i].vertex_count <= 0)
        {
            continue;
        }

        MikuPan_RenderTexturedTriangles3DWithState(
            (sceGsTex0 *)&g_mikupan_gus_batches[i].tex0,
            &g_mikupan_gus_vertices[g_mikupan_gus_batches[i].first_vertex][0],
            g_mikupan_gus_batches[i].vertex_count,
            MIKUPAN_DEPTH_LEQUAL,
            MIKUPAN_GPU_BLEND_NORMAL);
    }

    g_mikupan_gus_vertex_count = 0;
    g_mikupan_gus_batch_count = 0;
}
