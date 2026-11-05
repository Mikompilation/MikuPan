#include "mikupan_disc_image.h"

#include "spdlog/spdlog.h"

#include <SDL3/SDL_error.h>
#include <SDL3/SDL_iostream.h>
#include <SDL3/SDL_stdinc.h>

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstring>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <vector>

namespace
{

constexpr uint64_t kSectorSize = 2048;
constexpr uint64_t kFirstVolumeDescriptorSector = 16;
constexpr uint8_t kPrimaryVolumeDescriptor = 1;
constexpr uint8_t kVolumeDescriptorTerminator = 255;
constexpr uint8_t kFileFlagDirectory = 0x02;

/// Directory record layout (ECMA-119 9.1). Numbers are stored both ways
/// round; these are the little-endian copies.
constexpr size_t kRecordExtent = 2;
constexpr size_t kRecordSize = 10;
constexpr size_t kRecordFlags = 25;
constexpr size_t kRecordNameLength = 32;
constexpr size_t kRecordName = 33;

/// Volume descriptor layout (ECMA-119 8.4).
constexpr size_t kVolumeBlockSize = 128;
constexpr size_t kVolumeRootRecord = 156;

/// The game's discs are shallow and small. These only stop a damaged image
/// from sending the walk into a loop or a huge allocation.
constexpr int kMaxDirectoryDepth = 8;
constexpr uint32_t kMaxDirectorySize = 16 * 1024 * 1024;

struct DiscFile
{
    uint64_t offset;
    uint64_t size;
};

struct DiscImage
{
    std::string path;
    std::unordered_map<std::string, DiscFile> files;
};

/// The IOP and bake threads look files up while the main thread renders, so
/// readers take their own reference to an index that never changes.
std::mutex g_mounted_mutex;
std::shared_ptr<const DiscImage> g_mounted;

std::shared_ptr<const DiscImage> GetMounted()
{
    std::lock_guard<std::mutex> lock(g_mounted_mutex);
    return g_mounted;
}

uint32_t ReadLe32(const uint8_t* data)
{
    return static_cast<uint32_t>(data[0])
        | static_cast<uint32_t>(data[1]) << 8
        | static_cast<uint32_t>(data[2]) << 16
        | static_cast<uint32_t>(data[3]) << 24;
}

uint16_t ReadLe16(const uint8_t* data)
{
    return static_cast<uint16_t>(data[0] | data[1] << 8);
}

bool ReadExact(SDL_IOStream* io, uint64_t offset, void* buffer, size_t size)
{
    if (SDL_SeekIO(io, static_cast<Sint64>(offset), SDL_IO_SEEK_SET) < 0)
    {
        return false;
    }

    auto* dst = static_cast<uint8_t*>(buffer);
    while (size > 0)
    {
        const size_t got = SDL_ReadIO(io, dst, size);
        if (got == 0)
        {
            return false;
        }
        dst += got;
        size -= got;
    }

    return true;
}

/// "movie/scn0010.pss;1" and "MOVIE/SCN0010.PSS" name the same file.
std::string NormalizeName(const std::string& name)
{
    std::string normalized = name.substr(0, name.find(';'));

    while (!normalized.empty() && normalized.back() == '.')
    {
        normalized.pop_back();
    }

    std::transform(normalized.begin(), normalized.end(), normalized.begin(),
                   [](unsigned char ch) {
                       return static_cast<char>(std::toupper(ch));
                   });
    return normalized;
}

bool IndexDirectory(SDL_IOStream* io, uint64_t image_size, uint32_t extent,
                    uint32_t size, const std::string& prefix, int depth,
                    DiscImage* image)
{
    const uint64_t offset = extent * kSectorSize;

    if (depth > kMaxDirectoryDepth || size == 0 || size > kMaxDirectorySize
        || offset + size > image_size)
    {
        return false;
    }

    std::vector<uint8_t> directory(size);
    if (!ReadExact(io, offset, directory.data(), directory.size()))
    {
        return false;
    }

    size_t position = 0;
    while (position < directory.size())
    {
        const size_t length = directory[position];

        /// Records never straddle a sector; the rest of this one is padding.
        if (length == 0)
        {
            position = (position / kSectorSize + 1) * kSectorSize;
            continue;
        }

        if (length <= kRecordName || position + length > directory.size())
        {
            return false;
        }

        const uint8_t* record = &directory[position];
        position += length;

        const size_t name_length = record[kRecordNameLength];
        if (kRecordName + name_length > length)
        {
            return false;
        }

        const uint32_t record_extent = ReadLe32(record + kRecordExtent);
        const uint32_t record_size = ReadLe32(record + kRecordSize);

        if ((record[kRecordFlags] & kFileFlagDirectory) != 0)
        {
            /// "." and ".." are stored as the single bytes 0 and 1.
            if (name_length == 1 && record[kRecordName] <= 1)
            {
                continue;
            }

            const std::string name(
                reinterpret_cast<const char*>(record + kRecordName),
                name_length);

            if (!IndexDirectory(io, image_size, record_extent, record_size,
                                prefix + NormalizeName(name) + "/", depth + 1,
                                image))
            {
                return false;
            }
            continue;
        }

        const uint64_t file_offset = record_extent * kSectorSize;

        /// A truncated download still has its directories, so catch it here
        /// rather than as a failed read halfway through the game.
        if (file_offset + record_size > image_size)
        {
            return false;
        }

        const std::string name(
            reinterpret_cast<const char*>(record + kRecordName), name_length);
        image->files[prefix + NormalizeName(name)] = {file_offset, record_size};
    }

    return true;
}

bool IndexImage(SDL_IOStream* io, DiscImage* image)
{
    const Sint64 image_size = SDL_GetIOSize(io);
    if (image_size <= 0)
    {
        return false;
    }

    uint8_t descriptor[kSectorSize];

    for (uint64_t sector = kFirstVolumeDescriptorSector;; sector++)
    {
        if (!ReadExact(io, sector * kSectorSize, descriptor, sizeof(descriptor))
            || std::memcmp(descriptor + 1, "CD001", 5) != 0
            || descriptor[0] == kVolumeDescriptorTerminator)
        {
            return false;
        }

        if (descriptor[0] == kPrimaryVolumeDescriptor)
        {
            break;
        }
    }

    if (ReadLe16(descriptor + kVolumeBlockSize) != kSectorSize)
    {
        return false;
    }

    const uint8_t* root = descriptor + kVolumeRootRecord;
    return IndexDirectory(io, static_cast<uint64_t>(image_size),
                          ReadLe32(root + kRecordExtent),
                          ReadLe32(root + kRecordSize), "", 0, image);
}

struct DiscFileStream
{
    SDL_IOStream* image;
    Sint64 offset;
    Sint64 size;
    Sint64 position;
};

Sint64 SDLCALL DiscFileStreamSize(void* userdata)
{
    return static_cast<DiscFileStream*>(userdata)->size;
}

Sint64 SDLCALL DiscFileStreamSeek(void* userdata, Sint64 offset,
                                  SDL_IOWhence whence)
{
    auto* stream = static_cast<DiscFileStream*>(userdata);
    Sint64 position = offset;

    if (whence == SDL_IO_SEEK_CUR)
    {
        position += stream->position;
    }
    else if (whence == SDL_IO_SEEK_END)
    {
        position += stream->size;
    }

    if (position < 0)
    {
        SDL_SetError("Seek before the start of a disc file");
        return -1;
    }

    stream->position = position;
    return position;
}

size_t SDLCALL DiscFileStreamRead(void* userdata, void* buffer, size_t size,
                                  SDL_IOStatus* status)
{
    auto* stream = static_cast<DiscFileStream*>(userdata);
    const Sint64 remaining = stream->size - stream->position;

    if (remaining <= 0)
    {
        *status = SDL_IO_STATUS_EOF;
        return 0;
    }

    if (SDL_SeekIO(stream->image, stream->offset + stream->position,
                   SDL_IO_SEEK_SET) < 0)
    {
        *status = SDL_IO_STATUS_ERROR;
        return 0;
    }

    const size_t wanted =
        static_cast<size_t>(std::min<uint64_t>(size, remaining));
    const size_t got = SDL_ReadIO(stream->image, buffer, wanted);

    *status = SDL_GetIOStatus(stream->image);
    stream->position += static_cast<Sint64>(got);
    return got;
}

bool SDLCALL DiscFileStreamClose(void* userdata)
{
    auto* stream = static_cast<DiscFileStream*>(userdata);
    const bool closed = SDL_CloseIO(stream->image);
    delete stream;
    return closed;
}

} // namespace

