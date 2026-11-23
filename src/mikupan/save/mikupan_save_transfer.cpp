#include "mikupan/mikupan_region.h"
#include "mikupan_save_transfer.h"
#include "main/glob.h"
#include "mc/mc_exec.h"
#include "mc/mc_icon.h"
#include "mikupan/io/mikupan_file.h"
#include "os/eeiop/cdvd/eecdvd.h"
#include <limits>
#include "mikupan/mikupan_config.h"
#include "mikupan_memory_card.h"
#include "mikupan_save_paths.h"
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <set>
#include <system_error>
#ifdef _WIN32
#include <io.h>
#else
#include <fcntl.h>
#include <unistd.h>
#endif

MIKUPAN_NAMESPACE_BEGIN

namespace MikuPanSave
{
namespace
{
namespace fs = std::filesystem;
constexpr const char* Icon = "zero.ico";
std::string Name(int slot, SaveRegion region = BuildRegion)
{
    return std::string(RegionPrefix(region)) + std::to_string(slot);
}
uint32_t U32(const uint8_t* p)
{
    return uint32_t(p[0]) | uint32_t(p[1]) << 8 | uint32_t(p[2]) << 16
           | uint32_t(p[3]) << 24;
}
std::string Unique()
{
    static std::atomic<unsigned> n {0};
    return std::to_string(
               std::chrono::system_clock::now().time_since_epoch().count())
           + "-" + std::to_string(n++);
}
Bytes Read(const fs::path& p, size_t limit = 1024 * 1024)
{
    if (fs::is_symlink(p) || !fs::is_regular_file(p))
    {
        throw std::runtime_error("Missing or unsafe save file: "
                                 + PathUtf8(p.filename()));
    }
    auto size = fs::file_size(p);
    if (size > limit)
    {
        throw std::runtime_error("Save file is too large: "
                                 + PathUtf8(p.filename()));
    }
    std::ifstream f(p, std::ios::binary);
    Bytes data(size);
    if (!f || !f.read(reinterpret_cast<char*>(data.data()), size))
    {
        throw std::runtime_error("Cannot read save file: "
                                 + PathUtf8(p.filename()));
    }
    return data;
}
void Write(const fs::path& p, const Bytes& bytes, bool exclusive = false)
{
#ifdef _WIN32
    FILE* f = _wfopen(p.c_str(), exclusive ? L"wbx" : L"wb");
#else
    FILE* f = std::fopen(p.c_str(), exclusive ? "wbx" : "wb");
#endif
    if (!f)
    {
        throw std::runtime_error("Cannot create staged file: "
                                 + PathUtf8(p.filename()));
    }
    bool ok = std::fwrite(bytes.data(), 1, bytes.size(), f) == bytes.size()
              && std::fflush(f) == 0;
#ifdef _WIN32
    if (ok)
    {
        ok = _commit(_fileno(f)) == 0;
    }
#else
    if (ok)
    {
        ok = fsync(fileno(f)) == 0;
    }
#endif
    if (std::fclose(f) != 0)
    {
        ok = false;
    }
    if (!ok)
    {
        if (exclusive)
        {
            std::error_code ec;
            fs::remove(p, ec);
        }
        throw std::runtime_error(
            "Writing save data failed; original files were retained.");
    }
}
void SyncDirectory(const fs::path& path)
{
#ifndef _WIN32
    int fd = open(path.c_str(), O_RDONLY | O_DIRECTORY);
    if (fd < 0)
    {
        throw std::runtime_error("Cannot sync save directory.");
    }
    const int result = fsync(fd);
    const int error = errno;
    close(fd);
    // Some removable/network filesystems cannot flush directory handles.
    if (result != 0 && error != EINVAL && error != ENOTSUP)
    {
        throw std::runtime_error("Cannot sync save directory.");
    }
#endif
}
Folder Fresh(const Folder& source, bool ps2, SaveRegion region)
{
    Folder out;
    out.region = region;
    const auto system = source.files.find("icon.sys");
    const auto model = source.files.find(Icon);
    if (system != source.files.end() && model != source.files.end()
        && system->second.size() == 964
        && std::memcmp(system->second.data(), "PS2D", 4) == 0
        && !model->second.empty())
    {
        out.files["icon.sys"] = system->second;
        out.files[Icon] = model->second;
    }
    else
    {
        sceMcIconSys icon{};
        mcSetIconSys(&icon);
        std::strcpy(reinterpret_cast<char*>(icon.TitleName), mc_icon_title);
        const auto* bytes = reinterpret_cast<const uint8_t*>(&icon);
        out.files["icon.sys"] = Bytes(bytes, bytes + sizeof(icon));
        const IMG_ARRANGEMENT* arrangement = GetImgArrangementP(mc_iconFile[0]);
        if (arrangement == nullptr || arrangement->size == 0
            || arrangement->size > 1024 * 1024)
            throw std::runtime_error("Cannot load the PS2 browser icon from the game assets.");
        auto& data = out.files[Icon];
        data.resize(arrangement->size);
        if (!MikuPan_ReadDataFileRange("IMG_BD.BIN", int64_t(arrangement->start) * 2048,
                                     data.data(), data.size()))
            throw std::runtime_error("Cannot read the PS2 browser icon from the game assets.");
    }
    for (int i = 0; i < 3; ++i)
    {
        out.files[Name(i, region)] = EmptySlot(ps2 ? Ps2Size : NativeSize());
    }
    return out;
}
void WriteFolder(const fs::path& path, const Folder& folder, bool native)
{
    fs::create_directories(path);
    for (const auto& f : folder.files)
    {
        const auto destination = path
                  / (native
                             && f.first.compare(0, std::strlen(Prefix), Prefix)
                                    == 0
                         ? std::string(NativePrefix)
                               + f.first.substr(std::strlen(Prefix))
                         : f.first);
        if (fs::is_regular_file(destination) && Read(destination) == f.second)
            continue;
        Write(destination, f.second);
    }
    SyncDirectory(path);
    SyncDirectory(path.parent_path());
}
}// namespace
std::filesystem::path PcFolder()
{
    char path[4096] {};
    if (!MikuPan_ResolveUserPath(NativeFolderName, path, sizeof(path)))
    {
        return {};
    }
    return fs::u8path(path);
}
bool Inspect(const fs::path& path, bool native_allowed, Folder& folder,
             std::string& error)
{
    try
    {
        if (fs::is_symlink(path) || !fs::is_directory(path))
        {
            throw std::runtime_error(
                "Choose an extracted Fatal Frame save folder.");
        }
        Folder result;
        std::set<std::string> names = {"icon.sys", Icon};
        for (auto region : AllRegions)
        {
            if (native_allowed && region != BuildRegion)
                continue;
            for (int i = 0; i < 3; ++i)
            {
                names.insert(Name(i, region));
            }
        }
        for (const auto& e : fs::directory_iterator(path))
        {
            std::string n = PathUtf8(e.path().filename());
            if (native_allowed
                && n.compare(0, std::strlen(NativePrefix), NativePrefix) == 0)
            {
                n = std::string(Prefix) + n.substr(std::strlen(NativePrefix));
            }
            if (!names.count(n) && !(native_allowed && IsPcSaveFilename(fs::u8path(n), Prefix)))
            {
                continue;
            }
            result.files[n] = Read(e.path());
        }
        return InspectFiles(result.files, native_allowed, folder, error);
    }
    catch (const std::exception& e)
    {
        error = e.what();
        return false;
    }
}
bool InspectFiles(const SaveFiles& files, bool native_allowed, Folder& folder,
                  std::string& error)
{
    try
    {
        for (auto name : {"icon.sys", Icon})
        {
            if (!native_allowed && !files.count(name))
            {
                throw std::runtime_error("Missing PS2 browser icon files.");
            }
        }
        Folder result;
        result.files = files;
        bool identified = false;
        for (auto region : AllRegions)
        {
            if (native_allowed && region != BuildRegion)
                continue;
            if (files.count(Name(0, region)) || files.count(Name(1, region))
                || files.count(Name(2, region)))
            {
                if (identified)
                {
                    throw std::runtime_error(
                        "Save folder mixes multiple regions.");
                }
                result.region = region;
                identified = true;
            }
        }
        if (!identified && !native_allowed)
        {
            throw std::runtime_error(
                "Unsupported Fatal Frame save region/revision.");
        }
        if (!native_allowed)
        {
            const auto& icon = files.at("icon.sys");
            if (icon.size() != 964 || std::memcmp(icon.data(), "PS2D", 4)
                || files.at(Icon).empty())
            {
                throw std::runtime_error("Invalid PS2 browser icons.");
            }
        }
        std::vector<std::string> entries;
        if (native_allowed)
        {
            result.slots.clear();
            for (const auto& file : files)
                if (IsPcSaveFilename(fs::u8path(file.first), Prefix)
                    && !Empty(file.second))
                    entries.push_back(file.first);
            std::sort(entries.begin(), entries.end(), [](const auto& left, const auto& right) {
                const bool modern_left = IsModernSaveFilename(fs::u8path(left));
                const bool modern_right = IsModernSaveFilename(fs::u8path(right));
                if (modern_left != modern_right)
                    return modern_left;
                return PcSaveFileNumber(fs::u8path(left), Prefix)
                       > PcSaveFileNumber(fs::u8path(right), Prefix);
            });
            result.slots.resize(entries.size());
        }
        else
        {
            for (int i = 0; i < 3; ++i)
                entries.push_back(Name(i, result.region));
        }
        for (size_t i = 0; i < entries.size(); ++i)
        {
            auto& slot = result.slots[i];
            slot.filename = entries[i];
            if (!files.count(slot.filename))
            {
                slot.issue = "Missing gameplay slot: " + slot.filename;
                slot.description = "Unavailable save";
                continue;
            }
            const auto& data = files.at(slot.filename);
            slot.occupied = !Empty(data);
            if (data.size() != Ps2Size
                && (!native_allowed
                    || (data.size() != NativeSize()
                        && data.size() != NativeSize() - 160)))
            {
                slot.issue = "Unsupported save size/version for this build.";
                slot.description = "Unavailable save";
                continue;
            }
            if (!slot.occupied)
            {
                slot.description = "Empty";
                continue;
            }
            if (data.size() != Ps2Size && result.region != BuildRegion)
            {
                slot.issue = "PC save files must match this build's region.";
                slot.description = "Unavailable save";
                continue;
            }
            if (!(data.size() == Ps2Size
                      ? ValidateRegion(data, result.region, slot.issue)
                      : Validate(data, slot.issue)))
            {
                slot.issue += " This slot cannot be imported. Other compatible slots can still be selected.";
                slot.description = "Unavailable save";
                continue;
            }
            const auto* head = data.data() + 16;
            slot.player = "Miku";
            slot.chapter = U32(head + 4);
            slot.area = int(U32(head + 8));
            slot.hours = U32(head + 12);
            slot.minutes = U32(head + 16);
            slot.seconds = U32(head + 20);
            slot.finder = head[28] != 0;
            slot.description = "Miku | chapter " + std::to_string(slot.chapter)
                               + " | room " + std::to_string(slot.area);
        }
        folder = std::move(result);
        error.clear();
        return true;
    }
    catch (const std::exception& e)
    {
        // Keep identifiable damaged saves visible, but never transfer them.
        Folder damaged;
        unsigned regions = 0;
        for (auto region : AllRegions)
        {
            if (files.count(Name(0, region)) || files.count(Name(1, region))
                || files.count(Name(2, region)))
            {
                damaged.region = region;
                ++regions;
            }
        }
        if (regions == 1)
        {
            damaged.files = files;
            damaged.issue = e.what();
            for (int i = 0; i < 3; ++i)
            {
                auto& slot = damaged.slots[i];
                const auto found = files.find(Name(i, damaged.region));
                slot.occupied = found != files.end() && !Empty(found->second);
                slot.description = "Unavailable save";
                slot.issue = damaged.issue;
            }
            folder = std::move(damaged);
            error.clear();
            return true;
        }
        error = e.what();
        return false;
    }
}
}// namespace MikuPanSave

