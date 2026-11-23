#include "mikupan/mikupan_region.h"
#ifndef SCE_SIF_H
#define SCE_SIF_H
#include "common.h"

#include <stdint.h>


MIKUPAN_NAMESPACE_BEGIN

typedef struct {
	int64_t	data;
	int64_t	addr;
	unsigned int	size;
	unsigned int	mode;
} sceSifDmaData;

int sceSifDmaStat(unsigned int id);
unsigned int sceSifSetDma(sceSifDmaData *sdd, int len);


MIKUPAN_NAMESPACE_END
#endif // SCE_SIF_H
