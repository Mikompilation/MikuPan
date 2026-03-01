#include "libmc.h"
#include "mikupan/mikupan_file_c.h"
#include "mikupan/mikupan_logging_c.h"
#include "typedefs.h"
#include <stddef.h>

int fd;

int sceMcInit()
{
    return MikuPan_CreateDirectory("saves");
}

int sceMcSync(int mode, int *cmd, int *result)
{
    if (cmd != NULL)
    {
        *cmd = 0;
        *result = 0;
    }

    return 1;
}

int sceMcDelete(int port, int slot, const char *name)
{
    return 0;
}

int sceMcFormat(int port, int slot)
{
    return 0;
}

int sceMcMkdir(int port, int slot, const char *name)
{
    // Likely needs a constructed path based on port or slot + name
    return MikuPan_CreateDirectory(name);
}

int sceMcGetInfo(int port, int slot, int *type, int *free, int *format)
{
    *type = 2;
    *free = 1000000;
    *format = 1;
    return 0;
}

int sceMcWrite(int fd, const void *buffer, int size)
{
    return MikuPan_WriteFile("", fd, buffer, size);
}

int sceMcRead(int fd, void *buffer, int size)
{
    return MikuPan_ReadFile(fd, buffer, size);
}

int sceMcClose(int fd)
{
    return MikuPan_CloseFD(fd);
}

int sceMcOpen(int port, int slot, const char *name, int mode)
{
    info_log("CARD NAME: %s", name);
    fd = MikuPan_OpenFile(name);
    return 1;
}

int sceMcGetDir(int port, int slot, const char *name, unsigned mode, int maxent,
                sceMcTblGetDir *table)
{
    info_log("Memory Card requested directory %s", name);
    return 0;
}