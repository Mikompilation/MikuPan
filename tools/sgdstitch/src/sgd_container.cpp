#include "sgd_container.h"
#include "sgd_io.h"
#include "sgd_layout.h"

namespace sgd
{
namespace
{

constexpr int kMaxPakDepth = 3;
constexpr uint32_t kMaxPakEntries = 4096;

struct PakEntry
{
    size_t off;
    size_t size;
};

/// Same walk as GetFileInPak() / MpkMapUnit() / InitializeRoomUnder().
bool ReadPak(const uint8_t* data, size_t size, std::vector<PakEntry>* entries)
{
    if (size < 32)
    {
        return false;
    }

    const uint32_t count = Rd32(data);

    if (count == 0 || count > kMaxPakEntries || Rd32(data + 4) != 0
        || Rd32(data + 8) != 0 || Rd32(data + 12) != 0)
    {
        return false;
    }

    size_t p = 16;
    entries->clear();

    for (uint32_t i = 0; i < count; i++)
    {
        if (p + 16 > size)
        {
            return false;
        }

        const size_t entry = Rd32(data + p);

        if (entry == 0 || p + 16 + entry > size)
        {
            return false;
        }

        entries->push_back({p + 16, entry});
        p += 16 + entry;
    }

    return true;
}

}// namespace

bool LooksStitchable(const uint8_t* data, size_t size)
{
    std::vector<PakEntry> entries;
    return LooksLikeSgd(data, size) || ReadPak(data, size, &entries);
}

bool StitchBlob(uint8_t* data, size_t size, const StitchOptions& options,
                StitchStats* stats, std::vector<std::string>* log, int depth)
{
    if (LooksLikeSgd(data, size))
    {
        std::string message;
        const bool modified = StitchSgd(data, size, options, stats, &message);

        if (!message.empty() && log != nullptr)
        {
            log->push_back(message);
        }

        return modified;
    }

    std::vector<PakEntry> entries;

    if (depth >= kMaxPakDepth || !ReadPak(data, size, &entries))
    {
        return false;
    }

    bool modified = false;
    for (const PakEntry& e : entries)
    {
        modified |= StitchBlob(data + e.off, e.size, options, stats, log,
                               depth + 1);
    }

    return modified;
}

}// namespace sgd
