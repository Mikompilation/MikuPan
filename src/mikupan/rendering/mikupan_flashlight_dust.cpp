#include "mikupan_flashlight_dust.h"

#include "SDL3/SDL_timer.h"
#include "ee/eestruct.h"
#include "enums.h"
#include "main/glob.h"
#include "ingame/map/door_ctl.h"
#include "mikupan/rendering/mikupan_gpu.h"
#include "mikupan/rendering/mikupan_pipeline.h"
#include "mikupan/rendering/mikupan_renderer.h"
#include "mikupan/rendering/mikupan_renderer_internal.h"
#include "sce/libvu0.h"

#include <math.h>
#include <stddef.h>
#include <string.h>

#define MIKUPAN_DUST_PI 3.1415927f
#define MIKUPAN_DUST_PI2 6.2831855f
#define MIKUPAN_DUST_MAX_PARTICLES 1024
#define MIKUPAN_DUST_VERT_STRIDE 12
#define MIKUPAN_DUST_VERTS_PER_PARTICLE 6
#define MIKUPAN_DUST_MAX_VERTICES \
    (MIKUPAN_DUST_MAX_PARTICLES * MIKUPAN_DUST_VERTS_PER_PARTICLE)
#define MIKUPAN_DUST_MAX_DOOR_BURSTS 4

typedef struct MikuPan_DoorDustBurst
{
    int active;
    float position[3];
    float rotation;
    float start_time;
    unsigned int seed;
    int room_no;
} MikuPan_DoorDustBurst;

static const MikuPan_FlashlightDustRoomInfo g_dust_rooms[] = {
    {0, "R000 Genkan", 0.85f},
    {1, "R001 Fusuma", 0.90f},
    {2, "R002 Izanai", 0.80f},
    {3, "R003 Ousetsu", 0.70f},
    {4, "R004 Semai", 0.00f},
    {5, "R005 Men", 0.00f},
    {6, "R006 Hakoniwa", 0.00f},
    {7, "R007 Tomurai", 1.00f},
    {8, "R008 Kairou", 0.95f},
    {9, "R009 Nando", 1.10f},
    {10, "R010 Ningyo", 1.00f},
    {11, "R011 Goimon", 0.95f},
    {12, "R012 Ima", 0.85f},
    {13, "R013 Anutsu", 1.00f},
    {14, "R014 Butsuma", 0.90f},
    {15, "R015 Kouji", 0.85f},
    {16, "R016 Nakaniwa", 0.00f},
    {17, "R017 Kaidan", 0.95f},
    {18, "R018", 0.90f},
    {19, "R019", 0.90f},
    {20, "R020", 0.90f},
    {21, "R021 Uraniwa", 0.00f},
    {22, "R022 Nakasu", 0.00f},
    {23, "R023 Ikesu", 0.00f},
    {24, "R024 Tsukimi", 0.00f},
    {25, "R025 Sando", 0.00f},
    {26, "R026 Oyashiro", 0.00f},
    {27, "R027 Watari", 0.90f},
    {28, "R028 Kegare", 0.90f},
    {29, "R029 Karakuri", 0.90f},
    {30, "R030", 0.90f},
    {31, "R031", 0.95f},
    {32, "R032", 0.95f},
    {33, "R033", 1.00f},
    {34, "R034", 0.95f},
    {35, "R035 Koto", 0.95f},
    {36, "R036 K-Men", 0.95f},
    {37, "R037", 0.95f},
    {38, "R038 Tyoubou", 0.00f},
    {39, "R039", 0.95f},
    {40, "R040", 0.95f},
    {41, "R041 Hikae", 1.00f},
};

static MikuPan_FlashlightDustSettings g_dust_settings = {
    0,
    1,
    1,
    1,
    1,
    1.0f,
    6.0f,
};
static MikuPan_FlashlightDustDebugInfo g_dust_debug = {0};
static MikuPan_DoorDustBurst g_door_bursts[MIKUPAN_DUST_MAX_DOOR_BURSTS] = {0};
static float g_dust_vertices[MIKUPAN_DUST_MAX_VERTICES][MIKUPAN_DUST_VERT_STRIDE];
static unsigned int g_dust_texture_id = 0;
static unsigned int g_next_door_burst_seed = 1;
static unsigned short g_door_track_id[20] = {0};
static unsigned char g_door_track_use[20] = {0};
static unsigned char g_door_track_status[20] = {0};
static unsigned char g_door_track_initialized[20] = {0};
static int g_door_track_room_no = -1;
static int g_last_door_trigger_valid = 0;
static int g_last_door_work_index = -1;
static int g_last_door_id = -1;
static float g_last_door_position[3] = {0.0f, 0.0f, 0.0f};
static float g_last_door_rotation = 0.0f;
static int g_last_door_target_room_no = -1;
static int g_last_door_target_room_allowed = 0;
static int g_last_door_spawned = 0;
static float g_last_door_distance = 0.0f;

static float MikuPan_DustClamp01(float value)
{
    if (value < 0.0f)
    {
        return 0.0f;
    }
    if (value > 1.0f)
    {
        return 1.0f;
    }
    return value;
}

