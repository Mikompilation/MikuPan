#include "mikupan_controller.h"
#include "SDL3/SDL_scancode.h"
#include "mikupan/mikupan_config.h"
#include "mikupan/debug/mikupan_logging_c.h"
#include "mikupan/mikupan_utils.h"
#include "mikupan/gameplay/mikupan_item_icon_hud.h"
#include "main/glob.h"
#include "os/key_cnf.h"
#include "enums.h"
#include <SDL3/SDL_keyboard.h>
#include <SDL3/SDL_mouse.h>

#define CIMGUI_DEFINE_ENUMS_AND_STRUCTS
#include "cimgui.h"
#include "mikupan/ui/mikupan_ui.h"
#include "mikupan/ui/mikupan_rml_options.h"
#include "mikupan/ui/mikupan_rml_save_load.h"
#include "mikupan/ui/mikupan_rml_save_point.h"

#include <stdio.h>

static int remap_target = -1;
static int remap_target_kb = 0;

/// Active stick remap row (0..MIKUPAN_STICK_COUNT-1) or -1 when idle.
static int remap_stick_target = -1;

/// 0 = remapping the gamepad axis, 1 = remapping keyboard neg key,
/// 2 = remapping keyboard pos key. Only meaningful while remap_stick_target >= 0.
static int remap_stick_mode = 0;

SDL_Gamepad *mikupan_gamepad = NULL;
static int mikupan_preferred_gamepad_index = MIKUPAN_CONTROLLER_AUTO_INDEX;
static unsigned short mikupan_rumble_low = 0;
static unsigned short mikupan_rumble_high = 0;

static void MikuPan_ControllerStopRumble(void)
{
    if (mikupan_gamepad != NULL && (mikupan_rumble_low != 0 || mikupan_rumble_high != 0))
    {
        SDL_RumbleGamepad(mikupan_gamepad, 0, 0, 0);
    }

    mikupan_rumble_low = 0;
    mikupan_rumble_high = 0;
}

/* Which device's bindings the mapping UI shows: 1 = keyboard & mouse, 0 = the
 * selected gamepad. -1 until first drawn, then defaulted from whether a gamepad
 * is present. This is a UI view only -- in-game both devices are always read. */
static int mikupan_input_view_keyboard = -1;

#define MIKUPAN_FINDER_MOUSE_SENS_MIN 0.1f
#define MIKUPAN_FINDER_MOUSE_SENS_MAX 5.0f

/// Finder mouse-look runtime state. Defaults are kept here so a fresh config
/// (which leaves these zeroed) still starts with mouse-look on at 1.0x.
static int finder_mouse_enabled = 1;
static float finder_mouse_sensitivity = 1.0f;
static int finder_mouse_requested = 0;
static int finder_mouse_active = 0;
static int camera_activation_mode = MIKUPAN_CAMERA_ACTIVATION_HOLD;
static int special_action_previous_down[MIKUPAN_SPECIAL_ACTION_COUNT] = {};
static int camera_toggle_press_pending = 0;
static int camera_hold_engaged = 0;
static int camera_hold_release_pending = 0;
static int special_film_swap_direction = 0;
static int runtime_wheel_steps = 0;
static MikuPan_InputBinding pending_capture_binding = {};
static int pending_capture_binding_valid = 0;

static int MikuPan_ControllerConfigActionProfileTarget(int layout, int target);

static const char *mikupan_controller_labels[MIKUPAN_CONTROLLER_LOGICAL_COUNT] = {
    "Triangle",
    "Cross",
    "Square",
    "Circle",
    "DPad Up",
    "DPad Down",
    "DPad Left",
    "DPad Right",
    "R3 (Right Stick)",
    "Select",
    "Start",
    "L3 (Left Stick)",
    "R1 / RB",
    "L2 / LT",
    "R2 / RT",
    "L1 / LB",
};

static const char *mikupan_special_action_labels[MIKUPAN_SPECIAL_ACTION_COUNT] = {
    "Raise Camera",
    "Take Photo",
    "Previous Film",
    "Next Film",
    "Special Ability",
    "Run",
};

static const MikuPan_ControllerBindings mikupan_controller_map_defaults[MIKUPAN_CONTROLLER_LOGICAL_COUNT] = {
    {MIKUPAN_CONTROLLER_BIND_BUTTON,          SDL_GAMEPAD_BUTTON_NORTH},
    {MIKUPAN_CONTROLLER_BIND_BUTTON,          SDL_GAMEPAD_BUTTON_SOUTH},
    {MIKUPAN_CONTROLLER_BIND_BUTTON,           SDL_GAMEPAD_BUTTON_WEST},
    {MIKUPAN_CONTROLLER_BIND_BUTTON,           SDL_GAMEPAD_BUTTON_EAST},
    {MIKUPAN_CONTROLLER_BIND_BUTTON,        SDL_GAMEPAD_BUTTON_DPAD_UP},
    {MIKUPAN_CONTROLLER_BIND_BUTTON,      SDL_GAMEPAD_BUTTON_DPAD_DOWN},
    {MIKUPAN_CONTROLLER_BIND_BUTTON,      SDL_GAMEPAD_BUTTON_DPAD_LEFT},
    {MIKUPAN_CONTROLLER_BIND_BUTTON,     SDL_GAMEPAD_BUTTON_DPAD_RIGHT},
    {MIKUPAN_CONTROLLER_BIND_BUTTON,    SDL_GAMEPAD_BUTTON_RIGHT_STICK},
    {MIKUPAN_CONTROLLER_BIND_BUTTON,           SDL_GAMEPAD_BUTTON_BACK},
    {MIKUPAN_CONTROLLER_BIND_BUTTON,          SDL_GAMEPAD_BUTTON_START},
    {MIKUPAN_CONTROLLER_BIND_BUTTON,     SDL_GAMEPAD_BUTTON_LEFT_STICK},
    {MIKUPAN_CONTROLLER_BIND_BUTTON, SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER},
    {  MIKUPAN_CONTROLLER_BIND_AXIS,     SDL_GAMEPAD_AXIS_LEFT_TRIGGER},
    {  MIKUPAN_CONTROLLER_BIND_AXIS,    SDL_GAMEPAD_AXIS_RIGHT_TRIGGER},
    {MIKUPAN_CONTROLLER_BIND_BUTTON,  SDL_GAMEPAD_BUTTON_LEFT_SHOULDER},
};

/* Keyboard defaults clustered around the left hand so they pair with WASD
 * movement (the stick defaults below) and mouse aiming. Order matches
 * mikupan_controller_labels:
 *   Triangle=E  Cross=Space  Square=Unbound  Circle=C
 *   DPad=Arrows  R3=V  Select=Tab  Start=Esc  L3=Unbound
 *   R1=F (finder)  L2=Z / R2=X (finder zoom out/in)  L1=R */
static const int mikupan_keyboard_map_defaults[MIKUPAN_CONTROLLER_LOGICAL_COUNT] = {
    SDL_SCANCODE_E,    SDL_SCANCODE_SPACE,  0,
    SDL_SCANCODE_C,    SDL_SCANCODE_UP,     SDL_SCANCODE_DOWN,
    SDL_SCANCODE_LEFT, SDL_SCANCODE_RIGHT,  SDL_SCANCODE_V,
    SDL_SCANCODE_TAB,  SDL_SCANCODE_ESCAPE, 0,
    SDL_SCANCODE_F,    SDL_SCANCODE_Z,      SDL_SCANCODE_X,
    SDL_SCANCODE_R,
};

MikuPan_ControllerBindings mikupan_controller_map[MIKUPAN_CONTROLLER_LOGICAL_COUNT] = {
    {MIKUPAN_CONTROLLER_BIND_BUTTON,          SDL_GAMEPAD_BUTTON_NORTH},
    {MIKUPAN_CONTROLLER_BIND_BUTTON,          SDL_GAMEPAD_BUTTON_SOUTH},
    {MIKUPAN_CONTROLLER_BIND_BUTTON,           SDL_GAMEPAD_BUTTON_WEST},
    {MIKUPAN_CONTROLLER_BIND_BUTTON,           SDL_GAMEPAD_BUTTON_EAST},
    {MIKUPAN_CONTROLLER_BIND_BUTTON,        SDL_GAMEPAD_BUTTON_DPAD_UP},
    {MIKUPAN_CONTROLLER_BIND_BUTTON,      SDL_GAMEPAD_BUTTON_DPAD_DOWN},
    {MIKUPAN_CONTROLLER_BIND_BUTTON,      SDL_GAMEPAD_BUTTON_DPAD_LEFT},
    {MIKUPAN_CONTROLLER_BIND_BUTTON,     SDL_GAMEPAD_BUTTON_DPAD_RIGHT},
    {MIKUPAN_CONTROLLER_BIND_BUTTON,    SDL_GAMEPAD_BUTTON_RIGHT_STICK},
    {MIKUPAN_CONTROLLER_BIND_BUTTON,           SDL_GAMEPAD_BUTTON_BACK},
    {MIKUPAN_CONTROLLER_BIND_BUTTON,          SDL_GAMEPAD_BUTTON_START},
    {MIKUPAN_CONTROLLER_BIND_BUTTON,     SDL_GAMEPAD_BUTTON_LEFT_STICK},
    {MIKUPAN_CONTROLLER_BIND_BUTTON, SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER},
    {  MIKUPAN_CONTROLLER_BIND_AXIS,     SDL_GAMEPAD_AXIS_LEFT_TRIGGER},
    {  MIKUPAN_CONTROLLER_BIND_AXIS,    SDL_GAMEPAD_AXIS_RIGHT_TRIGGER},
    {MIKUPAN_CONTROLLER_BIND_BUTTON,  SDL_GAMEPAD_BUTTON_LEFT_SHOULDER},
};

/// rdata-byte slot for each logical stick axis (LX/LY/RX/RY).
static const int mikupan_stick_rdata_byte[MIKUPAN_STICK_COUNT] = {6, 7, 4, 5};

int mikupan_keyboard_map[MIKUPAN_CONTROLLER_LOGICAL_COUNT] = {
    SDL_SCANCODE_E,    SDL_SCANCODE_SPACE,  0,
    SDL_SCANCODE_C,    SDL_SCANCODE_UP,     SDL_SCANCODE_DOWN,
    SDL_SCANCODE_LEFT, SDL_SCANCODE_RIGHT,  SDL_SCANCODE_V,
    SDL_SCANCODE_TAB,  SDL_SCANCODE_ESCAPE, 0,
    SDL_SCANCODE_F,    SDL_SCANCODE_Z,      SDL_SCANCODE_X,
    SDL_SCANCODE_R,
};

static const char *mikupan_stick_labels[MIKUPAN_STICK_COUNT] = {
    "Left Stick X",
    "Left Stick Y",
    "Right Stick X",
    "Right Stick Y",
};

static const MikuPan_StickGamepadBinding mikupan_stick_controller_map_defaults[MIKUPAN_STICK_COUNT] = {
    {SDL_GAMEPAD_AXIS_LEFTX,  0},
    {SDL_GAMEPAD_AXIS_LEFTY,  0},
    {SDL_GAMEPAD_AXIS_RIGHTX, 0},
    {SDL_GAMEPAD_AXIS_RIGHTY, 0},
};

static const MikuPan_StickKeyboardBinding mikupan_stick_keyboard_map_defaults[MIKUPAN_STICK_COUNT] = {
    {SDL_SCANCODE_A, SDL_SCANCODE_D},
    {SDL_SCANCODE_W, SDL_SCANCODE_S},
    {0,              0             },
    {0,              0             },
};

MikuPan_StickGamepadBinding mikupan_stick_controller_map[MIKUPAN_STICK_COUNT] = {
    {SDL_GAMEPAD_AXIS_LEFTX,  0},
    {SDL_GAMEPAD_AXIS_LEFTY,  0},
    {SDL_GAMEPAD_AXIS_RIGHTX, 0},
    {SDL_GAMEPAD_AXIS_RIGHTY, 0},
};

MikuPan_StickKeyboardBinding mikupan_stick_keyboard_map[MIKUPAN_STICK_COUNT] = {
    {SDL_SCANCODE_A, SDL_SCANCODE_D},
    {SDL_SCANCODE_W, SDL_SCANCODE_S},
    {0,              0             },
    {0,              0             },
};

static const MikuPan_InputBinding mikupan_special_action_map_defaults[MIKUPAN_SPECIAL_ACTION_COUNT] = {
    {MIKUPAN_INPUT_BIND_MOUSE_BUTTON, SDL_BUTTON_RIGHT},
    {MIKUPAN_INPUT_BIND_MOUSE_BUTTON, SDL_BUTTON_LEFT},
    {MIKUPAN_INPUT_BIND_MOUSE_WHEEL, 1},
    {MIKUPAN_INPUT_BIND_MOUSE_WHEEL, -1},
    {MIKUPAN_INPUT_BIND_MOUSE_BUTTON, SDL_BUTTON_MIDDLE},
    {MIKUPAN_INPUT_BIND_KEYBOARD, SDL_SCANCODE_LSHIFT},
};

