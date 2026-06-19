#ifndef MIKUPAN_MIKUPAN_UI_SETTINGS_H
#define MIKUPAN_MIKUPAN_UI_SETTINGS_H

#define MIKUPAN_UI_THEME_COUNT 6
#define MIKUPAN_UI_FONT_COUNT 2

void MikuPan_UiSettingsInit(void);
void MikuPan_UiSettingsRender(void);

int MikuPan_ClampThemeIndex(int theme);
int MikuPan_ClampFontIndex(int font);
void MikuPan_ApplyUiFont(int font);
void MikuPan_ApplyUiFontScale(void);
void MikuPan_ApplyFatalFrameStyle(int theme);

#endif// MIKUPAN_MIKUPAN_UI_SETTINGS_H
