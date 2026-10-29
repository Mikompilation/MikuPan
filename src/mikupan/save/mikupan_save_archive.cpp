#include "mikupan_save_archive.h"
#include <algorithm>
#include <array>
#include <cstring>
#include <ctime>
#include <stdexcept>
#include <zlib.h>

namespace MikuPanSave
{
namespace
{
constexpr size_t Limit = 8 * 1024 * 1024;
void Need(bool ok, const char *error)
{
    if (!ok)
        throw std::runtime_error(error);
}
uint32_t Get(const Bytes &b, size_t p)
{
    Need(p <= b.size() && b.size() - p >= 4, "Truncated save archive.");
    return uint32_t(b[p]) | uint32_t(b[p + 1]) << 8 | uint32_t(b[p + 2]) << 16 |
           uint32_t(b[p + 3]) << 24;
}
void Put(Bytes &b, size_t p, uint32_t v)
{
    for (unsigned i = 0; i < 4; i++)
        b[p + i] = uint8_t(v >> (8 * i));
}
std::string Name(const Bytes &b, size_t p)
{
    Need(p <= b.size() && b.size() - p >= 32, "Truncated archive filename.");
    size_t n = 0;
    while (n < 32 && b[p + n])
        ++n;
    Need(n > 0 && n < 32, "Invalid archive filename.");
    std::string name(reinterpret_cast<const char *>(b.data() + p), n);
    Need(name != "." && name != ".." && name.find_first_of("/\\:") == std::string::npos,
         "Unsafe archive filename.");
    for (unsigned char c : name)
        Need(c >= 32 && c != 127, "Invalid archive filename.");
    return name;
}
void SetName(Bytes &b, size_t p, const std::string &name)
{
    Need(!name.empty() && name.size() < 32, "Archive filename is too long.");
    std::copy(name.begin(), name.end(), b.begin() + p);
    Need(Name(b, p) == name, "Invalid archive filename.");
}
uint32_t Crc(const Bytes &b)
{
    uint32_t crc = ~0u;
    for (size_t i = 0; i < b.size(); i++)
    {
        crc ^= i >= 12 && i < 16 ? 0 : b[i];
        for (int bit = 0; bit < 8; bit++)
            crc = (crc >> 1) ^ (0xedb88320u & (0u - (crc & 1)));
    }
    return ~crc;
}
// MAX's LZARI uses a 4096-byte dictionary and a 17-bit arithmetic interval.
class Lzari
{
    static constexpr unsigned Symbols = 314, Quarter = 32768;
    std::array<unsigned, Symbols + 1> freq{}, cumulative{}, symbol{}, character{};
    std::array<unsigned, 4097> positions{};
    uint32_t low = 0, high = 4 * Quarter, value = 0;
    Bytes bits;
    size_t cursor = 0;
    unsigned held = 0;
    void WriteBit(unsigned bit)
    {
        if (cursor % 8 == 0)
            bits.push_back(0);
        bits.back() |= uint8_t(bit << (7 - cursor % 8));
        ++cursor;
    }
    unsigned ReadBit()
    {
        Need(cursor < bits.size() * 8 + 32, "Truncated MAX compressed stream.");
        const unsigned bit =
            cursor < bits.size() * 8 ? (bits[cursor / 8] >> (7 - cursor % 8)) & 1 : 0;
        ++cursor;
        return bit;
    }
    void Emit(unsigned bit)
    {
        WriteBit(bit);
        while (held)
        {
            WriteBit(bit ^ 1);
            --held;
        }
    }
    void Update(unsigned s)
    {
        if (cumulative[0] >= Quarter - 1)
        {
            unsigned total = 0;
            for (unsigned i = Symbols; i > 0; --i)
            {
                cumulative[i] = total;
                freq[i] = (freq[i] + 1) / 2;
                total += freq[i];
            }
            cumulative[0] = total;
        }
        unsigned first = s;
        while (first > 1 && freq[first - 1] == freq[s])
            --first;
        const unsigned a = character[first], b = character[s];
        std::swap(character[first], character[s]);
        symbol[a] = s;
        symbol[b] = first;
        ++freq[first];
        for (unsigned i = 0; i < first; i++)
            ++cumulative[i];
    }
    void EncodeRange(unsigned upper, unsigned lower, unsigned total)
    {
        const uint32_t width = high - low;
        high = low + uint32_t(uint64_t(width) * upper / total);
        low += uint32_t(uint64_t(width) * lower / total);
        for (;;)
        {
            if (high <= 2 * Quarter)
                Emit(0);
            else if (low >= 2 * Quarter)
            {
                Emit(1);
                low -= 2 * Quarter;
                high -= 2 * Quarter;
            }
            else if (low >= Quarter && high <= 3 * Quarter)
            {
                ++held;
                low -= Quarter;
                high -= Quarter;
            }
            else
                break;
            low *= 2;
            high *= 2;
        }
    }
    unsigned DecodeRange(const unsigned *table, unsigned count)
    {
        Need(high > low && value >= low && value < high, "Invalid MAX arithmetic stream.");
        const unsigned scaled = unsigned((uint64_t(value - low + 1) * table[0] - 1) / (high - low));
        unsigned a = 1, b = count;
        while (a < b)
        {
            unsigned mid = (a + b) / 2;
            if (table[mid] > scaled)
                a = mid + 1;
            else
                b = mid;
        }
        const unsigned index = a;
        const uint32_t width = high - low;
        high = low + uint32_t(uint64_t(width) * table[index - 1] / table[0]);
        low += uint32_t(uint64_t(width) * table[index] / table[0]);
        for (;;)
        {
            if (high <= 2 * Quarter)
            {
            }
            else if (low >= 2 * Quarter)
            {
                low -= 2 * Quarter;
                high -= 2 * Quarter;
                value -= 2 * Quarter;
            }
            else if (low >= Quarter && high <= 3 * Quarter)
            {
                low -= Quarter;
                high -= Quarter;
                value -= Quarter;
            }
            else if (high > 2 * Quarter)
                break;
            low *= 2;
            high *= 2;
            value = value * 2 + ReadBit();
        }
        return index;
    }

