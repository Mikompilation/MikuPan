#ifndef MIKUPAN_MIKUPAN_TEXTOVERRIDE_H
#define MIKUPAN_MIKUPAN_TEXTOVERRIDE_H

#include <cstdint>

#ifdef __cplusplus
extern "C" {
#endif

// Extracts every ig_msg category plus every map's event/movie/special-event dialog (read straight out of IMG_HD.BIN/IMG_BD.BIN) and writes one game_text.po under resources/mods/text/<lang>/ -- one language on NTSC-U (en only), all 5 on the PAL/EU build.
void MikuPan_GameTextExtractAllPOs(void);

// Looks up `category` (e.g. "ig_msg#24", "m0_event#msg" -- the msgid prefix before the last "#<msg_no>" in game_text.po) + `msg_no`. If mikupan_configuration.text_mods_enabled is on and that category has at least one override, returns a host pointer into a MikuPan-owned buffer holding the WHOLE category (original bytes for untouched messages, re-encoded bytes for overridden ones) -- never a lone per-message buffer, since some game code (item.cpp's ChkPageYW) walks between neighboring messages by raw pointer arithmetic and needs them contiguous. Returns 0 if mods are off, the category has no overrides, `msg_no` is out of range, or the category couldn't be rebuilt.
int64_t MikuPan_GetTextModAddr(const char *category, int msg_no);

#ifdef __cplusplus
}
#endif

#endif
