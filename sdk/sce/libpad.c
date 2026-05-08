#include "libpad.h"

#include "mikupan/mikupan_logging_c.h"
#include "mikupan/mikupan_utils.h"
#include "os/pad.h"
#include <SDL3/SDL_gamepad.h>
#include <SDL3/SDL_keyboard.h>
#include <SDL3/SDL_scancode.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>

SDL_Gamepad *gamepad = NULL;

// Default mappings — applied at startup and by scePadResetBindings. Indices
// match sce_pad[] in src/os/pad.c (0=Triangle, 1=Cross, 2=Square, 3=Circle,
// 4-7=DPad, 8=R3, 9=Select, 10=Start, 11=L3, 12=R1, 13=L2, 14=R2, 15=L1).
static const SceGpBinding kGpDefaults[SCE_PAD_LOGICAL_COUNT] = {
    { SCE_BIND_BUTTON, SDL_GAMEPAD_BUTTON_NORTH          },
    { SCE_BIND_BUTTON, SDL_GAMEPAD_BUTTON_SOUTH          },
    { SCE_BIND_BUTTON, SDL_GAMEPAD_BUTTON_WEST           },
    { SCE_BIND_BUTTON, SDL_GAMEPAD_BUTTON_EAST           },
    { SCE_BIND_BUTTON, SDL_GAMEPAD_BUTTON_DPAD_UP        },
    { SCE_BIND_BUTTON, SDL_GAMEPAD_BUTTON_DPAD_DOWN      },
    { SCE_BIND_BUTTON, SDL_GAMEPAD_BUTTON_DPAD_LEFT      },
    { SCE_BIND_BUTTON, SDL_GAMEPAD_BUTTON_DPAD_RIGHT     },
    { SCE_BIND_BUTTON, SDL_GAMEPAD_BUTTON_RIGHT_STICK    },
    { SCE_BIND_BUTTON, SDL_GAMEPAD_BUTTON_BACK           },
    { SCE_BIND_BUTTON, SDL_GAMEPAD_BUTTON_START          },
    { SCE_BIND_BUTTON, SDL_GAMEPAD_BUTTON_LEFT_STICK     },
    { SCE_BIND_BUTTON, SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER },
    { SCE_BIND_AXIS,   SDL_GAMEPAD_AXIS_LEFT_TRIGGER     },
    { SCE_BIND_AXIS,   SDL_GAMEPAD_AXIS_RIGHT_TRIGGER    },
    { SCE_BIND_BUTTON, SDL_GAMEPAD_BUTTON_LEFT_SHOULDER  },
};

static const int kKbDefaults[SCE_PAD_LOGICAL_COUNT] = {
    SDL_SCANCODE_I, SDL_SCANCODE_K, SDL_SCANCODE_J, SDL_SCANCODE_L,
    SDL_SCANCODE_UP, SDL_SCANCODE_DOWN, SDL_SCANCODE_LEFT, SDL_SCANCODE_RIGHT,
    SDL_SCANCODE_M, SDL_SCANCODE_BACKSPACE, SDL_SCANCODE_ESCAPE, SDL_SCANCODE_N,
    SDL_SCANCODE_O, SDL_SCANCODE_8, SDL_SCANCODE_9, SDL_SCANCODE_U,
};

SceGpBinding sce_gp_map[SCE_PAD_LOGICAL_COUNT] = {
    { SCE_BIND_BUTTON, SDL_GAMEPAD_BUTTON_NORTH          },
    { SCE_BIND_BUTTON, SDL_GAMEPAD_BUTTON_SOUTH          },
    { SCE_BIND_BUTTON, SDL_GAMEPAD_BUTTON_WEST           },
    { SCE_BIND_BUTTON, SDL_GAMEPAD_BUTTON_EAST           },
    { SCE_BIND_BUTTON, SDL_GAMEPAD_BUTTON_DPAD_UP        },
    { SCE_BIND_BUTTON, SDL_GAMEPAD_BUTTON_DPAD_DOWN      },
    { SCE_BIND_BUTTON, SDL_GAMEPAD_BUTTON_DPAD_LEFT      },
    { SCE_BIND_BUTTON, SDL_GAMEPAD_BUTTON_DPAD_RIGHT     },
    { SCE_BIND_BUTTON, SDL_GAMEPAD_BUTTON_RIGHT_STICK    },
    { SCE_BIND_BUTTON, SDL_GAMEPAD_BUTTON_BACK           },
    { SCE_BIND_BUTTON, SDL_GAMEPAD_BUTTON_START          },
    { SCE_BIND_BUTTON, SDL_GAMEPAD_BUTTON_LEFT_STICK     },
    { SCE_BIND_BUTTON, SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER },
    { SCE_BIND_AXIS,   SDL_GAMEPAD_AXIS_LEFT_TRIGGER     },
    { SCE_BIND_AXIS,   SDL_GAMEPAD_AXIS_RIGHT_TRIGGER    },
    { SCE_BIND_BUTTON, SDL_GAMEPAD_BUTTON_LEFT_SHOULDER  },
};