static float MikuPan_DustSmooth01(float value)
{
    value = MikuPan_DustClamp01(value);
    return value * value * (3.0f - 2.0f * value);
}

static void MikuPan_DustResolveColor(float red,
                                     float green,
                                     float blue,
                                     float* output_red,
                                     float* output_green,
                                     float* output_blue)
{
    if (MikuPan_IsBlackWhiteModeActive())
    {
        const float gray = red * 0.299f + green * 0.587f + blue * 0.114f;
        *output_red = gray;
        *output_green = gray;
        *output_blue = gray;
        return;
    }

    *output_red = red;
    *output_green = green;
    *output_blue = blue;
}

static unsigned int MikuPan_DustHash(unsigned int value)
{
    value ^= value >> 16;
    value *= 0x7feb352du;
    value ^= value >> 15;
    value *= 0x846ca68bu;
    value ^= value >> 16;
    return value;
}

static float MikuPan_DustCellRandom01(int cell_x,
                                      int cell_y,
                                      int cell_z,
                                      int salt,
                                      int room_no)
{
    unsigned int value = (unsigned int)cell_x * 0x8da6b343u;
    value ^= (unsigned int)cell_y * 0xd8163841u;
    value ^= (unsigned int)cell_z * 0xcb1ab31fu;
    value ^= (unsigned int)salt * 0x165667b1u;
    value ^= (unsigned int)(room_no + 31) * 0x9e3779b9u;
    return (float)(MikuPan_DustHash(value) & 0x00ffffffu) / 16777215.0f;
}

static float MikuPan_DoorDustRandom01(const MikuPan_DoorDustBurst* burst,
                                      int particle_index,
                                      int salt)
{
    unsigned int value = burst->seed;
    value ^= (unsigned int)particle_index * 0x9e3779b9u;
    value ^= (unsigned int)salt * 0x85ebca6bu;
    value ^= (unsigned int)(burst->room_no + 31) * 0xc2b2ae35u;
    return (float)(MikuPan_DustHash(value) & 0x00ffffffu) / 16777215.0f;
}

static int MikuPan_DustFloorCell(float value, float cell_size)
{
    return (int)floorf(value / cell_size);
}

static float MikuPan_DustNowSeconds(void)
{
    return (float)((double)SDL_GetTicks() / 1000.0);
}

int MikuPan_IsFlashlightDustRoomEnabled(int room_no)
{
    return MikuPan_GetFlashlightDustRoomDensity(room_no) > 0.0f;
}

float MikuPan_GetFlashlightDustRoomDensity(int room_no)
{
    size_t index;

    for (index = 0; index < sizeof(g_dust_rooms) / sizeof(g_dust_rooms[0]); index++)
    {
        if (g_dust_rooms[index].room_no == room_no)
        {
            return g_dust_rooms[index].density;
        }
    }

    return 0.0f;
}

int MikuPan_GetFlashlightDustRoomCount(void)
{
    return (int)(sizeof(g_dust_rooms) / sizeof(g_dust_rooms[0]));
}

const MikuPan_FlashlightDustRoomInfo* MikuPan_GetFlashlightDustRoomInfo(int index)
{
    if (index < 0 || index >= MikuPan_GetFlashlightDustRoomCount())
    {
        return NULL;
    }

    return &g_dust_rooms[index];
}

static int MikuPan_DustGetAmbientParticleBudget(void)
{
    switch (g_dust_settings.level)
    {
    case 0:
        return 52;
    case 2:
        return 184;
    case 1:
    default:
        return 104;
    }
}

static int MikuPan_DustGetDoorParticleBudget(void)
{
    int budget;

    switch (g_dust_settings.level)
    {
    case 0:
        budget = 58;
        break;
    case 2:
        budget = 138;
        break;
    case 1:
    default:
        budget = 96;
        break;
    }

    budget = (int)((float)budget * g_dust_settings.door_disturbance_strength + 0.5f);
    if (budget < 12)
    {
        budget = 12;
    }
    if (budget > 192)
    {
        budget = 192;
    }
    return budget;
}

static float MikuPan_DustGetDoorDuration(void)
{
    if (g_dust_settings.door_disturbance_duration < 1.0f)
    {
        return 1.0f;
    }
    if (g_dust_settings.door_disturbance_duration > 15.0f)
    {
        return 15.0f;
    }
    return g_dust_settings.door_disturbance_duration;
}

static float MikuPan_DustGetLevelAlphaScale(void)
{
    switch (g_dust_settings.level)
    {
    case 0:
        return 1.15f;
    case 2:
        return 2.35f;
    case 1:
    default:
        return 1.70f;
    }
}

static int MikuPan_DustIsGameSceneUsable(void)
{
    if (render_back_msaa.texture.width <= 0 || render_back_msaa.texture.height <= 0)
    {
        return 0;
    }

    if (ingame_wrk.mode == INGAME_MODE_MENU)
    {
        return 0;
    }

    if (camera.fov <= 0.001f || camera.farz <= 0.001f)
    {
        return 0;
    }

    return 1;
}

