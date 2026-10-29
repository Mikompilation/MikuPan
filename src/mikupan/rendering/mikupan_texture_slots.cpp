#include "mikupan_texture_slots.h"

#include "mikupan/bake/mikupan_bake_format.h"
#include "mikupan/bake/mikupan_tim2.h"
#include "mikupan_baked_texture.h"
#include "mikupan_gpu.h"
#include "SDL3/SDL_timer.h"
#include "spdlog/spdlog.h"

#include <algorithm>
#include <cstring>
#include <memory>
#include <vector>

namespace
{

using namespace MikuPanBake;

constexpr int kBlocks = 1 << 14;
constexpr int kBlockMask = kBlocks - 1;

/// g_owner of a block no upload started at yet.
constexpr uint32_t kNothing = 0xFFFFFFFEu;
/// Owners of runtime uploads and of pictures without a stamp have this bit;
/// 2D ids never do.
constexpr uint32_t kRuntimeOwner = 0x80000000u;
/// The game trusts the sizes in a picture header it uploads; so do we.
constexpr size_t kTrusted = ~(size_t)0 >> 1;

constexpr int kPsmCt32 = 0x00;
constexpr int kPsmCt16 = 0x02;
constexpr int kPsmCt16s = 0x0A;
constexpr int kPsmT8 = 0x13;
constexpr int kPsmT4 = 0x14;
constexpr int kPsmZ32 = 0x30;
constexpr int kPsmZ16 = 0x32;
constexpr int kPsmZ16s = 0x3A;

bool Indexed(int psm)
{
    return psm == kPsmT8 || psm == kPsmT4;
}

/// The format a runtime transfer is kept in, as GS::GSHelper::Upload()
/// writes it: PSMZ32 as PSMCT32, every 16-bit format as PSMCT16. -1: a
/// format the game does not transfer at run time.
int StoredPsm(int psm)
{
    switch (psm)
    {
        case kPsmCt32:
        case kPsmZ32:
            return kPsmCt32;
        case kPsmCt16:
        case kPsmCt16s:
        case kPsmZ16:
        case kPsmZ16s:
            return kPsmCt16;
        case kPsmT8:
            return kPsmT8;
        default:
            return -1;
    }
}

int BytesPerPixel(int stored_psm)
{
    return stored_psm == kPsmCt32 ? 4 : stored_psm == kPsmCt16 ? 2 : 1;
}

/// The TEX0 fields a texture is drawn by: TBP0, TBW, PSM, TW and TH, plus
/// CBP for indexed formats and CSA for PSMT4 (what the decode reads).
uint64_t Key(uint64_t tex0)
{
    const int psm = (int)((tex0 >> 20) & 0x3F);
    uint64_t mask = 0x3FFFFFFFFull;

    if (Indexed(psm))
    {
        mask |= 0x3FFFull << 37;
    }

    if (psm == kPsmT4)
    {
        mask |= 0x1Full << 56;
    }

    return tex0 & mask;
}

/// A TEX0 moved by `offset` blocks, the way the game adds an offset to TBP0
/// and CBP when it uploads a picture elsewhere.
uint64_t Placed(uint64_t tex0, int offset)
{
    const uint64_t tbp = ((tex0 & 0x3FFF) + (uint64_t)offset) & 0x3FFF;
    const uint64_t cbp = (((tex0 >> 37) & 0x3FFF) + (uint64_t)offset) & 0x3FFF;
    return (tex0 & ~(0x3FFFull | (0x3FFFull << 37))) | tbp | (cbp << 37);
}

enum class Kind : uint8_t
{
    kNone,
    kPicture,  ///< a stamped TIM2 picture
    kDerived,  ///< a Tex2DDerived texture
    kUnstamped,///< a TIM2 picture without a stamp, decoded at upload
};

/// The texture registered for the TEX0s keyed by one block.
struct Slot
{
    uint64_t key = 0;
    uint32_t id = 0;                ///< kPicture, kDerived: 2D id
    uint32_t check_block = 0;       ///< it shows while g_owner of this
    uint32_t check_owner = kNoOwner;///< block is this (kNoOwner: always)
    Kind kind = Kind::kNone;
};

/// What a stamped picture's texture is decoded from, copied when the picture
/// is uploaded: the game reuses the memory of some files once they are
/// uploaded (the effect pack's becomes a screen copy buffer), as the PS2
/// kept the texels in VRAM.
struct Source
{
    std::vector<uint8_t> image;///< the picture whose texels it shows
    std::vector<uint8_t> clut; ///< its own picture, when that is another
};

/// A TIM2 picture the bake did not stamp (photos from save data, anything
/// that replaced a disc file): decoded when uploaded, as it may not stay in
/// memory.
struct Unstamped
{
    std::vector<uint8_t> source;///< the picture as last uploaded
    std::vector<uint8_t> pixels;
    int width = 0;
    int height = 0;
    unsigned int texture = 0;
    int texture_w = 0;
    int texture_h = 0;
    bool dirty = false;
};

/// The image runtime transfers (sceGsExecLoadImage()) built in a buffer
/// that starts at one block, row by row in its stored format.
struct RawImage
{
    uint32_t owner = 0;
    int psm = 0;///< StoredPsm()
    int dbw = 0;
    int width = 0;
    int height = 0;
    std::vector<uint8_t> data;
    uint32_t generation = 0;