int sce_kb_map[SCE_PAD_LOGICAL_COUNT] = {
    SDL_SCANCODE_I, SDL_SCANCODE_K, SDL_SCANCODE_J, SDL_SCANCODE_L,
    SDL_SCANCODE_UP, SDL_SCANCODE_DOWN, SDL_SCANCODE_LEFT, SDL_SCANCODE_RIGHT,
    SDL_SCANCODE_M, SDL_SCANCODE_BACKSPACE, SDL_SCANCODE_ESCAPE, SDL_SCANCODE_N,
    SDL_SCANCODE_O, SDL_SCANCODE_8, SDL_SCANCODE_9, SDL_SCANCODE_U,
};

void scePadResetBindings(void)
{
    for (int i = 0; i < SCE_PAD_LOGICAL_COUNT; i++)
    {
        sce_gp_map[i] = kGpDefaults[i];
        sce_kb_map[i] = kKbDefaults[i];
    }
}

SDL_Gamepad *scePadGetSdlGamepad(void)
{
    return gamepad;
}

int scePadPortOpen(int port, int slot, void* addr)
{
    if (gamepad != NULL && !SDL_GamepadConnected(gamepad))
    {
        SDL_CloseGamepad(gamepad);
        return 0;
    }

    if (gamepad != NULL || !SDL_HasGamepad())
    {
        return 0;
    }

    int num_gamepad = 0;
    SDL_JoystickID *joysticks_id = SDL_GetGamepads(&num_gamepad);

    if (joysticks_id == NULL)
    {
        return 0;
    }

    gamepad = SDL_OpenGamepad(joysticks_id[0]);

    info_log("Controller %s connected to", SDL_GetGamepadName(gamepad));

    SDL_free(joysticks_id);

    return 1;
}

int scePadInit(int mode)
{
    return 1;
}

int scePadGetState(int port, int slot)
{
    //if (gamepad == NULL)
    //{
    //    return scePadStateDiscon;
    //}

    return scePadStateStable;
}

int scePadRead(int port, int slot, unsigned char* rdata)
{
    for (int i = 1; i < 32; i++)
    {
        rdata[i] = 0xFF;
    }

    u_short* data = (u_short*)rdata;
    rdata[0] = 0;

    // Status 0x79: Is a DualShock 2.
    rdata[1] = 0x79;

    if (gamepad != NULL)
    {
        for (int i = 0; i < SCE_PAD_LOGICAL_COUNT; i++)
        {
            int pressed = 0;
            switch (sce_gp_map[i].kind)
            {
                case SCE_BIND_BUTTON:
                    pressed = SDL_GetGamepadButton(gamepad, (SDL_GamepadButton)sce_gp_map[i].code);
                    break;
                case SCE_BIND_AXIS:
                    pressed = SDL_GetGamepadAxis(gamepad, (SDL_GamepadAxis)sce_gp_map[i].code) != 0;
                    break;
                default:
                    break;
            }
            data[1] ^= pressed ? sce_pad[i] : 0;
        }

        rdata[5] = MikuPan_GamePadAxisToPS2(SDL_GetGamepadAxis(gamepad, SDL_GAMEPAD_AXIS_RIGHTY), 0);
        rdata[4] = MikuPan_GamePadAxisToPS2(SDL_GetGamepadAxis(gamepad, SDL_GAMEPAD_AXIS_RIGHTX), 0);
        rdata[7] = MikuPan_GamePadAxisToPS2(SDL_GetGamepadAxis(gamepad, SDL_GAMEPAD_AXIS_LEFTY), 0);
        rdata[6] = MikuPan_GamePadAxisToPS2(SDL_GetGamepadAxis(gamepad, SDL_GAMEPAD_AXIS_LEFTX), 0);
    }
    else
    {
        const bool *key_states = SDL_GetKeyboardState(NULL);

        for (int i = 0; i < SCE_PAD_LOGICAL_COUNT; i++)
        {
            int sc = sce_kb_map[i];
            int pressed = (sc >= 0 && sc < SDL_SCANCODE_COUNT) ? (int)key_states[sc] : 0;
            data[1] ^= pressed ? sce_pad[i] : 0;
        }

        rdata[5] = 127;
        rdata[4] = 127;
        rdata[7] = 127;
        rdata[6] = 127;
    }

    return 1;
}

