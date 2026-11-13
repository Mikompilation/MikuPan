#ifndef MIKUPAN_UNDUB_H
#define MIKUPAN_UNDUB_H

/// The undub: the voices, sound effects and movies of the Japanese release,
/// Zero (SLPS-25074), and Miku's original model, taken from a Japanese disc
/// the player owns. The rest of the game stays as this release has it.
///
/// It swaps archive entries, as a disc swap would. IMG_HD.BIN and IMG_BD.BIN
/// are served as one archive whose swapped entries point past the end of
/// this release's IMG_BD.BIN, into the Japanese one, so every reader of the
/// archive (CDVD loads, ADPCM streams, the texture bake) sees the same
/// entries. The Japanese model is baked like any other model, into a pack of
/// its own. The movies are read from the Japanese disc by name, and the
/// Japanese SE parameters and stream volumes go with the Japanese banks and
/// streams.
///
/// It is chosen once, at start-up: a change applies on the next launch.

#ifdef __cplusplus
#include <string>

struct SDL_IOStream;

extern "C" {
#else
#include <stdbool.h>
#endif

/// Mounts the Japanese disc and takes the swapped entries from it when the
/// undub is switched on. Called once the game data has been found, before
/// anything reads the archive; later calls do nothing.
void MikuPan_Undub_Activate(void);
/// Turns the undub off again before the game has started, when the Japanese
/// data cannot be used after all; `reason` is shown on the options screen.
void MikuPan_Undub_Deactivate(const char *reason);
/// Whether this run plays the Japanese data.
bool MikuPan_Undub_IsActive(void);
/// GetAdpcmVol(): the Japanese volume of a stream while the undub is active.
bool MikuPan_Undub_GetAdpcmVolume(int file_no, unsigned short *vol);

/// File picker for the Japanese disc (an .iso, or IMG_HD.BIN / IMG_BD.BIN
/// of a folder holding the disc's files). The choice is checked, and saved
/// with the undub switched on, by MikuPan_Undub_ServiceBrowser() on the
/// main thread.
void MikuPan_Undub_BrowseForDisc(void);
void MikuPan_Undub_ServiceBrowser(void);

#ifdef __cplusplus
}

/// The file layer (mikupan_file.cpp).

/// What the file layer hands out for `relative` (IMG_HD.BIN, IMG_BD.BIN or a
/// movie) while the undub is active; empty to resolve it as usual.
std::string MikuPan_Undub_GetDataPath(const std::string& relative);
bool MikuPan_Undub_IsUndubPath(const std::string& path);
SDL_IOStream *MikuPan_Undub_OpenStream(const std::string& path);

/// The options screen.

/// What the undub does this run, or why it cannot, in words for the player.
std::string MikuPan_Undub_GetStatus();
/// Whether the settings differ from what this run started with.
bool MikuPan_Undub_IsRestartPending();
#endif

#endif // MIKUPAN_UNDUB_H
