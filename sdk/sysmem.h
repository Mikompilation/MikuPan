#ifndef MIKUPAN_IOP_SYSMEM_H
#define MIKUPAN_IOP_SYSMEM_H

#include <stddef.h>

void* AllocSysMemory(int type, unsigned int size, void* addr);
int FreeSysMemory(void* ptr);
int QueryMemSize(void);
int QueryTotalFreeMemSize(void);
int QueryMaxFreeMemSize(void);

#endif
