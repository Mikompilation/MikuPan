#include "mikupan/gameplay/mikupan_title_scene.h"

#include "common.h"
#include "outgame/title.h"

#include "data/load_door_num.h"
#include "data/load_furn_num.h"
#include "data/room_name.h"
#include "enums.h"
#include "graphics/graph2d/effect_scr.h"
#include "graphics/graph2d/effect_obj.h"
#include "graphics/graph2d/effect_sub2.h"
#include "graphics/graph2d/g2d_debug.h"
#include "graphics/graph3d/gra3d.h"
#include "graphics/graph3d/sgcam.h"
#include "graphics/graph3d/sgdma.h"
#include "graphics/graph3d/sglib.h"
#include "graphics/motion/accessory.h"
#include "graphics/motion/mime.h"
#include "graphics/scene/fod.h"
#include "ingame/ig_glob.h"
#include "main/glob.h"
#include "ingame/map/door_ctl.h"
#include "ingame/map/furn_ctl.h"
#include "ingame/map/furn_eff.h"
#include "ingame/map/furn_spe/furn_spe.h"
#include "ingame/map/map_ctrl.h"
#include "mikupan/gs/mikupan_gs_c.h"
#include "mikupan/mikupan_config.h"
#include "mikupan/mikupan_memory.h"
#include "mikupan/mikupan_rng.h"
#include "mikupan/mikupan_utils.h"
#include "mikupan/rendering/mikupan_renderer.h"
#include "mikupan/ui/mikupan_ui.h"
#include "mikupan/ui/mikupan_ui_theme.h"
#include "os/eeiop/adpcm/ea_cmd.h"
#include "os/eeiop/adpcm/ea_ctrl.h"
#include "os/eeiop/adpcm/ea_dat.h"
#include "os/eeiop/cdvd/eecdvd.h"
#include "outgame/outgame.h"

#define CIMGUI_DEFINE_ENUMS_AND_STRUCTS
#include "cimgui.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

extern FOD_EFF_PARAM eff_param;

void MikuPan_TitleResetSceneColourState(void)
{
    eff_param.mono_flg = 0;
    ChangeMonochrome(0);
}

#define PI 3.1415927f
#define DEG2RAD(x) ((float)(x)*PI/180.0f)

#define TITLE_BG_MSN_NO 0
#define TITLE_BG_MSN_MAX 4
#define TITLE_BG_ROOM_NO R000_GENKAN
#define TITLE_BG_ROOM_BLOCK 0
#define TITLE_BG_ROOM_MAX 63
#define TITLE_BG_MODEL_LOAD_MAX 64
/* Frames to wait for the room background to load before giving up, so a stuck
 * CDVD load can never leave the title on an infinite black screen. */
#define TITLE_BG_LOAD_TIMEOUT (60 * 8)
#define TITLE_BG_TRANSITION_FADE_FRAMES 120
#define TITLE_BG_TRANSITION_LEAD_FRAMES 180
#define TITLE_BG_INITIAL_FADE_FRAMES 60
#define TITLE_BG_FADE_ALPHA_MAX 0x80
#define TITLE_BGM_FADE_OUT_FRAMES 60
#define TITLE_BGM_FADE_IN_FRAMES 60
#define TITLE_BGM_DEFAULT_FILE_NO AB013_STR
#define TITLE_AUDIO_FILE_MAX RSHADE_SGD

typedef enum {
    TITLE_BG_ROOM_UNLOADED = 0,
    TITLE_BG_ROOM_LOADING_EFFECT, /* EFF001 texture bank for room effects */
    TITLE_BG_ROOM_LOADING,      /* room model (PK2 + LIT) only */
    TITLE_BG_ROOM_LOADING_MAP,  /* mission map data (for furniture placement) */
    TITLE_BG_ROOM_LOADING_FURN, /* furniture + door models, one id at a time */
    TITLE_BG_ROOM_READY,
} TITLE_BG_ROOM_STATE;

typedef enum {
    TITLE_BG_FADE_IDLE = 0,
    TITLE_BG_FADE_OUT,
    TITLE_BG_FADE_WAIT_READY,
    TITLE_BG_FADE_IN,
} TITLE_BG_FADE_STATE;

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
        .msn_no = 0,
        .room_no = 0,
        .audio_file_no = 1540,
        .cycle_seconds = 20,
        .camera = {
            .camera_p = {2853.2f, -526.0f, 1363.5f},
            .camera_i = {2763.2f, -550.0f, 1688.5f},
            .fov_deg = 44.0f,
        },
        .lerp_enabled = 1,
        .lerp_seconds = 20.0f,
        .lerp_t = 0.0f,
        .lerp_a = {
            .camera_p = {2986.7f, -526.0f, 881.6f},
            .camera_i = {2896.7f, -550.0f, 1206.6f},
            .fov_deg = 44.0f,
        },
        .lerp_b = {
            .camera_p = {2853.2f, -526.0f, 1363.5f},
            .camera_i = {2763.2f, -550.0f, 1688.5f},
            .fov_deg = 44.0f,
        },
    },
    {
        .msn_no = 0,
        .room_no = 2,
        .audio_file_no = 1540,
        .cycle_seconds = 50,
        .camera = {
            .camera_p = {500.0f, -495.9f, 2246.1f},
            .camera_i = {500.0f, -750.0f, 3868.2f},
            .fov_deg = 44.0f,
        },
        .lerp_enabled = 1,
        .lerp_seconds = 50.0f,
        .lerp_t = 0.0f,
        .lerp_a = {
            .camera_p = {500.0f, -495.9f, -18.1f},
            .camera_i = {500.0f, -750.0f, 1604.0f},
            .fov_deg = 44.0f,
        },
        .lerp_b = {
            .camera_p = {500.0f, -495.9f, 6481.9f},
            .camera_i = {500.0f, -750.0f, 8104.0f},
            .fov_deg = 44.0f,
        },
    },
    {
        .msn_no = 0,
        .room_no = 10,
        .audio_file_no = 1540,
        .cycle_seconds = 50,
        .camera = {
            .camera_p = {1386.4f, -457.7f, 3307.6f},
            .camera_i = {1903.9f, -11.9f, 1185.3f},
            .fov_deg = 44.0f,
        },
        .lerp_enabled = 1,
        .lerp_seconds = 50.0f,
        .lerp_t = 0.0f,
        .lerp_a = {
            .camera_p = {2649.6f, -457.7f, 3314.4f},
            .camera_i = {920.9f, -11.9f, 1421.5f},
            .fov_deg = 44.0f,
        },
        .lerp_b = {
            .camera_p = {880.4f, -457.7f, 3304.9f},
            .camera_i = {2297.6f, -11.9f, 1090.7f},
            .fov_deg = 44.0f,
        },
    },
    {
        .msn_no = 0,
        .room_no = 25,
        .audio_file_no = 1540,
        .cycle_seconds = 40,
        .camera = {
            .camera_p = {5210.0f, -1324.0f, 3939.8f},
            .camera_i = {5210.0f, -2232.0f, 6823.8f},
            .fov_deg = 44.0f,
        },
        .lerp_enabled = 1,
        .lerp_seconds = 40.0f,
        .lerp_t = 0.0f,
        .lerp_a = {
            .camera_p = {5210.0f, -803.8f, 2090.5f},
            .camera_i = {5210.0f, -1711.9f, 4974.5f},
            .fov_deg = 44.0f,
        },
        .lerp_b = {
            .camera_p = {5210.0f, -1703.8f, 5290.5f},
            .camera_i = {5210.0f, -2611.9f, 8174.5f},
            .fov_deg = 44.0f,
        },
    },
};
#define TITLE_BG_PRESET_COUNT ((int)(sizeof(title_bg_presets) / sizeof(title_bg_presets[0])))

