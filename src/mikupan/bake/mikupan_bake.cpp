#include "mikupan_bake.h"

#include "mikupan_bake_data.h"
#include "mikupan_bake_format.h"
#include "mikupan_tim2.h"
#include "mikupan/gs/mikupan_gs_memory.h"

#include <algorithm>
#include <cstddef>
#include <cstring>
#include <map>
#include <memory>
#include <set>
#include <unordered_map>
#include <utility>

namespace MikuPanBake
{
namespace
{

constexpr uint64_t kSectorSize = 0x800;
constexpr uint32_t kTex0Register = 6;
constexpr uint32_t kStcycl44 = 0x01000404;
constexpr uint32_t kGifRegsAd = 0x0E;
constexpr const char* k3dPrefix = "bin/3ddata/";
/// Rewritten entries and images start 16-byte aligned after the header.
constexpr uint64_t kDataStart = 128;

std::string Hex(uint64_t v)
{
    static const char digits[] = "0123456789abcdef";
    std::string s;

    do
    {
        s.insert(s.begin(), digits[v & 15]);
        v >>= 4;
    } while (v != 0);

    return "0x" + s;
}

uint64_t Align16(uint64_t v)
{
    return (v + 15) & ~(uint64_t)15;
}

bool EndsWith(const std::string& s, const char* suffix)
{
    const size_t n = std::strlen(suffix);
    return s.size() >= n && s.compare(s.size() - n, n, suffix) == 0;
}

/// 64-bit content hash used to find identical images (offline only; the
/// game never hashes). Matches are confirmed byte by byte.
uint64_t HashBytes(const uint8_t* p, size_t n, uint64_t seed)
{
    uint64_t h = seed ^ (n * 0x9E3779B97F4A7C15ULL);
    size_t i = 0;

    for (; i + 8 <= n; i += 8)
    {
        uint64_t v;
        std::memcpy(&v, p + i, 8);
        v *= 0xFF51AFD7ED558CCDULL;
        v ^= v >> 32;
        h = (h ^ v) * 0xC4CEB9FE1A85EC53ULL;
        h ^= h >> 29;
    }

    for (; i < n; i++)
    {
        h = (h ^ p[i]) * 0x100000001B3ULL;
    }

    h ^= h >> 33;
    h *= 0xFF51AFD7ED558CCDULL;
    h ^= h >> 33;
    return h;
}

// ---------------------------------------------------------------------------
// Player models: the player mesh (.mpk) is drawn with the textures of
// whichever costume .pk2 is loaded (src/data/plyr_file_id.h). The names are
// the same in the NTSC-U and PAL tables.

struct CostumeFiles
{
    const char* pk2;
    const char* bwc;
    const char* clt;
};

struct PlayerModel
{
    const char* mpk;
    int count;
    CostumeFiles costumes[4];
};

#define MIKUPAN_MDL "bin/3ddata/man/mdl/"
const PlayerModel kPlayerModels[] = {
    {MIKUPAN_MDL "m000_miku.mpk",
     4,
     {{MIKUPAN_MDL "m000_miku.pk2", MIKUPAN_MDL "m000_miku.bwc",
       MIKUPAN_MDL "m000_miku.clt"},
      {MIKUPAN_MDL "m000_spe1.pk2", MIKUPAN_MDL "m000_spe1.bwc",
       MIKUPAN_MDL "m000_spe1.clt"},
      {MIKUPAN_MDL "m000_spe2.pk2", MIKUPAN_MDL "m000_spe2.bwc",
       MIKUPAN_MDL "m000_spe2.clt"},
      {MIKUPAN_MDL "m000_spe3.pk2", MIKUPAN_MDL "m000_spe3.bwc",
       MIKUPAN_MDL "m000_spe3.clt"}}},
    {MIKUPAN_MDL "m001_mafuyu.mpk",
     1,
     {{MIKUPAN_MDL "m001_mafuyu.pk2", MIKUPAN_MDL "m001_mafuyu.bwc",
       MIKUPAN_MDL "m001_mafuyu.clt"}}},
};
#undef MIKUPAN_MDL

constexpr int kPlayerModelCount =
    (int)(sizeof(kPlayerModels) / sizeof(kPlayerModels[0]));

// ---------------------------------------------------------------------------
// 2D textures the game draws by a TEX0 that is no picture's own: sprite
// table entries (src/data*/) whose texels and CLUT lie inside the images of
// one 2D file. They are baked in every file that writes all of what the
// used rectangles read. The values are the same in the NTSC-U and PAL
// tables.

struct DerivedRect
{
    int u;
    int v;
    int w;
    int h;
};

struct DerivedTexture
{
    const char* label;
    uint64_t tex0;
    int rect_count;
    DerivedRect rects[10];
};

const DerivedTexture kDerivedTextures[] = {
    /// mesbox[]: inside two images of the effect pack (eff001.pk2).
    {"message box",
     0x2006e6059940b6ceULL,
     8,
     {{1, 1, 30, 14},
      {33, 1, 30, 14},
      {1, 17, 30, 14},
      {33, 17, 30, 14},
      {1, 33, 30, 14},
      {33, 33, 30, 14},
      {1, 49, 30, 14},
      {33, 49, 30, 14}}},
    /// btntex[]: an effect pack image drawn with a CLUT that lies inside
    /// another of its images.
    {"button icons",
     0x2006e4059940b5c0ULL,
     4,
     {{0, 0, 24, 24}, {26, 0, 24, 24}, {0, 26, 24, 24}, {26, 26, 24, 24}}},
    /// number_tex[1]: digits inside the 512x512 images at block 0x1A40
    /// (camera HUD, mode select, logo).
    {"number font 1",
     0x2003b8a621311cc5ULL,
     10,
     {{0, 0, 24, 40},
      {24, 0, 24, 40},
      {48, 0, 24, 40},
      {72, 0, 24, 40},
      {96, 0, 24, 40},
      {0, 40, 24, 40},
      {24, 40, 24, 40},
      {48, 40, 24, 40},
      {72, 40, 24, 40},
      {96, 40, 24, 40}}},
    /// font_sprt[] of the PAL title screen.
    {"title letters",
     0x200519a5dd30a88dULL,
     2,
     {{1, 1, 126, 48}, {1, 51, 10, 48}}},
};

// ---------------------------------------------------------------------------
// GS VRAM that also records which nibbles an upload wrote, so a decode can
// tell whether every texel and CLUT entry it read came from the replayed
// uploads (and not from whatever the game had in VRAM before).

class Vram
{
  public:
    Vram() : written_((size_t)GS::kVramAllocSize * 2 / 64, 0)
    {
    }

    /// Clears what uploads wrote since the last reset: a 2D file touches a
    /// few hundred KB of the 8 MB, a model a few MB.
    void Reset()
    {
        if (dirty_begin_ < dirty_end_)
        {
            const size_t first_byte = (size_t)(dirty_begin_ / 2);
            const size_t end_byte = (size_t)((dirty_end_ + 1) / 2);
            std::memset(gs_.mem_.data() + first_byte, 0, end_byte - first_byte);
            std::fill(written_.begin() + (ptrdiff_t)(dirty_begin_ / 64),
                      written_.begin() + (ptrdiff_t)((dirty_end_ + 63) / 64),
                      0);
        }

        dirty_begin_ = ~(uint64_t)0;
        dirty_end_ = 0;
    }

    /// GS::GSHelper::Upload(): `ignored` counts transfers with a DPSM it
    /// does not store.
    bool Apply(const Upload& u, size_t* ignored, std::string* error)
    {
        const size_t need = UploadBytes(u);

        if (need == 0)
        {
            (*ignored)++;
            return true;
        }

        if (need > u.available)
        {
            *error = "an upload reads past the end of its file";
            return false;
        }

        gs_.Upload(u.dpsm, u.dbp, u.dbw, u.dsax, u.dsay, u.rrw, u.rrh,
                   u.data);

        for (int y = u.dsay; y < u.dsay + u.rrh; ++y)
        {
            for (int x = u.dsax; x < u.dsax + u.rrw; ++x)
            {
                switch (u.dpsm)
                {
                    case PSMZ32:
                    case PSMCT32:
                        Mark(2 * (uint64_t)GS::GetPixelAddressPSMCT32(
                                     u.dbp, u.dbw, x, y),
                             8);
                        break;
                    case PSMT8:
                        Mark(2 * (uint64_t)(GS::GetPixelAddressPSMT8(
                                                u.dbp, u.dbw, x, y)
                                            & kByteMask),
                             2);
                        break;
                    case PSMT4:
                        Mark((uint64_t)(GS::GetPixelAddressPSMT4(u.dbp, u.dbw,
                                                                 x, y)
                                        & kNibbleMask),
                             1);
                        break;
                    default:
                        Mark(2 * (uint64_t)GS::GetPixelAddressPSMCT16(
                                     u.dbp, u.dbw, x, y),
                             4);
                        break;
                }
            }
        }

        return true;
    }

    /// GS::GSHelper::DecodeTexture() into `out`; `uncovered` counts texels
    /// of the top-left `region_w` x `region_h` whose index/colour or CLUT
    /// entry no replayed upload wrote.
    bool Decode(const GS::Tex0& t, uint8_t* out, size_t* uncovered,
                int region_w = 1 << 30, int region_h = 1 << 30) const
    {
        if (!gs_.DecodeTexture(t, out))
        {
            return false;
        }

        *uncovered = Uncovered(t, 0, 0, std::min(1 << t.tw, region_w),
                               std::min(1 << t.th, region_h));
        return true;
    }

    /// GS::GSHelper::DecodeTexture() into `out`, without the coverage count.
    bool Decode(const GS::Tex0& t, uint8_t* out) const
    {
        return gs_.DecodeTexture(t, out);
    }