static void MikuPan_DustBuildFlashlightMatrix(sceVu0FMATRIX output)
{
    float rotation_x = plyr_wrk.spot_rot[0] + plyr_wrk.move_box.rot[0];
    float rotation_y = plyr_wrk.spot_rot[1] + plyr_wrk.move_box.rot[1];

    while (rotation_x < -MIKUPAN_DUST_PI)
    {
        rotation_x += MIKUPAN_DUST_PI2;
    }
    while (MIKUPAN_DUST_PI <= rotation_x)
    {
        rotation_x -= MIKUPAN_DUST_PI2;
    }

    while (rotation_y < -MIKUPAN_DUST_PI)
    {
        rotation_y += MIKUPAN_DUST_PI2;
    }
    while (MIKUPAN_DUST_PI <= rotation_y)
    {
        rotation_y -= MIKUPAN_DUST_PI2;
    }

    sceVu0UnitMatrix(output);
    sceVu0RotMatrixX(output, output, rotation_x);
    sceVu0RotMatrixY(output, output, rotation_y);

    if (fabsf(plyr_wrk.spot_pos[0]) + fabsf(plyr_wrk.spot_pos[1])
            + fabsf(plyr_wrk.spot_pos[2])
        > 0.001f)
    {
        sceVu0TransMatrix(output, output, plyr_wrk.spot_pos);
    }
    else
    {
        sceVu0TransMatrix(output, output, plyr_wrk.move_box.pos);
    }
}

static int MikuPan_DustProjectClip(const sceVu0FVECTOR world,
                                   float output_clip[4])
{
    const float* matrix = MikuPan_GetWorldClipView();
    const float x = world[0];
    const float y = world[1];
    const float z = world[2];

    output_clip[0] = matrix[0] * x + matrix[4] * y + matrix[8] * z + matrix[12];
    output_clip[1] = matrix[1] * x + matrix[5] * y + matrix[9] * z + matrix[13];
    output_clip[2] = matrix[2] * x + matrix[6] * y + matrix[10] * z + matrix[14];
    output_clip[3] = matrix[3] * x + matrix[7] * y + matrix[11] * z + matrix[15];

    return output_clip[3] > 0.001f;
}

static float MikuPan_DustScreenFactor(const float clip[4])
{
    float x;
    float y;
    float edge;

    if (clip[3] <= 0.001f)
    {
        return 0.0f;
    }

    x = fabsf(clip[0] / clip[3]);
    y = fabsf(clip[1] / clip[3]);
    edge = x > y ? x : y;
    if (edge >= 1.12f)
    {
        return 0.0f;
    }

    return 1.0f - MikuPan_DustSmooth01((edge - 0.88f) / 0.24f);
}

static void MikuPan_DustWriteVertex(float* destination,
                                    float u,
                                    float v,
                                    float red,
                                    float green,
                                    float blue,
                                    float alpha,
                                    float x,
                                    float y,
                                    float z,
                                    float w)
{
    destination[0] = u;
    destination[1] = v;
    destination[2] = 0.0f;
    destination[3] = 0.0f;
    destination[4] = red;
    destination[5] = green;
    destination[6] = blue;
    destination[7] = alpha;
    destination[8] = x;
    destination[9] = y;
    destination[10] = z;
    destination[11] = w;
}

static int MikuPan_DustAddQuad(int* vertex_count,
                               const float clip[4],
                               float radius_ndc_x,
                               float radius_ndc_y,
                               float red,
                               float green,
                               float blue,
                               float alpha)
{
    const int base = *vertex_count;
    const float delta_x = radius_ndc_x * clip[3];
    const float delta_y = radius_ndc_y * clip[3];

    if (base + MIKUPAN_DUST_VERTS_PER_PARTICLE > MIKUPAN_DUST_MAX_VERTICES)
    {
        return 0;
    }

    MikuPan_DustWriteVertex(g_dust_vertices[base + 0], 0.0f, 1.0f,
                            red, green, blue, alpha,
                            clip[0] - delta_x, clip[1] - delta_y, clip[2], clip[3]);
    MikuPan_DustWriteVertex(g_dust_vertices[base + 1], 1.0f, 1.0f,
                            red, green, blue, alpha,
                            clip[0] + delta_x, clip[1] - delta_y, clip[2], clip[3]);
    MikuPan_DustWriteVertex(g_dust_vertices[base + 2], 0.0f, 0.0f,
                            red, green, blue, alpha,
                            clip[0] - delta_x, clip[1] + delta_y, clip[2], clip[3]);
    MikuPan_DustWriteVertex(g_dust_vertices[base + 3], 1.0f, 1.0f,
                            red, green, blue, alpha,
                            clip[0] + delta_x, clip[1] - delta_y, clip[2], clip[3]);
    MikuPan_DustWriteVertex(g_dust_vertices[base + 4], 1.0f, 0.0f,
                            red, green, blue, alpha,
                            clip[0] + delta_x, clip[1] + delta_y, clip[2], clip[3]);
    MikuPan_DustWriteVertex(g_dust_vertices[base + 5], 0.0f, 0.0f,
                            red, green, blue, alpha,
                            clip[0] - delta_x, clip[1] + delta_y, clip[2], clip[3]);

    *vertex_count = base + MIKUPAN_DUST_VERTS_PER_PARTICLE;
    return 1;
}

