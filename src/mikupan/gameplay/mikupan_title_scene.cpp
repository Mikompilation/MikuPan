#include "mikupan/gameplay/mikupan_title_scene.h"

#include "common.h"
#include "enums.h"
#include "graphics/graph2d/effect.h"
#include "graphics/graph2d/effect_sub.h"
#include "graphics/graph3d/libsg.h"
#include "graphics/graph3d/sglib.h"
#include "mikupan/mikupan_rng.h"
#include "sce/libvu0.h"

#include <math.h>
#include <string.h>

#define MIKUPAN_TITLE_SCENE_DUST_NEAR_MAX 40
#define MIKUPAN_TITLE_SCENE_DUST_FAR_MAX 56
#define MIKUPAN_TITLE_SCENE_DUST_MAX                                           \
    (MIKUPAN_TITLE_SCENE_DUST_NEAR_MAX + MIKUPAN_TITLE_SCENE_DUST_FAR_MAX)
#define MIKUPAN_TITLE_SCENE_DUST_SPAWN_PER_FRAME 2
#define MIKUPAN_TITLE_SCENE_DUST_TEXNO 6
#define MIKUPAN_TITLE_SCENE_PI 3.1415927f
#define MIKUPAN_TITLE_SCENE_CANDLE_X 3025.0f
#define MIKUPAN_TITLE_SCENE_CANDLE_Y -570.0f
#define MIKUPAN_TITLE_SCENE_CANDLE_Z 2460.0f
#define MIKUPAN_TITLE_SCENE_CANDLE_LIGHT_POWER 1100.0f
#define MIKUPAN_TITLE_SCENE_CANDLE_FLAME_SCALE_A 0.5f
#define MIKUPAN_TITLE_SCENE_CANDLE_FLAME_SCALE_B 2.0f

typedef struct
{
    int active;
    float age;
    float life;
    float size;
    float alpha;
    int far_layer;
    sceVu0FVECTOR pos;
    sceVu0FVECTOR vel;
} MIKUPAN_TITLE_SCENE_DUST;

static int g_title_scene_active = 0;
static int g_title_scene_seeded = 0;
static int g_title_scene_msn_no = -1;
static int g_title_scene_room_no = -1;
static int g_title_scene_frame = 0;
static void* g_title_scene_flame = NULL;
static sceVu0FVECTOR g_title_scene_flame_pos = {
    MIKUPAN_TITLE_SCENE_CANDLE_X,
    MIKUPAN_TITLE_SCENE_CANDLE_Y,
    MIKUPAN_TITLE_SCENE_CANDLE_Z,
    1.0f,
};
static SgCAMERA g_title_scene_camera = {0};
static MIKUPAN_TITLE_SCENE_DUST
    g_title_scene_dust[MIKUPAN_TITLE_SCENE_DUST_MAX] = {0};

static float MikuPan_TitleSceneRand01(void)
{
    return (float) MikuPan_Rand() / (float) MikuPan_RAND_MAX;
}

static float MikuPan_TitleSceneRandRange(float min_value, float max_value)
{
    return min_value + (max_value - min_value) * MikuPan_TitleSceneRand01();
}

static float MikuPan_TitleSceneClampFloat(float value, float min_value,
                                          float max_value)
{
    if (value < min_value)
    {
        return min_value;
    }

    if (value > max_value)
    {
        return max_value;
    }

    return value;
}

