#include "mikupan/mikupan_region.h"
#ifndef MAIN_GAMEMAIN_H
#define MAIN_GAMEMAIN_H

#include "typedefs.h"

#include <stdint.h>


MIKUPAN_NAMESPACE_BEGIN

extern int init_load_id;
extern int64_t init_load_adr;
extern int init_load_size;

void GameMain();
int GameInit();
int GameInitLoad();
void GameModeChange(u_char mode);

#ifdef BUILD_EU_VERSION
int LanguageSelectMain();
#endif


MIKUPAN_NAMESPACE_END
#endif // MAIN_GAMEMAIN_H
