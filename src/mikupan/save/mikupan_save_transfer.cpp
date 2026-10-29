#include "mikupan_save_transfer.h"
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
namespace MikuPanSave
{
namespace
{
namespace fs = std::filesystem;
constexpr const char *Icon = "zero.ico";
std::string Name(int slot) { return std::string(Prefix) + std::to_string(slot); }
uint32_t U32(const uint8_t *p)
{
    return uint32_t(p[0]) | uint32_t(p[1]) << 8 | uint32_t(p[2]) << 16 | uint32_t(p[3]) << 24;
}
std::string Unique()
{
    static std::atomic<unsigned> n{0};
    return std::to_string(std::chrono::system_clock::now().time_since_epoch().count()) + "-" +
           std::to_string(n++);
}
Bytes Read(const fs::path &p, size_t limit = 1024 * 1024)
{
    if (fs::is_symlink(p) || !fs::is_regular_file(p))
        throw std::runtime_error("Missing or unsafe save file: " + PathUtf8(p.filename()));
    auto size = fs::file_size(p);
    if (size > limit)
        throw std::runtime_error("Save file is too large: " + PathUtf8(p.filename()));
    std::ifstream f(p, std::ios::binary);
    Bytes data(size);
    if (!f || !f.read(reinterpret_cast<char *>(data.data()), size))
        throw std::runtime_error("Cannot read save file: " + PathUtf8(p.filename()));
    return data;
}
void Write(const fs::path &p, const Bytes &bytes, bool exclusive = false)
{
#ifdef _WIN32
    FILE *f = _wfopen(p.c_str(), exclusive ? L"wbx" : L"wb");
#else
    FILE *f = std::fopen(p.c_str(), exclusive ? "wbx" : "wb");
#endif
    if (!f)
        throw std::runtime_error("Cannot create staged file: " + PathUtf8(p.filename()));
    bool ok = std::fwrite(bytes.data(), 1, bytes.size(), f) == bytes.size() && std::fflush(f) == 0;
#ifdef _WIN32
    if (ok)
        ok = _commit(_fileno(f)) == 0;
#else
    if (ok)
        ok = fsync(fileno(f)) == 0;
#endif
    if (std::fclose(f) != 0)
        ok = false;
    if (!ok)
    {
        if (exclusive)
        {
            std::error_code ec;
            fs::remove(p, ec);
        }
        throw std::runtime_error("Writing save data failed; original files were retained.");
    }
}
void SyncDirectory(const fs::path &path)
{
#ifndef _WIN32
    int fd = open(path.c_str(), O_RDONLY | O_DIRECTORY);
    if (fd < 0)
        throw std::runtime_error("Cannot sync save directory.");
    const int result = fsync(fd);
    const int error = errno;
    close(fd);
    // Some removable/network filesystems cannot flush directory handles.
    if (result != 0 && error != EINVAL && error != ENOTSUP)
        throw std::runtime_error("Cannot sync save directory.");
#endif
}
Folder Fresh(const Folder &source, bool ps2)
{
    Folder out;
    out.files["icon.sys"] = source.files.at("icon.sys");
    out.files[Icon] = source.files.at(Icon);
    for (int i = 0; i < 3; ++i)
        out.files[Name(i)] = EmptySlot(ps2 ? Ps2Size : NativeSize());
    return out;
}
void WriteFolder(const fs::path &path, const Folder &folder, bool native)
{
    fs::create_directories(path);
    for (const auto &f : folder.files)
        Write(path / (native && f.first.compare(0, std::strlen(Prefix), Prefix) == 0
                          ? std::string(NativePrefix) + f.first.substr(std::strlen(Prefix))
                          : f.first),
              f.second);
    SyncDirectory(path);
    SyncDirectory(path.parent_path());
}
} // namespace
std::filesystem::path PcFolder()
{
    char path[4096]{};
    if (!MikuPan_ResolveUserPath(NativeFolderName, path, sizeof(path)))
        return {};
    return fs::u8path(path);
}
bool Inspect(const fs::path &path, bool native_allowed, Folder &folder, std::string &error)
{
    try
    {
        if (fs::is_symlink(path) || !fs::is_directory(path))
            throw std::runtime_error("Choose the extracted save folder for this build's region.");
        Folder result;
        std::set<std::string> names = {"icon.sys", Icon};
        for (int i = 0; i < 3; ++i)
            names.insert(Name(i));
        for (const auto &e : fs::directory_iterator(path))
        {
            std::string n = PathUtf8(e.path().filename());
            if (native_allowed && n.compare(0, std::strlen(NativePrefix), NativePrefix) == 0)
                n = std::string(Prefix) + n.substr(std::strlen(NativePrefix));
            if (!names.count(n))
                throw std::runtime_error("Unexpected file in save folder: " + n);
            result.files[n] = Read(e.path());
        }
        return InspectFiles(result.files, native_allowed, folder, error);
    }
    catch (const std::exception &e)
    {
        error = e.what();
        return false;
    }
}
bool InspectFiles(const SaveFiles &files, bool native_allowed, Folder &folder, std::string &error)
{
    try
    {
        if (files.size() != 5)
            throw std::runtime_error(
                "A gameplay save folder must contain three slots and two icon files.");
        for (auto name : {"icon.sys", Icon})
            if (!files.count(name))
                throw std::runtime_error("Missing PS2 browser icon files.");
        Folder result;
        result.files = files;
        const auto &icon = files.at("icon.sys");
        if (icon.size() != 964 || std::memcmp(icon.data(), "PS2D", 4) || files.at(Icon).empty())
            throw std::runtime_error("Invalid PS2 browser icons.");
        for (int i = 0; i < 3; ++i)
        {
            if (!files.count(Name(i)))
                throw std::runtime_error("Missing gameplay slot: " + Name(i));
            const auto &data = files.at(Name(i));
            auto &slot = result.slots[i];
            if (data.size() != Ps2Size && (!native_allowed || (data.size() != NativeSize() &&
                                                               data.size() != NativeSize() - 160)))
                throw std::runtime_error("Unsupported save size/version for this build.");
            slot.occupied = !Empty(data);
            if (!slot.occupied)
            {
                slot.description = "Empty";
                continue;
            }
            if (!Validate(data, error))
                return false;
            const auto *head = data.data() + 16;
            slot.player = "Miku";
            slot.chapter = U32(head + 4);
            slot.area = int(U32(head + 8));
            slot.hours = U32(head + 12);
            slot.minutes = U32(head + 16);
            slot.seconds = U32(head + 20);
            slot.finder = head[28] != 0;
            slot.description = "Miku | chapter " + std::to_string(slot.chapter) + " | room " +
                               std::to_string(slot.area);
        }
        folder = std::move(result);
        error.clear();
        return true;
    }
    catch (const std::exception &e)
    {
        error = e.what();
        return false;
    }
}
} // namespace MikuPanSave

