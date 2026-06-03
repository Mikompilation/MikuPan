#include "mikupan_controller.h"
#include "SDL3/SDL_scancode.h"
#include "mikupan_config.h"
#include "mikupan_logging_c.h"
#include "mikupan_utils.h"
#include <SDL3/SDL_keyboard.h>

#define CIMGUI_DEFINE_ENUMS_AND_STRUCTS
#include "cimgui.h"
#include "ui/mikupan_ui.h"

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

static const int mikupan_keyboard_map_defaults[MIKUPAN_CONTROLLER_LOGICAL_COUNT] = {
    SDL_SCANCODE_I,         SDL_SCANCODE_K,      SDL_SCANCODE_J,
    SDL_SCANCODE_L,         SDL_SCANCODE_UP,     SDL_SCANCODE_DOWN,
    SDL_SCANCODE_LEFT,      SDL_SCANCODE_RIGHT,  SDL_SCANCODE_M,
    SDL_SCANCODE_BACKSPACE, SDL_SCANCODE_ESCAPE, SDL_SCANCODE_N,
    SDL_SCANCODE_O,         SDL_SCANCODE_8,      SDL_SCANCODE_9,
    SDL_SCANCODE_U,
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
    SDL_SCANCODE_I,         SDL_SCANCODE_K,      SDL_SCANCODE_J,
    SDL_SCANCODE_L,         SDL_SCANCODE_UP,     SDL_SCANCODE_DOWN,
    SDL_SCANCODE_LEFT,      SDL_SCANCODE_RIGHT,  SDL_SCANCODE_M,
    SDL_SCANCODE_BACKSPACE, SDL_SCANCODE_ESCAPE, SDL_SCANCODE_N,
    SDL_SCANCODE_O,         SDL_SCANCODE_8,      SDL_SCANCODE_9,
    SDL_SCANCODE_U,
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
        SDL_CloseGamepad(mikupan_gamepad);
        mikupan_gamepad = NULL;
    }
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

    char preview[160];
    if (mikupan_preferred_gamepad_index == MIKUPAN_CONTROLLER_AUTO_INDEX)
    {
        snprintf(preview, sizeof(preview), "Auto (first available)");
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

    if (igBeginCombo("Controller", preview, 0))
    {
        const bool auto_selected =
            mikupan_preferred_gamepad_index == MIKUPAN_CONTROLLER_AUTO_INDEX;
        if (igSelectable_Bool("Auto (first available)", auto_selected, 0, (ImVec2){0, 0}))
        {
            MikuPan_ControllerSetPreferredGamepadIndex(MIKUPAN_CONTROLLER_AUTO_INDEX);
        }
        if (auto_selected)
        {
            igSetItemDefaultFocus();
        }

        for (int i = 0; i < num_gamepads; i++)
        {
            char label[160];
            snprintf(label, sizeof(label), "%d: %s", i + 1,
                     MikuPan_ControllerNameForId(joysticks_id[i]));

            const bool is_selected = mikupan_preferred_gamepad_index == i;
            if (igSelectable_Bool(label, is_selected, 0, (ImVec2){0, 0}))
            {
                MikuPan_ControllerSetPreferredGamepadIndex(i);
            }
            if (is_selected)
            {
                igSetItemDefaultFocus();
            }
        }

        igEndCombo();
    }

    if (mikupan_gamepad != NULL)
    {
        const char *name = SDL_GetGamepadName(mikupan_gamepad);
        const char *safe_name = (name != NULL && name[0] != '\0') ? name : "<unknown controller>";
        int active_index = -1;
        if (joysticks_id != NULL)
        {
            active_index = MikuPan_ControllerFindGamepadIndex(
                joysticks_id, num_gamepads, SDL_GetGamepadID(mikupan_gamepad));
        }

        if (active_index >= 0)
        {
            igTextDisabled("Active: %d: %s", active_index + 1, safe_name);
        }
        else
        {
            igTextDisabled("Active: %s", safe_name);
        }
    }
    else if (num_gamepads > 0)
    {
        igTextDisabled("No active controller for the current selection.");
    }
    else
    {
        igTextDisabled("No controllers detected.");
    }

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

    cfg->bindings_saved = 1;
}