MikuPan_InputBinding mikupan_special_action_map[MIKUPAN_SPECIAL_ACTION_COUNT] = {
    {MIKUPAN_INPUT_BIND_MOUSE_BUTTON, SDL_BUTTON_RIGHT},
    {MIKUPAN_INPUT_BIND_MOUSE_BUTTON, SDL_BUTTON_LEFT},
    {MIKUPAN_INPUT_BIND_MOUSE_WHEEL, 1},
    {MIKUPAN_INPUT_BIND_MOUSE_WHEEL, -1},
    {MIKUPAN_INPUT_BIND_MOUSE_BUTTON, SDL_BUTTON_MIDDLE},
    {MIKUPAN_INPUT_BIND_KEYBOARD, SDL_SCANCODE_LSHIFT},
};

static MikuPan_InputBinding MikuPan_NormalizeInputBinding(
    MikuPan_InputBinding binding)
{
    if (binding.kind == MIKUPAN_INPUT_BIND_KEYBOARD
        && binding.code > 0 && binding.code < SDL_SCANCODE_COUNT)
    {
        return binding;
    }

    if (binding.kind == MIKUPAN_INPUT_BIND_MOUSE_BUTTON
        && binding.code >= SDL_BUTTON_LEFT && binding.code <= SDL_BUTTON_X2)
    {
        return binding;
    }

    if (binding.kind == MIKUPAN_INPUT_BIND_MOUSE_WHEEL && binding.code != 0)
    {
        binding.code = binding.code > 0 ? 1 : -1;
        return binding;
    }

    return {MIKUPAN_INPUT_BIND_NONE, 0};
}

u_short mikupan_controller_game_bitmask[MIKUPAN_CONTROLLER_LOGICAL_COUNT] = {
    0x1000,  0x4000,  0x8000,  0x2000,
    0x10,    0x40,    0x80,    0x20,
    0x4,     0x1,     0x8,     0x2,
    0x800,   0x100,   0x200,   0x400,
};

static void MikuPan_CloseCurrentGamepad(void)
{
    if (mikupan_gamepad != NULL)
    {
        SDL_RumbleGamepad(mikupan_gamepad, 0, 0, 0);
        SDL_CloseGamepad(mikupan_gamepad);
        mikupan_gamepad = NULL;
    }

    mikupan_rumble_low = 0;
    mikupan_rumble_high = 0;
}

void MikuPan_ControllerSetPreferredGamepadIndex(int index)
{
    if (index < MIKUPAN_CONTROLLER_AUTO_INDEX)
    {
        index = MIKUPAN_CONTROLLER_AUTO_INDEX;
    }

    if (mikupan_preferred_gamepad_index == index)
    {
        return;
    }

    mikupan_preferred_gamepad_index = index;
    MikuPan_CloseCurrentGamepad();
    MikuPan_OpenController();
}

int MikuPan_ControllerGetPreferredGamepadIndex(void)
{
    return mikupan_preferred_gamepad_index;
}

void MikuPan_ControllerProcessEvent(const SDL_Event *event)
{
    if (event == NULL)
    {
        return;
    }

    if (event->type == SDL_EVENT_KEY_DOWN && !event->key.repeat)
    {
        pending_capture_binding.kind = MIKUPAN_INPUT_BIND_KEYBOARD;
        pending_capture_binding.code = event->key.scancode;
        pending_capture_binding_valid = 1;
    }
    else if (event->type == SDL_EVENT_MOUSE_BUTTON_DOWN)
    {
        pending_capture_binding.kind = MIKUPAN_INPUT_BIND_MOUSE_BUTTON;
        pending_capture_binding.code = event->button.button;
        pending_capture_binding_valid = 1;
    }
    else if (event->type == SDL_EVENT_MOUSE_WHEEL && event->wheel.y != 0.0f)
    {
        const int direction = event->wheel.y > 0.0f ? 1 : -1;
        runtime_wheel_steps += direction;
        if (runtime_wheel_steps > 8)
        {
            runtime_wheel_steps = 8;
        }
        else if (runtime_wheel_steps < -8)
        {
            runtime_wheel_steps = -8;
        }
        pending_capture_binding.kind = MIKUPAN_INPUT_BIND_MOUSE_WHEEL;
        pending_capture_binding.code = direction;
        pending_capture_binding_valid = 1;
    }
}

void MikuPan_InputBindingCaptureReset(void)
{
    pending_capture_binding = {};
    pending_capture_binding_valid = 0;
    runtime_wheel_steps = 0;
}

int MikuPan_InputBindingCaptureAnyDown(void)
{
    const bool *keys = SDL_GetKeyboardState(NULL);
    if (keys != NULL)
    {
        for (int i = 4; i < SDL_SCANCODE_COUNT; i++)
        {
            if (keys[i])
            {
                return 1;
            }
        }
    }

    const SDL_MouseButtonFlags buttons = SDL_GetMouseState(NULL, NULL);
    return buttons != 0 ? 1 : 0;
}

int MikuPan_InputBindingCapturePoll(MikuPan_InputBinding *binding)
{
    if (binding == NULL)
    {
        return 0;
    }

    if (pending_capture_binding_valid)
    {
        *binding = pending_capture_binding;
        pending_capture_binding_valid = 0;
        return 1;
    }

    const bool *keys = SDL_GetKeyboardState(NULL);
    if (keys != NULL)
    {
        for (int i = 4; i < SDL_SCANCODE_COUNT; i++)
        {
            if (keys[i])
            {
                binding->kind = MIKUPAN_INPUT_BIND_KEYBOARD;
                binding->code = i;
                return 1;
            }
        }
    }

    const SDL_MouseButtonFlags buttons = SDL_GetMouseState(NULL, NULL);
    for (int button = SDL_BUTTON_LEFT; button <= SDL_BUTTON_X2; button++)
    {
        if ((buttons & SDL_BUTTON_MASK(button)) != 0)
        {
            binding->kind = MIKUPAN_INPUT_BIND_MOUSE_BUTTON;
            binding->code = button;
            return 1;
        }
    }

    return 0;
}

static const char *MikuPan_ControllerNameForId(SDL_JoystickID id)
{
    const char *name = SDL_GetGamepadNameForID(id);
    return (name != NULL && name[0] != '\0') ? name : "<unknown controller>";
}

static int MikuPan_ControllerFindGamepadIndex(SDL_JoystickID *ids, int count,
                                              SDL_JoystickID id)
{
    for (int i = 0; i < count; i++)
    {
        if (ids[i] == id)
        {
            return i;
        }
    }

    return -1;
}

void MikuPan_ControllerDrawDeviceSelectorUi(void)
{
    int num_gamepads = 0;
    SDL_JoystickID *joysticks_id = SDL_GetGamepads(&num_gamepads);

    if (mikupan_input_view_keyboard < 0)
    {
        mikupan_input_view_keyboard = (num_gamepads > 0) ? 0 : 1;
    }

    char preview[160];
    if (mikupan_input_view_keyboard)
    {
        snprintf(preview, sizeof(preview), "Keyboard & Mouse");
    }
    else if (mikupan_preferred_gamepad_index == MIKUPAN_CONTROLLER_AUTO_INDEX)
    {
        snprintf(preview, sizeof(preview), "Auto (first available controller)");
    }
    else if (joysticks_id != NULL &&
             mikupan_preferred_gamepad_index >= 0 &&
             mikupan_preferred_gamepad_index < num_gamepads)
    {
        snprintf(preview, sizeof(preview), "%d: %s",
                 mikupan_preferred_gamepad_index + 1,
                 MikuPan_ControllerNameForId(joysticks_id[mikupan_preferred_gamepad_index]));
    }
    else
    {
        snprintf(preview, sizeof(preview), "%d: <not connected>",
                 mikupan_preferred_gamepad_index + 1);
    }

    if (igBeginCombo("Input device", preview, 0))
    {
        const bool auto_selected =
            !mikupan_input_view_keyboard &&
            mikupan_preferred_gamepad_index == MIKUPAN_CONTROLLER_AUTO_INDEX;
        if (igSelectable_Bool("Auto (first available controller)", auto_selected, 0, ImVec2{0, 0}))
        {
            mikupan_input_view_keyboard = 0;
            MikuPan_ControllerSetPreferredGamepadIndex(MIKUPAN_CONTROLLER_AUTO_INDEX);
        }
        if (auto_selected)
        {
            igSetItemDefaultFocus();
        }

        const bool kbm_selected = mikupan_input_view_keyboard != 0;
        if (igSelectable_Bool("Keyboard & Mouse", kbm_selected, 0, ImVec2{0, 0}))
        {
            mikupan_input_view_keyboard = 1;
        }
        if (kbm_selected)
        {
            igSetItemDefaultFocus();
        }

        for (int i = 0; i < num_gamepads; i++)
        {
            char label[160];
            snprintf(label, sizeof(label), "%d: %s", i + 1,
                     MikuPan_ControllerNameForId(joysticks_id[i]));

            const bool is_selected =
                !mikupan_input_view_keyboard && mikupan_preferred_gamepad_index == i;
            if (igSelectable_Bool(label, is_selected, 0, ImVec2{0, 0}))
            {
                mikupan_input_view_keyboard = 0;
                MikuPan_ControllerSetPreferredGamepadIndex(i);
            }
            if (is_selected)
            {
                igSetItemDefaultFocus();
            }
        }

        igEndCombo();
    }

    igTextDisabled("Keyboard/mouse and controller are always active together; "
                   "this selects which bindings to edit below.");

    if (joysticks_id != NULL)
    {
        SDL_free(joysticks_id);
    }
}

int MikuPan_OpenController()
{
    if (mikupan_gamepad != NULL && !SDL_GamepadConnected(mikupan_gamepad))
    {
        MikuPan_CloseCurrentGamepad();
    }

    if (mikupan_gamepad != NULL || !SDL_HasGamepad())
    {
        return 0;
    }

    int num_gamepad = 0;
    SDL_JoystickID *joysticks_id = SDL_GetGamepads(&num_gamepad);

    if (joysticks_id == NULL)
    {
        return 0;
    }

    int selected_index = mikupan_preferred_gamepad_index;
    if (selected_index == MIKUPAN_CONTROLLER_AUTO_INDEX)
    {
        selected_index = 0;
    }

    if (selected_index < 0 || selected_index >= num_gamepad)
    {
        SDL_free(joysticks_id);
        return 0;
    }

    mikupan_gamepad = SDL_OpenGamepad(joysticks_id[selected_index]);
    if (mikupan_gamepad != NULL)
    {
        const char *name = SDL_GetGamepadName(mikupan_gamepad);
        const char *safe_name = (name != NULL && name[0] != '\0') ? name : "<unknown controller>";
        info_log("Controller %d connected: %s",
                 selected_index + 1, safe_name);
    }
    else
    {
        info_log("Failed to open controller %d: %s",
                 selected_index + 1, SDL_GetError());
    }

    SDL_free(joysticks_id);

    return mikupan_gamepad != NULL ? 1 : 0;
}

void MikuPan_ControllerResetBindings(void)
{
    for (int i = 0; i < MIKUPAN_CONTROLLER_LOGICAL_COUNT; i++)
    {
        mikupan_controller_map[i] = mikupan_controller_map_defaults[i];
        mikupan_keyboard_map[i] = mikupan_keyboard_map_defaults[i];
    }
    for (int i = 0; i < MIKUPAN_STICK_COUNT; i++)
    {
        mikupan_stick_controller_map[i] = mikupan_stick_controller_map_defaults[i];
        mikupan_stick_keyboard_map[i] = mikupan_stick_keyboard_map_defaults[i];
    }
    for (int i = 0; i < MIKUPAN_SPECIAL_ACTION_COUNT; i++)
    {
        mikupan_special_action_map[i] = mikupan_special_action_map_defaults[i];
        special_action_previous_down[i] = 0;
    }
    camera_activation_mode = MIKUPAN_CAMERA_ACTIVATION_HOLD;
    camera_toggle_press_pending = 0;
    camera_hold_engaged = 0;
    camera_hold_release_pending = 0;
    special_film_swap_direction = 0;
    runtime_wheel_steps = 0;
    MikuPan_InputBindingCaptureReset();
    finder_mouse_enabled = 1;
    finder_mouse_sensitivity = 1.0f;
    MikuPan_SetControllerRumbleEnabled(1);
    MikuPan_SetFinderDpadFilmSwapEnabled(0);
    MikuPan_SetMirrorStoneHudEnabled(0);
    MikuPan_ResetCustomActionProfile();
    MikuPan_SetCustomActionProfileEnabled(0);
}

