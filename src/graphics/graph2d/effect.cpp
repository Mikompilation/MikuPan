#include "common.h"
#include "enums.h"
#include "typedefs.h"

#define INCLUDING_FROM_EFFECT_C
#include "effect.h"
#undef INCLUDING_FROM_EFFECT_C

#include "sce/libvu0.h"

#include "graphics/graph2d/effect_ene.h"
#include "graphics/graph2d/effect_obj.h"
#include "graphics/graph2d/effect_oth.h"
#include "graphics/graph2d/effect_rdr.h"
#include "graphics/graph2d/effect_scr.h"
#include "graphics/graph2d/effect_sub.h"
#include "graphics/graph2d/g2d_debug.h"
#include "graphics/graph2d/message.h"
#include "graphics/graph2d/rare_ene.h"
#include "graphics/graph2d/sprt.h"
#include "graphics/graph3d/gra3d.h"
#include "graphics/graph3d/libsg.h"
#include "graphics/graph3d/sglib.h"
#include "ingame/event/ev_main.h"
#include "ingame/ig_glob.h"
#include "ingame/photo/pht_make.h"
#include "main/glob.h"
#include "mikupan/rendering/mikupan_renderer.h"
// #include "ingame/entry/ap_sgost.h" // DO NOT INCLUDE

#include <stdarg.h>
#include <string.h>

#include "data/camdat.h"// SPRT_DAT camdat[];
#include "data/effdat.h"// SPRT_DAT effdat[];
#include "ingame/entry/ap_sgost.h"
static int tmp_effect_off = 0;
static void *r23_e1 = 0;
static void *r23_e2 = 0;
static u_char r28_torch_flag = 0;
static int set_buffer[2];

#define EFFECT_DEBUG_COUNT (EF_SEPIA + 1)

typedef struct {
    int id;
    const char *enum_name;
    const char *function_name;
    const char *note;
    u_char can_toggle;
    u_char persistent;
} EFFECT_DEBUG_ENTRY;

static EFFECT_DEBUG_ENTRY effect_debug_entries[EFFECT_DEBUG_COUNT] = {
    {EF_NULL, "EF_NULL", "-", "No effect body.", 0, 0},
    {EF_Z_DEP, "EF_Z_DEP", "SetForcusDepth", "", 1, 0},
    {EF_DITHER, "EF_DITHER", "SetDither3", "", 1, 0},
    {EF_BLUR_N, "EF_BLUR_N", "SetBlur", "", 1, 0},
    {EF_BLUR_B, "EF_BLUR_B", "SetBlur", "", 1, 0},
    {EF_BLUR_W, "EF_BLUR_W", "SetBlur", "", 1, 0},
    {EF_DEFORM, "EF_DEFORM", "SetDeform", "", 1, 0},
    {EF_FOCUS, "EF_FOCUS", "SetFocus1", "", 1, 0},
    {EF_OVERLAP, "EF_OVERLAP", "SetOverRap", "", 1, 0},
    {EF_FADEFRAME, "EF_FADEFRAME", "SetFadeFrame", "", 1, 0},
    {EF_RENZFLARE, "EF_RENZFLARE", "SetRenzFlare", "", 1, 1},
    {EF_BLACKFILTER, "EF_BLACKFILTER", "SetBlackFilter", "", 1, 0},
    {EF_NEGA, "EF_NEGA", "SetNega", "", 1, 0},
    {EF_NCONTRAST, "EF_NCONTRAST", "SetContrast2", "", 1, 0},
    {EF_NCONTRAST2, "EF_NCONTRAST2", "SetContrast2", "", 1, 0},
    {EF_NCONTRAST3, "EF_NCONTRAST3", "SetContrast3", "", 1, 0},
    {EF_MAGATOKI, "EF_MAGATOKI", "SetMAGATOKI", "", 1, 1},
    {EF_ENEDMG1, "EF_ENEDMG1", "SetEneDmgEffect1_Sub", "Needs live enemy damage state.", 0, 0},
    {EF_ENEDMG2, "EF_ENEDMG2", "SetEneDmgEffect2_Sub", "Needs live enemy damage state.", 0, 0},
    {EF_SPIRIT, "EF_SPIRIT", "SetSpirit", "Spawns nested flame buffers; use scene data path.", 0, 0},
    {EF_HALO, "EF_HALO", "SetHalo", "", 1, 1},
    {EF_RIPPLE, "EF_RIPPLE", "SetRipple", "", 1, 0},
    {EF_RIPPLE2, "EF_RIPPLE2", "SetRipple", "", 1, 0},
    {EF_FIRE, "EF_FIRE", "SetFire", "", 1, 1},
    {EF_FIRE2, "EF_FIRE2", "SetFire2", "", 1, 1},
    {EF_TORCH, "EF_TORCH", "SetTorch", "", 1, 1},
    {EF_SMOKE, "EF_SMOKE", "SetSmoke", "", 1, 1},
    {EF_PDEFORM, "EF_PDEFORM", "SetPartsDeform", "", 1, 1},
    {EF_ENEFIRE, "EF_ENEFIRE", "SetEneFire", "Needs live enemy pointers.", 0, 0},
    {EF_DUST, "EF_DUST", "SetDust", "", 1, 1},
    {EF_WATERDROP, "EF_WATERDROP", "SetWaterdrop", "", 1, 1},
    {EF_SUNSHINE, "EF_SUNSHINE", "SetSunshine", "", 1, 1},
    {EF_NEGACIRCLE, "EF_NEGACIRCLE", "SetNegaCircle", "", 1, 0},
    {EF_ENEFACE, "EF_ENEFACE", "SetEneFace", "", 1, 1},
    {EF_FACESPIRIT, "EF_FACESPIRIT", "SetFaceSpirit", "Needs fly/facespirit runtime state.", 0, 0},
    {EF_DITHER2, "EF_DITHER2", "SetDither4", "", 1, 0},
    {EF_Z_DEP2, "EF_Z_DEP2", "SetForcusDepth2", "", 1, 0},
    {EF_HAZE, "EF_HAZE", "SetHaze_Pond", "Only visible in haze-enabled rooms.", 1, 0},
    {EF_PBLUR, "EF_PBLUR", "pblur", "", 1, 0},
    {EF_ENEIN, "EF_ENEIN", "SetPartsDeform", "Needs enemy-specific position/state.", 0, 0},
    {EF_ENEOUT, "EF_ENEOUT", "SetEneSeal", "Needs live enemy state.", 0, 0},
    {EF_LUMINE, "EF_LUMINE", "-", "SetEffects has data setup, but no renderer consumes it.", 0, 0},
    {EF_STEALTH, "EF_STEALTH", "-", "SetEffects has data setup, but no renderer consumes it.", 0, 0},
    {EF_STEALTH2, "EF_STEALTH2", "-", "SetEffects has data setup, but no renderer consumes it.", 0, 0},
    {EF_007, "EF_007", "-", "SetEffects has data setup, but no renderer consumes it.", 0, 0},
    {EF_MONO, "EF_MONO", "ChangeMonochrome", "", 1, 0},
    {EF_SEPIA, "EF_SEPIA", "FOD_EFF_FRAME.sepia", "FOD parses this flag, but the port has no sepia renderer yet.", 0, 0},
};

static int effect_debug_enabled[EFFECT_DEBUG_COUNT] = {0};
static void *effect_debug_handle[EFFECT_DEBUG_COUNT] = {0};
static u_char effect_debug_blur_alpha = 0x64;
static u_char effect_debug_nega_alpha = 0x80;
static float effect_debug_rate = 1.0f;
static float effect_debug_parts_speed = 0.0f;
static float effect_debug_parts_rate = 1.0f;
static float effect_debug_parts_trate = 1.0f;
static u_char effect_debug_magatoki_flag = 0;
static int effect_debug_mono_active = 0;
static int effect_debug_haze_active = 0;
static sceVu0FVECTOR effect_debug_pos = {0.0f, 0.0f, 0.0f, 1.0f};
static sceVu0FVECTOR effect_debug_pos2 = {0.0f, 0.0f, 0.0f, 1.0f};
static sceVu0FVECTOR effect_debug_rot = {0.0f, 0.0f, 0.0f, 1.0f};

static EFFECT_CONT *MikuPan_EffectDebugCont(int id)
{
    if (id < 0 || id >= EFFECT_DEBUG_COUNT)
    {
        return NULL;
    }

    return (EFFECT_CONT *)effect_debug_handle[id];
}