static int MikuPan_DustEnsureTexture(void)
{
    unsigned char pixels[16 * 16 * 4];
    int y;
    int x;

    if (g_dust_texture_id != 0)
    {
        return 1;
    }

    for (y = 0; y < 16; y++)
    {
        for (x = 0; x < 16; x++)
        {
            const float fx = ((float)x + 0.5f) / 16.0f * 2.0f - 1.0f;
            const float fy = ((float)y + 0.5f) / 16.0f * 2.0f - 1.0f;
            const float distance = sqrtf(fx * fx + fy * fy);
            float alpha = 1.0f - MikuPan_DustSmooth01(distance);
            const int index = (y * 16 + x) * 4;

            if (distance >= 1.0f)
            {
                alpha = 0.0f;
            }

            alpha = powf(alpha, 1.35f);
            pixels[index + 0] = 255;
            pixels[index + 1] = 255;
            pixels[index + 2] = 255;
            pixels[index + 3] = (unsigned char)(alpha * 255.0f + 0.5f);
        }
    }

    g_dust_texture_id = MikuPan_GPUCreateTextureRGBA8(16, 16, pixels, 16 * 4, 0, 0);
    return g_dust_texture_id != 0;
}

static float MikuPan_DustConeFactor(sceVu0FVECTOR world,
                                    sceVu0FMATRIX flashlight_inverse_matrix,
                                    float near_z,
                                    float far_z,
                                    float cone_width,
                                    float cone_height,
                                    float* output_depth)
{
    sceVu0FVECTOR local = {0.0f, 0.0f, 0.0f, 1.0f};
    float cone_radius;
    float radius_x;
    float radius_y;
    float radial;
    float radial_fade;
    float depth_fade;

    sceVu0ApplyMatrix(local, flashlight_inverse_matrix, world);
    if (output_depth != NULL)
    {
        *output_depth = local[2];
    }

    if (local[2] <= near_z || local[2] >= far_z)
    {
        return 0.0f;
    }

    cone_radius = local[2] * cone_width;
    if (cone_radius <= 0.001f)
    {
        return 0.0f;
    }

    radius_x = local[0] / cone_radius;
    radius_y = local[1] / (cone_radius * cone_height);
    radial = sqrtf(radius_x * radius_x + radius_y * radius_y);
    if (radial >= 1.0f)
    {
        return 0.0f;
    }

    radial_fade = 1.0f
                  - 0.72f * MikuPan_DustSmooth01((radial - 0.42f) / 0.58f);
    depth_fade = MikuPan_DustSmooth01((local[2] - near_z) / 180.0f)
                 * (1.0f - MikuPan_DustSmooth01((local[2] - far_z * 0.73f)
                                                / (far_z * 0.27f)));
    return radial_fade * depth_fade;
}