void MikuPan_ControllerStoreBindingsToConfig(void)
{
    MikuPan_ConfigInput *cfg = &mikupan_configuration.input;

    for (int i = 0; i < MIKUPAN_CONTROLLER_LOGICAL_COUNT; i++)
    {
        cfg->controller_kind[i]   = mikupan_controller_map[i].kind;
        cfg->controller_code[i]   = mikupan_controller_map[i].code;
        cfg->keyboard_scancode[i] = mikupan_keyboard_map[i];
    }
    for (int i = 0; i < MIKUPAN_STICK_COUNT; i++)
    {
        cfg->stick_axis[i]   = mikupan_stick_controller_map[i].axis;
        cfg->stick_invert[i] = mikupan_stick_controller_map[i].invert;
        cfg->stick_kb_neg[i] = mikupan_stick_keyboard_map[i].neg_scancode;
        cfg->stick_kb_pos[i] = mikupan_stick_keyboard_map[i].pos_scancode;
    }
    cfg->action_profile_saved = 1;
    cfg->action_profile_layout = 2;
    cfg->action_profile_enabled =
        MikuPan_IsCustomActionProfileEnabled();
    cfg->action_profile_subjective_move =
        MikuPan_CustomActionProfileUsesSubjectiveMove();
    cfg->action_profile_dpad_subjective_move =
        MikuPan_CustomActionProfileUsesDpadSubjectiveMove();
    cfg->action_profile_stick_subjective_move =
        MikuPan_CustomActionProfileUsesStickSubjectiveMove();
    cfg->movement_style_override_enabled =
        MikuPan_MovementStyleOverrideEnabled();
    cfg->action_profile_finder_reverse_y =
        MikuPan_CustomActionProfileUsesFinderReverseY();
    cfg->action_profile_finder_swap_sticks =
        MikuPan_CustomActionProfileSwapsFinderSticks();
    cfg->finder_dpad_film_swap_enabled =
        MikuPan_FinderDpadFilmSwapEnabled();
    cfg->mirror_stone_hud_enabled = MikuPan_MirrorStoneHudEnabled();
    cfg->improved_movement_collisions_enabled =
        MikuPan_ImprovedMovementCollisionsEnabled();
    cfg->finder_mouse_enabled = finder_mouse_enabled;
    cfg->finder_mouse_sensitivity = finder_mouse_sensitivity;
    cfg->special_bindings_saved = 1;
    cfg->special_bindings_count = MIKUPAN_SPECIAL_ACTION_COUNT;
    cfg->camera_activation_mode = camera_activation_mode;
    for (int i = 0; i < MIKUPAN_SPECIAL_ACTION_COUNT; i++)
    {
        cfg->special_bind_kind[i] = mikupan_special_action_map[i].kind;
        cfg->special_bind_code[i] = mikupan_special_action_map[i].code;
    }
    cfg->rumble_enabled = MikuPan_ControllerRumbleEnabled();
    for (int i = 0; i < MIKUPAN_ACTION_PROFILE_ACTION_COUNT; i++)
    {
        cfg->action_profile_normal[i] =
            MikuPan_GetCustomActionProfileTarget(
                MIKUPAN_ACTION_PROFILE_MODE_NORMAL, i);
        cfg->action_profile_finder[i] =
            MikuPan_GetCustomActionProfileTarget(
                MIKUPAN_ACTION_PROFILE_MODE_FINDER, i);
    }

    cfg->bindings_saved = 1;
}

void MikuPan_ControllerLoadBindingsFromConfig(void)
{
    const MikuPan_ConfigInput *cfg = &mikupan_configuration.input;

    MikuPan_SetControllerRumbleEnabled(cfg->rumble_enabled);
    MikuPan_ResetCustomActionProfile();
    MikuPan_SetCustomActionProfileEnabled(0);

    /* Only apply when the config actually holds saved bindings; otherwise keep
     * the runtime defaults set up at init. */
    if (!cfg->bindings_saved)
    {
        return;
    }

    for (int i = 0; i < MIKUPAN_CONTROLLER_LOGICAL_COUNT; i++)
    {
        mikupan_controller_map[i].kind = cfg->controller_kind[i];
        mikupan_controller_map[i].code = cfg->controller_code[i];
        mikupan_keyboard_map[i]        = cfg->keyboard_scancode[i];
    }
    for (int i = 0; i < MIKUPAN_STICK_COUNT; i++)
    {
        mikupan_stick_controller_map[i].axis         = cfg->stick_axis[i];
        mikupan_stick_controller_map[i].invert       = cfg->stick_invert[i];
        mikupan_stick_keyboard_map[i].neg_scancode   = cfg->stick_kb_neg[i];
        mikupan_stick_keyboard_map[i].pos_scancode   = cfg->stick_kb_pos[i];
    }
    if (cfg->special_bindings_saved)
    {
        int saved_count = cfg->special_bindings_count;
        if (saved_count <= 0)
        {
            saved_count = MIKUPAN_SPECIAL_ACTION_RUN;
        }
        if (saved_count > MIKUPAN_SPECIAL_ACTION_COUNT)
        {
            saved_count = MIKUPAN_SPECIAL_ACTION_COUNT;
        }

        for (int i = 0; i < MIKUPAN_SPECIAL_ACTION_COUNT; i++)
        {
            if (i < saved_count)
            {
                mikupan_special_action_map[i] = MikuPan_NormalizeInputBinding({
                    cfg->special_bind_kind[i],
                    cfg->special_bind_code[i],
                });
            }
            else
            {
                mikupan_special_action_map[i] = mikupan_special_action_map_defaults[i];
            }
        }

        if (saved_count <= MIKUPAN_SPECIAL_ACTION_RUN)
        {
            if (mikupan_keyboard_map[2] == SDL_SCANCODE_Q
                || mikupan_keyboard_map[2] == SDL_SCANCODE_R)
            {
                mikupan_keyboard_map[2] = 0;
            }
            if (mikupan_keyboard_map[11] == SDL_SCANCODE_LSHIFT)
            {
                mikupan_keyboard_map[11] = 0;
            }
        }
    }
    else
    {
        for (int i = 0; i < MIKUPAN_SPECIAL_ACTION_COUNT; i++)
        {
            mikupan_special_action_map[i] = mikupan_special_action_map_defaults[i];
        }

        if (mikupan_keyboard_map[2] == SDL_SCANCODE_Q
            || mikupan_keyboard_map[2] == SDL_SCANCODE_R)
        {
            mikupan_keyboard_map[2] = 0;
        }
        if (mikupan_keyboard_map[11] == SDL_SCANCODE_LSHIFT)
        {
            mikupan_keyboard_map[11] = 0;
        }
    }
    MikuPan_SetCameraActivationMode(cfg->camera_activation_mode);
    /* A sensitivity of 0 means the config predates this option, so keep the
     * runtime defaults instead of disabling mouse-look on older configs. */
    if (cfg->finder_mouse_sensitivity > 0.0f)
    {
        finder_mouse_enabled = cfg->finder_mouse_enabled ? 1 : 0;
        MikuPan_SetFinderMouseSensitivity(cfg->finder_mouse_sensitivity);
    }
    MikuPan_SetFinderDpadFilmSwapEnabled(
        cfg->finder_dpad_film_swap_enabled);
    MikuPan_SetMirrorStoneHudEnabled(cfg->mirror_stone_hud_enabled);
    if (cfg->action_profile_saved)
    {
        if (cfg->action_profile_layout >= 2)
        {
            MikuPan_SetCustomActionProfileDpadSubjectiveMove(
                cfg->action_profile_dpad_subjective_move);
            MikuPan_SetCustomActionProfileStickSubjectiveMove(
                cfg->action_profile_stick_subjective_move);
        }
        else
        {
            MikuPan_SetCustomActionProfileSubjectiveMove(
                cfg->action_profile_subjective_move);
        }
        MikuPan_SetMovementStyleOverrideEnabled(
            cfg->movement_style_override_enabled);
        MikuPan_SetCustomActionProfileFinderReverseY(
            cfg->action_profile_finder_reverse_y);
        MikuPan_SetCustomActionProfileFinderSwapSticks(
            cfg->action_profile_finder_swap_sticks);
        for (int i = 0; i < MIKUPAN_ACTION_PROFILE_ACTION_COUNT; i++)
        {
            MikuPan_SetCustomActionProfileTarget(
                MIKUPAN_ACTION_PROFILE_MODE_NORMAL, i,
                MikuPan_ControllerConfigActionProfileTarget(
                    cfg->action_profile_layout,
                    cfg->action_profile_normal[i]));
            MikuPan_SetCustomActionProfileTarget(
                MIKUPAN_ACTION_PROFILE_MODE_FINDER, i,
                MikuPan_ControllerConfigActionProfileTarget(
                    cfg->action_profile_layout,
                    cfg->action_profile_finder[i]));
        }
    }
    MikuPan_SetCustomActionProfileEnabled(
        cfg->action_profile_saved ? cfg->action_profile_enabled : 0);
}


int MikuPan_ImprovedMovementCollisionsEnabled(void)
{
    return mikupan_configuration.input.improved_movement_collisions_enabled;
}

void MikuPan_SetImprovedMovementCollisionsEnabled(int enabled)
{
    mikupan_configuration.input.improved_movement_collisions_enabled = enabled ? 1 : 0;
}

int MikuPan_CameraActivationMode(void)
{
    return camera_activation_mode;
}

void MikuPan_SetCameraActivationMode(int mode)
{
    camera_activation_mode = mode == MIKUPAN_CAMERA_ACTIVATION_TOGGLE
                                 ? MIKUPAN_CAMERA_ACTIVATION_TOGGLE
                                 : MIKUPAN_CAMERA_ACTIVATION_HOLD;
    camera_toggle_press_pending = 0;
    camera_hold_engaged = 0;
    camera_hold_release_pending = 0;
}

int MikuPan_ConsumeSpecialFilmSwapDirection(void)
{
    const int direction = special_film_swap_direction;
    special_film_swap_direction = 0;
    return direction;
}

static int MikuPan_ScancodeBoundToSpecialAction(int scancode)
{
    for (int i = 0; i < MIKUPAN_SPECIAL_ACTION_COUNT; i++)
    {
        const MikuPan_InputBinding binding = mikupan_special_action_map[i];
        if (binding.kind == MIKUPAN_INPUT_BIND_KEYBOARD
            && binding.code == scancode)
        {
            return 1;
        }
    }

    return 0;
}

static int MikuPan_InputBindingDown(MikuPan_InputBinding binding,
                                    const bool *key_states,
                                    SDL_MouseButtonFlags mouse_buttons)
{
    if (binding.kind == MIKUPAN_INPUT_BIND_KEYBOARD)
    {
        return key_states != NULL && binding.code > 0
               && binding.code < SDL_SCANCODE_COUNT
               && key_states[binding.code];
    }

    if (binding.kind == MIKUPAN_INPUT_BIND_MOUSE_BUTTON)
    {
        return binding.code >= SDL_BUTTON_LEFT && binding.code <= SDL_BUTTON_X2
               && (mouse_buttons & SDL_BUTTON_MASK(binding.code)) != 0;
    }

    return 0;
}

static int MikuPan_IsFinderInputMode(void)
{
    return plyr_wrk.mode == PMODE_FINDER
           || plyr_wrk.mode == PMODE_FINDER_IN
           || plyr_wrk.mode == PMODE_FINDER_END
           || plyr_wrk.mode == PMODE_FIN_CAM;
}

static int MikuPan_SpecialActionsEnabled(void)
{
    if (sys_wrk.game_mode != GAME_MODE_INGAME
        || ingame_wrk.mode != INGAME_MODE_NOMAL
        || MikuPan_UiIsMenuOpen()
        || MikuPan_RmlOptionsIsOpen()
        || MikuPan_RmlSaveLoadIsOpen()
        || MikuPan_RmlSavePointInputEnabled())
    {
        return 0;
    }

    ImGuiIO *io = igGetIO_Nil();
    return io == NULL
           || (!io->WantCaptureMouse && !io->WantCaptureKeyboard
               && !io->WantTextInput);
}

static int MikuPan_ActionTargetToControllerLogical(int target)
{
    static const int target_to_logical[MIKUPAN_ACTION_PROFILE_ACTION_COUNT] = {
        4, 5, 6, 7, 0, 1, 2, 3, 15, 13, 12, 14, 10, 9, 11, 8,
    };

    if (target < 0 || target >= MIKUPAN_ACTION_PROFILE_ACTION_COUNT)
    {
        return -1;
    }

    return target_to_logical[target];
}

