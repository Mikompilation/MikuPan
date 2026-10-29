#include "mikupan_save_codec.h"
#include "ingame/event/wan_soul.h"
#include "ingame/map/map_area.h"
#include "main/glob.h"
#include "mc/mc.h"
#include "outgame/btl_mode/btl_mode.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <limits>
namespace MikuPanSave
{
namespace
{
constexpr std::array<size_t, 30> ps2_sizes = {
    32,   16,   28,   320,  136, 1024, 4288, 20,  96,  48,  250, 250, 200, 200,   5,
    1200, 1344, 4000, 3600, 42,  5,    24,   860, 208, 216, 2,   320, 284, 25600, 540960};
struct Ps2Room
{
    uint32_t room, furniture[30];
    uint16_t room_model, furniture_models[30];
    uint8_t id;
};
struct Ps2Door
{
    uint32_t door;
    uint16_t model;
};
struct Ps2Area
{
    Ps2Room rooms[6];
    Ps2Door doors[20];
    alignas(16) float pos[4];
    uint8_t area, previous, room[6], registered[5], event_sound, ghosts;
    uint32_t padding;
};
static_assert(sizeof(Ps2Room) == 188 && sizeof(Ps2Door) == 8 && sizeof(Ps2Area) == 1344);
uint64_t Get(const uint8_t *p, size_t size = 4)
{
    uint64_t n = 0;
    for (size_t i = 0; i < size; ++i)
        n |= uint64_t(p[i]) << (i * 8);
    return n;
}
void Put(uint8_t *p, uint64_t n, size_t size = 4)
{
    for (size_t i = 0; i < size; ++i)
        p[i] = uint8_t(n >> (i * 8));
}
bool Fail(std::string &error, const char *text)
{
    error = text;
    return false;
}
size_t Size(unsigned b, bool native, bool legacy = false)
{
    return native ? (b == 26 && legacy ? 160 : size_t(mc_gamedata_str[b].size)) : ps2_sizes[b];
}
size_t Offset(unsigned b, bool native, bool legacy = false)
{
    size_t o = 16;
    for (unsigned i = 0; i < b; ++i)
        o += Size(i, native, legacy);
    return o;
}
size_t Align(size_t n) { return (n + 3) & ~size_t(3); }
struct Span
{
    size_t ps2, host, size;
};
void Copy(uint8_t *out, const uint8_t *in, bool native, std::initializer_list<Span> spans)
{
    for (const auto &s : spans)
        std::memcpy(out + (native ? s.host : s.ps2), in + (native ? s.ps2 : s.host), s.size);
}
bool Convert(const Bytes &in, Bytes &out, bool native, std::string &error)
{
    if (!Validate(in, error))
        return false;
    bool source_native = in.size() != Ps2Size;
    bool legacy = source_native && in.size() != NativeSize();
    if (source_native && legacy)
    {
        Bytes upgraded(NativeSize(), 0);
        size_t at = Offset(26, true, true);
        std::copy_n(in.data(), at, upgraded.data());
        for (unsigned i = 0; i < 20; ++i)
        {
            auto *d = upgraded.data() + at + i * sizeof(STAGE_WRK);
            const auto *p = in.data() + at + i * 8;
            std::copy_n(p, 4, d);
            Put(d + offsetof(STAGE_WRK, best_shot), uint64_t(int64_t(int32_t(Get(p + 4)))), 8);
        }
        size_t from = Offset(27, true, true), to = Offset(27, true);
        std::copy(in.begin() + from, in.end(), upgraded.begin() + to);
        Seal(upgraded);
        return Convert(upgraded, out, native, error);
    }
    if (source_native == native)
    {
        out = in;
        return true;
    }
    Bytes result(native ? NativeSize() : Ps2Size, 0);
    std::copy_n(in.data(), 16, result.data());
    for (unsigned b = 0; b < 30; ++b)
    {
        const auto *p = in.data() + Offset(b, source_native, legacy);
        auto *dst = result.data() + Offset(b, native);
        if (b == 6)
        {
            continue;
        } // Enemy work is cleared by InGameInit2 before play resumes.
        if (b == 7)
        {
            Copy(dst, p, native,
                 {{4, offsetof(MAP_WRK, floor), 10}, {16, offsetof(MAP_WRK, padding), 4}});
            continue;
        }
        if (b == 16)
        {
            for (unsigned i = 0; i < 6; ++i)
                Copy(dst + i * (native ? sizeof(ROOM_MDATA) : sizeof(Ps2Room)),
                     p + i * (native ? sizeof(Ps2Room) : sizeof(ROOM_MDATA)), native,
                     {{124, offsetof(ROOM_MDATA, mno_room), 63}});
            for (unsigned i = 0; i < 20; ++i)
                Copy(dst + (native ? offsetof(AREA_WRK, dmd) : offsetof(Ps2Area, doors)) +
                         i * (native ? sizeof(DOOR_MDATA) : sizeof(Ps2Door)),
                     p + (native ? offsetof(Ps2Area, doors) : offsetof(AREA_WRK, dmd)) +
                         i * (native ? sizeof(Ps2Door) : sizeof(DOOR_MDATA)),
                     native, {{4, offsetof(DOOR_MDATA, mno_door), 2}});
            Copy(dst, p, native,
                 {{offsetof(Ps2Area, pos), offsetof(AREA_WRK, tmp_after_pos), 31},
                  {offsetof(Ps2Area, padding), offsetof(AREA_WRK, padding), 4}});
            continue;
        }
        if (b == 23)
        {
            if (Get(p) != 0)
                return Fail(error, "A wandering soul is active; save again after it disappears.");
            Copy(dst, p, native,
                 {{0, 0, 168},
                  {172, offsetof(WANDER_SOUL_WRK, room_no), 1},
                  {176, offsetof(WANDER_SOUL_WRK, dist), 4},
                  {184, offsetof(WANDER_SOUL_WRK, wanso_wait_time), 16}});
            dst[offsetof(WANDER_SOUL_WRK, disp_flg)] = 0;
            continue;
        }
        if (Size(b, false) != Size(b, true))
            return Fail(error, "Unsupported native save layout.");
        std::memcpy(dst, p, Size(b, false));
    }
    // Map/room caches are rebuilt by MapDataInit and LoadStartDataSet after loading.
    Seal(result);
    out = std::move(result);
    error.clear();
    return true;
}
} // namespace
size_t NativeSize()
{
    size_t n = 16;
    for (unsigned b = 0; b < 30; ++b)
        n += Size(b, true);
    return Align(n);
}
bool Empty(const Bytes &d) { return d.size() >= 48 && Get(d.data() + 16) == 0; }
bool Checksum(const Bytes &d)
{
    if (d.size() < 16)
        return false;
    uint32_t sum = 0;
    for (size_t i = 16; i < d.size(); ++i)
        sum += d[i];
    return sum == Get(d.data());
}
void Seal(Bytes &d)
{
    uint32_t sum = 0;
    for (size_t i = 16; i < d.size(); ++i)
        sum += d[i];
    Put(d.data(), sum);
}
Bytes EmptySlot(size_t size)
{
    Bytes d(size, 0xff);
    if (size >= 64)
        std::fill(d.begin() + 16, d.begin() + 64, 0);
    return d;
}
bool Validate(const Bytes &d, std::string &error)
{
    if (sizeof(void *) != 8)
        return Fail(error, "Save conversion requires a 64-bit build.");
    size_t old = NativeSize() - 160;
    if (d.size() != Ps2Size && d.size() != NativeSize() && d.size() != old)
        return Fail(error, "Unknown gameplay save size/version.");
    if (Empty(d) || !Checksum(d))
        return Fail(error, "Empty slot or invalid save checksum.");
    const auto *head = d.data() + 16;
    if (Get(head) != 1 || Get(head + 4) > 5 || Get(head + 8) > 255 || Get(head + 16) >= 60 ||
        Get(head + 20) >= 60 || Get(head + 24) >= 60 || head[30] > 1 || head[31] > 3)
        return Fail(error, "Invalid mission, time or difficulty in save header.");
#ifdef BUILD_EU_VERSION
    if ((head[29] & 0x7f) > 4)
        return Fail(error, "Invalid EU save language.");
#endif
    bool n = d.size() != Ps2Size, old_native = n && d.size() != NativeSize();
    for (unsigned b = 0; b < 30; ++b)
        if (Size(b, true) == 0)
            return Fail(error, "Save layout is not ready.");
    const auto *map = d.data() + Offset(7, n, old_native);
    if (map[n ? offsetof(MAP_WRK, floor) : 4] >= 4)
        return Fail(error, "Invalid saved map floor.");
    const auto *player = d.data() + Offset(5, n, old_native);
    for (size_t field : {offsetof(PLYR_WRK, move_box) + offsetof(MOVE_BOX, pos),
                         offsetof(PLYR_WRK, move_box) + offsetof(MOVE_BOX, rot)})
        for (unsigned i = 0; i < 4; ++i)
        {
            float value;
            std::memcpy(&value, player + field + i * 4, 4);
            if (!std::isfinite(value))
                return Fail(error, "Invalid player transform.");
        }
    error.clear();
    return true;
}
bool ToNative(const Bytes &in, Bytes &out, std::string &error)
{
    return Convert(in, out, true, error);
}
bool ToPs2(const Bytes &in, Bytes &out, std::string &error)
{
    return Convert(in, out, false, error);
}
} // namespace MikuPanSave