static int MikuPan_DustBuildAmbientParticles(int room_no,
                                             float room_density,
                                             float time_seconds,
                                             sceVu0FMATRIX flashlight_inverse_matrix,
                                             int* vertex_count)
{
    const float near_z = 65.0f;
    const float far_z = 1540.0f;
    const float cone_width = 0.78f;
    const float cone_height = 0.62f;
    const float cell_xz = 285.0f;
    const float cell_y = 190.0f;
    const int range_xz = 8;
    const int range_y = 4;
    const int center_x = MikuPan_DustFloorCell(plyr_wrk.move_box.pos[0], cell_xz);
    const int center_y = MikuPan_DustFloorCell(plyr_wrk.move_box.pos[1], cell_y);
    const int center_z = MikuPan_DustFloorCell(plyr_wrk.move_box.pos[2], cell_xz);
    const int budget = MikuPan_DustGetAmbientParticleBudget();
    const float alpha_scale = MikuPan_DustGetLevelAlphaScale() * room_density;
    int particle_count = 0;
    int delta_z;
    int delta_y;
    int delta_x;

    for (delta_z = -range_xz; delta_z <= range_xz; delta_z++)
    {
        for (delta_y = -range_y; delta_y <= range_y; delta_y++)
        {
            for (delta_x = -range_xz; delta_x <= range_xz; delta_x++)
            {
                const int cell_x = center_x + delta_x;
                const int cell_y_index = center_y + delta_y;
                const int cell_z = center_z + delta_z;
                float spawn_chance;
                float flashlight_factor;
                float depth;
                float flicker;
                float alpha;
                float size_pixels;
                float clip[4];
                float ndc_radius_x;
                float ndc_radius_y;
                sceVu0FVECTOR world = {0.0f, 0.0f, 0.0f, 1.0f};

                if (particle_count >= budget)
                {
                    return particle_count;
                }

                spawn_chance = MikuPan_DustCellRandom01(cell_x, cell_y_index,
                                                        cell_z, 0, room_no);
                if (spawn_chance > 0.66f)
                {
                    continue;
                }

                world[0] = ((float)cell_x + MikuPan_DustCellRandom01(
                    cell_x, cell_y_index, cell_z, 1, room_no)) * cell_xz;
                world[1] = ((float)cell_y_index + MikuPan_DustCellRandom01(
                    cell_x, cell_y_index, cell_z, 2, room_no)) * cell_y;
                world[2] = ((float)cell_z + MikuPan_DustCellRandom01(
                    cell_x, cell_y_index, cell_z, 3, room_no)) * cell_xz;

                {
                    const float phase_a = MikuPan_DustCellRandom01(
                        cell_x, cell_y_index, cell_z, 4, room_no)
                        * MIKUPAN_DUST_PI2;
                    const float phase_b = MikuPan_DustCellRandom01(
                        cell_x, cell_y_index, cell_z, 5, room_no)
                        * MIKUPAN_DUST_PI2;
                    const float drift_speed = 0.34f
                        + MikuPan_DustCellRandom01(
                            cell_x, cell_y_index, cell_z, 6, room_no) * 0.34f;
                    const float drift_radius = 48.0f
                        + MikuPan_DustCellRandom01(
                            cell_x, cell_y_index, cell_z, 7, room_no) * 72.0f;
                    const float vertical_radius = 22.0f
                        + MikuPan_DustCellRandom01(
                            cell_x, cell_y_index, cell_z, 8, room_no) * 38.0f;
                    const float turbulence_speed = 1.10f
                        + MikuPan_DustCellRandom01(
                            cell_x, cell_y_index, cell_z, 9, room_no) * 1.35f;
                    const float turbulence_radius = 8.0f
                        + MikuPan_DustCellRandom01(
                            cell_x, cell_y_index, cell_z, 10, room_no) * 20.0f;

                    world[0] += sinf(time_seconds * drift_speed + phase_a)
                                * drift_radius;
                    world[0] += sinf(time_seconds * turbulence_speed + phase_b)
                                * turbulence_radius;
                    world[1] += sinf(time_seconds * drift_speed * 0.61f
                                     + phase_b * 1.37f)
                                * vertical_radius;
                    world[1] += cosf(time_seconds * turbulence_speed * 0.83f
                                     + phase_a)
                                * turbulence_radius * 0.42f;
                    world[2] += cosf(time_seconds * drift_speed * 0.82f
                                     + phase_b)
                                * drift_radius * 0.92f;
                    world[2] += sinf(time_seconds * turbulence_speed * 0.73f
                                     + phase_a * 1.71f)
                                * turbulence_radius;
                }

                flashlight_factor = MikuPan_DustConeFactor(
                    world, flashlight_inverse_matrix, near_z, far_z,
                    cone_width, cone_height, &depth);
                if (flashlight_factor <= 0.0f)
                {
                    continue;
                }

                if (!MikuPan_DustProjectClip(world, clip))
                {
                    continue;
                }

                flicker = 0.86f
                          + 0.14f * sinf(time_seconds * 0.73f
                                         + MikuPan_DustCellRandom01(
                                             cell_x, cell_y_index, cell_z,
                                             11, room_no) * MIKUPAN_DUST_PI2);
                alpha = 0.105f * alpha_scale * flashlight_factor * flicker;
                if (alpha <= 0.006f)
                {
                    continue;
                }

                size_pixels = 0.70f
                              + MikuPan_DustCellRandom01(
                                  cell_x, cell_y_index, cell_z, 12, room_no)
                                * 1.85f;
                size_pixels *= 1.0f
                               + (1.0f - MikuPan_DustClamp01(depth / far_z))
                                 * 0.38f;
                ndc_radius_x = size_pixels * 2.0f
                               / (float)render_back_msaa.texture.width;
                ndc_radius_y = size_pixels * 2.0f
                               / (float)render_back_msaa.texture.height;

                {
                    float red;
                    float green;
                    float blue;

                    MikuPan_DustResolveColor(0.82f, 0.78f, 0.64f,
                                             &red, &green, &blue);
                    if (!MikuPan_DustAddQuad(vertex_count, clip,
                                             ndc_radius_x, ndc_radius_y,
                                             red, green, blue, alpha))
                    {
                        return particle_count;
                    }
                }
                particle_count++;
            }
        }
    }

    return particle_count;
}