static float MikuPan_TitleSceneCandleFlicker(void)
{
    static int last_frame = -1;
    static int next_target_frame = 0;
    static float current = 1.0f;
    static float target = 1.0f;

    int frame = g_title_scene_frame;
    float t = (float) frame;

    if (frame != last_frame)
    {
        if (frame >= next_target_frame)
        {
            target = MikuPan_TitleSceneRandRange(0.92f, 1.07f);
            next_target_frame =
                frame + (int) MikuPan_TitleSceneRandRange(2.0f, 7.0f);
        }

        current += (target - current) * 0.32f;
        last_frame = frame;
    }

    return MikuPan_TitleSceneClampFloat(current
                                            + SgSinf(t * 0.43f + 0.7f) * 0.012f
                                            + SgSinf(t * 1.17f + 2.1f) * 0.006f,
                                        0.88f, 1.08f);
}
static void MikuPan_TitleSceneNormalize3(float* v)
{
    float len = SgSqrtf(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);

    if (len <= 0.00001f)
    {
        v[0] = 0.0f;
        v[1] = 0.0f;
        v[2] = 1.0f;
        v[3] = 1.0f;
        return;
    }

    v[0] /= len;
    v[1] /= len;
    v[2] /= len;
    v[3] = 1.0f;
}

static void MikuPan_TitleSceneCross3(float* out, const float* a, const float* b)
{
    out[0] = a[1] * b[2] - a[2] * b[1];
    out[1] = a[2] * b[0] - a[0] * b[2];
    out[2] = a[0] * b[1] - a[1] * b[0];
    out[3] = 1.0f;
}

static void MikuPan_TitleSceneComputeBasis(const SgCAMERA* camera,
                                           sceVu0FVECTOR forward,
                                           sceVu0FVECTOR right,
                                           sceVu0FVECTOR up)
{
    forward[0] = camera->i[0] - camera->p[0];
    forward[1] = camera->i[1] - camera->p[1];
    forward[2] = camera->i[2] - camera->p[2];
    forward[3] = 1.0f;
    MikuPan_TitleSceneNormalize3(forward);

    Vu0CopyVector(up, camera->yd);

    if (SgSqrtf(up[0] * up[0] + up[1] * up[1] + up[2] * up[2]) <= 0.00001f)
    {
        up[0] = 0.0f;
        up[1] = 1.0f;
        up[2] = 0.0f;
        up[3] = 1.0f;
    }
    else
    {
        MikuPan_TitleSceneNormalize3(up);
    }

    MikuPan_TitleSceneCross3(right, forward, up);

    if (SgSqrtf(right[0] * right[0] + right[1] * right[1] + right[2] * right[2])
        <= 0.00001f)
    {
        right[0] = 1.0f;
        right[1] = 0.0f;
        right[2] = 0.0f;
        right[3] = 1.0f;
    }
    else
    {
        MikuPan_TitleSceneNormalize3(right);
    }

    MikuPan_TitleSceneCross3(up, right, forward);
    MikuPan_TitleSceneNormalize3(up);
}

static void MikuPan_TitleSceneResetDust(void)
{
    memset(g_title_scene_dust, 0, sizeof(g_title_scene_dust));
    g_title_scene_seeded = 0;
}

static void MikuPan_TitleSceneResetFlame(void)
{
    if (g_title_scene_flame != NULL)
    {
        ResetEffects(g_title_scene_flame);
        g_title_scene_flame = NULL;
    }
}

static void MikuPan_TitleSceneEnsureFlame(void)
{
    EFFECT_CONT* effect = (EFFECT_CONT*) g_title_scene_flame;

    if (effect != NULL && effect->dat.uc8[0] == EF_FIRE)
    {
        return;
    }

    g_title_scene_flame_pos[0] = MIKUPAN_TITLE_SCENE_CANDLE_X;
    g_title_scene_flame_pos[1] = MIKUPAN_TITLE_SCENE_CANDLE_Y;
    g_title_scene_flame_pos[2] = MIKUPAN_TITLE_SCENE_CANDLE_Z;
    g_title_scene_flame_pos[3] = 1.0f;

    g_title_scene_flame =
        SetFireEffect(2, 3, g_title_scene_flame_pos, 0x50, 0x34, 0x18, MIKUPAN_TITLE_SCENE_CANDLE_FLAME_SCALE_A, 0xff, 0xc8, 0x80, MIKUPAN_TITLE_SCENE_CANDLE_FLAME_SCALE_B);
}

