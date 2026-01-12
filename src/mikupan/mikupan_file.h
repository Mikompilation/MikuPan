#ifndef MIKUPAN_FILE_LOADING_H
#define MIKUPAN_FILE_LOADING_H
#include <cstdint>
#include <typedefs.h>

extern "C"
{
    void MikuPan_LoadImgHdFile();
    void MikuPan_ReadFullFile(const char *filename, char* buffer);
    void MikuPan_ReadFileInArchive(int sector, int size, u_int *address);
    u_int MikuPan_GetFileSize(const char *filename);
}

#endif //MIKUPAN_FILE_LOADING_H