#include "mikupan/mikupan_region.h"
#ifndef INGAME_IG_INIT_H
#define INGAME_IG_INIT_H

#include "typedefs.h"

// extern LOAD_START_WRK load_start_wrk;


MIKUPAN_NAMESPACE_BEGIN

void InitCamera();
void InitPlyr();
void InitPlyr2(int film_no);
void InitPlyrAfterLoad();
void InitEnemy();
void EnemyActDataLoad();
void InitFlyWrk();
void InitFilm();
void LoadStartDataInit();
int LoadStartDataSet();
void InitLoadStartLock();
int GetLoadStartLock();


MIKUPAN_NAMESPACE_END
#endif // INGAME_IG_INIT_H