static void MikuPan_TitleSceneSpawnDust(int index, int random_age)
{
    sceVu0FVECTOR forward;
    sceVu0FVECTOR right;
    sceVu0FVECTOR up;
    sceVu0FVECTOR center;
    MIKUPAN_TITLE_SCENE_DUST* dust = &g_title_scene_dust[index];
    float right_offset;
    float up_offset;
    float forward_offset;
    float forward_center;
    float up_center;
    float drift_scale;
    float fade_age;

    MikuPan_TitleSceneComputeBasis(&g_title_scene_camera, forward, right, up);

    dust->far_layer = index >= MIKUPAN_TITLE_SCENE_DUST_NEAR_MAX;

    if (dust->far_layer)
    {
        forward_center = MikuPan_TitleSceneRandRange(1050.0f, 1550.0f);
        up_center = MikuPan_TitleSceneRandRange(40.0f, 180.0f);
        right_offset = MikuPan_TitleSceneRandRange(-1250.0f, 1250.0f);
        up_offset = MikuPan_TitleSceneRandRange(-380.0f, 520.0f);
        forward_offset = MikuPan_TitleSceneRandRange(-320.0f, 360.0f);
        dust->life = MikuPan_TitleSceneRandRange(300.0f, 520.0f);
        dust->size = MikuPan_TitleSceneRandRange(5.0f, 11.0f);
        dust->alpha = MikuPan_TitleSceneRandRange(8.0f, 17.0f);
        drift_scale = 0.45f;
    }
    else
    {
        forward_center = MikuPan_TitleSceneRandRange(540.0f, 780.0f);
        up_center = MikuPan_TitleSceneRandRange(20.0f, 90.0f);
        right_offset = MikuPan_TitleSceneRandRange(-720.0f, 720.0f);
        up_offset = MikuPan_TitleSceneRandRange(-220.0f, 320.0f);
        forward_offset = MikuPan_TitleSceneRandRange(-160.0f, 160.0f);
        dust->life = MikuPan_TitleSceneRandRange(220.0f, 380.0f);
        dust->size = MikuPan_TitleSceneRandRange(10.0f, 20.0f);
        dust->alpha = MikuPan_TitleSceneRandRange(12.0f, 24.0f);
        drift_scale = 0.75f;
    }

    center[0] = g_title_scene_camera.p[0] + forward[0] * forward_center
                + up[0] * up_center;
    center[1] = g_title_scene_camera.p[1] + forward[1] * forward_center
                + up[1] * up_center;
    center[2] = g_title_scene_camera.p[2] + forward[2] * forward_center
                + up[2] * up_center;
    center[3] = 1.0f;

    dust->active = 1;
    dust->pos[0] = center[0] + right[0] * right_offset + up[0] * up_offset
                   + forward[0] * forward_offset;
    dust->pos[1] = center[1] + right[1] * right_offset + up[1] * up_offset
                   + forward[1] * forward_offset;
    dust->pos[2] = center[2] + right[2] * right_offset + up[2] * up_offset
                   + forward[2] * forward_offset;
    dust->pos[3] = 1.0f;

    dust->vel[0] = (right[0] * MikuPan_TitleSceneRandRange(-0.8f, 0.8f)
                    + up[0] * MikuPan_TitleSceneRandRange(-0.15f, 0.35f)
                    + forward[0] * MikuPan_TitleSceneRandRange(-0.10f, 0.28f))
                   * drift_scale;
    dust->vel[1] = (right[1] * MikuPan_TitleSceneRandRange(-0.8f, 0.8f)
                    + up[1] * MikuPan_TitleSceneRandRange(-0.15f, 0.35f)
                    + forward[1] * MikuPan_TitleSceneRandRange(-0.10f, 0.28f))
                   * drift_scale;
    dust->vel[2] = (right[2] * MikuPan_TitleSceneRandRange(-0.8f, 0.8f)
                    + up[2] * MikuPan_TitleSceneRandRange(-0.15f, 0.35f)
                    + forward[2] * MikuPan_TitleSceneRandRange(-0.10f, 0.28f))
                   * drift_scale;
    dust->vel[3] = 1.0f;

    fade_age =
        random_age ? MikuPan_TitleSceneRandRange(0.0f, dust->life) : 0.0f;
    dust->age = fade_age;
}