static void MikuPan_EffectDebugUpdateAnchor(void)
{
    if (sys_wrk.game_mode != GAME_MODE_OUTGAME)
    {
        Vu0CopyVector(effect_debug_pos, plyr_wrk.move_box.pos);
    }
    else
    {
        Vu0CopyVector(effect_debug_pos, camera.i);
    }

    effect_debug_pos[1] -= 120.0f;
    effect_debug_pos[3] = 1.0f;

    Vu0CopyVector(effect_debug_pos2, effect_debug_pos);
    effect_debug_pos2[1] -= 700.0f;
    effect_debug_pos2[3] = 1.0f;

    effect_debug_rot[0] = 0.0f;
    effect_debug_rot[1] = 0.0f;
    effect_debug_rot[2] = 0.0f;
    effect_debug_rot[3] = 1.0f;
}

static void MikuPan_EffectDebugResetState(void)
{
    memset(effect_debug_enabled, 0, sizeof(effect_debug_enabled));
    memset(effect_debug_handle, 0, sizeof(effect_debug_handle));
    effect_debug_magatoki_flag = 0;
    effect_debug_mono_active = 0;
    effect_debug_haze_active = 0;
}

static void MikuPan_EffectDebugStopPersistent(int id)
{
    EFFECT_CONT *ec = MikuPan_EffectDebugCont(id);

    if (ec == NULL)
    {
        return;
    }

    if (id == EF_MAGATOKI)
    {
        effect_debug_magatoki_flag = 4;

        if (ec->dat.uc8[0] == 0)
        {
            effect_debug_handle[id] = NULL;
        }

        return;
    }

    ResetEffects(ec);
    effect_debug_handle[id] = NULL;
}

static void MikuPan_EffectDebugStartPersistent(int id)
{
    EFFECT_CONT *ec = MikuPan_EffectDebugCont(id);

    if (ec != NULL && ec->dat.uc8[0] == 0)
    {
        effect_debug_handle[id] = NULL;
        ec = NULL;
    }

    if (ec != NULL)
    {
        return;
    }

    switch (id)
    {
        case EF_RENZFLARE:
            effect_debug_handle[id] =
                SetEffects(id, 2, 4, effect_debug_pos, effect_debug_rot);
            break;
        case EF_MAGATOKI:
            effect_debug_magatoki_flag = 1;
            effect_debug_handle[id] =
                SetEffects(id, 8, 30, 30, &effect_debug_magatoki_flag);
            break;
        case EF_HALO:
            effect_debug_handle[id] = SetEffects(
                id, 2, 0, effect_debug_pos, 0x80, 0x80, 0xff, 0.8f, 0);
            break;
        case EF_FIRE:
            effect_debug_handle[id] = SetEffects(
                id, 2, 3, effect_debug_pos, 0x50, 0x46, 0x1e, 0.4f, 0xf0,
                0xd0, 0xa0, 3.0f);
            break;
        case EF_FIRE2:
            effect_debug_handle[id] = SetEffects(
                id, 2, 3, effect_debug_pos, 0x50, 0x46, 0x1e, 0.4f, 0xf0,
                0xd0, 0xa0, 3.0f, 1.0f);
            break;
        case EF_TORCH:
            effect_debug_handle[id] =
                SetEffects(id, 2, 1, effect_debug_pos, &effect_debug_rate,
                           &effect_debug_rate);
            break;
        case EF_SMOKE:
            effect_debug_handle[id] = SetEffects(id, 2, effect_debug_pos);
            break;
        case EF_PDEFORM:
            effect_debug_handle[id] =
                SetEffects(id, 2, 23, 0x80, 0.8f, 0.8f, effect_debug_pos, 0, 0,
                           0, (void *)0, &effect_debug_parts_speed,
                           &effect_debug_parts_rate, &effect_debug_parts_trate);
            break;
        case EF_DUST:
            effect_debug_handle[id] = SetEffects(id, 2, effect_debug_pos);
            break;
        case EF_WATERDROP:
            effect_debug_handle[id] = SetEffects(
                id, 2, effect_debug_pos, 1, 250.0f, 200, 0x80, 0x80, 0x80);
            break;
        case EF_SUNSHINE:
            effect_debug_handle[id] =
                SetEffects(id, 2, effect_debug_pos, effect_debug_pos2,
                           effect_debug_rot, 8, 0.9f, 0.6f, 0xff, 0xf0, 0xc0);
            break;
        case EF_ENEFACE:
            effect_debug_handle[id] =
                SetEffects(id, 2, 0, 0, effect_debug_pos[0],
                           effect_debug_pos[1], effect_debug_pos[2]);
            break;
    }
}

static void MikuPan_EffectDebugApplyOneShot(int id)
{
    switch (id)
    {
        case EF_Z_DEP:
            SetEffects(id, 1);
            break;
        case EF_DITHER:
            SetEffects(id, 1, 2, 24.0f, 8.0f, 0x6a, 0x65);
            break;
        case EF_BLUR_N:
        case EF_BLUR_B:
        case EF_BLUR_W:
            SetEffects(id, 1, &effect_debug_blur_alpha, 1000, 1800, 320.0f,
                       112.0f);
            break;
        case EF_DEFORM:
            SetEffects(id, 1, 2, 12);
            break;
        case EF_FOCUS:
            SetEffects(id, 1, 20);
            break;
        case EF_OVERLAP:
            SetEffects(id, 1, 0x28);
            break;
        case EF_FADEFRAME:
            SetEffects(id, 1, 0x50, 0x80000);
            break;
        case EF_BLACKFILTER:
            SetEffects(id, 1, 0x48);
            break;
        case EF_NEGA:
            SetEffects(id, 1, 0x40, 0xc4, &effect_debug_nega_alpha);
            break;
        case EF_NCONTRAST:
        case EF_NCONTRAST2:
        case EF_NCONTRAST3:
            SetEffects(id, 1, 0x80, 0x80);
            break;
        case EF_RIPPLE:
            SetEffects(id, 1, 1, 16, effect_debug_pos);
            break;
        case EF_RIPPLE2:
            SetEffects(id, 8, 1, 16, 0x80, 0x80, 0x80, 1.0f, 3.2f,
                       effect_debug_pos, effect_debug_rot, 1);
            break;
        case EF_NEGACIRCLE:
            SetEffects(id, 1, 320.0f, 224.0f, 180.0f, 0x60, 0x80, 0x40,
                       0x40);
            break;
        case EF_DITHER2:
            SetEffects(id, 1, 1, 18.0f, 7.0f);
            break;
        case EF_Z_DEP2:
            SetEffects(id, 1);
            break;
        case EF_HAZE:
            if (!effect_debug_haze_active)
            {
                SetHaze_Pond_SW(1);
                effect_debug_haze_active = 1;
            }
            break;
        case EF_PBLUR:
            dbg_wrk.eff_prtblr_sw = 1;
            dbg_wrk.eff_prtblr_alp = 0x46;
            break;
        case EF_MONO:
            if (!effect_debug_mono_active)
            {
                ChangeMonochrome(1);
                effect_debug_mono_active = 1;
            }
            break;
    }
}

int MikuPan_EffectDebugCount(void)
{
    return EFFECT_DEBUG_COUNT;
}

const char *MikuPan_EffectDebugEnumName(int id)
{
    if (id < 0 || id >= EFFECT_DEBUG_COUNT)
    {
        return "?";
    }

    return effect_debug_entries[id].enum_name;
}

const char *MikuPan_EffectDebugFunctionName(int id)
{
    if (id < 0 || id >= EFFECT_DEBUG_COUNT)
    {
        return "?";
    }

    return effect_debug_entries[id].function_name;
}

const char *MikuPan_EffectDebugNote(int id)
{
    if (id < 0 || id >= EFFECT_DEBUG_COUNT)
    {
        return "";
    }

    return effect_debug_entries[id].note;
}

int MikuPan_EffectDebugCanToggle(int id)
{
    if (id < 0 || id >= EFFECT_DEBUG_COUNT)
    {
        return 0;
    }

    return effect_debug_entries[id].can_toggle != 0;
}

int MikuPan_EffectDebugEnabled(int id)
{
    if (id < 0 || id >= EFFECT_DEBUG_COUNT)
    {
        return 0;
    }

    return effect_debug_enabled[id] != 0;
}

void MikuPan_EffectDebugSetEnabled(int id, int enabled)
{
    if (id < 0 || id >= EFFECT_DEBUG_COUNT
        || !MikuPan_EffectDebugCanToggle(id))
    {
        return;
    }

    effect_debug_enabled[id] = enabled != 0;
}

void MikuPan_EffectDebugSetAll(int enabled)
{
    int i;

    for (i = 0; i < EFFECT_DEBUG_COUNT; i++)
    {
        MikuPan_EffectDebugSetEnabled(i, enabled);
    }
}

