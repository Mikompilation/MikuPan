#include "common.h"
#include "title.h"

#include "btl_mode/btl_menu.h"
#include "data/room_name.h"
#include "ee/kernel.h"
#include "enums.h"
#include "graphics/graph2d/effect_obj.h"
#include "graphics/graph2d/g2d_debug.h"
#include "graphics/graph2d/message.h"
#include "graphics/graph2d/tim2.h"
#include "graphics/graph3d/libsg.h"
#include "graphics/graph3d/gra3d.h"
#include "graphics/graph3d/load3d.h"
#include "graphics/graph3d/sgcam.h"
#include "graphics/graph3d/sgdma.h"
#include "graphics/graph3d/sglib.h"
#include "graphics/motion/accessory.h"
#include "graphics/motion/mdlwork.h"
#include "graphics/motion/mime.h"
#include "graphics/mov/movie.h"
#include "ingame/ig_glob.h"
#include "ingame/map/door_ctl.h"
#include "ingame/map/furn_ctl.h"
#include "ingame/map/furn_spe/furn_spe.h"
#include "ingame/map/map_ctrl.h"
#include "ingame/menu/ig_album.h"
#include "ingame/menu/ig_camra.h"
#include "ingame/menu/ig_glst.h"
#include "ingame/menu/ig_menu.h"
#include "main/gamemain.h"
#include "main/glob.h"
#include "mc/mc_at.h"
#include "mc/mc_main.h"
#include "memory_album.h"
#include "mikupan/gs/mikupan_gs_c.h"
#include "mikupan/mikupan_config.h"
#include "mikupan/mikupan_logging_c.h"
#include "mikupan/mikupan_memory.h"
#include "mikupan/mikupan_rng.h"
#include "mikupan/rendering/mikupan_renderer.h"
#include "os/eeiop/adpcm/ea_cmd.h"
#include "os/eeiop/adpcm/ea_ctrl.h"
#include "os/eeiop/adpcm/ea_dat.h"
#include "os/eeiop/cdvd/eecdvd.h"
#include "os/eeiop/eese.h"
#include "outgame.h"

#define CIMGUI_DEFINE_ENUMS_AND_STRUCTS
#include "cimgui.h"
#include "mikupan/mikupan_utils.h"
#include "mikupan/ui/mikupan_ui.h"
#include "mikupan/ui/mikupan_ui_theme.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int opening_movie_type = 0;
TITLE_WRK title_wrk = {0};
TTL_DSP_WRK ttl_dsp;
OUT_DITHER_STR out_dither = {0};

static int exit_prompt_open = 0;
static int exit_prompt_sel = 1;

#define PI 3.1415927f
#define DEG2RAD(x) ((float)(x)*PI/180.0f)

#define TITLE_BG_MSN_NO 3
#define TITLE_BG_MSN_MAX 4
#define TITLE_BG_ROOM_NO 16
#define TITLE_BG_ROOM_BLOCK 0
#define TITLE_BG_ROOM_MAX 63
#define TITLE_BGM_DEFAULT_FILE_NO 1541
#define TITLE_AUDIO_FILE_MAX RSHADE_SGD

typedef enum {
    TITLE_BG_ROOM_UNLOADED = 0,
    TITLE_BG_ROOM_LOADING_MAP,
    TITLE_BG_ROOM_LOADING,
    TITLE_BG_ROOM_READY,
} TITLE_BG_ROOM_STATE;

typedef struct {
    float camera_p[3];
    float camera_i[3];
    float fov_deg;
} TITLE_BG_CAMERA_POINT;

typedef struct {
    int msn_no;
    int room_no;
    int audio_file_no;
    int cycle_seconds;
    TITLE_BG_CAMERA_POINT camera;
    int lerp_enabled;
    float lerp_seconds;
    float lerp_t;
    TITLE_BG_CAMERA_POINT lerp_a;
    TITLE_BG_CAMERA_POINT lerp_b;
} TITLE_BG_PRESET;

static const TITLE_BG_PRESET title_bg_presets[] = {
    {
        .msn_no = 3,
        .room_no = 16,
        .audio_file_no = 1541,
        .cycle_seconds = 120,
        .camera = {
            .camera_p = {4654.2f, -450.0f, 5244.8f},
            .camera_i = {4931.7f, -350.0f, 4171.2f},
            .fov_deg = 79.4f,
        },
        .lerp_enabled = 1,
        .lerp_seconds = 115.0f,
        .lerp_t = 0.0f,
        .lerp_a = {
            .camera_p = {5153.2f, -450.0f, 5373.8f},
            .camera_i = {5430.7f, -350.0f, 4300.2f},
            .fov_deg = 79.4f,
        },
        .lerp_b = {
            .camera_p = {4088.2f, -450.0f, 5098.5f},
            .camera_i = {4365.7f, -350.0f, 4024.9f},
            .fov_deg = 79.4f,
        },
    },
    {
        .msn_no = 3,
        .room_no = 14,
        .audio_file_no = 1539,
        .cycle_seconds = 120,
        .camera = {
            .camera_p = {2272.5f, -50.0f, -12.4f},
            .camera_i = {2288.7f, -365.0f, 892.0f},
            .fov_deg = 100.0f,
        },
        .lerp_enabled = 1,
        .lerp_seconds = 115.0f,
        .lerp_t = 0.0f,
        .lerp_a = {
            .camera_p = {2272.5f, -50.0f, -12.4f},
            .camera_i = {2288.7f, -365.0f, 892.0f},
            .fov_deg = 100.0f,
        },
        .lerp_b = {
            .camera_p = {2322.6f, -1150.0f, 2787.1f},
            .camera_i = {2338.8f, -1165.0f, 3691.5f},
            .fov_deg = 45.0f,
        },
    },
    {
        .msn_no = 3,
        .room_no = 10,
        .audio_file_no = 1540,
        .cycle_seconds = 120,
        .camera = {
            .camera_p = {1402.7f, -450.0f, 2767.5f},
            .camera_i = {1394.6f, -350.0f, 1233.7f},
            .fov_deg = 43.4f,
        },
        .lerp_enabled = 1,
        .lerp_seconds = 115.0f,
        .lerp_t = 0.0f,
        .lerp_a = {
            .camera_p = {1402.7f, -450.0f, 2767.5f},
            .camera_i = {1394.6f, -350.0f, 1233.7f},
            .fov_deg = 43.4f,
        },
        .lerp_b = {
            .camera_p = {1402.7f, -450.0f, 2767.5f},
            .camera_i = {1394.6f, -350.0f, 1233.7f},
            .fov_deg = 10.0f,
        },
    },
    {
        .msn_no = 3,
        .room_no = 10,
        .audio_file_no = 1541,
        .cycle_seconds = 60,
        .camera = {
            .camera_p = {1509.8f, -595.0f, 1254.6f},
            .camera_i = {481.6f, -480.0f, 1218.4f},
            .fov_deg = 32.1f,
        },
        .lerp_enabled = 1,
        .lerp_seconds = 51.0f,
        .lerp_t = 0.0f,
        .lerp_a = {
            .camera_p = {1509.8f, -595.0f, 1254.6f},
            .camera_i = {481.6f, -480.0f, 1218.4f},
            .fov_deg = 32.1f,
        },
        .lerp_b = {
            .camera_p = {1450.0f, -595.0f, 2953.5f},
            .camera_i = {421.8f, -480.0f, 2917.3f},
            .fov_deg = 32.1f,
        },
    },
    {
        .msn_no = 3,
        .room_no = 6,
        .audio_file_no = 1563,
        .cycle_seconds = 120,
        .camera = {
            .camera_p = {5311.4f, -20.0f, 955.7f},
            .camera_i = {4619.5f, -135.0f, 1534.4f},
            .fov_deg = 64.1f,
        },
        .lerp_enabled = 1,
        .lerp_seconds = 115.0f,
        .lerp_t = 0.0f,
        .lerp_a = {
            .camera_p = {5311.4f, -20.0f, 955.7f},
            .camera_i = {4619.5f, -135.0f, 1534.4f},
            .fov_deg = 64.1f,
        },
        .lerp_b = {
            .camera_p = {5311.4f, -1120.0f, 955.7f},
            .camera_i = {4619.5f, -835.0f, 1534.4f},
            .fov_deg = 64.1f,
        },
    },
};

#define TITLE_BG_PRESET_COUNT ((int)(sizeof(title_bg_presets) / sizeof(title_bg_presets[0])))

static TITLE_BG_ROOM_STATE title_bg_room_state = TITLE_BG_ROOM_UNLOADED;
static SgCAMERA title_bg_camera;
static int title_bg_msn_no = 3;
static int title_bg_room_no = 16;
static float title_bg_camera_p[3] = {4654.2f, -450.0f, 5244.8f};
static float title_bg_camera_i[3] = {4931.7f, -350.0f, 4171.2f};
static float title_bg_camera_fov_deg = 79.4f;
static float title_bg_camera_move_step = 100.0f;
static int title_bg_auto_cycle = 1;
static int title_bg_auto_cycle_seconds = 10;
static int title_bg_auto_cycle_timer = 0;
static int title_bg_preset_index = 0;
static TITLE_BG_CAMERA_POINT title_bg_camera_lerp_a = {
    .camera_p = {5153.2f, -450.0f, 5373.8f},
    .camera_i = {5430.7f, -350.0f, 4300.2f},
    .fov_deg = 79.4f,
};
static TITLE_BG_CAMERA_POINT title_bg_camera_lerp_b = {
    .camera_p = {4088.2f, -450.0f, 5098.5f},
    .camera_i = {4365.7f, -350.0f, 4024.9f},
    .fov_deg = 79.4f,
};
static float title_bg_camera_lerp_t = 0.469f;
static float title_bg_camera_lerp_seconds = 15.9f;
static int title_bg_camera_lerp_enabled = 1;
static int title_bg_camera_lerp_timer = 447;
static int title_debug_window_visible = 1;
static int title_bgm_file_no = TITLE_BGM_DEFAULT_FILE_NO;
static int title_bgm_playing_file_no = -1;
static int title_bg_map_load_id = -1;
static int title_bg_floor = 0;
static int title_bg_map_room_no = 0xff;

int TitleUseRoomBackground(void)
{
    return mikupan_configuration.title_room_background != 0;
}

int TitleDebugWindowVisible(void)
{
    return title_debug_window_visible != 0;
}

void TitleSetDebugWindowVisible(int enabled)
{
    title_debug_window_visible = enabled != 0;
}

#include "data/title_sprt.h" // data 342c90 */ SPRT_DAT title_sprt[11];
#include "data/font_sprt.h" // data 342df0 */ SPRT_DAT font_sprt[20];
#ifdef BUILD_EU_VERSION
#include "data/logotex.h" // static SPRT_DAT logotex[];
#endif

#ifdef BUILD_EU_VERSION
static TITLE_SYS title_sys;
#endif

#ifdef BUILD_EU_VERSION
#define SPRITE_ADDRESS 0xa30000
#define TIM2_ADDRESS 0x1e30000
#define MC_WORK_ADDRESS 0x420000

#define SPRITE_ADDR_1 0x0c80000
#define SPRITE_ADDR_2 0x1d10000
#define SPRITE_ADDR_3 0x1d23680
#define SPRITE_ADDR_4 0x1d54030

#define ALBUM_DESIGN_SIDE_0_ADDRESS 0x1d83000
#define ALBUM_DESIGN_SIDE_1_ADDRESS 0x1dc3470
#else
#define SPRITE_ADDRESS 0xa30000
#define TIM2_ADDRESS 0x1e30000
#define MC_WORK_ADDRESS 0x420000

#define SPRITE_ADDR_1 0x0c80000
#define SPRITE_ADDR_2 0x1d15600
#define SPRITE_ADDR_3 0x1d28c80
#define SPRITE_ADDR_4 0x1d59630

#define ALBUM_DESIGN_SIDE_0_ADDRESS 0x1d88100
#define ALBUM_DESIGN_SIDE_1_ADDRESS 0x1dc8570
#endif

#ifdef BUILD_EU_VERSION

void ChangeTVMode(int mode)
{
    if (sys_wrk.pal_disp_mode == mode)
    {
        return;
    }
    sys_wrk.pal_disp_mode = mode;

    if (mode == 0)
    {
        g_db.disp[1].display.DX = 0x290;
        g_db.disp[1].display.DY = 0x68;
        g_db.disp[0].display.DX = 0x290;
        g_db.disp[0].display.DY = 0x68;

        mc_language = mc_language & 0x7f;
    }
    else
    {
        g_db.disp[1].display.DX = 0x27c;
        g_db.disp[1].display.DY = 0x32;
        g_db.disp[0].display.DX = 0x27c;
        g_db.disp[0].display.DY = 0x32;

        mc_language |= 0x80;
    }

    vfunc();

    clearGsMem(0, 0, 0, 640, 448);
    sceGsSyncPath(0, 0);

    vfunc();

    clearGsMem(0, 0, 0, 640, 448);
    sceGsSyncPath(0, 0);

    vfunc();
    vfunc();

    sceGsResetPath();
    sceDmaReset(1);
    sceGsResetGraph(0, SCE_GS_INTERLACE, mode == 0 ? SCE_GS_PAL: SCE_GS_NTSC, SCE_GS_FRAME);

    vfunc();
    vfunc();

    ttl_dsp.no_disp = 0xf;
}
#endif

static void TitleBgSetVector(sceVu0FVECTOR vector, float x, float y, float z, float w)
{
    vector[0] = x;
    vector[1] = y;
    vector[2] = z;
    vector[3] = w;
}

static int TitleBgClampInt(int value, int min_value, int max_value)
{
    if (value < min_value)
    {
        return min_value;
    }

    return value > max_value ? max_value : value;
}

static float TitleBgClampFloat(float value, float min_value, float max_value)
{
    if (value < min_value)
    {
        return min_value;
    }

    return value > max_value ? max_value : value;
}

static float TitleBgLerpFloat(float a, float b, float t)
{
    return a + (b - a) * t;
}

static const char *TitleBgRoomName(int room_no)
{
    if (room_no >= 0 && room_no < (int)(sizeof(room_name) / sizeof(room_name[0])))
    {
        return room_name[room_no];
    }

    return "(unnamed)";
}

static const char *TitleBgRoomStateName(void)
{
    switch (title_bg_room_state)
    {
    case TITLE_BG_ROOM_UNLOADED:
        return "unloaded";
    case TITLE_BG_ROOM_LOADING_MAP:
        return "loading map";
    case TITLE_BG_ROOM_LOADING:
        return "loading";
    case TITLE_BG_ROOM_READY:
        return "ready";
    }

    return "unknown";
}

static int TitleAudioFileNo(void)
{
    if (title_bgm_file_no < 0)
    {
        title_bgm_file_no = TITLE_BGM_DEFAULT_FILE_NO;
    }
    else if (title_bgm_file_no > TITLE_AUDIO_FILE_MAX)
    {
        title_bgm_file_no = TITLE_AUDIO_FILE_MAX;
    }

    return title_bgm_file_no;
}

static int TitleAudioActiveFileNo(void)
{
    if (TitleUseRoomBackground() == 0)
    {
        return AO000_TITLE_STR;
    }

    return TitleAudioFileNo();
}

static void TitleAudioPlayBgm(void)
{
    int file_no = TitleAudioActiveFileNo();

    EAdpcmCmdPlay(0, 1, file_no, 0, GetAdpcmVol(file_no), 0x280, 0xfff, 0);
    title_bgm_playing_file_no = file_no;
}

static void TitleAudioRestartBgm(void)
{
    EAdpcmCmdStop(0, 0, 0);
    TitleAudioPlayBgm();
}

static void TitleAudioStopBgm(void)
{
    EAdpcmCmdStop(0, 0, 0);
    title_bgm_playing_file_no = -1;
}

void TitleSetUseRoomBackground(int enabled)
{
    int use_room_background = enabled != 0;

    if (mikupan_configuration.title_room_background == use_room_background)
    {
        return;
    }

    mikupan_configuration.title_room_background = use_room_background;

    if (title_bgm_playing_file_no != -1)
    {
        TitleAudioRestartBgm();
    }
}

static void TitleBgRequestRoomReload(void)
{
    title_bg_room_state = TITLE_BG_ROOM_UNLOADED;
    title_bg_map_load_id = -1;
    title_bg_map_room_no = 0xff;
}

static void TitleBgPauseAutoCycle(void)
{
    title_bg_auto_cycle = 0;
    title_bg_auto_cycle_timer = 0;
}

static void TitleBgPauseCameraLerp(void)
{
    title_bg_camera_lerp_enabled = 0;
    title_bg_camera_lerp_timer = 0;
}

static void TitleBgPauseCameraAnimations(void)
{
    TitleBgPauseAutoCycle();
    TitleBgPauseCameraLerp();
}

static void TitleBgCopyCurrentCamera(TITLE_BG_CAMERA_POINT *point)
{
    point->camera_p[0] = title_bg_camera_p[0];
    point->camera_p[1] = title_bg_camera_p[1];
    point->camera_p[2] = title_bg_camera_p[2];
    point->camera_i[0] = title_bg_camera_i[0];
    point->camera_i[1] = title_bg_camera_i[1];
    point->camera_i[2] = title_bg_camera_i[2];
    point->fov_deg = title_bg_camera_fov_deg;
}

