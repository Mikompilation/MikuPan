#ifndef MIKUPAN_MIKUPAN_FILE_C_H
#define MIKUPAN_MIKUPAN_FILE_C_H
#include "typedefs.h"
#include <stdint.h>

void MikuPan_LoadImgHdFile();
void MikuPan_ReadFullFile(const char *filename, char *buffer);
void MikuPan_ReadFileInArchive(int sector, int size, u_int *address);
void MikuPan_ReadFileInArchive64(int sector, int size, int64_t address);
u_int MikuPan_GetFileSize(const char *filename);
u_char MikuPan_OpenFile(const char *filename);
int MikuPan_ReadFile(int fd, void *buffer, int size);
int MikuPan_WriteFile(const char *filename, int fd, void *buffer, int size);
int MikuPan_CreateDirectory(const char *path);
void MikuPan_CloseFD();
int GetFD();

#endif//MIKUPAN_MIKUPAN_FILE_C_H
