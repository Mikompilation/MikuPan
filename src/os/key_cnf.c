#include <string.h>

#include "common.h"
#include "typedefs.h"

#include "enums.h"
#include "main/glob.h"
#include "os/key_cnf.h"

u_short *default_key[32] = {
    &pad[0].cnt[0],  &pad[0].cnt[1],  &pad[0].cnt[2],  &pad[0].cnt[3],
    &pad[0].cnt[4],  &pad[0].cnt[5],  &pad[0].cnt[6],  &pad[0].cnt[7],
    &pad[0].cnt[8],  &pad[0].cnt[9],  &pad[0].cnt[10], &pad[0].cnt[11],
    &pad[0].cnt[12], &pad[0].cnt[13], &pad[0].cnt[14], &pad[0].cnt[15],
    &pad[1].cnt[0],  &pad[1].cnt[1],  &pad[1].cnt[2],  &pad[1].cnt[3],
    &pad[1].cnt[4],  &pad[1].cnt[5],  &pad[1].cnt[6],  &pad[1].cnt[7],
    &pad[1].cnt[8],  &pad[1].cnt[9],  &pad[1].cnt[10], &pad[1].cnt[11],
    &pad[1].cnt[12], &pad[1].cnt[13], &pad[1].cnt[14], &pad[1].cnt[15],
};
u_short *default_key_bak[32] = {
    &pad[0].cnt_bak[0],  &pad[0].cnt_bak[1],  &pad[0].cnt_bak[2],  &pad[0].cnt_bak[3],
    &pad[0].cnt_bak[4],  &pad[0].cnt_bak[5],  &pad[0].cnt_bak[6],  &pad[0].cnt_bak[7],
    &pad[0].cnt_bak[8],  &pad[0].cnt_bak[9],  &pad[0].cnt_bak[10], &pad[0].cnt_bak[11],
    &pad[0].cnt_bak[12], &pad[0].cnt_bak[13], &pad[0].cnt_bak[14], &pad[0].cnt_bak[15],
    &pad[1].cnt_bak[0],  &pad[1].cnt_bak[1],  &pad[1].cnt_bak[2],  &pad[1].cnt_bak[3],
    &pad[1].cnt_bak[4],  &pad[1].cnt_bak[5],  &pad[1].cnt_bak[6],  &pad[1].cnt_bak[7],
    &pad[1].cnt_bak[8],  &pad[1].cnt_bak[9],  &pad[1].cnt_bak[10], &pad[1].cnt_bak[11],
    &pad[1].cnt_bak[12], &pad[1].cnt_bak[13], &pad[1].cnt_bak[14], &pad[1].cnt_bak[15],
};
u_char key_type[32] = {
    0,  1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 11, 12, 13, 14, 15,
    16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31,
};

static u_short unmapped_key_now = 0;
static u_short unmapped_key_bak = 0;

static const u_char default_action_profile[MIKUPAN_ACTION_PROFILE_ACTION_COUNT] = {
    0,  1,  2,  3,  4,  5,  6,  7,
    8,  9,  10, 11, 12, 13, 14, 15,
};

static const char *action_profile_target_labels[MIKUPAN_ACTION_PROFILE_ACTION_COUNT] = {
    "DPad Up",
    "DPad Down",
    "DPad Left",
    "DPad Right",
    "Triangle",
    "Cross",
    "Square",
    "Circle",
    "L1",
    "L2",
    "R1",
    "R2",
    "Start",
    "Select",
    "L3",
    "R3",
};

static const char *normal_action_labels[MIKUPAN_ACTION_PROFILE_ACTION_COUNT] = {
    "Move / Aim Up",
    "Move / Aim Down",
    "Move / Aim Left",
    "Move / Aim Right",
    "Menu",
    "Search / Obtain",
    "Run",
    "Finder ON",
    "Special Functions",
    "Eye View / L2",
    "Shutter / R1",
    "Eye View / R2",
    "Pause",
    "Select",
    "L3",
    "R3",
};

static const char *finder_action_labels[MIKUPAN_ACTION_PROFILE_ACTION_COUNT] = {
    "Finder Aim Up",
    "Finder Aim Down",
    "Finder Aim Left",
    "Finder Aim Right",
    "Turn Around",
    "Shutter",
    "Quick Turn",
    "Finder OFF",
    "Special Functions",
    "Finder Control / L2",
    "Shutter",
    "Finder Control / R2",
    "Pause",
    "Select",
    "L3",
    "R3",
};

static u_char custom_action_profile[MIKUPAN_ACTION_PROFILE_MODE_COUNT]
                                   [MIKUPAN_ACTION_PROFILE_ACTION_COUNT] = {
    {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15},
    {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15},
};

