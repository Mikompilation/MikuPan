#ifndef MIKUPAN_MIKUPAN_I18N_H
#define MIKUPAN_MIKUPAN_I18N_H

#ifdef __cplusplus
extern "C" {
#endif

/* Returns the translation for msgid in the current UI language (see
 * MikuPan_GetUiLanguage). Translations are loaded on first use from
 * resources/lang/<code>.po (fr/de/es/it) and cached in memory.
 *
 * Falls back to msgid itself when the current language is English, the
 * .po file for that language is missing, or msgid has no entry in it -
 * so callers can always pass the English string as msgid and get a sane
 * result even before every language is fully translated.
 */
const char *MikuPan_Translate(const char *msgid);

/* Drops every cached translation table so the next MikuPan_Translate call
 * reloads the .po files from disk. Useful for iterating on translations
 * without restarting the game.
 */
void MikuPan_TranslationsReset(void);

#ifdef __cplusplus
}
#endif

#endif