    /// The texture of the TEX0 last drawn from it.
    unsigned int texture = 0;
    int texture_w = 0;
    int texture_h = 0;
    uint32_t texture_generation = 0;
    const RawImage* texture_clut = nullptr;
    uint32_t texture_clut_generation = 0;
};

struct FrameStats
{
    int picture_uploads = 0;
    int raw_uploads = 0;
    int raw_upload_bytes = 0;
    int textures_created = 0;
    double create_ms = 0.0;
    int misses = 0;
};

uint32_t g_owner[kBlocks];
Slot g_by_clut[kBlocks];
Slot g_by_texel[kBlocks];
std::unique_ptr<Unstamped> g_unstamped_clut[kBlocks];
std::unique_ptr<Unstamped> g_unstamped_texel[kBlocks];
std::unique_ptr<RawImage> g_raw[kBlocks];
bool g_initialized = false;

/// Per 2D id: the GPU texture (0 until first drawn) and its source.
std::vector<unsigned int> g_textures;
std::vector<Source> g_sources;
uint32_t g_next_runtime_owner = 0;
std::vector<uint8_t> g_pixels;

FrameStats g_frame;
FrameStats g_last_frame;

uint64_t g_reported_misses[32];
int g_reported_miss_count = 0;
bool g_reported_raw_format = false;

void Init()
{
    if (g_initialized)
    {
        return;
    }

    std::fill(std::begin(g_owner), std::end(g_owner), kNothing);
    g_initialized = true;
}

uint32_t NewRuntimeOwner()
{
    uint32_t owner;

    do
    {
        owner = kRuntimeOwner | (g_next_runtime_owner++ & ~kRuntimeOwner);
    } while (owner == kNothing || owner == kNoOwner);

    return owner;
}

bool ReadStamp(const uint8_t* header, Tim2Stamp* out)
{
    std::memcpy(out, header + kTim2StampOffset, sizeof(*out));
    return out->marker == kTim2StampMarker;
}

/// A stamp of the loaded pack on the picture it was baked for, naming a
/// texture the game can draw: its Tex2DEntry, or nullptr.
const Tex2DEntry* Stamped(const uint8_t* header, const Tim2Picture& p,
                          Tim2Stamp* out)
{
    if (!ReadStamp(header, out))
    {
        return nullptr;
    }

    const Tex2DEntry* e = MikuPanBakedTexture2D::Entry(out->id);
    const Tim2Tex0 t = SplitTex0(p.tex0);

    /// The game rewrites PSM, CPSM, CSM, CSA and CLD of GsTex0 in memory,
    /// never the size.
    return e != nullptr && e->width == (1 << t.tw) && e->height == (1 << t.th)
               ? e
               : nullptr;
}

void SizeTables()
{
    const uint32_t count = MikuPanBakedTexture2D::Count();

    if (g_textures.size() != count)
    {
        g_textures.resize(count, 0);
        g_sources.resize(count);
    }
}

/// Bytes of a picture from its header to the end of its CLUT.
std::vector<uint8_t> CopyPicture(const uint8_t* header, const Tim2Picture& p)
{
    return std::vector<uint8_t>(header, p.clut + p.clut_size);
}

/// Keeps what a stamped picture's texture is decoded from, once.
void KeepSource(const uint8_t* header, const Tim2Picture& p,
                const Tim2Stamp& stamp, const Tex2DEntry& e)
{
    SizeTables();

    Source& source = g_sources[stamp.id];

    if (e.offset != 0 || !source.image.empty())
    {
        return;
    }

    if (stamp.base == 0)
    {
        source.image = CopyPicture(header, p);
        return;
    }

    const uint8_t* base_header = header + stamp.base;
    Tim2Stamp base_stamp;
    Tim2Picture base;

    if (ReadStamp(base_header, &base_stamp) && base_stamp.id == stamp.owner
        && ReadTim2Picture(base_header, kTrusted, &base))
    {
        source.image = CopyPicture(base_header, base);
        source.clut = CopyPicture(header, p);
    }
}

unsigned int CreateTexture(int w, int h, const uint8_t* pixels)
{
    const Uint64 t0 = SDL_GetPerformanceCounter();
    const unsigned int texture =
        MikuPan_GPUCreateTextureRGBA8(w, h, pixels, w * 4, 1, 0);
    g_frame.textures_created++;
    g_frame.create_ms += (double)(SDL_GetPerformanceCounter() - t0) * 1000.0
                         / (double)SDL_GetPerformanceFrequency();
    return texture;
}

void UpdateTexture(unsigned int* texture, int w, int h, int* texture_w,
                   int* texture_h, const uint8_t* pixels)
{
    if (*texture != 0 && *texture_w == w && *texture_h == h)
    {
        MikuPan_GPUUploadTextureRGBA8(*texture, w, h, pixels, w * 4);
        return;
    }

    if (*texture != 0)
    {
        MikuPan_GPUReleaseTexture(*texture);
    }

    *texture = CreateTexture(w, h, pixels);
    *texture_w = w;
    *texture_h = h;
}

void RegisterPicture(Slot* slot, const uint8_t* header, const Tim2Picture& p,
                     const Tim2Stamp& stamp, const Tex2DEntry& e, int offset)
{
    const uint64_t tex0 = Placed(e.tex0, offset);

    *slot = Slot{};

    if (!Tex0Drawable(tex0))
    {
        return;
    }

    KeepSource(header, p, stamp, e);
    slot->key = Key(tex0);
    slot->kind = Kind::kPicture;
    slot->id = stamp.id;
    slot->check_block = (uint32_t)(tex0 & kBlockMask);
    slot->check_owner = stamp.owner;
}

/// The textures (Tex2DDerived) the upload of picture `id` completes.
void RegisterDerived(uint32_t id, int offset)
{
    const Tex2DEntry* e = MikuPanBakedTexture2D::Entry(id);

    for (uint32_t k = 0; e != nullptr && k < e->derived_count; k++)
    {
        const Tex2DDerived* d =
            MikuPanBakedTexture2D::Derived(e->derived_first + k);
        const uint64_t tex0 = Placed(d->tex0, offset);
        Slot& slot = g_by_texel[tex0 & kBlockMask];

        slot = Slot{};
        slot.key = Key(tex0);
        slot.kind = Kind::kDerived;
        slot.id = d->id;
        slot.check_block = (d->check_block + (uint32_t)offset) & kBlockMask;
        slot.check_owner = d->check_owner;
    }
}

void RegisterUnstamped(Slot* slot, std::unique_ptr<Unstamped>* holder,
                       const uint8_t* header, const Tim2Picture& p,
                       int offset, int texel_block)
{
    const uint64_t tex0 = Placed(p.tex0, offset);
    const Tim2Tex0 t = SplitTex0(tex0);

    *slot = Slot{};

    if (!Tex0Drawable(tex0) || p.image_size == 0)
    {
        return;
    }

    if (*holder == nullptr)
    {
        *holder = std::make_unique<Unstamped>();
    }

    Unstamped& u = **holder;
    const size_t bytes = (size_t)(p.clut + p.clut_size - header);

    /// The same picture as last time: keep its texture.
    if (u.source.size() != bytes
        || std::memcmp(u.source.data(), header, bytes) != 0)
    {
        u.source.assign(header, header + bytes);
        u.width = 1 << t.tw;
        u.height = 1 << t.th;
        u.pixels.resize((size_t)u.width * u.height * 4);

        if (!DecodeTim2Texture(p, p, p.tex0, u.pixels.data()))
        {
            u.source.clear();
            return;
        }

        u.dirty = true;
    }

    slot->key = Key(tex0);
    slot->kind = Kind::kUnstamped;
    slot->check_block = (uint32_t)texel_block;
    slot->check_owner = g_owner[texel_block];
}

bool Shows(const Slot& s, uint64_t key)
{
    return s.kind != Kind::kNone && s.key == key
           && (s.check_owner == kNoOwner
               || g_owner[s.check_block] == s.check_owner);
}

/// The pixels a stamped picture or derived texture shows: from the pack, or
/// decoded from the picture data kept when it was uploaded.
bool IdPixels(const Slot& s, std::vector<uint8_t>* out, int* w, int* h)
{
    const Tex2DEntry* e = MikuPanBakedTexture2D::Entry(s.id);

    if (e == nullptr)
    {
        return false;
    }

    *w = e->width;
    *h = e->height;

    if (e->offset != 0)
    {
        return MikuPanBakedTexture2D::ReadPixels(s.id, out);
    }

    SizeTables();

    const Source& source = g_sources[s.id];
    Tim2Picture image;
    Tim2Picture clut;

    if (source.image.empty()
        || !ReadTim2Picture(source.image.data(), source.image.size(), &image))
    {
        return false;
    }

    if (source.clut.empty())
    {
        clut = image;
    }
    else if (!ReadTim2Picture(source.clut.data(), source.clut.size(), &clut))
    {
        return false;
    }

    out->resize((size_t)e->width * e->height * 4);
    return DecodeTim2Texture(image, clut, e->tex0, out->data());
}

unsigned int IdTexture(const Slot& s)
{
    SizeTables();

    if (s.id >= g_textures.size())
    {
        return 0;
    }

    if (g_textures[s.id] == 0)
    {
        int w;
        int h;

        if (!IdPixels(s, &g_pixels, &w, &h))
        {
            return 0;
        }

        g_textures[s.id] = CreateTexture(w, h, g_pixels.data());
    }

    return g_textures[s.id];
}

unsigned int UnstampedTexture(Unstamped* u)
{
    if (u == nullptr)
    {
        return 0;
    }

    if (u->dirty || u->texture == 0)
    {
        UpdateTexture(&u->texture, u->width, u->height, &u->texture_w,
                      &u->texture_h, u->pixels.data());
        u->dirty = false;
    }

    return u->texture;
}

/// A runtime image at TEX0's TBP0 (and for PSMT8 the CLUT image at its
/// CBP), both still the last uploads at their blocks.
bool FindRaw(uint64_t tex0, RawImage** image, const RawImage** clut)
{
    const Tim2Tex0 t = SplitTex0(tex0);
    RawImage* r = g_raw[t.tbp0].get();

    *image = nullptr;
    *clut = nullptr;

    if (r == nullptr || g_owner[t.tbp0] != r->owner || r->dbw != t.tbw
        || r->psm != StoredPsm(t.psm))
    {
        return false;
    }

    if (t.psm == kPsmT8)
    {
        const RawImage* c = g_raw[t.cbp].get();

        if (c == nullptr || g_owner[t.cbp] != c->owner || c->psm != kPsmCt32)
        {
            return false;
        }

        *clut = c;
    }

    *image = r;
    return true;
}

/// What GS::GSHelper::DecodeTexture() reads from a runtime image. Texels
/// outside it are 0.
void RawPixels(uint64_t tex0, const RawImage& r, const RawImage* clut,
               std::vector<uint8_t>* out)
{
    const Tim2Tex0 t = SplitTex0(tex0);
    const int w = 1 << t.tw;
    const int h = 1 << t.th;

    out->assign((size_t)w * h * 4, 0);

    for (int y = 0; y < std::min(h, r.height); y++)
    {
        for (int x = 0; x < std::min(w, r.width); x++)
        {
            uint8_t* d = &(*out)[((size_t)y * w + x) * 4];
            const size_t i = (size_t)y * r.width + x;
            int cx;
            int cy;

            if (r.psm == kPsmCt32)
            {
                Ct32ToRgba8(&r.data[i * 4], d);
            }
            else if (r.psm == kPsmCt16)
            {
                const uint8_t* s = &r.data[i * 2];
                Ct16ToRgba8((uint16_t)(s[0] | (s[1] << 8)), d);
            }
            else if (clut != nullptr)
            {
                T8ClutEntry(r.data[i], &cx, &cy);

                if (cx < clut->width && cy < clut->height)
                {
                    Ct32ToRgba8(
                        &clut->data[((size_t)cy * clut->width + cx) * 4], d);
                }
            }
        }
    }
}

unsigned int RawTexture(uint64_t tex0)
{
    RawImage* r;
    const RawImage* clut;

    if (!FindRaw(tex0, &r, &clut))
    {
        return 0;
    }

    const Tim2Tex0 t = SplitTex0(tex0);
    const int w = 1 << t.tw;
    const int h = 1 << t.th;

    if (r->texture == 0 || r->texture_w != w || r->texture_h != h
        || r->texture_generation != r->generation || r->texture_clut != clut
        || (clut != nullptr && r->texture_clut_generation != clut->generation))
    {
        RawPixels(tex0, *r, clut, &g_pixels);
        UpdateTexture(&r->texture, w, h, &r->texture_w, &r->texture_h,
                      g_pixels.data());
        r->texture_generation = r->generation;
        r->texture_clut = clut;
        r->texture_clut_generation = clut != nullptr ? clut->generation : 0;
    }

    return r->texture;
}

/// The slot that shows `tex0` and its unstamped holder, or nullptr.
const Slot* FindSlot(uint64_t tex0, Unstamped** unstamped)
{
    const Tim2Tex0 t = SplitTex0(tex0);
    const uint64_t key = Key(tex0);

    if (Indexed(t.psm) && Shows(g_by_clut[t.cbp], key))
    {
        *unstamped = g_unstamped_clut[t.cbp].get();
        return &g_by_clut[t.cbp];
    }

    if (Shows(g_by_texel[t.tbp0], key))
    {
        *unstamped = g_unstamped_texel[t.tbp0].get();
        return &g_by_texel[t.tbp0];
    }

    return nullptr;
}

void ReportMiss(uint64_t tex0)
{
    g_frame.misses++;

    for (int i = 0; i < g_reported_miss_count; i++)
    {
        if (g_reported_misses[i] == tex0)
        {
            return;
        }
    }

    if (g_reported_miss_count < (int)(sizeof(g_reported_misses)
                                      / sizeof(g_reported_misses[0])))
    {
        g_reported_misses[g_reported_miss_count++] = tex0;
        spdlog::info("2D texture: nothing uploaded shows TEX0 {:#018x}",
                     tex0);
    }
}

void ReleaseTexture(unsigned int* texture)
{
    if (*texture != 0)
    {
        MikuPan_GPUReleaseTexture(*texture);
        *texture = 0;
    }
}

}// namespace