static int MikuPan_DustBuildDoorParticles(float time_seconds,
                                          sceVu0FMATRIX flashlight_inverse_matrix,
                                          int* active_bursts,
                                          int* vertex_count)
{
    const float duration = MikuPan_DustGetDoorDuration();
    int particle_count = 0;
    int burst_index;

    *active_bursts = 0;

    for (burst_index = 0; burst_index < MIKUPAN_DUST_MAX_DOOR_BURSTS; burst_index++)
    {
        MikuPan_DoorDustBurst* burst = &g_door_bursts[burst_index];
        const float elapsed = time_seconds - burst->start_time;
        float age;
        float tangent_x;
        float tangent_z;
        float normal_x;
        float normal_z;
        int budget;
        int particle_index;

        if (!burst->active)
        {
            continue;
        }

        if (elapsed < 0.0f || elapsed >= duration)
        {
            burst->active = 0;
            continue;
        }

        (*active_bursts)++;
        age = elapsed / duration;
        tangent_x = cosf(burst->rotation);
        tangent_z = -sinf(burst->rotation);
        normal_x = sinf(burst->rotation);
        normal_z = cosf(burst->rotation);
        budget = MikuPan_DustGetDoorParticleBudget();

        for (particle_index = 0; particle_index < budget; particle_index++)
        {
            const float tangent_offset =
                (MikuPan_DoorDustRandom01(burst, particle_index, 0) - 0.5f) * 560.0f;
            const float vertical_offset =
                18.0f + MikuPan_DoorDustRandom01(burst, particle_index, 1) * 610.0f;
            const float normal_offset =
                (MikuPan_DoorDustRandom01(burst, particle_index, 2) - 0.5f) * 150.0f;
            const float tangent_speed =
                (MikuPan_DoorDustRandom01(burst, particle_index, 3) - 0.5f) * 210.0f;
            const float normal_speed =
                (MikuPan_DoorDustRandom01(burst, particle_index, 4) - 0.5f) * 360.0f;
            const float vertical_speed =
                25.0f + MikuPan_DoorDustRandom01(burst, particle_index, 5) * 145.0f;
            const float phase =
                MikuPan_DoorDustRandom01(burst, particle_index, 6) * MIKUPAN_DUST_PI2;
            const float frequency =
                4.0f + MikuPan_DoorDustRandom01(burst, particle_index, 7) * 8.0f;
            const float erratic_amount =
                18.0f + MikuPan_DoorDustRandom01(burst, particle_index, 8) * 72.0f;
            const float motion_time = elapsed * (1.0f - age * 0.68f);
            const float erratic_normal = sinf(elapsed * frequency + phase) * erratic_amount;
            const float erratic_tangent =
                sinf(elapsed * frequency * 0.71f + phase * 1.73f)
                * erratic_amount * 0.48f;
            const float appear = MikuPan_DustSmooth01(age / 0.045f);
            const float disappear =
                1.0f - MikuPan_DustSmooth01((age - 0.46f) / 0.54f);
            const float random_alpha =
                0.62f + MikuPan_DoorDustRandom01(burst, particle_index, 9) * 0.58f;
            float flashlight_factor;
            float screen_factor;
            float alpha;
            float size_pixels;
            float clip[4];
            float ndc_radius_x;
            float ndc_radius_y;
            sceVu0FVECTOR world = {0.0f, 0.0f, 0.0f, 1.0f};

            world[0] = burst->position[0]
                       + tangent_x * tangent_offset
                       + normal_x * normal_offset
                       + tangent_x * tangent_speed * motion_time
                       + normal_x * normal_speed * motion_time
                       + normal_x * erratic_normal
                       + tangent_x * erratic_tangent;
            world[1] = burst->position[1]
                       + vertical_offset
                       + vertical_speed * motion_time
                       + sinf(elapsed * frequency * 1.31f + phase)
                         * erratic_amount * 0.24f;
            world[2] = burst->position[2]
                       + tangent_z * tangent_offset
                       + normal_z * normal_offset
                       + tangent_z * tangent_speed * motion_time
                       + normal_z * normal_speed * motion_time
                       + normal_z * erratic_normal
                       + tangent_z * erratic_tangent;

            if (!MikuPan_DustProjectClip(world, clip))
            {
                continue;
            }

            screen_factor = MikuPan_DustScreenFactor(clip);
            if (screen_factor <= 0.0f)
            {
                continue;
            }

            flashlight_factor = MikuPan_DustConeFactor(
                world, flashlight_inverse_matrix, 35.0f, 2050.0f,
                1.02f, 0.80f, NULL);
            flashlight_factor = 0.035f
                                + MikuPan_DustSmooth01(flashlight_factor) * 0.965f;

            alpha = 0.38f * g_dust_settings.door_disturbance_strength
                    * random_alpha * appear * disappear
                    * flashlight_factor * screen_factor;
            alpha = MikuPan_DustClamp01(alpha);
            if (alpha <= 0.006f)
            {
                continue;
            }

            size_pixels = 0.85f
                          + MikuPan_DoorDustRandom01(
                              burst, particle_index, 10) * 2.35f;
            ndc_radius_x = size_pixels * 2.0f
                           / (float)render_back_msaa.texture.width;
            ndc_radius_y = size_pixels * 2.0f
                           / (float)render_back_msaa.texture.height;

            {
                float red;
                float green;
                float blue;

                MikuPan_DustResolveColor(0.86f, 0.80f, 0.65f,
                                         &red, &green, &blue);
                if (!MikuPan_DustAddQuad(vertex_count, clip,
                                         ndc_radius_x, ndc_radius_y,
                                         red, green, blue, alpha))
                {
                    return particle_count;
                }
            }
            particle_count++;
        }
    }

    return particle_count;
}

MikuPan_FlashlightDustSettings* MikuPan_GetFlashlightDustSettings(void)
{
    return &g_dust_settings;
}

const MikuPan_FlashlightDustDebugInfo* MikuPan_GetFlashlightDustDebugInfo(void)
{
    return &g_dust_debug;
}