static int custom_action_profile_enabled = 0;
static int custom_action_profile_dpad_subjective_move = 0;
static int custom_action_profile_stick_subjective_move = 0;
static int custom_action_profile_last_subjective_move = 0;
static int custom_action_profile_finder_reverse_y = 0;
/* Default to the modern twin-stick finder layout: left stick / WASD moves the
 * character, right stick / mouse aims the camera. Flip to 0 for the classic
 * layout (left stick aims, right stick moves). */
static int custom_action_profile_finder_swap_sticks = 1;

static int IsFinderProfileMode(void)
{
    return plyr_wrk.mode == PMODE_FINDER ||
           plyr_wrk.mode == PMODE_FINDER_IN ||
           plyr_wrk.mode == PMODE_FINDER_END ||
           plyr_wrk.mode == PMODE_FIN_CAM;
}

static int ClampActionProfileMode(int mode)
{
    if (mode != MIKUPAN_ACTION_PROFILE_MODE_FINDER)
    {
        return MIKUPAN_ACTION_PROFILE_MODE_NORMAL;
    }

    return MIKUPAN_ACTION_PROFILE_MODE_FINDER;
}

static int NormalMoveDpadActive(void)
{
    return key_now[0] != NULL && key_now[1] != NULL &&
           key_now[2] != NULL && key_now[3] != NULL &&
           (DPAD_UP_PRESSED() != 0 || DPAD_DOWN_PRESSED() != 0 ||
            DPAD_LEFT_PRESSED() != 0 || DPAD_RIGHT_PRESSED() != 0);
}

static int NormalMoveStickActive(void)
{
    return pad[0].id == 0x79 && pad[0].an_dir[0] != 0xff;
}

static u_char ClampActionProfileTarget(int target)
{
    if (target < 0 || target >= MIKUPAN_ACTION_PROFILE_ACTION_COUNT)
    {
        return 0xff;
    }

    return (u_char) target;
}

static void ApplyKeyMap(const u_char *map)
{
    int i;

    for (i = 0; i < MIKUPAN_ACTION_PROFILE_ACTION_COUNT; i++)
    {
        u_char target = map[i];
        key_type[i] = target;

        if (target < MIKUPAN_ACTION_PROFILE_ACTION_COUNT)
        {
            key_now[i] = default_key[target];
            key_bak[i] = default_key_bak[target];
        }
        else
        {
            key_now[i] = &unmapped_key_now;
            key_bak[i] = &unmapped_key_bak;
        }
    }

    for (i = MIKUPAN_ACTION_PROFILE_ACTION_COUNT; i < 32; i++)
    {
        key_type[i] = (u_char)i;
        key_now[i] = default_key[i];
        key_bak[i] = default_key_bak[i];
    }
}

void SetDefaultKeyType()
{
    ApplyKeyMap(default_action_profile);
}

void SetKeyType()
{
    const u_char *map;

    if (custom_action_profile_enabled)
    {
        map = custom_action_profile[IsFinderProfileMode()
                                        ? MIKUPAN_ACTION_PROFILE_MODE_FINDER
                                        : MIKUPAN_ACTION_PROFILE_MODE_NORMAL];
    }
    else
    {
        map = default_action_profile;
    }

    ApplyKeyMap(map);
}

void MikuPan_ResetCustomActionProfile(void)
{
    int mode;

    for (mode = 0; mode < MIKUPAN_ACTION_PROFILE_MODE_COUNT; mode++)
    {
        memcpy(custom_action_profile[mode], default_action_profile,
               sizeof(default_action_profile));
    }

    custom_action_profile_dpad_subjective_move = 0;
    custom_action_profile_stick_subjective_move = 0;
    custom_action_profile_last_subjective_move = 0;
    custom_action_profile_finder_reverse_y = 0;
    custom_action_profile_finder_swap_sticks = 1;
    SetKeyType();
}

void MikuPan_SetCustomActionProfileEnabled(int enabled)
{
    custom_action_profile_enabled = enabled ? 1 : 0;
    SetKeyType();
}

int MikuPan_IsCustomActionProfileEnabled(void)
{
    return custom_action_profile_enabled;
}

void MikuPan_SetCustomActionProfileSubjectiveMove(int enabled)
{
    custom_action_profile_dpad_subjective_move = enabled ? 1 : 0;
    custom_action_profile_stick_subjective_move = enabled ? 1 : 0;
}

int MikuPan_CustomActionProfileUsesSubjectiveMove(void)
{
    return custom_action_profile_dpad_subjective_move &&
           custom_action_profile_stick_subjective_move;
}

void MikuPan_SetCustomActionProfileDpadSubjectiveMove(int enabled)
{
    custom_action_profile_dpad_subjective_move = enabled ? 1 : 0;
}