static void TitleBgApplyCameraPoint(const TITLE_BG_CAMERA_POINT *point)
{
    title_bg_camera_p[0] = point->camera_p[0];
    title_bg_camera_p[1] = point->camera_p[1];
    title_bg_camera_p[2] = point->camera_p[2];
    title_bg_camera_i[0] = point->camera_i[0];
    title_bg_camera_i[1] = point->camera_i[1];
    title_bg_camera_i[2] = point->camera_i[2];
    title_bg_camera_fov_deg = point->fov_deg;
}

static void TitleBgCopyPresetCamera(const TITLE_BG_PRESET *preset)
{
    TitleBgApplyCameraPoint(&preset->camera);
}

static void TitleBgApplyCameraLerp(void)
{
    float t = TitleBgClampFloat(title_bg_camera_lerp_t, 0.0f, 1.0f);

    title_bg_camera_lerp_t = t;
    title_bg_camera_p[0] = TitleBgLerpFloat(title_bg_camera_lerp_a.camera_p[0], title_bg_camera_lerp_b.camera_p[0], t);
    title_bg_camera_p[1] = TitleBgLerpFloat(title_bg_camera_lerp_a.camera_p[1], title_bg_camera_lerp_b.camera_p[1], t);
    title_bg_camera_p[2] = TitleBgLerpFloat(title_bg_camera_lerp_a.camera_p[2], title_bg_camera_lerp_b.camera_p[2], t);
    title_bg_camera_i[0] = TitleBgLerpFloat(title_bg_camera_lerp_a.camera_i[0], title_bg_camera_lerp_b.camera_i[0], t);
    title_bg_camera_i[1] = TitleBgLerpFloat(title_bg_camera_lerp_a.camera_i[1], title_bg_camera_lerp_b.camera_i[1], t);
    title_bg_camera_i[2] = TitleBgLerpFloat(title_bg_camera_lerp_a.camera_i[2], title_bg_camera_lerp_b.camera_i[2], t);
    title_bg_camera_fov_deg = TitleBgLerpFloat(title_bg_camera_lerp_a.fov_deg, title_bg_camera_lerp_b.fov_deg, t);
}

static int TitleBgCameraLerpFrameCount(void)
{
    int frames;

    title_bg_camera_lerp_seconds = TitleBgClampFloat(title_bg_camera_lerp_seconds, 0.1f, 120.0f);
    frames = (int)(title_bg_camera_lerp_seconds * 60.0f);

    if (frames < 1)
    {
        frames = 1;
    }

    return frames;
}

static void TitleBgSetCameraLerpTimerFromT(void)
{
    int frames = TitleBgCameraLerpFrameCount();

    title_bg_camera_lerp_t = TitleBgClampFloat(title_bg_camera_lerp_t, 0.0f, 1.0f);
    title_bg_camera_lerp_timer = (int)(title_bg_camera_lerp_t * frames);

    if (title_bg_camera_lerp_timer > frames)
    {
        title_bg_camera_lerp_timer = frames;
    }
}

static void TitleBgCameraLerpUpdate(void)
{
    int frames;

    if (title_bg_camera_lerp_enabled == 0)
    {
        return;
    }

    frames = TitleBgCameraLerpFrameCount();
    title_bg_camera_lerp_t = (float)title_bg_camera_lerp_timer / (float)frames;
    TitleBgApplyCameraLerp();

    if (title_bg_camera_lerp_timer >= frames)
    {
        title_bg_camera_lerp_enabled = 0;
        title_bg_camera_lerp_timer = frames;
        title_bg_camera_lerp_t = 1.0f;
        TitleBgApplyCameraLerp();
        return;
    }

    title_bg_camera_lerp_timer++;
}

static void TitleBgApplyPreset(int preset_index)
{
    const TITLE_BG_PRESET *preset;
    int reload_room;

    preset_index = TitleBgClampInt(preset_index, 0, TITLE_BG_PRESET_COUNT - 1);
    preset = &title_bg_presets[preset_index];
    reload_room = title_bg_msn_no != preset->msn_no || title_bg_room_no != preset->room_no;

    title_bg_preset_index = preset_index;
    title_bg_msn_no = preset->msn_no;
    title_bg_room_no = preset->room_no;
    title_bgm_file_no = preset->audio_file_no;
    title_bg_auto_cycle_seconds = preset->cycle_seconds;
    TitleBgCopyPresetCamera(preset);
    title_bg_camera_lerp_a = preset->lerp_a;
    title_bg_camera_lerp_b = preset->lerp_b;
    title_bg_camera_lerp_seconds = preset->lerp_seconds;
    title_bg_camera_lerp_t = preset->lerp_t;
    title_bg_camera_lerp_enabled = preset->lerp_enabled;
    TitleBgSetCameraLerpTimerFromT();
    title_bg_auto_cycle_timer = 0;

    if (title_bg_camera_lerp_enabled != 0)
    {
        TitleBgApplyCameraLerp();
    }

    if (title_bgm_playing_file_no != -1 && title_bgm_playing_file_no != title_bgm_file_no)
    {
        TitleAudioRestartBgm();
    }

    if (reload_room)
    {
        TitleBgRequestRoomReload();
    }
}

static void TitleBgApplyNextPreset(void)
{
    int next_index = title_bg_preset_index + 1;

    if (next_index >= TITLE_BG_PRESET_COUNT)
    {
        next_index = 0;
    }

    TitleBgApplyPreset(next_index);
}

static void TitleBgApplyPrevPreset(void)
{
    int prev_index = title_bg_preset_index - 1;

    if (prev_index < 0)
    {
        prev_index = TITLE_BG_PRESET_COUNT - 1;
    }

    TitleBgApplyPreset(prev_index);
}

static void TitleBgAutoCycleUpdate(void)
{
    int cycle_frames;

    if (title_bg_auto_cycle == 0 || title_bg_room_state != TITLE_BG_ROOM_READY)
    {
        return;
    }

    title_bg_auto_cycle_seconds = TitleBgClampInt(title_bg_auto_cycle_seconds, 1, 120);
    cycle_frames = title_bg_auto_cycle_seconds * 60;

    if (++title_bg_auto_cycle_timer >= cycle_frames)
    {
        TitleBgApplyNextPreset();
    }
}

static void TitleBgSetMissionNo(int msn_no)
{
    msn_no = TitleBgClampInt(msn_no, 0, TITLE_BG_MSN_MAX);

    if (title_bg_msn_no == msn_no)
    {
        return;
    }

    title_bg_msn_no = msn_no;
    TitleBgPauseCameraAnimations();
    TitleBgRequestRoomReload();
}

static void TitleBgSetRoomNo(int room_no)
{
    room_no = TitleBgClampInt(room_no, 0, TITLE_BG_ROOM_MAX);

    if (title_bg_room_no == room_no)
    {
        return;
    }

    title_bg_room_no = room_no;
    TitleBgPauseCameraAnimations();
    TitleBgRequestRoomReload();
}

static void TitleBgResetCamera(void)
{
    const TITLE_BG_PRESET *preset = &title_bg_presets[title_bg_preset_index];

    if (title_bg_msn_no == preset->msn_no && title_bg_room_no == preset->room_no)
    {
        TitleBgCopyPresetCamera(preset);
        return;
    }

    title_bg_camera_p[0] = 0.0f;
    title_bg_camera_p[1] = -450.0f;
    title_bg_camera_p[2] = -700.0f;
    title_bg_camera_i[0] = 0.0f;
    title_bg_camera_i[1] = -350.0f;
    title_bg_camera_i[2] = 0.0f;
    title_bg_camera_fov_deg = 44.0f;
}

static void TitleBgAddVector(float vector[3], float x, float y, float z)
{
    vector[0] += x;
    vector[1] += y;
    vector[2] += z;
}

static float TitleBgNormalize3(float vector[3])
{
    float length = sqrtf(vector[0] * vector[0] + vector[1] * vector[1] + vector[2] * vector[2]);

    if (length < 0.001f)
    {
        vector[0] = 0.0f;
        vector[1] = 0.0f;
        vector[2] = 1.0f;
        return 1.0f;
    }

    vector[0] /= length;
    vector[1] /= length;
    vector[2] /= length;

    return length;
}

static void TitleBgCameraForward(float forward[3])
{
    forward[0] = title_bg_camera_i[0] - title_bg_camera_p[0];
    forward[1] = title_bg_camera_i[1] - title_bg_camera_p[1];
    forward[2] = title_bg_camera_i[2] - title_bg_camera_p[2];
    TitleBgNormalize3(forward);
}

static void TitleBgCameraFlatBasis(float forward[3], float right[3])
{
    forward[0] = title_bg_camera_i[0] - title_bg_camera_p[0];
    forward[1] = 0.0f;
    forward[2] = title_bg_camera_i[2] - title_bg_camera_p[2];
    TitleBgNormalize3(forward);

    right[0] = forward[2];
    right[1] = 0.0f;
    right[2] = -forward[0];
}

static void TitleBgMoveCameraAndTarget(float x, float y, float z)
{
    TitleBgAddVector(title_bg_camera_p, x, y, z);
    TitleBgAddVector(title_bg_camera_i, x, y, z);
}

static void TitleBgPanForward(float amount)
{
    float forward[3];
    float right[3];

    TitleBgCameraFlatBasis(forward, right);
    TitleBgMoveCameraAndTarget(forward[0] * amount, 0.0f, forward[2] * amount);
}

static void TitleBgPanRight(float amount)
{
    float forward[3];
    float right[3];

    TitleBgCameraFlatBasis(forward, right);
    TitleBgMoveCameraAndTarget(right[0] * amount, 0.0f, right[2] * amount);
}

static void TitleBgPanY(float amount)
{
    TitleBgMoveCameraAndTarget(0.0f, amount, 0.0f);
}

static void TitleBgDolly(float amount)
{
    float forward[3];

    TitleBgCameraForward(forward);
    TitleBgAddVector(title_bg_camera_p, forward[0] * amount, forward[1] * amount, forward[2] * amount);
}

static void TitleBgLookRight(float amount)
{
    float forward[3];
    float right[3];

    TitleBgCameraFlatBasis(forward, right);
    TitleBgAddVector(title_bg_camera_i, right[0] * amount, 0.0f, right[2] * amount);
}

static void TitleBgLookY(float amount)
{
    TitleBgAddVector(title_bg_camera_i, 0.0f, amount, 0.0f);
}

static int TitleBgLightCount(SgLIGHT *lights, int max_lights)
{
    int count = lights[0].num;

    if (count < 0)
    {
        return 0;
    }

    return count > max_lights ? max_lights : count;
}

static void TitleBgCopyRoomLights(void)
{
    LIGHT_PACK *light_pack = &room_wrk.mylight[TITLE_BG_ROOM_BLOCK];
    int count;
    int i;

    *light_pack = (LIGHT_PACK){0};
    Vu0CopyVector(light_pack->ambient, room_ambient_light);

    count = TitleBgLightCount(room_pararell_light, 3);
    light_pack->parallel_num = count;

    for (i = 0; i < count; i++)
    {
        Vu0CopyVector(light_pack->parallel[i].direction, room_pararell_light[i].direction);
        Vu0CopyVector(light_pack->parallel[i].diffuse, room_pararell_light[i].diffuse);
    }

    count = TitleBgLightCount(room_point_light, 3);
    light_pack->point_num = count;

    for (i = 0; i < count; i++)
    {
        Vu0CopyVector(light_pack->point[i].pos, room_point_light[i].pos);
        Vu0CopyVector(light_pack->point[i].diffuse, room_point_light[i].diffuse);
        light_pack->point[i].power = room_point_light[i].power;
    }

    count = TitleBgLightCount(room_spot_light, 3);
    light_pack->spot_num = count;

    for (i = 0; i < count; i++)
    {
        Vu0CopyVector(light_pack->spot[i].pos, room_spot_light[i].pos);
        Vu0CopyVector(light_pack->spot[i].direction, room_spot_light[i].direction);
        Vu0CopyVector(light_pack->spot[i].diffuse, room_spot_light[i].diffuse);
        light_pack->spot[i].intens = room_spot_light[i].intens;
        light_pack->spot[i].power = room_spot_light[i].power;
    }
}

static void TitleBgSetupRoomState(void)
{
    room_wrk.room_no = title_bg_map_room_no;
    room_wrk.disp_no[0] = title_bg_room_no;
    room_wrk.disp_no[1] = 0xff;
    TitleBgSetVector(room_wrk.pos[0], 0.0f, 0.0f, 0.0f, 1.0f);
    TitleBgSetVector(room_wrk.pos[1], 0.0f, 0.0f, 0.0f, 1.0f);
}

static int TitleBgResolveMapRoom(void)
{
    int floor;
    int room_no;

    title_bg_floor = 0;
    title_bg_map_room_no = 0xff;

    for (floor = 0; floor < 4; floor++)
    {
        if (floor_exist[title_bg_msn_no][floor] == 0)
        {
            continue;
        }

        title_bg_floor = floor;

        for (room_no = 0; room_no <= TITLE_BG_ROOM_MAX; room_no++)
        {
            if (GetRoomIdFromRoomNoFloor(MAP_ROOM_DAT, room_no, floor) == title_bg_room_no)
            {
                title_bg_map_room_no = room_no;
                return 1;
            }
        }
    }

    return 0;
}

static void TitleBgSetupMapWork(void)
{
    TitleBgResolveMapRoom();

    map_wrk.floor = title_bg_floor;
    map_wrk.dat_adr = GetFloorTopAddr(title_bg_floor);
    map_wrk.now_room = title_bg_room_no;
    map_wrk.next_room = 0xff;
    map_wrk.room_update_flg = 0;
    map_wrk.mirror_flg = 0;

    TitleBgSetupRoomState();
}

static void TitleBgSetupFurnitureWork(void)
{
    int old_msn_no = ingame_wrk.msn_no;
    int i;

    ingame_wrk.msn_no = title_bg_msn_no;
    InitMapStatus(title_bg_msn_no);
    InitAreaReadWrk();
    PreGameInitFActWrk();

    memset(furn_wrk, 0, sizeof(furn_wrk));
    memset(fefct_wrk, 0, sizeof(fefct_wrk));
    room_wrk = (ROOM_WRK){0};

    for (i = 0; i < 60; i++)
    {
        FurnSetWrkNoUse(&furn_wrk[i], i);
    }

    mimChodoInitWork();
    acsInitRopeWork();
    acsInitChodoWork();
    DoorAcInit();
    TitleBgSetupMapWork();
    DoorCtrlInit();

    if (title_bg_map_room_no != 0xff)
    {
        FurnDataInit();
        DoorDataInit();
        FurnSortFurnWrk(0);
    }

    TitleBgSetupRoomState();
    ingame_wrk.msn_no = old_msn_no;
}

static void TitleBgSetupCamera(void)
{
    memset(&title_bg_camera, 0, sizeof(title_bg_camera));

    /* The title room is rendered at origin, so keep this camera in room-local space. */
    TitleBgSetVector(
        title_bg_camera.p,
        title_bg_camera_p[0],
        title_bg_camera_p[1],
        title_bg_camera_p[2],
        1.0f);
    TitleBgSetVector(
        title_bg_camera.i,
        title_bg_camera_i[0],
        title_bg_camera_i[1],
        title_bg_camera_i[2],
        1.0f);

    title_bg_camera.roll = PI;
    title_bg_camera.fov = DEG2RAD(title_bg_camera_fov_deg);
    title_bg_camera.nearz = 0.1f;
    title_bg_camera.farz = 32768.0f;
    title_bg_camera.ax = 1.0f;
    title_bg_camera.ay = 0.40689999f;
    title_bg_camera.cx = 2048.0f;
    title_bg_camera.cy = 2048.0f;
    title_bg_camera.zmin = 0.0f;
    title_bg_camera.zmax = 16777215.0f;
}

static void TitleBgBeginRoomLoad(void)
{
    InitModelLoad();
    title_bg_map_load_id = MissonMapDataLoad(title_bg_msn_no);
    title_bg_room_state = TITLE_BG_ROOM_LOADING_MAP;
}

static void TitleBgUpdateRoomLoad(void)
{
    if (title_bg_room_state == TITLE_BG_ROOM_UNLOADED)
    {
        TitleBgBeginRoomLoad();
    }

    if (title_bg_room_state == TITLE_BG_ROOM_LOADING_MAP)
    {
        if (title_bg_map_load_id >= 0 && IsLoadEnd(title_bg_map_load_id) == 0)
        {
            return;
        }

        TitleBgSetupMapWork();
        RoomMdlLoadReq(NULL, TITLE_BG_ROOM_BLOCK, title_bg_msn_no, title_bg_room_no, 0);
        title_bg_room_state = TITLE_BG_ROOM_LOADING;
    }

    if (title_bg_room_state == TITLE_BG_ROOM_LOADING && RoomMdlLoadWait() != 0)
    {
        InitialDmaBuffer();
        TitleBgSetupFurnitureWork();
        title_bg_room_state = TITLE_BG_ROOM_READY;
    }
}

