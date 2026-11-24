#include "mikupan/mikupan_region.h"
#ifndef MIKUPAN_SAVE_PATHS_H
#define MIKUPAN_SAVE_PATHS_H
#include <filesystem>
#include <algorithm>
#include <charconv>
#include <cstdio>
#include <string>

MIKUPAN_NAMESPACE_BEGIN

namespace MikuPanSave
{
// SDL and ImGui use UTF-8; filesystem paths use UTF-16 on Windows.
inline std::string PathUtf8(const std::filesystem::path &path)
{
    const auto text = path.u8string();
    return {reinterpret_cast<const char *>(text.data()), text.size()};
}
inline bool IsModernSaveFilename(const std::filesystem::path& path)
{
    return PathUtf8(path.filename()).rfind("MikuPanSave", 0) == 0
           && path.extension() == ".dat";
}
inline bool IsPcSaveFilename(const std::filesystem::path& path,
                             const std::string& legacy_prefix)
{
    if (IsModernSaveFilename(path))
        return true;
    const std::string filename = PathUtf8(path.filename());
    if (legacy_prefix.empty() || filename.rfind(legacy_prefix, 0) != 0)
        return false;
    const auto suffix = filename.substr(legacy_prefix.size());
    return !suffix.empty() && std::all_of(suffix.begin(), suffix.end(),
        [](unsigned char ch) { return ch >= '0' && ch <= '9'; });
}
inline int PcSaveFileNumber(const std::filesystem::path& path,
                            const std::string& legacy_prefix)
{
    const std::string stem = PathUtf8(path.stem());
    const std::string prefix = stem.rfind("MikuPanSave", 0) == 0
                                   ? "MikuPanSave" : legacy_prefix;
    if (prefix.empty() || stem.rfind(prefix, 0) != 0 || stem.size() <= prefix.size())
        return -1;
    int number = -1;
    const char* first = stem.data() + prefix.size();
    const char* last = stem.data() + stem.size();
    const auto result = std::from_chars(first, last, number);
    return result.ec == std::errc() && result.ptr == last ? number : -1;
}
inline std::string ModernSaveFilename(int number)
{
    char name[64];
    std::snprintf(name, sizeof(name), "MikuPanSave%04d.dat", number);
    return name;
}
} // namespace MikuPanSave

MIKUPAN_NAMESPACE_END
#endif