int MikuPan_CustomActionProfileUsesDpadSubjectiveMove(void)
{
    return custom_action_profile_dpad_subjective_move;
}

void MikuPan_SetCustomActionProfileStickSubjectiveMove(int enabled)
{
    custom_action_profile_stick_subjective_move = enabled ? 1 : 0;
}

int MikuPan_CustomActionProfileUsesStickSubjectiveMove(void)
{
    return custom_action_profile_stick_subjective_move;
}

void MikuPan_SetCustomActionProfileFinderReverseY(int enabled)
{
    custom_action_profile_finder_reverse_y = enabled ? 1 : 0;
}

int MikuPan_CustomActionProfileUsesFinderReverseY(void)
{
    return custom_action_profile_finder_reverse_y;
}

void MikuPan_SetCustomActionProfileFinderSwapSticks(int enabled)
{
    custom_action_profile_finder_swap_sticks = enabled ? 1 : 0;
}

int MikuPan_CustomActionProfileSwapsFinderSticks(void)
{
    return custom_action_profile_finder_swap_sticks;
}

int MikuPan_KeyProfileUsesSubjectiveMove(void)
{
    if (custom_action_profile_enabled)
    {
        if (NormalMoveStickActive())
        {
            custom_action_profile_last_subjective_move =
                custom_action_profile_stick_subjective_move;
            return custom_action_profile_last_subjective_move;
        }
        if (NormalMoveDpadActive())
        {
            custom_action_profile_last_subjective_move =
                custom_action_profile_dpad_subjective_move;
            return custom_action_profile_last_subjective_move;
        }
        return custom_action_profile_last_subjective_move;
    }

    return opt_wrk.key_type == 1 || opt_wrk.key_type == 3 ||
           opt_wrk.key_type == 5 || opt_wrk.key_type == 7;
}

int MikuPan_KeyProfileUsesFinderReverseY(void)
{
    if (custom_action_profile_enabled)
    {
        return custom_action_profile_finder_reverse_y;
    }

    return opt_wrk.key_type == 4 || opt_wrk.key_type == 5 ||
           opt_wrk.key_type == 6 || opt_wrk.key_type == 7;
}

int MikuPan_KeyProfileSwapsFinderSticks(void)
{
    return custom_action_profile_finder_swap_sticks;
}

int MikuPan_KeyProfileUsesFinderShoulderToggle(void)
{
    if (custom_action_profile_enabled)
    {
        return 0;
    }

    return opt_wrk.key_type == 2 || opt_wrk.key_type == 3 ||
           opt_wrk.key_type == 6 || opt_wrk.key_type == 7;
}

int MikuPan_GetCustomActionProfileTarget(int mode, int action)
{
    mode = ClampActionProfileMode(mode);
    if (action < 0 || action >= MIKUPAN_ACTION_PROFILE_ACTION_COUNT)
    {
        return MIKUPAN_ACTION_PROFILE_TARGET_NONE;
    }

    u_char target = custom_action_profile[mode][action];
    return target < MIKUPAN_ACTION_PROFILE_ACTION_COUNT
               ? target
               : MIKUPAN_ACTION_PROFILE_TARGET_NONE;
}

void MikuPan_SetCustomActionProfileTarget(int mode, int action, int target)
{
    mode = ClampActionProfileMode(mode);
    if (action < 0 || action >= MIKUPAN_ACTION_PROFILE_ACTION_COUNT)
    {
        return;
    }

    custom_action_profile[mode][action] = ClampActionProfileTarget(target);
    SetKeyType();
}

int MikuPan_GetDefaultActionProfileTarget(int action)
{
    if (action < 0 || action >= MIKUPAN_ACTION_PROFILE_ACTION_COUNT)
    {
        return MIKUPAN_ACTION_PROFILE_TARGET_NONE;
    }

    return default_action_profile[action];
}

const char *MikuPan_ActionProfileModeLabel(int mode)
{
    return ClampActionProfileMode(mode) == MIKUPAN_ACTION_PROFILE_MODE_FINDER
               ? "Finder Mode"
               : "Normal Mode";
}

const char *MikuPan_ActionProfileActionLabel(int mode, int action)
{
    if (action < 0 || action >= MIKUPAN_ACTION_PROFILE_ACTION_COUNT)
    {
        return "<unknown action>";
    }

    return ClampActionProfileMode(mode) == MIKUPAN_ACTION_PROFILE_MODE_FINDER
               ? finder_action_labels[action]
               : normal_action_labels[action];
}

const char *MikuPan_ActionProfileTargetLabel(int target)
{
    if (target < 0 || target >= MIKUPAN_ACTION_PROFILE_ACTION_COUNT)
    {
        return "<unmapped>";
    }

    return action_profile_target_labels[target];
}