void MikuPan_EffectDebugApply(void)
{
    int i;

    MikuPan_EffectDebugUpdateAnchor();

    for (i = 0; i < EFFECT_DEBUG_COUNT; i++)
    {
        if (!MikuPan_EffectDebugCanToggle(i))
        {
            continue;
        }

        if (effect_debug_entries[i].persistent)
        {
            if (effect_debug_enabled[i])
            {
                MikuPan_EffectDebugStartPersistent(i);
            }
            else
            {
                MikuPan_EffectDebugStopPersistent(i);
            }
        }
        else if (effect_debug_enabled[i])
        {
            MikuPan_EffectDebugApplyOneShot(i);
        }
    }

    if (!effect_debug_enabled[EF_HAZE] && effect_debug_haze_active)
    {
        SetHaze_Pond_SW(0);
        effect_debug_haze_active = 0;
    }

    if (!effect_debug_enabled[EF_PBLUR])
    {
        dbg_wrk.eff_prtblr_sw = 0;
    }

    if (!effect_debug_enabled[EF_MONO] && effect_debug_mono_active)
    {
        ChangeMonochrome(0);
        effect_debug_mono_active = 0;
    }
}

void InitEffects()
{
    eff_blur_off = 1;
    eff_dith_off = 0;
    look_debugmenu = 1;
    stop_effects = 0;
    change_efbank = 1;
    monochrome_mode = 0;
    magatoki_mode = 0;
    shibata_set_off = 0;
    eff_filament_off = 0;
    effect_disp_flg = 1;

    InitShibataSet();
    InitEffectSub();
    InitEffectScr();
    InitEffectObj();
    InitEffectOth();
    InitEffectRdr();
    InitEffectEne();
    EneDmgTexInit();
    InitPhotoMake();

    memset(efcnt, 0, 64 * sizeof(EFFECT_CONT));
    memset(efcnt_cnt, 0, 64 * sizeof(EFFECT_CONT));
    memset(efcntm, 0, 48 * sizeof(EFFECT_CONT));
    memset(efcntm_cnt, 0, 48 * sizeof(EFFECT_CONT));

    SetScreenEffect();

    r23_e1 = NULL;
    r23_e2 = NULL;
    r28_torch_flag = 0;

    set_buffer[0] = 0;
    set_buffer[1] = 0;
    now_buffer[0] = 0;
    now_buffer[1] = 0;

    MikuPan_EffectDebugResetState();
}

void InitEffectsEF()
{
    int i;
    static void *e[3] = {0};
    static float aalp = 1.0f;
    static sceVu0FVECTOR canal1 = {27200.0f, -2000.0f, 29500.0f, 1.0f};
    static sceVu0FVECTOR canal2 = {28000.0f, -2000.0f, 29250.0f, 1.0f};
    static sceVu0FVECTOR torch_pos[3] = {
        { 7825.0f, 2600.0f, 35150.0f, 1.0f},
        { 7825.0f, 2575.0f, 38100.0f, 1.0f},
        {11225.0f, 2600.0f, 35150.0f, 1.0f},
    };

    set_buffer[0] = now_buffer[0];
    set_buffer[1] = now_buffer[1];

    effect_disp_flg = (ingame_wrk.stts & 0x20) == 0 ? effect_disp_flg : 0;

    now_buffer[0] = 0;
    now_buffer[1] = 0;

    InitEffectScrEF();
    InitEffectObjEF();
    InitEffectOthEF();
    InitEffectRdrEF();
    InitEffectEneEF();
    SettleGhostDispEffect();

    if (shibata_set_off == 0 && look_debugmenu != 0)
    {
        if (msbtset.dt.sw != 0)
        {
            SetEffects(EF_DITHER, 1, msbtset.dt.type, msbtset.dt.alp,
                       msbtset.dt.spd, msbtset.dt.amax, msbtset.dt.cmax);
        }

        if (msbtset.bl.sw != 0)
        {
            SetEffects(EF_BLUR_N, 1, &msbtset.bl.alp, msbtset.bl.scl,
                       msbtset.bl.rot, msbtset.bl.x, msbtset.bl.y);
        }

        if (msbtset.df.sw != 0)
        {
            SetEffects(EF_DEFORM, 1, msbtset.df.type, msbtset.df.rate);
        }

        if (msbtset.cn.sw != 0)
        {
            switch (msbtset.cn.type)
            {
                case 1:
                    SetEffects(EF_NCONTRAST, 1, msbtset.cn.col, msbtset.cn.alp);
                    break;
                case 2:
                    SetEffects(EF_NCONTRAST2, 1, msbtset.cn.col,
                               msbtset.cn.alp);
                    break;
                case 3:
                    SetEffects(EF_NCONTRAST3, 1, msbtset.cn.col,
                               msbtset.cn.alp);
                    break;
            }
        }

        if (msbtset.ff.sw != 0)
        {
            SetEffects(EF_FADEFRAME, 1, msbtset.ff.alp, 0x80000);
        }

        if (msbtset.ng.sw != 0)
        {
            SetEffects(EF_NEGA, 1, msbtset.ng.col, msbtset.ng.alp,
                       msbtset.ng.alp2);
        }

        if (msbtset.mn.sw == 0)
        {
            if (efcnt[EF_MONO].dat.uc8[0] == 0)
            {
                efcnt[EF_MONO].dat.uc8[0] = 1;
                ChangeMonochrome(0);
            }
        }

        if (msbtset.mn.sw != 0 && efcnt[EF_MONO].dat.uc8[0] == 0)
        {
            efcnt[EF_MONO].dat.uc8[0] = 1;
            ChangeMonochrome(1);
        }
    }

    MikuPan_EffectDebugApply();

    for (i = 0; i < 3; i++)
    {
        if (fly_display[i] != 0)
        {
            if (e[i] == NULL)
            {
                e[i] = SetEffects(EF_FACESPIRIT, 2, fly_wrk[i].move_box.pos,
                                  0x50, 0x50, 0x5c, &aalp, i);
            }
        }
        else
        {
            if (e[i] != NULL)
            {
                ResetEffects(e[i]);
                e[i] = NULL;
            }
        }
    }

    if (plyr_wrk.pr_info.room_no != 0x17)
    {
        if (r23_e1 != NULL)
        {
            ResetEffects(r23_e1);
            r23_e1 = NULL;
        }
        if (r23_e2 != NULL)
        {
            ResetEffects(r23_e2);
            r23_e2 = NULL;
        }
    }
    else
    {
        if (r23_e1 == NULL)
        {
            r23_e1 = SetEffects(EF_WATERDROP, 2, canal1, 1, 250.0, 200, 0x80,
                                0x80, 0x80);
        }

        if (r23_e2 == NULL)
        {
            r23_e2 = SetEffects(EF_WATERDROP, 2, canal2, 4, 250.0, 0x104, 0x80,
                                0x80, 0x80);
        }
    }

    if (plyr_wrk.pr_info.room_no != R029)
    {
        if (r28_torch_flag != 0)
        {
            for (i = 0; i < 3; i++)
            {
                ResetRDPFire(i);
            }
        }

        r28_torch_flag = 0;
    }
    else
    {
        if (r28_torch_flag == 0)
        {
            for (i = 0; i < 3; i++)
            {
                SetRDPFire(torch_pos[i], i);
            }

            r28_torch_flag = 1;
        }
    }
}

SBTSET msbtset = {0};
EFFECT_CONT efcnt[64] = {0};
EFFECT_CONT efcntm[48] = {0};
EFFECT_CONT efcnt_cnt[64] = {0};
EFFECT_CONT efcntm_cnt[48] = {0};

void EffectEndSet()
{
    eff_blur_off = 0;
}

