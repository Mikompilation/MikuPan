#ifndef MIKUPAN_MIKUPAN_FILE_C_H
#define MIKUPAN_MIKUPAN_FILE_C_H
#include "typedefs.h"
#include <stdint.h>

void MikuPan_LoadImgHdFile();
void MikuPan_ReadFullFile(const char *filename, char *buffer);
void MikuPan_ReadFileInArchive(int sector, int size, u_int *address);
void MikuPan_ReadFileInArchive64(int sector, int size, int64_t address);
u_int MikuPan_GetFileSize(const char *filename);
u_char MikuPan_OpenFile(const char *filename, void *buffer, int size);
u_char MikuPan_SaveFile(const char *filename, void *buffer, int size);

#endif//MIKUPAN_MIKUPAN_FILE_C_H