static void MikuPan_TitleSceneEnsureDustSeeded(void)
{
    int i;

    if (g_title_scene_seeded)
    {
        return;
    }

    for (i = 0; i < MIKUPAN_TITLE_SCENE_DUST_MAX; i++)
    {
        MikuPan_TitleSceneSpawnDust(i, 1);
    }

    g_title_scene_seeded = 1;
}

static void MikuPan_TitleSceneUpdateDust(void)
{
    int i;
    int spawned = 0;

    MikuPan_TitleSceneEnsureDustSeeded();

    for (i = 0; i < MIKUPAN_TITLE_SCENE_DUST_MAX; i++)
    {
        MIKUPAN_TITLE_SCENE_DUST* dust = &g_title_scene_dust[i];

        if (!dust->active)
        {
            if (spawned < MIKUPAN_TITLE_SCENE_DUST_SPAWN_PER_FRAME)
            {
                MikuPan_TitleSceneSpawnDust(i, 0);
                spawned++;
            }

            continue;
        }

        dust->age += 1.0f;
        dust->pos[0] += dust->vel[0];
        dust->pos[1] += dust->vel[1];
        dust->pos[2] += dust->vel[2];

        if (dust->age >= dust->life)
        {
            dust->active = 0;
        }
    }
}

static void
MikuPan_TitleSceneRenderDustParticle(const MIKUPAN_TITLE_SCENE_DUST* dust,
                                     const DRAW_ENV* draw_env)
{
    sceVu0FMATRIX wlm;
    float life_t = dust->age / dust->life;
    float fade = SgSinf(life_t * MIKUPAN_TITLE_SCENE_PI);
    int core_alpha;
    int blur_alpha;

    if (fade <= 0.0f)
    {
        return;
    }

    core_alpha =
        (int) MikuPan_TitleSceneClampFloat(dust->alpha * fade, 0.0f, 127.0f);
    blur_alpha = (int) MikuPan_TitleSceneClampFloat(dust->alpha * fade * 0.35f,
                                                    0.0f, 127.0f);

    if (core_alpha <= 0 && blur_alpha <= 0)
    {
        return;
    }

    sceVu0UnitMatrix(wlm);
    sceVu0TransMatrix(wlm, wlm, (float*) dust->pos);

    if (blur_alpha > 0)
    {
        Set3DPosTexure(wlm, (DRAW_ENV*) draw_env,
                       MIKUPAN_TITLE_SCENE_DUST_TEXNO, dust->size * 1.8f,
                       dust->size * 1.8f, 0xd6, 0xdc, 0xe6,
                       (u_char) blur_alpha);
    }

    if (core_alpha > 0)
    {
        Set3DPosTexure(wlm, (DRAW_ENV*) draw_env,
                       MIKUPAN_TITLE_SCENE_DUST_TEXNO, dust->size, dust->size,
                       0xf0, 0xf2, 0xf6, (u_char) core_alpha);
    }
}

static int MikuPan_TitleScenePickPointLightSlot(const LIGHT_PACK* light_pack)
{
    int i;
    int weakest = 0;
    float weakest_power;

    if (light_pack->point_num < 3)
    {
        return light_pack->point_num;
    }

    weakest_power = light_pack->point[0].power;

    for (i = 1; i < 3; i++)
    {
        if (light_pack->point[i].power < weakest_power)
        {
            weakest_power = light_pack->point[i].power;
            weakest = i;
        }
    }

    return weakest;
}