static void MikuPan_InjectSpecialAction(int *pressed_buttons, int action)
{
    const int mode = MikuPan_IsFinderInputMode()
                         ? MIKUPAN_ACTION_PROFILE_MODE_FINDER
                         : MIKUPAN_ACTION_PROFILE_MODE_NORMAL;
    const int target = MikuPan_IsCustomActionProfileEnabled()
                           ? MikuPan_GetCustomActionProfileTarget(mode, action)
                           : MikuPan_GetDefaultActionProfileTarget(action);
    const int logical = MikuPan_ActionTargetToControllerLogical(target);
    if (logical >= 0 && logical < MIKUPAN_CONTROLLER_LOGICAL_COUNT)
    {
        pressed_buttons[logical] = 1;
    }
}

static void MikuPan_ApplySpecialActions(int *pressed_buttons,
                                        const bool *key_states)
{
    const SDL_MouseButtonFlags mouse_buttons = SDL_GetMouseState(NULL, NULL);
    int wheel_direction = 0;
    if (runtime_wheel_steps > 0)
    {
        wheel_direction = 1;
        runtime_wheel_steps--;
    }
    else if (runtime_wheel_steps < 0)
    {
        wheel_direction = -1;
        runtime_wheel_steps++;
    }

    int down[MIKUPAN_SPECIAL_ACTION_COUNT] = {};
    int activated[MIKUPAN_SPECIAL_ACTION_COUNT] = {};
    for (int i = 0; i < MIKUPAN_SPECIAL_ACTION_COUNT; i++)
    {
        const MikuPan_InputBinding binding = mikupan_special_action_map[i];
        down[i] = MikuPan_InputBindingDown(binding, key_states, mouse_buttons);
        activated[i] = (down[i] && !special_action_previous_down[i])
                       || (binding.kind == MIKUPAN_INPUT_BIND_MOUSE_WHEEL
                           && binding.code == wheel_direction);
    }

    const MikuPan_InputBinding camera_binding =
        mikupan_special_action_map[MIKUPAN_SPECIAL_ACTION_RAISE_CAMERA];
    const int camera_released =
        special_action_previous_down[MIKUPAN_SPECIAL_ACTION_RAISE_CAMERA]
        && !down[MIKUPAN_SPECIAL_ACTION_RAISE_CAMERA];

    if (!MikuPan_SpecialActionsEnabled())
    {
        if (camera_activation_mode == MIKUPAN_CAMERA_ACTIVATION_HOLD
            && camera_binding.kind != MIKUPAN_INPUT_BIND_MOUSE_WHEEL
            && camera_hold_engaged && camera_released)
        {
            if (MikuPan_IsFinderInputMode())
            {
                camera_hold_release_pending = 1;
            }
            else
            {
                camera_hold_engaged = 0;
                camera_hold_release_pending = 0;
            }
        }

        if (sys_wrk.game_mode != GAME_MODE_INGAME
            || (!MikuPan_IsFinderInputMode()
                && plyr_wrk.mode != PMODE_NORMAL))
        {
            camera_hold_engaged = 0;
            camera_hold_release_pending = 0;
        }

        for (int i = 0; i < MIKUPAN_SPECIAL_ACTION_COUNT; i++)
        {
            special_action_previous_down[i] = down[i];
        }
        camera_toggle_press_pending = 0;
        special_film_swap_direction = 0;
        runtime_wheel_steps = 0;
        return;
    }
    int camera_pressed = 0;
    if (camera_activation_mode == MIKUPAN_CAMERA_ACTIVATION_TOGGLE
        || camera_binding.kind == MIKUPAN_INPUT_BIND_MOUSE_WHEEL)
    {
        camera_hold_engaged = 0;
        camera_hold_release_pending = 0;
        if (activated[MIKUPAN_SPECIAL_ACTION_RAISE_CAMERA])
        {
            camera_toggle_press_pending = 1;
        }
        if (camera_toggle_press_pending)
        {
            camera_pressed = 1;
            camera_toggle_press_pending = 0;
        }
    }
    else
    {
        const int camera_down =
            down[MIKUPAN_SPECIAL_ACTION_RAISE_CAMERA];
        if (activated[MIKUPAN_SPECIAL_ACTION_RAISE_CAMERA])
        {
            camera_hold_engaged = 1;
            camera_hold_release_pending = 0;
            if (plyr_wrk.mode == PMODE_NORMAL)
            {
                camera_pressed = 1;
            }
        }

        if (camera_hold_engaged && camera_released)
        {
            if (plyr_wrk.mode == PMODE_FINDER)
            {
                camera_pressed = 1;
                camera_hold_engaged = 0;
            }
            else if (MikuPan_IsFinderInputMode())
            {
                camera_hold_release_pending = 1;
            }
            else
            {
                camera_hold_engaged = 0;
            }
        }

        if (camera_hold_release_pending)
        {
            if (plyr_wrk.mode == PMODE_FINDER)
            {
                camera_pressed = 1;
                camera_hold_engaged = 0;
                camera_hold_release_pending = 0;
            }
            else if (!MikuPan_IsFinderInputMode())
            {
                camera_hold_engaged = 0;
                camera_hold_release_pending = 0;
            }
        }
    }

    if (camera_pressed
        && (plyr_wrk.mode == PMODE_NORMAL || MikuPan_IsFinderInputMode()))
    {
        MikuPan_InjectSpecialAction(
            pressed_buttons,
            MikuPan_KeyProfileUsesFinderShoulderToggle() ? 10 : 7);
    }

    if (plyr_wrk.mode == PMODE_NORMAL
        && down[MIKUPAN_SPECIAL_ACTION_RUN])
    {
        MikuPan_InjectSpecialAction(pressed_buttons, 6);
    }

    if (plyr_wrk.mode == PMODE_FINDER)
    {
        if (down[MIKUPAN_SPECIAL_ACTION_TAKE_PHOTO]
            || activated[MIKUPAN_SPECIAL_ACTION_TAKE_PHOTO])
        {
            MikuPan_InjectSpecialAction(
                pressed_buttons,
                MikuPan_KeyProfileUsesFinderShoulderToggle() ? 7 : 10);
        }

        if (down[MIKUPAN_SPECIAL_ACTION_SPECIAL_ABILITY]
            || activated[MIKUPAN_SPECIAL_ACTION_SPECIAL_ABILITY])
        {
            MikuPan_InjectSpecialAction(pressed_buttons, 8);
        }

        if (activated[MIKUPAN_SPECIAL_ACTION_FILM_PREVIOUS])
        {
            special_film_swap_direction = -1;
        }
        if (activated[MIKUPAN_SPECIAL_ACTION_FILM_NEXT])
        {
            special_film_swap_direction = 1;
        }
    }

    for (int i = 0; i < MIKUPAN_SPECIAL_ACTION_COUNT; i++)
    {
        special_action_previous_down[i] = down[i];
    }
}

int MikuPan_ReadController(unsigned char *rdata)
{
    MikuPan_ControllerDrawRemapWindow();

    for (int i = 1; i < 32; i++)
    {
        rdata[i] = 0xFF;
    }

    u_short *data = (u_short *) rdata;
    rdata[0] = 0;

    // Status 0x79: Is a DualShock 2.
    rdata[1] = 0x79;

    /* Read gamepad and keyboard every frame so both drive the game at once.
     * 127 is the shared neutral (MikuPan_GamePadAxisToPS2(0, 0) == 127, the same
     * value the keyboard path rests at). */
    const int stick_center = 127;
    const bool *key_states = SDL_GetKeyboardState(NULL);
    int pressed_buttons[MIKUPAN_CONTROLLER_LOGICAL_COUNT] = {};

    /* Buttons: OR the two sources per logical button, then flip the bit once.
     * XOR-ing per source would cancel out when both press the same button. */
    for (int i = 0; i < MIKUPAN_CONTROLLER_LOGICAL_COUNT; i++)
    {
        int pressed = 0;

        if (mikupan_gamepad != NULL)
        {
            switch (mikupan_controller_map[i].kind)
            {
                case MIKUPAN_CONTROLLER_BIND_BUTTON:
                    pressed = SDL_GetGamepadButton(
                        mikupan_gamepad, (SDL_GamepadButton) mikupan_controller_map[i].code);
                    break;
                case MIKUPAN_CONTROLLER_BIND_AXIS:
                    pressed = SDL_GetGamepadAxis(
                                  mikupan_gamepad, (SDL_GamepadAxis) mikupan_controller_map[i].code)
                              != 0;
                    break;
                default:
                    break;
            }
        }

        if (!pressed && key_states != NULL)
        {
            int sc = mikupan_keyboard_map[i];
            if (sc >= 0 && sc < SDL_SCANCODE_COUNT
                && !MikuPan_ScancodeBoundToSpecialAction(sc)
                && key_states[sc])
            {
                pressed = 1;
            }
        }

        pressed_buttons[i] = pressed;
    }

    MikuPan_ApplySpecialActions(pressed_buttons, key_states);

    for (int i = 0; i < MIKUPAN_CONTROLLER_LOGICAL_COUNT; i++)
    {
        data[1] ^= pressed_buttons[i] ? mikupan_controller_game_bitmask[i] : 0;
    }

    /* Sticks: combine both sources per axis, keeping whichever is deflected
     * further from centre so either device can take over at any time. */
    for (int i = 0; i < MIKUPAN_STICK_COUNT; i++)
    {
        int gamepad_value = stick_center;
        if (mikupan_gamepad != NULL)
        {
            MikuPan_StickGamepadBinding b = mikupan_stick_controller_map[i];
            int raw = 0;
            if (b.axis >= 0)
            {
                raw = SDL_GetGamepadAxis(mikupan_gamepad, (SDL_GamepadAxis) b.axis);
                if (b.invert)
                {
                    raw = -raw;
                }
            }
            gamepad_value = MikuPan_GamePadAxisToPS2(raw, 0);
        }

        int keyboard_value = stick_center;
        if (key_states != NULL)
        {
            MikuPan_StickKeyboardBinding b = mikupan_stick_keyboard_map[i];
            int neg = (b.neg_scancode > 0 && b.neg_scancode < SDL_SCANCODE_COUNT)
                          ? (int) key_states[b.neg_scancode]
                          : 0;
            int pos = (b.pos_scancode > 0 && b.pos_scancode < SDL_SCANCODE_COUNT)
                          ? (int) key_states[b.pos_scancode]
                          : 0;
            if (neg && !pos) keyboard_value = 0;
            else if (pos && !neg) keyboard_value = 255;
        }

        int gamepad_def = gamepad_value - stick_center;
        if (gamepad_def < 0) gamepad_def = -gamepad_def;
        int keyboard_def = keyboard_value - stick_center;
        if (keyboard_def < 0) keyboard_def = -keyboard_def;

        rdata[mikupan_stick_rdata_byte[i]] =
            (unsigned char) (gamepad_def >= keyboard_def ? gamepad_value
                                                         : keyboard_value);
    }

    return 1;
}

static const char *MikuPan_GamepadButtonFaceLabel(SDL_Gamepad *gamepad, SDL_GamepadButton button)
{
    if (gamepad == NULL)
    {
        return NULL;
    }

    switch (SDL_GetGamepadButtonLabel(gamepad, button))
    {
        case SDL_GAMEPAD_BUTTON_LABEL_A:
            return "A";
        case SDL_GAMEPAD_BUTTON_LABEL_B:
            return "B";
        case SDL_GAMEPAD_BUTTON_LABEL_X:
            return "X";
        case SDL_GAMEPAD_BUTTON_LABEL_Y:
            return "Y";
        case SDL_GAMEPAD_BUTTON_LABEL_CROSS:
            return "Cross";
        case SDL_GAMEPAD_BUTTON_LABEL_CIRCLE:
            return "Circle";
        case SDL_GAMEPAD_BUTTON_LABEL_SQUARE:
            return "Square";
        case SDL_GAMEPAD_BUTTON_LABEL_TRIANGLE:
            return "Triangle";
        default:
            return NULL;
    }
}

static int MikuPan_GamepadHasPlayStationLabels(SDL_Gamepad *gamepad)
{
    if (gamepad == NULL)
    {
        return 0;
    }

    SDL_GamepadButtonLabel south = SDL_GetGamepadButtonLabel(gamepad, SDL_GAMEPAD_BUTTON_SOUTH);
    SDL_GamepadButtonLabel east = SDL_GetGamepadButtonLabel(gamepad, SDL_GAMEPAD_BUTTON_EAST);
    return south == SDL_GAMEPAD_BUTTON_LABEL_CROSS || east == SDL_GAMEPAD_BUTTON_LABEL_CIRCLE;
}

