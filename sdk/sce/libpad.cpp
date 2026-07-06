#include "libpad.h"

#include "mikupan/io/mikupan_controller.h"


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

static constexpr int scePadPrimaryPort = 0;
static constexpr int scePadCommandRejected = 0;
static constexpr int scePadCommandAccepted = 1;
static constexpr int scePadActuatorCount = 2;
static constexpr int scePadActuatorSupported = 1;
static constexpr int scePadActuatorUnsupported = 0;

static int scePadCommandResultForPort(int port)
{
    return port == scePadPrimaryPort ? scePadCommandAccepted : scePadCommandRejected;
}

int scePadSetMainMode(int port, int slot, int offs, int lock)
{
    return scePadCommandResultForPort(port);
}

int scePadInfoAct(int port, int slot, int actno, int term)
{
    if (port != scePadPrimaryPort)
    {
        return scePadActuatorUnsupported;
    }

    if (actno < 0)
    {
        return scePadActuatorCount;
    }

    return actno < scePadActuatorCount ? scePadActuatorSupported : scePadActuatorUnsupported;
}

int scePadSetActAlign(int port, int slot, const unsigned char* data)
{
    return scePadCommandResultForPort(port);
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
    if (port != scePadPrimaryPort)
    {
        return scePadCommandRejected;
    }

    return MikuPan_ControllerRumble(data);
}