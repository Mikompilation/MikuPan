#include "asset_check.h"

#include <vector>

using MikuPanHost::Engine;
using MikuPanHost::Region;

static constexpr uint64_t kSectorSize = 2048;
static constexpr size_t kIndexEntrySize = 8;

static uint32_t Read32(const unsigned char* p)
{
    return uint32_t(p[0]) | uint32_t(p[1]) << 8 | uint32_t(p[2]) << 16
        | uint32_t(p[3]) << 24;
}

static bool ReadAt(SDL_IOStream* io, uint64_t offset, void* buffer, size_t size)
{
    if (SDL_SeekIO(io, static_cast<Sint64>(offset), SDL_IO_SEEK_SET) < 0)
    {
        return false;
    }

    auto* dst = static_cast<unsigned char*>(buffer);
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

bool RegionOfIndexSize(int64_t size, Region* region)
{
    for (const Engine* engine :
         {&MikuPanHost::GetUsEngine(), &MikuPanHost::GetEuEngine()})
    {
        if (size == int64_t(engine->file_count()) * int64_t(kIndexEntrySize))
        {
            *region = engine->region;
            return true;
        }
    }

    return false;
}

GameDataCheck CheckGameData(SDL_IOStream* index, SDL_IOStream* archive,
                            Region* region)
{
    const Sint64 index_size = index != nullptr ? SDL_GetIOSize(index) : -1;
    const Sint64 archive_size = archive != nullptr ? SDL_GetIOSize(archive) : -1;

    if (index_size <= 0 || archive_size <= 0)
    {
        return GameDataCheck::Unreadable;
    }

    if (!RegionOfIndexSize(index_size, region))
    {
        return GameDataCheck::UnsupportedRelease;
    }

    std::vector<unsigned char> header(static_cast<size_t>(index_size));
    if (!ReadAt(index, 0, header.data(), header.size()))
    {
        return GameDataCheck::Unreadable;
    }

    for (size_t i = 0; i < header.size(); i += kIndexEntrySize)
    {
        const uint64_t start = uint64_t(Read32(&header[i])) * kSectorSize;
        const uint64_t size = Read32(&header[i + 4]);

        if (start > uint64_t(archive_size)
            || size > uint64_t(archive_size) - start)
        {
            return GameDataCheck::Mismatched;
        }
    }

    /// An archive of the other release may still be large enough for this
    /// index, so look at an entry that can be recognized: the message bank
    /// starts with a table of offsets into itself.
    const Engine& engine = *region == Region::EU ? MikuPanHost::GetEuEngine()
                                                 : MikuPanHost::GetUsEngine();
    const unsigned char* entry =
        &header[size_t(engine.message_file) * kIndexEntrySize];
    const uint64_t start = uint64_t(Read32(entry)) * kSectorSize;
    const uint32_t size = Read32(entry + 4);
    unsigned char first[4];

    if (size < sizeof(first))
    {
        return GameDataCheck::Mismatched;
    }

    if (!ReadAt(archive, start, first, sizeof(first)))
    {
        return GameDataCheck::Unreadable;
    }

    /// Categories may share text or be stored out of order.
    const uint32_t first_offset = Read32(first);
    if (first_offset < 200 || first_offset > 512 || first_offset % 4 != 0
        || first_offset > size)
    {
        return GameDataCheck::Mismatched;
    }

    std::vector<unsigned char> offsets(first_offset);
    if (!ReadAt(archive, start, offsets.data(), offsets.size()))
    {
        return GameDataCheck::Unreadable;
    }

    for (size_t i = 0; i < offsets.size(); i += 4)
    {
        const uint32_t offset = Read32(&offsets[i]);
        if (offset < first_offset || offset > size)
        {
            return GameDataCheck::Mismatched;
        }
    }

    return GameDataCheck::Playable;
}
