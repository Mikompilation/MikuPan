#include "libpad.h"
#include "os/pad.h"
#include <SDL3/SDL_gamepad.h>
#include <SDL3/SDL_keyboard.h>
#include <stddef.h>

SDL_Gamepad *gamepad = NULL;

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

    if (gamepad != NULL)
    {
        data[1] ^= SDL_GetGamepadButton(gamepad, SDL_GAMEPAD_BUTTON_NORTH)          ? sce_pad[0] : 0;
        data[1] ^= SDL_GetGamepadButton(gamepad, SDL_GAMEPAD_BUTTON_SOUTH)          ? sce_pad[1] : 0;
        data[1] ^= SDL_GetGamepadButton(gamepad, SDL_GAMEPAD_BUTTON_WEST)           ? sce_pad[2] : 0;
        data[1] ^= SDL_GetGamepadButton(gamepad, SDL_GAMEPAD_BUTTON_EAST)           ? sce_pad[3] : 0;
        data[1] ^= SDL_GetGamepadButton(gamepad, SDL_GAMEPAD_BUTTON_DPAD_UP)        ? sce_pad[4] : 0;
        data[1] ^= SDL_GetGamepadButton(gamepad, SDL_GAMEPAD_BUTTON_DPAD_DOWN)      ? sce_pad[5] : 0;
        data[1] ^= SDL_GetGamepadButton(gamepad, SDL_GAMEPAD_BUTTON_DPAD_LEFT)      ? sce_pad[6] : 0;
        data[1] ^= SDL_GetGamepadButton(gamepad, SDL_GAMEPAD_BUTTON_DPAD_RIGHT)     ? sce_pad[7] : 0;
        data[1] ^= SDL_GetGamepadButton(gamepad, SDL_GAMEPAD_BUTTON_RIGHT_STICK)    ? sce_pad[8] : 0;
        data[1] ^= SDL_GetGamepadButton(gamepad, SDL_GAMEPAD_BUTTON_BACK)           ? sce_pad[9] : 0;
        data[1] ^= SDL_GetGamepadButton(gamepad, SDL_GAMEPAD_BUTTON_START)          ? sce_pad[10] : 0;
        data[1] ^= SDL_GetGamepadButton(gamepad, SDL_GAMEPAD_BUTTON_LEFT_STICK)     ? sce_pad[11] : 0;
        data[1] ^= SDL_GetGamepadButton(gamepad, SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER) ? sce_pad[12] : 0;
        data[1] ^= SDL_GetGamepadAxis(gamepad, SDL_GAMEPAD_AXIS_LEFT_TRIGGER)       ? sce_pad[13] : 0;
        data[1] ^= SDL_GetGamepadAxis(gamepad, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER)      ? sce_pad[14] : 0;
        data[1] ^= SDL_GetGamepadButton(gamepad, SDL_GAMEPAD_BUTTON_LEFT_SHOULDER)  ? sce_pad[15] : 0;
    }
    else
    {
        const bool *key_states = SDL_GetKeyboardState(NULL);

        data[1] ^= key_states[SDL_SCANCODE_I]           ? sce_pad[0] : 0;
        data[1] ^= key_states[SDL_SCANCODE_K]           ? sce_pad[1] : 0;
        data[1] ^= key_states[SDL_SCANCODE_J]           ? sce_pad[2] : 0;
        data[1] ^= key_states[SDL_SCANCODE_L]           ? sce_pad[3] : 0;
        data[1] ^= key_states[SDL_SCANCODE_UP]          ? sce_pad[4] : 0;
        data[1] ^= key_states[SDL_SCANCODE_DOWN]        ? sce_pad[5] : 0;
        data[1] ^= key_states[SDL_SCANCODE_LEFT]        ? sce_pad[6] : 0;
        data[1] ^= key_states[SDL_SCANCODE_RIGHT]       ? sce_pad[7] : 0;
        data[1] ^= key_states[SDL_SCANCODE_M]           ? sce_pad[8] : 0;
        data[1] ^= key_states[SDL_SCANCODE_BACKSPACE]   ? sce_pad[9] : 0;
        data[1] ^= key_states[SDL_SCANCODE_ESCAPE]      ? sce_pad[10] : 0;
        data[1] ^= key_states[SDL_SCANCODE_N]           ? sce_pad[11] : 0;
        data[1] ^= key_states[SDL_SCANCODE_O]           ? sce_pad[12] : 0;
        data[1] ^= key_states[SDL_SCANCODE_8]           ? sce_pad[13] : 0;
        data[1] ^= key_states[SDL_SCANCODE_9]           ? sce_pad[14] : 0;
        data[1] ^= key_states[SDL_SCANCODE_U]           ? sce_pad[15] : 0;
    }

    return 1;
}

/// 4: STANDARD CONTROLLER (Dualshock)
/// 7: ANALOG CONTROLLER (Dualshock 2)
int scePadInfoMode(int port, int slot, int term, int offs)
{
    return 5;
}

int scePadSetMainMode(int port, int slot, int offs, int lock)
{
}

int scePadInfoAct(int port, int slot, int actno, int term)
{
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
}

int scePadInfoPressMode(int port, int slot)
{
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