static int MikuPan_GamepadHasNintendoLabels(SDL_Gamepad *gamepad)
{
    if (gamepad == NULL)
    {
        return 0;
    }

    SDL_GamepadButtonLabel south = SDL_GetGamepadButtonLabel(gamepad, SDL_GAMEPAD_BUTTON_SOUTH);
    SDL_GamepadButtonLabel east = SDL_GetGamepadButtonLabel(gamepad, SDL_GAMEPAD_BUTTON_EAST);
    SDL_GamepadButtonLabel west = SDL_GetGamepadButtonLabel(gamepad, SDL_GAMEPAD_BUTTON_WEST);
    SDL_GamepadButtonLabel north = SDL_GetGamepadButtonLabel(gamepad, SDL_GAMEPAD_BUTTON_NORTH);
    return south == SDL_GAMEPAD_BUTTON_LABEL_B && east == SDL_GAMEPAD_BUTTON_LABEL_A &&
           west == SDL_GAMEPAD_BUTTON_LABEL_Y && north == SDL_GAMEPAD_BUTTON_LABEL_X;
}

static int MikuPan_GamepadHasXboxLabels(SDL_Gamepad *gamepad)
{
    if (gamepad == NULL)
    {
        return 0;
    }

    SDL_GamepadButtonLabel south = SDL_GetGamepadButtonLabel(gamepad, SDL_GAMEPAD_BUTTON_SOUTH);
    SDL_GamepadButtonLabel east = SDL_GetGamepadButtonLabel(gamepad, SDL_GAMEPAD_BUTTON_EAST);
    SDL_GamepadButtonLabel west = SDL_GetGamepadButtonLabel(gamepad, SDL_GAMEPAD_BUTTON_WEST);
    SDL_GamepadButtonLabel north = SDL_GetGamepadButtonLabel(gamepad, SDL_GAMEPAD_BUTTON_NORTH);
    return south == SDL_GAMEPAD_BUTTON_LABEL_A && east == SDL_GAMEPAD_BUTTON_LABEL_B &&
           west == SDL_GAMEPAD_BUTTON_LABEL_X && north == SDL_GAMEPAD_BUTTON_LABEL_Y;
}

static const char *MikuPan_GamepadShoulderLabel(SDL_Gamepad *gamepad, SDL_GamepadButton button)
{
    if (MikuPan_GamepadHasPlayStationLabels(gamepad))
    {
        if (button == SDL_GAMEPAD_BUTTON_LEFT_SHOULDER)
            return "L1";
        if (button == SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER)
            return "R1";
    }

    if (MikuPan_GamepadHasNintendoLabels(gamepad))
    {
        if (button == SDL_GAMEPAD_BUTTON_LEFT_SHOULDER)
            return "L";
        if (button == SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER)
            return "R";
    }

    if (MikuPan_GamepadHasXboxLabels(gamepad))
    {
        if (button == SDL_GAMEPAD_BUTTON_LEFT_SHOULDER)
            return "LB";
        if (button == SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER)
            return "RB";
    }

    return NULL;
}

static const char *MikuPan_GamepadTriggerAxisLabel(SDL_Gamepad *gamepad, SDL_GamepadAxis axis)
{
    if (MikuPan_GamepadHasPlayStationLabels(gamepad))
    {
        if (axis == SDL_GAMEPAD_AXIS_LEFT_TRIGGER)
            return "L2";
        if (axis == SDL_GAMEPAD_AXIS_RIGHT_TRIGGER)
            return "R2";
    }

    if (MikuPan_GamepadHasNintendoLabels(gamepad))
    {
        if (axis == SDL_GAMEPAD_AXIS_LEFT_TRIGGER)
            return "ZL";
        if (axis == SDL_GAMEPAD_AXIS_RIGHT_TRIGGER)
            return "ZR";
    }

    if (MikuPan_GamepadHasXboxLabels(gamepad))
    {
        if (axis == SDL_GAMEPAD_AXIS_LEFT_TRIGGER)
            return "LT";
        if (axis == SDL_GAMEPAD_AXIS_RIGHT_TRIGGER)
            return "RT";
    }

    return NULL;
}

static const char *MikuPan_GamepadButtonFallbackLabel(SDL_GamepadButton button)
{
    switch (button)
    {
        case SDL_GAMEPAD_BUTTON_NORTH:
            return "Y / Triangle";
        case SDL_GAMEPAD_BUTTON_SOUTH:
            return "A / Cross";
        case SDL_GAMEPAD_BUTTON_WEST:
            return "X / Square";
        case SDL_GAMEPAD_BUTTON_EAST:
            return "B / Circle";
        case SDL_GAMEPAD_BUTTON_DPAD_UP:
            return "DPad Up";
        case SDL_GAMEPAD_BUTTON_DPAD_DOWN:
            return "DPad Down";
        case SDL_GAMEPAD_BUTTON_DPAD_LEFT:
            return "DPad Left";
        case SDL_GAMEPAD_BUTTON_DPAD_RIGHT:
            return "DPad Right";
        case SDL_GAMEPAD_BUTTON_LEFT_STICK:
            return "Left Stick (L3)";
        case SDL_GAMEPAD_BUTTON_RIGHT_STICK:
            return "Right Stick (R3)";
        case SDL_GAMEPAD_BUTTON_LEFT_SHOULDER:
            return "L1 / LB";
        case SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER:
            return "R1 / RB";
        case SDL_GAMEPAD_BUTTON_START:
            return "Start";
        case SDL_GAMEPAD_BUTTON_BACK:
            return "Select / Back";
        case SDL_GAMEPAD_BUTTON_GUIDE:
            return "Guide";
        case SDL_GAMEPAD_BUTTON_MISC1:
            return "Misc 1";
        case SDL_GAMEPAD_BUTTON_RIGHT_PADDLE1:
            return "Right Paddle 1";
        case SDL_GAMEPAD_BUTTON_LEFT_PADDLE1:
            return "Left Paddle 1";
        case SDL_GAMEPAD_BUTTON_RIGHT_PADDLE2:
            return "Right Paddle 2";
        case SDL_GAMEPAD_BUTTON_LEFT_PADDLE2:
            return "Left Paddle 2";
        case SDL_GAMEPAD_BUTTON_TOUCHPAD:
            return "Touchpad";
        case SDL_GAMEPAD_BUTTON_MISC2:
            return "Misc 2";
        case SDL_GAMEPAD_BUTTON_MISC3:
            return "Misc 3";
        case SDL_GAMEPAD_BUTTON_MISC4:
            return "Misc 4";
        case SDL_GAMEPAD_BUTTON_MISC5:
            return "Misc 5";
        case SDL_GAMEPAD_BUTTON_MISC6:
            return "Misc 6";
        default:
            break;
    }

    const char *name = SDL_GetGamepadStringForButton(button);
    return (name != NULL && name[0] != '\0') ? name : "Unknown Button";
}

const char *MikuPan_ControllerBindingLabel(MikuPan_ControllerBindings binding)
{
    SDL_Gamepad *gamepad = mikupan_gamepad;

    if (binding.kind == MIKUPAN_CONTROLLER_BIND_BUTTON)
    {
        SDL_GamepadButton button = (SDL_GamepadButton) binding.code;
        const char *face_label = MikuPan_GamepadButtonFaceLabel(gamepad, button);
        if (face_label != NULL)
        {
            return face_label;
        }

        const char *shoulder_label = MikuPan_GamepadShoulderLabel(gamepad, button);
        if (shoulder_label != NULL)
        {
            return shoulder_label;
        }

        return MikuPan_GamepadButtonFallbackLabel(button);
    }

    if (binding.kind == MIKUPAN_CONTROLLER_BIND_AXIS)
    {
        SDL_GamepadAxis axis = (SDL_GamepadAxis) binding.code;
        const char *trigger_label = MikuPan_GamepadTriggerAxisLabel(gamepad, axis);
        if (trigger_label != NULL)
        {
            return trigger_label;
        }

        switch (axis)
        {
            case SDL_GAMEPAD_AXIS_LEFT_TRIGGER:
                return "L2 / LT (axis)";
            case SDL_GAMEPAD_AXIS_RIGHT_TRIGGER:
                return "R2 / RT (axis)";
            case SDL_GAMEPAD_AXIS_LEFTX:
                return "Left Stick X";
            case SDL_GAMEPAD_AXIS_LEFTY:
                return "Left Stick Y";
            case SDL_GAMEPAD_AXIS_RIGHTX:
                return "Right Stick X";
            case SDL_GAMEPAD_AXIS_RIGHTY:
                return "Right Stick Y";
            default:
                break;
        }

        const char *name = SDL_GetGamepadStringForAxis(axis);
        return (name != NULL && name[0] != '\0') ? name : "Unknown Axis";
    }

    return "<unmapped>";
}

const char *MikuPan_ControllerScanCodeLabel(int scancode)
{
    if (scancode <= 0 || scancode >= SDL_SCANCODE_COUNT)
    {
        return "<unmapped>";
    }

    const char *name = SDL_GetScancodeName((SDL_Scancode) scancode);
    if (name == NULL || name[0] == '\0')
    {
        return "<unknown>";
    }

    return name;
}

const char *MikuPan_InputBindingLabel(MikuPan_InputBinding binding)
{
    if (binding.kind == MIKUPAN_INPUT_BIND_KEYBOARD)
    {
        return MikuPan_ControllerScanCodeLabel(binding.code);
    }

    if (binding.kind == MIKUPAN_INPUT_BIND_MOUSE_BUTTON)
    {
        switch (binding.code)
        {
            case SDL_BUTTON_LEFT:
                return "Left Mouse";
            case SDL_BUTTON_MIDDLE:
                return "Middle Mouse";
            case SDL_BUTTON_RIGHT:
                return "Right Mouse";
            case SDL_BUTTON_X1:
                return "Mouse 4";
            case SDL_BUTTON_X2:
                return "Mouse 5";
            default:
                return "<unknown mouse button>";
        }
    }

    if (binding.kind == MIKUPAN_INPUT_BIND_MOUSE_WHEEL)
    {
        return binding.code > 0 ? "Wheel Up" : "Wheel Down";
    }

    return "<unmapped>";
}

const char *MikuPan_SpecialActionLabel(int action)
{
    if (action < 0 || action >= MIKUPAN_SPECIAL_ACTION_COUNT)
    {
        return "<unknown action>";
    }

    return mikupan_special_action_labels[action];
}

const char *MikuPan_ControllerStickAxisLabel(int sdl_axis)
{
    switch (sdl_axis)
    {
        case SDL_GAMEPAD_AXIS_LEFTX:  return "Left Stick X";
        case SDL_GAMEPAD_AXIS_LEFTY:  return "Left Stick Y";
        case SDL_GAMEPAD_AXIS_RIGHTX: return "Right Stick X";
        case SDL_GAMEPAD_AXIS_RIGHTY: return "Right Stick Y";
        default:                      return "<unmapped>";
    }
}

SDL_Gamepad *MikuPan_GetController(void)
{
    return mikupan_gamepad;
}

int MikuPan_ControllerRumbleEnabled(void)
{
    return mikupan_configuration.input.rumble_enabled ? 1 : 0;
}

void MikuPan_SetControllerRumbleEnabled(int enabled)
{
    mikupan_configuration.input.rumble_enabled = enabled ? 1 : 0;
    opt_wrk.pad_move = mikupan_configuration.input.rumble_enabled ? 0 : 1;

    if (!mikupan_configuration.input.rumble_enabled)
    {
        MikuPan_ControllerStopRumble();
    }
}

int MikuPan_ControllerRumble(const unsigned char *data)
{
    if (data == NULL)
    {
        return 0;
    }

    if (!MikuPan_ControllerRumbleEnabled())
    {
        MikuPan_ControllerStopRumble();
        return 1;
    }

    if (mikupan_gamepad == NULL)
    {
        MikuPan_OpenController();
        if (mikupan_gamepad == NULL)
        {
            return 0;
        }
    }

    unsigned short low = (unsigned short)((unsigned int)data[1] * 257u);
    unsigned short high = data[0] != 0 ? 0xffffu : 0u;

    if (low == 0 && high == 0 && mikupan_rumble_low == 0 && mikupan_rumble_high == 0)
    {
        return 1;
    }

    if (!SDL_RumbleGamepad(mikupan_gamepad, low, high, 120))
    {
        return 0;
    }

    mikupan_rumble_low = low;
    mikupan_rumble_high = high;
    return 1;
}

int MikuPan_FinderMouseLookEnabled(void)
{
    return finder_mouse_enabled;
}

void MikuPan_SetFinderMouseLookEnabled(int enabled)
{
    finder_mouse_enabled = enabled ? 1 : 0;
}

float MikuPan_FinderMouseSensitivity(void)
{
    return finder_mouse_sensitivity;
}

void MikuPan_SetFinderMouseSensitivity(float sensitivity)
{
    if (sensitivity < MIKUPAN_FINDER_MOUSE_SENS_MIN)
    {
        sensitivity = MIKUPAN_FINDER_MOUSE_SENS_MIN;
    }
    else if (sensitivity > MIKUPAN_FINDER_MOUSE_SENS_MAX)
    {
        sensitivity = MIKUPAN_FINDER_MOUSE_SENS_MAX;
    }

    finder_mouse_sensitivity = sensitivity;
}