static int TitleBgRoomReady(void)
{
    TitleBgUpdateRoomLoad();

    return title_bg_room_state == TITLE_BG_ROOM_READY;
}

static void TitleBgDrawRoom(void)
{
    int room_no;

    if (TitleUseRoomBackground() == 0)
    {
        return;
    }

    TitleBgAutoCycleUpdate();
    room_no = title_bg_room_no;

    if (TitleBgRoomReady() == 0)
    {
        return;
    }

    if (room_addr_tbl[room_no].near_sgd == NULL || room_addr_tbl[room_no].lit_data == NULL)
    {
        return;
    }

    TitleBgCameraLerpUpdate();

    TitleBgSetupCamera();
    TitleBgSetupRoomState();

    SgSetRefCamera(&title_bg_camera);
    ClearTextureCache();
    SgTEXTransEnable();
    SetEnvironment();
    SgSetFog(
        fog_param[room_no][0], fog_param[room_no][1],
        fog_param[room_no][2], fog_param[room_no][3],
        fog_rgb[room_no][0], fog_rgb[room_no][1], fog_rgb[room_no][2]);

    SgReadLights(
        room_addr_tbl[room_no].near_sgd,
        room_addr_tbl[room_no].lit_data,
        room_ambient_light,
        room_pararell_light,
        3,
        room_point_light,
        16,
        room_spot_light,
        16);

    TitleBgCopyRoomLights();
    FurnAplyAmbient();

    CalcRoomCoord(room_addr_tbl[room_no].near_sgd, room_wrk.pos[0]);

    if (room_addr_tbl[room_no].ss_sgd != NULL)
    {
        CalcRoomCoord(room_addr_tbl[room_no].ss_sgd, room_wrk.pos[0]);
    }

    search_num = 0;
    search_num2 = 0;
    DrawOneRoom(TITLE_BG_ROOM_BLOCK);
    DrawFurnitureForced(room_no);
}

static void TitleBgDebugUi(void)
{
    if (TitleDebugWindowVisible() == 0)
    {
        return;
    }

    int msn_no = title_bg_msn_no;
    int room_no = title_bg_room_no;
    int auto_cycle = title_bg_auto_cycle;
    int cycle_seconds = title_bg_auto_cycle_seconds;
    int preset_index = title_bg_preset_index + 1;
    int camera_lerp_enabled = title_bg_camera_lerp_enabled;
    int audio_file_no = title_bgm_file_no;
    int use_room_background = TitleUseRoomBackground();
    char constants[512];
    char full_values[2048];

    igSetNextWindowPos(
        (ImVec2){12.0f, 36.0f},
        ImGuiCond_FirstUseEver,
        (ImVec2){0.0f, 0.0f});

    igBegin("Title Background Room", NULL, ImGuiWindowFlags_AlwaysAutoResize);

    igText("State: %s", TitleBgRoomStateName());
    igText("Room asset: %s", TitleBgRoomName(title_bg_room_no));
    igText("Map floor: %d local room: %d", title_bg_floor, title_bg_map_room_no);

    igSeparator();
    igText("Title style");

    if (igCheckbox("Use room background", (bool *)&use_room_background))
    {
        TitleSetUseRoomBackground(use_room_background);
    }

    igSeparator();
    igText("Title audio");

    if (igInputInt("Room audio file ID", &audio_file_no, 1, 10, 0))
    {
        if (audio_file_no < 0)
        {
            audio_file_no = 0;
        }
        else if (audio_file_no > TITLE_AUDIO_FILE_MAX)
        {
            audio_file_no = TITLE_AUDIO_FILE_MAX;
        }

        title_bgm_file_no = audio_file_no;
    }

    igText("Last started file ID: %d", title_bgm_playing_file_no);
    igText("Effective title BGM: %d", TitleAudioActiveFileNo());
    igText("Volume table value: %u", GetAdpcmVol(TitleAudioActiveFileNo()));

    if (igButton("Play / restart audio", (ImVec2){0.0f, 0.0f}))
    {
        TitleAudioRestartBgm();
    }

    igSameLine(0.0f, -1.0f);

    if (igButton("Stop audio", (ImVec2){0.0f, 0.0f}))
    {
        TitleAudioStopBgm();
    }

    if (igButton("Use 1541##title_audio", (ImVec2){0.0f, 0.0f}))
    {
        title_bgm_file_no = TITLE_BGM_DEFAULT_FILE_NO;
        TitleAudioRestartBgm();
    }

    igSameLine(0.0f, -1.0f);

    if (igButton("Use original title BGM##title_audio", (ImVec2){0.0f, 0.0f}))
    {
        title_bgm_file_no = AO000_TITLE_STR;
        TitleAudioRestartBgm();
    }

    igSeparator();

    if (igCheckbox("Auto cycle presets", (bool *)&auto_cycle))
    {
        title_bg_auto_cycle = auto_cycle != 0;
        title_bg_auto_cycle_timer = 0;

        if (title_bg_auto_cycle != 0)
        {
            TitleBgPauseCameraLerp();
        }
    }

    if (igInputInt("Cycle seconds", &cycle_seconds, 1, 5, 0))
    {
        title_bg_auto_cycle_seconds = TitleBgClampInt(cycle_seconds, 1, 120);
        title_bg_auto_cycle_timer = 0;
    }

    if (igInputInt("Preset", &preset_index, 1, 1, 0))
    {
        TitleBgPauseCameraAnimations();
        TitleBgApplyPreset(preset_index - 1);
    }

    igSameLine(0.0f, -1.0f);

    if (igButton("Prev##title_bg_preset", (ImVec2){0.0f, 0.0f}))
    {
        TitleBgPauseCameraAnimations();
        TitleBgApplyPrevPreset();
    }

    igSameLine(0.0f, -1.0f);

    if (igButton("Next##title_bg_preset", (ImVec2){0.0f, 0.0f}))
    {
        TitleBgPauseCameraAnimations();
        TitleBgApplyNextPreset();
    }

    igText("Preset count: %d", TITLE_BG_PRESET_COUNT);

    if (igInputInt("Mission", &msn_no, 1, 10, 0))
    {
        TitleBgSetMissionNo(msn_no);
    }

    if (igInputInt("Room", &room_no, 1, 5, 0))
    {
        TitleBgSetRoomNo(room_no);
    }

    if (igButton("Reload room", (ImVec2){0.0f, 0.0f}))
    {
        TitleBgPauseCameraAnimations();
        TitleBgRequestRoomReload();
    }

    igSameLine(0.0f, -1.0f);

    if (igButton("Reset camera", (ImVec2){0.0f, 0.0f}))
    {
        TitleBgPauseCameraAnimations();
        TitleBgResetCamera();
    }

    igSeparator();
    igDragFloat("Move step", &title_bg_camera_move_step, 5.0f, 1.0f, 2500.0f, "%.1f", 0);

    igText("Pan camera + target");

    if (igButton("Forward##title_bg_pan", (ImVec2){92.0f, 0.0f}))
    {
        TitleBgPauseCameraAnimations();
        TitleBgPanForward(title_bg_camera_move_step);
    }

    igSameLine(0.0f, -1.0f);

    if (igButton("Back##title_bg_pan", (ImVec2){92.0f, 0.0f}))
    {
        TitleBgPauseCameraAnimations();
        TitleBgPanForward(-title_bg_camera_move_step);
    }

    if (igButton("Left##title_bg_pan", (ImVec2){92.0f, 0.0f}))
    {
        TitleBgPauseCameraAnimations();
        TitleBgPanRight(-title_bg_camera_move_step);
    }

    igSameLine(0.0f, -1.0f);

    if (igButton("Right##title_bg_pan", (ImVec2){92.0f, 0.0f}))
    {
        TitleBgPauseCameraAnimations();
        TitleBgPanRight(title_bg_camera_move_step);
    }

    if (igButton("Up##title_bg_pan", (ImVec2){92.0f, 0.0f}))
    {
        TitleBgPauseCameraAnimations();
        TitleBgPanY(-title_bg_camera_move_step);
    }

    igSameLine(0.0f, -1.0f);

    if (igButton("Down##title_bg_pan", (ImVec2){92.0f, 0.0f}))
    {
        TitleBgPauseCameraAnimations();
        TitleBgPanY(title_bg_camera_move_step);
    }

    igText("Dolly camera");

    if (igButton("In##title_bg_dolly", (ImVec2){92.0f, 0.0f}))
    {
        TitleBgPauseCameraAnimations();
        TitleBgDolly(title_bg_camera_move_step);
    }

    igSameLine(0.0f, -1.0f);

    if (igButton("Out##title_bg_dolly", (ImVec2){92.0f, 0.0f}))
    {
        TitleBgPauseCameraAnimations();
        TitleBgDolly(-title_bg_camera_move_step);
    }

    igText("Look target");

    if (igButton("Left##title_bg_look", (ImVec2){92.0f, 0.0f}))
    {
        TitleBgPauseCameraAnimations();
        TitleBgLookRight(-title_bg_camera_move_step);
    }

    igSameLine(0.0f, -1.0f);

    if (igButton("Right##title_bg_look", (ImVec2){92.0f, 0.0f}))
    {
        TitleBgPauseCameraAnimations();
        TitleBgLookRight(title_bg_camera_move_step);
    }

    if (igButton("Up##title_bg_look", (ImVec2){92.0f, 0.0f}))
    {
        TitleBgPauseCameraAnimations();
        TitleBgLookY(-title_bg_camera_move_step);
    }

    igSameLine(0.0f, -1.0f);

    if (igButton("Down##title_bg_look", (ImVec2){92.0f, 0.0f}))
    {
        TitleBgPauseCameraAnimations();
        TitleBgLookY(title_bg_camera_move_step);
    }

    igSeparator();

    if (igDragFloat3("Camera position", title_bg_camera_p, 10.0f, -50000.0f, 50000.0f, "%.1f", 0))
    {
        TitleBgPauseCameraAnimations();
    }

    if (igDragFloat3("Camera target", title_bg_camera_i, 10.0f, -50000.0f, 50000.0f, "%.1f", 0))
    {
        TitleBgPauseCameraAnimations();
    }

    if (igSliderFloat("FOV", &title_bg_camera_fov_deg, 10.0f, 100.0f, "%.1f", 0))
    {
        TitleBgPauseCameraAnimations();
    }

    igSeparator();
    igText("Camera lerp");

    if (igButton("Set A from camera##title_bg_lerp", (ImVec2){0.0f, 0.0f}))
    {
        TitleBgPauseCameraAnimations();
        TitleBgCopyCurrentCamera(&title_bg_camera_lerp_a);
    }

    igSameLine(0.0f, -1.0f);

    if (igButton("Apply A##title_bg_lerp", (ImVec2){0.0f, 0.0f}))
    {
        TitleBgPauseCameraAnimations();
        TitleBgApplyCameraPoint(&title_bg_camera_lerp_a);
        title_bg_camera_lerp_t = 0.0f;
    }

    if (igButton("Set B from camera##title_bg_lerp", (ImVec2){0.0f, 0.0f}))
    {
        TitleBgPauseCameraAnimations();
        TitleBgCopyCurrentCamera(&title_bg_camera_lerp_b);
    }

    igSameLine(0.0f, -1.0f);

    if (igButton("Apply B##title_bg_lerp", (ImVec2){0.0f, 0.0f}))
    {
        TitleBgPauseCameraAnimations();
        TitleBgApplyCameraPoint(&title_bg_camera_lerp_b);
        title_bg_camera_lerp_t = 1.0f;
    }

    if (igSliderFloat("Lerp A to B", &title_bg_camera_lerp_t, 0.0f, 1.0f, "%.3f", 0))
    {
        TitleBgPauseCameraAnimations();
        TitleBgApplyCameraLerp();
    }

    igDragFloat("Lerp seconds", &title_bg_camera_lerp_seconds, 0.1f, 0.1f, 120.0f, "%.1f", 0);

    if (igCheckbox("Play camera lerp", (bool *)&camera_lerp_enabled))
    {
        title_bg_camera_lerp_enabled = camera_lerp_enabled != 0;
        title_bg_camera_lerp_timer = 0;

        if (title_bg_camera_lerp_enabled != 0)
        {
            title_bg_auto_cycle = 0;
            title_bg_auto_cycle_timer = 0;
            title_bg_camera_lerp_t = 0.0f;
            TitleBgApplyCameraLerp();
        }
    }

    snprintf(
        constants,
        sizeof(constants),
        "msn=%d room=%d p={%.1ff, %.1ff, %.1ff} i={%.1ff, %.1ff, %.1ff} fov=%.1ff",
        title_bg_msn_no,
        title_bg_room_no,
        title_bg_camera_p[0],
        title_bg_camera_p[1],
        title_bg_camera_p[2],
        title_bg_camera_i[0],
        title_bg_camera_i[1],
        title_bg_camera_i[2],
        title_bg_camera_fov_deg);

    snprintf(
        full_values,
        sizeof(full_values),
        "title_bg_setup\n"
        "msn=%d room=%d room_background=%d audio=%d auto_cycle=%d cycle_seconds=%d preset=%d\n"
        "camera p={%.1ff, %.1ff, %.1ff} i={%.1ff, %.1ff, %.1ff} fov=%.1ff\n"
        "lerp enabled=%d seconds=%.1ff t=%.3ff\n"
        "lerp_a p={%.1ff, %.1ff, %.1ff} i={%.1ff, %.1ff, %.1ff} fov=%.1ff\n"
        "lerp_b p={%.1ff, %.1ff, %.1ff} i={%.1ff, %.1ff, %.1ff} fov=%.1ff",
        title_bg_msn_no,
        title_bg_room_no,
        TitleUseRoomBackground(),
        TitleAudioFileNo(),
        title_bg_auto_cycle,
        title_bg_auto_cycle_seconds,
        title_bg_preset_index + 1,
        title_bg_camera_p[0],
        title_bg_camera_p[1],
        title_bg_camera_p[2],
        title_bg_camera_i[0],
        title_bg_camera_i[1],
        title_bg_camera_i[2],
        title_bg_camera_fov_deg,
        title_bg_camera_lerp_enabled,
        title_bg_camera_lerp_seconds,
        title_bg_camera_lerp_t,
        title_bg_camera_lerp_a.camera_p[0],
        title_bg_camera_lerp_a.camera_p[1],
        title_bg_camera_lerp_a.camera_p[2],
        title_bg_camera_lerp_a.camera_i[0],
        title_bg_camera_lerp_a.camera_i[1],
        title_bg_camera_lerp_a.camera_i[2],
        title_bg_camera_lerp_a.fov_deg,
        title_bg_camera_lerp_b.camera_p[0],
        title_bg_camera_lerp_b.camera_p[1],
        title_bg_camera_lerp_b.camera_p[2],
        title_bg_camera_lerp_b.camera_i[0],
        title_bg_camera_lerp_b.camera_i[1],
        title_bg_camera_lerp_b.camera_i[2],
        title_bg_camera_lerp_b.fov_deg);

    igSeparator();
    igTextWrapped("%s", constants);

    if (igButton("Copy current camera", (ImVec2){0.0f, 0.0f}))
    {
        igSetClipboardText(constants);
    }

    igSeparator();
    igTextWrapped("%s", full_values);

    if (igButton("Copy full setup", (ImVec2){0.0f, 0.0f}))
    {
        igSetClipboardText(full_values);
    }

    igEnd();
}

static void TitleDrawClassicBackground(int use_alpha, u_char alpha)
{
    int i;
    DISP_SPRT ds;

    if (TitleUseRoomBackground() != 0)
    {
        return;
    }

    for (i = 0; i < 11; i++)
    {
        CopySprDToSpr(&ds, &title_sprt[i]);

        if (use_alpha != 0)
        {
            ds.alpha = alpha;
        }

        DispSprD(&ds);
    }
}

static ImU32 TitleImGuiColor(u_char r, u_char g, u_char b, u_char a)
{
    return ((ImU32)a << 24) | ((ImU32)b << 16) | ((ImU32)g << 8) | r;
}

static float TitleTextWidth(ImFont *font, float font_size, const char *text)
{
    ImVec2 size = ImFont_CalcTextSizeA(
        font, font_size, 100000.0f, 0.0f, text, NULL, NULL);

    return size.x;
}

static float TitleFontSizeForWidth(ImFont *font, const char *text,
                                   float target_width)
{
    float base_size = 100.0f;
    float base_width = TitleTextWidth(font, base_size, text);

    if (base_width <= 0.0f)
    {
        return base_size;
    }

    return base_size * (target_width / base_width);
}

static ImVec2 TitleCenteredTextPos(ImGuiIO *io, ImFont *font, float font_size,
                                   const char *text, float center_y)
{
    ImVec2 size = ImFont_CalcTextSizeA(
        font, font_size, 100000.0f, 0.0f, text, NULL, NULL);

    return (ImVec2){
        io->DisplaySize.x * 0.5f - size.x * 0.5f,
        center_y - size.y * 0.5f,
    };
}

static void TitleDrawText(ImDrawList *draw_list, ImFont *font, float font_size,
                          ImVec2 pos, ImU32 color, const char *text,
                          const ImVec4 *clip_rect)
{
    ImDrawList_AddText_FontPtr(
        draw_list, font, font_size, pos, color, text, NULL, 0.0f, clip_rect);
}

