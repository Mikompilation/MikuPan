#ifndef SCE_LIBMC_H
#define SCE_LIBMC_H

typedef int _iconVu0IVECTOR[4];
typedef float _iconVu0FVECTOR[4];

typedef struct
{
	unsigned char Head[4];
	unsigned short Reserv1;
	unsigned short OffsLF;
	unsigned int Reserv2;
	unsigned int TransRate;
	_iconVu0IVECTOR BgColor[4];
	_iconVu0FVECTOR LightDir[3];
	_iconVu0FVECTOR LightColor[3];
	_iconVu0FVECTOR Ambient;
	unsigned char TitleName[68];
	unsigned char FnameView[64];
	unsigned char FnameCopy[64];
	unsigned char FnameDel[64];
	unsigned char Reserve3[512];
} sceMcIconSys;

typedef struct
{
	unsigned char Resv2,Sec,Min,Hour;
	unsigned char Day,Month;
	unsigned short Year;
} sceMcStDateTime;

typedef struct
{
	sceMcStDateTime _Create;
	sceMcStDateTime _Modify;
	unsigned int FileSizeByte;
	unsigned short AttrFile;
	unsigned short Reserve1;
	unsigned int Reserve2;
	unsigned int PdaAplNo;
	unsigned char EntryName[32];
} sceMcTblGetDir __attribute__((aligned (64)));

#define sceMcFuncNoCardInfo	    (1)
#define sceMcFuncNoOpen		    (2)
#define sceMcFuncNoClose	    (3)
#define sceMcFuncNoSeek		    (4)
#define sceMcFuncNoRead		    (5)
#define sceMcFuncNoWrite	    (6)
#define sceMcFuncNoFlush	    (10)
#define sceMcFuncNoMkdir	    (11)
#define sceMcFuncNoChDir	    (12)
#define sceMcFuncNoGetDir	    (13)
#define sceMcFuncNoFileInfo	    (14)
#define sceMcFuncNoDelete	    (15)
#define sceMcFuncNoFormat	    (16)
#define sceMcFuncNoUnformat	    (17)
#define sceMcFuncNoEntSpace	    (18)
#define sceMcFuncNoRename	    (19)
#define sceMcFuncChgPrior	    (20)
#define sceMcFuncSlotMax	    (21)
#define sceMcFuncNoReadFast	    (22)
#define sceMcFuncNoWriteFast	(27)

#define sceMcFileInfoCreate	0x01
#define sceMcFileInfoModify	0x02
#define sceMcFileInfoAttr	0x04

#define sceMcFileAttrReadable		SCE_STM_R
#define sceMcFileAttrWriteable		SCE_STM_W
#define sceMcFileAttrWritable		SCE_STM_W
#define sceMcFileAttrExecutable		SCE_STM_X
#define sceMcFileAttrDupProhibit	SCE_STM_C
#define sceMcFileAttrSubdir		    SCE_STM_D
#define sceMcFileAttrClosed		    0x0080
#define sceMcFileAttrPDAExec		0x0800
#define sceMcFileAttrPS1		    0x1000

#define sceMcResSucceed		    (0)
#define sceMcResChangedCard	    (-1)
#define sceMcResNoFormat	    (-2)
#define sceMcResFullDevice	    (-3)
#define sceMcResNoEntry		    (-4)
#define sceMcResDeniedPermit	(-5)
#define sceMcResNotEmpty	    (-6)
#define sceMcResUpLimitHandle	(-7)
#define sceMcResFailReplace	    (-8)
#define sceMcResChangedBank	    (-3)

#define sceMcIniSucceed		(0)
#define sceMcIniErrKernel	(-101)
#define sceMcIniOldMcserv	(-120)
#define sceMcIniOldMcman	(-121)
#define sceMcErrSifRpc		(-91)

#define sceMcErrUnbind		(-100)
#define sceMcErrSemapho		(-200)
#define sceMcErrNullStr		(-210)

#define sceMcTypeNoCard		(0)
#define sceMcTypePS1		(1)
#define sceMcTypePS2		(2)
#define sceMcTypePDA		(3)
#define sceMcTypePSBB		(4)

#define sceMcExecRun		(0)
#define sceMcExecIdle		(-1)
#define sceMcExecFinish		(1)

typedef enum SceMcFunc
{
    SCE_MC_FUNC_GETINFO                 = sceMcFuncNoCardInfo,
    SCE_MC_FUNC_OPEN                    = sceMcFuncNoOpen,
    SCE_MC_FUNC_CLOSE                   = sceMcFuncNoClose,
    SCE_MC_FUNC_SEEK                    = sceMcFuncNoSeek,
    SCE_MC_FUNC_READ                    = sceMcFuncNoRead,
    SCE_MC_FUNC_WRITE                   = sceMcFuncNoWrite,
    SCE_MC_FUNC_FLUSH                   = sceMcFuncNoFlush,
    SCE_MC_FUNC_MKDIR                   = sceMcFuncNoMkdir,
    SCE_MC_FUNC_CHDIR                   = sceMcFuncNoChDir,
    SCE_MC_FUNC_GETDIR                  = sceMcFuncNoGetDir,
    SCE_MC_FUNC_SETFILEINFO             = sceMcFuncNoFileInfo,
    SCE_MC_FUNC_DELETE                  = sceMcFuncNoDelete,
    SCE_MC_FUNC_FORMAT                  = sceMcFuncNoFormat,
    SCE_MC_FUNC_UNFORMAT                = sceMcFuncNoUnformat,
    SCE_MC_FUNC_GETENTSPACE             = sceMcFuncNoEntSpace,
    SCE_MC_FUNC_RENAME                  = sceMcFuncNoRename,
    SCE_MC_FUNC_CHANGETHREADPRIORITY    = sceMcFuncChgPrior,
    SCE_MC_FUNC_GETSLOTMAX              = sceMcFuncSlotMax,
    SCE_MC_FUNC_READFAST                = sceMcFuncNoReadFast,
    SCE_MC_FUNC_WRITEFAST               = sceMcFuncNoWriteFast,

    SCE_MC_FUNC_SYNC = 30,
    SCE_MC_FUNC_INIT,
    SCE_MC_FUNC_END
} SceMcFunc;

int sceMcInit();
int sceMcSync(int mode, int *cmd, int *result);
int sceMcDelete(int	port, int slot, const char *name);
int sceMcFormat(int port, int slot);
int sceMcMkdir(int port, int slot, const char *name);
int sceMcGetInfo(int port, int slot, int *type, int *free, int *format);
int sceMcWrite(int fd, const void *buffer, int size);
int sceMcRead(int fd, void *buffer, int size);
int sceMcClose(int fd);
int sceMcOpen(int port, int slot, const char *name, int mode);
int sceMcGetDir(int port, int slot, const char *name, unsigned mode, int maxent, sceMcTblGetDir *table);
int sceMcGetInfo(int port, int slot, int *type, int *free, int *format);

#endif // SCE_LIBMC_H