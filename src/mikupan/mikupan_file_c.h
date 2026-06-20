#ifndef MIKUPAN_MIKUPAN_FILE_C_H
#define MIKUPAN_MIKUPAN_FILE_C_H
#include "typedefs.h"
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

typedef struct
{
    unsigned char Resv2,Sec,Min,Hour;
    unsigned char Day,Month;
    unsigned short Year;
} MikuPan_McStDateTime;

TYPEDEF_ALIGNED(64, struct
{
    MikuPan_McStDateTime _Create;
    MikuPan_McStDateTime _Modify;
    unsigned int FileSizeByte;
    unsigned short AttrFile;
    unsigned short Reserve1;
    unsigned int Reserve2;
    unsigned int PdaAplNo;
    unsigned char EntryName[32];
} MikuPan_McTblGetDir);

void MikuPan_LoadImgHdFile();
void MikuPan_ServiceMissingDataFolderDialog();
void MikuPan_RequestDataFolderSelection(const char *missing_path);
bool MikuPan_HasRequiredDataFiles(char *missing_file, size_t missing_file_size);
/* Records that file data was loaded at a PS2 main-memory offset; requests a
   texture-cache flush only when an address is loaded over a second time. */
void MikuPan_NotifyPs2MemoryLoad(int ps2_address);
void MikuPan_ReadFullFile(const char *filename, char *buffer);
void MikuPan_ReadFileInArchive(int sector, int size, u_int *address);
void MikuPan_BufferFile(int sector, int size, int64_t address);
u_int MikuPan_GetFileSize(const char *filename);
u_char MikuPan_OpenFile(const char *filename, void *buffer, int size);
u_char MikuPan_SaveFile(const char *filename, void *buffer, int size);
bool MikuPan_ResolveCdPath(const char* path, char* buffer, size_t buffer_size);
bool MikuPan_ResolveBasePath(const char* path, char* buffer, size_t buffer_size);
u_char MikuPan_ReadFile(const char *filename, void *buffer, int size);
u_char MikuPan_WriteFile(const char *filename, const void *buffer, int size);
u_char MikuPan_CreateFolder(const char *folder);
u_char MikuPan_FolderExists(const char *folder);
int MikuPan_GetListFiles(const char *folder, MikuPan_McTblGetDir *table, int max_entries);

#endif//MIKUPAN_MIKUPAN_FILE_C_H
