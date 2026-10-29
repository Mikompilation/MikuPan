#include "mikupan_bake_data.h"

#include "mikupan/gs/mikupan_gs_memory.h"

namespace MikuPanBake
{
namespace
{

constexpr uint32_t kMaxPakEntries = 4096;
constexpr int kMaxPakDepth = 3;
constexpr uint32_t kMaxBlocks = 4096;
constexpr size_t kMaxUnitsPerChain = 1u << 20;

/// sceGsLoadImage: GIF tag, BITBLTBUF, TRXPOS, TRXREG, TRXDIR, GIF image tag.
constexpr size_t kLoadImageSize = 96;
/// SGDTRI2FILEHEADER: a 16-byte VIF header in front of the load image.
constexpr size_t kTri2PacketHeader = 16;

bool Fail(std::string* error, const std::string& why)
{
    if (error != nullptr)
    {
        *error = why;
    }

    return false;
}

/// One sceGsLoadImage at `li` (its image data follows it).
Upload ReadLoadImage(const uint8_t* data, size_t size, size_t li)
{
    const uint64_t bitbltbuf = Rd64(data + li + 16);
    const uint64_t trxpos = Rd64(data + li + 32);
    const uint64_t trxreg = Rd64(data + li + 48);

    Upload u;
    u.dbp = (int)((bitbltbuf >> 32) & 0x3FFF);
    u.dbw = (int)((bitbltbuf >> 48) & 0x3F);
    u.dpsm = (int)((bitbltbuf >> 56) & 0x3F);
    u.dsax = (int)((trxpos >> 32) & 0x7FF);
    u.dsay = (int)((trxpos >> 48) & 0x7FF);
    u.rrw = (int)(trxreg & 0xFFF);
    u.rrh = (int)((trxreg >> 32) & 0xFFF);
    u.data = data + li + kLoadImageSize;
    u.available = size - (li + kLoadImageSize);
    return u;
}

/// NLOOP of the GIF image tag that ends a load image.
size_t ImageQwords(const uint8_t* data, size_t li)
{
    return (size_t)(Rd64(data + li + 80) & 0x7FFF);
}

/// One TRI2 packet, as the game's GS mirror of it used to upload it: the
/// image, and for any DPSM other than PSMCT32 the CLUT load image that
/// follows it.
bool ReadTri2Packet(const uint8_t* data, size_t size, size_t packet,
                    std::vector<Upload>* out, std::string* error)
{
    const size_t li = packet + kTri2PacketHeader;

    if (li + kLoadImageSize > size)
    {
        return Fail(error, "TRI2 packet runs past the file");
    }

    const Upload image = ReadLoadImage(data, size, li);
    out->push_back(image);

    if (image.dpsm != PSMCT32)
    {
        const size_t clut_li = li + kLoadImageSize + ImageQwords(data, li) * 16;

        if (clut_li + kLoadImageSize > size)
        {
            return Fail(error, "TRI2 CLUT runs past the file");
        }

        out->push_back(ReadLoadImage(data, size, clut_li));
    }

    return true;
}

/// Walks `count` packets from `q` (each: 16-byte VIF header whose u16 at
/// +12 counts the qwords after it). Packets of one unit stay inside it.
bool ReadPackets(const uint8_t* data, size_t size, size_t q, size_t unit_end,
                 int count, std::vector<std::vector<Upload>>* packets,
                 std::string* error)
{
    for (int i = 0; i < count; i++)
    {
        if (q + kTri2PacketHeader > unit_end)
        {
            return Fail(error, "TRI2 packet outside its unit");
        }

        std::vector<Upload> uploads;
        if (!ReadTri2Packet(data, size, q, &uploads, error))
        {
            return false;
        }

        packets->push_back(std::move(uploads));
        q += kTri2PacketHeader + (size_t)Rd16(data + q + 12) * 16;
    }

    return true;
}

}// namespace

bool ReadPak(const uint8_t* data, size_t base, size_t size,
             std::vector<PakEntry>* entries)
{
    entries->clear();

    if (size < 32)
    {
        return false;
    }

    const uint32_t count = Rd32(data + base);

    if (count == 0 || count > kMaxPakEntries || Rd32(data + base + 4) != 0
        || Rd32(data + base + 8) != 0 || Rd32(data + base + 12) != 0)
    {
        return false;
    }

    const size_t end = base + size;
    size_t p = base + 16;

    for (uint32_t i = 0; i < count; i++)
    {
        if (p + 16 > end)
        {
            return false;
        }

        const size_t n = Rd32(data + p);

        if (n == 0 || p + 16 + n > end)
        {
            return false;
        }

        PakEntry e;
        e.off = p + 16;
        e.size = n;
        e.type = Rd32(data + p + 4);
        entries->push_back(e);
        p += 16 + n;
    }

    return true;
}

bool IsSgd(const uint8_t* data, size_t top, size_t size)
{
    if (size < 0x1C || Rd32(data + top) != kSgdVersion || data[top + 4] != 0)
    {
        return false;
    }

    const uint32_t blocks = Rd32(data + top + 0x14);
    return blocks > 0 && blocks <= kMaxBlocks
           && 0x18 + (size_t)blocks * 4 <= size;
}

void FindSgds(const uint8_t* data, size_t base, size_t size,
              std::vector<SgdRef>* out, int depth)
{
    if (IsSgd(data, base, size))
    {
        out->push_back({base, size});
        return;
    }

    std::vector<PakEntry> entries;

    if (depth >= kMaxPakDepth || !ReadPak(data, base, size, &entries))
    {
        return;
    }

    for (const PakEntry& e : entries)
    {
        FindSgds(data, e.off, e.size, out, depth + 1);
    }
}

bool WalkChains(const uint8_t* data, const SgdRef& sgd,
                std::vector<std::vector<Unit>>* blocks, std::string* error)
{
    const uint32_t count = Rd32(data + sgd.top + 0x14);
    const size_t end = sgd.top + sgd.size;

    blocks->assign(count, {});

    for (uint32_t i = 0; i < count; i++)
    {
        const uint32_t start = Rd32(data + sgd.top + 0x18 + i * 4);

        if (start == 0)
        {
            continue;
        }

        size_t p = sgd.top + start;
        std::vector<Unit>& units = (*blocks)[i];

        while (true)
        {
            if (p + 16 > end)
            {
                return Fail(error, "block chain runs past the SGD");
            }

            const uint32_t len = Rd32(data + p);

            if (len == 0)
            {
                break;
            }

            if (len < 16 || (len & 15) != 0 || p + len > end)
            {
                return Fail(error, "bad procedure unit length");
            }

            units.push_back({p, Rd32(data + p + 4), len});
            p += len;

            if (units.size() > kMaxUnitsPerChain)
            {
                return Fail(error, "block chain too long");
            }
        }
    }

    return true;
}

size_t MeshTextureSlot(const uint8_t* data, const Unit& mesh,
                       uint32_t* marker)
{
    const uint8_t type = data[mesh.off + 13];
    size_t slot = 0;

    if (type == 0x02 || type == 0x0A || type == 0x82)
    {
        slot = mesh.off + 0x28;
    }
    else if (type == 0x12 || type == 0x32)
    {
        /// MikuPan_RenderMeshType0x32(): the slot is bound only when it
        /// lies before the colour data (sOffsetToPrim, in words).
        const int16_t to_prim = (int16_t)Rd16(data + mesh.off + 22);

        if (0x38 < (int)to_prim * 4)
        {
            slot = mesh.off + 0x38;
        }
    }

    if (slot == 0 || slot + 16 > mesh.off + mesh.len)
    {
        return 0;
    }

    *marker = Rd32(data + slot + 8);
    return slot;
}

size_t UploadBytes(const Upload& u)
{
    const size_t texels = (size_t)u.rrw * (size_t)u.rrh;

    switch (u.dpsm)
    {
        case PSMZ32:
        case PSMCT32:
            return texels * 4;
        case PSMZ16:
        case PSMZ16S:
        case PSMCT16:
        case PSMCT16S:
            return texels * 2;
        case PSMT8:
            return texels;
        case PSMT4:
            return (texels + 1) / 2;
        default:
            return 0;
    }
}

bool ParseTri2Unit(const uint8_t* data, size_t size, const Unit& unit,
                   std::vector<Upload>* out, std::string* error)
{
    const uint32_t count = Rd32(data + unit.off + 8);
    const int pads = (int)Rd32(data + unit.off + 12);

    if (count > 4096 || pads < 0)
    {
        return Fail(error, "bad TRI2 unit header");
    }

    std::vector<std::vector<Upload>> packets;
    if (!ReadPackets(data, size, unit.off + 16 + (size_t)(pads / 4) * 4,
                     unit.off + unit.len, (int)count, &packets, error))
    {
        return false;
    }

    for (std::vector<Upload>& p : packets)
    {
        out->insert(out->end(), p.begin(), p.end());
    }

    return true;
}

bool ParseTextureAnimation(const uint8_t* data, size_t size, const Unit& unit,
                           TextureAnimationInfo* info,
                           std::vector<std::vector<Upload>>* frames,
                           std::string* error)
{
    info->tex_num = data[unit.off + 8];
    info->pads = data[unit.off + 9];
    info->frames_off = unit.off + 16 + (size_t)(info->pads / 4) * 4;

    frames->clear();
    return ReadPackets(data, size, info->frames_off, unit.off + unit.len,
                       info->tex_num, frames, error);
}

bool ParseTim2Pak(const uint8_t* data, size_t data_size, size_t base,
                  size_t size, int offset, std::vector<Upload>* out,
                  std::string* error)
{
    std::vector<PakEntry> entries;

    /// A pak with no files uploads nothing (the shadow-only models have
    /// one).
    if (size >= 16 && Rd32(data + base) == 0)
    {
        return true;
    }

    if (!ReadPak(data, base, size, &entries))
    {
        return Fail(error, "bad TIM2 pak");
    }

    for (const PakEntry& e : entries)
    {
        /// MakeTim2Direct3(): FormatId 0 aligns the picture header to 16
        /// bytes, 1 to 128; anything else uploads nothing.
        const uint8_t format_id = e.size > 5 ? data[e.off + 5] : 0xFF;
        size_t ph;

        if (format_id == 0)
        {
            ph = e.off + 16;
        }
        else if (format_id == 1)
        {
            ph = e.off + 0x80;
        }
        else
        {
            continue;
        }

        if (ph + 0x30 > e.off + e.size)
        {
            return Fail(error, "TIM2 picture header outside its file");
        }

        const uint32_t clut_size = Rd32(data + ph + 4);
        const uint32_t image_size = Rd32(data + ph + 8);
        const uint16_t header_size = Rd16(data + ph + 12);
        const uint16_t clut_colors = Rd16(data + ph + 14);
        const uint16_t width = Rd16(data + ph + 20);
        const uint16_t height = Rd16(data + ph + 22);
        const GS::Tex0 tex0 = GS::DecodeTex0(Rd64(data + ph + 24));
        const size_t image = ph + header_size;

        if (image_size != 0)
        {
            Upload u;
            u.dpsm = tex0.psm;
            /// BITBLTBUF.DBP is 14 bits wide.
            u.dbp = (tex0.tbp0 + offset) & 0x3FFF;
            u.dbw = tex0.tbw;
            u.rrw = width & 0xFFF;
            u.rrh = height & 0xFFF;

            if (image > data_size)
            {
                return Fail(error, "TIM2 image outside the file");
            }

            u.data = data + image;
            u.available = data_size - image;
            out->push_back(u);
        }

        if (clut_size != 0)
        {
            const size_t clut = image + image_size;

            Upload u;
            u.dpsm = PSMCT32;
            u.dbp = (tex0.cbp + offset) & 0x3FFF;
            u.dbw = 1;
            u.rrw = clut_colors == 16 ? 8 : 16;
            u.rrh = clut_colors == 16 ? 2 : clut_colors / 16;

            if (clut > data_size)
            {
                return Fail(error, "TIM2 CLUT outside the file");
            }

            u.data = data + clut;
            u.available = data_size - clut;
            out->push_back(u);
        }
    }

    return true;
}

}// namespace MikuPanBake
