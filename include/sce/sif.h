#ifndef SCE_SIF_H
#define SCE_SIF_H
#include "common.h"

#include <stdint.h>

typedef struct {
	unsigned int	data;
	int64_t	addr;
	unsigned int	size;
	unsigned int	mode;
} sceSifDmaData;

int sceSifDmaStat(unsigned int id);
unsigned int sceSifSetDma(sceSifDmaData *sdd, int len);

#endif // SCE_SIF_H
