#include "asset_check.h"
#include <cstdint>
#include <fstream>
#include <vector>

static uint32_t Read32(const unsigned char* p)
{
    return uint32_t(p[0]) | uint32_t(p[1]) << 8 | uint32_t(p[2]) << 16 | uint32_t(p[3]) << 24;
}

bool InspectAssets(const std::filesystem::path& root, Region& region, std::string& error)
{
    error.clear();
    auto fail = [&](const char* message) { error = message; return false; };
    if (root.string().size() >= 512)
        return fail("The data-folder path is too long for this engine.");
    std::error_code ec;
    const auto header_size = std::filesystem::file_size(root / "IMG_HD.BIN", ec);
    if (ec) return fail("Cannot read IMG_HD.BIN in the selected folder.");
    const auto archive_size = std::filesystem::file_size(root / "IMG_BD.BIN", ec);
    if (ec) return fail("Cannot read IMG_BD.BIN in the selected folder.");
    if (header_size == uint64_t(MikuPanHost::GetUsEngine().file_count()) * 8)
        region = Region::US;
    else if (header_size == uint64_t(MikuPanHost::GetEuEngine().file_count()) * 8)
        region = Region::EU;
    else
        return fail("The archive index does not match a supported US or EU layout.");

    std::vector<unsigned char> header(static_cast<size_t>(header_size));
    std::ifstream index(root / "IMG_HD.BIN", std::ios::binary);
    if (!index.read(reinterpret_cast<char*>(header.data()), header.size()))
        return fail("Cannot read the complete archive index.");
    for (size_t i = 0; i < header.size(); i += 8)
    {
        const uint64_t start = uint64_t(Read32(header.data() + i)) * 2048;
        const uint64_t size = Read32(header.data() + i + 4);
        if (start > archive_size || size > archive_size - start)
            return fail("The archives are incomplete or do not belong together: an entry is outside IMG_BD.BIN.");
    }

    const int message_id = region == Region::EU ? MikuPanHost::GetEuEngine().message_file : MikuPanHost::GetUsEngine().message_file;
    const auto* entry = header.data() + message_id * 8;
    const uint64_t start = uint64_t(Read32(entry)) * 2048;
    const uint32_t size = Read32(entry + 4);
    std::ifstream archive(root / "IMG_BD.BIN", std::ios::binary);
    archive.seekg(start);
    unsigned char first[4];
    if (size < 4 || !archive.read(reinterpret_cast<char*>(first), 4))
        return fail("Cannot read the regional message-bank header.");
    const uint32_t first_offset = Read32(first);
    // Categories may share text or be stored out of order.
    if (first_offset < 200 || first_offset > 512 || first_offset % 4 || first_offset > size)
        return fail("The message bank does not match the selected archive index. Check that both BIN files come from the same game.");
    std::vector<unsigned char> offsets(first_offset);
    archive.seekg(start);
    if (!archive.read(reinterpret_cast<char*>(offsets.data()), offsets.size()))
        return fail("Cannot read the message-bank index.");
    for (size_t i = 0; i < offsets.size(); i += 4)
    {
        const uint32_t offset = Read32(offsets.data() + i);
        if (offset < first_offset || offset > size)
            return fail("The message-bank index is invalid or belongs to another archive.");
    }
    return true;
}