void *SetEffects(int id, int fl, ...)
{
    va_list ap;
    va_start(ap, fl);

    int ret;
    EFFECT_CONT *ec;

    if ((ingame_wrk.stts & 0x20) && ev_wrk.movie_on != 4
        && effect_disp_flg == 0)
    {
        return NULL;
    }

    switch (id)
    {
        case 0:
        case 1:
        case 2:
        case 3:
        case 4:
        case 5:
        case 6:
        case 7:
        case 8:
        case 9:
        case 10:
        case 11:
        case 12:
        case 13:
        case 14:
        case 15:
        case 16:
        case 19:
        case 35:
        case 36:
        case 37:
            if (change_efbank != 0)
            {
                ec = &efcnt[id];
            }
            else
            {
                ec = &efcnt_cnt[id];
            }
            break;
        case 20:
        case 21:
        case 22:
        case 23:
        case 24:
        case 25:
        case 26:
        case 27:
        case 28:
        case 29:
        case 30:
        case 31:
        case 32:
        case 33:
        case 34:
        case 38:
        case 40:
            ret = SearchEmptyEffectBuf();

            if (ret == -1)
            {
                return NULL;
            }

            if (change_efbank != 0)
            {
                ec = &efcntm[ret];
            }
            else
            {
                ec = &efcntm_cnt[ret];
            }
            break;
        default:
            return NULL;
    }

    switch (id)
    {
        case EF_Z_DEP:
            ec->dat.uc8[0] = 1;
            ec->dat.uc8[1] = fl;
            break;
        case EF_Z_DEP2:
            ec->dat.uc8[0] = 36;
            ec->dat.uc8[1] = fl;
            break;
        case EF_DITHER:
            ec->dat.uc8[0] = 2;
            ec->dat.uc8[1] = fl;
            ec->dat.uc8[2] = (u_char) va_arg(ap, int);
            ec->dat.fl32[2] = va_arg(ap, double);
            ec->dat.fl32[3] = va_arg(ap, double);
            ec->dat.uc8[3] = (u_char) va_arg(ap, int);
            ec->dat.uc8[4] = (u_char) va_arg(ap, int);

            if (fl & 4)
            {
                ec->in = va_arg(ap, u_int);
                ec->keep = va_arg(ap, u_int);
                ec->out = va_arg(ap, u_int);
                ec->cnt = 0;
                ec->flow =
                    (ec->in == 0 ? (ec->keep == 0 ? ec->out != 0 ? 2 : 3 : 1)
                                 : 0);
            }
            break;
        case EF_DITHER2:
            ec->dat.uc8[0] = 35;
            ec->dat.uc8[1] = fl;
            ec->dat.uc8[2] = (u_char) va_arg(ap, int);
            ec->dat.fl32[1] = va_arg(ap, double);
            ec->dat.fl32[2] = va_arg(ap, double);
            break;
        case EF_BLUR_N:
            ec->dat.uc8[0] = 3;
            ec->dat.uc8[1] = fl;
            ec->dat.uc8[2] = 0;
            ec->pnt[0] = va_arg(ap, void *);
            ec->dat.ui32[2] = va_arg(ap, u_int);
            ec->dat.ui32[3] = va_arg(ap, u_int);
            ec->fw[0] = va_arg(ap, double);
            ec->fw[1] = va_arg(ap, double);
            break;
        case EF_BLUR_B:
            ec->dat.uc8[0] = 4;
            ec->dat.uc8[1] = fl;
            ec->dat.uc8[2] = 1;
            ec->pnt[0] = va_arg(ap, void *);
            ec->dat.ui32[2] = va_arg(ap, u_int);
            ec->dat.ui32[3] = va_arg(ap, u_int);
            ec->fw[0] = va_arg(ap, double);
            ec->fw[1] = va_arg(ap, double);
            break;
        case EF_BLUR_W:
            ec->dat.uc8[0] = 5;
            ec->dat.uc8[1] = fl;
            ec->dat.uc8[2] = 2;
            ec->pnt[0] = va_arg(ap, void *);
            ec->dat.ui32[2] = va_arg(ap, u_int);
            ec->dat.ui32[3] = va_arg(ap, u_int);
            ec->fw[0] = va_arg(ap, double);
            ec->fw[1] = va_arg(ap, double);
            break;
        case EF_DEFORM:
            ec->dat.uc8[0] = 6;
            ec->dat.uc8[1] = fl;
            ec->dat.uc8[2] = (u_char) va_arg(ap, int);
            ec->dat.uc8[3] = (u_char) va_arg(ap, int);
            ec->dat.uc8[4] = 0;

            if (fl & 4)
            {
                ec->flow =
                    (ec->in == 0 ? (ec->keep == 0 ? ec->out != 0 ? 2 : 3 : 1)
                                 : 0);
                ec->cnt = 0;
                ec->in = va_arg(ap, u_int);
                ec->keep = va_arg(ap, u_int);
                ec->out = va_arg(ap, u_int);
            }
            break;
        case EF_FOCUS:
            ec->dat.uc8[0] = 7;
            ec->dat.uc8[1] = fl;
            ec->dat.uc8[2] = (u_char) va_arg(ap, int);
            break;
        case EF_STEALTH:
            ec->dat.uc8[0] = 42;
            ec->dat.uc8[1] = fl;
            ec->dat.uc8[2] = 0;
            ec->pnt[0] = va_arg(ap, void *);
            break;
        case EF_STEALTH2:
            ec->dat.uc8[0] = 42;
            ec->dat.uc8[1] = fl;
            ec->dat.uc8[2] = 1;
            ec->pnt[0] = va_arg(ap, void *);
            break;
        case EF_007:
            ec->dat.uc8[0] = 42;
            ec->dat.uc8[1] = fl;
            ec->dat.uc8[2] = 2;
            ec->pnt[0] = va_arg(ap, void *);
            break;
        case EF_MONO:
            ec->dat.uc8[0] = 45;
            ec->dat.uc8[1] = fl;
            break;
        case EF_SEPIA:
            ec->dat.uc8[0] = 46;
            ec->dat.uc8[1] = fl;
            break;
        case EF_OVERLAP:
            ec->dat.uc8[0] = 8;
            ec->dat.uc8[1] = fl;
            ec->dat.uc8[2] = (u_char) va_arg(ap, int);
            break;
        case EF_FADEFRAME:
            ec->dat.uc8[0] = 9;
            ec->dat.uc8[1] = fl;
            ec->dat.uc8[2] = (u_char) va_arg(ap, int);
            ec->dat.ui32[1] = va_arg(ap, u_int);
            break;
        case EF_RENZFLARE:
            ec->dat.uc8[0] = 10;
            ec->dat.uc8[1] = fl;
            ec->dat.uc8[2] = (u_char) va_arg(ap, int);
            ec->pnt[0] = va_arg(ap, void *);
            ec->pnt[1] = va_arg(ap, void *);
            break;
        case EF_HAZE:
            ec->dat.uc8[0] = 37;
            ec->dat.uc8[1] = fl;
            break;
        case EF_BLACKFILTER:
            ec->dat.uc8[0] = 11;
            ec->dat.uc8[1] = fl;
            ec->dat.uc8[2] = (u_char) va_arg(ap, int);
            break;
        case EF_NEGA:
            ec->dat.uc8[0] = 12;
            ec->dat.uc8[1] = fl;
            ec->dat.uc8[2] = (u_char) va_arg(ap, int);
            ec->dat.uc8[3] = (u_char) va_arg(ap, int);

            if (fl & 4)
            {
                ec->in = va_arg(ap, u_int);
                ec->keep = va_arg(ap, u_int);
                ec->out = va_arg(ap, u_int);
                ec->cnt = 0;
                ec->flow =
                    (ec->in == 0 ? (ec->keep == 0 ? ec->out != 0 ? 2 : 3 : 1)
                                 : 0);
            }
            else
            {
                ec->pnt[0] = va_arg(ap, void *);
            }
            break;
        case EF_NCONTRAST:
            ec->dat.uc8[0] = 13;
            ec->dat.uc8[1] = fl;
            ec->dat.uc8[2] = (u_char) va_arg(ap, int);
            ec->dat.uc8[3] = (u_char) va_arg(ap, int);
            break;
        case EF_NCONTRAST2:
            ec->dat.uc8[0] = 14;
            ec->dat.uc8[1] = fl;
            ec->dat.uc8[2] = (u_char) va_arg(ap, int);
            ec->dat.uc8[3] = (u_char) va_arg(ap, int);
            break;
        case EF_NCONTRAST3:
            ec->dat.uc8[0] = 15;
            ec->dat.uc8[1] = fl;
            ec->dat.uc8[2] = (u_char) va_arg(ap, int);
            ec->dat.uc8[3] = (u_char) va_arg(ap, int);
            break;
        case EF_MAGATOKI:
            ec->dat.uc8[0] = 16;
            ec->dat.uc8[1] = fl;
            ec->dat.uc8[2] = fl;
            ec->flow = 0;
            ec->in = va_arg(ap, u_int);
            ec->out = va_arg(ap, u_int);
            ec->pnt[0] = va_arg(ap, void *);
            break;
        case EF_ENEDMG1:
            ec->dat.uc8[0] = 17;
            ec->dat.uc8[1] = fl;
            ec->dat.uc8[2] = (char) va_arg(ap, int);
            break;
        case EF_ENEDMG2:
            ec->dat.uc8[0] = 18;
            ec->dat.uc8[1] = fl;
            ec->dat.uc8[2] = (char) va_arg(ap, int);
            break;
        case EF_SPIRIT:
            ec->dat.uc8[0] = 19;
            ec->dat.uc8[1] = fl;
            ec->flow = va_arg(ap, int);
            ec->pnt[0] = va_arg(ap, void *);
            ec->dat.uc8[2] = (u_char) va_arg(ap, int);
            ec->dat.uc8[3] = (u_char) va_arg(ap, int);
            ec->dat.uc8[4] = (u_char) va_arg(ap, int);
            ec->dat.fl32[2] = va_arg(ap, double);
            ec->dat.uc8[5] = (u_char) va_arg(ap, int);
            ec->dat.uc8[6] = (u_char) va_arg(ap, int);
            ec->dat.uc8[7] = (u_char) va_arg(ap, int);
            ec->dat.fl32[3] = va_arg(ap, double);
            ec->pnt[1] = va_arg(ap, void *);
            ec->cnt = 0;
            ec->max = 0;
            ec->keep = 0;
            break;
        case EF_PBLUR:
            ec->dat.uc8[0] = 38;
            ec->dat.uc8[1] = fl;
            ec->dat.uc8[2] = (u_char) va_arg(ap, int);
            break;
        case EF_LUMINE:
            ec->dat.uc8[0] = 41;
            ec->dat.uc8[1] = fl;
            ec->pnt[0] = va_arg(ap, void *);
            break;
        case EF_FIRE:
            ec->dat.uc8[0] = 23;
            ec->dat.uc8[1] = fl | 0x80;
            ec->flow = (char) va_arg(ap, int);
            ec->pnt[0] = va_arg(ap, void *);
            ec->dat.uc8[2] = (u_char) va_arg(ap, int);
            ec->dat.uc8[3] = (u_char) va_arg(ap, int);
            ec->dat.uc8[4] = (u_char) va_arg(ap, int);
            ec->dat.fl32[2] = va_arg(ap, double);
            ec->dat.uc8[5] = (u_char) va_arg(ap, int);
            ec->dat.uc8[6] = (u_char) va_arg(ap, int);
            ec->dat.uc8[7] = (u_char) va_arg(ap, int);
            ec->dat.fl32[3] = va_arg(ap, double);

            if (ec->flow == 3)
            {
                float r;

                // inlined from effect.h
                r = vu0Rand();
                // end of inlined section
                ec->cnt = r * 21.0f;
            }
            else
            {
                ec->cnt = 0;
            }
            break;
        case EF_FIRE2:
            ec->dat.uc8[0] = 24;
            ec->dat.uc8[1] = fl | 0x80;
            ec->flow = va_arg(ap, int);
            ec->pnt[0] = va_arg(ap, void *);
            ec->dat.uc8[2] = (u_char) va_arg(ap, int);
            ec->dat.uc8[3] = (u_char) va_arg(ap, int);
            ec->dat.uc8[4] = (u_char) va_arg(ap, int);
            ec->dat.fl32[2] = va_arg(ap, double);
            ec->dat.uc8[5] = (u_char) va_arg(ap, int);
            ec->dat.uc8[6] = (u_char) va_arg(ap, int);
            ec->dat.uc8[7] = (u_char) va_arg(ap, int);
            ec->dat.fl32[3] = va_arg(ap, double);
            ec->fw[0] = va_arg(ap, double);

            if (ec->flow == 3)
            {
                float r;
                // inlined from effect.h
                r = vu0Rand();
                // end of inlined section
                ec->cnt = r * 21.0f;
            }
            else
            {
                ec->cnt = 0;
            }
            break;
        case EF_HALO:
            ec->dat.uc8[0] = 20;
            ec->dat.uc8[1] = fl | 0x80;
            ec->dat.uc8[2] = (u_char) va_arg(ap, int);
            ec->pnt[0] = va_arg(ap, void *);
            ec->dat.uc8[3] = (u_char) va_arg(ap, int);
            ec->dat.uc8[5] = (u_char) va_arg(ap, int);
            ec->dat.fl32[2] = va_arg(ap, double);
            ec->dat.uc8[6] = (u_char) va_arg(ap, int);
            break;
        case EF_RIPPLE:
            ec->dat.uc8[0] = 21;
            ec->dat.uc8[1] = fl;
            ec->dat.uc8[2] = (u_char) va_arg(ap, int);
            ec->dat.uc8[3] = (u_char) va_arg(ap, int);
            ec->dat.uc8[4] = 0;
            ec->dat.uc8[7] = 0;
            ec->pnt[0] = va_arg(ap, void *);
            break;
        case EF_RIPPLE2:
            ec->dat.uc8[0] = 22;
            ec->dat.uc8[1] = fl;
            ec->dat.uc8[2] = (u_char) va_arg(ap, int);
            ec->dat.uc8[3] = (u_char) va_arg(ap, int);
            ec->cnt = 0;
            ec->dat.uc8[4] = (u_char) va_arg(ap, int);
            ec->dat.uc8[5] = (u_char) va_arg(ap, int);
            ec->dat.uc8[6] = (u_char) va_arg(ap, int);
            ec->dat.uc8[7] = 0;
            ec->dat.fl32[2] = va_arg(ap, double);
            ec->dat.fl32[3] = va_arg(ap, double);
            ec->pnt[0] = va_arg(ap, void *);
            ec->pnt[1] = va_arg(ap, void *);

            if (fl & 8)
            {
                ec->dat.uc8[7] = (u_char) va_arg(ap, int);
            }
            break;
        case EF_PDEFORM:
            ec->dat.uc8[0] = 27;
            ec->dat.uc8[1] = fl | 0x80;
            ec->dat.uc8[2] = (u_char) va_arg(ap, int);
            ec->max = va_arg(ap, u_int);
            ec->dat.uc8[4] = 0xff;
            ec->dat.fl32[2] = va_arg(ap, double);
            ec->dat.fl32[3] = va_arg(ap, double);
            ec->pnt[0] = va_arg(ap, void *);
            ec->fw[0] = 0.0f;
            ec->cnt = 0;
            ec->in = va_arg(ap, u_int);
            ec->keep = va_arg(ap, u_int);
            ec->out = va_arg(ap, u_int);
            ec->pnt[1] = va_arg(ap, void *);
            ec->pnt[2] = va_arg(ap, void *);
            ec->pnt[4] = va_arg(ap, void *);
            ec->pnt[5] = NULL;
            ec->pnt[5] = va_arg(ap, void *);

            if (fl & 4)
            {
                ec->flow =
                    (ec->in == 0 ? (ec->keep == 0 ? ec->out != 0 ? 2 : 3 : 1)
                                 : 0);
            }
            break;
        case EF_ENEIN:
            ec->dat.uc8[0] = 39;
            ec->dat.uc8[1] = fl | 0x80;
            ec->dat.uc8[2] = (u_char) va_arg(ap, int);
            ec->dat.uc8[3] = (u_char) va_arg(ap, int);
            ec->dat.uc8[4] = 0xff;
            ec->dat.fl32[3] = va_arg(ap, double);
            ec->pnt[0] = va_arg(ap, void *);
            ec->cnt = 0;
            ec->in = va_arg(ap, u_int);
            ec->keep = va_arg(ap, u_int);
            ec->out = va_arg(ap, u_int);
            ec->max = 100;

            if (fl & 4)
            {
                ec->flow =
                    (ec->in == 0 ? (ec->keep == 0 ? ec->out != 0 ? 2 : 3 : 1)
                                 : 0);
            }
            break;
        case EF_ENEOUT:
            ec->dat.uc8[0] = 40;
            ec->dat.uc8[1] = fl;
            ec->dat.uc8[2] = 0;
            ec->dat.uc8[3] = 0;
            ec->dat.uc8[4] = 0;
            ec->dat.uc8[6] = (u_char) va_arg(ap, int);
            ec->dat.uc8[5] = (u_char) va_arg(ap, int);
            ec->dat.uc8[7] = 0;
            ec->fw[0] = 2.0f;
            ec->fw[1] = 0.0f;
            ec->fw[2] = va_arg(ap, double);
            ec->pnt[0] = NULL;
            break;
        case EF_DUST:
            ec->dat.uc8[0] = 29;
            ec->dat.uc8[1] = fl | 0x80;
            ec->dat.uc8[2] = 1;
            ec->pnt[0] = va_arg(ap, void *);
            break;
        case EF_WATERDROP:
            ec->dat.uc8[0] = 30;
            ec->dat.uc8[1] = fl | 0x80;
            ec->pnt[0] = va_arg(ap, void *);
            ec->dat.uc8[5] = (u_char) va_arg(ap, int);
            ec->dat.fl32[2] = va_arg(ap, double);
            ec->dat.ui32[3] = 0;
            ec->cnt = ec->max = va_arg(ap, u_int);
            ec->dat.uc8[2] = (u_char) va_arg(ap, int);
            ec->dat.uc8[3] = (u_char) va_arg(ap, int);
            ec->dat.uc8[4] = (u_char) va_arg(ap, int);
            ec->dat.uc8[6] = 0;
            break;
        case EF_SUNSHINE:
            ec->dat.uc8[0] = 31;
            ec->dat.uc8[1] = fl;
            ec->pnt[0] = va_arg(ap, void *);
            ec->pnt[1] = va_arg(ap, void *);
            ec->pnt[2] = va_arg(ap, void *);
            ec->max = va_arg(ap, u_int);
            ec->dat.fl32[2] = va_arg(ap, double);
            ec->dat.fl32[3] = va_arg(ap, double);
            ec->cnt = 0;
            ec->dat.uc8[2] = (u_char) va_arg(ap, int);
            ec->dat.uc8[3] = (u_char) va_arg(ap, int);
            ec->dat.uc8[4] = (u_char) va_arg(ap, int);
            break;
        case EF_ENEFIRE:
            ec->dat.uc8[0] = 28;
            ec->dat.uc8[1] = fl | 0x80;
            ec->dat.uc8[2] = (u_char) va_arg(ap, int);
            ec->pnt[0] = va_arg(ap, void *);
            ec->pnt[4] = va_arg(ap, void *);
            ec->pnt[1] = va_arg(ap, void *);
            ec->pnt[2] = va_arg(ap, void *);
            ec->pnt[3] = NULL;
            ec->dat.ui32[3] = va_arg(ap, u_int);
            ec->pnt[5] = va_arg(ap, void *);
            break;
        case EF_TORCH:
            ec->dat.uc8[0] = 25;
            ec->dat.uc8[1] = fl | 0x80;
            ec->dat.uc8[2] = (u_char) va_arg(ap, int);
            ec->dat.uc8[3] = 0;
            ec->pnt[0] = va_arg(ap, void *);
            ec->pnt[1] = NULL;
            ec->pnt[2] = va_arg(ap, void *);
            ec->pnt[3] = va_arg(ap, void *);
            break;
        case EF_SMOKE:
            ec->dat.uc8[0] = 26;
            ec->dat.uc8[1] = fl | 0x80;
            ec->pnt[0] = va_arg(ap, void *);
            ec->pnt[1] = NULL;
            break;
        case EF_NEGACIRCLE:
            ec->dat.uc8[0] = 32;
            ec->dat.uc8[1] = fl;
            ec->dat.fl32[1] = va_arg(ap, double);
            ec->dat.fl32[2] = va_arg(ap, double);
            ec->dat.fl32[3] = va_arg(ap, double);
            ec->dat.uc8[3] = (u_char) va_arg(ap, int);
            ec->in = va_arg(ap, u_int);
            ec->keep = va_arg(ap, u_int);
            ec->out = va_arg(ap, u_int);
            break;
        case EF_ENEFACE:
            ec->dat.uc8[0] = 33;
            ec->dat.uc8[1] = fl;
            ec->dat.uc8[2] = (u_char) va_arg(ap, int);
            ec->dat.uc8[3] = (u_char) va_arg(ap, int);
            ec->dat.fl32[1] = va_arg(ap, double);
            ec->dat.fl32[2] = va_arg(ap, double);
            ec->dat.fl32[3] = va_arg(ap, double);
            ec->cnt = 0;
            ec->out = 0;
            break;
        case EF_FACESPIRIT:
            ec->dat.uc8[1] = fl;
            ec->dat.uc8[0] = 34;
            ec->pnt[0] = va_arg(ap, void *);
            ec->dat.uc8[2] = (u_char) va_arg(ap, int);
            ec->dat.uc8[3] = (u_char) va_arg(ap, int);
            ec->dat.uc8[4] = (u_char) va_arg(ap, int);
            ec->pnt[1] = va_arg(ap, void *);
            ec->dat.uc8[5] = (u_char) va_arg(ap, int);
            ec->dat.uc8[6] = 0;
            ec->cnt = 0;
            ec->max = 0;
            ec->keep = 0;
            break;
        case EF_NULL:
            // do nothing ...
            break;
    }

    va_end(ap);

    return ec;
}

