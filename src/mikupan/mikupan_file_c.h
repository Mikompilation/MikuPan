#ifndef MIKUPAN_MIKUPAN_FILE_C_H
#define MIKUPAN_MIKUPAN_FILE_C_H
#include "typedefs.h"
#include <stdint.h>

typedef struct
{
    unsigned char Resv2,Sec,Min,Hour;
    unsigned char Day,Month;
    unsigned short Year;
} MikuPan_McStDateTime;

typedef struct
{
    MikuPan_McStDateTime _Create;
    MikuPan_McStDateTime _Modify;
    unsigned int FileSizeByte;
    unsigned short AttrFile;
    unsigned short Reserve1;
    unsigned int Reserve2;
    unsigned int PdaAplNo;
    unsigned char EntryName[32];
} MikuPan_McTblGetDir __attribute__((aligned (64)));

void MikuPan_LoadImgHdFile();
void MikuPan_ReadFullFile(const char *filename, char *buffer);
void MikuPan_ReadFileInArchive(int sector, int size, u_int *address);
void MikuPan_ReadFileInArchive64(int sector, int size, int64_t address);
u_int MikuPan_GetFileSize(const char *filename);
u_char MikuPan_ReadFile(const char *filename, void *buffer, int size);
u_char MikuPan_WriteFile(const char *filename, void *buffer, int size);
u_char MikuPan_CreateFolder(const char *folder);
u_char MikuPan_FolderExists(const char *folder);
int MikuPan_GetListFiles(const char *folder, MikuPan_McTblGetDir *table);
char* MikuPan_GetRelativePath(const char* path);

#endif//MIKUPAN_MIKUPAN_FILE_C_H