void MikuPan_ControllerLoadBindingsFromConfig(void)
{
    const MikuPan_ConfigInput *cfg = &mikupan_configuration.input;

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

    if (mikupan_gamepad != NULL)
    {
        for (int i = 0; i < MIKUPAN_CONTROLLER_LOGICAL_COUNT; i++)
        {
            int pressed = 0;
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
            data[1] ^= pressed ? mikupan_controller_game_bitmask[i] : 0;
        }

        for (int i = 0; i < MIKUPAN_STICK_COUNT; i++)
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
            rdata[mikupan_stick_rdata_byte[i]] = MikuPan_GamePadAxisToPS2(raw, 0);
        }
    }
    else
    {
        const bool *key_states = SDL_GetKeyboardState(NULL);

        for (int i = 0; i < MIKUPAN_CONTROLLER_LOGICAL_COUNT; i++)
        {
            int sc = mikupan_keyboard_map[i];
            int pressed =
                (sc >= 0 && sc < SDL_SCANCODE_COUNT) ? (int) key_states[sc] : 0;
            data[1] ^= pressed ? mikupan_controller_game_bitmask[i] : 0;
        }

        for (int i = 0; i < MIKUPAN_STICK_COUNT; i++)
        {
            MikuPan_StickKeyboardBinding b = mikupan_stick_keyboard_map[i];
            int neg = (b.neg_scancode > 0 && b.neg_scancode < SDL_SCANCODE_COUNT)
                          ? (int) key_states[b.neg_scancode]
                          : 0;
            int pos = (b.pos_scancode > 0 && b.pos_scancode < SDL_SCANCODE_COUNT)
                          ? (int) key_states[b.pos_scancode]
                          : 0;
            unsigned char v = 127;
            if (neg && !pos) v = 0;
            else if (pos && !neg) v = 255;
            rdata[mikupan_stick_rdata_byte[i]] = v;
        }
    }

    return 1;
}

