#include "mikupan/mikupan_region.h"
#ifndef MIKUPAN_MIKUPAN_UI_CHEATS_H
#define MIKUPAN_MIKUPAN_UI_CHEATS_H


MIKUPAN_NAMESPACE_BEGIN

void MikuPan_CheatSetInventoryItem(int item_no, int value);
const char* MikuPan_CheatInventoryItemName(int item_no);
void MikuPan_UiInventoryItemSlider(int item_no);
void MikuPan_CheatSetAllMenuItems(int value);
void MikuPan_CheatSetPhotoMode(int enabled);
void MikuPan_CheatSyncPhotoMode(void);
void MikuPan_CheatMaxCoreInventory(void);
void MikuPan_CheatUnlockAllCostumes(void);
void MikuPan_CheatUnlockCameraMenu(void);
void MikuPan_CheatUnlockCameraAbilities(void);
void MikuPan_CheatMaxCameraUpgrades(void);
void MikuPan_CheatUnlockAll(void);
void MikuPan_UiInventoryCheatsMenu(void);
void MikuPan_UiUnlockCheatsMenu(void);
void MikuPan_UiCheatsRender(void);
int MikuPan_IsTofuModeEnabled(void);
const float* MikuPan_GetTofuColor(void);


MIKUPAN_NAMESPACE_END
#endif// MIKUPAN_MIKUPAN_UI_CHEATS_H