bool MikuPan_DiscImageMount(const char* path, std::string* error)
{
    SDL_IOStream* io = SDL_IOFromFile(path, "rb");
    if (io == nullptr)
    {
        spdlog::warn("Cannot open disc image {}: {}", path, SDL_GetError());
        *error = "That disc image could not be opened.";
        return false;
    }

    auto image = std::make_shared<DiscImage>();
    image->path = path;

    const bool indexed = IndexImage(io, image.get());
    SDL_CloseIO(io);

    if (!indexed)
    {
        spdlog::warn("{} is not a readable ISO 9660 image", path);
        *error = "That file is not a complete disc image.";
        return false;
    }

    spdlog::info("Mounted disc image {} ({} files)", path, image->files.size());

    std::lock_guard<std::mutex> lock(g_mounted_mutex);
    g_mounted = std::move(image);
    return true;
}

void MikuPan_DiscImageUnmount()
{
    std::lock_guard<std::mutex> lock(g_mounted_mutex);
    g_mounted.reset();
}

bool MikuPan_DiscImageHasFile(const std::string& name)
{
    const auto image = GetMounted();
    return image != nullptr
        && image->files.find(NormalizeName(name)) != image->files.end();
}

SDL_IOStream* MikuPan_DiscImageOpenFile(const std::string& name)
{
    const auto image = GetMounted();
    if (image == nullptr)
    {
        return nullptr;
    }

    const auto file = image->files.find(NormalizeName(name));
    if (file == image->files.end())
    {
        return nullptr;
    }

    SDL_IOStream* io = SDL_IOFromFile(image->path.c_str(), "rb");
    if (io == nullptr)
    {
        return nullptr;
    }

    auto* stream =
        new DiscFileStream{io, static_cast<Sint64>(file->second.offset),
                           static_cast<Sint64>(file->second.size), 0};

    SDL_IOStreamInterface iface;
    SDL_INIT_INTERFACE(&iface);
    iface.size = DiscFileStreamSize;
    iface.seek = DiscFileStreamSeek;
    iface.read = DiscFileStreamRead;
    iface.close = DiscFileStreamClose;

    SDL_IOStream* opened = SDL_OpenIO(&iface, stream);
    if (opened == nullptr)
    {
        SDL_CloseIO(io);
        delete stream;
    }

    return opened;
}
