#include "libpad.h"

#include "os/pad.h"

#include <SDL3/SDL_gamepad.h>

#include <stddef.h>

SDL_Gamepad *gamepad = NULL;

int scePadPortOpen(int port, int slot, void* addr)
{
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
    return 1;
}

int scePadInit(int mode)
{
    return 1;
}

int scePadGetState(int port, int slot)
{
    if (gamepad == NULL)
    {
        return scePadStateDiscon;
    }

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

    data[1] ^= SDL_GetGamepadButton(gamepad, SDL_GAMEPAD_BUTTON_NORTH)      ? sce_pad[0] : 0; // Verified
    data[1] ^= SDL_GetGamepadButton(gamepad, SDL_GAMEPAD_BUTTON_SOUTH)      ? sce_pad[1] : 0; // Verified
    data[1] ^= SDL_GetGamepadButton(gamepad, SDL_GAMEPAD_BUTTON_WEST)       ? sce_pad[2] : 0; // Verified
    data[1] ^= SDL_GetGamepadButton(gamepad, SDL_GAMEPAD_BUTTON_EAST)       ? sce_pad[3] : 0; // Verified
    data[1] ^= SDL_GetGamepadButton(gamepad, SDL_GAMEPAD_BUTTON_DPAD_UP)    ? sce_pad[4] : 0; // Verified
    data[1] ^= SDL_GetGamepadButton(gamepad, SDL_GAMEPAD_BUTTON_DPAD_DOWN)  ? sce_pad[5] : 0; // Verified
    data[1] ^= SDL_GetGamepadButton(gamepad, SDL_GAMEPAD_BUTTON_DPAD_LEFT)  ? sce_pad[6] : 0; // Verified
    data[1] ^= SDL_GetGamepadButton(gamepad, SDL_GAMEPAD_BUTTON_DPAD_RIGHT) ? sce_pad[7] : 0; // Verified

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
}

int scePadGetReqState(int port, int slot)
{
}

int scePadInfoPressMode(int port, int slot)
{
}

int scePadEnterPressMode(int port, int slot)
{
}

int scePadSetActDirect(int port, int slot, const unsigned char* data)
{
}
