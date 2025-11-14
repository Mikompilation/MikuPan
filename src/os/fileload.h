#ifndef OS_FILELOAD_H
#define OS_FILELOAD_H

#include "typedefs.h"

#include <stdint.h>

int FileLoadInit();
void FileLoadNReq(int file_no, int64_t addr);
int FileLoadNEnd(int file_no);
void FileLoadB(int file_no, int64_t addr);

#endif // OS_FILELOAD_H