static void TitleDrawTextOutline(ImDrawList *draw_list, ImFont *font,
                                 float font_size, ImVec2 pos,
                                 const char *text, float radius,
                                 ImU32 shadow, ImU32 outline)
{
    TitleDrawText(
        draw_list, font, font_size,
        (ImVec2){pos.x + radius * 1.4f, pos.y + radius * 1.8f},
        shadow, text, NULL);

    TitleDrawText(draw_list, font, font_size,
                  (ImVec2){pos.x - radius, pos.y}, outline, text, NULL);
    TitleDrawText(draw_list, font, font_size,
                  (ImVec2){pos.x + radius, pos.y}, outline, text, NULL);
    TitleDrawText(draw_list, font, font_size,
                  (ImVec2){pos.x, pos.y - radius}, outline, text, NULL);
    TitleDrawText(draw_list, font, font_size,
                  (ImVec2){pos.x, pos.y + radius}, outline, text, NULL);
    TitleDrawText(draw_list, font, font_size,
                  (ImVec2){pos.x - radius, pos.y - radius}, outline, text,
                  NULL);
    TitleDrawText(draw_list, font, font_size,
                  (ImVec2){pos.x + radius, pos.y - radius}, outline, text,
                  NULL);
    TitleDrawText(draw_list, font, font_size,
                  (ImVec2){pos.x - radius, pos.y + radius}, outline, text,
                  NULL);
    TitleDrawText(draw_list, font, font_size,
                  (ImVec2){pos.x + radius, pos.y + radius}, outline, text,
                  NULL);
}

static void TitleDrawBlockLogoLine(ImDrawList *draw_list, ImFont *font,
                                   float font_size, ImVec2 pos,
                                   const char *text, float display_scale)
{
    ImVec2 text_size = ImFont_CalcTextSizeA(
        font, font_size, 100000.0f, 0.0f, text, NULL, NULL);
    float outline_radius = 4.0f * display_scale;
    ImVec4 upper_clip = {
        pos.x - 24.0f * display_scale,
        pos.y - 8.0f * display_scale,
        pos.x + text_size.x + 24.0f * display_scale,
        pos.y + text_size.y * 0.48f,
    };
    ImVec4 middle_clip = {
        pos.x - 24.0f * display_scale,
        pos.y + text_size.y * 0.42f,
        pos.x + text_size.x + 24.0f * display_scale,
        pos.y + text_size.y * 0.78f,
    };
    ImVec4 lower_clip = {
        pos.x - 24.0f * display_scale,
        pos.y + text_size.y * 0.72f,
        pos.x + text_size.x + 24.0f * display_scale,
        pos.y + text_size.y + 8.0f * display_scale,
    };

    TitleDrawTextOutline(
        draw_list, font, font_size, pos, text, outline_radius,
        TitleImGuiColor(4, 0, 8, 210),
        TitleImGuiColor(84, 16, 14, 235));
    TitleDrawText(draw_list, font, font_size,
                  (ImVec2){pos.x + 2.0f * display_scale, pos.y},
                  TitleImGuiColor(65, 13, 22, 245), text, NULL);
    TitleDrawText(draw_list, font, font_size, pos,
                  TitleImGuiColor(155, 42, 34, 248), text, NULL);
    TitleDrawText(draw_list, font, font_size, pos,
                  TitleImGuiColor(218, 105, 70, 230), text, &upper_clip);
    TitleDrawText(draw_list, font, font_size,
                  (ImVec2){pos.x, pos.y - 1.0f * display_scale},
                  TitleImGuiColor(245, 232, 209, 245), text, &middle_clip);
    TitleDrawText(draw_list, font, font_size, pos,
                  TitleImGuiColor(116, 24, 29, 245), text, &lower_clip);
}

static void TitleDrawSerifSubtitle(ImDrawList *draw_list, ImFont *font,
                                   const char *text, float target_width,
                                   float center_y, float display_scale)
{
    float font_size = TitleFontSizeForWidth(font, text, target_width);
    ImGuiIO *io = igGetIO_Nil();
    ImVec2 pos = TitleCenteredTextPos(io, font, font_size, text, center_y);
    float radius = 2.0f * display_scale;

    TitleDrawText(draw_list, font, font_size,
                  (ImVec2){pos.x + radius, pos.y + radius},
                  TitleImGuiColor(0, 0, 0, 190), text, NULL);
    TitleDrawText(draw_list, font, font_size,
                  (ImVec2){pos.x - radius, pos.y},
                  TitleImGuiColor(75, 19, 26, 220), text, NULL);
    TitleDrawText(draw_list, font, font_size,
                  (ImVec2){pos.x + radius, pos.y},
                  TitleImGuiColor(75, 19, 26, 220), text, NULL);
    TitleDrawText(draw_list, font, font_size, pos,
                  TitleImGuiColor(222, 198, 184, 245), text, NULL);
}

static void TitleDrawGameTitle(void)
{
    const char *top_text = "FATAL";
    const char *bottom_text = "FRAME";
    const char *story_text = "BASED ON A TRUE STORY.";
    ImGuiIO *io = igGetIO_Nil();
    ImDrawList *draw_list = igGetBackgroundDrawList_Nil();
    ImFont *block_font = MikuPan_GetTitleBlockFont();
    float display_scale = MikuPan_UiThemeGetDisplayScale();
    float target_width;
    float block_size;
    ImVec2 top_pos;
    ImVec2 bottom_pos;

    if (TitleUseRoomBackground() == 0)
    {
        return;
    }

    if (block_font == NULL)
    {
        block_font = igGetFont();
    }

    if (block_font == NULL || draw_list == NULL)
    {
        return;
    }

    if (display_scale <= 0.0f)
    {
        display_scale = 1.0f;
    }

    target_width = io->DisplaySize.x * 0.62f;
    block_size = TitleFontSizeForWidth(block_font, top_text, target_width);
    top_pos = TitleCenteredTextPos(
        io, block_font, block_size, top_text, io->DisplaySize.y * 0.26f);
    bottom_pos = TitleCenteredTextPos(
        io, block_font, block_size, bottom_text, io->DisplaySize.y * 0.43f);

    TitleDrawBlockLogoLine(
        draw_list, block_font, block_size, top_pos, top_text, display_scale);
    TitleDrawBlockLogoLine(
        draw_list, block_font, block_size, bottom_pos, bottom_text,
        display_scale);
    TitleDrawSerifSubtitle(
        draw_list, block_font, story_text, io->DisplaySize.x * 0.44f,
        io->DisplaySize.y * 0.59f, display_scale);
}

