#ifndef SCE_LIBPAD_H
#define SCE_LIBPAD_H

#define scePadDmaBufferMax	(16)

int scePadPortOpen(int port, int slot, void* addr);
int scePadInit(int mode);
int scePadGetState(int port, int slot);
int scePadRead(int port, int slot, unsigned char* rdata);
int scePadInfoMode(int port, int slot, int term, int offs);
int scePadSetMainMode(int port, int slot, int offs, int lock);
int scePadInfoAct(int port, int slot, int actno, int term);
int scePadSetActAlign(int port, int slot, const unsigned char* data);
int scePadGetReqState(int port, int slot);
int scePadInfoPressMode(int port, int slot);
int scePadEnterPressMode(int port, int slot);
int scePadSetActDirect(int port, int slot, const unsigned char* data);

#define scePadStateDiscon	    (0)
#define scePadStateFindPad	    (1)
#define scePadStateFindCTP1	    (2)
#define scePadStateExecCmd	    (5)
#define scePadStateStable	    (6)
#define scePadStateError	    (7)
#define scePadStateClosed	    (99)

#define scePadReqStateComplete	(0)
#define scePadReqStateFaild	    (1)
#define scePadReqStateFailed	(1)
#define scePadReqStateBusy	    (2)

#define InfoModeCurID		    (1)
#define InfoModeCurExID		    (2)
#define InfoModeCurExOffs	    (3)
#define InfoModeIdTable		    (4)

// -- Remappable input bindings ----------------------------------------------
// Each of the 16 logical PS2 buttons (indices 0..15, matching sce_pad[]) is
// driven by either a gamepad button, a gamepad axis (treated as boolean for
// triggers), or — in keyboard fallback mode — a single SDL scancode. The UI
// in mikupan_ui.c reads/writes these arrays directly to let the user remap
// controls; libpad.c reads them every scePadRead.

#define SCE_BIND_NONE   (0)
#define SCE_BIND_BUTTON (1)
#define SCE_BIND_AXIS   (2)

typedef struct
{
    int kind;  // SCE_BIND_NONE / SCE_BIND_BUTTON / SCE_BIND_AXIS
    int code;  // SDL_GamepadButton or SDL_GamepadAxis (depending on kind)
} SceGpBinding;

#define SCE_PAD_LOGICAL_COUNT (16)

extern SceGpBinding sce_gp_map[SCE_PAD_LOGICAL_COUNT];
extern int          sce_kb_map[SCE_PAD_LOGICAL_COUNT];

// Restore both maps to their compiled-in defaults. Called by the remap UI's
// "Reset to defaults" button.
void scePadResetBindings(void);

// Opaque accessor for the live SDL gamepad pointer — exposes just enough for
// the UI to query button/axis state for the live "which button is pressed"
// highlight without reaching across the libpad/SDL layer itself. Returns NULL
// when no gamepad is connected (keyboard fallback path).
struct SDL_Gamepad;
struct SDL_Gamepad *scePadGetSdlGamepad(void);

// Human-readable name for a binding — used by the remap UI to label each row.
// Returns a static string; never NULL.
const char *scePadBindingLabel(SceGpBinding binding);
const char *scePadScancodeLabel(int scancode);

#endif // SCE_LIBPAD_H
