#include "mikupan_save_region.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <iterator>
#include <stdexcept>
namespace MikuPanSave
{
namespace
{
constexpr std::array<size_t, 30> sizes = {
    32,  16,  28,  320, 136, 1024, 4288, 20,   96,    48,
    250, 250, 200, 200, 5,   1200, 1344, 4000, 3600,  42,
    5,   24,  860, 208, 216, 2,    320,  284,  25600, 540960};
constexpr size_t Offset(unsigned block)
{
    size_t at = 16;
    for (unsigned i = 0; i < block; ++i)
    {
        at += sizes[i];
    }
    return at;
}
uint32_t Get(const uint8_t* p, unsigned n = 4)
{
    uint32_t value = 0;
    for (unsigned i = 0; i < n; ++i)
    {
        value |= uint32_t(p[i]) << (8 * i);
    }
    return value;
}
void Put(uint8_t* p, uint32_t value, unsigned n = 4)
{
    for (unsigned i = 0; i < n; ++i)
    {
        p[i] = uint8_t(value >> (8 * i));
    }
}
unsigned Index(SaveRegion region)
{
    const auto n = unsigned(region);
    if (n > 2)
    {
        throw std::runtime_error("Unknown save region.");
    }
    return n;
}
struct Asset
{
    uint16_t id, target[3], types;
    bool event;
};
struct Polygon
{
    int32_t edge[4][3];
    uint8_t type;
};
struct Record
{
    uint16_t first, count;
};
struct RoomDifference
{
    uint16_t key, first[3], count[3];
};
#include "mikupan_save_region_data.inc"
const Asset& FindAsset(SaveRegion region, uint16_t id, unsigned type)
{
    const Asset *begin, *end;
    switch (region)
    {
        case SaveRegion::JP:
            begin = std::begin(assets_jp);
            end = std::end(assets_jp);
            break;
        case SaveRegion::US:
            begin = std::begin(assets_us);
            end = std::end(assets_us);
            break;
        case SaveRegion::EU:
            begin = std::begin(assets_eu);
            end = std::end(assets_eu);
            break;
        default:
            throw std::runtime_error("Unknown save region.");
    }
    auto it = std::lower_bound(begin, end, id, [](const Asset& a, uint16_t v) {
        return a.id < v;
    });
    for (; it != end && it->id == id; ++it)
    {
        if (it->types & (1u << type))
        {
            return *it;
        }
    }
    throw std::runtime_error(
        "Saved resource references do not match the declared region. "
        "This may be an already-converted save or an unsupported game "
        "revision.");
}
bool Inside(uint16_t record, int64_t z, int64_t x)
{
    const auto& r = records[record];
    for (unsigned p = r.first; p < r.first + r.count; ++p)
    {
        const auto& poly = polygons[p];
        bool inside = true;
        for (unsigned i = 0; i < 4; ++i)
        {
            const auto& e = poly.edge[i];
            const int sign = i < poly.type ? -1 : 1;
            inside &=
                (int64_t(e[0]) * z + int64_t(e[1]) * x + e[2]) * sign >= 0;
        }
        if (inside)
        {
            return true;
        }
    }
    return false;
}
void CheckPosition(const Bytes& data, SaveRegion source, SaveRegion target)
{
    if (data[44])
    {
        return;// Chapter-start saves rebuild their player position.
    }
    const unsigned chapter = Get(data.data() + 20),
                   room = Get(data.data() + 24);
    const unsigned floor = data[Offset(7) + 4];
    const unsigned key = chapter * 1024 + floor * 256 + room;
    if (!std::binary_search(std::begin(valid_rooms), std::end(valid_rooms),
                            key))
    {
        throw std::runtime_error(
            "This saved room/floor cannot be checked for regional conversion.");
    }
    float position[4];
    std::memcpy(position, data.data() + Offset(5) + 352, sizeof(position));
    for (float v : position)
    {
        if (!std::isfinite(v) || double(v) < -2147483648.0
            || double(v) > 2147483647.0)
        {
            throw std::runtime_error(
                "Invalid player position for regional conversion.");
        }
    }
    const int64_t x = uint16_t(int32_t(position[0])),
                  z = uint16_t(int32_t(position[2]));
    for (const auto& r : room_differences)
    {
        if (r.key != key)
        {
            continue;
        }
        const unsigned s = Index(source), t = Index(target);
        const auto *src = record_refs + r.first[s],
                   *dst = record_refs + r.first[t];
        for (unsigned i = 0; i < r.count[t]; ++i)
        {
            if (std::find(src, src + r.count[s], dst[i]) == src + r.count[s]
                && Inside(dst[i], z, x))
            {
                bool blocked_before = false;
                for (unsigned j = 0; j < r.count[s]; ++j)
                {
                    blocked_before |= Inside(src[j], z, x);
                }
                if (!blocked_before)
                {
                    throw std::runtime_error(
                        "This position is blocked in the destination region. "
                        "Save at another location in the source game first.");
                }
            }
        }
    }
}
void NormalizeChapterStart(Bytes& data)
{
    if (data[44] != 1)
    {
        return;
    }
    // Chapter initialization discards this entire resource table.
    std::fill_n(data.data() + Offset(3), sizes[3], 0);
    for (unsigned i = 0; i < 40; ++i)
    {
        Put(data.data() + Offset(3) + i * 8, 65535, 2);
    }
    Seal(data);
}
}// namespace
const char* RegionLabel(SaveRegion region)
{
    switch (region)
    {
        case SaveRegion::JP:
            return "Japan / SLPS-25074";
        case SaveRegion::US:
            return "US / SLUS-20388";
        case SaveRegion::EU:
            return "Europe / SLES-50821";
    }
    return "Unknown region";
}
const char* RegionPrefix(SaveRegion region)
{
    switch (region)
    {
        case SaveRegion::JP:
            return "BISLPS-25074zero";
        case SaveRegion::US:
            return "BASLUS-20388zero";
        case SaveRegion::EU:
            return "BESLES-50821zero";
    }
    return "";
}
std::string RegionFolder(SaveRegion region)
{
    return std::string(RegionPrefix(region)) + "0";
}
std::string RegionNotes(SaveRegion source, SaveRegion target)
{
    if (source == target)
    {
        return {};
    }
    std::string text = "Control layout resets to the destination default. ";
    if (source == SaveRegion::JP || target == SaveRegion::JP)
    {
        text +=
            "Existing documents, camera upgrades and optional sound history "
            "are preserved; "
            "regional bonus unlocks are not recalculated.";
    }
    return text;
}
bool ValidateRegion(const Bytes& data, SaveRegion source, std::string& error)
{
    try
    {
        Index(source);
        if (data.size() != Ps2Size || !Validate(data, error))
        {
            throw std::runtime_error(
                error.empty() ? "Expected a PS2 gameplay save." : error);
        }
        if (data[44] > 1
            || (source == SaveRegion::EU ? (data[45] & 127) > 4
                                         : data[45] != 0))
        {
            throw std::runtime_error(
                "Save header does not match its declared region.");
        }
        if (Get(data.data() + Offset(23)))
        {
            throw std::runtime_error(
                "Save again after the wandering soul disappears before "
                "converting.");
        }
        Bytes checked = data;
        NormalizeChapterStart(checked);
        for (unsigned i = 0; i < 40; ++i)
        {
            const auto* p = checked.data() + Offset(3) + i * 8;
            const uint16_t id = Get(p, 2);
            const unsigned type = p[2];
            if (id == 65535)
            {
                continue;
            }
            if (type < 1 || type > 14 || type == 12)
            {
                throw std::runtime_error("Unsupported saved resource request.");
            }
            if (type != 13)
            {
                FindAsset(source, id, type);
            }
            if (type == 7
                && Get(p + 4)
                       != (source == SaveRegion::JP ? 0x800000u : 0x7f8000u))
            {
                throw std::runtime_error(
                    "Saved map references disagree with the declared region. "
                    "This save revision is not supported.");
            }
        }
        error.clear();
        return true;
    }
    catch (const std::exception& e)
    {
        error = e.what();
        return false;
    }
}
bool ConvertRegion(const Bytes& data, SaveRegion source, SaveRegion target,
                   Bytes& output, std::string& error, unsigned language,
                   bool display_60hz)
{
    try
    {
        Index(target);
        if (!ValidateRegion(data, source, error))
        {
            return false;
        }
        if (language > 4)
        {
            throw std::runtime_error("Unknown EU language.");
        }
        Bytes next = data;
        NormalizeChapterStart(next);
        if (source == target)
        {
            output = std::move(next);
            return true;
        }
        CheckPosition(data, source, target);
        for (unsigned i = 0; i < 40; ++i)
        {
            auto* p = next.data() + Offset(3) + i * 8;
            const uint16_t id = Get(p, 2);
            const unsigned type = p[2];
            if (id == 65535 || type == 13)
            {
                continue;
            }
            const auto& a = FindAsset(source, id, type);
            if (a.target[Index(target)] == 65535)
            {
                throw std::runtime_error(
                    "A saved resource has no unambiguous destination "
                    "equivalent.");
            }
            Put(p, a.target[Index(target)], 2);
            if (a.event)
            {
                p[2] = target == SaveRegion::EU ? 14 : 1;
            }
            else if (type == 14)
            {
                throw std::runtime_error(
                    "Unsupported localized resource request.");
            }
            if (type == 7)
            {
                Put(p + 4, target == SaveRegion::JP ? 0x800000 : 0x7f8000);
            }
        }
        if (source == SaveRegion::JP || target == SaveRegion::JP)
        {
            std::fill_n(next.data() + Offset(15), sizes[15], 0);
            for (unsigned i = 0; i < 159; ++i)
            {
                const unsigned western = i < 19 ? i : i + 3;
                std::copy_n(data.data() + Offset(15)
                                + (source == SaveRegion::JP ? i : western) * 4,
                            4,
                            next.data() + Offset(15)
                                + (target == SaveRegion::JP ? i : western) * 4);
            }
            if (target != SaveRegion::JP)
            {
                uint16_t state = 1;
                if (Get(data.data() + 20) == 4 && !data[44])
                {
                    if (data[Offset(10) + 105])
                    {
                        state = 65535;
                    }
                    else if (data[Offset(10) + 100])
                    {
                        if (Get(data.data() + Offset(15) + 18 * 4)
                            || data[Offset(12) + 58])
                        {
                            throw std::runtime_error(
                                "Finish the current quest-item interaction and "
                                "save again.");
                        }
                        state = 0;
                    }
                }
                for (unsigned i = 19; i <= 21; ++i)
                {
                    Put(next.data() + Offset(15) + i * 4, state, 2);
                    Put(next.data() + Offset(15) + i * 4 + 2, state, 2);
                }
            }
        }
        next[Offset(1) + 2] = 0;
        next[45] = target == SaveRegion::EU
                       ? uint8_t(language | (display_60hz ? 128 : 0))
                       : 0;
        Seal(next);
        Bytes native;
        if (!ToNative(next, native, error) || !ToPs2(native, next, error)
            || !ValidateRegion(next, target, error))
        {
            return false;
        }
        output = std::move(next);
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