extern "C" {

void MikuPan_TexSlots_UploadImage(const void* picture_header, int block)
{
    const uint8_t* header = (const uint8_t*)picture_header;
    Tim2Picture p;
    Tim2Stamp stamp;

    if (header == nullptr || !ReadTim2Picture(header, kTrusted, &p))
    {
        return;
    }

    Init();
    block &= kBlockMask;

    const Tex2DEntry* e = Stamped(header, p, &stamp);
    const Tim2Tex0 t = SplitTex0(e != nullptr ? e->tex0 : p.tex0);
    const int offset = (block - t.tbp0) & kBlockMask;

    g_owner[block] = e != nullptr ? stamp.id : NewRuntimeOwner();
    g_frame.picture_uploads++;

    /// Indexed pictures are registered with their CLUT.
    if (e != nullptr && !Indexed(t.psm))
    {
        RegisterPicture(&g_by_texel[block], header, p, stamp, *e, offset);
    }
    else if (!Indexed(t.psm))
    {
        RegisterUnstamped(&g_by_texel[block], &g_unstamped_texel[block],
                          header, p, offset, block);
    }

    /// Without a CLUT, the image is the picture's last upload.
    if (e != nullptr && p.clut_size == 0)
    {
        RegisterDerived(stamp.id, offset);
    }
}

void MikuPan_TexSlots_UploadClut(const void* picture_header, int block)
{
    const uint8_t* header = (const uint8_t*)picture_header;
    Tim2Picture p;
    Tim2Stamp stamp;

    if (header == nullptr || !ReadTim2Picture(header, kTrusted, &p))
    {
        return;
    }

    Init();
    block &= kBlockMask;

    const Tex2DEntry* e = Stamped(header, p, &stamp);
    const Tim2Tex0 t = SplitTex0(e != nullptr ? e->tex0 : p.tex0);
    const int offset = (block - t.cbp) & kBlockMask;

    g_owner[block] = e != nullptr ? stamp.id : NewRuntimeOwner();
    g_frame.picture_uploads++;

    if (e != nullptr && Indexed(t.psm))
    {
        RegisterPicture(&g_by_clut[block], header, p, stamp, *e, offset);
    }
    else if (Indexed(t.psm))
    {
        RegisterUnstamped(&g_by_clut[block], &g_unstamped_clut[block], header,
                          p, offset, (t.tbp0 + offset) & kBlockMask);
    }

    if (e != nullptr)
    {
        RegisterDerived(stamp.id, offset);
    }
}

void MikuPan_TexSlots_UploadRaw(int dbp, int dbw, int dpsm, int x, int y,
                                int w, int h, const void* data)
{
    if (w <= 0 || h <= 0 || x < 0 || y < 0 || data == nullptr)
    {
        return;
    }

    Init();

    const int block = dbp & kBlockMask;
    const int psm = StoredPsm(dpsm);

    if (psm < 0)
    {
        /// Only the formats the game transfers at run time are kept.
        if (!g_reported_raw_format)
        {
            spdlog::warn("2D texture: a runtime transfer in PSM {:#x} is "
                         "ignored",
                         dpsm);
            g_reported_raw_format = true;
        }

        g_owner[block] = NewRuntimeOwner();
        return;
    }

    std::unique_ptr<RawImage>& slot = g_raw[block];
    const int bpp = BytesPerPixel(psm);

    /// A transfer at (0, 0) starts a new image; others add to the image
    /// still at the block (images sent in strips).
    if (slot == nullptr || g_owner[block] != slot->owner || slot->psm != psm
        || slot->dbw != dbw || (x == 0 && y == 0))
    {
        if (slot == nullptr)
        {
            slot = std::make_unique<RawImage>();
        }

        slot->owner = NewRuntimeOwner();
        slot->psm = psm;
        slot->dbw = dbw;
        slot->width = 0;
        slot->height = 0;
        slot->data.clear();
        g_owner[block] = slot->owner;
    }

    RawImage& r = *slot;

    if (x + w > r.width || y + h > r.height)
    {
        const int new_w = std::max(r.width, x + w);
        const int new_h = std::max(r.height, y + h);
        std::vector<uint8_t> grown((size_t)new_w * new_h * bpp, 0);

        for (int row = 0; row < r.height; row++)
        {
            std::memcpy(&grown[(size_t)row * new_w * bpp],
                        &r.data[(size_t)row * r.width * bpp],
                        (size_t)r.width * bpp);
        }

        r.data.swap(grown);
        r.width = new_w;
        r.height = new_h;
    }

    const uint8_t* src = (const uint8_t*)data;

    for (int row = 0; row < h; row++)
    {
        std::memcpy(&r.data[((size_t)(y + row) * r.width + x) * bpp],
                    src + (size_t)row * w * bpp, (size_t)w * bpp);
    }

    r.generation++;
    g_frame.raw_uploads++;
    g_frame.raw_upload_bytes += w * h * bpp;
}

void MikuPan_TexSlots_StoreRaw(int sbp, int sbw, int spsm, int x, int y,
                               int w, int h, void* out)
{
    const int psm = StoredPsm(spsm);

    if (out == nullptr || w <= 0 || h <= 0)
    {
        return;
    }

    if (psm != kPsmCt32 && psm != kPsmCt16)
    {
        spdlog::warn("Unsupported sceGsExecStoreImage SPSM: {:#x}", spsm);
        std::memset(out, 0, (size_t)w * h * 4);
        return;
    }

    Init();

    const int bpp = BytesPerPixel(psm);
    const int block = sbp & kBlockMask;
    const RawImage* r = g_raw[block].get();
    uint8_t* dst = (uint8_t*)out;

    if (r == nullptr || g_owner[block] != r->owner || r->psm != psm
        || r->dbw != sbw)
    {
        r = nullptr;
    }

    for (int row = 0; row < h; row++)
    {
        for (int col = 0; col < w; col++)
        {
            uint8_t* d = dst + ((size_t)row * w + col) * bpp;
            const int sx = x + col;
            const int sy = y + row;

            if (r != nullptr && sx < r->width && sy < r->height)
            {
                std::memcpy(d, &r->data[((size_t)sy * r->width + sx) * bpp],
                            bpp);
            }
            else
            {
                std::memset(d, 0, bpp);
            }
        }
    }
}

unsigned int MikuPan_TexSlots_Resolve(uint64_t tex0)
{
    Init();

    Unstamped* unstamped = nullptr;
    const Slot* s = FindSlot(tex0, &unstamped);
    unsigned int texture = 0;

    if (s != nullptr)
    {
        texture = s->kind == Kind::kUnstamped ? UnstampedTexture(unstamped)
                                              : IdTexture(*s);
    }
    else
    {
        texture = RawTexture(tex0);
    }

    if (texture == 0 && Tex0Drawable(tex0))
    {
        ReportMiss(tex0);
    }

    return texture;
}

unsigned int MikuPan_TexSlots_CreateCopy(uint64_t tex0)
{
    Init();

    Unstamped* unstamped = nullptr;
    const Slot* s = FindSlot(tex0, &unstamped);
    int w = 0;
    int h = 0;

    if (s != nullptr && s->kind == Kind::kUnstamped)
    {
        return unstamped != nullptr
                   ? CreateTexture(unstamped->width, unstamped->height,
                                   unstamped->pixels.data())
                   : 0;
    }

    if (s != nullptr)
    {
        return IdPixels(*s, &g_pixels, &w, &h)
                   ? CreateTexture(w, h, g_pixels.data())
                   : 0;
    }

    RawImage* r;
    const RawImage* clut;

    if (!FindRaw(tex0, &r, &clut))
    {
        ReportMiss(tex0);
        return 0;
    }

    const Tim2Tex0 t = SplitTex0(tex0);
    RawPixels(tex0, *r, clut, &g_pixels);
    return CreateTexture(1 << t.tw, 1 << t.th, g_pixels.data());
}

void MikuPan_TexSlots_ResetFrameStats(void)
{
    g_last_frame = g_frame;
    g_frame = FrameStats{};
}

int MikuPan_TexSlots_GetPictureUploads(void)
{
    return g_last_frame.picture_uploads;
}

int MikuPan_TexSlots_GetRawUploads(void)
{
    return g_last_frame.raw_uploads;
}

int MikuPan_TexSlots_GetRawUploadBytes(void)
{
    return g_last_frame.raw_upload_bytes;
}

int MikuPan_TexSlots_GetTexturesCreated(void)
{
    return g_last_frame.textures_created;
}

float MikuPan_TexSlots_GetCreateMs(void)
{
    return (float)g_last_frame.create_ms;
}

int MikuPan_TexSlots_GetMisses(void)
{
    return g_last_frame.misses;
}

void MikuPan_TexSlots_ForEachTexture(
    void (*callback)(unsigned int texture, int width, int height, uint32_t id,
                     void* userdata),
    void* userdata)
{
    for (uint32_t id = 0; id < g_textures.size(); id++)
    {
        const Tex2DEntry* e = MikuPanBakedTexture2D::Entry(id);

        if (g_textures[id] != 0 && e != nullptr)
        {
            callback(g_textures[id], e->width, e->height, id, userdata);
        }
    }

    for (int i = 0; i < kBlocks; i++)
    {
        const Unstamped* u[2] = {g_unstamped_clut[i].get(),
                                 g_unstamped_texel[i].get()};

        for (const Unstamped* p : u)
        {
            if (p != nullptr && p->texture != 0)
            {
                callback(p->texture, p->texture_w, p->texture_h, kNoTexture,
                         userdata);
            }
        }

        if (g_raw[i] != nullptr && g_raw[i]->texture != 0)
        {
            callback(g_raw[i]->texture, g_raw[i]->texture_w,
                     g_raw[i]->texture_h, kNoTexture, userdata);
        }
    }
}

void MikuPan_TexSlots_ReleaseTextures(void)
{
    for (unsigned int& texture : g_textures)
    {
        ReleaseTexture(&texture);
    }

    for (int i = 0; i < kBlocks; i++)
    {
        if (g_unstamped_clut[i] != nullptr)
        {
            ReleaseTexture(&g_unstamped_clut[i]->texture);
        }

        if (g_unstamped_texel[i] != nullptr)
        {
            ReleaseTexture(&g_unstamped_texel[i]->texture);
        }

        if (g_raw[i] != nullptr)
        {
            ReleaseTexture(&g_raw[i]->texture);
        }
    }
}

void MikuPan_TexSlots_Shutdown(void)
{
    MikuPan_TexSlots_ReleaseTextures();

    for (int i = 0; i < kBlocks; i++)
    {
        g_unstamped_clut[i].reset();
        g_unstamped_texel[i].reset();
        g_raw[i].reset();
        g_by_clut[i] = Slot{};
        g_by_texel[i] = Slot{};
    }

    g_textures.clear();
    g_sources.clear();
    g_initialized = false;
}

}// extern "C"
