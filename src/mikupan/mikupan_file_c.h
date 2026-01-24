#ifndef MIKUPAN_MIKUPAN_FILE_C_H
#define MIKUPAN_MIKUPAN_FILE_C_H
#include "typedefs.h"

void MikuPan_LoadImgHdFile();
void MikuPan_ReadFullFile(const char *filename, char* buffer);
void MikuPan_ReadFileInArchive(int sector, int size, u_int *address);
void MikuPan_ReadFileInArchive64(int sector, int size, int64_t address);
u_int MikuPan_GetFileSize(const char *filename);

#endif//MIKUPAN_MIKUPAN_FILE_C_H