/// 4: STANDARD CONTROLLER (Dualshock)
/// 7: ANALOG CONTROLLER (Dualshock 2)
int scePadInfoMode(int port, int slot, int term, int offs)
{
    return 7;
}

int scePadSetMainMode(int port, int slot, int offs, int lock)
{
    return scePadReqStateComplete;
}

int scePadInfoAct(int port, int slot, int actno, int term)
{
    return scePadReqStateComplete;
}

int scePadSetActAlign(int port, int slot, const unsigned char* data)
{
    if (gamepad == NULL)
    {
        return 0;
    }

    return SDL_RumbleGamepad(gamepad, data[0] * 32896, data[1] * 257, 100);
}

int scePadGetReqState(int port, int slot)
{
    return scePadReqStateComplete;
}

int scePadInfoPressMode(int port, int slot)
{
    return scePadReqStateComplete;
}

int scePadEnterPressMode(int port, int slot)
{
    return 1;
}

int scePadSetActDirect(int port, int slot, const unsigned char* data)
{
    if (gamepad == NULL)
    {
        return 0;
    }

    return SDL_RumbleGamepad(gamepad, data[0] * 32896, data[1] * 257, 100);
}

const char *scePadBindingLabel(SceGpBinding binding)
{
    if (binding.kind == SCE_BIND_BUTTON)
    {
        switch (binding.code)
        {
            case SDL_GAMEPAD_BUTTON_NORTH:          return "Y / Triangle";
            case SDL_GAMEPAD_BUTTON_SOUTH:          return "A / Cross";
            case SDL_GAMEPAD_BUTTON_WEST:           return "X / Square";
            case SDL_GAMEPAD_BUTTON_EAST:           return "B / Circle";
            case SDL_GAMEPAD_BUTTON_DPAD_UP:        return "DPad Up";
            case SDL_GAMEPAD_BUTTON_DPAD_DOWN:      return "DPad Down";
            case SDL_GAMEPAD_BUTTON_DPAD_LEFT:      return "DPad Left";
            case SDL_GAMEPAD_BUTTON_DPAD_RIGHT:     return "DPad Right";
            case SDL_GAMEPAD_BUTTON_LEFT_STICK:     return "Left Stick (L3)";
            case SDL_GAMEPAD_BUTTON_RIGHT_STICK:    return "Right Stick (R3)";
            case SDL_GAMEPAD_BUTTON_LEFT_SHOULDER:  return "L1 / LB";
            case SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER: return "R1 / RB";
            case SDL_GAMEPAD_BUTTON_START:          return "Start";
            case SDL_GAMEPAD_BUTTON_BACK:           return "Select / Back";
            case SDL_GAMEPAD_BUTTON_GUIDE:          return "Guide";
            default:                                return "Unknown Button";
        }
    }
    if (binding.kind == SCE_BIND_AXIS)
    {
        switch (binding.code)
        {
            case SDL_GAMEPAD_AXIS_LEFT_TRIGGER:  return "L2 / LT (axis)";
            case SDL_GAMEPAD_AXIS_RIGHT_TRIGGER: return "R2 / RT (axis)";
            default:                             return "Unknown Axis";
        }
    }
    return "<unmapped>";
}

const char *scePadScancodeLabel(int scancode)
{
    if (scancode < 0 || scancode >= SDL_SCANCODE_COUNT)
    {
        return "<unmapped>";
    }
    const char *name = SDL_GetScancodeName((SDL_Scancode)scancode);
    if (name == NULL || name[0] == '\0')
    {
        return "<unknown>";
    }
    return name;
}