void ResetEffects(void *p)
{
    if (p != NULL)
    {
        *(u_char *) p = 0;
    }
}

int SearchEmptyEffectBuf()
{
    int i;
    int fl;
    EFFECT_CONT *ecm;

    fl = -1;

    if (change_efbank != 0)
    {
        ecm = efcntm;
    }
    else
    {
        ecm = efcntm_cnt;
    }

    for (i = 0; i < 48 && fl == -1; i++)
    {
        if (ecm[i].dat.uc8[0] == 0)
        {
            fl = i;
        }
    }

    return fl;
}

void EffectZSort()
{
    int i;
    int j;
    int num;
    sceVu0IVECTOR ivec;
    sceVu0FVECTOR vpos;
    sceVu0FVECTOR vtw;
    sceVu0FVECTOR wpos;
    sceVu0FMATRIX wlm;
    sceVu0FMATRIX slm;
    EFFECT_CONT *ecm;
    EFFECT_CONT *ec;
    static int efzsort[48];
    float *v1;

    memset(wpos, 0, sizeof(sceVu0FVECTOR));

    wpos[3] = 1.0;

    if (change_efbank != 0)
    {
        ecm = efcntm;
    }
    else
    {
        ecm = efcntm_cnt;
    }

    for (i = 0, num = 0; i < 48; i++)
    {
        if (ecm[i].dat.uc8[0] != 0)
        {
            if ((ecm[i].dat.uc8[1] & 0x80) != 0)
            {
                v1 = (float *)ecm[i].pnt[0];

                // inlined from ../../graphics/graph3d/libsg.h
                Vu0CopyVector(vpos, v1);
                // end of inlined section

                sceVu0UnitMatrix(wlm);

                wlm[0][0] = wlm[1][1] = wlm[2][2] = 25.0f;

                sceVu0TransMatrix(wlm, wlm, vpos);
                sceVu0MulMatrix(slm, SgWSMtx, wlm);
                sceVu0RotTransPers(ivec, slm, wpos, 0);

                ecm[i].z = ivec[2];
            }
            else
            {
                ecm[i].z = 0;
            }

            efzsort[num] = i;
            num++;
        }
    }

    for (j = 0; j < num - 1; j++)
    {

        for (i = j + 1; i < num; i++)
        {
            if (ecm[efzsort[i]].z < ecm[efzsort[j]].z)
            {
                int tmp = efzsort[i];
                efzsort[i] = efzsort[j];
                efzsort[j] = tmp;
            }
        }
    }

    for (i = 0; i < num; i++)
    {
        ec = &ecm[efzsort[i]];

        switch (ec->dat.uc8[0])
        {
            case EF_FIRE:
                SetFire(ec);
                break;
            case EF_FIRE2:
                SetFire2(ec);
                break;
            case EF_HALO:
                SetHalo(ec);
                break;
            case EF_RIPPLE:
            case EF_RIPPLE2:
                SetRipple(ec);
                break;
            case EF_PDEFORM:
            case EF_ENEIN:
                SetPartsDeform(ec);
                break;
            case EF_WATERDROP:
                SetWaterdrop(ec);
                break;
            case EF_SUNSHINE:
                SetSunshine(ec);
                break;
            case EF_TORCH:
                SetTorch(ec);
                break;
            case EF_SMOKE:
                SetSmoke(ec);
                break;
            case EF_ENEFACE:
                SetEneFace(ec);
                break;
            case EF_FACESPIRIT:
                SetFaceSpirit(ec);
                break;
        }
    }

    for (i = 0; i < num; i++)
    {
        ec = &ecm[efzsort[i]];

        switch (ec->dat.uc8[0])
        {
            case 29:
                SetDust(ec);
                break;
        }
    }

    SubRDFire3();
    RunRipple2();
}

