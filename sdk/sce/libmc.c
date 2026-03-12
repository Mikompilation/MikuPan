#include "libmc.h"
#include "mikupan/mikupan_file_c.h"
#include "mikupan/mikupan_logging_c.h"
#include "typedefs.h"
#include <stddef.h>
#include <string.h>

int retval = 0;
int mcRunCmdNo = 0;
char sifParamPath[0x3ff] = {0};

int sceMcInit()
{
    return sceMcExecFinish;
}

int sceMcSync(int mode, int *cmd, int *result)
{
    if (mcRunCmdNo == 0)
    {
        return sceMcExecIdle;
    }

    if (cmd != NULL)
    {
        *cmd = mcRunCmdNo;
    }

    mcRunCmdNo = 0;

    if (result != NULL)
    {
        *result = retval;
    }

    return sceMcExecFinish;
}

int sceMcDelete(int port, int slot, const char *name)
{
    mcRunCmdNo = SCE_MC_FUNC_DELETE;
    return sceMcExecRun;
}

int sceMcFormat(int port, int slot)
{
    mcRunCmdNo = SCE_MC_FUNC_FORMAT;
    return sceMcExecRun;
}

int sceMcMkdir(int port, int slot, const char *name)
{
    info_log("MC requested folder creation: %s", name);
    mcRunCmdNo = SCE_MC_FUNC_MKDIR;
    retval = MikuPan_CreateFolder(name);
    return sceMcExecRun;
}

int sceMcGetInfo(int port, int slot, int *type, int *free, int *format)
{
    *type = sceMcTypePS2;
    *free = 1000000;
    *format = 1;
    mcRunCmdNo = SCE_MC_FUNC_GETINFO;
    retval = sceMcResSucceed;
    return sceMcExecRun;
}

int sceMcWrite(int fd, const void *buffer, int size)
{
    info_log("MC requested file write: %s", sifParamPath);
    mcRunCmdNo = SCE_MC_FUNC_WRITE;
    retval = MikuPan_WriteFile(sifParamPath, buffer, size);
    return sceMcExecRun;
}

int sceMcRead(int fd, void *buffer, int size)
{
    mcRunCmdNo = SCE_MC_FUNC_READ;
    retval = MikuPan_ReadFile(sifParamPath, buffer, size);
    return sceMcExecRun;
}

int sceMcClose(int fd)
{
    mcRunCmdNo = SCE_MC_FUNC_CLOSE;
    retval = sceMcResSucceed;
    return sceMcExecRun;
}

int sceMcOpen(int port, int slot, const char *name, int mode)
{
    info_log("MC requested file: %s", name);
    strncpy(sifParamPath, name, sizeof(sifParamPath));
    mcRunCmdNo = SCE_MC_FUNC_OPEN;
    retval = sceMcResSucceed;
    return sceMcExecRun;
}

int sceMcGetDir(int port, int slot, const char *name, unsigned mode, int maxent,
                sceMcTblGetDir *table)
{
    if (name == NULL || *name == 0)
    {
        return sceMcErrNullStr;
    }

    info_log("MC requested directory %s", name);

    strncpy(sifParamPath, name, sizeof(sifParamPath));

    mcRunCmdNo = SCE_MC_FUNC_GETDIR;

    if (!MikuPan_FolderExists(name))
    {
        retval = sceMcResNoEntry;
    }
    else
    {
        retval = MikuPan_GetListFiles(name, (MikuPan_McTblGetDir*)table);
    }

    return sceMcExecRun;
}