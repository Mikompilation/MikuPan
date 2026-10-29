#ifndef MIKUPAN_SAVE_REGION_H
#define MIKUPAN_SAVE_REGION_H
#include "mikupan_save_codec.h"
namespace MikuPanSave
{
enum class SaveRegion
{
    JP,
    US,
    EU
};
#ifdef BUILD_EU_VERSION
inline constexpr SaveRegion BuildRegion = SaveRegion::EU;
#else
inline constexpr SaveRegion BuildRegion = SaveRegion::US;
#endif
const char* RegionLabel(SaveRegion region);
const char* RegionPrefix(SaveRegion region);
std::string RegionFolder(SaveRegion region);
std::string RegionNotes(SaveRegion source, SaveRegion target);
bool ValidateRegion(const Bytes& data, SaveRegion source, std::string& error);
bool ConvertRegion(const Bytes& data, SaveRegion source, SaveRegion target,
                   Bytes& output, std::string& error, unsigned language = 0,
                   bool display_60hz = false);
}// namespace MikuPanSave
#endif