const char *MikuPan_ControllerBindingLabel(MikuPan_ControllerBindings binding)
{
    if (binding.kind == MIKUPAN_CONTROLLER_BIND_BUTTON)
    {
        switch (binding.code)
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
            default:
                return "Unknown Button";
        }
    }

    if (binding.kind == MIKUPAN_CONTROLLER_BIND_AXIS)
    {
        switch (binding.code)
        {
            case SDL_GAMEPAD_AXIS_LEFT_TRIGGER:
                return "L2 / LT (axis)";
            case SDL_GAMEPAD_AXIS_RIGHT_TRIGGER:
                return "R2 / RT (axis)";
            default:
                return "Unknown Axis";
        }
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

int MikuPan_ControllerRumble(const unsigned char *data)
{
    if (mikupan_gamepad == NULL)
    {
        return 0;
    }

    return SDL_RumbleGamepad(mikupan_gamepad, data[0] * 32896, data[1] * 257, 100);
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
    igInvisibleButton("##gp_canvas", (ImVec2){W, H}, 0);

    ImDrawList *dl = igGetWindowDrawList();

    const ImU32 body_col     = 0xFF2A2A30;
    const ImU32 body_outline = 0xFF606070;
    const ImU32 plate_col    = 0xFF505058;
    const ImU32 active_col   = 0xFF50C8FF;

    // -- Body & grips --
    ImVec2 b_min = (ImVec2){origin.x + 70.0f,        origin.y + 50.0f};
    ImVec2 b_max = (ImVec2){origin.x + W - 70.0f,    origin.y + H - 30.0f};
    ImDrawList_AddRectFilled(dl, b_min, b_max, body_col, 30.0f, 0);
    ImDrawList_AddRect      (dl, b_min, b_max, body_outline, 30.0f, 0, 0);

    ImDrawList_AddCircleFilled(dl, (ImVec2){origin.x + 80,     origin.y + H - 60}, 50, body_col,     24);
    ImDrawList_AddCircle      (dl, (ImVec2){origin.x + 80,     origin.y + H - 60}, 50, body_outline, 24, 2.0f);
    ImDrawList_AddCircleFilled(dl, (ImVec2){origin.x + W - 80, origin.y + H - 60}, 50, body_col,     24);
    ImDrawList_AddCircle      (dl, (ImVec2){origin.x + W - 80, origin.y + H - 60}, 50, body_outline, 24, 2.0f);

    // -- Shoulders & triggers --
    int lb = gp ? SDL_GetGamepadButton(gp, SDL_GAMEPAD_BUTTON_LEFT_SHOULDER)         : 0;
    int rb = gp ? SDL_GetGamepadButton(gp, SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER)        : 0;
    int lt = gp ? (SDL_GetGamepadAxis(gp, SDL_GAMEPAD_AXIS_LEFT_TRIGGER)  > 8000)    : 0;
    int rt = gp ? (SDL_GetGamepadAxis(gp, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER) > 8000)    : 0;

    ImDrawList_AddRectFilled(dl, (ImVec2){origin.x + 60,  origin.y + 28},
                                  (ImVec2){origin.x + 140, origin.y + 48},
                                  col_if(lb, plate_col, active_col), 6.0f, 0);
    ImDrawList_AddRectFilled(dl, (ImVec2){origin.x + 70,  origin.y + 8},
                                  (ImVec2){origin.x + 130, origin.y + 26},
                                  col_if(lt, plate_col, active_col), 6.0f, 0);
    ImDrawList_AddRectFilled(dl, (ImVec2){origin.x + W - 140, origin.y + 28},
                                  (ImVec2){origin.x + W - 60,  origin.y + 48},
                                  col_if(rb, plate_col, active_col), 6.0f, 0);
    ImDrawList_AddRectFilled(dl, (ImVec2){origin.x + W - 130, origin.y + 8},
                                  (ImVec2){origin.x + W - 70,  origin.y + 26},
                                  col_if(rt, plate_col, active_col), 6.0f, 0);

    ImDrawList_AddText_Vec2(dl, (ImVec2){origin.x + 90,        origin.y + 31}, 0xFFFFFFFF, "L1", NULL);
    ImDrawList_AddText_Vec2(dl, (ImVec2){origin.x + 90,        origin.y + 11}, 0xFFFFFFFF, "L2", NULL);
    ImDrawList_AddText_Vec2(dl, (ImVec2){origin.x + W - 110,   origin.y + 31}, 0xFFFFFFFF, "R1", NULL);
    ImDrawList_AddText_Vec2(dl, (ImVec2){origin.x + W - 110,   origin.y + 11}, 0xFFFFFFFF, "R2", NULL);

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

        ImDrawList_AddRectFilled(dl, (ImVec2){dx - t/2, dy - a},
                                      (ImVec2){dx + t/2, dy - t/2},
                                      col_if(up, plate_col, active_col), 3, 0);
        ImDrawList_AddRectFilled(dl, (ImVec2){dx - t/2, dy + t/2},
                                      (ImVec2){dx + t/2, dy + a},
                                      col_if(down, plate_col, active_col), 3, 0);
        ImDrawList_AddRectFilled(dl, (ImVec2){dx - a,   dy - t/2},
                                      (ImVec2){dx - t/2, dy + t/2},
                                      col_if(left, plate_col, active_col), 3, 0);
        ImDrawList_AddRectFilled(dl, (ImVec2){dx + t/2, dy - t/2},
                                      (ImVec2){dx + a,   dy + t/2},
                                      col_if(right, plate_col, active_col), 3, 0);
        ImDrawList_AddRectFilled(dl, (ImVec2){dx - t/2, dy - t/2},
                                      (ImVec2){dx + t/2, dy + t/2},
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
        ImDrawList_AddCircleFilled(dl, (ImVec2){fx,     fy - d}, r, col_if(north, 0xFF306030, 0xFF60FF60), 0);
        ImDrawList_AddCircleFilled(dl, (ImVec2){fx,     fy + d}, r, col_if(south, 0xFF303060, 0xFF6080FF), 0);
        ImDrawList_AddCircleFilled(dl, (ImVec2){fx - d, fy    }, r, col_if(west,  0xFF603060, 0xFFFF60FF), 0);
        ImDrawList_AddCircleFilled(dl, (ImVec2){fx + d, fy    }, r, col_if(east,  0xFF603030, 0xFFFF6060), 0);

        ImDrawList_AddCircle(dl, (ImVec2){fx,     fy - d}, r, 0xFFA0A0A0, 0, 1.5f);
        ImDrawList_AddCircle(dl, (ImVec2){fx,     fy + d}, r, 0xFFA0A0A0, 0, 1.5f);
        ImDrawList_AddCircle(dl, (ImVec2){fx - d, fy    }, r, 0xFFA0A0A0, 0, 1.5f);
        ImDrawList_AddCircle(dl, (ImVec2){fx + d, fy    }, r, 0xFFA0A0A0, 0, 1.5f);

        ImDrawList_AddText_Vec2(dl, (ImVec2){fx - 4,     fy - d - 7}, 0xFFFFFFFF, "Y", NULL);
        ImDrawList_AddText_Vec2(dl, (ImVec2){fx - 4,     fy + d - 7}, 0xFFFFFFFF, "A", NULL);
        ImDrawList_AddText_Vec2(dl, (ImVec2){fx - d - 4, fy - 7    }, 0xFFFFFFFF, "X", NULL);
        ImDrawList_AddText_Vec2(dl, (ImVec2){fx + d - 4, fy - 7    }, 0xFFFFFFFF, "B", NULL);
    }

    // -- Select / Start --
    {
        int back  = gp ? SDL_GetGamepadButton(gp, SDL_GAMEPAD_BUTTON_BACK)  : 0;
        int start = gp ? SDL_GetGamepadButton(gp, SDL_GAMEPAD_BUTTON_START) : 0;

        ImDrawList_AddRectFilled(dl, (ImVec2){origin.x + W/2 - 38, origin.y + 105},
                                      (ImVec2){origin.x + W/2 - 12, origin.y + 117},
                                      col_if(back, plate_col, active_col), 4, 0);
        ImDrawList_AddRectFilled(dl, (ImVec2){origin.x + W/2 + 12, origin.y + 105},
                                      (ImVec2){origin.x + W/2 + 38, origin.y + 117},
                                      col_if(start, plate_col, active_col), 4, 0);
        ImDrawList_AddText_Vec2(dl, (ImVec2){origin.x + W/2 - 42, origin.y + 92}, 0xFFC0C0C0, "Select", NULL);
        ImDrawList_AddText_Vec2(dl, (ImVec2){origin.x + W/2 + 14, origin.y + 92}, 0xFFC0C0C0, "Start",  NULL);
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

        ImDrawList_AddCircleFilled(dl, (ImVec2){lcx, lcy}, 26, 0xFF202028, 0);
        ImDrawList_AddCircle      (dl, (ImVec2){lcx, lcy}, 26, body_outline, 0, 1.5f);
        ImDrawList_AddCircleFilled(dl, (ImVec2){lcx + lsx*16, lcy + lsy*16}, 16,
                                       col_if(lpress, plate_col, active_col), 0);

        ImDrawList_AddCircleFilled(dl, (ImVec2){rcx, rcy}, 26, 0xFF202028, 0);
        ImDrawList_AddCircle      (dl, (ImVec2){rcx, rcy}, 26, body_outline, 0, 1.5f);
        ImDrawList_AddCircleFilled(dl, (ImVec2){rcx + rsx*16, rcy + rsy*16}, 16,
                                       col_if(rpress, plate_col, active_col), 0);
    }
}

static void MikuPan_ControllerDrawControllerBindingList(SDL_Gamepad *gp)
{
    igTextDisabled(gp != NULL ? "Click 'Edit' on a row, then press a button on your controller."
                              : "No controller connected. Connect one to remap gamepad bindings.");

    for (int i = 0; i < MIKUPAN_CONTROLLER_LOGICAL_COUNT; i++)
    {
        igPushID_Int(i);

        igText("%-18s", mikupan_controller_labels[i]);
        igSameLine(180.0f, -1.0f);
        igText("%-22s", MikuPan_ControllerBindingLabel(mikupan_controller_map[i]));
        igSameLine(390.0f, -1.0f);

        if (remap_target == i && remap_target_kb == 0)
        {
            igTextColored((ImVec4){1.0f, 0.7f, 0.2f, 1.0f}, "Press a button...");
            igSameLine(0.0f, -1.0f);
            if (igButton("Cancel", (ImVec2){0, 0}))
            {
                remap_target = -1;
            }
        }
        else
        {
            if (igButton("Edit", (ImVec2){60, 0}))
            {
                remap_target = i;
                remap_target_kb = 0;
            }
            igSameLine(0.0f, -1.0f);
            if (igButton("Clear", (ImVec2){60, 0}))
            {
                mikupan_controller_map[i].kind = MIKUPAN_CONTROLLER_BIND_NONE;
                mikupan_controller_map[i].code = 0;
            }
        }

        igPopID();
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
    igTextDisabled("Active when no controller is connected. Click 'Edit' then press a key.");

    for (int i = 0; i < MIKUPAN_CONTROLLER_LOGICAL_COUNT; i++)
    {
        igPushID_Int(0x100 + i);

        igText("%-18s", mikupan_controller_labels[i]);
        igSameLine(180.0f, -1.0f);
        igText("%-22s", MikuPan_ControllerScanCodeLabel(mikupan_keyboard_map[i]));
        igSameLine(390.0f, -1.0f);

        if (remap_target == i && remap_target_kb == 1)
        {
            igTextColored((ImVec4){1.0f, 0.7f, 0.2f, 1.0f}, "Press a key...");
            igSameLine(0.0f, -1.0f);

            if (igButton("Cancel", (ImVec2){0, 0}))
            {
                remap_target = -1;
            }
        }
        else
        {
            if (igButton("Edit", (ImVec2){60, 0}))
            {
                remap_target = i;
                remap_target_kb = 1;
            }

            igSameLine(0.0f, -1.0f);

            if (igButton("Clear", (ImVec2){60, 0}))
            {
                mikupan_keyboard_map[i] = 0;
            }
        }

        igPopID();
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
    igTextDisabled(gp != NULL
                       ? "Click 'Edit' on a stick row, then move a stick on your controller."
                       : "No controller connected. Connect one to remap stick axes.");

    for (int i = 0; i < MIKUPAN_STICK_COUNT; i++)
    {
        igPushID_Int(0x200 + i);

        igText("%-18s", mikupan_stick_labels[i]);
        igSameLine(180.0f, -1.0f);
        igText("%-18s", MikuPan_ControllerStickAxisLabel(mikupan_stick_controller_map[i].axis));
        igSameLine(340.0f, -1.0f);

        bool invert = mikupan_stick_controller_map[i].invert != 0;
        if (igCheckbox("Invert", &invert))
        {
            mikupan_stick_controller_map[i].invert = invert ? 1 : 0;
        }
        igSameLine(0.0f, -1.0f);

        if (remap_stick_target == i && remap_stick_mode == 0)
        {
            igTextColored((ImVec4){1.0f, 0.7f, 0.2f, 1.0f}, "Move a stick...");
            igSameLine(0.0f, -1.0f);
            if (igButton("Cancel", (ImVec2){0, 0}))
            {
                remap_stick_target = -1;
            }
        }
        else
        {
            if (igButton("Edit", (ImVec2){60, 0}))
            {
                remap_stick_target = i;
                remap_stick_mode = 0;
            }
            igSameLine(0.0f, -1.0f);
            if (igButton("Clear", (ImVec2){60, 0}))
            {
                mikupan_stick_controller_map[i].axis = -1;
            }
        }

        igPopID();
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
    igTextDisabled("Active when no controller is connected. Two keys per axis drive it to its extremes.");

    for (int i = 0; i < MIKUPAN_STICK_COUNT; i++)
    {
        igPushID_Int(0x300 + i);

        igText("%-16s", mikupan_stick_labels[i]);
        igSameLine(170.0f, -1.0f);
        igText("- %-10s", MikuPan_ControllerScanCodeLabel(mikupan_stick_keyboard_map[i].neg_scancode));
        igSameLine(0.0f, -1.0f);

        if (remap_stick_target == i && remap_stick_mode == 1)
        {
            igTextColored((ImVec4){1.0f, 0.7f, 0.2f, 1.0f}, "Press key...");
            igSameLine(0.0f, -1.0f);
            if (igButton("Cancel##neg", (ImVec2){0, 0}))
            {
                remap_stick_target = -1;
            }
        }
        else
        {
            if (igButton("Edit##neg", (ImVec2){60, 0}))
            {
                remap_stick_target = i;
                remap_stick_mode = 1;
            }
            igSameLine(0.0f, -1.0f);
            if (igButton("Clear##neg", (ImVec2){60, 0}))
            {
                mikupan_stick_keyboard_map[i].neg_scancode = 0;
            }
        }

        igSameLine(0.0f, 18.0f);
        igText("+ %-10s", MikuPan_ControllerScanCodeLabel(mikupan_stick_keyboard_map[i].pos_scancode));
        igSameLine(0.0f, -1.0f);

        if (remap_stick_target == i && remap_stick_mode == 2)
        {
            igTextColored((ImVec4){1.0f, 0.7f, 0.2f, 1.0f}, "Press key...");
            igSameLine(0.0f, -1.0f);
            if (igButton("Cancel##pos", (ImVec2){0, 0}))
            {
                remap_stick_target = -1;
            }
        }
        else
        {
            if (igButton("Edit##pos", (ImVec2){60, 0}))
            {
                remap_stick_target = i;
                remap_stick_mode = 2;
            }
            igSameLine(0.0f, -1.0f);
            if (igButton("Clear##pos", (ImVec2){60, 0}))
            {
                mikupan_stick_keyboard_map[i].pos_scancode = 0;
            }
        }

        igPopID();
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

void MikuPan_ControllerDrawRemapWindow(void)
{
    if (!MikuPan_ShowControllerRemapWindow())
    {
        return;
    }

    igSetNextWindowSize((ImVec2){600.0f, 720.0f}, ImGuiCond_FirstUseEver);
    igBegin("Controller Mapping", NULL, 0);

    MikuPan_ControllerDrawDeviceSelectorUi();
    igSpacing();

    SDL_Gamepad *gp = MikuPan_GetController();

    if (gp != NULL)
    {
        igText("Connected: %s", SDL_GetGamepadName(gp));
    }
    else
    {
        igTextDisabled("No controller connected (keyboard fallback active).");
    }

    MikuPan_ControllerDrawControllerImage(gp);

    if (igButton("Reset to defaults", (ImVec2){0, 0}))
    {
        MikuPan_ControllerResetBindings();
        remap_target = -1;
        remap_stick_target = -1;
    }

    igSameLine(0.0f, -1.0f);
    igTextDisabled("(restores buttons, sticks, and keyboard mappings)");

    igSpacing();

    if (igBeginTabBar("##remap_tabs", 0))
    {
        if (igBeginTabItem("Gamepad Buttons", NULL, 0))
        {
            MikuPan_ControllerDrawControllerBindingList(gp);
            igEndTabItem();
        }
        if (igBeginTabItem("Gamepad Sticks", NULL, 0))
        {
            MikuPan_ControllerDrawStickGamepadList(gp);
            igEndTabItem();
        }
        if (igBeginTabItem("Keyboard Buttons", NULL, 0))
        {
            MikuPan_ControllerDrawKeyboardBindingList();
            igEndTabItem();
        }
        if (igBeginTabItem("Keyboard Sticks", NULL, 0))
        {
            MikuPan_ControllerDrawStickKeyboardList();
            igEndTabItem();
        }
        igEndTabBar();
    }

    igEnd();
}