static TITLE_BG_ROOM_STATE title_bg_room_state = TITLE_BG_ROOM_UNLOADED;
static SgCAMERA title_bg_camera;
static int title_bg_msn_no = TITLE_BG_MSN_NO;
static int title_bg_room_no = TITLE_BG_ROOM_NO;
static float title_bg_camera_p[3] = {2986.7f, -526.0f, 881.6f};
static float title_bg_camera_i[3] = {2896.7f, -550.0f, 1206.6f};
static float title_bg_camera_fov_deg = 44.0f;
static float title_bg_camera_move_step = 100.0f;
static int title_bg_auto_cycle = 1;
static int title_bg_auto_cycle_seconds = 20;
static int title_bg_auto_cycle_timer = 0;
static int title_bg_preset_index = 0;
static TITLE_BG_CAMERA_POINT title_bg_camera_lerp_a = {
    .camera_p = {2986.7f, -526.0f, 881.6f},
    .camera_i = {2896.7f, -550.0f, 1206.6f},
    .fov_deg = 44.0f,
};
static TITLE_BG_CAMERA_POINT title_bg_camera_lerp_b = {
    .camera_p = {2853.2f, -526.0f, 1363.5f},
    .camera_i = {2763.2f, -550.0f, 1688.5f},
    .fov_deg = 44.0f,
};
static float title_bg_camera_lerp_t = 0.0f;
static float title_bg_camera_lerp_seconds = 20.0f;
static int title_bg_camera_lerp_enabled = 1;
static int title_bg_camera_lerp_timer = 0;
static int title_debug_window_visible = 0;
static int title_bgm_file_no = 1540;
static int title_bgm_playing_file_no = -1;
static int title_bg_effect_load_id = -1;
static int title_bg_map_load_id = -1;
static int title_bg_floor = 0;
static int title_bg_map_room_no = 0xff;
static int title_bg_room_world_pos_valid = 0;
static sceVu0FVECTOR title_bg_room_world_pos = {0.0f, 0.0f, 0.0f, 1.0f};
static u_int *title_bg_furn_load_addr = NULL;
static int title_bg_model_load_id[TITLE_BG_MODEL_LOAD_MAX];
static int title_bg_model_load_num = 0;
static int title_bg_load_timer = 0;
static int title_bg_load_failed = 0;
static TITLE_BG_FADE_STATE title_bg_fade_state = TITLE_BG_FADE_IDLE;
static int title_bg_fade_timer = 0;
static int title_bg_fade_alpha = 0;
static int title_bg_fade_frame_count = TITLE_BG_TRANSITION_FADE_FRAMES;
static int title_bg_pending_preset_index = -1;
static int title_bg_pending_msn_no = -1;
static int title_bg_pending_room_no = -1;

int MikuPan_TitleUseRoomBackground(void)
{
    /* A load that never completes must not hang the title; once the watchdog
     * gives up we behave as if the room background were off (classic 2D title). */
    if (title_bg_load_failed)
    {
        return 0;
    }

    return mikupan_configuration.title_room_background != 0;
}

int MikuPan_TitleUseDither(void)
{
    return mikupan_configuration.title_dither != 0;
}

int MikuPan_TitleDebugWindowVisible(void)
{
    return title_debug_window_visible != 0;
}

void MikuPan_TitleSetDebugWindowVisible(int enabled)
{
    title_debug_window_visible = enabled != 0;
}

static void TitleBgSetVector(sceVu0FVECTOR vector, float x, float y, float z, float w)
{
    vector[0] = x;
    vector[1] = y;
    vector[2] = z;
    vector[3] = w;
}

static float TitleBgLerpFloat(float a, float b, float t)
{
    return a + (b - a) * t;
}

static int TitleBgFadeFrameCount(void)
{
    return MikuPan_ClampInt(title_bg_fade_frame_count, 1, 240);
}

static void TitleBgBeginFadeOut(void)
{
    title_bg_fade_frame_count = TITLE_BG_TRANSITION_FADE_FRAMES;
    title_bg_fade_state = TITLE_BG_FADE_OUT;
    title_bg_fade_timer = 0;
    title_bg_fade_alpha = 0;
}

static void TitleBgBeginInitialFadeIn(void)
{
    title_bg_fade_frame_count = TITLE_BG_INITIAL_FADE_FRAMES;
    title_bg_fade_state = TITLE_BG_FADE_WAIT_READY;
    title_bg_fade_timer = 0;
    title_bg_fade_alpha = TITLE_BG_FADE_ALPHA_MAX;
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
    case TITLE_BG_ROOM_LOADING_EFFECT:
        return "loading effects";
    case TITLE_BG_ROOM_LOADING:
        return "loading model";
    case TITLE_BG_ROOM_LOADING_MAP:
        return "loading map";
    case TITLE_BG_ROOM_LOADING_FURN:
        return "loading furniture";
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
    if (MikuPan_TitleUseRoomBackground() == 0)
    {
        return AO000_TITLE_STR;
    }

    return TitleAudioFileNo();
}