static int MikuPan_TitleScenePickParallelLightSlot(const LIGHT_PACK* light_pack)
{
    int i;
    int weakest = 0;
    float weakest_power;

    if (light_pack->parallel_num < 3)
    {
        return light_pack->parallel_num;
    }

    weakest_power = light_pack->parallel[0].diffuse[0]
                    + light_pack->parallel[0].diffuse[1]
                    + light_pack->parallel[0].diffuse[2];

    for (i = 1; i < 3; i++)
    {
        float power = light_pack->parallel[i].diffuse[0]
                      + light_pack->parallel[i].diffuse[1]
                      + light_pack->parallel[i].diffuse[2];

        if (power < weakest_power)
        {
            weakest_power = power;
            weakest = i;
        }
    }

    return weakest;
}

static POINT_WRK* MikuPan_TitleSceneAddPointLight(LIGHT_PACK* light_pack)
{
    int slot = MikuPan_TitleScenePickPointLightSlot(light_pack);

    if (light_pack->point_num < 3)
    {
        light_pack->point_num++;
    }

    return &light_pack->point[slot];
}

static PARARELL_WRK* MikuPan_TitleSceneAddParallelLight(LIGHT_PACK* light_pack)
{
    int slot = MikuPan_TitleScenePickParallelLightSlot(light_pack);

    if (light_pack->parallel_num < 3)
    {
        light_pack->parallel_num++;
    }

    return &light_pack->parallel[slot];
}

static void MikuPan_TitleSceneApplyCandleLight(LIGHT_PACK* light_pack)
{
    POINT_WRK* point = MikuPan_TitleSceneAddPointLight(light_pack);
    float flicker = MikuPan_TitleSceneCandleFlicker();

    point->pos[0] = MIKUPAN_TITLE_SCENE_CANDLE_X;
    point->pos[1] = MIKUPAN_TITLE_SCENE_CANDLE_Y;
    point->pos[2] = MIKUPAN_TITLE_SCENE_CANDLE_Z;
    point->pos[3] = 1.0f;
    point->diffuse[0] = 0.26f * flicker;
    point->diffuse[1] = 0.185f * flicker;
    point->diffuse[2] = 0.085f * flicker;
    point->diffuse[3] = 1.0f;
    point->power = MIKUPAN_TITLE_SCENE_CANDLE_LIGHT_POWER * flicker;

    light_pack->ambient[0] = MikuPan_TitleSceneClampFloat(
        light_pack->ambient[0] + 0.010f * flicker, 0.0f, 1.0f);
    light_pack->ambient[1] = MikuPan_TitleSceneClampFloat(
        light_pack->ambient[1] + 0.005f * flicker, 0.0f, 1.0f);
    light_pack->ambient[2] = MikuPan_TitleSceneClampFloat(
        light_pack->ambient[2] + 0.002f * flicker, 0.0f, 1.0f);
}

static void MikuPan_TitleSceneApplyMoonLight(LIGHT_PACK* light_pack,
                                             const sceVu0FVECTOR forward,
                                             const sceVu0FVECTOR right,
                                             const sceVu0FVECTOR up)
{
    sceVu0FVECTOR moon_pos;
    PARARELL_WRK* parallel;

    moon_pos[0] = g_title_scene_camera.p[0] - forward[0] * 900.0f
                  - right[0] * 700.0f + up[0] * 850.0f;
    moon_pos[1] = g_title_scene_camera.p[1] - forward[1] * 900.0f
                  - right[1] * 700.0f + up[1] * 850.0f;
    moon_pos[2] = g_title_scene_camera.p[2] - forward[2] * 900.0f
                  - right[2] * 700.0f + up[2] * 850.0f;
    moon_pos[3] = 1.0f;

    parallel = MikuPan_TitleSceneAddParallelLight(light_pack);
    parallel->direction[0] = g_title_scene_camera.i[0] - moon_pos[0];
    parallel->direction[1] = g_title_scene_camera.i[1] - moon_pos[1];
    parallel->direction[2] = g_title_scene_camera.i[2] - moon_pos[2];
    parallel->direction[3] = 1.0f;
    MikuPan_TitleSceneNormalize3(parallel->direction);
    parallel->diffuse[0] = 0.16f;
    parallel->diffuse[1] = 0.20f;
    parallel->diffuse[2] = 0.30f;
    parallel->diffuse[3] = 1.0f;

    light_pack->ambient[0] = MikuPan_TitleSceneClampFloat(
        light_pack->ambient[0] + 0.014f, 0.0f, 1.0f);
    light_pack->ambient[1] = MikuPan_TitleSceneClampFloat(
        light_pack->ambient[1] + 0.020f, 0.0f, 1.0f);
    light_pack->ambient[2] = MikuPan_TitleSceneClampFloat(
        light_pack->ambient[2] + 0.034f, 0.0f, 1.0f);
}