void MikuPan_TriggerDoorDust(float x,
                             float y,
                             float z,
                             float rotation,
                             int room_no)
{
    MikuPan_DoorDustBurst* selected = NULL;
    float oldest_time = 0.0f;
    int index;

    if (!g_dust_settings.enabled || !g_dust_settings.door_disturbance_enabled)
    {
        return;
    }

    for (index = 0; index < MIKUPAN_DUST_MAX_DOOR_BURSTS; index++)
    {
        MikuPan_DoorDustBurst* burst = &g_door_bursts[index];
        if (!burst->active)
        {
            selected = burst;
            break;
        }

        if (selected == NULL || burst->start_time < oldest_time)
        {
            selected = burst;
            oldest_time = burst->start_time;
        }
    }

    if (selected == NULL)
    {
        return;
    }

    selected->active = 1;
    selected->position[0] = x;
    selected->position[1] = y;
    selected->position[2] = z;
    selected->rotation = rotation;
    selected->start_time = MikuPan_DustNowSeconds();
    selected->seed = MikuPan_DustHash(g_next_door_burst_seed++ * 0x9e3779b9u);
    selected->room_no = room_no;
}

static float MikuPan_DustDistanceSquaredXZ(float x0,
                                           float z0,
                                           float x1,
                                           float z1)
{
    const float dx = x0 - x1;
    const float dz = z0 - z1;
    return dx * dx + dz * dz;
}

static void MikuPan_GetDoorDustPositionAtPlayer(float position[3],
                                                 float* rotation)
{
    sceVu0FMATRIX flashlight_matrix;
    sceVu0FVECTOR local = {0.0f, 0.0f, 720.0f, 1.0f};
    sceVu0FVECTOR world = {0.0f, 0.0f, 0.0f, 1.0f};

    MikuPan_DustBuildFlashlightMatrix(flashlight_matrix);
    sceVu0ApplyMatrix(world, flashlight_matrix, local);

    position[0] = world[0];
    position[1] = world[1] - 300.0f;
    position[2] = world[2];
    *rotation = plyr_wrk.move_box.rot[1];
}

static void MikuPan_TriggerDoorDustFromDoor(int door_work_index)
{
    const DOOR_WRK* door = &door_wrk[door_work_index];
    const int current_room_no = plyr_wrk.pr_info.room_no;
    const int target_room_no =
        (int)GetRoomIdBeyondDoor(door->door_id, (u_char)current_room_no);
    float spawn_position[3];
    float spawn_rotation;

    MikuPan_GetDoorDustPositionAtPlayer(spawn_position, &spawn_rotation);

    g_last_door_trigger_valid = 1;
    g_last_door_work_index = door_work_index;
    g_last_door_id = door->door_id;
    g_last_door_position[0] = spawn_position[0];
    g_last_door_position[1] = spawn_position[1];
    g_last_door_position[2] = spawn_position[2];
    g_last_door_rotation = spawn_rotation;
    g_last_door_target_room_no = target_room_no;
    g_last_door_target_room_allowed =
        target_room_no != 0xff
        && MikuPan_IsFlashlightDustRoomEnabled(target_room_no);
    g_last_door_spawned = 0;
    g_last_door_distance = sqrtf(MikuPan_DustDistanceSquaredXZ(
        spawn_position[0], spawn_position[2],
        plyr_wrk.move_box.pos[0], plyr_wrk.move_box.pos[2]));

    if (!g_dust_settings.enabled
        || !g_dust_settings.door_disturbance_enabled
        || !g_last_door_target_room_allowed)
    {
        return;
    }

    MikuPan_TriggerDoorDust(spawn_position[0],
                            spawn_position[1],
                            spawn_position[2],
                            spawn_rotation,
                            target_room_no);
    g_last_door_spawned = 1;
}

static int MikuPan_IsDoorOpeningStatus(int status)
{
    return status == DOOR_STTS_OPENMTN
           || status == DOOR_STTS_OPENING
           || status == DOOR_STTS_OPEN
           || status == DOOR_STTS_OPEN_FORCE
           || status == DOOR_STTS_INERT_OPN
           || status == DOOR_STTS_MANUAL_OPEN;
}

void MikuPan_UpdateDoorDustTriggers(void)
{
    const int room_no = plyr_wrk.pr_info.room_no;
    int index;

    if (g_door_track_room_no != room_no)
    {
        memset(g_door_track_initialized, 0, sizeof(g_door_track_initialized));
        g_door_track_room_no = room_no;
    }

    for (index = 0; index < 20; index++)
    {
        const unsigned short door_id = door_wrk[index].door_id;
        const unsigned char use = door_wrk[index].use;
        const unsigned char status = door_wrk[index].stts;
        const int identity_changed = !g_door_track_initialized[index]
                                     || g_door_track_id[index] != door_id
                                     || g_door_track_use[index] != use;

        if (!identity_changed
            && IsUseDoor(use) != 0
            && door_id != 0xffff
            && MikuPan_IsDoorOpeningStatus(status)
            && !MikuPan_IsDoorOpeningStatus(g_door_track_status[index]))
        {
            MikuPan_TriggerDoorDustFromDoor(index);
        }

        g_door_track_id[index] = door_id;
        g_door_track_use[index] = use;
        g_door_track_status[index] = status;
        g_door_track_initialized[index] = 1;
    }
}