void MikuPan_FinderMouseRequest(void)
{
    finder_mouse_requested = 1;
}

void MikuPan_FinderMouseUpdate(void)
{
    ImGuiIO* io = igGetIO_Nil();
    int ui_blocks_mouse =
        MikuPan_UiIsMenuOpen()
        || (io != NULL && (io->WantCaptureMouse || io->WantTextInput));

    int want = finder_mouse_requested && finder_mouse_enabled && !ui_blocks_mouse;

    finder_mouse_requested = 0;

    if (want == finder_mouse_active)
    {
        return;
    }

    SDL_Window *window = MikuPan_GetUiWindow();
    if (window == NULL)
    {
        return;
    }

    SDL_SetWindowRelativeMouseMode(window, want ? true : false);
    finder_mouse_active = want;

    if (want)
    {
        /* Drop motion accumulated while the cursor was free so aiming does not
         * jump on the first engaged frame. */
        SDL_GetRelativeMouseState(NULL, NULL);
    }
}

int MikuPan_FinderMouseConsume(float *dx, float *dy)
{
    float mx = 0.0f;
    float my = 0.0f;

    if (finder_mouse_active)
    {
        SDL_GetRelativeMouseState(&mx, &my);
    }

    if (dx != NULL)
    {
        *dx = mx;
    }

    if (dy != NULL)
    {
        *dy = my;
    }

    return finder_mouse_active && (mx != 0.0f || my != 0.0f);
}

static int MikuPan_ControllerConfigActionProfileTarget(int layout, int target)
{
    static const int legacy_target_to_current[MIKUPAN_ACTION_PROFILE_ACTION_COUNT] = {
        4, 5, 6, 7, 0, 1, 2, 3, 15, 13, 12, 14, 10, 9, 11, 8,
    };

    if (target < 0 || target >= MIKUPAN_ACTION_PROFILE_ACTION_COUNT)
    {
        return MIKUPAN_ACTION_PROFILE_TARGET_NONE;
    }

    if (layout <= 0)
    {
        return legacy_target_to_current[target];
    }

    return target;
}

static ImU32 col_if(int pressed, ImU32 normal, ImU32 active)
{
    return pressed ? active : normal;
}

static int MikuPan_ControllerFindPressedInput(SDL_Gamepad *gp, int *out_kind, int *out_code)
{
    if (gp == NULL)
    {
        return 0;
    }

    for (int b = 0; b < SDL_GAMEPAD_BUTTON_COUNT; b++)
    {
        if (SDL_GetGamepadButton(gp, (SDL_GamepadButton)b))
        {
            *out_kind = MIKUPAN_CONTROLLER_BIND_BUTTON;
            *out_code = b;
            return 1;
        }
    }

    // Triggers only — sticks have rest values that drift and would constantly
    // commit a rebind. Use a generous half-press threshold.
    if (SDL_GetGamepadAxis(gp, SDL_GAMEPAD_AXIS_LEFT_TRIGGER) > 16384)
    {
        *out_kind = MIKUPAN_CONTROLLER_BIND_AXIS;
        *out_code = SDL_GAMEPAD_AXIS_LEFT_TRIGGER;
        return 1;
    }
    if (SDL_GetGamepadAxis(gp, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER) > 16384)
    {
        *out_kind = MIKUPAN_CONTROLLER_BIND_AXIS;
        *out_code = SDL_GAMEPAD_AXIS_RIGHT_TRIGGER;
        return 1;
    }

    return 0;
}

static int MikuPan_ControllerFindMovedStickAxis(SDL_Gamepad *gp, int *out_axis)
{
    if (gp == NULL)
    {
        return 0;
    }

    // Generous threshold so resting drift on a worn stick doesn't accidentally
    // bind. We only consider the four stick axes, not the triggers.
    const int threshold = 20000;
    const int axes[] = {
        SDL_GAMEPAD_AXIS_LEFTX,
        SDL_GAMEPAD_AXIS_LEFTY,
        SDL_GAMEPAD_AXIS_RIGHTX,
        SDL_GAMEPAD_AXIS_RIGHTY,
    };

    for (int i = 0; i < 4; i++)
    {
        int v = SDL_GetGamepadAxis(gp, (SDL_GamepadAxis) axes[i]);
        if (v > threshold || v < -threshold)
        {
            *out_axis = axes[i];
            return 1;
        }
    }
    return 0;
}

static int MikuPan_ControllerFindPressedScanCode(void)
{
    const bool *keys = SDL_GetKeyboardState(NULL);
    if (keys == NULL)
    {
        return -1;
    }

    // Skip ENTER/SPACE so the user can confirm UI buttons without
    // immediately rebinding to the same key.
    for (int i = 4; i < SDL_SCANCODE_COUNT; i++)
    {
        if (i == SDL_SCANCODE_RETURN || i == SDL_SCANCODE_KP_ENTER ||
            i == SDL_SCANCODE_SPACE  || i == SDL_SCANCODE_ESCAPE)
        {
            continue;
        }
        if (keys[i])
        {
            return i;
        }
    }
    return -1;
}

static void MikuPan_ControllerDrawControllerImage(SDL_Gamepad *gp)
{
    const float W = 540.0f;
    const float H = 270.0f;

    ImVec2 origin = igGetCursorScreenPos();
    igInvisibleButton("##controller_remap_gp_canvas", ImVec2{W, H}, 0);

    ImDrawList *dl = igGetWindowDrawList();

    const ImU32 body_col     = 0xFF2A2A30;
    const ImU32 body_outline = 0xFF606070;
    const ImU32 plate_col    = 0xFF505058;
    const ImU32 active_col   = 0xFF50C8FF;

    // -- Body & grips --
    ImVec2 b_min = ImVec2{origin.x + 70.0f,        origin.y + 50.0f};
    ImVec2 b_max = ImVec2{origin.x + W - 70.0f,    origin.y + H - 30.0f};
    ImDrawList_AddRectFilled(dl, b_min, b_max, body_col, 30.0f, 0);
    ImDrawList_AddRect      (dl, b_min, b_max, body_outline, 30.0f, 0, 0);

    ImDrawList_AddCircleFilled(dl, ImVec2{origin.x + 80,     origin.y + H - 60}, 50, body_col,     24);
    ImDrawList_AddCircle      (dl, ImVec2{origin.x + 80,     origin.y + H - 60}, 50, body_outline, 24, 2.0f);
    ImDrawList_AddCircleFilled(dl, ImVec2{origin.x + W - 80, origin.y + H - 60}, 50, body_col,     24);
    ImDrawList_AddCircle      (dl, ImVec2{origin.x + W - 80, origin.y + H - 60}, 50, body_outline, 24, 2.0f);

    // -- Shoulders & triggers --
    int lb = gp ? SDL_GetGamepadButton(gp, SDL_GAMEPAD_BUTTON_LEFT_SHOULDER)         : 0;
    int rb = gp ? SDL_GetGamepadButton(gp, SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER)        : 0;
    int lt = gp ? (SDL_GetGamepadAxis(gp, SDL_GAMEPAD_AXIS_LEFT_TRIGGER)  > 8000)    : 0;
    int rt = gp ? (SDL_GetGamepadAxis(gp, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER) > 8000)    : 0;

    ImDrawList_AddRectFilled(dl, ImVec2{origin.x + 60,  origin.y + 28},
                                  ImVec2{origin.x + 140, origin.y + 48},
                                  col_if(lb, plate_col, active_col), 6.0f, 0);
    ImDrawList_AddRectFilled(dl, ImVec2{origin.x + 70,  origin.y + 8},
                                  ImVec2{origin.x + 130, origin.y + 26},
                                  col_if(lt, plate_col, active_col), 6.0f, 0);
    ImDrawList_AddRectFilled(dl, ImVec2{origin.x + W - 140, origin.y + 28},
                                  ImVec2{origin.x + W - 60,  origin.y + 48},
                                  col_if(rb, plate_col, active_col), 6.0f, 0);
    ImDrawList_AddRectFilled(dl, ImVec2{origin.x + W - 130, origin.y + 8},
                                  ImVec2{origin.x + W - 70,  origin.y + 26},
                                  col_if(rt, plate_col, active_col), 6.0f, 0);

    ImDrawList_AddText_Vec2(dl, ImVec2{origin.x + 90,        origin.y + 31}, 0xFFFFFFFF, "L1", NULL);
    ImDrawList_AddText_Vec2(dl, ImVec2{origin.x + 90,        origin.y + 11}, 0xFFFFFFFF, "L2", NULL);
    ImDrawList_AddText_Vec2(dl, ImVec2{origin.x + W - 110,   origin.y + 31}, 0xFFFFFFFF, "R1", NULL);
    ImDrawList_AddText_Vec2(dl, ImVec2{origin.x + W - 110,   origin.y + 11}, 0xFFFFFFFF, "R2", NULL);

    // -- D-Pad (cross of 4 rectangles) --
    {
        float dx = origin.x + 145.0f;
        float dy = origin.y + 110.0f;
        float a  = 18.0f;
        float t  = 18.0f;

        int up    = gp ? SDL_GetGamepadButton(gp, SDL_GAMEPAD_BUTTON_DPAD_UP)    : 0;
        int down  = gp ? SDL_GetGamepadButton(gp, SDL_GAMEPAD_BUTTON_DPAD_DOWN)  : 0;
        int left  = gp ? SDL_GetGamepadButton(gp, SDL_GAMEPAD_BUTTON_DPAD_LEFT)  : 0;
        int right = gp ? SDL_GetGamepadButton(gp, SDL_GAMEPAD_BUTTON_DPAD_RIGHT) : 0;

        ImDrawList_AddRectFilled(dl, ImVec2{dx - t/2, dy - a},
                                      ImVec2{dx + t/2, dy - t/2},
                                      col_if(up, plate_col, active_col), 3, 0);
        ImDrawList_AddRectFilled(dl, ImVec2{dx - t/2, dy + t/2},
                                      ImVec2{dx + t/2, dy + a},
                                      col_if(down, plate_col, active_col), 3, 0);
        ImDrawList_AddRectFilled(dl, ImVec2{dx - a,   dy - t/2},
                                      ImVec2{dx - t/2, dy + t/2},
                                      col_if(left, plate_col, active_col), 3, 0);
        ImDrawList_AddRectFilled(dl, ImVec2{dx + t/2, dy - t/2},
                                      ImVec2{dx + a,   dy + t/2},
                                      col_if(right, plate_col, active_col), 3, 0);
        ImDrawList_AddRectFilled(dl, ImVec2{dx - t/2, dy - t/2},
                                      ImVec2{dx + t/2, dy + t/2},
                                      plate_col, 0, 0);
    }

    // -- Face buttons (4 colored circles in a diamond) --
    {
        float fx = origin.x + W - 145.0f;
        float fy = origin.y + 110.0f;
        float r  = 14.0f;
        float d  = 24.0f;

        int north = gp ? SDL_GetGamepadButton(gp, SDL_GAMEPAD_BUTTON_NORTH) : 0;
        int south = gp ? SDL_GetGamepadButton(gp, SDL_GAMEPAD_BUTTON_SOUTH) : 0;
        int west  = gp ? SDL_GetGamepadButton(gp, SDL_GAMEPAD_BUTTON_WEST)  : 0;
        int east  = gp ? SDL_GetGamepadButton(gp, SDL_GAMEPAD_BUTTON_EAST)  : 0;

        // North = Triangle (green); South = Cross (blue);
        // West  = Square   (pink ); East  = Circle (red ).
        ImDrawList_AddCircleFilled(dl, ImVec2{fx,     fy - d}, r, col_if(north, 0xFF306030, 0xFF60FF60), 0);
        ImDrawList_AddCircleFilled(dl, ImVec2{fx,     fy + d}, r, col_if(south, 0xFF303060, 0xFF6080FF), 0);
        ImDrawList_AddCircleFilled(dl, ImVec2{fx - d, fy    }, r, col_if(west,  0xFF603060, 0xFFFF60FF), 0);
        ImDrawList_AddCircleFilled(dl, ImVec2{fx + d, fy    }, r, col_if(east,  0xFF603030, 0xFFFF6060), 0);

        ImDrawList_AddCircle(dl, ImVec2{fx,     fy - d}, r, 0xFFA0A0A0, 0, 1.5f);
        ImDrawList_AddCircle(dl, ImVec2{fx,     fy + d}, r, 0xFFA0A0A0, 0, 1.5f);
        ImDrawList_AddCircle(dl, ImVec2{fx - d, fy    }, r, 0xFFA0A0A0, 0, 1.5f);
        ImDrawList_AddCircle(dl, ImVec2{fx + d, fy    }, r, 0xFFA0A0A0, 0, 1.5f);

        ImDrawList_AddText_Vec2(dl, ImVec2{fx - 4,     fy - d - 7}, 0xFFFFFFFF, "Y", NULL);
        ImDrawList_AddText_Vec2(dl, ImVec2{fx - 4,     fy + d - 7}, 0xFFFFFFFF, "A", NULL);
        ImDrawList_AddText_Vec2(dl, ImVec2{fx - d - 4, fy - 7    }, 0xFFFFFFFF, "X", NULL);
        ImDrawList_AddText_Vec2(dl, ImVec2{fx + d - 4, fy - 7    }, 0xFFFFFFFF, "B", NULL);
    }

    // -- Select / Start --
    {
        int back  = gp ? SDL_GetGamepadButton(gp, SDL_GAMEPAD_BUTTON_BACK)  : 0;
        int start = gp ? SDL_GetGamepadButton(gp, SDL_GAMEPAD_BUTTON_START) : 0;

        ImDrawList_AddRectFilled(dl, ImVec2{origin.x + W/2 - 38, origin.y + 105},
                                      ImVec2{origin.x + W/2 - 12, origin.y + 117},
                                      col_if(back, plate_col, active_col), 4, 0);
        ImDrawList_AddRectFilled(dl, ImVec2{origin.x + W/2 + 12, origin.y + 105},
                                      ImVec2{origin.x + W/2 + 38, origin.y + 117},
                                      col_if(start, plate_col, active_col), 4, 0);
        ImDrawList_AddText_Vec2(dl, ImVec2{origin.x + W/2 - 42, origin.y + 92}, 0xFFC0C0C0, "Select", NULL);
        ImDrawList_AddText_Vec2(dl, ImVec2{origin.x + W/2 + 14, origin.y + 92}, 0xFFC0C0C0, "Start",  NULL);
    }

    // -- Analog sticks --
    {
        int lpress = gp ? SDL_GetGamepadButton(gp, SDL_GAMEPAD_BUTTON_LEFT_STICK)  : 0;
        int rpress = gp ? SDL_GetGamepadButton(gp, SDL_GAMEPAD_BUTTON_RIGHT_STICK) : 0;

        float lsx = gp ? SDL_GetGamepadAxis(gp, SDL_GAMEPAD_AXIS_LEFTX)  / 32767.0f : 0.0f;
        float lsy = gp ? SDL_GetGamepadAxis(gp, SDL_GAMEPAD_AXIS_LEFTY)  / 32767.0f : 0.0f;
        float rsx = gp ? SDL_GetGamepadAxis(gp, SDL_GAMEPAD_AXIS_RIGHTX) / 32767.0f : 0.0f;
        float rsy = gp ? SDL_GetGamepadAxis(gp, SDL_GAMEPAD_AXIS_RIGHTY) / 32767.0f : 0.0f;

        float lcx = origin.x + 215.0f;
        float lcy = origin.y + 190.0f;
        float rcx = origin.x + W - 215.0f;
        float rcy = origin.y + 190.0f;

        ImDrawList_AddCircleFilled(dl, ImVec2{lcx, lcy}, 26, 0xFF202028, 0);
        ImDrawList_AddCircle      (dl, ImVec2{lcx, lcy}, 26, body_outline, 0, 1.5f);
        ImDrawList_AddCircleFilled(dl, ImVec2{lcx + lsx*16, lcy + lsy*16}, 16,
                                       col_if(lpress, plate_col, active_col), 0);

        ImDrawList_AddCircleFilled(dl, ImVec2{rcx, rcy}, 26, 0xFF202028, 0);
        ImDrawList_AddCircle      (dl, ImVec2{rcx, rcy}, 26, body_outline, 0, 1.5f);
        ImDrawList_AddCircleFilled(dl, ImVec2{rcx + rsx*16, rcy + rsy*16}, 16,
                                       col_if(rpress, plate_col, active_col), 0);
    }
}

