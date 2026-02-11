#ifndef MIKUPAN_FILE_LOADING_H
#define MIKUPAN_FILE_LOADING_H
#include <cstdint>
#include <typedefs.h>

extern "C" {
void MikuPan_LoadImgHdFile();
void MikuPan_ReadFullFile(const char *filename, char *buffer);
void MikuPan_ReadFileInArchive(int sector, int size, u_int *address);
void MikuPan_ReadFileInArchive64(int sector, int size, int64_t address);
u_int MikuPan_GetFileSize(const char *filename);
u_char MikuPan_OpenFile(const char *filename, void *buffer);
u_char MikuPan_SaveFile(const char *filename, void *buffer, int size);
}

#endif//MIKUPAN_FILE_LOADING_H