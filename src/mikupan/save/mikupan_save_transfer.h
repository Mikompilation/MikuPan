#ifndef MIKUPAN_SAVE_TRANSFER_H
#define MIKUPAN_SAVE_TRANSFER_H
#include "mikupan_save_codec.h"
#include "mikupan_save_archive.h"
#include <array>
#include <filesystem>
#include <map>
#include <vector>
namespace MikuPanSave
{
struct SlotInfo
{
    bool occupied = false;
    std::string description;
    std::string player;
    unsigned chapter = 0, hours = 0, minutes = 0, seconds = 0;
    int area = 0, place = 0;
    bool finder = false;
};
struct Folder
{
    std::map<std::string, Bytes> files;
    std::array<SlotInfo, 3> slots;
};
enum class StorageKind
{
    Folder,
    Card,
    BlankCard
};
struct Storage
{
    StorageKind kind = StorageKind::Folder;
    std::filesystem::path path;
    bool exists = false, has_save = false;
    Folder save;
    Bytes card;
};
struct SlotMapping
{
    int source, destination;
};
std::filesystem::path PcFolder();
bool InspectFiles(const std::map<std::string, Bytes> &files, bool native_allowed, Folder &folder,
                  std::string &error);
bool ReadStorage(const std::filesystem::path &path, StorageKind kind, bool native_allowed,
                 Storage &storage, std::string &error);
bool BuildBatch(const Folder &source, const Folder *destination,
                const std::vector<SlotMapping> &slots, bool export_ps2, Folder &result,
                std::string &error);
// Uses the captured snapshots; card output must be a new filename. No persistent backups.
bool TransferBatch(const Storage &source, const Storage &destination,
                   const std::vector<SlotMapping> &slots, bool export_ps2,
                   const std::filesystem::path &card_output, std::string &error, bool ecc = true,
                   SaveArchiveFormat archive = SaveArchiveFormat::None);
bool Inspect(const std::filesystem::path &path, bool native_allowed, Folder &folder,
             std::string &error);
} // namespace MikuPanSave
#endif
