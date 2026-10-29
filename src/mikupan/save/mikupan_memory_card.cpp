#include "mikupan_memory_card.h"
#include <algorithm>
#include <array>
#include <cstring>
#include <ctime>
#include <set>
#include <stdexcept>

namespace MikuPanSave
{
namespace
{
// Format reference: https://www.ps2savetools.com/ps2memcardformat.html
constexpr char Signature[] = "Sony PS2 Memory Card Format ";
constexpr uint32_t End = 0xffffffffu, Free = 0x7fffffffu;
constexpr size_t DataSize = 8 * 1024 * 1024, PageCount = DataSize / 512;
using Record = std::array<uint8_t, 512>;
void Require(bool ok, const char* message)
{
    if (!ok)
    {
        throw std::runtime_error(message);
    }
}
uint32_t Get(const uint8_t* p, unsigned bytes = 4)
{
    uint32_t value = 0;
    for (unsigned i = 0; i < bytes; ++i)
    {
        value |= uint32_t(p[i]) << (8 * i);
    }
    return value;
}
void Put(uint8_t* p, uint32_t value, unsigned bytes = 4)
{
    for (unsigned i = 0; i < bytes; ++i)
    {
        p[i] = uint8_t(value >> (8 * i));
    }
}
// Derive parity groups from bit addresses instead of using imported ECC tables.
constexpr std::array<uint8_t, 256> ColumnParity()
{
    std::array<uint8_t, 256> result {};
    for (unsigned value = 0; value < 256; ++value)
    {
        for (unsigned bit = 0; bit < 8; ++bit)
        {
            if (value & (1u << bit))
            {
                for (unsigned axis = 0; axis < 3; ++axis)
                {
                    result[value] ^=
                        uint8_t(1u << (axis + ((bit & (1u << axis)) ? 4 : 0)));
                }
            }
        }
    }
    return result;
}
std::array<uint8_t, 3> Ecc(const uint8_t* data)
{
    static constexpr auto columns = ColumnParity();
    std::array<uint8_t, 3> code {0x77, 0x7f, 0x7f};
    for (unsigned address = 0; address < 128; ++address)
    {
        unsigned value = data[address];
        code[0] ^= columns[value];
        value ^= value >> 4;
        value ^= value >> 2;
        value ^= value >> 1;
        if (value & 1)
        {
            code[1] ^= uint8_t((~address) & 0x7f);
            code[2] ^= uint8_t(address);
        }
    }
    return code;
}
void Timestamp(uint8_t* p)
{
    const std::time_t now = std::time(nullptr) + 9 * 3600;
    std::tm t {};
#ifdef _WIN32
    if (gmtime_s(&t, &now) != 0)
    {
        return;
    }
#else
    if (!gmtime_r(&now, &t))
    {
        return;
    }
#endif
    p[0] = 0;
    p[1] = uint8_t(t.tm_sec);
    p[2] = uint8_t(t.tm_min);
    p[3] = uint8_t(t.tm_hour);
    p[4] = uint8_t(t.tm_mday);
    p[5] = uint8_t(t.tm_mon + 1);
    Put(p + 6, unsigned(t.tm_year + 1900), 2);
}
Record MakeRecord(const std::string& name, bool directory, uint32_t cluster,
                  uint32_t length)
{
    Require(!name.empty() && name.size() < 32, "Invalid card filename.");
    Record r {};
    Put(r.data(), directory ? 0x8427 : 0x8497, 2);
    Put(r.data() + 4, length);
    Put(r.data() + 16, cluster);
    Timestamp(r.data() + 8);
    Timestamp(r.data() + 24);
    std::memcpy(r.data() + 64, name.data(), name.size());
    return r;
}
struct Node
{
    Record record {};
    size_t location = 0;
    std::vector<uint32_t> chain;
    std::vector<Node> children;
    std::string name;
};
class Card
{
   public:
    Bytes data;
    std::vector<std::array<uint8_t, 16>> spare =
        std::vector<std::array<uint8_t, 16>>(PageCount);
    std::array<bool, PageCount> checked {};
    std::array<size_t, 8192> fat {};
    std::set<uint32_t> owned;
    uint32_t offset = 0, limit = 0;
    bool with_ecc = false;
    Node root;
    explicit Card(const Bytes& image)
    {
        Require(image.size() == DataSize || image.size() == PageCount * 528,
                "Only standard 8 MB PS2 cards are supported (raw or ECC).");
        with_ecc = image.size() != DataSize;
        data.resize(DataSize);
        for (size_t page = 0; page < PageCount; ++page)
        {
            std::copy_n(image.data() + page * (with_ecc ? 528 : 512), 512,
                        data.data() + page * 512);
            spare[page].fill(0xff);
            if (with_ecc)
            {
                std::copy_n(image.data() + page * 528 + 512, 16,
                            spare[page].data());
            }
        }
        CheckPage(0);
        Require(std::memcmp(data.data(), Signature, sizeof(Signature) - 1) == 0,
                "Not a formatted PS2 memory card.");
        Require(std::memcmp(data.data() + 28, "1.", 2) == 0
                    && Get(data.data() + 40, 2) == 512
                    && Get(data.data() + 42, 2) == 2
                    && Get(data.data() + 44, 2) == 16
                    && Get(data.data() + 48) == 8192 && data[336] == 2
                    && Get(data.data() + 60) == 0,
                "Unsupported PS2 card geometry or version.");
        offset = Get(data.data() + 52);
        limit = Get(data.data() + 56);
        Require(offset >= 2 && offset < 8192 && limit > 0
                    && limit <= 8192 - offset,
                "Invalid card allocation range.");
        for (unsigned i = 0; i < 32; ++i)
        {
            Require(Get(data.data() + 208 + i * 4) == End,
                    "Cards with bad-block remapping are not supported.");
        }
        const uint32_t backup1 = Get(data.data() + 64),
                       backup2 = Get(data.data() + 68);
        Require(backup1 < 1024 && backup2 < 1024 && backup1 != backup2
                    && (offset + limit) <= std::min(backup1, backup2) * 8,
                "Invalid card backup-block range.");
        Require(std::all_of(data.begin() + backup2 * 8192,
                            data.begin() + (backup2 + 1) * 8192,
                            [](uint8_t b) {
                                return b == 0xff;
                            }),
                "Card contains an unfinished save; recover it in the emulator "
                "first.");
        std::set<uint32_t> system;
        for (uint32_t table = 0; table < 32; ++table)
        {
            const uint32_t indirect =
                Get(data.data() + 80);// 8 MB needs one indirect table.
            Require(indirect > 0 && indirect < offset,
                    "Invalid indirect FAT cluster.");
            CheckCluster(indirect);
            const uint32_t physical =
                Get(data.data() + indirect * 1024 + table * 4);
            Require(physical > 0 && physical < offset && physical != indirect
                        && system.insert(physical).second,
                    "Invalid or overlapping FAT clusters.");
            CheckCluster(physical);
            for (uint32_t i = 0; i < 256; ++i)
            {
                fat[table * 256 + i] = physical * 1024 + i * 4;
            }
        }
        const size_t root_location = offset * 1024;
        std::copy_n(data.data() + root_location, 512, root.record.data());
        root.location = root_location;
        root.name = ".";
        ReadNode(root, true, 0);
    }
    void CheckPage(size_t page)
    {
        Require(page < PageCount, "Card page outside image.");
        if (checked[page] || !with_ecc)
        {
            return;
        }
        const auto* p = data.data() + page * 512;
        const bool erased = std::all_of(p, p + 512,
                                        [](uint8_t b) {
                                            return b == 0xff;
                                        })
                            && std::all_of(spare[page].begin(),
                                           spare[page].end(), [](uint8_t b) {
                                               return b == 0xff;
                                           });
        if (!erased)
        {
            for (unsigned i = 0; i < 4; ++i)
            {
                const auto code = Ecc(p + i * 128);
                Require(std::equal(code.begin(), code.end(),
                                   spare[page].begin() + i * 3),
                        "Memory-card ECC mismatch; the card was not changed.");
            }
        }
        checked[page] = true;
    }
    void CheckCluster(uint32_t physical)
    {
        Require(physical < 8192, "Card cluster outside image.");
        CheckPage(physical * 2);
        CheckPage(physical * 2 + 1);
    }
    uint32_t Fat(uint32_t cluster) const
    {
        Require(cluster < limit, "Allocation chain outside card.");
        return Get(data.data() + fat[cluster]);
    }
    void SetFat(uint32_t cluster, uint32_t next)
    {
        Put(data.data() + fat[cluster], next);
    }
    std::vector<uint32_t> Chain(uint32_t start, size_t bytes)
    {
        const size_t count = (bytes + 1023) / 1024;
        Require(count <= limit, "File length exceeds card capacity.");
        if (!count)
        {
            Require(start == End, "Invalid empty card file.");
            return {};
        }
        std::vector<uint32_t> result;
        uint32_t current = start;
        for (size_t i = 0; i < count; ++i)
        {
            Require(current < limit && owned.insert(current).second,
                    "Cyclic or overlapping card allocation.");
            CheckCluster(offset + current);
            result.push_back(current);
            const uint32_t next = Fat(current);
            Require((next & 0x80000000u) != 0,
                    "File references a free card cluster.");
            if (i + 1 == count)
            {
                Require(next == End,
                        "File allocation does not match its length.");
            }
            else
            {
                Require(next != End, "Truncated card allocation chain.");
            }
            current = next & Free;
        }
        return result;
    }
    size_t At(const Node& node, size_t byte) const
    {
        Require(byte / 1024 < node.chain.size(),
                "Directory entry outside allocation.");
        return (offset + node.chain[byte / 1024]) * 1024 + byte % 1024;
    }
    void ReadNode(Node& node, bool directory, unsigned depth)
    {
        Require(depth < 32, "Memory-card directories are too deeply nested.");
        const uint32_t mode = Get(node.record.data(), 2),
                       length = Get(node.record.data() + 4);
        Require((mode & 0x8000) && (mode & 0x30) == (directory ? 0x20u : 0x10u),
                "Invalid memory-card entry type.");
        Require(!directory || (length >= 2 && length <= limit * 2),
                "Invalid memory-card directory length.");
        node.chain = Chain(Get(node.record.data() + 16),
                           directory ? size_t(length) * 512 : length);
        if (!directory)
        {
            return;
        }
        std::set<std::string> names;
        for (uint32_t i = 0; i < length; ++i)
        {
            const size_t at = At(node, size_t(i) * 512);
            const auto* r = data.data() + at;
            if (i < 2)
            {
                Require((Get(r, 2) & 0x8000) != 0,
                        "Missing card directory links.");
            }
            if (!(Get(r, 2) & 0x8000))
            {
                continue;
            }
            const auto* end = std::find(r + 64, r + 96, uint8_t(0));
            Require(end != r + 96, "Unterminated card filename.");
            const std::string name(reinterpret_cast<const char*>(r + 64),
                                   size_t(end - r - 64));
            if (i < 2)
            {
                Require(name == (i == 0 ? "." : ".."),
                        "Missing card directory links.");
                continue;
            }
            Require(!name.empty() && name != "." && name != ".."
                        && names.insert(name).second,
                    "Duplicate or invalid card filename.");
            for (unsigned char c : name)
            {
                Require(c >= 32 && c != '/' && c != '*' && c != '?',
                        "Invalid card filename.");
            }
            Node child;
            child.name = name;
            child.location = at;
            std::copy_n(r, 512, child.record.data());
            ReadNode(child, (Get(r, 2) & 0x20) != 0, depth + 1);
            node.children.push_back(std::move(child));
        }
    }
    Node* Find(Node& directory, const std::string& name)
    {
        for (auto& n : directory.children)
        {
            if (n.name == name)
            {
                return &n;
            }
        }
        return nullptr;
    }
    Bytes Contents(const Node& file) const
    {
        Bytes bytes(Get(file.record.data() + 4));
        for (size_t i = 0; i < bytes.size(); i += 1024)
        {
            std::copy_n(data.data() + (offset + file.chain[i / 1024]) * 1024,
                        std::min(size_t(1024), bytes.size() - i),
                        bytes.data() + i);
        }
        return bytes;
    }
    SaveFiles GameFiles(bool& found, const char* folder)
    {
        SaveFiles files;
        auto* dir = Find(root, folder);
        found = dir != nullptr;
        if (!dir)
        {
            return files;
        }
        Require((Get(dir->record.data(), 2) & 0x30) == 0x20,
                "Game save is not a directory.");
        for (const auto& file : dir->children)
        {
            Require((Get(file.record.data(), 2) & 0x30) == 0x10,
                    "Unexpected directory inside game save.");
            files.emplace(file.name, Contents(file));
        }
        return files;
    }
    uint32_t Allocate()
    {
        // PS2 reserves capacity above the first 8000 allocatable clusters.
        for (uint32_t c = 0; c < std::min(limit, 8000u); ++c)
        {
            if (!(Fat(c) & 0x80000000u))
            {
                SetFat(c, End);
                std::fill_n(data.data() + (offset + c) * 1024, 1024, 0);
                return c;
            }
        }
        throw std::runtime_error(
            "Not enough free space on the destination card.");
    }
    void StoreRecord(Node& node)
    {
        std::copy(node.record.begin(), node.record.end(),
                  data.begin() + node.location);
    }
    Node& Append(Node& parent, const std::string& name, bool directory)
    {
        uint32_t count = Get(parent.record.data() + 4), index = count;
        for (uint32_t i = 2; i < count; ++i)
        {
            if (!(Get(data.data() + At(parent, size_t(i) * 512), 2) & 0x8000))
            {
                index = i;
                break;
            }
        }
        if (index == count)
        {
            if (size_t(count) * 512 == parent.chain.size() * 1024)
            {
                const auto next = Allocate();
                SetFat(parent.chain.back(), 0x80000000u | next);
                parent.chain.push_back(next);
            }
            Put(parent.record.data() + 4, count + 1);
            StoreRecord(parent);
        }
        Node child;
        child.name = name;
        child.location = At(parent, size_t(index) * 512);
        child.record = MakeRecord(name, directory, End, 0);
        if (directory)
        {
            const uint32_t first = Allocate();
            child.chain.push_back(first);
            Put(child.record.data() + 16, first);
            Put(child.record.data() + 4, 2);
            auto dot = MakeRecord(".", true, parent.chain.front(), 0);
            Put(dot.data() + 20, index);
            auto up = MakeRecord("..", true, 0, 0);
            std::copy(dot.begin(), dot.end(),
                      data.begin() + (offset + first) * 1024);
            std::copy(up.begin(), up.end(),
                      data.begin() + (offset + first) * 1024 + 512);
        }
        StoreRecord(child);
        parent.children.push_back(std::move(child));
        return parent.children.back();
    }
    void ReplaceGame(const SaveFiles& files, const char* folder)
    {
        Node* directory = Find(root, folder);
        if (!directory)
        {
            directory = &Append(root, folder, true);
        }
        for (const auto& old : directory->children)
        {
            Require(files.count(old.name) != 0,
                    "Unexpected file in existing game save.");
        }
        for (const auto& [name, bytes] : files)
        {
            Require(bytes.size() <= DataSize,
                    "Save file exceeds memory-card capacity.");
            Node* file = Find(*directory, name);
            if (!file)
            {
                file = &Append(*directory, name, false);
            }
            else if (Contents(*file) == bytes)
            {
                continue;
            }
            for (uint32_t c : file->chain)
            {
                SetFat(c, Free);
            }
            file->chain.clear();
            for (size_t at = 0; at < bytes.size(); at += 1024)
            {
                const uint32_t c = Allocate();
                if (!file->chain.empty())
                {
                    SetFat(file->chain.back(), 0x80000000u | c);
                }
                file->chain.push_back(c);
                std::copy_n(bytes.data() + at,
                            std::min(size_t(1024), bytes.size() - at),
                            data.data() + (offset + c) * 1024);
            }
            Put(file->record.data() + 4, uint32_t(bytes.size()));
            Put(file->record.data() + 16,
                file->chain.empty() ? End : file->chain.front());
            Timestamp(file->record.data() + 24);
            StoreRecord(*file);
        }
    }
    Bytes Encode(bool ecc = true) const
    {
        if (!ecc)
        {
            return data;
        }
        Bytes image(PageCount * 528);
        for (size_t page = 0; page < PageCount; ++page)
        {
            auto* out = image.data() + page * 528;
            std::copy_n(data.data() + page * 512, 512, out);
            std::copy(spare[page].begin(), spare[page].end(), out + 512);
            const bool erased = std::all_of(out, out + 528, [](uint8_t b) {
                return b == 0xff;
            });
            if (!erased)
            {
                for (unsigned i = 0; i < 4; ++i)
                {
                    const auto code = Ecc(out + i * 128);
                    std::copy(code.begin(), code.end(), out + 512 + i * 3);
                }
            }
        }
        return image;
    }
};
Bytes Blank()
{
    Bytes data(DataSize, 0xff);
    std::fill_n(data.data(), 512, 0);
    std::memcpy(data.data(), Signature, sizeof(Signature) - 1);
    std::memcpy(data.data() + 28, "1.2.0.0", 7);
    Put(data.data() + 40, 512, 2);
    Put(data.data() + 42, 2, 2);
    Put(data.data() + 44, 16, 2);
    Put(data.data() + 46, 0xff00, 2);
    Put(data.data() + 48, 8192);
    Put(data.data() + 52, 41);
    Put(data.data() + 56, 8135);
    Put(data.data() + 60, 0);
    Put(data.data() + 64, 1023);
    Put(data.data() + 68, 1022);
    std::fill_n(data.data() + 80, 256, 0xff);
    Put(data.data() + 80, 8);
    data[336] = 2;
    data[337] = 0x2b;// ECC-capable, erase-to-ones card.
    for (uint32_t t = 0; t < 32; ++t)
    {
        Put(data.data() + 8 * 1024 + t * 4, 9 + t);
        for (uint32_t c = 0; c < 256; ++c)
        {
            Put(data.data() + (9 + t) * 1024 + c * 4,
                t * 256 + c < 8135 ? Free : End);
        }
    }
    Put(data.data() + 9 * 1024, End);
    std::fill_n(data.data() + 41 * 1024, 1024, 0);
    auto dot = MakeRecord(".", true, 0, 2), up = MakeRecord("..", true, 0, 0);
    Put(up.data(), 0xa426, 2);
    std::copy(dot.begin(), dot.end(), data.begin() + 41 * 1024);
    std::copy(up.begin(), up.end(), data.begin() + 41 * 1024 + 512);
    return data;
}
}// namespace
bool ReadMemoryCard(const Bytes& image, SaveFiles& files, bool& found,
                    std::string& error, const char* folder)
{
    try
    {
        Card card(image);
        auto result = card.GameFiles(found, folder);
        files = std::move(result);
        error.clear();
        return true;
    }
    catch (const std::exception& e)
    {
        error = e.what();
        return false;
    }
}
bool WriteMemoryCard(const Bytes& base, const SaveFiles& files, Bytes& output,
                     std::string& error, bool ecc, const char* folder)
{
    try
    {
        Card card(base.empty() ? Blank() : base);
        card.ReplaceGame(files, folder);
        auto result = card.Encode(ecc);
        Card checked(result);
        bool found = false;
        Require(checked.GameFiles(found, folder) == files && found,
                "Exported card verification failed.");
        output = std::move(result);
        error.clear();
        return true;
    }
    catch (const std::exception& e)
    {
        error = e.what();
        return false;
    }
}
}// namespace MikuPanSave