    /// Texels of the rectangle whose index/colour or CLUT entry no replayed
    /// upload wrote.
    size_t Uncovered(const GS::Tex0& t, int x0, int y0, int w, int h) const
    {
        size_t missing = 0;

        for (int y = y0; y < y0 + h; ++y)
        {
            for (int x = x0; x < x0 + w; ++x)
            {
                bool ok = true;

                switch (t.psm)
                {
                    case PSMCT32:
                        ok = Written(2 * (uint64_t)GS::GetPixelAddressPSMCT32(
                                             t.tbp0, t.tbw, x, y),
                                     8);
                        break;
                    case PSMCT16:
                    case PSMCT16S:
                        ok = Written(2 * (uint64_t)GS::GetPixelAddressPSMCT16(
                                             t.tbp0, t.tbw, x, y),
                                     4);
                        break;
                    case PSMT8:
                    {
                        const int a =
                            GS::GetPixelAddressPSMT8(t.tbp0, t.tbw, x, y)
                            & kByteMask;
                        const int index = (uint8_t)gs_.mem_[a];
                        int cy = (index & 0xE0) >> 4;
                        int cx = index & 0x07;
                        if (index & 0x08)
                            cy++;
                        if (index & 0x10)
                            cx += 8;
                        const int p =
                            GS::GetPixelAddressPSMCT32(t.cbp, t.tbw, cx, cy);
                        ok = Written(2 * (uint64_t)a, 2)
                             && Written(2 * (uint64_t)p, 8);
                        break;
                    }
                    case PSMT4:
                    {
                        const int n =
                            GS::GetPixelAddressPSMT4(t.tbp0, t.tbw, x, y)
                            & kNibbleMask;
                        const int index =
                            ((uint8_t)gs_.mem_[n >> 1] >> ((n & 1) << 2))
                            & 0x0F;
                        const int cy = ((index >> 3) & 1) + (t.csa & 0x0E);
                        const int cx = (index & 7) + ((t.csa & 1) << 3);
                        const int p =
                            GS::GetPixelAddressPSMCT32(t.cbp, t.tbw, cx, cy);
                        ok = Written((uint64_t)n, 1)
                             && Written(2 * (uint64_t)p, 8);
                        break;
                    }
                    default:
                        break;
                }

                if (!ok)
                {
                    missing++;
                }
            }
        }

        return missing;
    }

  private:
    static constexpr int kByteMask = GS::kVramAllocSize - 1;
    static constexpr int kNibbleMask = 2 * GS::kVramAllocSize - 1;

    /// `count` (1, 2, 4 or 8) nibbles from `nibble`, aligned to `count`.
    void Mark(uint64_t nibble, int count)
    {
        written_[nibble >> 6] |= ((1ULL << count) - 1) << (nibble & 63);
        dirty_begin_ = std::min(dirty_begin_, nibble);
        dirty_end_ = std::max(dirty_end_, nibble + (uint64_t)count);
    }

    bool Written(uint64_t nibble, int count) const
    {
        const uint64_t mask = ((1ULL << count) - 1) << (nibble & 63);
        return (written_[nibble >> 6] & mask) == mask;
    }

    GS::GSHelper gs_;
    std::vector<uint64_t> written_;
    /// Nibbles uploads wrote since the last Reset().
    uint64_t dirty_begin_ = ~(uint64_t)0;
    uint64_t dirty_end_ = 0;
};

// ---------------------------------------------------------------------------
// What one archive entry needs: its texture contexts (sets of uploads that
// make up a VRAM state), its mesh slots and the containers to rewrite.

enum class Selector
{
    kNone,   ///< one state
    kFrames, ///< texture animation: state k = frames 0..k uploaded
    kCostume,///< player: state k = costume k's .pk2 uploaded
};

struct Context
{
    Selector selector = Selector::kNone;
    /// VRAM state per selector value (at least one).
    std::vector<std::vector<Upload>> colour;
    /// Black & white state per selector value, or empty.
    std::vector<std::vector<Upload>> mono;
    /// Player: colour state after SetPlyrClut(0) uploaded the .clt CLUTs.
    /// Must decode exactly like `colour`.
    std::vector<std::vector<Upload>> restored;
    bool player = false;
    std::string label;
};

struct SlotRef
{
    size_t slot = 0;///< absolute offset of the 16 data bytes
    uint64_t tex0 = 0;
    int context = -1;///< -1: no upload in the file writes its texture
};

enum class SiteKind
{
    kTri2Unit,     ///< becomes kUnitTextureList, list at unit + 16
    kAnimationUnit,///< parameters kept, list at the old frame data
    kTexturePak,   ///< pak header count 0, list at + 16
};

struct ListSite
{
    SiteKind kind = SiteKind::kTri2Unit;
    size_t at = 0;  ///< unit, or pak header
    size_t list = 0;///< TextureListHeader
    size_t end = 0; ///< the list must end before this
    std::vector<int> contexts;
};

struct FilePlan
{
    std::vector<Context> contexts;
    std::vector<SlotRef> slots;
    std::vector<ListSite> sites;
    std::vector<Unit> retired;
    std::vector<std::unique_ptr<std::vector<uint8_t>>> buffers;///< costume
                                                               ///< files
    size_t sgds = 0;
    int player_model = -1;
};

/// The id and info a TEX0 resolves to in one context.
struct Resolved
{
    uint32_t id = kNoTexture;
    uint32_t info = 0;
    uint32_t variants = 0;///< consecutive ids from `id`
};

/// Decoded images of one context: images[state][tex0 index].
struct DecodedContext
{
    std::vector<uint64_t> tex0s;
    std::vector<bool> valid;///< the game binds a texture for this TEX0
    std::vector<std::vector<std::vector<uint8_t>>> colour;
    std::vector<std::vector<std::vector<uint8_t>>> mono;
};

// ---------------------------------------------------------------------------

struct File2D;

class Baker
{
  public:
    Baker(const Source& source, const Options& options, Sink* sink,
          Stats* stats)
        : source_(source), options_(options), sink_(sink), stats_(stats)
    {
    }

    bool Run(std::string* error);

  private:
    bool ReadEntry(size_t fid, std::vector<uint8_t>* out, std::string* error);
    int FindName(const char* name) const;
    void Log(const std::string& line) const;
    void Progress(const char* stage, size_t done, size_t total) const;
    bool Cancelled(std::string* error) const;

    bool BuildPlan(size_t fid, const std::vector<uint8_t>& data,
                   FilePlan* plan, std::string* error);
    bool PlanGeneric(const std::vector<uint8_t>& data, FilePlan* plan,
                     std::string* error);
    bool PlanMdl(const std::vector<uint8_t>& data, size_t base, size_t size,
                 int depth, FilePlan* plan, std::string* error);
    bool PlanPlayer(int model, const std::vector<uint8_t>& data,
                    FilePlan* plan, std::string* error);
    bool AddSlots(const std::vector<uint8_t>& data, const SgdRef& sgd,
                  int context, FilePlan* plan, std::string* error);

    bool Decode(const FilePlan& plan, const std::string& name,
                bool report, std::vector<DecodedContext>* decoded,
                std::string* error);

    bool StoreImage(int w, int h, const uint8_t* rgba, uint32_t* id,
                    std::string* error);
    bool AssignIds(const std::vector<const uint8_t*>& images, int w, int h,
                   uint32_t* base, std::string* error);
    bool ReadImage(uint32_t id, std::vector<uint8_t>* out,
                   std::string* error);

    bool BakeFile(size_t fid, const std::vector<uint8_t>& data,
                  std::vector<uint8_t>* out, std::string* error);
    bool WriteLists(const FilePlan& plan,
                    const std::vector<std::vector<Resolved>>& resolved,
                    std::vector<uint8_t>* out, std::string* error);
    bool BakePlayerCostume(int model, int costume, size_t fid,
                           std::vector<uint8_t>* out, std::string* error);
    bool WriteEntry(size_t fid, const std::vector<uint8_t>& data,
                    std::string* error);

    bool Bake2D(std::string* error);
    bool Bake2DFile(size_t fid, const std::vector<uint8_t>& data,
                    const std::vector<size_t>& headers, std::string* error);
    bool BakeDerived(const std::string& name, const File2D& f,
                     const std::vector<uint32_t>& ids, std::string* error);
    bool StorePixels(const std::vector<uint8_t>& rgba, uint64_t* offset,
                     std::string* error);

    bool Verify(std::string* error);
    bool VerifyFile(size_t fid, std::string* error);
    bool Verify2DFile(size_t fid, std::string* error);
    bool WritePack(std::string* error);

    const Source& source_;
    const Options& options_;
    Sink* sink_;
    Stats* stats_;

    std::vector<std::pair<uint32_t, uint32_t>> entries_;
    std::map<std::pair<uint32_t, uint32_t>, EntryOverride> overrides_;
    std::vector<size_t> rewritten_;
    uint64_t pos_ = kDataStart;

    std::vector<TextureEntry> directory_;
    std::unordered_multimap<uint64_t, uint32_t> by_hash_;
    std::map<std::vector<uint32_t>, uint32_t> groups_;

    /// Player models: the (id, info) of every textured slot, in slot order,
    /// for the costume .pk2 lists.
    std::vector<std::pair<uint32_t, uint32_t>> player_slots_[2];
    bool player_baked_[2] = {false, false};

    /// 2D textures, the derived textures each 2D id's upload completes,
    /// and the stamps per archive entry.
    std::vector<Tex2DEntry> tex2d_;
    std::vector<std::vector<Tex2DDerived>> derived_;
    std::vector<PatchRecord> patches_;
    std::map<std::pair<uint32_t, uint32_t>, PatchRange> patch_ranges_;
    std::vector<size_t> files_2d_;

