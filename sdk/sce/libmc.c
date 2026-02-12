#include "libmc.h"
#include "mikupan/mikupan_file_c.h"
#include "mikupan/mikupan_logging_c.h"

int sceMcInit()
{
    return 1;
}

int sceMcSync(int mode, int *cmd, int *result)
{
    return 0;
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
    return 0;
}

int sceMcGetInfo(int port, int slot, int *type, int *free, int *format)
{
    return 0;
}

int sceMcWrite(int fd, const void *buffer, int size)
{
    return 0;
}

int sceMcRead(int fd, void *buffer, int size)
{
    return MikuPan_OpenFile("", buffer, size);
}

int sceMcClose(int fd)
{
    return 0;
}

int sceMcOpen(int port, int slot, const char *name, int mode)
{

    info_log("CARD NAME: %s\n", name);

    return 1;
}

int sceMcGetDir(int port, int slot, const char *name, unsigned mode, int maxent,
                sceMcTblGetDir *table)
{
    // Checks if files exist
    return 0;
}