void MikuPan_TitleAudioPlayBgm(u_short fade_in_frames)
{
    int file_no = TitleAudioActiveFileNo();

    EAdpcmCmdPlay(0, 1, file_no, 0, GetAdpcmVol(file_no), 0x280, 0xfff, fade_in_frames);
    title_bgm_playing_file_no = file_no;
}

static void TitleAudioRestartBgm(void)
{
    EAdpcmCmdStop(0, 0, TITLE_BGM_FADE_OUT_FRAMES);
    MikuPan_TitleAudioPlayBgm(TITLE_BGM_FADE_IN_FRAMES);
}

static void TitleAudioStopBgm(void)
{
    EAdpcmCmdStop(0, 0, 0);
    title_bgm_playing_file_no = -1;
}

static void TitleBgResetFade(void)
{
    title_bg_fade_state = TITLE_BG_FADE_IDLE;
    title_bg_fade_timer = 0;
    title_bg_fade_alpha = 0;
    title_bg_fade_frame_count = TITLE_BG_TRANSITION_FADE_FRAMES;
    title_bg_pending_preset_index = -1;
    title_bg_pending_msn_no = -1;
    title_bg_pending_room_no = -1;
}

void MikuPan_TitleSetUseRoomBackground(int enabled)
{
    int use_room_background = enabled != 0;

    if (mikupan_configuration.title_room_background == use_room_background)
    {
        return;
    }

    mikupan_configuration.title_room_background = use_room_background;
    TitleBgResetFade();

    if (title_bgm_playing_file_no != -1)
    {
        TitleAudioRestartBgm();
    }
}

void MikuPan_TitleSetUseDither(int enabled)
{
    mikupan_configuration.title_dither = enabled != 0;
}

