#pragma once
#include "engine_api.h"

#include <cstdint>

enum class GameDataCheck
{
    Playable,
    /// A read failed, or a file is not there.
    Unreadable,
    /// Not the index of a release this executable has an engine for.
    UnsupportedRelease,
    /// IMG_HD.BIN and IMG_BD.BIN are of different releases, or cut short.
    Mismatched,
};

/// Every release has its own number of files, and IMG_HD.BIN an 8-byte entry
/// for each, so its size tells the releases apart.
bool RegionOfIndexSize(int64_t size, MikuPanHost::Region* region);

/// Whether IMG_HD.BIN and IMG_BD.BIN belong together and can be played, and
/// by which engine. The streams stay the caller's; either may be NULL.
GameDataCheck CheckGameData(SDL_IOStream* index, SDL_IOStream* archive,
                            MikuPanHost::Region* region);
