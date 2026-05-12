#ifndef MIKUPAN_MIKUPAN_CONTROLLER_H
#define MIKUPAN_MIKUPAN_CONTROLLER_H

#include <SDL3/SDL_gamepad.h>

#define MIKUPAN_CONTROLLER_BIND_NONE   (0)
#define MIKUPAN_CONTROLLER_BIND_BUTTON (1)
#define MIKUPAN_CONTROLLER_BIND_AXIS   (2)

typedef struct
{
    /// MIKUPAN_CONTROLLER_BIND_NONE / MIKUPAN_CONTROLLER_BIND_BUTTON / MIKUPAN_CONTROLLER_BIND_AXIS
    int kind;

    /// SDL_GamepadButton or SDL_GamepadAxis (depending on kind)
    int code;
} MikuPan_ControllerBindings;

#define MIKUPAN_CONTROLLER_LOGICAL_COUNT (16)

/// Logical analog stick axes — index into the stick mapping arrays.
#define MIKUPAN_STICK_LX    (0)
#define MIKUPAN_STICK_LY    (1)
#define MIKUPAN_STICK_RX    (2)
#define MIKUPAN_STICK_RY    (3)
#define MIKUPAN_STICK_COUNT (4)

typedef struct
{
    /// SDL_GamepadAxis (LEFTX / LEFTY / RIGHTX / RIGHTY). -1 when unbound.
    int axis;

    /// Non-zero to invert the axis (e.g. for inverted-Y preference).
    int invert;
} MikuPan_StickGamepadBinding;

typedef struct
{
    /// Scancode that drives the axis to its negative extreme (up / left). 0 = unbound.
    int neg_scancode;

    /// Scancode that drives the axis to its positive extreme (down / right). 0 = unbound.
    int pos_scancode;
} MikuPan_StickKeyboardBinding;

extern MikuPan_ControllerBindings   mikupan_controller_map[MIKUPAN_CONTROLLER_LOGICAL_COUNT];
extern int                          mikupan_keyboard_map[MIKUPAN_CONTROLLER_LOGICAL_COUNT];
extern MikuPan_StickGamepadBinding  mikupan_stick_controller_map[MIKUPAN_STICK_COUNT];
extern MikuPan_StickKeyboardBinding mikupan_stick_keyboard_map[MIKUPAN_STICK_COUNT];

int MikuPan_OpenController();
int MikuPan_ReadController(unsigned char* rdata);
void MikuPan_ControllerResetBindings(void);
const char *MikuPan_ControllerBindingLabel(MikuPan_ControllerBindings binding);
const char *MikuPan_ControllerScanCodeLabel(int scancode);
const char *MikuPan_ControllerStickAxisLabel(int sdl_axis);
SDL_Gamepad* MikuPan_GetController(void);
int MikuPan_ControllerRumble(const unsigned char* data);
void MikuPan_ControllerDrawRemapWindow(void);


#endif//MIKUPAN_MIKUPAN_CONTROLLER_H