int MikuPan_TitleSceneIsTargetRoom(int msn_no, int room_no)
{
    return msn_no == 0 && room_no == R000_GENKAN;
}

void MikuPan_TitleSceneBeginFrame(int enabled, int msn_no, int room_no,
                                  const SgCAMERA* camera)
{
    int active = enabled != 0 && camera != NULL
                 && MikuPan_TitleSceneIsTargetRoom(msn_no, room_no);

    if (!active)
    {
        g_title_scene_active = 0;
        g_title_scene_msn_no = -1;
        g_title_scene_room_no = -1;
        MikuPan_TitleSceneResetDust();
        MikuPan_TitleSceneResetFlame();
        return;
    }

    if (!g_title_scene_active || g_title_scene_msn_no != msn_no
        || g_title_scene_room_no != room_no)
    {
        MikuPan_TitleSceneResetDust();
    }

    g_title_scene_active = 1;
    g_title_scene_msn_no = msn_no;
    g_title_scene_room_no = room_no;
    g_title_scene_frame++;
    g_title_scene_camera = *camera;
    MikuPan_TitleSceneEnsureFlame();
}

void MikuPan_TitleSceneAmendRoomLightPack(LIGHT_PACK* light_pack)
{
    sceVu0FVECTOR forward;
    sceVu0FVECTOR right;
    sceVu0FVECTOR up;

    if (!g_title_scene_active || light_pack == NULL)
    {
        return;
    }

    MikuPan_TitleSceneComputeBasis(&g_title_scene_camera, forward, right, up);
    MikuPan_TitleSceneApplyCandleLight(light_pack);
    MikuPan_TitleSceneApplyMoonLight(light_pack, forward, right, up);
}

void MikuPan_TitleSceneDraw(void)
{
    int i;
    DRAW_ENV draw_env = {
        .tex1 = SCE_GS_SET_TEX1_1(1, 0, SCE_GS_LINEAR,
                                  SCE_GS_LINEAR_MIPMAP_LINEAR, 0, 0, 0),
        .alpha = SCE_GS_SET_ALPHA_1(SCE_GS_ALPHA_CS, SCE_GS_ALPHA_ZERO,
                                    SCE_GS_ALPHA_AS, SCE_GS_ALPHA_CD, 0),
        .zbuf = SCE_GS_SET_ZBUF_1(0x8c, SCE_GS_PSMCT24, 1),
        .test = SCE_GS_SET_TEST_1(1, SCE_GS_ALPHA_GREATER, 0, SCE_GS_AFAIL_KEEP,
                                  0, 0, 1, SCE_GS_DEPTH_GEQUAL),
        .clamp = SCE_GS_SET_CLAMP_1(SCE_GS_REPEAT, SCE_GS_REPEAT, 0, 0, 0, 0),
        .prim =
            SCE_GIF_SET_TAG(4, SCE_GS_TRUE, SCE_GS_TRUE, 84, SCE_GIF_PACKED, 3),
    };

    if (!g_title_scene_active)
    {
        return;
    }

    MikuPan_TitleSceneUpdateDust();

    for (i = 0; i < MIKUPAN_TITLE_SCENE_DUST_MAX; i++)
    {
        if (!g_title_scene_dust[i].active)
        {
            continue;
        }

        MikuPan_TitleSceneRenderDustParticle(&g_title_scene_dust[i], &draw_env);
    }
}
