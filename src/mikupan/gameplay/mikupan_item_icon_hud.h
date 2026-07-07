#ifndef MIKUPAN_GAMEPLAY_MIKUPAN_ITEM_ICON_HUD_H
#define MIKUPAN_GAMEPLAY_MIKUPAN_ITEM_ICON_HUD_H

#include "typedefs.h"

void MikuPan_QuickSwapFinderFilm(void);
int MikuPan_ConsumeFinderFilmCounterSnap(void);
void MikuPan_DrawFinderFilmIcon(short int pos_x, short int pos_y, u_char fade_alpha);
void MikuPan_DrawMirrorStoneHudIcon(short int pos_x, short int pos_y, u_char fade_alpha);
void MikuPan_DrawHealingItemHudIcon(short int pos_x, short int pos_y, u_char fade_alpha);
void MikuPan_SetMirrorStoneHudEnabled(int enabled);
int MikuPan_MirrorStoneHudEnabled(void);
void MikuPan_ResetItemIconHud(void);

#endif