    Vram vram_;
};

void Baker::Log(const std::string& line) const
{
    if (options_.log)
    {
        options_.log(line);
    }
}

void Baker::Progress(const char* stage, size_t done, size_t total) const
{
    if (options_.progress)
    {
        options_.progress(stage, done, total);
    }
}

bool Baker::Cancelled(std::string* error) const
{
    if (options_.cancelled && options_.cancelled())
    {
        *error = "the bake was cancelled";
        return true;
    }

    return false;
}

int Baker::FindName(const char* name) const
{
    for (size_t i = 0; i < source_.names.size(); i++)
    {
        if (source_.names[i] == name)
        {
            return (int)i;
        }
    }

    return -1;
}

bool Baker::ReadEntry(size_t fid, std::vector<uint8_t>* out,
                      std::string* error)
{
    const uint64_t offset = (uint64_t)entries_[fid].first * kSectorSize;
    const uint32_t size = entries_[fid].second;

    if (offset + size > source_.bd_size)
    {
        *error = "entry " + std::to_string(fid) + " lies outside IMG_BD.BIN";
        return false;
    }

    out->resize(size);

    if (size != 0 && !source_.read_bd(offset, out->data(), size))
    {
        *error = "cannot read entry " + std::to_string(fid) + " of IMG_BD.BIN";
        return false;
    }

    return true;
}

// --- Planning ---------------------------------------------------------------

/// Checks the A+D packet around a slot (see mikupan_bake_format.h) and that
/// the data is not baked already.
bool CheckSlotPacket(const std::vector<uint8_t>& data, size_t slot,
                     uint32_t marker, std::string* error)
{
    if (marker == kSlotMarker)
    {
        *error = "the archive is already baked; bake the original IMG_BD.BIN";
        return false;
    }

    if (marker != kTex0Register || slot < 0x18
        || Rd32(&data[slot - 0x18]) != kStcycl44
        || (Rd32(&data[slot - 0x14]) >> 16) != 0x6C02
        || Rd32(&data[slot - 0x08]) != kGifRegsAd
        || Rd32(&data[slot + 0x0C]) != 0)
    {
        *error = "unexpected texture packet at " + Hex(slot);
        return false;
    }

    return true;
}

bool Baker::AddSlots(const std::vector<uint8_t>& data, const SgdRef& sgd,
                     int context, FilePlan* plan, std::string* error)
{
    std::vector<std::vector<Unit>> blocks;

    if (!WalkChains(data.data(), sgd, &blocks, error))
    {
        return false;
    }

    for (const std::vector<Unit>& units : blocks)
    {
        for (const Unit& u : units)
        {
            if (u.category == kUnitTextureList || u.category == kUnitRetired)
            {
                *error = "the archive is already baked; bake the original "
                         "IMG_BD.BIN";
                return false;
            }

            if (u.category != kUnitMesh)
            {
                continue;
            }

            uint32_t marker = 0;
            const size_t slot = MeshTextureSlot(data.data(), u, &marker);

            if (slot == 0)
            {
                continue;
            }

            if (!CheckSlotPacket(data, slot, marker, error))
            {
                return false;
            }

            SlotRef s;
            s.slot = slot;
            s.tex0 = Rd64(&data[slot]);
            s.context = context;
            plan->slots.push_back(s);
        }
    }

    plan->sgds++;
    return true;
}

/// Room, furniture, door and item files, `.acs` and the TRI2 `.mpk`: each
/// SGD with upload units in block 0 (SgSortPreProcess*) is a context of its
/// own; an SGD without any (a room's far and shadow models) draws with the
/// VRAM the previous one left.
bool Baker::PlanGeneric(const std::vector<uint8_t>& data, FilePlan* plan,
                        std::string* error)
{
    std::vector<SgdRef> sgds;
    FindSgds(data.data(), 0, data.size(), &sgds);

    std::vector<size_t> waiting;///< slot index ranges without a context yet
    int current = -1;

    for (const SgdRef& sgd : sgds)
    {
        std::vector<std::vector<Unit>> blocks;

        if (!WalkChains(data.data(), sgd, &blocks, error))
        {
            return false;
        }

        std::vector<Unit> tri2;
        std::vector<Unit> mono;
        std::vector<Unit> animation;

        if (!blocks.empty())
        {
            for (const Unit& u : blocks[0])
            {
                if (u.category == kUnitTri2)
                {
                    tri2.push_back(u);
                }
                else if (u.category == kUnitMonoClut)
                {
                    mono.push_back(u);
                }
                else if (u.category == kUnitTextureAnimation)
                {
                    animation.push_back(u);
                }
            }
        }

        if (tri2.empty() && mono.empty() && animation.empty())
        {
            const size_t first = plan->slots.size();

            if (!AddSlots(data, sgd, current, plan, error))
            {
                return false;
            }

            if (current < 0)
            {
                for (size_t i = first; i < plan->slots.size(); i++)
                {
                    waiting.push_back(i);
                }
            }

            continue;
        }

        if (animation.size() > 1)
        {
            *error = "an SGD has more than one texture animation unit";
            return false;
        }

        Context c;
        std::vector<Upload> base;

        for (const Unit& u : tri2)
        {
            if (!ParseTri2Unit(data.data(), data.size(), u, &base, error))
            {
                return false;
            }
        }

        std::vector<Upload> mono_uploads;

        for (const Unit& u : mono)
        {
            if (!ParseTri2Unit(data.data(), data.size(), u, &mono_uploads,
                               error))
            {
                return false;
            }
        }

        ListSite site;
        bool have_site = false;

        if (!animation.empty())
        {
            TextureAnimationInfo info;
            std::vector<std::vector<Upload>> frames;

            if (!ParseTextureAnimation(data.data(), data.size(), animation[0],
                                       &info, &frames, error))
            {
                return false;
            }

            /// LoadTextureAnimation() uploads frames 0..current every time,
            /// after the TRI2 units; state k is what frame k shows.
            std::vector<Upload> state = base;

            for (const std::vector<Upload>& frame : frames)
            {
                state.insert(state.end(), frame.begin(), frame.end());
                c.colour.push_back(state);
            }

            if (frames.size() > 1)
            {
                c.selector = Selector::kFrames;
            }

            ListSite a;
            a.kind = SiteKind::kAnimationUnit;
            a.at = animation[0].off;
            a.list = info.frames_off;
            a.end = animation[0].off + animation[0].len;
            a.contexts.push_back((int)plan->contexts.size());
            plan->sites.push_back(a);
        }

        if (c.colour.empty())
        {
            c.colour.push_back(base);
        }

        if (!mono.empty())
        {
            /// SgSortPreProcessP() loads the mono CLUT units after the TRI2
            /// units and the animation frames.
            for (const std::vector<Upload>& state : c.colour)
            {
                std::vector<Upload> bw = state;
                bw.insert(bw.end(), mono_uploads.begin(), mono_uploads.end());
                c.mono.push_back(bw);
            }
        }

        /// The first TRI2 unit carries the list; with no TRI2 unit, the
        /// first mono CLUT unit does (the animation unit lists its ids too).
        std::vector<Unit> uploads = tri2;
        uploads.insert(uploads.end(), mono.begin(), mono.end());

        for (const Unit& u : uploads)
        {
            if (!have_site)
            {
                site.kind = SiteKind::kTri2Unit;
                site.at = u.off;
                site.list = u.off + 16;
                site.end = u.off + u.len;
                site.contexts.push_back((int)plan->contexts.size());
                have_site = true;
            }
            else
            {
                plan->retired.push_back(u);
            }
        }

        if (have_site)
        {
            plan->sites.push_back(site);
        }

        current = (int)plan->contexts.size();
        c.label = "SGD @" + Hex(sgd.top);
        plan->contexts.push_back(std::move(c));

        if (!AddSlots(data, sgd, current, plan, error))
        {
            return false;
        }

        for (size_t i : waiting)
        {
            plan->slots[i].context = current;
        }

        waiting.clear();
    }

    return true;
}

/// Character .mdl: a pak with a type-5 entry (.mpk of SGDs) and a type-6
/// entry (pak of TIM2 files that SetManmdlTm2() uploads at the model's VRAM
/// offset; baked at offset 0, which relocates identically).
/// m055_syouall.mdl nests three such paks.
bool Baker::PlanMdl(const std::vector<uint8_t>& data, size_t base,
                    size_t size, int depth, FilePlan* plan,
                    std::string* error)
{
    std::vector<PakEntry> entries;

    if (!ReadPak(data.data(), base, size, &entries))
    {
        return true;
    }

    const PakEntry* models = nullptr;
    const PakEntry* textures = nullptr;

    for (const PakEntry& e : entries)
    {
        if (e.type == kPakModels && models == nullptr)
        {
            models = &e;
        }
        else if (e.type == kPakTextures && textures == nullptr)
        {
            textures = &e;
        }
    }

    if (models == nullptr || textures == nullptr)
    {
        if (depth < 2)
        {
            for (const PakEntry& e : entries)
            {
                if (!PlanMdl(data, e.off, e.size, depth + 1, plan, error))
                {
                    return false;
                }
            }
        }

        return true;
    }

    /// A model with an empty texture pak (the shadow-only models) uploads
    /// nothing; any textured mesh in it has no source.
    int context = -1;

    if (textures->size < 16 || Rd32(&data[textures->off]) != 0)
    {
        Context c;
        c.colour.emplace_back();
        c.label = "model @" + Hex(base);

        if (!ParseTim2Pak(data.data(), data.size(), textures->off,
                          textures->size, 0, &c.colour[0], error))
        {
            return false;
        }

        context = (int)plan->contexts.size();
        plan->contexts.push_back(std::move(c));

        ListSite site;
        site.kind = SiteKind::kTexturePak;
        site.at = textures->off;
        site.list = textures->off + 16;
        site.end = textures->off + textures->size;
        site.contexts.push_back(context);
        plan->sites.push_back(site);
    }

    std::vector<SgdRef> sgds;
    FindSgds(data.data(), models->off, models->size, &sgds);

    for (const SgdRef& sgd : sgds)
    {
        std::vector<std::vector<Unit>> blocks;

        if (!WalkChains(data.data(), sgd, &blocks, error))
        {
            return false;
        }

        for (const Unit& u : blocks.empty() ? std::vector<Unit>() : blocks[0])
        {
            if (u.category == kUnitTri2 || u.category == kUnitMonoClut
                || u.category == kUnitTextureAnimation)
            {
                *error = "a character model has its own upload units";
                return false;
            }
        }

        if (!AddSlots(data, sgd, context, plan, error))
        {
            return false;
        }
    }

    return true;
}

/// Player .mpk: one context whose states are the costume .pk2 uploads; the
/// black & white states add the costume's .bwc CLUTs (SetPlyrClut(1)).
bool Baker::PlanPlayer(int model, const std::vector<uint8_t>& data,
                       FilePlan* plan, std::string* error)
{
    const PlayerModel& pm = kPlayerModels[model];
    Context c;
    c.player = true;
    c.selector = pm.count > 1 ? Selector::kCostume : Selector::kNone;
    c.label = pm.mpk;

    for (int k = 0; k < pm.count; k++)
    {
        const char* files[3] = {pm.costumes[k].pk2, pm.costumes[k].bwc,
                                pm.costumes[k].clt};
        std::vector<Upload> uploads[3];

        for (int f = 0; f < 3; f++)
        {
            const int fid = FindName(files[f]);

            if (fid < 0)
            {
                *error = std::string("missing player file ") + files[f];
                return false;
            }

            plan->buffers.push_back(std::make_unique<std::vector<uint8_t>>());
            std::vector<uint8_t>& buf = *plan->buffers.back();

            if (!ReadEntry((size_t)fid, &buf, error)
                || !ParseTim2Pak(buf.data(), buf.size(), 0, buf.size(), 0,
                                 &uploads[f], error))
            {
                *error = std::string(files[f]) + ": " + *error;
                return false;
            }
        }

        std::vector<Upload> bw = uploads[0];
        bw.insert(bw.end(), uploads[1].begin(), uploads[1].end());
        std::vector<Upload> restored = uploads[0];
        restored.insert(restored.end(), uploads[2].begin(), uploads[2].end());

        c.colour.push_back(uploads[0]);
        c.mono.push_back(bw);
        c.restored.push_back(restored);
    }

    plan->contexts.push_back(std::move(c));
    plan->player_model = model;

    std::vector<SgdRef> sgds;
    FindSgds(data.data(), 0, data.size(), &sgds);

    for (const SgdRef& sgd : sgds)
    {
        if (!AddSlots(data, sgd, 0, plan, error))
        {
            return false;
        }
    }

    return true;
}

bool Baker::BuildPlan(size_t fid, const std::vector<uint8_t>& data,
                      FilePlan* plan, std::string* error)
{
    const std::string& name = source_.names[fid];

    for (int m = 0; m < kPlayerModelCount; m++)
    {
        if (name == kPlayerModels[m].mpk)
        {
            return PlanPlayer(m, data, plan, error);
        }
    }

    if (EndsWith(name, ".mdl"))
    {
        if (!PlanMdl(data, 0, data.size(), 0, plan, error))
        {
            return false;
        }

        /// Any SGD the model walk did not reach would keep TEX0 slots.
        std::vector<SgdRef> sgds;
        FindSgds(data.data(), 0, data.size(), &sgds);

        if (sgds.size() != plan->sgds)
        {
            *error = "a character file has models outside its texture pak";
            return false;
        }

        return true;
    }

    return PlanGeneric(data, plan, error);
}

// --- Decoding ---------------------------------------------------------------

/// `report`: count and log texels the replayed uploads do not cover (the
/// bake pass reports, the verify pass does not repeat it).
bool Baker::Decode(const FilePlan& plan, const std::string& name, bool report,
                   std::vector<DecodedContext>* decoded, std::string* error)
{
    decoded->assign(plan.contexts.size(), {});

    for (const SlotRef& s : plan.slots)
    {
        if (s.context < 0)
        {
            continue;
        }

        DecodedContext& d = (*decoded)[s.context];

        if (std::find(d.tex0s.begin(), d.tex0s.end(), s.tex0) == d.tex0s.end())
        {
            d.tex0s.push_back(s.tex0);
        }
    }

    for (size_t c = 0; c < plan.contexts.size(); c++)
    {
        const Context& ctx = plan.contexts[c];
        DecodedContext& d = (*decoded)[c];

        d.valid.resize(d.tex0s.size());

        for (size_t t = 0; t < d.tex0s.size(); t++)
        {
            const GS::Tex0 tex0 = GS::DecodeTex0(d.tex0s[t]);
            int addr;
            int size;
            d.valid[t] = GS::GetRawTextureRegion(tex0, &addr, &size)
                         && GS::IsDecodableTexture(tex0);
        }

        /// pass 0: colour, 1: black & white, 2: player colour after .clt
        for (int pass = 0; pass < 3; pass++)
        {
            const std::vector<std::vector<Upload>>& states =
                pass == 0 ? ctx.colour : pass == 1 ? ctx.mono : ctx.restored;

            if (pass == 1)
            {
                d.mono.assign(states.size(), {});
            }
            else if (pass == 0)
            {
                d.colour.assign(states.size(), {});
            }

            for (size_t s = 0; s < states.size(); s++)
            {
                size_t ignored = 0;
                vram_.Reset();

                for (const Upload& u : states[s])
                {
                    if (!vram_.Apply(u, &ignored, error))
                    {
                        *error = name + " (" + ctx.label + "): " + *error;
                        return false;
                    }
                }

                if (ignored != 0 && report)
                {
                    Log(name + ": " + std::to_string(ignored)
                        + " uploads with a format the game ignores");
                }

                std::vector<std::vector<uint8_t>> images(d.tex0s.size());

                for (size_t t = 0; t < d.tex0s.size(); t++)
                {
                    if (!d.valid[t])
                    {
                        continue;
                    }

                    const GS::Tex0 tex0 = GS::DecodeTex0(d.tex0s[t]);
                    std::vector<uint8_t>& img = images[t];
                    size_t uncovered = 0;

                    img.resize((size_t)4 << (tex0.tw + tex0.th));
                    vram_.Decode(tex0, img.data(), &uncovered);

                    if (uncovered != 0 && pass == 0 && report)
                    {
                        stats_->uncovered_texels += uncovered;
                        stats_->uncovered_textures++;
                        Log(name + " (" + ctx.label + "): TEX0 "
                            + Hex(d.tex0s[t]) + " reads "
                            + std::to_string(uncovered)
                            + " texels no upload of the file wrote");
                    }
                }

                if (pass == 0)
                {
                    d.colour[s] = std::move(images);
                }
                else if (pass == 1)
                {
                    d.mono[s] = std::move(images);
                }
                else
                {
                    for (size_t t = 0; t < d.tex0s.size(); t++)
                    {
                        if (d.valid[t] && images[t] != d.colour[s][t])
                        {
                            *error = name + ": the colour CLUTs of the "
                                            "player .clt differ from the "
                                            "costume .pk2 (TEX0 "
                                     + Hex(d.tex0s[t]) + ")";
                            return false;
                        }
                    }
                }
            }
        }
    }

    return true;
}

enum class BwMatch
{
    kSame, ///< black & white image equals the colour image
    kGray, ///< black & white image is gray(colour) within 1
    kOther,///< anything else: cannot be expressed with the shader flag
};

/// Every texel: same alpha, and each channel of the black & white texel
/// within 1 of (r + g + b) / 3 of the colour texel (ToBlackWhite() in
/// mikupan_common.hlsli).
BwMatch CompareBlackWhite(const std::vector<uint8_t>& colour,
                          const std::vector<uint8_t>& mono)
{
    bool same = true;

    for (size_t i = 0; i + 3 < colour.size(); i += 4)
    {
        const int gray = (colour[i] + colour[i + 1] + colour[i + 2]) / 3;

        if (mono[i + 3] != colour[i + 3])
        {
            return BwMatch::kOther;
        }

        for (int ch = 0; ch < 3; ch++)
        {
            const int d = (int)mono[i + ch] - gray;

            if (d < -1 || d > 1)
            {
                return BwMatch::kOther;
            }

            if (mono[i + ch] != colour[i + ch])
            {
                same = false;
            }
        }
    }

    return same ? BwMatch::kSame : BwMatch::kGray;
}

// --- Images and ids -----------------------------------------------------------

bool Baker::ReadImage(uint32_t id, std::vector<uint8_t>* out,
                      std::string* error)
{
    const TextureEntry& e = directory_[id];
    out->resize((size_t)e.width * e.height * 4);

    if (!sink_->read(e.offset, out->data(), out->size()))
    {
        *error = "cannot read back the pack";
        return false;
    }

    return true;
}

bool Baker::StoreImage(int w, int h, const uint8_t* rgba, uint32_t* id,
                       std::string* error)
{
    const size_t bytes = (size_t)w * h * 4;
    const uint64_t hash = HashBytes(rgba, bytes, ((uint64_t)w << 16) | h);
    auto range = by_hash_.equal_range(hash);
    std::vector<uint8_t> stored;

    for (auto it = range.first; it != range.second; ++it)
    {
        const TextureEntry& e = directory_[it->second];

        if (e.width != w || e.height != h)
        {
            continue;
        }

        if (!ReadImage(it->second, &stored, error))
        {
            return false;
        }

        if (std::memcmp(stored.data(), rgba, bytes) == 0)
        {
            *id = it->second;
            return true;
        }
    }

    const uint64_t offset = Align16(pos_);

    if (!sink_->write(offset, rgba, bytes))
    {
        *error = "cannot write the pack";
        return false;
    }

    pos_ = offset + bytes;

    TextureEntry e;
    e.offset = offset;
    e.width = (uint16_t)w;
    e.height = (uint16_t)h;
    e.owner = (uint32_t)directory_.size();
    *id = e.owner;
    directory_.push_back(e);
    by_hash_.emplace(hash, *id);

    stats_->images++;
    stats_->image_bytes += bytes;
    return true;
}

/// Ids for one texture: a single id, or `images.size()` consecutive ids
/// (frames or costumes) that alias the stored images through `owner`.
bool Baker::AssignIds(const std::vector<const uint8_t*>& images, int w, int h,
                      uint32_t* base, std::string* error)
{
    std::vector<uint32_t> owners(images.size());

    for (size_t k = 0; k < images.size(); k++)
    {
        if (!StoreImage(w, h, images[k], &owners[k], error))
        {
            return false;
        }
    }

    if (owners.size() == 1)
    {
        *base = owners[0];
        return true;
    }

    auto it = groups_.find(owners);

    if (it != groups_.end())
    {
        *base = it->second;
        return true;
    }

    *base = (uint32_t)directory_.size();

    for (uint32_t owner : owners)
    {
        TextureEntry e = directory_[owner];
        e.owner = owner;
        directory_.push_back(e);
    }

    groups_.emplace(owners, *base);
    return true;
}

// --- Rewriting ----------------------------------------------------------------

bool Baker::BakeFile(size_t fid, const std::vector<uint8_t>& data,
                     std::vector<uint8_t>* out, std::string* error)
{
    const std::string& name = source_.names[fid];
    FilePlan plan;

    if (!BuildPlan(fid, data, &plan, error))
    {
        if (error->find(name) == std::string::npos)
        {
            *error = name + ": " + *error;
        }

        return false;
    }

    if (plan.slots.empty() && plan.sites.empty())
    {
        out->clear();
        return true;
    }

    std::vector<DecodedContext> decoded;

    if (!Decode(plan, name, true, &decoded, error))
    {
        return false;
    }

    /// Resolve each distinct TEX0 of each context to an id.
    std::vector<std::vector<Resolved>> resolved(plan.contexts.size());

    for (size_t c = 0; c < plan.contexts.size(); c++)
    {
        const Context& ctx = plan.contexts[c];
        const DecodedContext& d = decoded[c];
        const size_t states = d.colour.size();

        resolved[c].resize(d.tex0s.size());

        for (size_t t = 0; t < d.tex0s.size(); t++)
        {
            Resolved& r = resolved[c][t];

            if (!d.valid[t])
            {
                continue;
            }

            const GS::Tex0 tex0 = GS::DecodeTex0(d.tex0s[t]);
            const int w = 1 << tex0.tw;
            const int h = 1 << tex0.th;

            bool all_equal = true;
            for (size_t s = 1; s < states; s++)
            {
                if (d.colour[s][t] != d.colour[0][t])
                {
                    all_equal = false;
                }
            }

            bool black_white = false;
            std::vector<BwMatch> bw(states, BwMatch::kSame);

            if (!d.mono.empty())
            {
                for (size_t s = 0; s < states; s++)
                {
                    bw[s] = CompareBlackWhite(d.colour[s][t], d.mono[s][t]);

                    if (bw[s] == BwMatch::kOther)
                    {
                        *error = name + " (" + ctx.label + "): the black & "
                                 "white image of TEX0 " + Hex(d.tex0s[t])
                                 + " is not gray(colour); the shader flag "
                                   "cannot reproduce it";
                        return false;
                    }

                    black_white |= bw[s] == BwMatch::kGray;
                }

                /// One flag covers every state: a state whose black & white
                /// image is its colour image must then already be gray.
                for (size_t s = 0; black_white && s < states; s++)
                {
                    if (bw[s] == BwMatch::kSame
                        && CompareBlackWhite(d.colour[s][t], d.colour[s][t])
                               != BwMatch::kSame)
                    {
                        *error = name + ": TEX0 " + Hex(d.tex0s[t])
                                 + " turns gray in black & white mode in "
                                   "one variant but not in another";
                        return false;
                    }
                }
            }

            std::vector<const uint8_t*> images;
            for (size_t s = 0; s < (all_equal ? 1 : states); s++)
            {
                images.push_back(d.colour[s][t].data());
            }

            if (!AssignIds(images, w, h, &r.id, error))
            {
                return false;
            }

            r.variants = (uint32_t)images.size();

            if (!all_equal)
            {
                if (ctx.selector == Selector::kFrames)
                {
                    r.info |= (uint32_t)states & kInfoFramesMask;
                }
                else
                {
                    r.info |= kInfoCostume;
                }
            }

            if (black_white)
            {
                r.info |= kInfoBlackWhite;
            }

            if (ctx.player)
            {
                r.info |= kInfoPlayerClut;
            }
        }
    }

    *out = data;

    for (const SlotRef& s : plan.slots)
    {
        Resolved r;

        if (s.context >= 0)
        {
            const DecodedContext& d = decoded[s.context];
            const size_t t = (size_t)(
                std::find(d.tex0s.begin(), d.tex0s.end(), s.tex0)
                - d.tex0s.begin());
            r = resolved[s.context][t];
        }
        else
        {
            Log(name + ": a mesh at " + Hex(s.slot)
                + " has no upload in its file; it binds nothing");
        }

        Wr32(&(*out)[s.slot], r.id);
        Wr32(&(*out)[s.slot + 4], r.info);
        Wr32(&(*out)[s.slot + 8], kSlotMarker);
        Wr32(&(*out)[s.slot + 12], 0);

        stats_->slots++;
        stats_->slots_no_texture += r.id == kNoTexture;
        stats_->slots_black_white += (r.info & kInfoBlackWhite) != 0;
        stats_->slots_animated += (r.info & kInfoFramesMask) != 0;
        stats_->slots_costume += (r.info & kInfoCostume) != 0;

        if (plan.player_model >= 0)
        {
            player_slots_[plan.player_model].push_back({r.id, r.info});
        }
    }

    if (plan.player_model >= 0)
    {
        player_baked_[plan.player_model] = true;
    }

    for (const Unit& u : plan.retired)
    {
        Wr32(&(*out)[u.off + 4], kUnitRetired);
        std::fill(out->begin() + (std::ptrdiff_t)(u.off + 8),
                  out->begin() + (std::ptrdiff_t)(u.off + u.len), 0);
    }

    stats_->sgds += plan.sgds;
    return WriteLists(plan, resolved, out, error);
}

/// Sorted ids a set of contexts can bind (all frames and costumes).
std::vector<uint32_t> ListIds(const std::vector<int>& contexts,
                              const std::vector<std::vector<Resolved>>& resolved)
{
    std::set<uint32_t> ids;

    for (int c : contexts)
    {
        for (const Resolved& r : resolved[c])
        {
            for (uint32_t k = 0; r.id != kNoTexture && k < r.variants; k++)
            {
                ids.insert(r.id + k);
            }
        }
    }

    return std::vector<uint32_t>(ids.begin(), ids.end());
}

bool PutList(std::vector<uint8_t>* out, size_t list, size_t end,
             const std::vector<uint32_t>& ids, uint64_t stamp,
             uint32_t costume, uint32_t flags, std::string* error)
{
    if ((list & 15) != 0
        || list + sizeof(TextureListHeader) + ids.size() * 4 > end)
    {
        *error = "a texture list does not fit in its container";
        return false;
    }

    uint8_t* p = &(*out)[list];
    std::memcpy(p, kPackMagic, 4);
    Wr32(p + 4, (uint32_t)ids.size());
    Wr64(p + 8, stamp);
    Wr32(p + 16, costume);
    Wr32(p + 20, flags);
    Wr32(p + 24, 0);
    Wr32(p + 28, 0);

    for (size_t i = 0; i < ids.size(); i++)
    {
        Wr32(p + sizeof(TextureListHeader) + i * 4, ids[i]);
    }

    return true;
}

bool Baker::WriteLists(const FilePlan& plan,
                       const std::vector<std::vector<Resolved>>& resolved,
                       std::vector<uint8_t>* out, std::string* error)
{
    for (const ListSite& site : plan.sites)
    {
        switch (site.kind)
        {
            case SiteKind::kTri2Unit:
                Wr32(&(*out)[site.at + 4], kUnitTextureList);
                std::fill(out->begin() + (std::ptrdiff_t)(site.at + 8),
                          out->begin() + (std::ptrdiff_t)site.end, 0);
                break;
            case SiteKind::kAnimationUnit:
                std::fill(out->begin() + (std::ptrdiff_t)site.list,
                          out->begin() + (std::ptrdiff_t)site.end, 0);
                break;
            case SiteKind::kTexturePak:
                Wr32(&(*out)[site.at], 0);
                std::fill(out->begin() + (std::ptrdiff_t)(site.at + 16),
                          out->begin() + (std::ptrdiff_t)site.end, 0);
                break;
        }

        if (!PutList(out, site.list, site.end,
                     ListIds(site.contexts, resolved), options_.build_stamp,
                     0, 0, error))
        {
            return false;
        }
    }

    return true;
}

bool Baker::BakePlayerCostume(int model, int costume, size_t fid,
                              std::vector<uint8_t>* out, std::string* error)
{
    if (!player_baked_[model])
    {
        *error = std::string("the player model ") + kPlayerModels[model].mpk
                 + " was not baked";
        return false;
    }

    if (!ReadEntry(fid, out, error))
    {
        return false;
    }

    std::set<uint32_t> ids;
    for (const auto& [id, info] : player_slots_[model])
    {
        if (id != kNoTexture)
        {
            ids.insert((info & kInfoCostume) ? id + (uint32_t)costume : id);
        }
    }

    Wr32(&(*out)[0], 0);
    std::fill(out->begin() + 16, out->end(), 0);
    return PutList(out, 16, out->size(),
                   std::vector<uint32_t>(ids.begin(), ids.end()),
                   options_.build_stamp, (uint32_t)costume, kListFlagPlayer,
                   error);
}

bool Baker::WriteEntry(size_t fid, const std::vector<uint8_t>& data,
                       std::string* error)
{
    const uint64_t offset = Align16(pos_);

    if (!sink_->write(offset, data.data(), data.size()))
    {
        *error = "cannot write the pack";
        return false;
    }

    pos_ = offset + data.size();

    EntryOverride o{};
    o.offset = offset;
    o.size = (uint32_t)data.size();
    overrides_[entries_[fid]] = o;
    rewritten_.push_back(fid);

    stats_->files_rewritten++;
    stats_->entry_bytes += data.size();
    return true;
}

// --- 2D textures ----------------------------------------------------------------

/// One 2D file: its pictures and the uploads SetSprFile() makes for them.
struct File2D
{
    std::vector<size_t> headers;
    std::vector<Tim2Picture> pictures;
    std::vector<Upload> uploads;
    std::vector<size_t> upload_picture;///< per upload
    /// The last upload that starts at a block: picture and whether it is
    /// the picture's CLUT.
    std::map<int, std::pair<size_t, bool>> starts;
};

/// Reads the pictures and lists their uploads in MakeTim2ClutDirect3()
/// order at offset 0: per picture its image at TBP0 (MakeTim2Direct3(),
/// skipped without image data), then its CLUT at CBP (MakeClutDirect3()).
/// False if a picture's data runs past the file.
bool ReadFile2D(const std::vector<uint8_t>& data,
                const std::vector<size_t>& headers, File2D* f)
{
    const uint8_t* end = data.data() + data.size();

    f->headers = headers;
    f->pictures.assign(headers.size(), {});

    for (size_t i = 0; i < headers.size(); i++)
    {
        Tim2Picture& p = f->pictures[i];

        if (!ReadTim2Picture(data.data() + headers[i],
                             data.size() - headers[i], &p))
        {
            return false;
        }

        const Tim2Tex0 t = SplitTex0(p.tex0);

        if (p.image_size != 0)
        {
            Upload u;
            u.dpsm = t.psm;
            u.dbp = t.tbp0;
            u.dbw = t.tbw;
            u.rrw = p.width & 0xFFF;
            u.rrh = p.height & 0xFFF;
            u.data = p.image;
            u.available = (size_t)(end - p.image);
            f->uploads.push_back(u);
            f->upload_picture.push_back(i);
            f->starts[u.dbp] = {i, false};
        }

        if (p.clut_size != 0)
        {
            Upload u;
            u.dpsm = PSMCT32;
            u.dbp = t.cbp;
            u.dbw = 1;
            u.rrw = p.clut_colors == 16 ? 8 : 16;
            u.rrh = p.clut_colors == 16 ? 2 : p.clut_colors / 16;
            u.data = p.clut;
            u.available = (size_t)(end - p.clut);
            f->uploads.push_back(u);
            f->upload_picture.push_back(i);
            f->starts[u.dbp] = {i, true};
        }
    }

    for (const Upload& u : f->uploads)
    {
        if (UploadBytes(u) > u.available)
        {
            return false;
        }
    }

    return true;
}

/// MikuPan_SetTexture() binds a texture for this TEX0 (else nothing).
bool Drawable(uint64_t tex0)
{
    const GS::Tex0 t = GS::DecodeTex0(tex0);
    int addr;
    int size;
    return GS::GetRawTextureRegion(t, &addr, &size) && GS::IsDecodableTexture(t);
}

/// Clears the texels of a w x h RGBA8 image outside its top-left
/// keep_w x keep_h.
void ZeroOutside(std::vector<uint8_t>* rgba, int w, int h, int keep_w,
                 int keep_h)
{
    for (int y = 0; y < h; y++)
    {
        const int from = y < keep_h ? std::min(std::max(keep_w, 0), w) : 0;
        std::fill(rgba->begin() + ((ptrdiff_t)y * w + from) * 4,
                  rgba->begin() + ((ptrdiff_t)y + 1) * w * 4, 0);
    }
}

/// True if two (1 << TW)-wide RGBA8 images agree inside the rectangles.
bool SameInRects(const std::vector<uint8_t>& a, const std::vector<uint8_t>& b,
                 const GS::Tex0& t, const DerivedTexture& d)
{
    if (a.size() != b.size())
    {
        return false;
    }

    const int w = 1 << t.tw;

    for (int r = 0; r < d.rect_count; r++)
    {
        const DerivedRect& rc = d.rects[r];

        for (int y = rc.v; y < rc.v + rc.h; y++)
        {
            const size_t at = ((size_t)y * w + rc.u) * 4;

            if (std::memcmp(&a[at], &b[at], (size_t)rc.w * 4) != 0)
            {
                return false;
            }
        }
    }

    return true;
}

/// The id a stamp names for the picture whose upload starts at `block`.
uint32_t OwnerAt(const File2D& f, const std::vector<uint32_t>& ids, int block)
{
    const auto it = f.starts.find(block);
    return it == f.starts.end() ? kNoOwner : ids[it->second.first];
}

bool Baker::StorePixels(const std::vector<uint8_t>& rgba, uint64_t* offset,
                        std::string* error)
{
    *offset = Align16(pos_);

    if (!sink_->write(*offset, rgba.data(), rgba.size()))
    {
        *error = "cannot write the pack";
        return false;
    }

    pos_ = *offset + rgba.size();
    stats_->image_bytes_2d += rgba.size();
    return true;
}

/// Every 2D file: a TIM2 file or a pk2 of TIM2 files outside bin/3ddata/.
bool Baker::Bake2D(std::string* error)
{
    const size_t count = entries_.size();
    std::set<std::pair<uint32_t, uint32_t>> seen;
    std::vector<uint8_t> data;
    std::vector<size_t> headers;

    for (size_t fid = 0; fid < count; fid++)
    {
        Progress("bake 2D", fid, count);

        if (Cancelled(error))
        {
            return false;
        }

        if (entries_[fid].second == 0
            || source_.names[fid].rfind(k3dPrefix, 0) == 0
            || !seen.insert(entries_[fid]).second)
        {
            continue;
        }

        if (!ReadEntry(fid, &data, error))
        {
            return false;
        }

        if (FindTim2Pictures(data.data(), data.size(), &headers)
            && !Bake2DFile(fid, data, headers, error))
        {
            return false;
        }
    }

    Progress("bake 2D", count, count);
    return true;
}

/// Gives each picture of a 2D file an id and works out what its TEX0 shows
/// once the whole file is uploaded: its own image and CLUT, the image of
/// the picture before it at the same TBP0 with its own CLUT (CLUT-only
/// pictures: the colour / monochrome pairs), both decoded by the game from
/// the pictures in memory, or else the GS image stored as pixels.
bool Baker::Bake2DFile(size_t fid, const std::vector<uint8_t>& data,
                       const std::vector<size_t>& headers, std::string* error)
{
    const std::string& name = source_.names[fid];
    File2D f;

    if (!ReadFile2D(data, headers, &f))
    {
        Log(name + ": a picture runs past the file; the game decodes it "
                   "unbaked");
        return true;
    }

    size_t ignored = 0;
    vram_.Reset();

    for (const Upload& u : f.uploads)
    {
        if (!vram_.Apply(u, &ignored, error))
        {
            *error = name + ": " + *error;
            return false;
        }
    }

    const size_t n = f.pictures.size();
    std::vector<uint32_t> ids(n, kNoTexture);
    uint32_t next = (uint32_t)tex2d_.size();

    for (size_t i = 0; i < n; i++)
    {
        /// The game decides with Tex0Drawable() alone.
        if (Drawable(f.pictures[i].tex0) != Tex0Drawable(f.pictures[i].tex0))
        {
            *error = name + ": Tex0Drawable() disagrees with the GS for TEX0 "
                     + Hex(f.pictures[i].tex0);
            return false;
        }

        if (Drawable(f.pictures[i].tex0))
        {
            ids[i] = next++;
        }
    }

    tex2d_.resize(next);
    derived_.resize(next);

    std::vector<uint8_t> gs;
    std::vector<uint8_t> linear;
    PatchRange range;
    range.first = (uint32_t)patches_.size();
    range.count = (uint32_t)n;

    for (size_t i = 0; i < n; i++)
    {
        const Tim2Picture& p = f.pictures[i];
        uint32_t owner = kNoOwner;
        int32_t base = 0;

        if (ids[i] != kNoTexture)
        {
            const GS::Tex0 t = GS::DecodeTex0(p.tex0);
            const int tw = 1 << t.tw;
            const int th = 1 << t.th;

            gs.assign((size_t)tw * th * 4, 0);
            vram_.Decode(t, gs.data());
            ZeroOutside(&gs, tw, th, p.width, p.height);

            Tex2DEntry& e = tex2d_[ids[i]];
            e = Tex2DEntry{};
            e.tex0 = p.tex0;
            e.width = (uint16_t)tw;
            e.height = (uint16_t)th;

            bool from_memory = false;
            const auto at = f.starts.find(t.tbp0);

            if (at != f.starts.end())
            {
                const size_t o = at->second.first;
                owner = ids[o];
                linear.resize(gs.size());

                if (!at->second.second
                    && DecodeTim2Texture(f.pictures[o], p, p.tex0,
                                         linear.data())
                    && linear == gs)
                {
                    from_memory = true;
                    base = (int32_t)((int64_t)headers[o] - (int64_t)headers[i]);
                }
            }

            if (from_memory)
            {
                stats_->pictures_2d_base += base != 0;
            }
            else
            {
                const size_t uncovered = vram_.Uncovered(
                    t, 0, 0, std::min(tw, p.width), std::min(th, p.height));

                if (!StorePixels(gs, &e.offset, error))
                {
                    return false;
                }

                stats_->pictures_2d_baked++;
                Log(name + ": picture " + std::to_string(i) + " (TEX0 "
                    + Hex(p.tex0) + ") shows more than one picture; stored "
                    "as pixels"
                    + (uncovered != 0
                           ? ", " + std::to_string(uncovered)
                                 + " texels written by no upload of the file"
                           : ""));
            }

            stats_->pictures_2d++;
        }

        PatchRecord r{};
        r.offset = (uint32_t)(headers[i] + kTim2StampOffset);
        std::memcpy(r.before, &data[r.offset], sizeof(r.before));
        Wr32(r.after, ids[i]);
        Wr32(r.after + 4, owner);
        Wr32(r.after + 8, kTim2StampMarker);
        Wr32(r.after + 12, (uint32_t)base);
        patches_.push_back(r);
    }

    if (!BakeDerived(name, f, ids, error))
    {
        return false;
    }

    patch_ranges_[entries_[fid]] = range;
    files_2d_.push_back(fid);
    stats_->files_2d++;
    return true;
}

/// The derived textures (kDerivedTextures) this file holds: every texel and
/// CLUT entry the used rectangles read is written by the file and no
/// picture of it draws by that TEX0. The texture is attached to the
/// picture whose upload last changes it. Expects vram_ to hold the whole
/// file, and leaves it so.
bool Baker::BakeDerived(const std::string& name, const File2D& f,
                        const std::vector<uint32_t>& ids, std::string* error)
{
    for (const DerivedTexture& d : kDerivedTextures)
    {
        const GS::Tex0 t = GS::DecodeTex0(d.tex0);
        bool covered = Drawable(d.tex0);

        for (int r = 0; covered && r < d.rect_count; r++)
        {
            const DerivedRect& rc = d.rects[r];
            covered = vram_.Uncovered(t, rc.u, rc.v, rc.w, rc.h) == 0;
        }

        for (const Tim2Picture& p : f.pictures)
        {
            covered = covered && p.tex0 != d.tex0;
        }

        if (!covered)
        {
            continue;
        }

        std::vector<uint8_t> full((size_t)4 << (t.tw + t.th));
        vram_.Decode(t, full.data());

        /// Replay the file picture by picture: the last picture whose
        /// uploads change the rectangles completes the texture.
        std::vector<uint8_t> previous;
        std::vector<uint8_t> current(full.size());
        size_t completing = 0;
        size_t ignored = 0;
        size_t k = 0;
        vram_.Reset();

        for (size_t i = 0; i < f.pictures.size(); i++)
        {
            for (; k < f.uploads.size() && f.upload_picture[k] == i; k++)
            {
                if (!vram_.Apply(f.uploads[k], &ignored, error))
                {
                    *error = name + ": " + *error;
                    return false;
                }
            }

            vram_.Decode(t, current.data());

            if (!SameInRects(current, previous, t, d))
            {
                completing = i;
                previous = current;
            }
        }

        if (ids[completing] == kNoTexture)
        {
            Log(name + ": " + d.label + " is completed by a picture the "
                "game cannot draw; skipped");
            continue;
        }

        const Tim2Tex0 ct = SplitTex0(f.pictures[completing].tex0);
        Tex2DDerived rec{};
        rec.tex0 = d.tex0;
        rec.check_block = (uint32_t)(f.pictures[completing].image_size != 0
                                         ? ct.tbp0
                                         : ct.cbp);
        rec.check_owner = OwnerAt(f, ids, (int)rec.check_block);
        rec.id = (uint32_t)tex2d_.size();

        Tex2DEntry e{};
        e.tex0 = d.tex0;
        e.width = (uint16_t)(1 << t.tw);
        e.height = (uint16_t)(1 << t.th);

        if (!StorePixels(full, &e.offset, error))
        {
            return false;
        }

        tex2d_.push_back(e);
        derived_.emplace_back();
        derived_[ids[completing]].push_back(rec);
        stats_->derived_2d++;
        Log(name + ": " + d.label + " (TEX0 " + Hex(d.tex0) + ") baked");
    }

    return true;
}

// --- Verification -------------------------------------------------------------

bool Baker::VerifyFile(size_t fid, std::string* error)
{
    const std::string& name = source_.names[fid];
    const EntryOverride& o = overrides_[entries_[fid]];
    std::vector<uint8_t> original;
    std::vector<uint8_t> baked(o.size);

    if (!ReadEntry(fid, &original, error)
        || !sink_->read(o.offset, baked.data(), baked.size()))
    {
        *error = name + ": cannot read back the rewritten entry";
        return false;
    }

    if (baked.size() != original.size())
    {
        *error = name + ": the rewritten entry changed size";
        return false;
    }

    /// Bytes the bake may change.
    std::vector<uint8_t> allowed(original.size(), 0);
    auto allow = [&allowed](size_t from, size_t to) {
        std::fill(allowed.begin() + (std::ptrdiff_t)from, allowed.begin() + (std::ptrdiff_t)to, 1);
    };

    for (int m = 0; m < kPlayerModelCount; m++)
    {
        for (int k = 0; k < kPlayerModels[m].count; k++)
        {
            if (name != kPlayerModels[m].costumes[k].pk2)
            {
                continue;
            }

            /// A costume list holds every id its costume can bind.
            allow(0, original.size());

            if (Rd32(&baked[0]) != 0
                || std::memcmp(&baked[16], kPackMagic, 4) != 0
                || Rd64(&baked[24]) != options_.build_stamp
                || Rd32(&baked[32]) != (uint32_t)k)
            {
                *error = name + ": bad costume list";
                return false;
            }

            const uint32_t count = Rd32(&baked[20]);
            std::set<uint32_t> listed;
            for (uint32_t i = 0; i < count; i++)
            {
                listed.insert(Rd32(&baked[48 + i * 4]));
            }

            for (const auto& [id, info] : player_slots_[m])
            {
                const uint32_t want =
                    (info & kInfoCostume) ? id + (uint32_t)k : id;

                if (id != kNoTexture && listed.count(want) == 0)
                {
                    *error = name + ": costume list misses id "
                             + std::to_string(want);
                    return false;
                }
            }

            return true;
        }
    }

    FilePlan plan;
    std::vector<DecodedContext> decoded;

    if (!BuildPlan(fid, original, &plan, error)
        || !Decode(plan, name, false, &decoded, error))
    {
        return false;
    }

    for (const Unit& u : plan.retired)
    {
        allow(u.off + 4, u.off + u.len);
    }

    for (const ListSite& site : plan.sites)
    {
        allow(site.kind == SiteKind::kTri2Unit ? site.at + 4
              : site.kind == SiteKind::kTexturePak ? site.at
                                                   : site.list,
              site.end);
    }

    /// Ids each context's list holds.
    std::vector<std::set<uint32_t>> listed(plan.contexts.size());

    for (const ListSite& site : plan.sites)
    {
        if (std::memcmp(&baked[site.list], kPackMagic, 4) != 0
            || Rd64(&baked[site.list + 8]) != options_.build_stamp)
        {
            *error = name + ": bad texture list at " + Hex(site.list);
            return false;
        }

        const uint32_t count = Rd32(&baked[site.list + 4]);
        for (int c : site.contexts)
        {
            for (uint32_t i = 0; i < count; i++)
            {
                listed[c].insert(Rd32(&baked[site.list + 32 + i * 4]));
            }
        }
    }

    /// Images read back from the pack, by owner id (many meshes of a file
    /// share a texture).
    std::map<uint32_t, std::vector<uint8_t>> images;

    for (const SlotRef& s : plan.slots)
    {
        allow(s.slot, s.slot + 16);

        const uint32_t id = Rd32(&baked[s.slot]);
        const uint32_t info = Rd32(&baked[s.slot + 4]);

        if (Rd32(&baked[s.slot + 8]) != kSlotMarker
            || Rd32(&baked[s.slot + 12]) != 0)
        {
            *error = name + ": slot " + Hex(s.slot) + " is not baked";
            return false;
        }

        if (s.context < 0)
        {
            if (id != kNoTexture)
            {
                *error = name + ": a mesh without texture source has an id";
                return false;
            }

            continue;
        }

        const Context& ctx = plan.contexts[s.context];
        const DecodedContext& d = decoded[s.context];
        const size_t t = (size_t)(std::find(d.tex0s.begin(), d.tex0s.end(),
                                            s.tex0)
                                  - d.tex0s.begin());

        if (!d.valid[t])
        {
            if (id != kNoTexture)
            {
                *error = name + ": an undecodable TEX0 got an id";
                return false;
            }

            continue;
        }

        const uint32_t frames = info & kInfoFramesMask;
        const size_t states = d.colour.size();

        if ((frames != 0 && (ctx.selector != Selector::kFrames
                             || frames != states))
            || ((info & kInfoCostume) && ctx.selector != Selector::kCostume)
            || (((info & kInfoPlayerClut) != 0) != ctx.player))
        {
            *error = name + ": slot " + Hex(s.slot) + " has wrong info bits";
            return false;
        }

        for (size_t st = 0; st < states; st++)
        {
            /// The runtime bind rule.
            uint32_t want = id;
            if (info & kInfoCostume)
            {
                want += (uint32_t)st;
            }
            if (frames > 1)
            {
                want += (uint32_t)std::min<size_t>(st, frames - 1);
            }

            if (want >= directory_.size()
                || (ctx.player ? false : listed[s.context].count(want) == 0))
            {
                *error = name + ": slot " + Hex(s.slot) + " id "
                         + std::to_string(want) + " is not listed";
                return false;
            }

            const uint32_t owner = directory_[want].owner;
            auto cached = images.find(owner);

            if (directory_[owner].owner != owner
                || directory_[owner].offset != directory_[want].offset
                || (cached == images.end()
                    && !ReadImage(owner, &images[owner], error)))
            {
                *error = name + ": bad directory entry " + std::to_string(want);
                return false;
            }

            const std::vector<uint8_t>& image = images[owner];

            if (image != d.colour[st][t])
            {
                *error = name + ": slot " + Hex(s.slot) + " state "
                         + std::to_string(st) + " shows the wrong image";
                return false;
            }

            if (!d.mono.empty())
            {
                const BwMatch m = CompareBlackWhite(image, d.mono[st][t]);
                const bool flag = (info & kInfoBlackWhite) != 0;

                if (m == BwMatch::kOther || (!flag && m != BwMatch::kSame)
                    || (flag && m == BwMatch::kSame
                        && CompareBlackWhite(image, image) != BwMatch::kSame))
                {
                    *error = name + ": slot " + Hex(s.slot)
                             + " black & white mismatch";
                    return false;
                }
            }
            else if (info & kInfoBlackWhite)
            {
                *error = name + ": black & white flag without a BW CLUT";
                return false;
            }
        }

        stats_->verified_slots++;
    }

    for (size_t i = 0; i < original.size(); i++)
    {
        if (!allowed[i] && original[i] != baked[i])
        {
            *error = name + ": byte " + Hex(i) + " outside the texture data "
                                                 "changed";
            return false;
        }
    }

    return true;
}

/// Loads a 2D file the way the game does (the entry with its stamps
/// applied) and checks every picture and derived texture: what the game
/// shows from the stamp - decoded from the pictures, or the stored pixels -
/// against a fresh decode of the whole file uploaded to GS memory, and the
/// owners the game compares at draw time.
bool Baker::Verify2DFile(size_t fid, std::string* error)
{
    const std::string& name = source_.names[fid];
    const PatchRange& range = patch_ranges_.at(entries_[fid]);
    std::vector<uint8_t> data;

    if (!ReadEntry(fid, &data, error))
    {
        return false;
    }

    for (uint32_t k = 0; k < range.count; k++)
    {
        const PatchRecord& r = patches_[range.first + k];

        if (r.offset + sizeof(r.after) > data.size()
            || std::memcmp(&data[r.offset], r.before, sizeof(r.before)) != 0)
        {
            *error = name + ": a stamp does not fit its picture";
            return false;
        }

        std::memcpy(&data[r.offset], r.after, sizeof(r.after));
    }

    std::vector<size_t> headers;
    File2D f;

    if (!FindTim2Pictures(data.data(), data.size(), &headers)
        || headers.size() != range.count || !ReadFile2D(data, headers, &f))
    {
        *error = name + ": the stamped file does not read back";
        return false;
    }

    const size_t n = headers.size();
    std::vector<uint32_t> ids(n);
    size_t ignored = 0;
    vram_.Reset();

    for (const Upload& u : f.uploads)
    {
        if (!vram_.Apply(u, &ignored, error))
        {
            *error = name + ": " + *error;
            return false;
        }
    }

    for (size_t i = 0; i < n; i++)
    {
        ids[i] = Rd32(&data[headers[i] + kTim2StampOffset]);
    }

    std::vector<uint8_t> gs;
    std::vector<uint8_t> shown;

    for (size_t i = 0; i < n; i++)
    {
        const Tim2Picture& p = f.pictures[i];
        const uint8_t* stamp = &data[headers[i] + kTim2StampOffset];
        const uint32_t owner = Rd32(stamp + 4);
        const int32_t base = (int32_t)Rd32(stamp + 12);
        const std::string where = name + ": picture " + std::to_string(i);

        if (Rd32(stamp + 8) != kTim2StampMarker
            || (ids[i] == kNoTexture) != !Drawable(p.tex0)
            || (ids[i] != kNoTexture && ids[i] >= tex2d_.size()))
        {
            *error = where + ": bad stamp";
            return false;
        }

        if (ids[i] == kNoTexture)
        {
            continue;
        }

        const GS::Tex0 t = GS::DecodeTex0(p.tex0);
        const Tex2DEntry& e = tex2d_[ids[i]];

        if (e.width != (1 << t.tw) || e.height != (1 << t.th)
            || e.tex0 != p.tex0)
        {
            *error = where + ": bad texture entry";
            return false;
        }

        gs.assign((size_t)e.width * e.height * 4, 0);
        vram_.Decode(t, gs.data());
        ZeroOutside(&gs, e.width, e.height, p.width, p.height);
        shown.assign(gs.size(), 0);

        if (e.offset == 0)
        {
            const auto source = std::find(headers.begin(), headers.end(),
                                          (size_t)((int64_t)headers[i] + base));

            if (source == headers.end()
                || !DecodeTim2Texture(f.pictures[source - headers.begin()], p,
                                      p.tex0, shown.data()))
            {
                *error = where + ": its texture does not decode";
                return false;
            }
        }
        else if (!sink_->read(e.offset, shown.data(), shown.size()))
        {
            *error = "cannot read back the pack";
            return false;
        }

        if (shown != gs)
        {
            *error = where + " (TEX0 " + Hex(p.tex0) + ") shows other pixels "
                     "than the GS";
            return false;
        }

        if (owner != OwnerAt(f, ids, t.tbp0))
        {
            *error = where + ": bad owner";
            return false;
        }

        for (const Tex2DDerived& rec : derived_[ids[i]])
        {
            const GS::Tex0 dt = GS::DecodeTex0(rec.tex0);
            const Tex2DEntry& de = tex2d_[rec.id];

            gs.assign((size_t)de.width * de.height * 4, 0);
            shown.assign(gs.size(), 0);
            vram_.Decode(dt, gs.data());

            if (de.offset == 0
                || !sink_->read(de.offset, shown.data(), shown.size())
                || shown != gs
                || rec.check_owner != OwnerAt(f, ids, (int)rec.check_block))
            {
                *error = where + ": derived texture " + Hex(rec.tex0)
                         + " does not match";
                return false;
            }

            stats_->verified_2d++;
        }

        stats_->verified_2d++;
    }

    return true;
}

bool Baker::Verify(std::string* error)
{
    const size_t total = rewritten_.size() + files_2d_.size();

    for (size_t i = 0; i < rewritten_.size(); i++)
    {
        Progress("verify", i, total);

        if (Cancelled(error) || !VerifyFile(rewritten_[i], error))
        {
            return false;
        }
    }

    for (size_t i = 0; i < files_2d_.size(); i++)
    {
        Progress("verify", rewritten_.size() + i, total);

        if (Cancelled(error) || !Verify2DFile(files_2d_[i], error))
        {
            return false;
        }
    }

    Progress("verify", total, total);
    return true;
}

// --- Pack -----------------------------------------------------------------------

bool Baker::WritePack(std::string* error)
{
    std::vector<uint8_t> buf;

    const uint64_t texture_directory = Align16(pos_);
    buf.resize(directory_.size() * sizeof(TextureEntry));
    for (size_t i = 0; i < directory_.size(); i++)
    {
        uint8_t* p = &buf[i * sizeof(TextureEntry)];
        Wr64(p, directory_[i].offset);
        Wr16(p + 8, directory_[i].width);
        Wr16(p + 10, directory_[i].height);
        Wr32(p + 12, directory_[i].owner);
    }

    if (!buf.empty() && !sink_->write(texture_directory, buf.data(), buf.size()))
    {
        *error = "cannot write the pack";
        return false;
    }

    const uint64_t entry_directory = Align16(texture_directory + buf.size());
    buf.assign(entries_.size() * sizeof(EntryOverride), 0);
    for (size_t i = 0; i < entries_.size(); i++)
    {
        auto it = overrides_.find(entries_[i]);

        if (entries_[i].second != 0 && it != overrides_.end())
        {
            uint8_t* p = &buf[i * sizeof(EntryOverride)];
            Wr64(p, it->second.offset);
            Wr32(p + 8, it->second.size);
        }
    }

    if (!buf.empty() && !sink_->write(entry_directory, buf.data(), buf.size()))
    {
        *error = "cannot write the pack";
        return false;
    }

    /// Probes: the middle of every source entry the pack replaces.
    const uint64_t probes = Align16(entry_directory + buf.size());
    buf.assign(entries_.size() * sizeof(EntryProbe), 0);
    for (size_t i = 0; i < entries_.size(); i++)
    {
        const uint32_t size = entries_[i].second;

        if (size == 0 || overrides_.count(entries_[i]) == 0)
        {
            continue;
        }

        const uint64_t at =
            (uint64_t)entries_[i].first * kSectorSize + ProbeOffset(size);

        if (!source_.read_bd(at, &buf[i * sizeof(EntryProbe)],
                             std::min<size_t>(size, sizeof(EntryProbe))))
        {
            *error = "cannot read IMG_BD.BIN";
            return false;
        }
    }

    if (!buf.empty() && !sink_->write(probes, buf.data(), buf.size()))
    {
        *error = "cannot write the pack";
        return false;
    }

    /// 2D textures: the derived textures of each picture follow one another.
    const uint64_t tex2d_directory = Align16(probes + buf.size());
    std::vector<Tex2DDerived> derived;
    buf.assign(tex2d_.size() * sizeof(Tex2DEntry), 0);
    for (size_t i = 0; i < tex2d_.size(); i++)
    {
        uint8_t* p = &buf[i * sizeof(Tex2DEntry)];
        Wr64(p, tex2d_[i].offset);
        Wr64(p + 8, tex2d_[i].tex0);
        Wr16(p + 16, tex2d_[i].width);
        Wr16(p + 18, tex2d_[i].height);
        Wr32(p + 20, (uint32_t)derived.size());
        Wr32(p + 24, (uint32_t)derived_[i].size());
        derived.insert(derived.end(), derived_[i].begin(), derived_[i].end());
    }

    if (!buf.empty() && !sink_->write(tex2d_directory, buf.data(), buf.size()))
    {
        *error = "cannot write the pack";
        return false;
    }

    const uint64_t derived_offset = Align16(tex2d_directory + buf.size());
    buf.assign(derived.size() * sizeof(Tex2DDerived), 0);
    for (size_t i = 0; i < derived.size(); i++)
    {
        uint8_t* p = &buf[i * sizeof(Tex2DDerived)];
        Wr64(p, derived[i].tex0);
        Wr32(p + 8, derived[i].id);
        Wr32(p + 12, derived[i].check_block);
        Wr32(p + 16, derived[i].check_owner);
    }

    if (!buf.empty() && !sink_->write(derived_offset, buf.data(), buf.size()))
    {
        *error = "cannot write the pack";
        return false;
    }

    const uint64_t patch_offset = Align16(derived_offset + buf.size());
    buf.assign(patches_.size() * sizeof(PatchRecord), 0);
    for (size_t i = 0; i < patches_.size(); i++)
    {
        uint8_t* p = &buf[i * sizeof(PatchRecord)];
        Wr32(p, patches_[i].offset);
        std::memcpy(p + 8, patches_[i].before, sizeof(patches_[i].before));
        std::memcpy(p + 24, patches_[i].after, sizeof(patches_[i].after));
    }

    if (!buf.empty() && !sink_->write(patch_offset, buf.data(), buf.size()))
    {
        *error = "cannot write the pack";
        return false;
    }

    /// Entries that share data share their stamps.
    const uint64_t patch_ranges = Align16(patch_offset + buf.size());
    buf.assign(entries_.size() * sizeof(PatchRange), 0);
    for (size_t i = 0; i < entries_.size(); i++)
    {
        auto it = patch_ranges_.find(entries_[i]);

        if (entries_[i].second != 0 && it != patch_ranges_.end())
        {
            uint8_t* p = &buf[i * sizeof(PatchRange)];
            Wr32(p, it->second.first);
            Wr32(p + 4, it->second.count);
        }
    }

    if (!buf.empty() && !sink_->write(patch_ranges, buf.data(), buf.size()))
    {
        *error = "cannot write the pack";
        return false;
    }

    const uint64_t hd_offset = Align16(patch_ranges + buf.size());
    if (!sink_->write(hd_offset, source_.hd.data(), source_.hd.size()))
    {
        *error = "cannot write the pack";
        return false;
    }

    const uint64_t pack_size = hd_offset + source_.hd.size();

    uint8_t header[sizeof(PackHeader)] = {};
    std::memcpy(header, kPackMagic, 4);
    Wr32(header + 0x04, kPackVersion);
    Wr64(header + 0x08, options_.build_stamp);
    Wr32(header + 0x10, (uint32_t)directory_.size());
    Wr32(header + 0x14, (uint32_t)entries_.size());
    Wr64(header + 0x18, texture_directory);
    Wr64(header + 0x20, entry_directory);
    Wr64(header + 0x28, hd_offset);
    Wr32(header + 0x30, (uint32_t)source_.hd.size());
    Wr32(header + 0x34, 0);
    Wr64(header + 0x38, source_.bd_size);
    Wr64(header + 0x40, pack_size);
    Wr64(header + 0x48, probes);
    Wr64(header + 0x50, tex2d_directory);
    Wr32(header + 0x58, (uint32_t)tex2d_.size());
    Wr32(header + 0x5C, (uint32_t)derived.size());
    Wr64(header + 0x60, derived_offset);
    Wr64(header + 0x68, patch_offset);
    Wr64(header + 0x70, patch_ranges);
    Wr32(header + 0x78, (uint32_t)patches_.size());

    /// The header goes last: an interrupted bake leaves no valid pack.
    if (!sink_->write(0, header, sizeof(header)))
    {
        *error = "cannot write the pack";
        return false;
    }

    stats_->textures = directory_.size();
    stats_->pack_size = pack_size;
    return true;
}

bool Baker::Run(std::string* error)
{
    const size_t count = source_.hd.size() / 8;

    if (count == 0 || source_.names.size() != count)
    {
        *error = "the file name table does not match IMG_HD.BIN";
        return false;
    }

    entries_.resize(count);
    for (size_t i = 0; i < count; i++)
    {
        entries_[i] = {Rd32(&source_.hd[i * 8]), Rd32(&source_.hd[i * 8 + 4])};
    }

    /// Player costume files are rewritten after their .mpk (pass 2).
    std::map<size_t, std::pair<int, int>> costume_files;

    for (int m = 0; m < kPlayerModelCount; m++)
    {
        if (FindName(kPlayerModels[m].mpk) < 0)
        {
            continue;
        }

        for (int k = 0; k < kPlayerModels[m].count; k++)
        {
            const int fid = FindName(kPlayerModels[m].costumes[k].pk2);

            if (fid < 0)
            {
                *error = std::string("missing player file ")
                         + kPlayerModels[m].costumes[k].pk2;
                return false;
            }

            costume_files[(size_t)fid] = {m, k};
        }
    }

    std::set<std::pair<uint32_t, uint32_t>> seen;
    std::vector<uint8_t> data;
    std::vector<uint8_t> baked;

    for (size_t fid = 0; fid < count; fid++)
    {
        Progress("bake", fid, count);

        if (Cancelled(error))
        {
            return false;
        }

        const std::string& name = source_.names[fid];

        if (entries_[fid].second == 0 || name.rfind(k3dPrefix, 0) != 0
            || costume_files.count(fid) != 0
            || !seen.insert(entries_[fid]).second)
        {
            continue;
        }

        if (!ReadEntry(fid, &data, error) || !BakeFile(fid, data, &baked, error))
        {
            return false;
        }

        if (!baked.empty() && !WriteEntry(fid, baked, error))
        {
            return false;
        }
    }

    for (const auto& [fid, which] : costume_files)
    {
        if (!seen.insert(entries_[fid]).second)
        {
            continue;
        }

        if (!BakePlayerCostume(which.first, which.second, fid, &baked, error)
            || !WriteEntry(fid, baked, error))
        {
            return false;
        }
    }

    Progress("bake", count, count);

    if (!Bake2D(error))
    {
        return false;
    }

    if (options_.verify && !Verify(error))
    {
        return false;
    }

    return WritePack(error);
}

}// namespace

bool BakeTextures(const Source& source, const Options& options, Sink* sink,
                  Stats* stats, std::string* error)
{
    Stats local;
    Baker baker(source, options, sink, stats != nullptr ? stats : &local);
    return baker.Run(error);
}

}// namespace MikuPanBake
