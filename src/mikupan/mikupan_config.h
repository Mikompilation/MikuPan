#ifndef MIKUPAN_MIKUPAN_CONFIG_H
#define MIKUPAN_MIKUPAN_CONFIG_H

#include "mikupan_basictypes.h"

#ifdef __cplusplus
extern "C" {
#endif

extern MikuPan_Config mikupan_configuration;

void MikuPan_LoadConfiguration(const char *filename);
int MikuPan_SaveConfiguration(const char *filename);

#ifdef __cplusplus
}
#endif

#endif//MIKUPAN_MIKUPAN_CONFIG_H