  public:
    Lzari()
    {
        for (unsigned i = 1; i <= Symbols; i++)
        {
            freq[i] = 1;
            symbol[i - 1] = i;
            character[i] = i - 1;
            cumulative[i - 1] = Symbols - i + 1;
        }
        for (unsigned i = 4096; i > 0; --i)
            positions[i - 1] = positions[i] + 10000 / (i + 200);
    }
    Bytes Decode(const Bytes &input, size_t size)
    {
        Need(size > 0 && size <= Limit && !input.empty(), "Invalid MAX expanded size.");
        bits = input;
        for (int i = 0; i < 17; i++)
            value = value * 2 + ReadBit();
        std::array<uint8_t, 4096> history{};
        std::fill_n(history.begin(), 4036, 0x20);
        unsigned write = 4036;
        Bytes out;
        out.reserve(size);
        while (out.size() < size)
        {
            const unsigned s = DecodeRange(cumulative.data(), Symbols);
            const unsigned token = character[s];
            Update(s);
            if (token < 256)
            {
                out.push_back(uint8_t(token));
                history[write] = uint8_t(token);
                write = (write + 1) & 4095;
            }
            else
            {
                const unsigned length = token - 256 + 3;
                unsigned read = (write - DecodeRange(positions.data(), 4096)) & 4095;
                Need(length <= size - out.size(), "MAX match exceeds expanded size.");
                for (unsigned i = 0; i < length; i++)
                {
                    uint8_t byte = history[read];
                    read = (read + 1) & 4095;
                    out.push_back(byte);
                    history[write] = byte;
                    write = (write + 1) & 4095;
                }
            }
        }
        return out;
    }
    Bytes Encode(const Bytes &input)
    {
        std::array<int, 65536> last;
        last.fill(-1);
        auto hash = [&](size_t p)
        {
            return unsigned((unsigned(input[p]) * 251 + input[p + 1]) * 251 + input[p + 2]) & 65535;
        };
        size_t p = 0;
        while (p < input.size())
        {
            unsigned length = 1, distance = 0;
            if (p + 2 < input.size())
            {
                const int previous = last[hash(p)];
                if (previous >= 0 && p - size_t(previous) <= 4096)
                {
                    unsigned n = 0;
                    while (n < 60 && p + n < input.size() &&
                           input[size_t(previous) + n] == input[p + n])
                        ++n;
                    if (n >= 3)
                    {
                        length = n;
                        distance = unsigned(p - size_t(previous));
                    }
                }
            }
            const unsigned token = length == 1 ? input[p] : 256 + length - 3;
            const unsigned s = symbol[token];
            EncodeRange(cumulative[s - 1], cumulative[s], cumulative[0]);
            Update(s);
            if (length > 1)
                EncodeRange(positions[distance - 1], positions[distance], positions[0]);
            for (unsigned i = 0; i < length; i++)
                if (p + i + 2 < input.size())
                    last[hash(p + i)] = int(p + i);
            p += length;
        }
        ++held;
        Emit(low < Quarter ? 0 : 1);
        // Arithmetic decoders need lookahead beyond the final coded symbol.
        for (int i = 0; i < 7; i++)
            WriteBit(0);
        return bits;
    }
};
void AddFile(SaveFiles &files, std::string name, const Bytes &body, size_t at, size_t length)
{
    Need(at <= body.size() && length <= body.size() - at, "Truncated archive file.");
    Need(
        files.emplace(std::move(name), Bytes(body.begin() + at, body.begin() + at + length)).second,
        "Duplicate archive filename.");
}
Bytes Entry(const std::string &name, uint32_t length, bool directory)
{
    Bytes entry(512, 0);
    Put(entry, 0, directory ? 0x8427 : 0x8497);
    Put(entry, 4, length);
    const auto now = std::time(nullptr);
    std::tm date{};
#ifdef _WIN32
    Need(localtime_s(&date, &now) == 0, "Cannot timestamp save archive.");
#else
    Need(localtime_r(&now, &date) != nullptr, "Cannot timestamp save archive.");
#endif
    for (size_t at : {size_t(8), size_t(24)})
    {
        entry[at + 1] = uint8_t(date.tm_sec);
        entry[at + 2] = uint8_t(date.tm_min);
        entry[at + 3] = uint8_t(date.tm_hour);
        entry[at + 4] = uint8_t(date.tm_mday);
        entry[at + 5] = uint8_t(date.tm_mon + 1);
        entry[at + 6] = uint8_t((date.tm_year + 1900) & 255);
        entry[at + 7] = uint8_t((date.tm_year + 1900) >> 8);
    }
    if (name == "." || name == "..")
        std::copy(name.begin(), name.end(), entry.begin() + 64);
    else
        SetName(entry, 64, name);
    return entry;
}
uint32_t XpsChecksum(const Bytes &bytes, size_t first, size_t last)
{
    uint32_t sum = 0;
    for (size_t i = first; i < last; ++i)
        sum += uint32_t(bytes[i]) << (sum % 24);
    return sum;
}
void Append32(Bytes &bytes, uint32_t value)
{
    const size_t at = bytes.size();
    bytes.resize(at + 4);
    Put(bytes, at, value);
}
Bytes XpsDescriptor(const std::string &name, uint32_t size, bool directory)
{
    const auto entry = Entry(name, size, directory);
    Bytes descriptor(250, 0);
    descriptor[0] = 250;
    SetName(descriptor, 2, name);
    Put(descriptor, 66, size);
    // X-Port stores the PS2 mode bytes in the opposite order.
    descriptor[78] = entry[1];
    descriptor[79] = entry[0];
    std::copy_n(entry.begin() + 8, 8, descriptor.begin() + 82);
    std::copy_n(entry.begin() + 24, 8, descriptor.begin() + 90);
    SetName(descriptor, 114, name);
    SetName(descriptor, 178, name);
    return descriptor;
}
SaveFiles ReadXps(const Bytes &bytes, std::string &directory)
{
    Need(Get(bytes, 17) == 0, "Unsupported X-Port save type.");
    size_t at = 21;
    for (int field = 0; field < 3; ++field)
    {
        const unsigned size = Get(bytes, at);
        at += 4;
        Need(size <= bytes.size() - at, "Truncated X-Port description.");
        at += size;
    }
    const unsigned length = Get(bytes, at);
    at += 4;
    Need(length <= bytes.size() - at, "Truncated X-Port body.");
    const size_t body = at, end = at + length;
    Need(end == bytes.size() || bytes.size() - end == 4, "Unexpected X-Port trailing data.");
    if (end != bytes.size())
    {
        const uint32_t stored = Get(bytes, end);
        Need(stored == 0 || stored == XpsChecksum(bytes, body, end), "X-Port checksum mismatch.");
    }
    SaveFiles files;
    unsigned count = 0;
    for (unsigned index = 0; index <= count; ++index)
    {
        Need(at <= end && end - at >= 98, "Truncated X-Port descriptor.");
        const unsigned header = Get(bytes, at) & 0xffff;
        Need(header >= 98 && header <= end - at, "Invalid X-Port descriptor size.");
        const auto name = Name(bytes, at + 2);
        const unsigned size = Get(bytes, at + 66);
        const unsigned mode = unsigned(bytes[at + 78]) << 8 | bytes[at + 79];
        at += header;
        if (index == 0)
        {
            Need((mode & 0x30) == 0x20 && size >= 3 && size <= 66,
                 "Invalid X-Port save directory.");
            directory = name;
            count = size - 2;
        }
        else
        {
            Need((mode & 0x30) == 0x10, "Unsupported X-Port subdirectory.");
            Need(size <= end - at, "Truncated X-Port file.");
            AddFile(files, name, bytes, at, size);
            at += size;
        }
    }
    Need(at == end, "X-Port body size does not match its files.");
    return files;
}
// CBS uses this fixed permutation as format data instead of an RC4 key schedule.
void CbsCrypt(Bytes &bytes)
{
    constexpr char permutation[] =
        "5f1f856f31aa3b1821b9ce1c074c9cb481b8ef9859aef926e380a3292d735162"
        "7c6446f4341af6e1ba3a0d82790a5c1671498eac8c9f351945943f560c91000b"
        "d7b0dd3966a176521357f3bb4ee5dcf06584b2d6df153c631d8914bdd236feb1"
        "ca8ba4c69e674737426d6a039270057d962f4090c4f13e3d01f7681ec3fc72b5"
        "54cfe741e44d835512220978fadea7060823bf0fccc19761c54ae6a011c2ea74"
        "0287d5d19db77e386053958d2577105e9b7fd86edaa22e204fcd8fcbbe5ae0ed"
        "2c9ad4e2afd0a9e8ad7abca8f2eeebf5a69928246c2b755df8d38617fbc07bb3"
        "58dbc74bff0450e98869c92aabfd5b1b8ad9ec27440e33c86b933248b63043a5";
    std::array<unsigned, 256> state{};
    auto nibble = [](char c) { return unsigned(c <= '9' ? c - '0' : c - 'a' + 10); };
    for (unsigned n = 0; n < state.size(); ++n)
        state[n] = nibble(permutation[n * 2]) * 16 + nibble(permutation[n * 2 + 1]);
    unsigned i = 0, j = 0;
    for (auto &byte : bytes)
    {
        i = (i + 1) & 255;
        j = (j + state[i]) & 255;
        std::swap(state[i], state[j]);
        byte ^= uint8_t(state[(state[i] + state[j]) & 255]);
    }
}
SaveFiles ReadCbs(const Bytes &data, std::string &directory)
{
    const unsigned header = Get(data, 8), expanded = Get(data, 12), packed = Get(data, 16);
    Need(header >= 124 && header <= data.size(), "Invalid CBS header size.");
    Need(expanded >= 64 && expanded <= Limit, "Invalid CBS expanded size.");
    Need(packed == data.size() || packed == data.size() - header, "Invalid CBS compressed size.");
    directory = Name(data, 20);
    Bytes compressed(data.begin() + header, data.end());
    CbsCrypt(compressed);
    Bytes body(expanded);
    uLongf produced = body.size();
    uLong consumed = compressed.size();
    const int status = uncompress2(body.data(), &produced, compressed.data(), &consumed);
    Need(status == Z_OK && produced == body.size() && consumed == compressed.size(),
         "Invalid or corrupt CBS compressed data.");
    SaveFiles files;
    size_t at = 0;
    while (at < body.size())
    {
        Need(body.size() - at >= 64 && files.size() < 64, "Invalid CBS file table.");
        const unsigned size = Get(body, at + 16);
        Need((Get(body, at + 20) & 0x30) == 0x10, "Unsupported CBS subdirectory.");
        const auto name = Name(body, at + 32);
        at += 64;
        AddFile(files, name, body, at, size);
        at += size;
    }
    return files;
}
Bytes WriteCbs(const std::string &directory, const SaveFiles &files)
{
    Bytes body;
    for (const auto &file : files)
    {
        const auto entry = Entry(file.first, uint32_t(file.second.size()), false);
        const size_t at = body.size();
        body.resize(at + 64, 0);
        std::copy_n(entry.begin() + 8, 8, body.begin() + at);
        std::copy_n(entry.begin() + 24, 8, body.begin() + at + 8);
        Put(body, at + 16, uint32_t(file.second.size()));
        Put(body, at + 20, 0x8497);
        SetName(body, at + 32, file.first);
        body.insert(body.end(), file.second.begin(), file.second.end());
    }
    Need(body.size() <= Limit, "Save archive is too large.");
    uLongf size = compressBound(body.size());
    Bytes compressed(size);
    Need(compress2(compressed.data(), &size, body.data(), body.size(), Z_BEST_COMPRESSION) == Z_OK,
         "CBS compression failed.");
    compressed.resize(size);
    CbsCrypt(compressed);
    Bytes out(296, 0);
    std::memcpy(out.data(), "CFU", 3);
    Put(out, 4, 8000);
    Put(out, 8, 296);
    Put(out, 12, uint32_t(body.size()));
    Put(out, 16, uint32_t(out.size() + compressed.size()));
    SetName(out, 20, directory);
    const auto entry = Entry(directory, uint32_t(files.size() + 2), true);
    std::copy_n(entry.begin() + 8, 8, out.begin() + 52);
    std::copy_n(entry.begin() + 24, 8, out.begin() + 60);
    Put(out, 76, 0x8427);
    SetName(out, 92, directory);
    out.insert(out.end(), compressed.begin(), compressed.end());
    return out;
}
} // namespace
bool ReadSaveArchive(const Bytes &data, std::string &directory, SaveFiles &files,
                     std::string &error)
{
    try
    {
        Need(data.size() <= Limit, "Save archive is too large.");
        SaveFiles result;
        std::string dir;
        if (data.size() >= 12 && std::memcmp(data.data(), "Ps2PowerSave\0", 12) == 0)
        {
            Need(data.size() >= 92, "Truncated MAX header.");
            const uint32_t crc = Get(data, 12);
            Need(crc == 0 || crc == Crc(data), "MAX checksum mismatch.");
            dir = Name(data, 16);
            const unsigned count = Get(data, 84), length = Get(data, 88),
                           compressed = Get(data, 80);
            Need(count > 0 && count <= 64, "Invalid MAX file count.");
            Need(compressed == length || (compressed >= 4 && compressed - 4 == data.size() - 92),
                 "Invalid MAX compressed size.");
            Bytes body = Lzari().Decode(Bytes(data.begin() + 92, data.end()), length);
            size_t at = 0;
            for (unsigned i = 0; i < count; i++)
            {
                const unsigned size = Get(body, at);
                const auto name = Name(body, at + 4);
                at += 36;
                AddFile(result, name, body, at, size);
                at += size;
                at = ((at + 8 + 15) & ~size_t(15)) - 8;
                Need(at <= body.size(), "Truncated MAX alignment.");
            }
            Need(at == body.size(), "Unexpected MAX trailing data.");
        }
        else if (data.size() >= 4 && std::memcmp(data.data(), "CFU\0", 4) == 0)
            result = ReadCbs(data, dir);
        else if (data.size() >= 17 && std::memcmp(data.data(), "\x0d\0\0\0SharkPortSave", 17) == 0)
            result = ReadXps(data, dir);
        else
        {
            Need(data.size() >= 1536 && (Get(data, 0) & 0x8020) == 0x8020,
                 "Not a supported PS2 save archive.");
            dir = Name(data, 64);
            const unsigned count = Get(data, 4);
            Need(count >= 3 && count <= 66, "Invalid PSU file count.");
            Need((Get(data, 512) & 0x8030) == 0x8020 && (Get(data, 1024) & 0x8030) == 0x8020 &&
                     std::memcmp(data.data() + 512 + 64, ".\0", 2) == 0 &&
                     std::memcmp(data.data() + 1024 + 64, "..\0", 3) == 0,
                 "Invalid PSU directory entries.");
            size_t at = 1536;
            for (unsigned i = 2; i < count; i++)
            {
                Need((Get(data, at) & 0x8030) == 0x8010, "Unsupported PSU directory or file mode.");
                const unsigned length = Get(data, at + 4);
                const auto name = Name(data, at + 64);
                at += 512;
                AddFile(result, name, data, at, length);
                at += length;
                const size_t padding = (1024 - length % 1024) % 1024;
                Need(padding <= data.size() - at, "Truncated PSU padding.");
                at += padding;
            }
            Need(at == data.size(), "Unexpected PSU trailing data.");
        }
        directory = std::move(dir);
        files = std::move(result);
        error.clear();
        return true;
    }
    catch (const std::exception &e)
    {
        error = e.what();
        return false;
    }
}
bool WriteSaveArchive(const std::string &directory, const SaveFiles &files,
                      SaveArchiveFormat format, Bytes &data, std::string &error)
{
    try
    {
        Need(format == SaveArchiveFormat::Psu || format == SaveArchiveFormat::Max ||
                 format == SaveArchiveFormat::Xps || format == SaveArchiveFormat::Cbs,
             "Choose a supported save archive format.");
        Need(!files.empty() && files.size() <= 64, "Invalid archive file count.");
        size_t total = 0;
        for (const auto &file : files)
        {
            Need(file.second.size() <= Limit - total, "Save archive is too large.");
            total += file.second.size();
        }
        Bytes out;
        if (format == SaveArchiveFormat::Cbs)
            out = WriteCbs(directory, files);
        else if (format == SaveArchiveFormat::Psu)
        {
            for (const auto &name : {directory, std::string("."), std::string("..")})
            {
                auto e = Entry(name, uint32_t(files.size() + 2), true);
                out.insert(out.end(), e.begin(), e.end());
            }
            for (const auto &file : files)
            {
                auto e = Entry(file.first, uint32_t(file.second.size()), false);
                out.insert(out.end(), e.begin(), e.end());
                out.insert(out.end(), file.second.begin(), file.second.end());
                out.resize(out.size() + (1024 - file.second.size() % 1024) % 1024, 0);
            }
        }
        else if (format == SaveArchiveFormat::Xps)
        {
            const char magic[] = "\x0d\0\0\0SharkPortSave";
            out.assign(magic, magic + 17);
            Append32(out, 0);
            Append32(out, uint32_t(directory.size()));
            out.insert(out.end(), directory.begin(), directory.end());
            Append32(out, 0); // Optional date and comment strings.
            Append32(out, 0);
            const size_t length_at = out.size();
            Append32(out, 0);
            const size_t body = out.size();
            auto descriptor = XpsDescriptor(directory, uint32_t(files.size() + 2), true);
            out.insert(out.end(), descriptor.begin(), descriptor.end());
            for (const auto &file : files)
            {
                descriptor = XpsDescriptor(file.first, uint32_t(file.second.size()), false);
                out.insert(out.end(), descriptor.begin(), descriptor.end());
                out.insert(out.end(), file.second.begin(), file.second.end());
            }
            Put(out, length_at, uint32_t(out.size() - body));
            Append32(out, XpsChecksum(out, body, out.size()));
        }
        else
        {
            Bytes body;
            for (const auto &file : files)
            {
                const size_t at = body.size();
                body.resize(at + 36, 0);
                Put(body, at, uint32_t(file.second.size()));
                SetName(body, at + 4, file.first);
                body.insert(body.end(), file.second.begin(), file.second.end());
                body.resize(((body.size() + 8 + 15) & ~size_t(15)) - 8, 0);
            }
            Need(body.size() <= Limit, "Save archive is too large.");
            const Bytes compressed = Lzari().Encode(body);
            out.resize(92, 0);
            std::memcpy(out.data(), "Ps2PowerSave", 12);
            SetName(out, 16, directory);
            SetName(out, 48, directory);
            Put(out, 80, uint32_t(compressed.size() + 4));
            Put(out, 84, uint32_t(files.size()));
            Put(out, 88, uint32_t(body.size()));
            out.insert(out.end(), compressed.begin(), compressed.end());
            Put(out, 12, Crc(out));
        }
        Need(out.size() <= Limit, "Save archive is too large.");
        data = std::move(out);
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
