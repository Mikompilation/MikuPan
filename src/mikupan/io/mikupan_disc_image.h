#ifndef MIKUPAN_DISC_IMAGE_H
#define MIKUPAN_DISC_IMAGE_H

#include <string>

struct SDL_IOStream;

/// Reads the game straight from an ISO 9660 disc image, so nothing has to be
/// extracted. Files are named by their path on the disc ("IMG_BD.BIN",
/// "MOVIE/SCN0010.PSS") in any case, and are read through an SDL_IOStream
/// that covers just that file.

/// Indexes the image at `path` (a plain path or an Android content URI) and
/// makes it the mounted one. On failure whatever was mounted stays mounted
/// and `error` says why, in words for the player.
bool MikuPan_DiscImageMount(const char* path, std::string* error);
void MikuPan_DiscImageUnmount();
bool MikuPan_DiscImageHasFile(const std::string& name);
/// Each stream has its own handle on the image, so several threads can read
/// at once. NULL when the file is not on the mounted image.
SDL_IOStream* MikuPan_DiscImageOpenFile(const std::string& name);

#endif // MIKUPAN_DISC_IMAGE_H
