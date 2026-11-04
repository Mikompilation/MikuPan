#include "mikupan/mikupan_region.h"
#ifndef MIKUPAN_IOP_SYSMEM_H
#define MIKUPAN_IOP_SYSMEM_H

#include <stddef.h>


MIKUPAN_NAMESPACE_BEGIN

void* AllocSysMemory(int type, unsigned int size, void* addr);
int FreeSysMemory(void* ptr);
int QueryMemSize(void);
int QueryTotalFreeMemSize(void);
int QueryMaxFreeMemSize(void);


MIKUPAN_NAMESPACE_END
#endif