void TitleCtrl()
{
	static u_int mc_pnum1;
	static u_int mc_pnum2;
	static u_int mc_atyp1;
	static u_int mc_atyp2;
	static u_int mc_slot1;
	static u_int mc_slot2;
	static u_int mc_file1;
	static u_int mc_file2;
	static int title_cnt;

    switch(title_wrk.mode)
    {
    case TITLE_INIT:
        ttl_dsp = (TTL_DSP_WRK){0};
        exit_prompt_open = 0;

#ifdef BUILD_EU_VERSION
        title_wrk.load_id = LoadReqLanguage(TITLE_E_PK2, SPRITE_ADDRESS);
#else
        title_wrk.load_id = LoadReq(TITLE_PK2, SPRITE_ADDRESS);
#endif
        if (TitleUseRoomBackground() != 0)
        {
            TitleBgBeginRoomLoad();
        }

        InitOutDither();
        MakeOutDither();

        title_wrk.mode = TITLE_WAIT;
    break;

    case TITLE_WAIT:
        if (IsLoadEnd(title_wrk.load_id) == 0)
        {
            break;
        }

        if (TitleUseRoomBackground() != 0 && TitleBgRoomReady() == 0)
        {
            break;
        }

        SeStopAll();

        title_wrk.csr = 0;
        ttl_dsp.timer = 0;
        title_wrk.mode = TITLE_TITLE_WAIT;
        title_cnt = 0;
    case TITLE_TITLE_WAIT:
        title_wrk.mode = TITLE_TITLE_WAIT2;

        EAdpcmFadeOut(0x3c);
    case TITLE_TITLE_WAIT2:
        TitleBgDrawRoom();
        SetSprFile(MikuPan_GetHostAddress(SPRITE_ADDRESS));

        TitleWaitMode();
        DispOutDither();
        TitleBgDebugUi();

        if (IsEndAdpcmFadeOut() != 0)
        {
            TitleAudioPlayBgm();

            title_wrk.mode = TITLE_TITLE;
            ttl_dsp.timer = 0;
        }
    break;
    case TITLE_TITLE:
        /* Freeze the attract-mode timeout while the exit prompt is up. */
        if (!exit_prompt_open)
        {
            title_cnt++;
        }

        TitleBgDrawRoom();
        SetSprFile(MikuPan_GetHostAddress(SPRITE_ADDRESS));
        TitleWaitMode();
        DispOutDither();
        TitleBgDebugUi();
        if (title_cnt >= 60*46 && title_wrk.mode != TITLE_TITLE_SEL_INIT)
        {
            title_wrk.mode = TITLE_MOVIE_STEP1;
            EAdpcmFadeOut(0x3c);
        }
    break;
    case TITLE_MOVIE_STEP1:
        if (IsEndAdpcmFadeOut() != 0)
        {
            title_cnt = 60*3;
            title_wrk.mode = TITLE_MOVIE_STEP2;
        }
    break;
    case TITLE_MOVIE_STEP2:
        if (title_cnt != 0)
        {
            if (--title_cnt != 0)
            {
                break;
            }
        }

        if (opening_movie_type != 0)
        {
            if (MoviePlay(SCENE_NO_9_20_0))
            {
                break;
            }
        }
        else
        {
            if (MoviePlay(SCENE_NO_9_10_0))
            {
                break;
            }
        }

        opening_movie_type = opening_movie_type == 0;
        title_wrk.mode = TITLE_INIT;
    break;
    case TITLE_INIT_FROM_IN_BGMREQ:
        if (IsEndAdpcmFadeOut() != 0)
        {
            TitleAudioPlayBgm();

            title_wrk.mode = TITLE_INIT_FROM_IN;
        }
    break;
    case TITLE_INIT_FROM_IN:
#ifdef BUILD_EU_VERSION
        title_wrk.load_id = LoadReqLanguage(TITLE_E_PK2, SPRITE_ADDRESS);
#else
        title_wrk.load_id = LoadReq(TITLE_PK2, SPRITE_ADDRESS);
#endif
        if (TitleUseRoomBackground() != 0)
        {
            TitleBgBeginRoomLoad();
        }

        title_wrk.mode = TITLE_WAIT_FROM_IN;
    break;
    case TITLE_WAIT_FROM_IN:
        if (IsLoadEnd(title_wrk.load_id) == 0)
        {
            break;
        }

        if (TitleUseRoomBackground() != 0 && TitleBgRoomReady() == 0)
        {
            break;
        }

        SeStopAll();

        title_wrk.csr = 0;
        title_wrk.mode = TITLE_TITLE_SEL;
    case TITLE_TITLE_SEL_BGMREQ:
        if (IsEndAdpcmFadeOut() != 0)
        {
            TitleAudioPlayBgm();

            title_wrk.mode = TITLE_TITLE_SEL_INIT;
        }
    break;
    case TITLE_TITLE_SEL_INIT:
        title_wrk.csr = 1;
        title_wrk.mode = TITLE_TITLE_SEL;
    case TITLE_TITLE_SEL:
        TitleBgDrawRoom();
        SetSprFile(MikuPan_GetHostAddress(SPRITE_ADDRESS));

        if (L1_PRESSED() >= 1 && R1_PRESSED()  >= 1)
        {
            ttl_dsp.mode = 0;
        }

        if (ttl_dsp.mode != 0)
        {
            TitleStartSlctYW(0, 0x80);
        }
        else
        {
            SetFTIM2File(FontTextAddress);
            TitleStartSlct();
        }

        DispOutDither();
        TitleBgDebugUi();
    break;
    case TITLE_LOAD_PRE:
        if (IsLoadEnd(title_wrk.load_id) != 0)
        {
            sys_wrk.load = 1;
            title_wrk.mode = TITLE_MEMCA_LOAD_WAIT;
            title_wrk.sub_mode = 0;
        }
    break;
    case TITLE_MEMCA_LOAD_WAIT:
        if (IsEndAdpcmFadeOut() != 0)
        {
            EAdpcmCmdPlay(0, 1, AB018_STR, 0, GetAdpcmVol(AB018_STR), 0x280, 0xfff, 0);

            title_wrk.mode = TITLE_MEMCA_LOAD;
        }
    break;
    case TITLE_MEMCA_LOAD:
        if (title_wrk.sub_mode == 0)
        {
            mcInit(1, (u_int *)MikuPan_GetHostPointer(MC_WORK_ADDRESS), 0);
            title_wrk.sub_mode = 7;
        }

        SetSprFile(MikuPan_GetHostAddress(SPRITE_ADDR_4));
        SetSprFile(MikuPan_GetHostAddress(SPRITE_ADDR_2));
        SetSprFile(MikuPan_GetHostAddress(SPRITE_ADDR_3));

        switch (McAtLoadChk(1))
        {
            case 0:
                // do nothing ...
            break;
            case 1:
                ingame_wrk.stts &= (0x80 | 0x40 | 0x10 | 0x8 | 0x4 | 0x2 | 0x1);

                InitialDmaBuffer();

                if (ingame_wrk.clear_count != 0)
                {
                    ModeSlctInit(0, 0);

                    OutGameModeChange(OUTGAME_MODE_MODESEL);
                }
                else
                {
                    EAdpcmFadeOut(0x3c);

                    LoadGameInit();

                    ingame_wrk.game = 0;

                    GameModeChange(GAME_MODE_INIT);
                }
            break;
            case 2:
                EAdpcmFadeOut(0x3c);

                title_wrk.mode = TITLE_TITLE_SEL_BGMREQ;

                ingame_wrk.stts &= (0x80 | 0x40 | 0x10 | 0x8 | 0x4 | 0x2 | 0x1);

                InitialDmaBuffer();
            break;
        }
    break;
    case TITLE_MODE_SEL_BGMREQ:
        if (IsEndAdpcmFadeOut() != 0)
        {
            TitleAudioPlayBgm();

            title_wrk.mode = TITLE_MODE_SEL;
        }
    break;
    case TITLE_MODE_SEL:
        TitleBgDrawRoom();
        SetSprFile(MikuPan_GetHostAddress(SPRITE_ADDRESS));

        if (ttl_dsp.mode != 0)
        {
            TitleSelectModeYW(0, 0x80);
        }
        else
        {
            SetFTIM2File(FontTextAddress);
            TitleSelectMode();
        }

        DispOutDither();
        TitleBgDebugUi();
    break;
    case TITLE_DIFF_SEL:
        TitleBgDrawRoom();
        SetSprFile(MikuPan_GetHostAddress(SPRITE_ADDRESS));
        TitleSelectDifficultyYW();
        TitleBgDebugUi();
    break;
    case TITLE_ALBM_LOAD_PRE:
        if (IsLoadEnd(title_wrk.load_id) != 0)
        {
            sys_wrk.load = 1;

            mc_pnum2 = mc_pnum1 = 0xff;
            mc_atyp2 = mc_atyp1 = 0xff;
            mc_slot2 = mc_slot1 = 0xff;
            mc_file2 = mc_file1 = 0xff;

            mcInit(5, (u_int *)MikuPan_GetHostPointer(MC_WORK_ADDRESS), 0);

            title_wrk.mode = TITLE_ALBM_LOAD0;
        }
    break;
    case TITLE_ALBM_LOAD0:
        if (IsEndAdpcmFadeOut() != 0)
        {
            EAdpcmCmdPlay(0, 1, AB018_STR, 0, GetAdpcmVol(AB018_STR), 0x280, 0xfff, 0);

            title_wrk.mode = TITLE_ALBM_LOAD1;
        }
    break;
    case TITLE_ALBM_LOAD1:
        SetSprFile(MikuPan_GetHostAddress(SPRITE_ADDR_4));
        SetSprFile(MikuPan_GetHostAddress(SPRITE_ADDR_2));
        SetSprFile(MikuPan_GetHostAddress(SPRITE_ADDR_3));

        switch (McAtLoadChk(2))
        {
        case 0: break;
        case 1:
            album_save_dat[0] = mc_album_save;
            mc_pnum1 = mc_photo_num;
            mc_atyp1 = mc_album_type;
            mc_slot1 = mc_ctrl.port + 1;
            mc_file1 = mc_ctrl.sel_file + 1;

            memcpy((void *)MikuPan_GetHostPointer(0xE80000), (void *)MikuPan_GetHostPointer(0x5a0000), 0x180000);

            mcInit(6, (u_int *)MikuPan_GetHostPointer(MC_WORK_ADDRESS), 0);

            title_wrk.mode = TITLE_ALBM_LOAD2;
            break;
        case 3: info_log("ここに来るわけがない！ (code shouldnt have reached here!)"); break;
        case 2:
            EAdpcmFadeOut(0x3c);

            title_wrk.mode = TITLE_TITLE_SEL_BGMREQ;
            break;
        }
    break;
    case TITLE_ALBM_LOAD2:
        SetSprFile(MikuPan_GetHostAddress(SPRITE_ADDR_2));
        SetSprFile(MikuPan_GetHostAddress(SPRITE_ADDR_3));

        switch (McAtLoadChk(2))
        {
        case 0:
            // do nothing ...
        break;
        case 1:
            album_save_dat[1] = mc_album_save;

            mc_pnum2 = mc_photo_num;
            mc_atyp2 = mc_album_type;
            mc_slot2 = mc_ctrl.port + 1;
            mc_file2 = mc_ctrl.sel_file + 1;
            memcpy((void *)MikuPan_GetHostPointer(0x1000000), (void *)MikuPan_GetHostPointer(0x5a0000), 0x180000);

            MemAlbmInit(1, mc_pnum1, mc_pnum2, mc_atyp1, mc_atyp2, mc_slot1, mc_slot2, mc_file1, mc_file2 & 0xff);

#ifdef BUILD_EU_VERSION
                title_wrk.load_id = LoadReqLanguage(PL_ALBM_FSM_E_PK2, SPRITE_ADDR_1);
                title_wrk.load_id = LoadReqLanguage(PL_ALBM_E_PK2, SPRITE_ADDR_2);
#else
                title_wrk.load_id = LoadReq(PL_ALBM_FSM_PK2, SPRITE_ADDR_1);
                title_wrk.load_id = LoadReq(PL_ALBM_PK2, SPRITE_ADDR_2);
#endif

            title_wrk.load_id = AlbmDesignLoad(0, mc_atyp1);
            title_wrk.load_id = AlbmDesignLoad(1, mc_atyp2);

            title_wrk.mode = TITLE_ALBM_MAIN_PRE;
        break;
        case 3:
            mc_pnum2 = 0;
            mc_atyp2 = 5;
            mc_slot2 = 0;
            mc_file2 = 0;

            memcpy((void *)MikuPan_GetHostPointer(0x1000000), (void *)MikuPan_GetHostPointer(0x5a0000), 0x180000);

            MemAlbmInit(1, mc_pnum1, mc_pnum2, mc_atyp1, mc_atyp2, mc_slot1, mc_slot2, mc_file1, mc_file2 & 0xff);
            NewAlbumInit(1);

#ifdef BUILD_EU_VERSION
                title_wrk.load_id = LoadReqLanguage(PL_ALBM_FSM_E_PK2, SPRITE_ADDR_1);
                title_wrk.load_id = LoadReqLanguage(PL_ALBM_E_PK2, SPRITE_ADDR_2);
#else
                title_wrk.load_id = LoadReq(PL_ALBM_FSM_PK2, SPRITE_ADDR_1);
                title_wrk.load_id = LoadReq(PL_ALBM_PK2, SPRITE_ADDR_2);
#endif

            title_wrk.load_id = AlbmDesignLoad(0, mc_atyp1);
            title_wrk.load_id = AlbmDesignLoad(1, mc_atyp2);

            title_wrk.mode = TITLE_ALBM_MAIN_PRE;
        break;
        case 2:
            EAdpcmFadeOut(0x3c);

            title_wrk.mode = TITLE_TITLE_SEL_BGMREQ;
        break;
        }
    break;
    case TITLE_ALBM_MAIN_PRE:
        if (IsLoadEnd(title_wrk.load_id) != 0)
        {
            title_wrk.mode = TITLE_ALBM_MAIN;
        }
    break;
    case TITLE_ALBM_MAIN:
        SetSprFile(MikuPan_GetHostAddress(SPRITE_ADDR_1));
        SetSprFile(MikuPan_GetHostAddress(SPRITE_ADDR_2));

        switch(SweetMemories(1, 0x80))
        {
        case 0: break;
        case 1:
            memcpy((void *)MikuPan_GetHostPointer(0x5a0000), (void *)MikuPan_GetHostPointer(0xe80000), 0x180000);

            mcInit(2, (u_int *)MikuPan_GetHostAddress(MC_WORK_ADDRESS), 0);

            title_wrk.load_side = 0;

#ifdef BUILD_EU_VERSION
                title_wrk.load_id = LoadReqLanguage(PL_PSVP_E_PK2, SPRITE_ADDR_4);
                title_wrk.load_id = LoadReqLanguage(PL_SAVE_E_PK2, SPRITE_ADDR_2);
                title_wrk.load_id = LoadReqLanguage(PL_ALBM_SAVE_E_PK2, SPRITE_ADDR_3);
#else
                title_wrk.load_id = LoadReq(PL_PSVP_PK2, SPRITE_ADDR_4);
                title_wrk.load_id = LoadReq(PL_SAVE_PK2, SPRITE_ADDR_2);
                title_wrk.load_id = LoadReq(PL_ALBM_SAVE_PK2, SPRITE_ADDR_3);
#endif

            title_wrk.mode = TITLE_ALBM_SAVE_PRE;
        break;
        case 2:
            memcpy((void *)MikuPan_GetHostPointer(0x5a0000), (void *)MikuPan_GetHostPointer(0x1000000), 0x180000);

            mcInit(2, (u_int *)MikuPan_GetHostAddress(MC_WORK_ADDRESS), 0);

            title_wrk.load_side = 1;

#ifdef BUILD_EU_VERSION
                title_wrk.load_id = LoadReqLanguage(PL_PSVP_E_PK2, SPRITE_ADDR_4);
                title_wrk.load_id = LoadReqLanguage(PL_SAVE_E_PK2, SPRITE_ADDR_2);
                title_wrk.load_id = LoadReqLanguage(PL_ALBM_SAVE_E_PK2, SPRITE_ADDR_3);
#else
                title_wrk.load_id = LoadReq(PL_PSVP_PK2, SPRITE_ADDR_4);
                title_wrk.load_id = LoadReq(PL_SAVE_PK2, SPRITE_ADDR_2);
                title_wrk.load_id = LoadReq(PL_ALBM_SAVE_PK2, SPRITE_ADDR_3);
#endif

            title_wrk.mode = TITLE_ALBM_SAVE_PRE;
        break;
        case 3:
#ifdef BUILD_EU_VERSION
                title_wrk.load_id = LoadReqLanguage(PL_PSVP_E_PK2, SPRITE_ADDR_4);
                title_wrk.load_id = LoadReqLanguage(PL_SAVE_E_PK2, SPRITE_ADDR_2);
                title_wrk.load_id = LoadReqLanguage(PL_ALBM_SAVE_E_PK2, SPRITE_ADDR_3);
#else
                title_wrk.load_id = LoadReq(PL_PSVP_PK2, SPRITE_ADDR_4);
                title_wrk.load_id = LoadReq(PL_SAVE_PK2, SPRITE_ADDR_2);
                title_wrk.load_id = LoadReq(PL_ALBM_SAVE_PK2, SPRITE_ADDR_3);
#endif

            title_wrk.load_side = 0;

            mcInit(5, (u_int *)MikuPan_GetHostAddress(MC_WORK_ADDRESS), 0);

            title_wrk.mode = TITLE_ALBM_LOAD_MODE_PRE;
        break;
        case 4:
#ifdef BUILD_EU_VERSION
                title_wrk.load_id = LoadReqLanguage(PL_PSVP_E_PK2, SPRITE_ADDR_4);
                title_wrk.load_id = LoadReqLanguage(PL_SAVE_E_PK2, SPRITE_ADDR_2);
                title_wrk.load_id = LoadReqLanguage(PL_ALBM_SAVE_E_PK2, SPRITE_ADDR_3);
#else
                title_wrk.load_id = LoadReq(PL_PSVP_PK2, SPRITE_ADDR_4);
                title_wrk.load_id = LoadReq(PL_SAVE_PK2, SPRITE_ADDR_2);
                title_wrk.load_id = LoadReq(PL_ALBM_SAVE_PK2, SPRITE_ADDR_3);
#endif

            title_wrk.load_side = 1;

            mcInit(5, (u_int *)MikuPan_GetHostAddress(MC_WORK_ADDRESS), 0);

            title_wrk.mode = TITLE_ALBM_LOAD_MODE_PRE;
        break;
        case 5:
            EAdpcmFadeOut(0x3c);

            title_wrk.mode = TITLE_TITLE_SEL_BGMREQ;
            title_wrk.csr = 0;

            ingame_wrk.stts &= (0x80 | 0x40 | 0x10 | 0x8 | 0x4 | 0x2 | 0x1);

            InitialDmaBuffer();
        break;
        }
    break;
    case TITLE_ALBM_LOAD_MODE_PRE:
        if (IsLoadEnd(title_wrk.load_id) != 0)
        {
            title_wrk.mode = TITLE_ALBM_LOAD_MODE;
        }
    break;
    case TITLE_ALBM_LOAD_MODE:
        SetSprFile(MikuPan_GetHostAddress(SPRITE_ADDR_4));
        SetSprFile(MikuPan_GetHostAddress(SPRITE_ADDR_2));
        SetSprFile(MikuPan_GetHostAddress(SPRITE_ADDR_3));

        switch (McAtLoadChk(2))
        {
        case 0:
            // do nothing ...
        break;
        case 3:
        case 1:
            if (title_wrk.load_side == 0)
            {
                memcpy((void *)MikuPan_GetHostPointer(0xe80000), (void *)MikuPan_GetHostPointer(0x5a0000), 0x180000);

                mc_pnum1 = mc_photo_num;
                mc_atyp1 = mc_album_type;
                mc_slot1 = mc_ctrl.port + 1;
                mc_file1 = mc_ctrl.sel_file + 1;

                album_save_dat[0] = mc_album_save;

                MemAlbmInit2(0, mc_pnum1, mc_atyp1, mc_slot1, mc_file1);
            }
            else
            {
                memcpy((void *)MikuPan_GetHostPointer(0x1000000), (void *)MikuPan_GetHostPointer(0x5a0000), 0x180000);

                mc_pnum2 = mc_photo_num;
                mc_atyp2 = mc_album_type;
                mc_slot2 = mc_ctrl.port + 1;
                mc_file2 = mc_ctrl.sel_file + 1;
                album_save_dat[1] = mc_album_save;
                MemAlbmInit2(1, mc_pnum2, mc_atyp2, mc_slot2, mc_file2);
            }

            title_wrk.load_id = AlbmDesignLoad(0, mc_atyp1);
            title_wrk.load_id = AlbmDesignLoad(1, mc_atyp2);

#ifdef BUILD_EU_VERSION
                title_wrk.load_id = LoadReqLanguage(PL_ALBM_E_PK2, SPRITE_ADDR_2);
#else
                title_wrk.load_id = LoadReq(PL_ALBM_PK2, SPRITE_ADDR_2);
#endif

            title_wrk.mode = TITLE_ALBM_MAIN_PRE;
        break;
        case 2:
            MemAlbmInit3();
            AlbmDesignLoad(0, mc_atyp1);
            AlbmDesignLoad(1, mc_atyp2);

#ifdef BUILD_EU_VERSION
                title_wrk.load_id = LoadReqLanguage(PL_ALBM_E_PK2, SPRITE_ADDR_2);
#else
                title_wrk.load_id = LoadReq(PL_ALBM_PK2, SPRITE_ADDR_2);
#endif

            title_wrk.mode = TITLE_ALBM_MAIN_PRE;
        break;
        }
    break;
    case TITLE_ALBM_SAVE_PRE:
        if (IsLoadEnd(title_wrk.load_id) != 0)
        {
            title_wrk.mode = TITLE_ALBM_SAVE;
        }
    break;
    case TITLE_ALBM_SAVE:
        SetSprFile(MikuPan_GetHostAddress(SPRITE_ADDR_4));
        SetSprFile(MikuPan_GetHostAddress(SPRITE_ADDR_2));
        SetSprFile(MikuPan_GetHostAddress(SPRITE_ADDR_3));

        switch (McAtAlbmChk())
        {
        case 0:
            // do nothing ...
        break;
        case 1:
            if (title_wrk.load_side == 0)
            {
                mc_pnum1 = mc_photo_num;
                mc_atyp1 = mc_album_type;
                mc_slot1 = mc_ctrl.port + 1;
                mc_file1 = mc_ctrl.sel_file + 1;

                MemAlbmInit2(0, mc_pnum1, mc_atyp1, mc_slot1, mc_file1);
            }
            else
            {
                mc_pnum2 = mc_photo_num;
                mc_atyp2 = mc_album_type;
                mc_slot2 = mc_ctrl.port + 1;
                mc_file2 = mc_ctrl.sel_file + 1;
                MemAlbmInit2(1, mc_pnum2, mc_atyp2, mc_slot2, mc_file2);
            }

            AlbmDesignLoad(0, mc_atyp1);
            AlbmDesignLoad(1, mc_atyp2);

#ifdef BUILD_EU_VERSION
                title_wrk.load_id = LoadReqLanguage(PL_ALBM_E_PK2, SPRITE_ADDR_2);
#else
                title_wrk.load_id = LoadReq(PL_ALBM_PK2, SPRITE_ADDR_2);
#endif

            title_wrk.mode = TITLE_ALBM_MAIN_PRE;
        break;
        case 2:
            MemAlbmInit3();

            AlbmDesignLoad(0, mc_atyp1);
            AlbmDesignLoad(1, mc_atyp2);

#ifdef BUILD_EU_VERSION
                title_wrk.load_id = LoadReqLanguage(PL_ALBM_E_PK2, SPRITE_ADDR_2);
#else
                title_wrk.load_id = LoadReq(PL_ALBM_PK2, SPRITE_ADDR_2);
#endif

            title_wrk.mode = TITLE_ALBM_MAIN_PRE;
        break;
        }
    break;
    case TITLE_MODE_SELECT:
        ModeSlctLoop();
    break;
    }

    if (ttl_dsp.timer < 60*2)
    {
        ttl_dsp.timer++;
    }
    else
    {
        ttl_dsp.timer = 0;
    }
}

static int TitleExitPromptButton(const char *label, int selected)
{
    int clicked;

    if (selected)
    {
        igPushStyleColor_Vec4(ImGuiCol_Button, (ImVec4){0.26f, 0.59f, 0.98f, 1.0f});
        igPushStyleColor_Vec4(ImGuiCol_ButtonHovered, (ImVec4){0.31f, 0.64f, 1.00f, 1.0f});
        igPushStyleColor_Vec4(ImGuiCol_ButtonActive, (ImVec4){0.20f, 0.50f, 0.90f, 1.0f});
    }

    clicked = igButton(label, (ImVec2){110.0f, 0.0f}) ? 1 : 0;

    if (selected)
    {
        igPopStyleColor(3);
    }

    return clicked;
}

static void TitleExitPrompt()
{
    ImGuiIO *io = igGetIO_Nil();
    ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize
        | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse
        | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_AlwaysAutoResize
        | ImGuiWindowFlags_NoNavInputs | ImGuiWindowFlags_NoNavFocus;
    int left;
    int right;
    int confirm;
    int yes_clicked;
    int no_clicked;

    igSetNextWindowPos(
        (ImVec2){io->DisplaySize.x * 0.5f, io->DisplaySize.y * 0.5f},
        ImGuiCond_Always, (ImVec2){0.5f, 0.5f});

    igBegin("##title_exit_prompt", NULL, flags);

    igText("Exit the game?");
    igDummy((ImVec2){0.0f, 6.0f});

    yes_clicked = TitleExitPromptButton("Yes", exit_prompt_sel == 0);
    igSameLine(0.0f, 16.0f);
    no_clicked = TitleExitPromptButton("No", exit_prompt_sel == 1);

    igEnd();

    left = (DPAD_LEFT_PRESSED() == 1) || (Ana2PadDirCnt(3) == 1);
    right = (DPAD_RIGHT_PRESSED() == 1) || (Ana2PadDirCnt(1) == 1);

    if (left || right)
    {
        exit_prompt_sel = exit_prompt_sel == 0;
        SeStartFix(SE_CSR0, 0, 0x1000, 0x1000, 0);
    }

    if (yes_clicked)
    {
        exit_prompt_sel = 0;
    }
    else if (no_clicked)
    {
        exit_prompt_sel = 1;
    }

    confirm = (CROSS_PRESSED() == 1) || (START_PRESSED() == 1)
              || yes_clicked || no_clicked;

    if (confirm)
    {
        if (exit_prompt_sel == 0)
        {
            SeStartFix(SE_CLIC, 0, 0x1000, 0x1000, 0);
            MikuPan_RequestQuit();
        }
        else
        {
            exit_prompt_open = 0;
            SeStartFix(SE_CANCEL, 0, 0x1000, 0x1000, 0);
        }
    }
    else if (TRIANGLE_PRESSED() == 1 || CIRCLE_PRESSED() == 1)
    {
        exit_prompt_open = 0;
        SeStartFix(SE_CANCEL, 0, 0x1000, 0x1000, 0);
    }
}