static void TitleBgRequestRoomReload(void)
{
    title_bg_room_state = TITLE_BG_ROOM_UNLOADED;
    title_bg_effect_load_id = -1;
    title_bg_map_load_id = -1;
    title_bg_map_room_no = 0xff;
    title_bg_room_world_pos_valid = 0;
    TitleBgSetVector(title_bg_room_world_pos, 0.0f, 0.0f, 0.0f, 1.0f);
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
    float t = MikuPan_ClampFloat(title_bg_camera_lerp_t, 0.0f, 1.0f);

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

    title_bg_camera_lerp_seconds = MikuPan_ClampFloat(title_bg_camera_lerp_seconds, 0.1f, 120.0f);
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

    title_bg_camera_lerp_t = MikuPan_ClampFloat(title_bg_camera_lerp_t, 0.0f, 1.0f);
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

static void TitleBgApplyPresetNow(int preset_index)
{
    const TITLE_BG_PRESET *preset;
    int reload_room;

    preset_index = MikuPan_ClampInt(preset_index, 0, TITLE_BG_PRESET_COUNT - 1);
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

static void TitleBgApplyRoomNow(int msn_no, int room_no)
{
    msn_no = MikuPan_ClampInt(msn_no, 0, TITLE_BG_MSN_MAX);
    room_no = MikuPan_ClampInt(room_no, 0, TITLE_BG_ROOM_MAX);

    if (title_bg_msn_no == msn_no && title_bg_room_no == room_no)
    {
        return;
    }

    title_bg_msn_no = msn_no;
    title_bg_room_no = room_no;
    title_bg_auto_cycle_timer = 0;
    TitleBgPauseCameraAnimations();
    TitleBgRequestRoomReload();
}

static int TitleBgShouldFadeRoomChange(int msn_no, int room_no)
{
    if (MikuPan_TitleUseRoomBackground() == 0)
    {
        return 0;
    }

    if (title_bg_room_state != TITLE_BG_ROOM_READY)
    {
        return 0;
    }

    if (title_bg_msn_no == msn_no && title_bg_room_no == room_no)
    {
        return 0;
    }

    return 1;
}

static void TitleBgBeginPresetTransition(int preset_index)
{
    const TITLE_BG_PRESET *preset;

    preset_index = MikuPan_ClampInt(preset_index, 0, TITLE_BG_PRESET_COUNT - 1);
    preset = &title_bg_presets[preset_index];

    if (TitleBgShouldFadeRoomChange(preset->msn_no, preset->room_no) == 0)
    {
        TitleBgApplyPresetNow(preset_index);
        return;
    }

    title_bg_pending_preset_index = preset_index;
    title_bg_pending_msn_no = -1;
    title_bg_pending_room_no = -1;
    TitleBgBeginFadeOut();

    if (title_bgm_playing_file_no != -1 && title_bgm_playing_file_no != preset->audio_file_no)
    {
        title_bgm_file_no = preset->audio_file_no;
        TitleAudioRestartBgm();
    }
}

static void TitleBgBeginRoomTransition(int msn_no, int room_no)
{
    msn_no = MikuPan_ClampInt(msn_no, 0, TITLE_BG_MSN_MAX);
    room_no = MikuPan_ClampInt(room_no, 0, TITLE_BG_ROOM_MAX);

    if (TitleBgShouldFadeRoomChange(msn_no, room_no) == 0)
    {
        TitleBgApplyRoomNow(msn_no, room_no);
        return;
    }

    title_bg_pending_preset_index = -1;
    title_bg_pending_msn_no = msn_no;
    title_bg_pending_room_no = room_no;
    TitleBgBeginFadeOut();
    TitleBgPauseCameraAnimations();
}

static void TitleBgApplyPreset(int preset_index)
{
    TitleBgBeginPresetTransition(preset_index);
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

static int TitleBgAutoCycleLeadFrameCount(void)
{
    return MikuPan_ClampInt(TITLE_BG_TRANSITION_LEAD_FRAMES, 1, 240);
}

static int TitleBgAutoCycleShouldAdvance(void)
{
    int cycle_frames;
    int cycle_trigger_frames;
    int lead_frames = TitleBgAutoCycleLeadFrameCount();

    title_bg_auto_cycle_seconds = MikuPan_ClampInt(title_bg_auto_cycle_seconds, 1, 120);
    cycle_frames = title_bg_auto_cycle_seconds * 60;
    cycle_trigger_frames = MikuPan_ClampInt(cycle_frames - lead_frames, 1, cycle_frames);

    if (title_bg_auto_cycle_timer >= cycle_trigger_frames)
    {
        return 1;
    }

    if (title_bg_camera_lerp_enabled != 0)
    {
        int lerp_frames = TitleBgCameraLerpFrameCount();
        int lerp_trigger_frames = MikuPan_ClampInt(lerp_frames - lead_frames, 1, lerp_frames);

        if (title_bg_camera_lerp_timer >= lerp_trigger_frames)
        {
            return 1;
        }
    }

    return 0;
}

static void TitleBgAutoCycleUpdate(void)
{
    if (title_bg_auto_cycle == 0 || title_bg_room_state != TITLE_BG_ROOM_READY
        || title_bg_fade_state != TITLE_BG_FADE_IDLE)
    {
        return;
    }

    title_bg_auto_cycle_timer++;

    if (TitleBgAutoCycleShouldAdvance() != 0)
    {
        TitleBgApplyNextPreset();
    }
}

static void TitleBgSetMissionNo(int msn_no)
{
    TitleBgBeginRoomTransition(msn_no, title_bg_room_no);
}

static void TitleBgSetRoomNo(int room_no)
{
    TitleBgBeginRoomTransition(title_bg_msn_no, room_no);
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

static void TitleBgSetupRoomState(void)
{
    room_wrk.room_no = title_bg_map_room_no;
    room_wrk.disp_no[0] = title_bg_room_no;
    room_wrk.disp_no[1] = 0xff;
    TitleBgSetVector(room_wrk.pos[0], 0.0f, 0.0f, 0.0f, 1.0f);
    TitleBgSetVector(room_wrk.pos[1], 0.0f, 0.0f, 0.0f, 1.0f);
}

static void TitleBgUpdateRoomWorldPos(void)
{
    title_bg_room_world_pos_valid = 0;
    TitleBgSetVector(title_bg_room_world_pos, 0.0f, 0.0f, 0.0f, 1.0f);

    if (title_bg_map_room_no == 0xff)
    {
        return;
    }

    if (GetRoomPos((u_char)title_bg_room_no, title_bg_room_world_pos) == 0)
    {
        title_bg_room_world_pos[3] = 1.0f;
        title_bg_room_world_pos_valid = 1;
    }
}

static void TitleBgApplyRoomWorldOffset(sceVu0FVECTOR pos)
{
    pos[0] -= title_bg_room_world_pos[0];
    pos[1] -= title_bg_room_world_pos[1];
    pos[2] -= title_bg_room_world_pos[2];
}

static void TitleBgLocalizeFurnitureWork(void)
{
    int i;

    if (title_bg_room_world_pos_valid == 0)
    {
        return;
    }

    for (i = 0; i < 60; i++)
    {
        FURN_WRK *fwp = &furn_wrk[i];
        int had_effect;

        if (fwp->use == 5 || fwp->room_id != title_bg_room_no)
        {
            continue;
        }

        had_effect = fwp->fewrk_no != 0xff;

        if (had_effect != 0)
        {
            FurnEfctFree(fwp);
        }

        TitleBgApplyRoomWorldOffset(fwp->pos);

        if (had_effect != 0)
        {
            FurnEfctSet(fwp);
        }
    }
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

    TitleBgUpdateRoomWorldPos();
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
    room_wrk = ROOM_WRK{0};

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
        TitleBgLocalizeFurnitureWork();
        FurnSortFurnWrk(0);
    }

    TitleBgSetupRoomState();
    ingame_wrk.msn_no = old_msn_no;
}

static int TitleBgFurnitureEntryIsDrawable(const FURN_WRK *fwp)
{
    return fwp->use != 5 && fwp->furn_no != 0xffff;
}

static int TitleBgCountDrawableFurnitureWork(void)
{
    int i;
    int count = 0;

    for (i = 0; i < 60; i++)
    {
        if (TitleBgFurnitureEntryIsDrawable(&furn_wrk[i]) != 0 &&
            furn_wrk[i].furn_no < 1000)
        {
            count++;
        }
    }

    return count;
}

static int TitleBgCountDrawableDoorWork(void)
{
    int i;
    int count = 0;

    for (i = 0; i < 60; i++)
    {
        if (TitleBgFurnitureEntryIsDrawable(&furn_wrk[i]) != 0 &&
            furn_wrk[i].furn_no >= 1000)
        {
            count++;
        }
    }

    return count;
}

static void TitleBgPreRenderFurnitureWork(void)
{
    int i;

    for (i = 0; i < 60; i++)
    {
        if (TitleBgFurnitureEntryIsDrawable(&furn_wrk[i]) != 0)
        {
            ChoudoPreRender(&furn_wrk[i]);
        }
    }
}

static int TitleBgModelCanLoad(u_short model_id, const short *model_ids)
{
    while (*model_ids != -1)
    {
        if (model_id == (u_short)*model_ids)
        {
            return 1;
        }

        model_ids++;
    }

    return 0;
}

static int TitleBgQueueModelLoad(
    int file_no, u_int **load_addr, u_int **model_addr, int *load_id)
{
    int id = -1;
    u_int *addr = *load_addr;
    int64_t next_addr = LoadReqGetHostPointerEnd(file_no, *load_addr, &id);

    if (id == -1)
    {
        return 0;
    }

    *load_id = id;
    *model_addr = addr;
    *load_addr = (u_int *)(uintptr_t)next_addr;

    return 1;
}

static void TitleBgQueueFurnitureModels(ROOM_LOAD_BLOCK *rlb)
{
    u_int *load_addr = title_bg_furn_load_addr;
    u_short model_id;
    int i;
    int queue_failed = 0;

    title_bg_model_load_num = 0;

    for (i = 0; i < rlb->furn_num && title_bg_model_load_num < TITLE_BG_MODEL_LOAD_MAX &&
         queue_failed == 0; i++)
    {
        model_id = rlb->furn_id[i];

        if (TitleBgModelCanLoad(model_id, load_furn_num) == 0)
        {
            continue;
        }

        if (TitleBgQueueModelLoad(
            F000_CLOCK_L_SGD + model_id,
            &load_addr,
            &furn_addr_tbl[model_id],
            &title_bg_model_load_id[title_bg_model_load_num]) == 0)
        {
            queue_failed = 1;
            continue;
        }

        title_bg_model_load_num++;
    }

    for (i = 0; i < rlb->door_num && title_bg_model_load_num < TITLE_BG_MODEL_LOAD_MAX &&
         queue_failed == 0; i++)
    {
        model_id = rlb->door_id[i];

        if (TitleBgModelCanLoad(model_id, load_door_num) == 0)
        {
            continue;
        }

        if (TitleBgQueueModelLoad(
            D000_GEN1_SGD + model_id,
            &load_addr,
            &door_addr_tbl[model_id],
            &title_bg_model_load_id[title_bg_model_load_num]) == 0)
        {
            queue_failed = 1;
            continue;
        }

        rlb->door_addr[i] = door_addr_tbl[model_id];
        title_bg_model_load_num++;
    }

    rlb->load_addr = load_addr;
}

static int TitleBgFurnitureModelsReady(void)
{
    if (title_bg_model_load_num <= 0)
    {
        return 1;
    }

    return IsLoadEnd(title_bg_model_load_id[title_bg_model_load_num - 1]) != 0;
}

static void TitleBgMapFurnitureModels(ROOM_LOAD_BLOCK *rlb)
{
    u_short model_id;
    int i;

    for (i = 0; i < rlb->furn_num; i++)
    {
        model_id = rlb->furn_id[i];

        if (furn_addr_tbl[model_id] != NULL)
        {
            SgMapUnit(furn_addr_tbl[model_id]);
        }
    }

    for (i = 0; i < rlb->door_num; i++)
    {
        model_id = rlb->door_id[i];

        if (door_addr_tbl[model_id] != NULL)
        {
            SgMapUnit(door_addr_tbl[model_id]);
        }
    }
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

static void TitleBgBeginRoomModelLoad(void)
{
    /*
     * Load the room model on its own, before any map data exists. RoomMdlLoadReq
     * calls GetRoomFurnID/GetRoomDoorID against MAP_DATA_ADDRESS; with the map
     * blob cleared they return 0, so only the PK2 + LIT get queued. RoomMdlLoadWait
     * then keys off the LIT id, which the IOP transfers *after* the whole 1.5MB PK2
     * (FIFO), so InitializeRoom never walks a half-transferred model. Bundling the
     * furniture loads here is exactly what made RoomMdlLoadWait finish on a
     * furniture id before the PK2 DMA landed (the zeroed-pk2 / far_sgd crash).
     */
    memset(MikuPan_GetHostPointer(MAP_DATA_ADDRESS), 0, 0x80);

    TitleBgSetupRoomState();
    title_bg_furn_load_addr =
        RoomMdlLoadReq(NULL, TITLE_BG_ROOM_BLOCK, title_bg_msn_no, title_bg_room_no, 0);
    title_bg_room_state = TITLE_BG_ROOM_LOADING;
}

void MikuPan_TitleBackgroundBeginRoomLoad(void)
{
    InitModelLoad();

    if (title_bg_fade_state == TITLE_BG_FADE_IDLE)
    {
        TitleBgBeginInitialFadeIn();
    }

    title_bg_load_timer = 0;
    title_bg_load_failed = 0;
    title_bg_model_load_num = 0;

    title_bg_effect_load_id = LoadReq(EFF001_PK2, EFFECT_ADDRESS);
    title_bg_room_state = TITLE_BG_ROOM_LOADING_EFFECT;
}

static int TitleBgRoomMdlLoadWait(void)
{
    int old_msn_no = ingame_wrk.msn_no;
    int ready;

    ingame_wrk.msn_no = title_bg_msn_no;
    ready = RoomMdlLoadWait();
    ingame_wrk.msn_no = old_msn_no;

    return ready;
}

static void TitleBgUpdateRoomLoad(void)
{
    ROOM_LOAD_BLOCK *rlb = &room_load_block[TITLE_BG_ROOM_BLOCK];

    if (title_bg_room_state == TITLE_BG_ROOM_UNLOADED)
    {
        MikuPan_TitleBackgroundBeginRoomLoad();
    }

    /*
     * Watchdog: if any load stage stalls (e.g. a CDVD load id that never reports
     * complete after returning to the title), recover instead of hanging forever
     * on a black screen.
     */
    if (title_bg_room_state != TITLE_BG_ROOM_READY)
    {
        if (++title_bg_load_timer > TITLE_BG_LOAD_TIMEOUT)
        {
            title_bg_load_timer = 0;

            if (title_bg_room_state == TITLE_BG_ROOM_LOADING_FURN)
            {
                /* Room model + map are up and the furniture work tables are built,
                 * so reveal the room even if some furniture models are still stuck. */
                title_bg_room_state = TITLE_BG_ROOM_READY;
            }
            else
            {
                /* The room model or map never arrived; fall back to the classic
                 * 2D title rather than hang on a black screen. */
                title_bg_load_failed = 1;
            }

            return;
        }
    }

    /* Stage 0: room effects. Pond/water in room 22 uses EFF001 textures, so bind
     * them before the gra3d background renderer starts calling room effects. */
    if (title_bg_room_state == TITLE_BG_ROOM_LOADING_EFFECT)
    {
        if (title_bg_effect_load_id >= 0 && IsLoadEnd(title_bg_effect_load_id) == 0)
        {
            return;
        }

        SetETIM2File(EFFECT_ADDRESS);
        InitEffects();
        InitEffectSub2();
        title_bg_load_timer = 0;
        TitleBgBeginRoomModelLoad();
    }

    /* Stage 1: room model. RoomMdlLoadWait runs InitializeRoom only once the PK2
     * is fully present, because no furniture loads were queued behind it. */
    if (title_bg_room_state == TITLE_BG_ROOM_LOADING)
    {
        if (TitleBgRoomMdlLoadWait() == 0)
        {
            return;
        }

        /*
         * MissonMapDataLoad() hardcodes MSN01MAP_OBJ, so it would leave mission
         * 1's map at MAP_DATA_ADDRESS while everything here treats it as mission
         * title_bg_msn_no (==3). InitFurnAttrFlg / GetRoomFurnID then walk
         * floor_exist[3]'s four floors over a two-floor blob and read garbage.
         * Load the matching mission map directly, the way scn_test does.
         */
        title_bg_map_load_id = LoadReq(MSN00MAP_OBJ + title_bg_msn_no, MAP_DATA_ADDRESS);
        title_bg_room_state = TITLE_BG_ROOM_LOADING_MAP;
    }

    /* Stage 2: mission map data, needed to resolve which furniture/doors live in
     * this room. Then build the work tables and the model id lists. */
    if (title_bg_room_state == TITLE_BG_ROOM_LOADING_MAP)
    {
        if (title_bg_map_load_id >= 0 && IsLoadEnd(title_bg_map_load_id) == 0)
        {
            return;
        }

        InitialDmaBuffer();
        TitleBgSetupFurnitureWork();

        rlb->furn_num = GetRoomFurnID(title_bg_room_no, rlb->furn_id, title_bg_msn_no);
        DelSameMdlID(rlb->furn_id, (int *)&rlb->furn_num);
        rlb->door_num = GetRoomDoorID(title_bg_room_no, rlb->door_id, title_bg_msn_no);
        TitleBgQueueFurnitureModels(rlb);
        title_bg_room_state = TITLE_BG_ROOM_LOADING_FURN;
    }

    /* Stage 3: furniture + door models. The title loader owns this queue so room
     * switches cannot inherit LoadInitFurnModel/LoadInitDoorModel static state. */
    if (title_bg_room_state == TITLE_BG_ROOM_LOADING_FURN)
    {
        if (TitleBgFurnitureModelsReady() == 0)
        {
            return;
        }

        TitleBgMapFurnitureModels(rlb);

        /*
         * FurnDataInit / DoorDataInit (run back in stage 2) pre-render while the
         * model address tables are still NULL, so redo it after mapping the SGDs.
         */
        TitleBgPreRenderFurnitureWork();

        title_bg_room_state = TITLE_BG_ROOM_READY;
    }
}

int MikuPan_TitleBackgroundRoomReady(void)
{
    TitleBgUpdateRoomLoad();

    return title_bg_room_state == TITLE_BG_ROOM_READY;
}

static void TitleBgApplyPendingFadeTarget(void)
{
    if (title_bg_pending_preset_index >= 0)
    {
        TitleBgApplyPresetNow(title_bg_pending_preset_index);
    }
    else if (title_bg_pending_msn_no >= 0 && title_bg_pending_room_no >= 0)
    {
        TitleBgApplyRoomNow(title_bg_pending_msn_no, title_bg_pending_room_no);
    }

    title_bg_pending_preset_index = -1;
    title_bg_pending_msn_no = -1;
    title_bg_pending_room_no = -1;
}

static void TitleBgFadeUpdate(int room_ready)
{
    int frames = TitleBgFadeFrameCount();

    switch (title_bg_fade_state)
    {
    case TITLE_BG_FADE_OUT:
        title_bg_fade_alpha = title_bg_fade_timer * TITLE_BG_FADE_ALPHA_MAX / frames;

        if (title_bg_fade_timer >= frames)
        {
            title_bg_fade_alpha = TITLE_BG_FADE_ALPHA_MAX;
            TitleBgApplyPendingFadeTarget();
            title_bg_fade_state = TITLE_BG_FADE_WAIT_READY;
            title_bg_fade_timer = 0;
        }
        else
        {
            title_bg_fade_timer++;
        }
    break;
    case TITLE_BG_FADE_WAIT_READY:
        title_bg_fade_alpha = TITLE_BG_FADE_ALPHA_MAX;

        if (room_ready != 0)
        {
            title_bg_fade_state = TITLE_BG_FADE_IN;
            title_bg_fade_timer = 0;
        }
    break;
    case TITLE_BG_FADE_IN:
        title_bg_fade_alpha =
            (frames - title_bg_fade_timer) * TITLE_BG_FADE_ALPHA_MAX / frames;

        if (title_bg_fade_timer >= frames)
        {
            title_bg_fade_state = TITLE_BG_FADE_IDLE;
            title_bg_fade_timer = 0;
            title_bg_fade_alpha = 0;
        }
        else
        {
            title_bg_fade_timer++;
        }
    break;
    case TITLE_BG_FADE_IDLE:
    default:
        title_bg_fade_alpha = 0;
    break;
    }
}

static void TitleBgDrawFadeOverlay(void)
{
    u_char ps2_alpha;
    float alpha;
    float vertices[4][8];

    if (title_bg_fade_alpha <= 0)
    {
        return;
    }

    ps2_alpha = (u_char)MikuPan_ClampInt(
        title_bg_fade_alpha, 0, TITLE_BG_FADE_ALPHA_MAX);
    alpha = MikuPan_ConvertScaleColor(ps2_alpha);

    vertices[0][0] = 0.0f; vertices[0][1] = 0.0f; vertices[0][2] = 0.0f; vertices[0][3] = alpha;
    vertices[0][4] = -1.0f; vertices[0][5] = -1.0f; vertices[0][6] = 0.0f; vertices[0][7] = 1.0f;
    vertices[1][0] = 0.0f; vertices[1][1] = 0.0f; vertices[1][2] = 0.0f; vertices[1][3] = alpha;
    vertices[1][4] = 1.0f; vertices[1][5] = -1.0f; vertices[1][6] = 0.0f; vertices[1][7] = 1.0f;
    vertices[2][0] = 0.0f; vertices[2][1] = 0.0f; vertices[2][2] = 0.0f; vertices[2][3] = alpha;
    vertices[2][4] = -1.0f; vertices[2][5] = 1.0f; vertices[2][6] = 0.0f; vertices[2][7] = 1.0f;
    vertices[3][0] = 0.0f; vertices[3][1] = 0.0f; vertices[3][2] = 0.0f; vertices[3][3] = alpha;
    vertices[3][4] = 1.0f; vertices[3][5] = 1.0f; vertices[3][6] = 0.0f; vertices[3][7] = 1.0f;

    MikuPan_RenderUntexturedSprite(&vertices[0][0]);
}

void MikuPan_TitleCalibrationPreviewOpen(void)
{
    const TITLE_BG_PRESET *preset = &title_bg_presets[0];
    int reload_room = title_bg_load_failed != 0
                      || title_bg_msn_no != preset->msn_no
                      || title_bg_room_no != preset->room_no;

    title_bg_load_failed = 0;
    title_bg_preset_index = 0;
    title_bg_msn_no = preset->msn_no;
    title_bg_room_no = preset->room_no;
    title_bg_auto_cycle = 0;
    title_bg_auto_cycle_timer = 0;
    title_bg_camera_lerp_enabled = 0;
    title_bg_camera_lerp_timer = 0;
    title_bg_camera_lerp_t = 0.0f;
    TitleBgApplyCameraPoint(&preset->camera);

    if (reload_room)
    {
        TitleBgRequestRoomReload();
    }
}

void MikuPan_TitleCalibrationPreviewDraw(void)
{
    int old_msn_no;

    if (title_bg_load_failed != 0 || MikuPan_TitleBackgroundRoomReady() == 0)
    {
        MikuPan_TitleSceneBeginFrame(0, -1, -1, NULL);
        return;
    }

    if (room_addr_tbl[title_bg_room_no].near_sgd == NULL
        || room_addr_tbl[title_bg_room_no].lit_data == NULL)
    {
        MikuPan_TitleSceneBeginFrame(0, -1, -1, NULL);
        return;
    }

    old_msn_no = ingame_wrk.msn_no;
    ingame_wrk.msn_no = title_bg_msn_no;

    TitleBgSetupCamera();
    MikuPan_TitleSceneBeginFrame(1, title_bg_msn_no, title_bg_room_no,
                                 &title_bg_camera);
    TitleBgSetupRoomState();

    gra3dDrawRoomBackgroundLocal(&title_bg_camera, GRA3D_ROOM_BG_DEFAULT,
                                 title_bg_room_world_pos);
    MikuPan_TitleSceneDraw();
    ingame_wrk.msn_no = old_msn_no;
}

void MikuPan_TitleBackgroundDrawRoom(void)
{
    int room_no;
    int room_ready;
    int old_msn_no;

    if (MikuPan_TitleUseRoomBackground() == 0)
    {
        MikuPan_TitleSceneBeginFrame(0, -1, -1, NULL);
        return;
    }

    room_no = title_bg_room_no;
    room_ready = MikuPan_TitleBackgroundRoomReady();

    if (room_ready == 0)
    {
        MikuPan_TitleSceneBeginFrame(0, -1, -1, NULL);
        TitleBgFadeUpdate(room_ready);
        TitleBgDrawFadeOverlay();
        return;
    }

    if (room_addr_tbl[room_no].near_sgd == NULL || room_addr_tbl[room_no].lit_data == NULL)
    {
        MikuPan_TitleSceneBeginFrame(0, -1, -1, NULL);
        TitleBgFadeUpdate(room_ready);
        TitleBgDrawFadeOverlay();
        return;
    }

    old_msn_no = ingame_wrk.msn_no;
    ingame_wrk.msn_no = title_bg_msn_no;

    TitleBgCameraLerpUpdate();
    TitleBgAutoCycleUpdate();

    TitleBgSetupCamera();
    MikuPan_TitleSceneBeginFrame(1, title_bg_msn_no, title_bg_room_no,
                                 &title_bg_camera);
    TitleBgSetupRoomState();

    gra3dDrawRoomBackgroundLocal(&title_bg_camera, GRA3D_ROOM_BG_DEFAULT,
                                 title_bg_room_world_pos);
    MikuPan_TitleSceneDraw();
    ingame_wrk.msn_no = old_msn_no;
    TitleBgFadeUpdate(room_ready);
    TitleBgDrawFadeOverlay();
}

void MikuPan_TitleBackgroundDebugUi(void)
{
    if (MikuPan_TitleDebugWindowVisible() == 0)
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
    int use_room_background = MikuPan_TitleUseRoomBackground();
    int use_title_dither = MikuPan_TitleUseDither();
    char constants[512];
    char full_values[2048];

    igSetNextWindowPos(
        ImVec2{12.0f, 36.0f},
        ImGuiCond_FirstUseEver,
        ImVec2{0.0f, 0.0f});

    igBegin("Title Background Room", NULL, ImGuiWindowFlags_AlwaysAutoResize);

    igText("State: %s", TitleBgRoomStateName());
    igText("Room asset: %s", TitleBgRoomName(title_bg_room_no));
    igText("Map floor: %d local room: %d", title_bg_floor, title_bg_map_room_no);
    igText(
        "Furn work/load: %d/%u  Door work/load: %d/%u  Model req: %d",
        TitleBgCountDrawableFurnitureWork(),
        room_load_block[TITLE_BG_ROOM_BLOCK].furn_num,
        TitleBgCountDrawableDoorWork(),
        room_load_block[TITLE_BG_ROOM_BLOCK].door_num,
        title_bg_model_load_num);

    igSeparator();
    igText("Title style");

    if (igCheckbox("Use room background", (bool *)&use_room_background))
    {
        MikuPan_TitleSetUseRoomBackground(use_room_background);
    }

    if (igCheckbox("Use title dithering", (bool *)&use_title_dither))
    {
        MikuPan_TitleSetUseDither(use_title_dither);
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

    if (igButton("Play / restart audio", ImVec2{0.0f, 0.0f}))
    {
        TitleAudioRestartBgm();
    }

    igSameLine(0.0f, -1.0f);

    if (igButton("Stop audio", ImVec2{0.0f, 0.0f}))
    {
        TitleAudioStopBgm();
    }

    if (igButton("Use default title audio##title_audio", ImVec2{0.0f, 0.0f}))
    {
        title_bgm_file_no = TITLE_BGM_DEFAULT_FILE_NO;
        TitleAudioRestartBgm();
    }

    igSameLine(0.0f, -1.0f);

    if (igButton("Use original title BGM##title_audio", ImVec2{0.0f, 0.0f}))
    {
        title_bgm_file_no = AO000_TITLE_STR;
        TitleAudioRestartBgm();
    }

    igSeparator();

    if (igCheckbox("Auto cycle title scenes", (bool *)&auto_cycle))
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
        title_bg_auto_cycle_seconds = MikuPan_ClampInt(cycle_seconds, 1, 120);
        title_bg_auto_cycle_timer = 0;
    }

    if (igInputInt("Title scene", &preset_index, 1, 1, 0))
    {
        TitleBgPauseCameraAnimations();
        TitleBgApplyPreset(preset_index - 1);
    }

    igSameLine(0.0f, -1.0f);

    if (igButton("Prev##title_bg_scene", ImVec2{0.0f, 0.0f}))
    {
        TitleBgPauseCameraAnimations();
        TitleBgApplyPrevPreset();
    }

    igSameLine(0.0f, -1.0f);

    if (igButton("Next##title_bg_scene", ImVec2{0.0f, 0.0f}))
    {
        TitleBgPauseCameraAnimations();
        TitleBgApplyNextPreset();
    }

    igText("Title scene count: %d", TITLE_BG_PRESET_COUNT);

    if (igInputInt("Mission", &msn_no, 1, 10, 0))
    {
        TitleBgSetMissionNo(msn_no);
    }

    if (igInputInt("Room", &room_no, 1, 5, 0))
    {
        TitleBgSetRoomNo(room_no);
    }

    if (igButton("Reload room", ImVec2{0.0f, 0.0f}))
    {
        TitleBgResetFade();
        TitleBgPauseCameraAnimations();
        TitleBgRequestRoomReload();
    }

    igSameLine(0.0f, -1.0f);

    if (igButton("Reset camera", ImVec2{0.0f, 0.0f}))
    {
        TitleBgPauseCameraAnimations();
        TitleBgResetCamera();
    }

    igSeparator();
    igDragFloat("Move step", &title_bg_camera_move_step, 5.0f, 1.0f, 2500.0f, "%.1f", 0);

    igText("Pan camera + target");

    if (igButton("Forward##title_bg_pan", ImVec2{92.0f, 0.0f}))
    {
        TitleBgPauseCameraAnimations();
        TitleBgPanForward(title_bg_camera_move_step);
    }

    igSameLine(0.0f, -1.0f);

    if (igButton("Back##title_bg_pan", ImVec2{92.0f, 0.0f}))
    {
        TitleBgPauseCameraAnimations();
        TitleBgPanForward(-title_bg_camera_move_step);
    }

    if (igButton("Left##title_bg_pan", ImVec2{92.0f, 0.0f}))
    {
        TitleBgPauseCameraAnimations();
        TitleBgPanRight(-title_bg_camera_move_step);
    }

    igSameLine(0.0f, -1.0f);

    if (igButton("Right##title_bg_pan", ImVec2{92.0f, 0.0f}))
    {
        TitleBgPauseCameraAnimations();
        TitleBgPanRight(title_bg_camera_move_step);
    }

    if (igButton("Up##title_bg_pan", ImVec2{92.0f, 0.0f}))
    {
        TitleBgPauseCameraAnimations();
        TitleBgPanY(-title_bg_camera_move_step);
    }

    igSameLine(0.0f, -1.0f);

    if (igButton("Down##title_bg_pan", ImVec2{92.0f, 0.0f}))
    {
        TitleBgPauseCameraAnimations();
        TitleBgPanY(title_bg_camera_move_step);
    }

    igText("Dolly camera");

    if (igButton("In##title_bg_dolly", ImVec2{92.0f, 0.0f}))
    {
        TitleBgPauseCameraAnimations();
        TitleBgDolly(title_bg_camera_move_step);
    }

    igSameLine(0.0f, -1.0f);

    if (igButton("Out##title_bg_dolly", ImVec2{92.0f, 0.0f}))
    {
        TitleBgPauseCameraAnimations();
        TitleBgDolly(-title_bg_camera_move_step);
    }

    igText("Look target");

    if (igButton("Left##title_bg_look", ImVec2{92.0f, 0.0f}))
    {
        TitleBgPauseCameraAnimations();
        TitleBgLookRight(-title_bg_camera_move_step);
    }

    igSameLine(0.0f, -1.0f);

    if (igButton("Right##title_bg_look", ImVec2{92.0f, 0.0f}))
    {
        TitleBgPauseCameraAnimations();
        TitleBgLookRight(title_bg_camera_move_step);
    }

    if (igButton("Up##title_bg_look", ImVec2{92.0f, 0.0f}))
    {
        TitleBgPauseCameraAnimations();
        TitleBgLookY(-title_bg_camera_move_step);
    }

    igSameLine(0.0f, -1.0f);

    if (igButton("Down##title_bg_look", ImVec2{92.0f, 0.0f}))
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

    if (igButton("Set A from camera##title_bg_lerp", ImVec2{0.0f, 0.0f}))
    {
        TitleBgPauseCameraAnimations();
        TitleBgCopyCurrentCamera(&title_bg_camera_lerp_a);
    }

    igSameLine(0.0f, -1.0f);

    if (igButton("Apply A##title_bg_lerp", ImVec2{0.0f, 0.0f}))
    {
        TitleBgPauseCameraAnimations();
        TitleBgApplyCameraPoint(&title_bg_camera_lerp_a);
        title_bg_camera_lerp_t = 0.0f;
    }

    if (igButton("Set B from camera##title_bg_lerp", ImVec2{0.0f, 0.0f}))
    {
        TitleBgPauseCameraAnimations();
        TitleBgCopyCurrentCamera(&title_bg_camera_lerp_b);
    }

    igSameLine(0.0f, -1.0f);

    if (igButton("Apply B##title_bg_lerp", ImVec2{0.0f, 0.0f}))
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
        "msn=%d room=%d room_background=%d audio=%d auto_cycle=%d cycle_seconds=%d title_scene=%d\n"
        "camera p={%.1ff, %.1ff, %.1ff} i={%.1ff, %.1ff, %.1ff} fov=%.1ff\n"
        "lerp enabled=%d seconds=%.1ff t=%.3ff\n"
        "lerp_a p={%.1ff, %.1ff, %.1ff} i={%.1ff, %.1ff, %.1ff} fov=%.1ff\n"
        "lerp_b p={%.1ff, %.1ff, %.1ff} i={%.1ff, %.1ff, %.1ff} fov=%.1ff",
        title_bg_msn_no,
        title_bg_room_no,
        MikuPan_TitleUseRoomBackground(),
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

    if (igButton("Copy current camera", ImVec2{0.0f, 0.0f}))
    {
        igSetClipboardText(constants);
    }

    igSeparator();
    igTextWrapped("%s", full_values);

    if (igButton("Copy full setup", ImVec2{0.0f, 0.0f}))
    {
        igSetClipboardText(full_values);
    }

    igEnd();
}