void EffectZSort2()
{
    int i;
    int j;
    int num;
    sceVu0IVECTOR ivec;
    sceVu0FVECTOR vpos;
    sceVu0FVECTOR vtw;
    sceVu0FVECTOR wpos;
    sceVu0FMATRIX wlm;
    sceVu0FMATRIX slm;
    EFFECT_CONT *ecm;
    EFFECT_CONT *ec;
    static int efzsort[48];
    float *v1;

    memset(wpos, 0, sizeof(sceVu0FVECTOR));

    wpos[3] = 1.0;

    if (change_efbank != 0)
    {
        ecm = efcntm;
    }
    else
    {
        ecm = efcntm_cnt;
    }

    for (i = 0, num = 0; i < 48; i++)
    {
        if (ecm[i].dat.uc8[0] != 0)
        {
            if ((ecm[i].dat.uc8[1] & 0x80) != 0)
            {
                v1 = (float *)ecm[i].pnt[0];

                // inlined from ../../graphics/graph3d/libsg.h
                Vu0CopyVector(vpos, v1);
                // end of inlined section

                sceVu0UnitMatrix(wlm);

                wlm[0][0] = wlm[1][1] = wlm[2][2] = 25.0f;

                sceVu0TransMatrix(wlm, wlm, vpos);
                sceVu0MulMatrix(slm, SgWSMtx, wlm);
                sceVu0RotTransPers(ivec, slm, wpos, 0);

                ecm[i].z = ivec[2];
            }
            else
            {
                ecm[i].z = 0;
            }

            efzsort[num] = i;
            num++;
        }
    }

    for (j = 0; j < num - 1; j++)
    {

        for (i = j + 1; i < num; i++)
        {
            if (ecm[efzsort[i]].z < ecm[efzsort[j]].z)
            {
                int tmp = efzsort[i];
                efzsort[i] = efzsort[j];
                efzsort[j] = tmp;
            }
        }
    }

    for (i = 0; i < num; i++)
    {
        ec = &ecm[efzsort[i]];

        switch (ec->dat.uc8[0])
        {
            case EF_ENEOUT:
                SetEneSeal(ec);
                break;
            case EF_ENEFIRE:
                SetEneFire(ec);
                break;
        }
    }
}