void MikuPan_TriggerDoorDustAtPlayer(void)
{
    float position[3];
    float rotation;

    MikuPan_GetDoorDustPositionAtPlayer(position, &rotation);
    MikuPan_TriggerDoorDust(position[0], position[1], position[2],
                            rotation, plyr_wrk.pr_info.room_no);
}

int MikuPan_TriggerDoorDustAtLastDoor(void)
{
    if (!g_last_door_trigger_valid || !g_last_door_target_room_allowed)
    {
        return 0;
    }

    MikuPan_TriggerDoorDust(g_last_door_position[0],
                            g_last_door_position[1],
                            g_last_door_position[2],
                            g_last_door_rotation,
                            g_last_door_target_room_no);
    return 1;
}

void MikuPan_RenderFlashlightDust(void)
{
    int room_no;
    int room_allowed;
    int vertex_count = 0;
    int ambient_particle_count = 0;
    int door_particle_count = 0;
    int active_door_bursts = 0;
    float room_density;
    float time_seconds;
    sceVu0FMATRIX flashlight_matrix;
    sceVu0FMATRIX flashlight_inverse_matrix;

    memset(&g_dust_debug, 0, sizeof(g_dust_debug));
    g_dust_debug.enabled = g_dust_settings.enabled;
    g_dust_debug.ambient_enabled = g_dust_settings.ambient_enabled;
    g_dust_debug.room_filter_enabled = g_dust_settings.room_filter_enabled;
    g_dust_debug.door_disturbance_enabled =
        g_dust_settings.door_disturbance_enabled;
    g_dust_debug.level = g_dust_settings.level;
    g_dust_debug.ambient_particle_budget = MikuPan_DustGetAmbientParticleBudget();
    g_dust_debug.room_no = plyr_wrk.pr_info.room_no;
    g_dust_debug.room_allowed =
        MikuPan_IsFlashlightDustRoomEnabled(g_dust_debug.room_no);
    g_dust_debug.room_density = g_dust_debug.room_allowed
                                    ? MikuPan_GetFlashlightDustRoomDensity(
                                          g_dust_debug.room_no)
                                    : 0.0f;
    g_dust_debug.last_door_trigger_valid = g_last_door_trigger_valid;
    g_dust_debug.last_door_work_index = g_last_door_work_index;
    g_dust_debug.last_door_id = g_last_door_id;
    g_dust_debug.last_door_position[0] = g_last_door_position[0];
    g_dust_debug.last_door_position[1] = g_last_door_position[1];
    g_dust_debug.last_door_position[2] = g_last_door_position[2];
    g_dust_debug.last_door_distance = g_last_door_distance;
    g_dust_debug.last_door_target_room_no = g_last_door_target_room_no;
    g_dust_debug.last_door_target_room_allowed =
        g_last_door_target_room_allowed;
    g_dust_debug.last_door_spawned = g_last_door_spawned;

    if (!g_dust_settings.enabled)
    {
        return;
    }

    if (!MikuPan_DustIsGameSceneUsable())
    {
        return;
    }

    if (!MikuPan_DustEnsureTexture())
    {
        return;
    }

    room_no = plyr_wrk.pr_info.room_no;
    room_allowed = MikuPan_IsFlashlightDustRoomEnabled(room_no);
    room_density = room_allowed ? MikuPan_GetFlashlightDustRoomDensity(room_no) : 0.78f;
    time_seconds = MikuPan_DustNowSeconds();

    g_dust_debug.room_no = room_no;
    g_dust_debug.room_allowed = room_allowed;
    g_dust_debug.room_density = room_allowed
                                    ? MikuPan_GetFlashlightDustRoomDensity(room_no)
                                    : 0.0f;

    MikuPan_DustBuildFlashlightMatrix(flashlight_matrix);
    sceVu0InversMatrix(flashlight_inverse_matrix, flashlight_matrix);

    if (g_dust_settings.ambient_enabled
        && (!g_dust_settings.room_filter_enabled || room_allowed))
    {
        ambient_particle_count = MikuPan_DustBuildAmbientParticles(
            room_no, room_density, time_seconds,
            flashlight_inverse_matrix, &vertex_count);
    }

    if (g_dust_settings.door_disturbance_enabled)
    {
        door_particle_count = MikuPan_DustBuildDoorParticles(
            time_seconds, flashlight_inverse_matrix,
            &active_door_bursts, &vertex_count);
    }

    g_dust_debug.ambient_particle_count = ambient_particle_count;
    g_dust_debug.door_particle_count = door_particle_count;
    g_dust_debug.active_door_bursts = active_door_bursts;
    g_dust_debug.vertex_count = vertex_count;

    if (vertex_count <= 0)
    {
        return;
    }

    MikuPan_SetViewportCached(0, 0,
                              render_back_msaa.texture.width,
                              render_back_msaa.texture.height);
    MikuPan_RenderTextureIdClipTriangles3DRaw(
        g_dust_texture_id,
        16,
        16,
        &g_dust_vertices[0][0],
        vertex_count,
        MIKUPAN_DEPTH_LEQUAL,
        MIKUPAN_GPU_BLEND_ADDITIVE);
}