namespace MikuPanSave
{
bool ReadStorage(const fs::path& path, StorageKind kind, bool native_allowed,
                 Storage& storage, std::string& error, const SaveRegion* card_region)
{
    try
    {
        if (path.empty() && kind != StorageKind::BlankCard)
        {
            throw std::runtime_error(
                "Cannot resolve the save folder or card path.");
        }
        Storage result;
        result.kind = kind;
        result.path = path;
        if (native_allowed && kind == StorageKind::Folder)
            result.save.slots.clear();
        if (kind == StorageKind::BlankCard)
        {
            storage = std::move(result);
            error.clear();
            return true;
        }
        result.exists = fs::exists(path);
        if (result.exists)
        {
            if (kind == StorageKind::Card)
            {
                result.card = Read(path, 8650752);
                SaveFiles files;
                if (result.card.size() == 8388608
                    || result.card.size() == 8650752)
                {
                    for (auto region :
                         AllRegions)
                    {
                        SaveFiles candidate;
                        bool found = false;
                        if (!ReadMemoryCard(result.card, candidate, found,
                                            error,
                                            RegionFolder(region).c_str()))
                        {
                            return false;
                        }
                        if (found)
                        {
                            result.regions.push_back(region);
                            if ((!card_region && !result.has_save)
                                || (card_region && region == *card_region))
                            {
                                result.has_save = true;
                                files = std::move(candidate);
                            }
                        }
                    }
                }
                else
                {
                    std::string directory;
                    if (!ReadSaveArchive(result.card, directory, files, error))
                    {
                        return false;
                    }
                    // Archive display names can differ; the internal files identify the game/region.
                    result.has_save = true;
                }
                if (result.has_save
                    && !InspectFiles(files, false, result.save, error))
                {
                    return false;
                }
            }
            else
            {
                if (!Inspect(path, native_allowed, result.save, error))
                {
                    return false;
                }
                result.has_save = std::any_of(result.save.slots.begin(), result.save.slots.end(),
                    [](const SlotInfo& slot) { return slot.occupied; });
            }
        }
        else if (kind == StorageKind::Card)
        {
            throw std::runtime_error("Choose an existing memory-card file.");
        }
        storage = std::move(result);
        error.clear();
        return true;
    }
    catch (const std::exception& e)
    {
        error = e.what();
        return false;
    }
}
bool BuildBatch(const Folder& source, const Folder* destination,
                const std::vector<SlotMapping>& slots, bool export_ps2,
                Folder& result, std::string& error, SaveRegion output_region)
{
    try
    {
        if (slots.empty() || (export_ps2 && slots.size() > 3))
        {
            throw std::runtime_error("Select between one and three saves.");
        }
        Folder checked_source, checked_destination;
        if (!InspectFiles(source.files, export_ps2, checked_source, error))
        {
            return false;
        }
        if (destination
            && !InspectFiles(destination->files, !export_ps2, checked_destination, error))
        {
            return false;
        }
        if (!checked_source.issue.empty())
        {
            throw std::runtime_error(checked_source.issue);
        }
        if (destination && !checked_destination.issue.empty())
        {
            throw std::runtime_error(checked_destination.issue);
        }
        const auto region = export_ps2 ? output_region : BuildRegion;
        if (destination && destination->region != region)
        {
            throw std::runtime_error(
                "Destination save region does not match the selected output.");
        }
        Folder next;
        if (destination)
            next = checked_destination;
        else if (export_ps2)
            next = Fresh(source, true, region);
        else
            next.region = region;
        int highest = 0;
        if (!export_ps2)
            for (const auto& file : next.files)
                if (IsModernSaveFilename(fs::u8path(file.first)))
                    highest = std::max(highest, PcSaveFileNumber(fs::u8path(file.first), Prefix));
        std::set<int> sources, destinations;
        std::set<std::string> targets;
        for (const auto& slot : slots)
        {
            if (slot.source < 0 || size_t(slot.source) >= checked_source.slots.size()
                || (export_ps2 && (slot.destination < 0 || slot.destination >= 3))
                || (!export_ps2 && (slot.destination < -1
                    || (slot.destination >= 0 && (!destination
                        || size_t(slot.destination) >= checked_destination.slots.size())))))
                throw std::runtime_error("Assign a valid destination to each selected save.");
            if (!sources.insert(slot.source).second)
                throw std::runtime_error("Each source save can be used only once.");
            if (slot.destination >= 0 && !destinations.insert(slot.destination).second)
                throw std::runtime_error("Each destination save can be used only once.");
            const auto& source_slot = checked_source.slots[slot.source];
            if (!source_slot.issue.empty())
                throw std::runtime_error(source_slot.issue);
            if (!source_slot.occupied)
                throw std::runtime_error("Cannot transfer an empty slot.");
            std::string filename;
            if (export_ps2)
                filename = Name(slot.destination, region);
            else if (slot.destination >= 0
                     && IsModernSaveFilename(fs::u8path(checked_destination.slots[slot.destination].filename)))
                filename = checked_destination.slots[slot.destination].filename;
            else
            {
                if (slot.destination >= 0)
                    next.files.erase(checked_destination.slots[slot.destination].filename);
                if (highest == std::numeric_limits<int>::max())
                    throw std::runtime_error("No numbered PC save filenames remain available.");
                filename = ModernSaveFilename(++highest);
            }
            if (!targets.insert(filename).second)
                throw std::runtime_error("Each destination save can be used only once.");
            auto payload = checked_source.files.at(source_slot.filename);
            if (export_ps2)
            {
                if (!ToPs2(payload, payload, error)
                    || !ConvertRegion(payload, source.region, region, payload,
                                      error))
                {
                    return false;
                }
            }
            else
            {
                unsigned language = 0;
                bool display_60hz = false;
#ifdef BUILD_EU_VERSION
                language = sys_wrk.language;
                display_60hz = sys_wrk.pal_disp_mode != 0;
#endif
                if (!ConvertRegion(payload, source.region, region, payload,
                                   error, language, display_60hz)
                    || !ToNative(payload, payload, error))
                {
                    return false;
                }
            }
            next.files[filename] = std::move(payload);
        }
        if (!InspectFiles(next.files, !export_ps2, result, error)) return false;
        if (!result.issue.empty()) throw std::runtime_error(result.issue);
        for (const auto& filename : targets)
        {
            const auto saved = std::find_if(result.slots.begin(), result.slots.end(),
                [&](const SlotInfo& slot) { return slot.filename == filename; });
            if (saved == result.slots.end() || !saved->issue.empty())
                throw std::runtime_error(saved == result.slots.end()
                    ? "The transferred save is missing." : saved->issue);
        }
        return true;
    }
    catch (const std::exception& e)
    {
        error = e.what();
        return false;
    }
}
namespace
{
bool SameStorage(const Storage& expected, bool native, std::string& error)
{
    Storage current;
    const SaveRegion* region = expected.regions.empty() ? nullptr : &expected.save.region;
    return ReadStorage(expected.path, expected.kind, native, current, error, region)
           && current.exists == expected.exists
           && current.has_save == expected.has_save
           && current.card == expected.card
           && current.save.files == expected.save.files;
}
void CommitFolder(const Storage& expected, const Folder& folder, bool native,
                  std::string& error)
{
    const auto dest = fs::absolute(expected.path).lexically_normal();
    if (fs::is_symlink(dest))
    {
        throw std::runtime_error("Destination cannot be a symbolic link.");
    }
    fs::create_directories(dest.parent_path());
    const auto stage = dest.parent_path() / (".mikupan-transfer-" + Unique());
    const auto rollback =
        dest.parent_path() / (".mikupan-rollback-" + Unique());
    bool moved = false, installed = false;
    try
    {
        if (!fs::create_directory(stage))
        {
            throw std::runtime_error("Cannot create staging directory.");
        }
        // Preserve preview caches and unrelated files when replacing the folder.
        if (expected.exists)
        {
            fs::copy(dest, stage, fs::copy_options::recursive
                                     | fs::copy_options::copy_symlinks);
        }
        if (expected.exists)
        {
            for (const auto& entry : fs::recursive_directory_iterator(dest))
                if (!entry.is_symlink() && entry.is_regular_file())
                    fs::last_write_time(stage / fs::relative(entry.path(), dest),
                                        entry.last_write_time());
        }
        if (native)
        {
            for (const auto& file : expected.save.files)
            {
                const auto next = folder.files.find(file.first);
                if (next == folder.files.end())
                {
                    const auto name = file.first.compare(0, std::strlen(Prefix), Prefix) == 0
                        ? std::string(NativePrefix) + file.first.substr(std::strlen(Prefix))
                        : file.first;
                    fs::remove(stage / fs::u8path(name));
                }
                if (IsModernSaveFilename(fs::u8path(file.first))
                    && next != folder.files.end() && next->second != file.second)
                {
                    auto preview = stage / fs::u8path(file.first);
                    preview.replace_extension(".png");
                    fs::remove(preview);
                }
            }
        }
        WriteFolder(stage, folder, native);
        Folder verified;
        if (!Inspect(stage, native, verified, error)
            || verified.files != folder.files)
        {
            throw std::runtime_error("Staged save verification failed. "
                                     + error);
        }
        if (!SameStorage(expected, native, error))
        {
            throw std::runtime_error(
                "Destination changed; refresh before transferring.");
        }
        if (expected.exists)
        {
            fs::rename(dest, rollback);
            moved = true;
        }
        fs::rename(stage, dest);
        installed = true;
        SyncDirectory(dest.parent_path());
    }
    catch (...)
    {
        std::error_code ec;
        if (installed)
        {
            fs::rename(dest, stage, ec);
            if (ec)
            {
                throw std::runtime_error(
                    "Could not roll back the transfer; previous folder remains "
                    "at "
                    + PathUtf8(rollback));
            }
        }
        if (moved)
        {
            fs::rename(rollback, dest, ec);
            if (ec)
            {
                throw std::runtime_error(
                    "Could not restore the previous folder; it remains at "
                    + PathUtf8(rollback));
            }
        }
        fs::remove_all(stage, ec);
        throw;
    }
    std::error_code ec;
    if (moved)
    {
        fs::remove_all(rollback, ec);
    }
    if (ec)
    {
        error = "Transfer complete; could not remove temporary folder "
                + PathUtf8(rollback);
    }
}
void CommitCard(const fs::path& path, const Bytes& bytes)
{
    if (path.empty() || fs::exists(path) || fs::is_symlink(path))
    {
        throw std::runtime_error(
            "Choose a new output filename; existing cards are never "
            "overwritten.");
    }
    auto dest = fs::absolute(path).lexically_normal();
    fs::create_directories(dest.parent_path());
    const auto stage = dest.parent_path() / (".mikupan-card-" + Unique());
    bool published = false;
    try
    {
        Write(stage, bytes);
        // Copy the validated bytes into a new file; exclusive creation refuses
        // overwrites.
        Write(dest, bytes, true);
        published = true;
        fs::remove(stage);
        SyncDirectory(dest.parent_path());
    }
    catch (...)
    {
        std::error_code ec;
        fs::remove(stage, ec);
        if (published)
        {
            fs::remove(dest, ec);
        }
        throw;
    }
}
}// namespace
bool TransferBatch(const Storage& source, const Storage& destination,
                   const std::vector<SlotMapping>& slots, bool export_ps2,
                   const fs::path& card_output, std::string& error, bool ecc,
                   SaveArchiveFormat archive, SaveRegion output_region)
{
    try
    {
        if (source.kind == StorageKind::BlankCard || !source.has_save)
        {
            throw std::runtime_error(
                "Source contains no supported Fatal Frame saves.");
        }
        if ((!export_ps2 && destination.kind != StorageKind::Folder)
            || (export_ps2 && source.kind != StorageKind::Folder))
        {
            throw std::runtime_error(
                "Choose PC saves on the PC side of the transfer.");
        }
        if (destination.kind == StorageKind::Folder
            && source.kind == StorageKind::Folder
            && fs::weakly_canonical(source.path)
                   == fs::weakly_canonical(destination.path))
        {
            throw std::runtime_error(
                "Source and destination must be different folders.");
        }
        if (!SameStorage(source, export_ps2, error)
            || !SameStorage(destination, !export_ps2, error))
        {
            throw std::runtime_error(
                "Source or destination changed; refresh and review the slots "
                "again. "
                + error);
        }
        Folder next;
        if (!BuildBatch(source.save,
                        destination.has_save ? &destination.save : nullptr,
                        slots, export_ps2, next, error, output_region))
        {
            return false;
        }
        error.clear();
        if (destination.kind == StorageKind::Folder)
        {
            CommitFolder(destination, next, !export_ps2, error);
        }
        else
        {
            Bytes output;
            if (archive == SaveArchiveFormat::None)
            {
                if (!WriteMemoryCard(destination.card, next.files, output,
                                     error, ecc,
                                     RegionFolder(next.region).c_str()))
                {
                    return false;
                }
            }
            else if (!WriteSaveArchive(RegionFolder(next.region), next.files,
                                       archive, output, error))
            {
                return false;
            }
            if (!SameStorage(source, export_ps2, error)
                || !SameStorage(destination, false, error))
            {
                throw std::runtime_error(
                    "Save files changed while preparing the card; refresh "
                    "before exporting.");
            }
            CommitCard(card_output, output);
        }
        return true;
    }
    catch (const std::exception& e)
    {
        error = e.what();
        return false;
    }
}
}// namespace MikuPanSave

MIKUPAN_NAMESPACE_END