namespace MikuPanSave
{
bool ReadStorage(const fs::path &path, StorageKind kind, bool native_allowed, Storage &storage,
                 std::string &error)
{
    try
    {
        if (path.empty() && kind != StorageKind::BlankCard)
            throw std::runtime_error("Cannot resolve the save folder or card path.");
        Storage result;
        result.kind = kind;
        result.path = path;
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
                if (result.card.size() == 8388608 || result.card.size() == 8650752)
                {
                    if (!ReadMemoryCard(result.card, files, result.has_save, error))
                        return false;
                }
                else
                {
                    std::string directory;
                    if (!ReadSaveArchive(result.card, directory, files, error))
                        return false;
                    if (directory != FolderName)
                        throw std::runtime_error(std::string("This archive is not a ") + RegionName + " Fatal Frame save.");
                    result.has_save = true;
                }
                if (result.has_save && !InspectFiles(files, false, result.save, error))
                    return false;
            }
            else
            {
                if (!Inspect(path, native_allowed, result.save, error))
                    return false;
                result.has_save = true;
            }
        }
        else if (kind == StorageKind::Card)
            throw std::runtime_error("Choose an existing memory-card file.");
        storage = std::move(result);
        error.clear();
        return true;
    }
    catch (const std::exception &e)
    {
        error = e.what();
        return false;
    }
}
bool BuildBatch(const Folder &source, const Folder *destination,
                const std::vector<SlotMapping> &slots, bool export_ps2, Folder &result,
                std::string &error)
{
    try
    {
        if (slots.empty() || slots.size() > 3)
            throw std::runtime_error("Select between one and three saves.");
        Folder checked;
        if (!InspectFiles(source.files, export_ps2, checked, error))
            return false;
        if (destination && !InspectFiles(destination->files, !export_ps2, checked, error))
            return false;
        Folder next = destination ? *destination : Fresh(source, export_ps2);
        std::set<int> sources, targets;
        for (const auto &slot : slots)
        {
            if (slot.source < 0 || slot.source >= 3 || slot.destination < 0 ||
                slot.destination >= 3)
                throw std::runtime_error("Assign a destination slot to each selected save.");
            if (!sources.insert(slot.source).second || !targets.insert(slot.destination).second)
                throw std::runtime_error("Each source and destination slot can be used only once.");
            if (!source.slots[slot.source].occupied)
                throw std::runtime_error("Cannot transfer an empty slot.");
            auto payload = source.files.at(Name(slot.source));
            if (!(export_ps2 ? ToPs2(payload, payload, error) : ToNative(payload, payload, error)))
                return false;
            next.files[Name(slot.destination)] = std::move(payload);
        }
        return InspectFiles(next.files, !export_ps2, result, error);
    }
    catch (const std::exception &e)
    {
        error = e.what();
        return false;
    }
}
namespace
{
bool SameStorage(const Storage &expected, bool native, std::string &error)
{
    Storage current;
    return ReadStorage(expected.path, expected.kind, native, current, error) &&
           current.exists == expected.exists && current.has_save == expected.has_save &&
           current.card == expected.card && current.save.files == expected.save.files;
}
void CommitFolder(const Storage &expected, const Folder &folder, bool native, std::string &error)
{
    const auto dest = fs::absolute(expected.path).lexically_normal();
    if (fs::is_symlink(dest))
        throw std::runtime_error("Destination cannot be a symbolic link.");
    fs::create_directories(dest.parent_path());
    const auto stage = dest.parent_path() / (".mikupan-transfer-" + Unique());
    const auto rollback = dest.parent_path() / (".mikupan-rollback-" + Unique());
    bool moved = false, installed = false;
    try
    {
        if (!fs::create_directory(stage))
            throw std::runtime_error("Cannot create staging directory.");
        WriteFolder(stage, folder, native);
        Folder verified;
        if (!Inspect(stage, native, verified, error) || verified.files != folder.files)
            throw std::runtime_error("Staged save verification failed. " + error);
        if (!SameStorage(expected, native, error))
            throw std::runtime_error("Destination changed; refresh before transferring.");
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
                throw std::runtime_error(
                    "Could not roll back the transfer; previous folder remains at " +
                    PathUtf8(rollback));
        }
        if (moved)
        {
            fs::rename(rollback, dest, ec);
            if (ec)
                throw std::runtime_error("Could not restore the previous folder; it remains at " +
                                         PathUtf8(rollback));
        }
        fs::remove_all(stage, ec);
        throw;
    }
    std::error_code ec;
    if (moved)
        fs::remove_all(rollback, ec);
    if (ec)
        error = "Transfer complete; could not remove temporary folder " + PathUtf8(rollback);
}
void CommitCard(const fs::path &path, const Bytes &bytes)
{
    if (path.empty() || fs::exists(path) || fs::is_symlink(path))
        throw std::runtime_error(
            "Choose a new output filename; existing cards are never overwritten.");
    auto dest = fs::absolute(path).lexically_normal();
    fs::create_directories(dest.parent_path());
    const auto stage = dest.parent_path() / (".mikupan-card-" + Unique());
    bool published = false;
    try
    {
        Write(stage, bytes);
        // Copy the validated bytes into a new file; exclusive creation refuses overwrites.
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
            fs::remove(dest, ec);
        throw;
    }
}
} // namespace
bool TransferBatch(const Storage &source, const Storage &destination,
                   const std::vector<SlotMapping> &slots, bool export_ps2,
                   const fs::path &card_output, std::string &error, bool ecc, SaveArchiveFormat archive)
{
    try
    {
        if (source.kind == StorageKind::BlankCard || !source.has_save)
            throw std::runtime_error("Source contains no saves for this build's region.");
        if ((!export_ps2 && destination.kind != StorageKind::Folder) ||
            (export_ps2 && source.kind != StorageKind::Folder))
            throw std::runtime_error("Choose PC saves on the PC side of the transfer.");
        if (destination.kind == StorageKind::Folder && source.kind == StorageKind::Folder &&
            fs::weakly_canonical(source.path) == fs::weakly_canonical(destination.path))
            throw std::runtime_error("Source and destination must be different folders.");
        if (!SameStorage(source, export_ps2, error) ||
            !SameStorage(destination, !export_ps2, error))
            throw std::runtime_error(
                "Source or destination changed; refresh and review the slots again. " + error);
        Folder next;
        if (!BuildBatch(source.save, destination.has_save ? &destination.save : nullptr, slots,
                        export_ps2, next, error))
            return false;
        error.clear();
        if (destination.kind == StorageKind::Folder)
            CommitFolder(destination, next, !export_ps2, error);
        else
        {
            Bytes output;
            if (archive == SaveArchiveFormat::None)
            {
                if (!WriteMemoryCard(destination.card, next.files, output, error, ecc))
                    return false;
            }
            else if (!WriteSaveArchive(FolderName, next.files, archive, output, error))
                return false;
            if (!SameStorage(source, export_ps2, error) || !SameStorage(destination, false, error))
                throw std::runtime_error(
                    "Save files changed while preparing the card; refresh before exporting.");
            CommitCard(card_output, output);
        }
        return true;
    }
    catch (const std::exception &e)
    {
        error = e.what();
        return false;
    }
}
} // namespace MikuPanSave
