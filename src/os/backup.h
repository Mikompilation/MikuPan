#include "mikupan/mikupan_region.h"
#ifndef OS_BACKUP_H
#define OS_BACKUP_H

#include "typedefs.h"


MIKUPAN_NAMESPACE_BEGIN

void SaveDataSet(u_char *addr0);
void SavePCFile();
void LoadPCFile();
void LoadDataDevelop(u_char *addr0);


MIKUPAN_NAMESPACE_END
#endif // OS_BACKUP_H