void EffectZSort3()
{
    int i;
    int j;
    int num;
    sceVu0IVECTOR ivec;
    sceVu0FVECTOR vpos;
    sceVu0FVECTOR vtw;
    sceVu0FVECTOR wpos;
    sceVu0FMATRIX wlm;// world local matrix ?
    sceVu0FMATRIX slm;// screen local matrix ?
    EFFECT_CONT *ecm;
    EFFECT_CONT *ec;
    static int efzsort[48];
    float *v1;

    memset(wpos, 0, sizeof(sceVu0FVECTOR));

    wpos[3] = 1.0;

    if (change_efbank != 0)
    {
        ecm = efcntm;
    }
    else
    {
        ecm = efcntm_cnt;
    }

    for (i = 0, num = 0; i < 48; i++)
    {
        if (ecm[i].dat.uc8[0] != 0)
        {
            if ((ecm[i].dat.uc8[1] & 0x80) != 0)
            {
                v1 = (float *)ecm[i].pnt[0];

                // inlined from ../../graphics/graph3d/libsg.h
                Vu0CopyVector(vpos, v1);
                // end of inlined section

                sceVu0UnitMatrix(wlm);

                wlm[0][0] = wlm[1][1] = wlm[2][2] = 25.0f;

                sceVu0TransMatrix(wlm, wlm, vpos);
                sceVu0MulMatrix(slm, SgWSMtx, wlm);
                sceVu0RotTransPers(ivec, slm, wpos, 0);

                ecm[i].z = ivec[2];
            }
            else
            {
                ecm[i].z = 0;
            }

            efzsort[num] = i;
            num++;
        }
    }

    for (j = 0; j < num - 1; j++)
    {

        for (i = j + 1; i < num; i++)
        {
            if (ecm[efzsort[i]].z < ecm[efzsort[j]].z)
            {
                int tmp = efzsort[i];
                efzsort[i] = efzsort[j];
                efzsort[j] = tmp;
            }
        }
    }

    for (i = 0; i < num; i++)
    {
        ec = &ecm[efzsort[i]];
        switch (ec->dat.uc8[0])
        {
            case 32:
                SetNegaCircle(ec);
                break;
        }
    }
}

int CheckEffectScrBuffer(int eno)
{
    int ret;

    if (set_buffer[0] == eno)
    {
        return 0;
    }

    if ((set_buffer[0] & 0xf) == 0)
    {
        set_buffer[0] = eno;
        return 0;
    }

    return -1;
}

void ResetEffectScrBuffer(int eno)
{
    if ((set_buffer[0] & eno) != 0)
    {
        set_buffer[0] = 0;
    }
    else if ((set_buffer[1] & eno) != 0)
    {
        set_buffer[1] = 0;
    }
}

void EffectControl(int no)
{
    static int fl;
    static void *ecw[3];
    EFFECT_CONT *ecm;
    int n;

    if (change_efbank != 0)
    {
        ecm = efcnt;
    }
    else
    {
        ecm = efcnt_cnt;
    }

    switch (no)
    {
        case GRA2D_CALL_IG2:
            if (dbg_wrk.oth_pkt_num_sw != 0)
            {
                if (L1_PRESSED() != 0)
                {
                    for (n = 0; n < 64; n++)
                    {
                        SetString2(16, ((n / 16) * 80) + 10,
                                   ((n - ((n / 16) * 16)) * 16) + 20, 1, 0x80,
                                   0x80, 0x80, "%2d:%2d", n,
                                   efcnt_cnt[n].dat.uc8[0]);
                    }

                    for (n = 0; n < 0x30; n++)
                    {
                        SetString2(0x10, ((n / 16) * 80) + 0x168,
                                   ((n - ((n / 16) * 16)) * 16) + 20, 1, 0x80,
                                   0x80, 0x80, "%2d:%2d", n,
                                   efcntm_cnt[n].dat.uc8[0]);
                    }
                }
                else
                {
                    for (n = 0; n < 64; n++)
                    {
                        SetString2(0x10, ((n / 16) * 80) + 0xA,
                                   ((n - ((n / 16) * 16)) * 16) + 20, 1, 0x80,
                                   0x80, 0x80, "%2d:%2d", n,
                                   efcnt[n].dat.uc8[0]);
                    }
                    for (n = 0; n < 0x30; n++)
                    {
                        SetString2(0x10, ((n / 16) * 80) + 0x168,
                                   ((n - ((n / 16) * 16)) * 16) + 20, 1, 0x80,
                                   0x80, 0x80, "%2d:%2d", n,
                                   efcntm[n].dat.uc8[0]);
                    }
                }
            }

            pblur();
            SetSky();
            SetPond();
            SetHaze_Pond();
            SetFirefly();
            SetCanal();
            SetSaveCameraLamp();

            if (sys_wrk.game_mode != GAME_MODE_OUTGAME && tmp_effect_off == 0
                && ingame_wrk.mode != INGAME_MODE_WANDER_SOUL && ingame_wrk.game != 1)
            {
                CheckItemEffect();
            }

            RunLeaf();
            SetHaze_Pond();
            SetFirefly();
            DrawRareEne();
            tes_p10();

            if (ecm[EF_RENZFLARE].dat.uc8[0] == EF_RENZFLARE)
            {
                SetRenzFlare(&ecm[EF_RENZFLARE]);
            }

            EffectZSort();

            if (ecm[EF_SPIRIT].dat.uc8[0] == EF_SPIRIT)
            {
                SetSpirit(&ecm[EF_SPIRIT]);
            }

            SetMAGATOKI2();

            if (ecm[EF_MAGATOKI].dat.uc8[0] == EF_MAGATOKI)
            {
                SetMAGATOKI(&ecm[EF_MAGATOKI]);
            }

            if (ecm[EF_Z_DEP].dat.uc8[0] == EF_Z_DEP)
            {
                SetForcusDepth(&ecm[EF_Z_DEP]);
            }

            if (ecm[EF_Z_DEP2].dat.uc8[0] == EF_Z_DEP2)
            {
                SetForcusDepth2(&ecm[EF_Z_DEP2]);
            }

            if (ecm[EF_BLUR_N].dat.uc8[0] == EF_BLUR_N)
            {
                SetBlur(&ecm[EF_BLUR_N]);
            }

            if (ecm[EF_BLUR_B].dat.uc8[0] == EF_BLUR_B)
            {
                SetBlur(&ecm[EF_BLUR_B]);
            }

            if (ecm[EF_BLUR_W].dat.uc8[0] == EF_BLUR_W)
            {
                SetBlur(&ecm[EF_BLUR_W]);
            }

            RunBlur(&ecm[EF_BLUR_N]);

            if (ecm[EF_DEFORM].dat.uc8[0] == EF_DEFORM)
            {
                SetDeform(&ecm[EF_DEFORM]);
            }

            if (ecm[EF_NCONTRAST].dat.uc8[0] == EF_NCONTRAST)
            {
                SetContrast2(&ecm[EF_NCONTRAST]);
            }

            if (ecm[EF_NEGA].dat.uc8[0] == EF_NEGA)
            {
                SetNega(&ecm[EF_NEGA]);
            }

            if (ecm[EF_NCONTRAST3].dat.uc8[0] == EF_NCONTRAST3)
            {
                SetContrast3(&ecm[EF_NCONTRAST3]);
            }

            if (ecm[EF_FOCUS].dat.uc8[0] == EF_FOCUS)
            {
                SetFocus1(&ecm[EF_FOCUS]);
            }

            RunFocus(&ecm[EF_FOCUS]);

            if (ecm[EF_DITHER].dat.uc8[0] == EF_DITHER)
            {
                SetDither3(&ecm[EF_DITHER]);
            }

            if (ecm[EF_DITHER2].dat.uc8[0] == EF_DITHER2)
            {
                SetDither4(&ecm[EF_DITHER2]);
            }

            if (ecm[EF_BLACKFILTER].dat.uc8[0] == EF_BLACKFILTER)
            {
                SetBlackFilter(&ecm[EF_BLACKFILTER]);
            }

            if (ecm[EF_NCONTRAST2].dat.uc8[0] == EF_NCONTRAST2)
            {
                SetContrast2(&ecm[EF_NCONTRAST2]);
            }

            tes_p11();
            SetRoomDirecPazzEne();
            break;
        case GRA2D_CALL_IG31:
            SetEneDmgEffect1_Sub();
            SetEneDmgEffect2_Sub();
            RunCamStop();
            RunCamSlow();
            RunCamPush();
            tes_p20();
            EffectZSort2();
            RunCamSearch();
            RunCamView();
            tes_p21();
            EffectZSort3();
            return;
        case GRA2D_CALL_IG32:
            tes_p3();

            if (ecm[EF_FADEFRAME].dat.uc8[0] == EF_FADEFRAME)
            {
                SetFadeFrame(&ecm[EF_FADEFRAME]);
            }

            if (ecm[EF_OVERLAP].dat.uc8[0] == EF_OVERLAP)
            {
                SetOverRap(&ecm[EF_OVERLAP]);
            }

            ScreenCtrl();
            CamSave();
            return;
        case GRA2D_CALL_IG0:
        case GRA2D_CALL_IG1:
        case GRA2D_CALL_IG0E:
        case GRA2D_CALL_IG1E:
        case GRA2D_CALL_IG3:
            return;
    }
    return;
}