void TitleWaitMode()
{
    /* f20 58 */ float alp;
    /* 0x0(sp) */ DISP_SPRT ds;
    int f;

    TitleDrawClassicBackground(0, 0);
    TitleDrawGameTitle();

    if (title_wrk.mode == TITLE_TITLE_WAIT)
    {
        return;
    }

    f = ttl_dsp.timer % 120;
    alp = (SgSinf(f / 120.0f * (3.1415927f * 2)) + 1.0f) * 64.0f;

    CopySprDToSpr(&ds, &font_sprt[17]);

    ds.alphar = SCE_GS_SET_ALPHA_1(SCE_GS_ALPHA_CS, SCE_GS_ALPHA_ZERO, SCE_GS_ALPHA_AS, SCE_GS_ALPHA_CD, 0);
    ds.alpha = alp;
    ds.tex1 = SCE_GS_SET_TEX1_1(1, 0, SCE_GS_LINEAR, SCE_GS_LINEAR_MIPMAP_LINEAR, 0, 0, 0);

    DispSprD(&ds);

    float message_y = PS2_RESOLUTION_Y_FLOAT - PS2_RESOLUTION_Y_FLOAT / 6 + 3;
    float message_x = PS2_RESOLUTION_X_FLOAT / 2 - 120;

    u_char r_message = 77;
    u_char g_message = 63;
    u_char b_message = 68;

    SetASCIIString3(0x10, message_x, message_y, 0, r_message, g_message, b_message, alp, (char *) "PRESS   TO LEAVE");
    SetASCIIString3(0x10, message_x + (7*12)+2, message_y, 2, r_message, g_message, b_message, alp, (char *) "\xD8");

    ttl_dsp.mode = 0;

    if (exit_prompt_open)
    {
        TitleExitPrompt();
        return;
    }

    if (CROSS_PRESSED() == 1 || START_PRESSED() == 1)
    {
        ttl_dsp.mode = 1;

        title_wrk.mode = TITLE_TITLE_SEL_INIT;
        title_wrk.csr = 0;

        SeStartFix(SE_CLIC, 0, 4096, 4096, 0);
    }
    else if (TRIANGLE_PRESSED() == 1)
    {
        /* Open the exit confirmation. The popup is rendered/handled from the
         * next frame so this same TRIANGLE press cannot also cancel it. */
        exit_prompt_open = 1;
        exit_prompt_sel = 1;

        SeStartFix(SE_CLIC, 0, 4096, 4096, 0);
    }
}

void TitleStartSlct()
{
    char *str_o = "o";
	/* s0 16 */ char *str1 = "ZERO HOUR";
	/* s1 17 */ char *str2 = "NEW GAME";
	/* s2 18 */ char *str3 = "LOAD GAME";
	/* s3 19 */ char *str4 = "ALBUM";
	/* s4 20 */ char *csr0 = "MISSION";

    TitleDrawClassicBackground(0, 0);

    SetASCIIString(70.0f, 110.0f, str1);

    SetASCIIString(160.0f, 190.0f, str2);
    SetASCIIString(160.0f, 230.0f, str3);
    SetASCIIString(160.0f, 270.0f, str4);
    SetASCIIString(230.0f, 350.0f, csr0);

    SetInteger2(0, 350.0f, 350.0f, 0, 0x80, 0x80, 0x80, ingame_wrk.msn_no);

    SetASCIIString(120.0f, title_wrk.csr * 40 + 190, str_o);

    if (
        DPAD_RIGHT_PRESSED() == 1 ||
        (DPAD_RIGHT_PRESSED() > 25 && (DPAD_RIGHT_PRESSED() % 5) == 1) ||
        Ana2PadDirCnt(1) == 1 ||
        (Ana2PadDirCnt(1) > 25 && (Ana2PadDirCnt(1) % 5) == 1)
    ) {
        ingame_wrk.msn_no++;
    }
    else if (
        DPAD_LEFT_PRESSED() == 1 ||
        (DPAD_LEFT_PRESSED() > 25 && (DPAD_LEFT_PRESSED() % 5) == 1) ||
        Ana2PadDirCnt(3) == 1 ||
        (Ana2PadDirCnt(3) > 25 && (Ana2PadDirCnt(3) % 5) == 1))
    {
        if (ingame_wrk.msn_no != 0)
        {
            ingame_wrk.msn_no--;
        }
    }

    if (CROSS_PRESSED() == 1 || START_PRESSED() == 1)
    {
        if (title_wrk.csr != 0x0)
        {
#ifdef BUILD_EU_VERSION
            title_wrk.load_id = LoadReq(PL_BGBG_PK2, 0x1cfefc0);
            title_wrk.load_id = LoadReqLanguage(PL_STTS_E_PK2, 0x1ce0000);
            title_wrk.load_id = LoadReqLanguage(PL_PSVP_E_PK2, SPRITE_ADDR_4);
            title_wrk.load_id = LoadReqLanguage(PL_SAVE_E_PK2, SPRITE_ADDR_2);
            title_wrk.load_id = LoadReq(SV_PHT_PK2, SPRITE_ADDR_3);
#else
            title_wrk.load_id = LoadReq(PL_BGBG_PK2, 0x1d05140);
            title_wrk.load_id = LoadReq(PL_STTS_PK2, 0x1ce0000);
            title_wrk.load_id = LoadReq(PL_PSVP_PK2, SPRITE_ADDR_4);
            title_wrk.load_id = LoadReq(PL_SAVE_PK2, SPRITE_ADDR_2);
            title_wrk.load_id = LoadReq(SV_PHT_PK2, SPRITE_ADDR_3);
#endif

            title_wrk.mode = TITLE_LOAD_PRE;

            EAdpcmFadeOut(0x3c);
        }
        else
        {
            NewGameInit();

            title_wrk.mode = TITLE_MODE_SEL;
            title_wrk.csr = 0;
        }

        SeStartFix(SE_CLIC, 0, 0x1000, 0x1000, 0);
    }
    else if (TRIANGLE_PRESSED() == 1)
    {
        ttl_dsp.timer = 0;
        title_wrk.mode = TITLE_TITLE;
        SeStartFix(SE_CANCEL, 0, 0x1000, 0x1000, 0);
    }
    else if (
        DPAD_UP_PRESSED() == 1 ||
        (DPAD_UP_PRESSED() > 25 && (DPAD_UP_PRESSED() % 5) == 1) ||
        Ana2PadDirCnt(0) == 1 ||
        (Ana2PadDirCnt(0) > 25 && (Ana2PadDirCnt(0) % 5) == 1)
    ) {
        title_wrk.csr = 0;
        SeStartFix(SE_CSR0, 0, 0x1000, 0x1000, 0);
    }
    else if (
        DPAD_DOWN_PRESSED() == 1 ||
        (DPAD_DOWN_PRESSED() > 25 && (DPAD_DOWN_PRESSED() % 5) == 1) ||
        Ana2PadDirCnt(2) == 1 ||
        (Ana2PadDirCnt(2) > 25 && (Ana2PadDirCnt(2) % 5) == 1)
    ) {
        title_wrk.csr = 1;
        SeStartFix(SE_CSR0, 0, 0x1000, 0x1000, 0);
    }
}

#ifdef BUILD_EU_VERSION
void TitleStartSlctYW(u_char pad_off, u_char alp_max)
{
    int i;
    u_char mode;
    int adj;
    u_char dsp;
    u_char rgb;
    u_char chr;
    u_char alp;
    u_int textbl[4] = { 8, 0, 7, 20 };
    DISP_SPRT ds;

    adj = 28;

    if (
        title_wrk.csr == 3 && (
            *key_now[3] == 1 ||
            (*key_now[3] > 25 && (*key_now[3] % 5) == 1) ||
            Ana2PadDirCnt(1) == 1 ||
            (Ana2PadDirCnt(1) > 25 && (Ana2PadDirCnt(1) % 5) == 1)
        )
    )
    {
        ChangeTVMode(1);
        SeStartFix(0, 0, 0x1000, 0x1000, 0);
    } else if (
        title_wrk.csr == 3 && (
            *key_now[2] == 1 ||
            (*key_now[2] > 25 && (*key_now[2] % 5) == 1) ||
            Ana2PadDirCnt(3) == 1 ||
            (Ana2PadDirCnt(3) > 25 && (Ana2PadDirCnt(3) % 5) == 1)
        )
    )
    {
        ChangeTVMode(0);
        SeStartFix(0, 0, 0x1000, 0x1000, 0);
    }

    if (ttl_dsp.no_disp == 0)
    {
        for (i = 0; i < 11; i++)
        {
            CopySprDToSpr(&ds, &title_sprt[i]);

            ds.alpha = alp_max;

            DispSprD(&ds);
        }
    }

    for (mode = 0; mode < 4; mode++)
    {
        if (ttl_dsp.no_disp == 0)
        {
            chr = textbl[mode];
            dsp = mode == title_wrk.csr ? 1 : 0;

            CopySprDToSpr(&ds, &font_sprt[chr]);

            ds.y += mode * adj;

            alp = rgb = dsp * (alp_max / 2) + (alp_max / 2);

            ds.alpha = alp;
            ds.r = rgb;
            ds.g = rgb;
            ds.b = rgb;

            if (dsp != 0)
            {
                ds.alphar = SCE_GS_SET_ALPHA_1(SCE_GS_ALPHA_CS, SCE_GS_ALPHA_ZERO, SCE_GS_ALPHA_AS, SCE_GS_ALPHA_CD, 0);
            }

            ds.tex1 = SCE_GS_SET_TEX1_1(1, 0, SCE_GS_LINEAR, SCE_GS_LINEAR_MIPMAP_LINEAR, 0, 0, 0);

            DispSprD(&ds);

            if (mode == 3 && title_wrk.csr == 3)
            {
                dsp = sys_wrk.pal_disp_mode == 0;

                CopySprDToSpr(&ds, &font_sprt[21]);

                ds.y += mode * adj;

                alp = rgb = dsp * (alp_max / 2) + (alp_max / 2);

                ds.alpha = alp;
                ds.r = rgb;
                ds.g = rgb;
                ds.b = rgb;

                if (dsp != 0)
                {
                    ds.alphar = SCE_GS_SET_ALPHA_1(SCE_GS_ALPHA_CS, SCE_GS_ALPHA_ZERO, SCE_GS_ALPHA_AS, SCE_GS_ALPHA_CD, 0);
                }

                DispSprD(&ds);

                dsp = sys_wrk.pal_disp_mode == 1;

                CopySprDToSpr(&ds, &font_sprt[22]);

                ds.y += mode * adj;

                alp = rgb = dsp * (alp_max / 2) + (alp_max / 2);

                ds.alpha = alp;
                ds.r = rgb;
                ds.g = rgb;
                ds.b = rgb;

                if (dsp)
                {
                    ds.alphar = SCE_GS_SET_ALPHA_1(SCE_GS_ALPHA_CS, SCE_GS_ALPHA_ZERO, SCE_GS_ALPHA_AS, SCE_GS_ALPHA_CD, 0);
                }

                DispSprD(&ds);
            }
        }
    }

    if (pad_off != 0)
    {
        return;
    }

    if (*key_now[5] == 1 || *key_now[0xc] == 1)
    {
        ingame_wrk.clear_count = 0;
        ingame_wrk.ghost_cnt = 0;
        ingame_wrk.rg_pht_cnt = 0;
        ingame_wrk.pht_cnt = 0;
        ingame_wrk.high_score = 0;
        ingame_wrk.difficult = 0;

        cribo.costume = 0;
        cribo.clear_info = 0;

        NewgameMenuAlbumInit();

        realtime_scene_flg = 0;

        MovieInitWrk();

        motInitMsn();

        switch (title_wrk.csr)
        {
        case 0:
            NewGameInit();

            title_wrk.mode = 9;
            title_wrk.csr = 0;

            SeStartFix(1, 0, 0x1000, 0x1000, 0);
        break;
        case 1:
            title_wrk.load_id = LoadReq(PL_BGBG_PK2, 0x1cfefc0);
            title_wrk.load_id = LoadReqLanguage(PL_STTS_E_PK2, 0x1ce0000);
            title_wrk.load_id = LoadReqLanguage(PL_PSVP_E_PK2, SPRITE_ADDR_4);
            title_wrk.load_id = LoadReqLanguage(PL_SAVE_E_PK2, SPRITE_ADDR_2);
            title_wrk.load_id = LoadReq(SV_PHT_PK2, SPRITE_ADDR_3);

            ingame_wrk.stts |= 0x20;

            InitialDmaBuffer();

            title_wrk.mode = 6;

            SeStartFix(1, 0, 0x1000, 0x1000, 0);
            EAdpcmFadeOut(60);
        break;
        case 2:
            title_wrk.load_id = LoadReq(PL_BGBG_PK2, 0x1cfefc0);
            title_wrk.load_id = LoadReqLanguage(PL_STTS_E_PK2, 0x1ce0000);
            title_wrk.load_id = LoadReqLanguage(PL_PSVP_E_PK2, SPRITE_ADDR_4);
            title_wrk.load_id = LoadReqLanguage(PL_SAVE_E_PK2, SPRITE_ADDR_2);
            title_wrk.load_id = LoadReqLanguage(PL_ALBM_SAVE_E_PK2, SPRITE_ADDR_3);

            ingame_wrk.stts |= 0x20;

            InitialDmaBuffer();

            title_wrk.mode = 14;

            SeStartFix(1, 0, 0x1000, 0x1000, 0);
            EAdpcmFadeOut(60);
        break;
        }
    }
    else if (*key_now[4] == 1)
    {
        ttl_dsp.timer = 0;
        title_wrk.mode = 2;

        SeStartFix(3, 0, 0x1000, 0x1000, 0);
    }
    else if (
        *key_now[1] == 1 ||
        (*key_now[1] > 25 && (*key_now[1] % 5) == 1) ||
        Ana2PadDirCnt(2) == 1 ||
        (Ana2PadDirCnt(2) > 25 && (Ana2PadDirCnt(2) % 5) == 1)
    )
    {
        if (title_wrk.csr < 3)
        {
            title_wrk.csr++;
        }
        else
        {
            title_wrk.csr = 0;
        }

        SeStartFix(0, 0, 0x1000, 0x1000, 0);
    }
    else if (
        *key_now[0] == 1 ||
        (*key_now[0] > 25 && (*key_now[0] % 5) == 1) ||
        Ana2PadDirCnt(0) == 1 ||
        (Ana2PadDirCnt(0) > 25 && (Ana2PadDirCnt(0) % 5) == 1)
    )
    {
        if (title_wrk.csr > 0)
        {
            title_wrk.csr--;
        }
        else
        {
            title_wrk.csr = 3;
        }

        SeStartFix(0, 0, 0x1000, 0x1000, 0);
    }

    if (ttl_dsp.no_disp != 0)
    {
        ttl_dsp.no_disp--;
    }
}
#else
void TitleStartSlctYW(u_char pad_off, u_char alp_max)
{
	/* s1 17 */ u_char mode;
	/* fp 30 */ u_char adj;
	/* s3 19 */ u_char dsp;
	/* s6 22 */ u_char chr1;
	/* s5 21 */ u_char chr2;
	/* v0 2 */ u_char alp;
	/* s0 16 */ u_char rgb;
	/* 0x0(sp) */ DISP_SPRT ds;

    TitleDrawClassicBackground(1, alp_max);

    for (mode = 0; mode < 3; mode++)
    {
        switch (mode)
        {
        case 0:
            chr1 = 8;
            chr2 = 4;

            switch (title_wrk.csr)
            {
            case 0:
                adj = 0;
                dsp = 1;
            break;
            case 1:
                adj = 0;
                dsp = 0;
            break;
            case 2:
                adj = 0;
                dsp = 0;
            break;
            }
        break;
        case 1:
            chr1 = 0;
            chr2 = 5;

            switch (title_wrk.csr)
            {
            case 0:
                adj = 35;
                dsp = 0;
            break;
            case 1:
                adj = 35;
                dsp = 1;
            break;
            case 2:
                adj = 35;
                dsp = 0;
            break;
            }
        break;
        case 2:
            chr1 = 7;
            chr2 = 0xff;

            switch (title_wrk.csr)
            {
            case 0:
                adj = 70;
                dsp = 0;
            break;
            case 1:
                adj = 70;
                dsp = 0;
            break;
            case 2:
                adj = 70;
                dsp = 1;
            break;
            }
        break;
        }

        CopySprDToSpr(&ds, &font_sprt[chr1]);

        ds.y += adj;

        alp = rgb = dsp * (alp_max / 2) + (alp_max / 2);

        ds.alpha = alp;
        ds.r = rgb; ds.g = rgb; ds.b = rgb;

        if (dsp != 0)
        {
            ds.alphar = SCE_GS_SET_ALPHA_1(SCE_GS_ALPHA_CS, SCE_GS_ALPHA_ZERO, SCE_GS_ALPHA_AS, SCE_GS_ALPHA_CD, 0);
        }

        ds.tex1 = SCE_GS_SET_TEX1_1(1, 0, SCE_GS_LINEAR, SCE_GS_LINEAR_MIPMAP_LINEAR, 0, 0, 0);

        DispSprD(&ds);

        if (chr2 != 0xff)
        {
            CopySprDToSpr(&ds, &font_sprt[chr2]);

            ds.y += adj;

            alp = rgb;

            ds.alpha = alp;
            ds.r = alp; ds.g = alp; ds.b = alp;

            if (dsp != 0)
            {
                ds.alphar = SCE_GS_SET_ALPHA_1(SCE_GS_ALPHA_CS, SCE_GS_ALPHA_ZERO, SCE_GS_ALPHA_AS, SCE_GS_ALPHA_CD, 0);
            }

            ds.tex1 = SCE_GS_SET_TEX1_1(1, 0, SCE_GS_LINEAR, SCE_GS_LINEAR_MIPMAP_LINEAR, 0, 0, 0);

            DispSprD(&ds);
        }
    }

    if (pad_off != 0)
    {
        return;
    }

    if (CROSS_PRESSED() == 1 || START_PRESSED() == 1)
    {
        ingame_wrk.clear_count = 0;
        ingame_wrk.ghost_cnt = 0;
        ingame_wrk.rg_pht_cnt = 0;
        ingame_wrk.pht_cnt = 0;
        ingame_wrk.high_score = 0;
        ingame_wrk.difficult = 0;

        cribo.costume = 0;
        cribo.clear_info = 0;

        NewgameMenuAlbumInit();

        realtime_scene_flg = 0;

        MovieInitWrk();

        motInitMsn();

        switch (title_wrk.csr)
        {
        case 0:
            NewGameInit();

            title_wrk.mode = 9;
            title_wrk.csr = 0;

            SeStartFix(SE_CLIC, 0, 0x1000, 0x1000, 0);
        break;
        case 1:
            title_wrk.load_id = LoadReq(PL_BGBG_PK2, PL_BGBG_PK2_ADDRESS);
            title_wrk.load_id = LoadReq(PL_STTS_PK2, PL_STTS_PK2_ADDRESS);
            title_wrk.load_id = LoadReq(PL_PSVP_PK2, PL_PSVP_PK2_ADDRESS);
            title_wrk.load_id = LoadReq(PL_SAVE_PK2, PL_SAVE_PK2_ADDRESS);
            title_wrk.load_id = LoadReq(SV_PHT_PK2, SV_PHT_PK2_ADDRESS);

            ingame_wrk.stts |= 0x20;

            InitialDmaBuffer();

            title_wrk.mode = 6;

            SeStartFix(SE_CLIC, 0, 0x1000, 0x1000, 0);
            EAdpcmFadeOut(60);
        break;
        case 2:
            title_wrk.load_id = LoadReq(PL_BGBG_PK2, PL_BGBG_PK2_ADDRESS);
            title_wrk.load_id = LoadReq(PL_STTS_PK2, PL_STTS_PK2_ADDRESS);
            title_wrk.load_id = LoadReq(PL_PSVP_PK2, PL_PSVP_PK2_ADDRESS);
            title_wrk.load_id = LoadReq(PL_SAVE_PK2, PL_SAVE_PK2_ADDRESS);
            title_wrk.load_id = LoadReq(PL_ALBM_SAVE_PK2, SV_PHT_PK2_ADDRESS);

            ingame_wrk.stts |= 0x20;

            InitialDmaBuffer();

            title_wrk.mode = 14;

            SeStartFix(SE_CLIC, 0, 0x1000, 0x1000, 0);
            EAdpcmFadeOut(60);
        break;
        }
    }
    else if (TRIANGLE_PRESSED() == 1)
    {
        ttl_dsp.timer = 0;
        title_wrk.mode = 2;

        SeStartFix(SE_CANCEL, 0, 0x1000, 0x1000, 0);
    }
    else if (
        DPAD_DOWN_PRESSED() == 1 ||
        (DPAD_DOWN_PRESSED() > 25 && (DPAD_DOWN_PRESSED() % 5) == 1) ||
        Ana2PadDirCnt(2) == 1 ||
        (Ana2PadDirCnt(2) > 25 && (Ana2PadDirCnt(2) % 5) == 1)
    )
    {
        if (title_wrk.csr < 2)
        {
            title_wrk.csr++;
        }
        else
        {
            title_wrk.csr = 0;
        }

        SeStartFix(SE_CSR0, 0, 0x1000, 0x1000, 0);
    }
    else if (
        DPAD_UP_PRESSED() == 1 ||
        (DPAD_UP_PRESSED() > 25 && (DPAD_UP_PRESSED() % 5) == 1) ||
        Ana2PadDirCnt(0) == 1 ||
        (Ana2PadDirCnt(0) > 25 && (Ana2PadDirCnt(0) % 5) == 1)
    )
    {
        if (title_wrk.csr > 0)
        {
            title_wrk.csr--;
        }
        else
        {
            title_wrk.csr = 2;
        }

        SeStartFix(SE_CSR0, 0, 0x1000, 0x1000, 0);
    }
}
#endif

