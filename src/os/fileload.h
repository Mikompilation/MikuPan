#include "mikupan/mikupan_region.h"
#ifndef OS_FILELOAD_H
#define OS_FILELOAD_H

#include "typedefs.h"

#include <stdint.h>


MIKUPAN_NAMESPACE_BEGIN

int FileLoadInit();
void FileLoadNReq(int file_no, int64_t addr);
int FileLoadNEnd(int file_no);
void FileLoadB(int file_no, int64_t addr);


MIKUPAN_NAMESPACE_END
#endif // OS_FILELOAD_H