void SetBlurOff()
{
    eff_blur_off = 1;
}

void SetDebugMenuSwitch(int sw)
{
    look_debugmenu = sw % 2;
}

void pblur()
{
    sceVu0FVECTOR vpos;
    SPRITE_DATA sd;
    DRAW_ENV de;
    float fy;
    float ss1;
    float ss2;
    float xx;
    float yy;
    float zz;
    float l;
    float ll;
    static float d1 = 0.0f;

    if (dbg_wrk.eff_prtblr_sw != 0)
    {
        LocalCopyBtoL(1, 0x1a40);

        sd = SPRITE_DATA{
            .g_GsTex0 =
                {
                           .TBP0 = 0x1A40,
                           .TBW = 0xA,
                           .PSM = 0x0,
                           .TW = 0xA,
                           .TH = 0x8,
                           .TCC = 0x0,
                           .TFX = 0x0,
                           .CBP = 0x0,
                           .CPSM = 0x0,
                           .CSM = 0x0,
                           .CSA = 0x0,
                           .CLD = 0x1,
                           },
            .g_nTexSizeW = 0x280,
            .g_nTexSizeH = 0xE0,
            .g_bMipmapLv = 0x0,
            .g_GsMiptbp1 = 0,
            .g_GsMiptbp2 = 0,
            .pos_x = -321.5f,
            .pos_y = -224.5f,
            .pos_z = 0,
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
            .mask = 0
        };

        de = DRAW_ENV{
            .tex1 = 0x161,
            .alpha = 0x44,
            .zbuf = 0x10100008c,
            .test = 0x30003,
            .clamp = 0,
            .prim = 0x50ab400000008001,
        };

        fy = 0.0f;
        ll = 2.0f;

        // inlined from ../../graphics/graph3d/libsg.h
        Vu0CopyVector(vpos, plyr_wrk.move_box.pos);
        // end of inlined section

        xx = (camera.p[0] - vpos[0]) * (camera.p[0] - vpos[0]);
        yy = (camera.p[1] - vpos[1]) * (camera.p[1] - vpos[1]);
        zz = (camera.p[2] - vpos[2]) * (camera.p[2] - vpos[2]);
        l = SgSqrtf(xx + yy + zz);

        if (l != fy)
        {
            ll = 1000.0f / l;
        }

        ll = ll < 1.0f ? 1.0f : ll;

        ss1 = SgSinf(((d1 + d1) / 180.0f) * 3.1415927f) * ll;
        ss2 = SgSinf(((d1 * 3.0f) / 180.0f) * 3.1415927f) * ll;

        if ((ss1 < 0.4f && ss1 > -0.4f) && (ss2 < 0.4f && ss2 > -0.4f))
        {
            d1 += 0.08f;
        }
        else
        {
            d1 += 0.2f;
        }

        sd.pos_x = -320.5f + ss1;
        sd.pos_y = -224.5f + ss2;
        sd.pos_z = 0;
        sd.alpha = dbg_wrk.eff_prtblr_alp;

        SetTexDirectS2(0, &sd, &de, 0);
        {
            float eff_hw, eff_hh;
            MikuPan_GetFullScreenHalfExtent(&eff_hw, &eff_hh);
            SetPanel(0xffff800, 320.0f - eff_hw, 224.0f - eff_hh, 320.0f + eff_hw, 224.0f + eff_hh, 0, 0, 0, 0x80);
        }
        LocalCopyLtoB2(1, ((sys_wrk.count + 1 & 1) * 0x23 << 0x26) >> 0x20);
    }
}

void TmpEffectOff(int id)
{
    if (id != 0)
    {
        tmp_effect_off = 1;
    }
    else
    {
        tmp_effect_off = 0;
    }
}

void tes_p10()
{
    return;
}

void tes_p11()
{
    return;
}

/// Tests the ContHeatHaze Effect On Player
void tes_p20()
{
    static void *ene_fire_work = NULL;
    sceVu0FVECTOR fire_pos;
    sceVu0FVECTOR fire_pos2;
    float fx;
    float fy;
    float fz;
    float l;
    float alp;
    static float off_x = 0.0f;
    static float off_z = 0.0f;

    if (dbg_wrk.eff_new_ene_ef_sw != 0)
    {
        if (ingame_wrk.mode != INGAME_MODE_MENU && plyr_wrk.mode != 1)
        {
            alp = dbg_wrk.eff_new_ene_ef_alp;

            Vu0CopyVector(fire_pos, plyr_wrk.move_box.pos);

            fx = (fire_pos[0] - camera.p[0]) * (fire_pos[0] - camera.p[0]);
            fy = (fire_pos[1] - camera.p[1]) * (fire_pos[1] - camera.p[1]);
            fz = (fire_pos[2] - camera.p[2]) * (fire_pos[2] - camera.p[2]);
            l = SgSqrtf(fx + fy + fz);

            fire_pos[0] =
                camera.p[0] + ((fire_pos[0] - camera.p[0]) * (l - 200.0f)) / l;
            fire_pos[1] =
                camera.p[1] + ((fire_pos[1] - camera.p[1]) * (l - 200.0f)) / l;
            fire_pos[2] =
                camera.p[2] + ((fire_pos[2] - camera.p[2]) * (l - 200.0f)) / l;

            fire_pos[3] = 1.0f;

            Vu0CopyVector(fire_pos2, fire_pos);

            fire_pos2[0] += off_x;
            fire_pos2[1] -= 700.0f;
            fire_pos2[2] += off_z;

            ene_fire_work = GetEnePartAddr(ene_fire_work, 11, 160);
            ene_fire_work = ContHeatHaze(
                ene_fire_work, 11, fire_pos, fire_pos2, 0, 128.0f, 128.0f,
                128.0f, alp, dbg_wrk.eff_new_ene_ef_sz * 100.0f, 1.0f);
        }
    }
    else
    {
        if (ene_fire_work != NULL)
        {
            ene_fire_work = NULL;
        }
    }
}

int change_efbank = 0;
int eff_blur_off = 0;
int effect_disp_flg = 0;
int eff_filament_off = 0;
int now_buffer[2] = {0, 0};
int eff_dith_off = 0;
int stop_effects = 0;
int monochrome_mode = 0;
int magatoki_mode = 0;
int shibata_set_off = 0;
int look_debugmenu = 0;

void tes_p21()
{
    return;
}

void tes_p3()
{
    return;
}