void TitleLoadCtrl()
{
    /// does nothing
}

void TitleSelectMode()
{
    /* s1 17 */ int i;
    /* 0x0(sp) */ char *mode_str[10] = {
        "STORY MODE",
        "BATTLE MODE",
        "OPTION",
        "MAP DATA EDIT",
        "SOUND TEST",
        "SND TEST",
        "SCENE TEST",
        "MOTION TEST",
        "ROOM SIZE CHECK",
        "LAYOUT TEST",
    };
    /* s5 21 */ char *csr0 = "o";

    TitleDrawClassicBackground(0, 0);

    for (i = 0; i < 10; i++)
    {
        SetASCIIString(110.0f, 30 + 40 * i, mode_str[i]);
    }

    SetASCIIString(30.0f, 30 + title_wrk.csr * 40, csr0);


    if (
        DPAD_UP_PRESSED() == 1 ||
        (DPAD_UP_PRESSED() > 25 && (DPAD_UP_PRESSED() % 5) == 1) ||
        Ana2PadDirCnt(0) == 1 ||
        (Ana2PadDirCnt(0) > 25 && (Ana2PadDirCnt(0) % 5) == 1)
    )
    {
        if (title_wrk.csr > 0)
        {
            title_wrk.csr--;
        }
        else
        {
            title_wrk.csr = 0x9;
        }

        SeStartFix(SE_CSR0, 0, 0x1000, 0x1000, 0);
    }
    else if (
        DPAD_DOWN_PRESSED() == 1 ||
        (DPAD_DOWN_PRESSED() > 25 && (DPAD_DOWN_PRESSED() % 5) == 1) ||
        Ana2PadDirCnt(2) == 1 ||
        (Ana2PadDirCnt(2) > 25 && (Ana2PadDirCnt(2) % 5) == 1)
    )
    {
        if (title_wrk.csr < 9)
        {
            title_wrk.csr++;
        }
        else
        {
            title_wrk.csr = 0;
        }

        SeStartFix(SE_CSR0, 0, 0x1000, 0x1000, 0);
    }
    else if (TRIANGLE_PRESSED() == 1)
    {
        title_wrk.mode = 3;

        SeStartFix(SE_CANCEL, 0, 0x1000, 0x1000, 0);
    }
    else if (CROSS_PRESSED() == 1)
    {
        switch(title_wrk.csr)
        {
            case 0:
                ingame_wrk.game = 0;

                GameModeChange(GAME_MODE_INIT);
                break;
            case 1:
                OutGameModeChange(OUTGAME_MODE_BATTLE);
                break;
            case 2:
                OutGameModeChange(OUTGAME_MODE_OPTION);
                break;
            case 3: // MAP DATA EDIT
                OutGameModeChange(OUTGAME_MODE_MAP_DATA_EDIT);
                break;
            case 4:
                OutGameModeChange(OUTGAME_MODE_SOUND_TEST);
                break;
            case 5:
                OutGameModeChange(OUTGAME_MODE_SND_TEST);
                break;
            case 6:
                OutGameModeChange(OUTGAME_MODE_SCENE_TEST);
                break;
            case 7:
                OutGameModeChange(OUTGAME_MODE_MOTION_TEST);
                break;
            case 8:
                OutGameModeChange(OUTGAME_MODE_ROOM_SIZE_CHECK);
                break;
            case 9:
                OutGameModeChange(OUTGAME_MODE_LAYOUT_TEST);
                break;
        }

        SeStartFix(SE_CLIC, 0, 0x1000, 0x1000, 0);
    }
}

void TitleSelectModeYW(u_char pad_off, u_char alp_max)
{
	/* s4 20 */ u_char mode;
	/* 0x94(sp) */ u_char adj;
	/* s3 19 */ u_char dsp;
	/* 0x98(sp) */ u_char chr1;
	/* s5 21 */ u_char chr2;
	/* v0 2 */ u_char alp;
	/* s0 16 */ u_char rgb;
	/* 0x0(sp) */ DISP_SPRT ds;

    TitleDrawClassicBackground(1, alp_max);

    for (mode = 0; mode < 2; mode++)
    {
        switch (mode)
        {
        case 0:
            chr1 = 3;
            chr2 = 6;

            switch (title_wrk.csr)
            {
            case 0:
                adj = 17;
                dsp = 1;
            break;
            case 1:
                adj = 17;
                dsp = 0;
            break;
            case 2:
                adj = 17;
                dsp = 0;
            break;
            }
        break;
        case 1:
            chr1 = 9;
            chr2 = 0xff;

            switch (title_wrk.csr)
            {
            case 0:
                adj = 52;
                dsp = 0;
            break;
            case 1:
                adj = 52;
                dsp = 1;
            break;
            case 2:
                adj = 52;
                dsp = 0;
            break;
            }
        break;
        }

        CopySprDToSpr(&ds, &font_sprt[chr1]);

        ds.y += adj;

        alp = rgb = dsp * (alp_max / 2) + (alp_max / 2);

        ds.alpha = alp;
        ds.r = rgb; ds.g = rgb; ds.b = rgb;

        if (dsp != 0)
        {
            ds.alphar = SCE_GS_SET_ALPHA_1(SCE_GS_ALPHA_CS, SCE_GS_ALPHA_ZERO, SCE_GS_ALPHA_AS, SCE_GS_ALPHA_CD, 0);
        }

        ds.tex1 = SCE_GS_SET_TEX1_1(1, 0, SCE_GS_LINEAR, SCE_GS_LINEAR_MIPMAP_LINEAR, 0, 0, 0);

        DispSprD(&ds);

        if (chr2 != 0xff)
        {
            CopySprDToSpr(&ds, &font_sprt[chr2]);

            ds.y += adj;

            alp = rgb;

            ds.alpha = alp;
            ds.r = alp; ds.g = alp; ds.b = alp;

            if (dsp != 0)
            {
                ds.alphar = SCE_GS_SET_ALPHA_1(SCE_GS_ALPHA_CS, SCE_GS_ALPHA_ZERO, SCE_GS_ALPHA_AS, SCE_GS_ALPHA_CD, 0);
            }

            ds.tex1 = SCE_GS_SET_TEX1_1(1, 0, SCE_GS_LINEAR, SCE_GS_LINEAR_MIPMAP_LINEAR, 0, 0, 0);

            DispSprD(&ds);
        }
    }

    if (pad_off != 0)
    {
        return;
    }

    if (CROSS_PRESSED() == 1 || START_PRESSED() == 1)
    {
        switch (title_wrk.csr)
        {
        case 0:
            EAdpcmFadeOut(60);
            SeStartFix(SE_CLIC, 0, 0x1000, 0x1000, 0);

            ingame_wrk.game = 0;

            GameModeChange(0);
        break;
        case 1:
            SeStartFix(SE_CLIC, 0, 0x1000, 0x1000, 0);
            ModeSlctInit(3, 5);

            title_wrk.mode = 27;

            OutGameModeChange(OUTGAME_MODE_MODESEL);
            EAdpcmFadeOut(60);
        break;
        }
    }
    else if (TRIANGLE_PRESSED() == 1)
    {
        title_wrk.mode = 23;
        SeStartFix(SE_CANCEL, 0, 0x1000, 0x1000, 0);
    }
    else if (
        DPAD_DOWN_PRESSED() == 1 ||
        (DPAD_DOWN_PRESSED() > 25 && (DPAD_DOWN_PRESSED() % 5) == 1) ||
        Ana2PadDirCnt(2) == 1 ||
        (Ana2PadDirCnt(2) > 25 && (Ana2PadDirCnt(2) % 5) == 1)
    )
    {
        if (title_wrk.csr == 0)
        {
            title_wrk.csr++;
        }
        else
        {
            title_wrk.csr = 0;
        }

        SeStartFix(SE_CSR0, 0, 0x1000, 0x1000, 0);
    }
    else if (
        DPAD_UP_PRESSED() == 1 ||
        (DPAD_UP_PRESSED() > 25 && (DPAD_UP_PRESSED() % 5) == 1) ||
        Ana2PadDirCnt(0) == 1 ||
        (Ana2PadDirCnt(0) > 25 && (Ana2PadDirCnt(0) % 5) == 1)
    )
    {
        if (title_wrk.csr > 0)
        {
            title_wrk.csr--;
        }
        else
        {
            title_wrk.csr = 1;
        }

        SeStartFix(SE_CSR0, 0, 0x1000, 0x1000, 0);
    }
}

void TitleSelectDifficultyYW()
{
	/* s0 16 */ u_char chr;
	/* f0 38 */ float alp;
	/* 0x0(sp) */ DISP_SPRT ds;
    int f;

    TitleDrawClassicBackground(0, 0);

    CopySprDToSpr(&ds, &font_sprt[16]);

    ds.y += 35.0f;
    ds.alpha = 0x40;
    ds.r = 0x40;
    ds.g = 0x40;
    ds.b = 0x40;
    ds.tex1 = 0x161;

    DispSprD(&ds);

    f = ttl_dsp.timer % 120;
    alp = (SgSinf(f / 120.0f * (PI * 2)) + 1.0f) * 64.0f;

    for (chr = 11; chr < 15; chr++)
    {
        CopySprDToSpr(&ds, &font_sprt[chr]);

        if (chr == 12 || chr == 14)
        {
            ds.att |= 0x2;
        }

        ds.alpha = alp;
        ds.alphar = SCE_GS_SET_ALPHA_1(SCE_GS_ALPHA_CS, SCE_GS_ALPHA_ZERO, SCE_GS_ALPHA_AS, SCE_GS_ALPHA_CD, 0);
        ds.tex1 = SCE_GS_SET_TEX1_1(1, 0, SCE_GS_LINEAR, SCE_GS_LINEAR_MIPMAP_LINEAR, 0, 0, 0);

        DispSprD(&ds);
    }

    switch (title_wrk.csr)
    {
    case 0:
        chr = 1;
    break;
    case 1:
        chr = 10;
    break;
    case 2:
        chr = 2;
    break;
    }

    CopySprDToSpr(&ds, &font_sprt[chr]);

    ds.alphar = SCE_GS_SET_ALPHA_1(SCE_GS_ALPHA_CS, SCE_GS_ALPHA_ZERO, SCE_GS_ALPHA_AS, SCE_GS_ALPHA_CD, 0);
    ds.tex1 = SCE_GS_SET_TEX1_1(1, 0, SCE_GS_LINEAR, SCE_GS_LINEAR_MIPMAP_LINEAR, 0, 0, 0);

    DispSprD(&ds);

    if (CROSS_PRESSED() == 1 || *key_now[0xc] == 1)
    {
        title_wrk.mode = 9;

        SeStartFix(SE_CLIC, 0, 0x1000, 0x1000, 0);
    }
    else if (TRIANGLE_PRESSED() == 1)
    {
        title_wrk.mode = 9;

        SeStartFix(SE_CANCEL, 0, 0x1000, 0x1000, 0);
    }
    else if (
        DPAD_RIGHT_PRESSED() == 1 ||
        (DPAD_RIGHT_PRESSED() > 25 && (DPAD_RIGHT_PRESSED() % 5) == 1) ||
        Ana2PadDirCnt(1) == 1 ||
        (Ana2PadDirCnt(1) > 25 && (Ana2PadDirCnt(1) % 5) == 1)
    )
    {
        if (title_wrk.csr <= 1)
        {
            title_wrk.csr++;
        }
        else
        {
            title_wrk.csr = 0;
        }

        SeStartFix(SE_CSR0, 0, 0x1000, 0x1000, 0);
    }
    else if (
        DPAD_LEFT_PRESSED() == 1 ||
        (DPAD_LEFT_PRESSED() > 25 && (DPAD_LEFT_PRESSED() % 5) == 1) ||
        Ana2PadDirCnt(3) == 1 ||
        (Ana2PadDirCnt(3) > 25 && (Ana2PadDirCnt(3) % 5) == 1)
    )
    {
        if (title_wrk.csr > 0)
        {
            title_wrk.csr--;
        }
        else
        {
            title_wrk.csr = 0x2;
        }

        SeStartFix(SE_CSR0, 0, 0x1000, 0x1000, 0);
    }
}

void NewGameInit()
{
    if (ttl_dsp.mode != 0)
    {
        ingame_wrk.msn_no = 0;
    }

    sys_wrk.load = 0;

    title_wrk.csr = 0;

    cribo.costume = 0;
    cribo.clear_info = 0;

    mc_msn_flg = 0;

    NewgameMenuRankInit();
    CameraCustomNewgameInit();
    NewgameMenuGlstInit();
    ClearStageWrk();
}