/* Shared look for "click the current binding to remap it". The button fills its
 * table cell and turns amber while waiting for input. Returns true when clicked. */
static bool MikuPan_DrawRemapValueButton(const char *label, bool armed)
{
    ImVec2 avail = igGetContentRegionAvail();
    if (avail.x < 60.0f)
    {
        avail.x = 60.0f;
    }
    avail.y = 0.0f;

    if (armed)
    {
        igPushStyleColor_U32(ImGuiCol_Button,        0xFF1E78D2);
        igPushStyleColor_U32(ImGuiCol_ButtonHovered, 0xFF2E88E2);
        igPushStyleColor_U32(ImGuiCol_ButtonActive,  0xFF1668C2);
    }

    bool clicked = igButton(armed ? "Press input... (Esc)"
                                  : (label != NULL && label[0] != '\0' ? label : "<unmapped>"),
                            avail);

    if (armed)
    {
        igPopStyleColor(3);
    }

    return clicked;
}

/* Tables clip every cell to its own column, so rows can no longer overlap on a
 * narrow window the way the old fixed igSameLine() offsets did. */
#define MIKUPAN_BIND_TABLE_FLAGS                                               \
    (ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerH |                   \
     ImGuiTableFlags_PadOuterX | ImGuiTableFlags_NoBordersInBody)

static void MikuPan_ControllerDrawControllerBindingList(SDL_Gamepad *gp)
{
    igTextWrapped(gp != NULL
                      ? "Click a binding, then press a button on your controller. 'x' clears it."
                      : "No controller connected. Connect one to remap gamepad bindings.");

    if (igBeginTable("##gp_buttons", 3, MIKUPAN_BIND_TABLE_FLAGS, ImVec2{0, 0}, 0.0f))
    {
        igTableSetupColumn("Action",  ImGuiTableColumnFlags_WidthStretch, 0.40f, 0);
        igTableSetupColumn("Binding", ImGuiTableColumnFlags_WidthStretch, 0.60f, 0);
        igTableSetupColumn("##clear", ImGuiTableColumnFlags_WidthFixed,  26.0f, 0);

        for (int i = 0; i < MIKUPAN_CONTROLLER_LOGICAL_COUNT; i++)
        {
            igPushID_Int(i);
            igTableNextRow(0, 0.0f);

            igTableNextColumn();
            igAlignTextToFramePadding();
            igTextUnformatted(mikupan_controller_labels[i], NULL);

            igTableNextColumn();
            bool armed = remap_target == i && remap_target_kb == 0;
            if (MikuPan_DrawRemapValueButton(
                    MikuPan_ControllerBindingLabel(mikupan_controller_map[i]), armed))
            {
                remap_target = armed ? -1 : i;
                remap_target_kb = 0;
            }

            igTableNextColumn();
            if (igButton("x", ImVec2{24, 0}))
            {
                mikupan_controller_map[i].kind = MIKUPAN_CONTROLLER_BIND_NONE;
                mikupan_controller_map[i].code = 0;
            }

            igPopID();
        }

        igEndTable();
    }

    if (remap_target >= 0 && remap_target_kb == 0)
    {
        int kind = 0, code = 0;
        if (MikuPan_ControllerFindPressedInput(gp, &kind, &code))
        {
            mikupan_controller_map[remap_target].kind = kind;
            mikupan_controller_map[remap_target].code = code;
            remap_target = -1;
        }
        if (igIsKeyPressed_Bool(ImGuiKey_Escape, 0))
        {
            remap_target = -1;
        }
    }
}

static void MikuPan_ControllerDrawKeyboardBindingList(void)
{
    igTextWrapped("Always active alongside the controller. Click a binding, then "
                  "press a key. 'x' clears it.");

    if (igBeginTable("##kb_buttons", 3, MIKUPAN_BIND_TABLE_FLAGS, ImVec2{0, 0}, 0.0f))
    {
        igTableSetupColumn("Action",  ImGuiTableColumnFlags_WidthStretch, 0.40f, 0);
        igTableSetupColumn("Key",     ImGuiTableColumnFlags_WidthStretch, 0.60f, 0);
        igTableSetupColumn("##clear", ImGuiTableColumnFlags_WidthFixed,  26.0f, 0);

        for (int i = 0; i < MIKUPAN_CONTROLLER_LOGICAL_COUNT; i++)
        {
            igPushID_Int(0x100 + i);
            igTableNextRow(0, 0.0f);

            igTableNextColumn();
            igAlignTextToFramePadding();
            igTextUnformatted(mikupan_controller_labels[i], NULL);

            igTableNextColumn();
            bool armed = remap_target == i && remap_target_kb == 1;
            if (MikuPan_DrawRemapValueButton(
                    MikuPan_ControllerScanCodeLabel(mikupan_keyboard_map[i]), armed))
            {
                remap_target = armed ? -1 : i;
                remap_target_kb = 1;
            }

            igTableNextColumn();
            if (igButton("x", ImVec2{24, 0}))
            {
                mikupan_keyboard_map[i] = 0;
            }

            igPopID();
        }

        igEndTable();
    }

    if (remap_target >= 0 && remap_target_kb == 1)
    {
        int sc = MikuPan_ControllerFindPressedScanCode();

        if (sc >= 0)
        {
            mikupan_keyboard_map[remap_target] = sc;
            remap_target = -1;
        }

        if (igIsKeyPressed_Bool(ImGuiKey_Escape, 0))
        {
            remap_target = -1;
        }
    }
}

static void MikuPan_ControllerDrawStickGamepadList(SDL_Gamepad *gp)
{
    igTextWrapped(gp != NULL
                      ? "Click an axis, then move a stick on your controller. 'x' clears it."
                      : "No controller connected. Connect one to remap stick axes.");

    if (igBeginTable("##gp_sticks", 4, MIKUPAN_BIND_TABLE_FLAGS, ImVec2{0, 0}, 0.0f))
    {
        igTableSetupColumn("Stick",    ImGuiTableColumnFlags_WidthStretch, 0.40f, 0);
        igTableSetupColumn("Axis",     ImGuiTableColumnFlags_WidthStretch, 0.60f, 0);
        igTableSetupColumn("##invert", ImGuiTableColumnFlags_WidthFixed,  96.0f, 0);
        igTableSetupColumn("##clear",  ImGuiTableColumnFlags_WidthFixed,  26.0f, 0);

        for (int i = 0; i < MIKUPAN_STICK_COUNT; i++)
        {
            igPushID_Int(0x200 + i);
            igTableNextRow(0, 0.0f);

            igTableNextColumn();
            igAlignTextToFramePadding();
            igTextUnformatted(mikupan_stick_labels[i], NULL);

            igTableNextColumn();
            bool armed = remap_stick_target == i && remap_stick_mode == 0;
            if (MikuPan_DrawRemapValueButton(
                    MikuPan_ControllerStickAxisLabel(mikupan_stick_controller_map[i].axis), armed))
            {
                remap_stick_target = armed ? -1 : i;
                remap_stick_mode = 0;
            }

            igTableNextColumn();
            igAlignTextToFramePadding();
            bool invert = mikupan_stick_controller_map[i].invert != 0;
            if (igCheckbox("Invert", &invert))
            {
                mikupan_stick_controller_map[i].invert = invert ? 1 : 0;
            }

            igTableNextColumn();
            if (igButton("x", ImVec2{24, 0}))
            {
                mikupan_stick_controller_map[i].axis = -1;
            }

            igPopID();
        }

        igEndTable();
    }

    if (remap_stick_target >= 0 && remap_stick_mode == 0)
    {
        int axis = 0;
        if (MikuPan_ControllerFindMovedStickAxis(gp, &axis))
        {
            mikupan_stick_controller_map[remap_stick_target].axis = axis;
            remap_stick_target = -1;
        }
        if (igIsKeyPressed_Bool(ImGuiKey_Escape, 0))
        {
            remap_stick_target = -1;
        }
    }
}

