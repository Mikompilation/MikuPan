#include "mikupan/mikupan_region.h"
#ifndef MIKUPAN_SAVE_REGION_H
#define MIKUPAN_SAVE_REGION_H
#include "mikupan_save_codec.h"
#include <array>

MIKUPAN_NAMESPACE_BEGIN

namespace MikuPanSave
{
enum class SaveRegion
{
    JP,
    US,
    EU,
    KR,
    Asia
};
#ifdef BUILD_EU_VERSION
inline constexpr SaveRegion BuildRegion = SaveRegion::EU;
#else
inline constexpr SaveRegion BuildRegion = SaveRegion::US;
#endif
inline constexpr std::array<SaveRegion, 5> AllRegions = {
    SaveRegion::JP, SaveRegion::US, SaveRegion::EU, SaveRegion::KR, SaveRegion::Asia
};
const char* RegionLabel(SaveRegion region);
const char* RegionPrefix(SaveRegion region);
std::string RegionFolder(SaveRegion region);
std::string RegionNotes(SaveRegion source, SaveRegion target);
bool ValidateRegion(const Bytes& data, SaveRegion source, std::string& error);
bool ConvertRegion(const Bytes& data, SaveRegion source, SaveRegion target,
                   Bytes& output, std::string& error, unsigned language = 0,
                   bool display_60hz = false);
}// namespace MikuPanSave

MIKUPAN_NAMESPACE_END
#endif
