#ifndef MC_MC_H
#define MC_MC_H

#include "typedefs.h"

#include <stdint.h>

typedef enum {
	MC_DATA_ADDR_HOST,
	MC_DATA_ADDR_PS2,
} MC_DATA_ADDR_KIND;

typedef struct {
	uintptr_t addr;
	int size;
	MC_DATA_ADDR_KIND addr_kind;
} MC_DATA_STR;

#define MC_DATA_HOST(host_addr, data_size) \
	{ .addr = (uintptr_t)(host_addr), .size = (data_size), .addr_kind = MC_DATA_ADDR_HOST }

#define MC_DATA_PS2(ps2_addr, data_size) \
	{ .addr = (uintptr_t)(ps2_addr), .size = (data_size), .addr_kind = MC_DATA_ADDR_PS2 }

extern MC_DATA_STR mc_gamedata_str[];
extern MC_DATA_STR mc_albumdata_str[];
extern MC_DATA_STR mc_albumdata2_str[];
extern u_long mc_gamedata_str_num;
extern u_long mc_albumdata_str_num;

#endif // MC_MC_H
