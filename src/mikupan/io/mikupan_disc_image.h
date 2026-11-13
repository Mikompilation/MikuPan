#ifndef MIKUPAN_DISC_IMAGE_H
#define MIKUPAN_DISC_IMAGE_H

#include <memory>
#include <string>

struct SDL_IOStream;

/// Reads the game straight from an ISO 9660 disc image, so nothing has to be
/// extracted. Files are named by their path on the disc ("IMG_BD.BIN",
/// "MOVIE/SCN0010.PSS") in any case, and are read through an SDL_IOStream
/// that covers just that file.

/// The directory of one disc image. It never changes once made, so any
/// thread may read through it.
struct MikuPan_DiscIndex;

/// Indexes the image at `path` (a plain path or an Android content URI). On
/// failure returns NULL and `error` says why, in words for the player.
std::shared_ptr<const MikuPan_DiscIndex>
MikuPan_DiscImageIndex(const char* path, std::string* error);
bool MikuPan_DiscIndexHasFile(const MikuPan_DiscIndex& image,
                              const std::string& name);
/// Each stream has its own handle on the image, so several threads can read
/// at once. NULL when the file is not on the image.
SDL_IOStream* MikuPan_DiscIndexOpenFile(const MikuPan_DiscIndex& image,
                                        const std::string& name);

/// The mounted image: the disc the game itself is read from.

/// Indexes the image at `path` and makes it the mounted one. On failure
/// whatever was mounted stays mounted and `error` says why, in words for the
/// player.
bool MikuPan_DiscImageMount(const char* path, std::string* error);
void MikuPan_DiscImageUnmount();
bool MikuPan_DiscImageHasFile(const std::string& name);
/// NULL when the file is not on the mounted image.
SDL_IOStream* MikuPan_DiscImageOpenFile(const std::string& name);

#endif // MIKUPAN_DISC_IMAGE_H