void LoadGameInit()
{
    sys_wrk.load = 1;
    title_wrk.csr = 0;
    ingame_wrk.mode = INGAME_MODE_NOMAL;

    cribo.clear_info &= 0x4;
    cribo.costume = 0;

    LoadgameMenuRankInit();
}

void InitOutDither()
{
    out_dither.cnt = 0.0f;
    out_dither.spd = 8.0f;
    //out_dither.alp = 64.0f; // Looks way to visible unlike og hardware
    out_dither.alp = 20.0f;
    out_dither.alpmx = 0x40;
    out_dither.colmx = 0x40;
    out_dither.type = 7;
}

void MakeOutDither()
{
    u_char pat[0x4000];
    u_int pal[0x100];
    int i;
    static sceGsLoadImage gs_limage1;
    static sceGsLoadImage gs_limage2;

    SetVURand(MikuPan_Rand() / (float)MikuPan_RAND_MAX);

    for (i = 0; i < 0x4000; i++)
    {
        pat[i] = out_dither.alpmx * vu0Rand();
    }

    for (i = 0; i < 0x100; i++)
    {
        pal[i] = (u_char)(out_dither.colmx * vu0Rand());
        pal[i] = 0 \
            | i      << 24 \
            | pal[i] << 16 \
            | pal[i] <<  8 \
            | pal[i] <<  0;
    }

    sceGsSetDefLoadImage(&gs_limage1, 0x37fc, 2, SCE_GS_PSMT8, 0, 0, 128, 128);
    sceGsSetDefLoadImage(&gs_limage2, 0x383c, 1, SCE_GS_PSMCT32, 0, 0, 16, 32);

    FlushCache(0);

    sceGsExecLoadImage(&gs_limage1, (u_long128 *)pat);
    sceGsExecLoadImage(&gs_limage2, (u_long128 *)pal);
    
    sceGsSyncPath(0, 0);
}

void DispOutDither()
{
    	/* 0x0(sp) */ SPRITE_DATA sd2 = {
        .g_GsTex0 = {
              .TBP0 = 0x37FC,
              .TBW = 0x2,
              .PSM = 0x13,
              .TW = 0x7,
              .TH = 0x7,
              .TCC = 0x1,
              .TFX = 0x0,
              .CBP = 0x383C,
              .CPSM = 0x0,
              .CSM = 0x0,
              .CSA = 0x0,
              .CLD = 0x1,
        },
           .g_nTexSizeW = 0x80,
           .g_nTexSizeH = 0x80,
           .g_bMipmapLv = 0,
           .g_GsMiptbp1 = 0,
           .g_GsMiptbp2 = 0,
           .pos_x = -320.5f,
           .pos_y = -224.5f,
           .pos_z = 0xFFFFF000,
           .size_w = 640.0f,
           .size_h = 448.0f,
           .scale_w = 1.0f,
           .scale_h = 1.0f,
           .clamp_u = 0,
           .clamp_v = 0,
           .rot_center = 0,
           .angle = 0.0f,
           .r = 0x80,
           .g = 0x80,
           .b = 0x80,
           .alpha = 0x80,
           .mask = 0x0,
    };
	/* 0x60(sp) */ DRAW_ENV de2 = {
        .tex1 = SCE_GS_SET_TEX1_1(1, 0, SCE_GS_LINEAR, SCE_GS_LINEAR_MIPMAP_LINEAR, 0, 0, 0),
        .alpha = SCE_GS_SET_ALPHA_1(SCE_GS_ALPHA_CS, SCE_GS_ALPHA_CD, SCE_GS_ALPHA_AS, SCE_GS_ALPHA_CD, 0),
        .zbuf = SCE_GS_SET_ZBUF_1(0x8c, SCE_GS_PSMCT24, 1),
        .test = SCE_GS_SET_TEST_1(1, SCE_GS_ALPHA_GEQUAL, 0, SCE_GS_AFAIL_KEEP, 0, 0, 1, SCE_GS_DEPTH_GREATER),
        .clamp = SCE_GS_SET_CLAMP_1(SCE_GS_REPEAT, SCE_GS_REPEAT, 0, 0, 0, 0),
        .prim = SCE_GIF_SET_TAG(1, SCE_GS_TRUE, SCE_GS_TRUE, SCE_GS_SET_PRIM(SCE_GS_PRIM_SPRITE, 0, 1, 0, 1, 0, 1, 0, 0), SCE_GIF_PACKED, 5)
    };

    switch(out_dither.type)
    {
    case 1:
        de2.alpha = 0x44;
    break;
    case 2:
        de2.alpha = 0x48;
    break;
    case 3:
        de2.alpha = 0x41;
    break;
    case 4:
        sd2.r = 0x80;
        sd2.g = 0x0;
        sd2.b = 0x80;
        de2.alpha = 0x41;
    break;
    case 5:
        sd2.r = 0x80;
        sd2.g = 0x80;
        sd2.b = 0x0;
        de2.alpha = 0x41;
    break;
    case 6:
        de2.alpha = 0x49;
    break;
    case 7:
        de2.alpha = 0x42;
    break;
    }

    {
        float eff_hw, eff_hh;
        MikuPan_GetFullScreenHalfExtent(&eff_hw, &eff_hh);
        sd2.pos_x = -eff_hw - 0.5f;
        sd2.size_w = eff_hw * 2.0f;

        sd2.clamp_u = SCE_GS_SET_CLAMP_1(SCE_GS_REPEAT, SCE_GS_REPEAT, 0, 0, 40, 0);
        sd2.clamp_v = SCE_GS_SET_CLAMP_1(SCE_GS_REPEAT, SCE_GS_REPEAT, 0, 0, 32, 0);
        sd2.alpha = (u_char)((SgSinf(DEG2RAD(out_dither.cnt)) + 1.0f) * out_dither.alp);
        SetTexDirectS2(-2, &sd2, &de2, 0);

        sd2.clamp_u = SCE_GS_SET_CLAMP_1(SCE_GS_REPEAT, SCE_GS_REPEAT, 64, 0, 44, 0);
        sd2.clamp_v = SCE_GS_SET_CLAMP_1(SCE_GS_REPEAT, SCE_GS_REPEAT, 0, 0, 32, 0);
        sd2.alpha = (u_char)((SgSinf(DEG2RAD(out_dither.cnt + 120.0f)) + 1.0f) * out_dither.alp);
        SetTexDirectS2(-2, &sd2, &de2, 0);

        sd2.clamp_u = SCE_GS_SET_CLAMP_1(SCE_GS_REPEAT, SCE_GS_REPEAT, 0, 0, 40, 0);
        sd2.clamp_v = SCE_GS_SET_CLAMP_1(SCE_GS_REPEAT, SCE_GS_REPEAT, 64, 0, 36, 0);
        sd2.alpha = (u_char)((SgSinf(DEG2RAD(out_dither.cnt + 240.0f)) + 1.0f) * out_dither.alp);
        SetTexDirectS2(-2, &sd2, &de2, 0);
    }

    out_dither.cnt += out_dither.spd;
}

/*
int AlbmDesignLoad(u_char side, u_char type)
{
    void *addr;
    int load_id;

    if (side == 0)
    {
        addr = MikuPan_GetHostPointer(PL_FNDR_PK2_ADDRESS);
    }

    else if (side == 1)
    {
        addr = MikuPan_GetHostPointer(PL_ALBM_SIDE_1_ADDRESS);
    }

    switch(type)
    {
        case 0:
            load_id = LoadReqToHostPointer(PL_ALBM_BW_PK2, addr);
            break;
        case 1:
            load_id = LoadReqToHostPointer(PL_ALBM_BP_PK2, addr);
            break;
        case 2:
            load_id = LoadReqToHostPointer(PL_ALBM_BR_PK2, addr);
            break;
        case 3:
            load_id = LoadReqToHostPointer(PL_ALBM_BG_PK2, addr);
            break;
        case 4:
            load_id = LoadReqToHostPointer(PL_ALBM_BB_PK2, addr);
            break;
        case 5:
            load_id = LoadReqToHostPointer(PL_ALBM_BO_PK2, addr);
            break;
        default:
            load_id = -1;
            break;
    }

    return load_id;
}
*/

int AlbmDesignLoad(u_char side, u_char type)
{
    u_int addr;
    int load_id;

    if (side == 0)
    {
        addr = ALBUM_DESIGN_SIDE_0_ADDRESS;
    }

    else if (side == 1)
    {
        addr = ALBUM_DESIGN_SIDE_1_ADDRESS;
    }

    switch(type)
    {
        case 0:
#ifdef BUILD_EU_VERSION
            load_id = LoadReqLanguage(PL_ALBM_BW_E_PK2, addr);
#else
            load_id = LoadReq(PL_ALBM_BW_PK2, addr);
#endif
            break;
        case 1:
#ifdef BUILD_EU_VERSION
            load_id = LoadReqLanguage(PL_ALBM_BP_E_PK2, addr);
#else
            load_id = LoadReq(PL_ALBM_BP_PK2, addr);
#endif
            break;
        case 2:
#ifdef BUILD_EU_VERSION
            load_id = LoadReqLanguage(PL_ALBM_BR_E_PK2, addr);
#else
            load_id = LoadReq(PL_ALBM_BR_PK2, addr);
#endif
            break;
        case 3:
#ifdef BUILD_EU_VERSION
            load_id = LoadReqLanguage(PL_ALBM_BG_E_PK2, addr);
#else
            load_id = LoadReq(PL_ALBM_BG_PK2, addr);
#endif
            break;
        case 4:
#ifdef BUILD_EU_VERSION
            load_id = LoadReqLanguage(PL_ALBM_BB_E_PK2, addr);
#else
            load_id = LoadReq(PL_ALBM_BB_PK2, addr);
#endif
            break;
        case 5:
#ifdef BUILD_EU_VERSION
            load_id = LoadReqLanguage(PL_ALBM_BO_E_PK2, addr);
#else
            load_id = LoadReq(PL_ALBM_BO_PK2, addr);
#endif
            break;
        default:
            load_id = -1;
            break;
    }

    return load_id;
}

#ifdef BUILD_EU_VERSION
void InitTecmotLogo()
{
    title_sys.logo_flow = 0;
    title_sys.cnt = 0;
    title_sys.alp = 0;
}

int SetTecmoLogo() {
    DISP_SPRT ds;
    int sec1;
    int sec2;
    int sec3;
    int sec4;
    int n;

    sec1 = 60;
    sec2 = 90;
    sec3 = 60;
    sec4 = 30;

    switch (title_sys.logo_flow)
    {
    case 0:
        title_sys.cnt = 0;
        title_sys.alp = 0;
        title_sys.logo_flow = 1;
    case 1:
        title_sys.alp = title_sys.cnt * 128 / sec1;
        title_sys.cnt++;
        if (title_sys.cnt >= sec1)
        {
            title_sys.cnt = 0;
            title_sys.logo_flow = 2;
        }
    break;
    case 2:
        title_sys.alp = 128;
        title_sys.cnt++;
        if (title_sys.cnt >= sec2)
        {
            title_sys.cnt = 0;
            title_sys.logo_flow = 3;
        }
    break;
    case 3:
        title_sys.alp = (sec3 - title_sys.cnt) * 128 / sec3;
        title_sys.cnt++;
        if (title_sys.cnt >= sec3)
        {
            title_sys.logo_flow = 4;
            title_sys.cnt = 0;
        }
    break;
    case 4:
        title_sys.alp = 0;
        title_sys.cnt++;
        if (title_sys.cnt >= sec4) {
            title_sys.cnt = 0;
            title_sys.alp = 0;
            title_sys.logo_flow = 5;
        }
    break;
    case 5:
        title_sys.alp = title_sys.cnt * 128 / sec1;
        title_sys.cnt++;
        if (title_sys.cnt >= sec1)
        {
            title_sys.cnt = 0;
            title_sys.logo_flow = 6;
        }
    break;
    case 6:
        title_sys.alp = 128;
        title_sys.cnt++;
        if (title_sys.cnt >= sec2)
        {
            title_sys.cnt = 0;
            title_sys.logo_flow = 7;
        }
    break;
    case 7:
        title_sys.alp = (sec3 - title_sys.cnt) * 128 / sec3;
        title_sys.cnt++;
        if (title_sys.cnt >= sec3)
        {
            title_sys.cnt = 0;
            title_sys.logo_flow = 8;
        }
    break;
    case 8:
        return 1;
    default:
    break;
    }

    SetSprFile3(0x1e90000, 0);

    n = title_sys.logo_flow >= 0 && title_sys.logo_flow < 4 ? 0 : 7;

    CopySprDToSpr(&ds, &logotex[n]);


    ds.zbuf = SCE_GS_SET_ZBUF_1(0x8c, SCE_GS_PSMCT24, 1);
    ds.test = SCE_GS_SET_TEST_1(1, SCE_GS_ALPHA_GREATER, 0, SCE_GS_AFAIL_KEEP, 0, 0, 1, SCE_GS_DEPTH_GEQUAL);
    ds.pri = logotex[n].pri;
    ds.z = 0x0fffffff - logotex[n].pri;
    ds.x = logotex[n].x;
    ds.y = logotex[n].y;
    ds.r = 0x60;
    ds.g = 0x60;
    ds.b = 0x60;
    ds.alpha = title_sys.alp;

    DispSprD(&ds);

    return 0;
}

void InitSelectLanguage()
{
    title_sys.lang_sel_flow = 0;
    title_sys.cnt = 0;
    title_sys.alp = 0;
}

int SetSelectLanguage(int cur_pos)
{
    int i;
    DISP_SPRT ds;
    int n;

    switch(title_sys.lang_sel_flow)
    {
    case 0:
        title_sys.load_id = LoadReq(LOGO_PK2, 0x1e90000);
        title_sys.lang_sel_flow = 1;
    case 1:
        if (IsLoadEnd(title_sys.load_id) != 0)
        {
            title_sys.alp = 0;
            title_sys.cnt = 0;
            title_sys.lang_sel_flow = 2;
        }
    break;
    case 2:
        if (title_sys.alp + 0x10 < 0x80)
        {
            title_sys.alp += 0x10;
        }
        else
        {
            title_sys.alp = 0x80;
            title_sys.lang_sel_flow = 3;
        }
    break;
    case 3:
        title_sys.alp = 0x80;
    break;
    case 4:
        if (title_sys.alp - 0x10 > 0)
        {
            title_sys.alp -= 0x10;
        }
        else
        {
            if (IsLoadEnd(init_load_id) != 0)
            {
                title_sys.lang_sel_flow = 5;
                title_sys.alp = 0;
            }
        }
    break;
    case 5:
        // do nothing ...
    break;
    }

    if (title_sys.lang_sel_flow > 1)
    {
        if (title_sys.lang_sel_flow == 3)
        {
            if (*key_now[1] == 1)
            {
                SeStartFix(0, 0, 0x1000, 0x1000, 0);

                if (sys_wrk.language < 4)
                {
                    sys_wrk.language++;
                }
                else
                {
                    sys_wrk.language = 0;
                }
            }
            else if (*key_now[0] == 1)
            {
                SeStartFix(0, 0, 0x1000, 0x1000, 0);

                if (sys_wrk.language > 0)
                {
                    sys_wrk.language--;
                }
                else
                {
                    sys_wrk.language = 4;
                }
            }
            else if (*key_now[5] == 1)
            {
                SeStartFix(1, 0, 0x1000, 0x1000, 0);

                title_sys.lang_sel_flow = 4;

                mc_language = mc_language | sys_wrk.language;

                init_load_id = LoadReqLanguage(IG_MSG_E_OBJ, 0x84a000);
            }
        }

        SetSprFile3(0x1e90000, 0);

        n = 1;

        CopySprDToSpr(&ds, &logotex[n]);

        ds.zbuf = SCE_GS_SET_ZBUF_1(0x8c, SCE_GS_PSMCT24, 1);
        ds.test = SCE_GS_SET_TEST_1(1, SCE_GS_ALPHA_GREATER, 0, SCE_GS_AFAIL_KEEP, 0, 0, 1, SCE_GS_DEPTH_GEQUAL);
        ds.pri = logotex[n].pri;
        ds.z = 0x0fffffff - logotex[n].pri;
        ds.x = logotex[n].x;
        ds.y = logotex[n].y;
        ds.r = 0x80;
        ds.g = 0x80;
        ds.b = 0x80;
        ds.alpha = title_sys.alp;

        DispSprD(&ds);

        for (i = 0; i < 5; i++)
        {
            n = i + 2;

            CopySprDToSpr(&ds, &logotex[n]);

            ds.tex1 = SCE_GS_SET_TEX1_1(1, 0, SCE_GS_LINEAR, SCE_GS_LINEAR_MIPMAP_LINEAR, 0, 0, 0);
            ds.x = 0x140 - (int)(logotex[n].w / 2);
            ds.y = 0x20 + (i) * 0x50;

            if (cur_pos == i)
            {
                ds.r = 0x80;
                ds.g = 0x80;
                ds.b = 0x80;
            }
            else
            {
                ds.r = 0x20;
                ds.g = 0x20;
                ds.b = 0x20;
            }

            ds.alpha = title_sys.alp;

            DispSprD(&ds);
        }
    }

    return title_sys.lang_sel_flow == 5;
}
#endif
