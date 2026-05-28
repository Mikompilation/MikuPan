#include "libpad.h"

#include "mikupan/mikupan_controller.h"


int scePadPortOpen(int port, int slot, void* addr)
{
    return MikuPan_OpenController();
}

int scePadInit(int mode)
{
    return 1;
}

int scePadGetState(int port, int slot)
{
    return scePadStateStable;
}

int scePadRead(int port, int slot, unsigned char* rdata)
{
    if (port == 0)
    {
        return MikuPan_ReadController(rdata);
    }

    return 0;
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
    return MikuPan_ControllerRumble(data);
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
    return MikuPan_ControllerRumble(data);
}