static void MikuPan_ControllerDrawStickKeyboardList(void)
{
    igTextWrapped("Always active alongside the controller. Each axis is driven to "
                  "its extremes by a negative and a positive key. Click a key to "
                  "rebind it, 'x' clears it.");

    if (igBeginTable("##kb_sticks", 5, MIKUPAN_BIND_TABLE_FLAGS, ImVec2{0, 0}, 0.0f))
    {
        igTableSetupColumn("Stick",     ImGuiTableColumnFlags_WidthStretch, 0.28f, 0);
        igTableSetupColumn("Negative",  ImGuiTableColumnFlags_WidthStretch, 0.36f, 0);
        igTableSetupColumn("##clr_neg", ImGuiTableColumnFlags_WidthFixed,  26.0f, 0);
        igTableSetupColumn("Positive",  ImGuiTableColumnFlags_WidthStretch, 0.36f, 0);
        igTableSetupColumn("##clr_pos", ImGuiTableColumnFlags_WidthFixed,  26.0f, 0);

        for (int i = 0; i < MIKUPAN_STICK_COUNT; i++)
        {
            igPushID_Int(0x300 + i);
            igTableNextRow(0, 0.0f);

            igTableNextColumn();
            igAlignTextToFramePadding();
            igTextUnformatted(mikupan_stick_labels[i], NULL);

            igTableNextColumn();
            igPushID_Int(1);
            bool neg_armed = remap_stick_target == i && remap_stick_mode == 1;
            if (MikuPan_DrawRemapValueButton(
                    MikuPan_ControllerScanCodeLabel(mikupan_stick_keyboard_map[i].neg_scancode),
                    neg_armed))
            {
                remap_stick_target = neg_armed ? -1 : i;
                remap_stick_mode = 1;
            }
            igPopID();

            igTableNextColumn();
            if (igButton("x##neg", ImVec2{24, 0}))
            {
                mikupan_stick_keyboard_map[i].neg_scancode = 0;
            }

            igTableNextColumn();
            igPushID_Int(2);
            bool pos_armed = remap_stick_target == i && remap_stick_mode == 2;
            if (MikuPan_DrawRemapValueButton(
                    MikuPan_ControllerScanCodeLabel(mikupan_stick_keyboard_map[i].pos_scancode),
                    pos_armed))
            {
                remap_stick_target = pos_armed ? -1 : i;
                remap_stick_mode = 2;
            }
            igPopID();

            igTableNextColumn();
            if (igButton("x##pos", ImVec2{24, 0}))
            {
                mikupan_stick_keyboard_map[i].pos_scancode = 0;
            }

            igPopID();
        }

        igEndTable();
    }

    if (remap_stick_target >= 0 && (remap_stick_mode == 1 || remap_stick_mode == 2))
    {
        int sc = MikuPan_ControllerFindPressedScanCode();
        if (sc >= 0)
        {
            if (remap_stick_mode == 1)
            {
                mikupan_stick_keyboard_map[remap_stick_target].neg_scancode = sc;
            }
            else
            {
                mikupan_stick_keyboard_map[remap_stick_target].pos_scancode = sc;
            }
            remap_stick_target = -1;
        }
        if (igIsKeyPressed_Bool(ImGuiKey_Escape, 0))
        {
            remap_stick_target = -1;
        }
    }
}

static void MikuPan_ControllerDrawActionTargetCombo(int mode, int action)
{
    int target = MikuPan_GetCustomActionProfileTarget(mode, action);
    const char *preview = MikuPan_ActionProfileTargetLabel(target);

    if (igBeginCombo("##target", preview, 0))
    {
        const bool none_selected = target == MIKUPAN_ACTION_PROFILE_TARGET_NONE;
        if (igSelectable_Bool("<unmapped>", none_selected, 0, ImVec2{0, 0}))
        {
            MikuPan_SetCustomActionProfileTarget(
                mode, action, MIKUPAN_ACTION_PROFILE_TARGET_NONE);
        }
        if (none_selected)
        {
            igSetItemDefaultFocus();
        }

        for (int i = 0; i < MIKUPAN_ACTION_PROFILE_ACTION_COUNT; i++)
        {
            const bool selected = target == i;
            if (igSelectable_Bool(MikuPan_ActionProfileTargetLabel(i), selected,
                                  0, ImVec2{0, 0}))
            {
                MikuPan_SetCustomActionProfileTarget(mode, action, i);
            }
            if (selected)
            {
                igSetItemDefaultFocus();
            }
        }

        igEndCombo();
    }
}

static void MikuPan_ControllerDrawActionProfileList(int mode)
{
    if (igBeginTable("##action_list", 3, MIKUPAN_BIND_TABLE_FLAGS, ImVec2{0, 0}, 0.0f))
    {
        igTableSetupColumn("Action",   ImGuiTableColumnFlags_WidthStretch, 0.45f, 0);
        igTableSetupColumn("Target",   ImGuiTableColumnFlags_WidthStretch, 0.55f, 0);
        igTableSetupColumn("##default", ImGuiTableColumnFlags_WidthFixed, 76.0f, 0);

        for (int i = 0; i < MIKUPAN_ACTION_PROFILE_ACTION_COUNT; i++)
        {
            igPushID_Int(0x500 + mode * 0x100 + i);
            igTableNextRow(0, 0.0f);

            igTableNextColumn();
            igAlignTextToFramePadding();
            igTextUnformatted(MikuPan_ActionProfileActionLabel(mode, i), NULL);

            igTableNextColumn();
            igSetNextItemWidth(igGetContentRegionAvail().x);
            MikuPan_ControllerDrawActionTargetCombo(mode, i);

            igTableNextColumn();
            if (igButton("Default", ImVec2{72, 0}))
            {
                MikuPan_SetCustomActionProfileTarget(
                    mode, i, MikuPan_GetDefaultActionProfileTarget(i));
            }

            igPopID();
        }

        igEndTable();
    }
}

static void MikuPan_ControllerDrawMovementModeCombo(
    const char *label, int subjective, void (*setter)(int))
{
    const char *preview = subjective ? "Subjective" : "Objective";

    igSetNextItemWidth(150.0f);
    if (igBeginCombo(label, preview, 0))
    {
        if (igSelectable_Bool("Objective", !subjective, 0, ImVec2{0, 0}))
        {
            setter(0);
        }
        if (!subjective)
        {
            igSetItemDefaultFocus();
        }

        if (igSelectable_Bool("Subjective", subjective, 0, ImVec2{0, 0}))
        {
            setter(1);
        }
        if (subjective)
        {
            igSetItemDefaultFocus();
        }

        igEndCombo();
    }
}

static void MikuPan_ControllerDrawActionProfileSettingsUi(void)
{
    bool enabled = MikuPan_IsCustomActionProfileEnabled() != 0;
    if (igCheckbox("Enable custom action profile", &enabled))
    {
        MikuPan_SetCustomActionProfileEnabled(enabled ? 1 : 0);
    }

    igSeparatorText("Normal movement");
    MikuPan_ControllerDrawMovementModeCombo(
        "DPad##normal_move_dpad",
        MikuPan_CustomActionProfileUsesDpadSubjectiveMove(),
        MikuPan_SetCustomActionProfileDpadSubjectiveMove);

    MikuPan_ControllerDrawMovementModeCombo(
        "Stick##normal_move_stick",
        MikuPan_CustomActionProfileUsesStickSubjectiveMove(),
        MikuPan_SetCustomActionProfileStickSubjectiveMove);

    bool reverse_y = MikuPan_CustomActionProfileUsesFinderReverseY() != 0;
    if (igCheckbox("Reverse finder vertical aim", &reverse_y))
    {
        MikuPan_SetCustomActionProfileFinderReverseY(reverse_y ? 1 : 0);
    }

    bool swap_sticks = MikuPan_CustomActionProfileSwapsFinderSticks() != 0;
    if (igCheckbox("Swap finder sticks", &swap_sticks))
    {
        MikuPan_SetCustomActionProfileFinderSwapSticks(swap_sticks ? 1 : 0);
    }
    igTextDisabled("On (default): left stick / WASD move, right stick / mouse "
                   "aim. Off: classic (left stick aims, right stick moves).");

    bool finder_film_swap = MikuPan_FinderDpadFilmSwapEnabled() != 0;
    if (igCheckbox("D-Pad quick film swap", &finder_film_swap))
    {
        MikuPan_SetFinderDpadFilmSwapEnabled(finder_film_swap ? 1 : 0);
    }
    igTextDisabled("Use D-Pad Up / Down in finder mode to switch film.\n"
                   "D-Pad finder aim is disabled while enabled.");

    bool mirror_stone_hud = MikuPan_MirrorStoneHudEnabled() != 0;
    if (igCheckbox("Mirror Stone HUD icon", &mirror_stone_hud))
    {
        MikuPan_SetMirrorStoneHudEnabled(mirror_stone_hud ? 1 : 0);
    }
    igTextDisabled("Shows a small Mirror Stone box next to the filament. It "
                   "stays visible even when you do not have one.");

    if (igButton("Reset custom profile", ImVec2{0, 0}))
    {
        MikuPan_ResetCustomActionProfile();
    }

    igSameLine(0.0f, -1.0f);
    igTextDisabled("(keeps the custom profile enabled state)");

    igSeparatorText("Finder Mouse Look");
    igTextDisabled("Aim the first-person finder with the mouse. Always active "
                   "(independent of the custom profile above).");

    bool mouse_enabled = MikuPan_FinderMouseLookEnabled() != 0;
    if (igCheckbox("Enable finder mouse look", &mouse_enabled))
    {
        MikuPan_SetFinderMouseLookEnabled(mouse_enabled ? 1 : 0);
    }

    float sensitivity = MikuPan_FinderMouseSensitivity();
    igSetNextItemWidth(220.0f);
    if (igSliderFloat("Mouse sensitivity", &sensitivity, 0.1f, 5.0f, "%.2fx", 0))
    {
        MikuPan_SetFinderMouseSensitivity(sensitivity);
    }

    igTextDisabled("Vertical aim follows the \"Reverse finder vertical aim\" "
                   "option above.");
}

static void MikuPan_ControllerDrawActionProfileMapsUi(void)
{
    igTextWrapped("Advanced: remap which in-game action each button triggers, "
                  "separately for normal and finder mode.");

    if (igCollapsingHeader_TreeNodeFlags("Normal Mode Actions", ImGuiTreeNodeFlags_DefaultOpen))
    {
        MikuPan_ControllerDrawActionProfileList(
            MIKUPAN_ACTION_PROFILE_MODE_NORMAL);
    }

    if (igCollapsingHeader_TreeNodeFlags("Finder Mode Actions", 0))
    {
        MikuPan_ControllerDrawActionProfileList(
            MIKUPAN_ACTION_PROFILE_MODE_FINDER);
    }
}

/* Renders the buttons + sticks for whichever device the selector is showing.
 * Wrapped in a scrolling child so a long list never pushes the tab bar around or
 * forces the window taller than a small screen. */
static void MikuPan_ControllerDrawBindingsTab(SDL_Gamepad *gp)
{
    if (igBeginChild_Str("##bindings_scroll", ImVec2{0, 0}, 0, 0))
    {
        if (mikupan_input_view_keyboard)
        {
            igSeparatorText("Buttons");
            MikuPan_ControllerDrawKeyboardBindingList();
            igSeparatorText("Sticks");
            MikuPan_ControllerDrawStickKeyboardList();
        }
        else
        {
            if (igCollapsingHeader_TreeNodeFlags("Live preview", 0))
            {
                MikuPan_ControllerDrawControllerImage(gp);
            }
            igSeparatorText("Buttons");
            MikuPan_ControllerDrawControllerBindingList(gp);
            igSeparatorText("Sticks");
            MikuPan_ControllerDrawStickGamepadList(gp);
        }
    }
    igEndChild();
}

void MikuPan_ControllerDrawRemapWindow(void)
{
    static int last_draw_frame = -1;

    if (!MikuPan_ShowControllerRemapWindow())
    {
        return;
    }

    int frame = igGetFrameCount();
    if (last_draw_frame == frame)
    {
        return;
    }
    last_draw_frame = frame;

    igSetNextWindowSize(ImVec2{560.0f, 600.0f}, ImGuiCond_FirstUseEver);
    igBegin("Controller Mapping", NULL, 0);

    SDL_Gamepad *gp = MikuPan_GetController();

    /* Device selector and reset stay above the tabs: the selector decides which
     * device the Bindings tab edits, and reset applies to everything. */
    MikuPan_ControllerDrawDeviceSelectorUi();

    if (gp != NULL)
    {
        igText("Connected: %s", SDL_GetGamepadName(gp));
    }
    else
    {
        igTextDisabled("No controller connected.");
    }

    if (igButton("Reset all input settings", ImVec2{0, 0}))
    {
        MikuPan_ControllerResetBindings();
        remap_target = -1;
        remap_stick_target = -1;
    }

    igSeparator();

    /* Three tabs replace the old stack of collapsing headers: Bindings (the
     * device's buttons/sticks), Movement & Aim (profile + mouse-look), and the
     * advanced per-action maps. The ###id keeps the first tab stable while its
     * label tracks the selected device. */
    if (igBeginTabBar("##input_tabs", ImGuiTabBarFlags_None))
    {
        if (igBeginTabItem(mikupan_input_view_keyboard ? "Keyboard & Mouse###bindings"
                                                       : "Gamepad###bindings",
                           NULL, 0))
        {
            MikuPan_ControllerDrawBindingsTab(gp);
            igEndTabItem();
        }

        if (igBeginTabItem("Movement & Aim", NULL, 0))
        {
            MikuPan_ControllerDrawActionProfileSettingsUi();
            igEndTabItem();
        }

        if (igBeginTabItem("Action Maps", NULL, 0))
        {
            MikuPan_ControllerDrawActionProfileMapsUi();
            igEndTabItem();
        }

        igEndTabBar();
    }

    igEnd();
}
