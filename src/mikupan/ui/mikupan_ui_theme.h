#ifndef MIKUPAN_MIKUPAN_UI_THEME_H
#define MIKUPAN_MIKUPAN_UI_THEME_H

#include "SDL3/SDL_video.h"

#define MIKUPAN_UI_THEME_COUNT 6
#define MIKUPAN_UI_FONT_COUNT 2

extern const char* const MikuPan_UiThemeLabels[MIKUPAN_UI_THEME_COUNT];
extern const char* const MikuPan_UiFontLabels[MIKUPAN_UI_FONT_COUNT];

void MikuPan_UiThemeInit(SDL_Window* window);
float MikuPan_UiThemeGetDisplayScale(void);

int MikuPan_ClampThemeIndex(int theme);
int MikuPan_ClampFontIndex(int font);
void MikuPan_ApplyUiFont(int font);
void MikuPan_ApplyUiFontScale(void);
void MikuPan_ApplyFatalFrameStyle(int theme);

#endif// MIKUPAN_MIKUPAN_UI_THEME_H
