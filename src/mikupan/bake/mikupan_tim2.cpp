#include "mikupan_tim2.h"

#include "mikupan_bake_data.h"

#include <algorithm>
#include <cstring>

namespace MikuPanBake
{
namespace
{

constexpr int kPsmCt32 = 0x00;
constexpr int kPsmCt16 = 0x02;
constexpr int kPsmCt16s = 0x0A;
constexpr int kPsmT8 = 0x13;
constexpr int kPsmT4 = 0x14;

constexpr uint32_t kMaxPk2Files = 4096;
/// GS::IsDecodableTexture(): both sides at most 4096.
constexpr int kMaxLog2Side = 12;
/// GS::kVramSize.
constexpr uint64_t kVramBytes = 4 * 1024 * 1024;

/// GS::AdjustPS2Alpha().
uint8_t Ps2Alpha(uint8_t alpha)
{
    return alpha <= 127 ? (uint8_t)(alpha << 1) : 0xFF;
}

/// Bytes the GS upload of a w x h image reads, 0 for a format the game
/// cannot draw.
size_t ImageBytes(int psm, int w, int h)
{
    const size_t texels = (size_t)w * (size_t)h;

    switch (psm)
    {
        case kPsmCt32:
            return texels * 4;
        case kPsmCt16:
        case kPsmCt16s:
            return texels * 2;
        case kPsmT8:
            return texels;
        case kPsmT4:
            return (texels + 1) / 2;
        default:
            return 0;
    }
}

/// Width in texels of one row of pages of a buffer `tbw` * 64 wide. A PSMT8
/// or PSMT4 page is 128 texels wide, so an odd width loses its last 64.
int RowTexels(int psm, int tbw)
{
    return psm == kPsmT8 || psm == kPsmT4 ? (tbw >> 1) * 128 : tbw * 64;
}

/// Offset of picture 0 of the TIM2 file at `file` (MakeTim2Direct3():
/// FormatId 0 aligns it to 16 bytes, 1 to 128), or 0 if it is no TIM2 file.
size_t FirstPictureOffset(const uint8_t* file, size_t size)
{
    if (size < 16 || std::memcmp(file, "TIM2", 4) != 0)
    {
        return 0;
    }

    switch (file[5])
    {
        case 0:
            return 16;
        case 1:
            return 0x80;
        default:
            return 0;
    }
}

bool AddTim2File(const uint8_t* data, size_t base, size_t size,
                 std::vector<size_t>* headers)
{
    const size_t first = FirstPictureOffset(data + base, size);
    const uint16_t count = size >= 8 ? Rd16(data + base + 6) : 0;

    if (first == 0 || count == 0 || first > size)
    {
        return false;
    }

    size_t ph = base + first;

    for (uint16_t i = 0; i < count; i++)
    {
        Tim2Picture picture;

        if (!ReadTim2Picture(data + ph, base + size - ph, &picture))
        {
            return false;
        }

        headers->push_back(ph);

        const uint32_t total = Rd32(data + ph);

        if (i + 1 < count
            && (total < kTim2PictureHeaderSize || total > base + size - ph))
        {
            return false;
        }

        ph += total;
    }

    return true;
}

}// namespace

bool ReadTim2Picture(const uint8_t* header, size_t available,
                     Tim2Picture* out)
{
    if (header == nullptr || available < kTim2PictureHeaderSize)
    {
        return false;
    }

    const uint32_t clut_size = Rd32(header + 4);
    const uint32_t image_size = Rd32(header + 8);
    const uint16_t header_size = Rd16(header + 12);

    if (header_size < kTim2PictureHeaderSize || header_size > available
        || image_size > available - header_size
        || clut_size > available - header_size - image_size)
    {
        return false;
    }

    out->header = header;
    out->image = header + header_size;
    out->clut = out->image + image_size;
    out->image_size = image_size;
    out->clut_size = clut_size;
    out->clut_colors = Rd16(header + 14);
    out->clut_type = header[18];
    out->image_type = header[19];
    out->width = Rd16(header + 20);
    out->height = Rd16(header + 22);
    out->tex0 = Rd64(header + 24);
    return true;
}

bool FindTim2Pictures(const uint8_t* data, size_t size,
                      std::vector<size_t>* headers)
{
    headers->clear();

    if (FirstPictureOffset(data, size) != 0)
    {
        return AddTim2File(data, 0, size, headers);
    }

    if (size < 16)
    {
        return false;
    }

    const uint32_t count = Rd32(data);

    if (count == 0 || count > kMaxPk2Files || 16 + (size_t)count * 4 > size)
    {
        return false;
    }

    for (uint32_t i = 0; i < count; i++)
    {
        const uint32_t off = Rd32(data + 16 + 4 * (size_t)i);

        if (off < 16 + (size_t)count * 4 || off >= size
            || !AddTim2File(data, off, size - off, headers))
        {
            headers->clear();
            return false;
        }
    }

    return true;
}

Tim2Tex0 SplitTex0(uint64_t tex0)
{
    Tim2Tex0 t;
    t.tbp0 = (int)(tex0 & 0x3FFF);
    t.tbw = (int)((tex0 >> 14) & 0x3F);
    t.psm = (int)((tex0 >> 20) & 0x3F);
    t.tw = (int)((tex0 >> 26) & 0x0F);
    t.th = (int)((tex0 >> 30) & 0x0F);
    t.cbp = (int)((tex0 >> 37) & 0x3FFF);
    t.csa = (int)((tex0 >> 56) & 0x1F);
    return t;
}

void Ct32ToRgba8(const uint8_t* texel, uint8_t* out)
{
    out[0] = texel[0];
    out[1] = texel[1];
    out[2] = texel[2];
    out[3] = Ps2Alpha(texel[3]);
}

void Ct16ToRgba8(uint16_t texel, uint8_t* out)
{
    const int r = texel & 0x1F;
    const int g = (texel >> 5) & 0x1F;
    const int b = (texel >> 10) & 0x1F;
    out[0] = (uint8_t)((r << 3) | (r >> 2));
    out[1] = (uint8_t)((g << 3) | (g >> 2));
    out[2] = (uint8_t)((b << 3) | (b >> 2));
    out[3] = (texel & 0x8000) ? 0xFF : 0x00;
}

void T8ClutEntry(int index, int* x, int* y)
{
    *y = ((index & 0xE0) >> 4) + ((index & 0x08) ? 1 : 0);
    *x = (index & 0x07) + ((index & 0x10) ? 8 : 0);
}

bool Tex0Drawable(uint64_t tex0)
{
    const Tim2Tex0 t = SplitTex0(tex0);

    if (t.tw > kMaxLog2Side || t.th > kMaxLog2Side)
    {
        return false;
    }

    const uint64_t texels = (uint64_t)1 << (t.tw + t.th);
    uint64_t bytes;

    switch (t.psm)
    {
        case kPsmCt32:
            bytes = texels * 4;
            break;
        case kPsmCt16:
        case kPsmCt16s:
            bytes = texels * 2;
            break;
        case kPsmT8:
            bytes = texels;
            break;
        case kPsmT4:
            bytes = (texels + 1) / 2;
            break;
        default:
            return false;
    }

    return (uint64_t)t.tbp0 * 256 + bytes <= kVramBytes;
}

bool DecodeTim2Texture(const Tim2Picture& image, const Tim2Picture& clut,
                       uint64_t tex0, uint8_t* out)
{
    const Tim2Tex0 t = SplitTex0(tex0);
    const Tim2Tex0 layout = SplitTex0(image.tex0);

    if (t.tw > kMaxLog2Side || t.th > kMaxLog2Side || layout.psm != t.psm
        || layout.tbw != t.tbw || image.width <= 0 || image.height <= 0
        || image.width > RowTexels(t.psm, t.tbw))
    {
        return false;
    }

    const size_t need = ImageBytes(t.psm, image.width, image.height);

    if (need == 0 || need > image.image_size)
    {
        return false;
    }

    /// The CLUT as MakeClutDirect3() places it: a PSMCT32 rectangle, 8x2
    /// for 16 colours, else 16 wide. The texture reads entry (cx, cy).
    uint8_t entries[256][4] = {};
    bool valid[256] = {};

    if (t.psm == kPsmT8 || t.psm == kPsmT4)
    {
        const int rw = clut.clut_colors == 16 ? 8 : 16;
        const int rh = clut.clut_colors == 16 ? 2 : clut.clut_colors / 16;

        if (rh == 0 || (size_t)rw * rh * 4 > clut.clut_size)
        {
            return false;
        }

        const int count = t.psm == kPsmT8 ? 256 : 16;

        for (int i = 0; i < count; i++)
        {
            int cx;
            int cy;

            if (t.psm == kPsmT8)
            {
                T8ClutEntry(i, &cx, &cy);
            }
            else
            {
                cy = ((i >> 3) & 1) + (t.csa & 0x0E);
                cx = (i & 7) + ((t.csa & 1) << 3);
            }

            if (cx >= rw || cy >= rh)
            {
                continue;
            }

            Ct32ToRgba8(clut.clut + ((size_t)cy * rw + cx) * 4, entries[i]);
            valid[i] = true;
        }
    }

    const int tex_w = 1 << t.tw;
    const int tex_h = 1 << t.th;
    const int w = std::min(image.width, tex_w);
    const int h = std::min(image.height, tex_h);

    std::memset(out, 0, (size_t)tex_w * tex_h * 4);

    for (int y = 0; y < h; y++)
    {
        uint8_t* d = out + (size_t)y * tex_w * 4;

        for (int x = 0; x < w; x++, d += 4)
        {
            const size_t i = (size_t)y * image.width + x;

            switch (t.psm)
            {
                case kPsmT8:
                case kPsmT4:
                {
                    const int index =
                        t.psm == kPsmT8
                            ? image.image[i]
                            : (image.image[i >> 1] >> ((i & 1) * 4)) & 0x0F;

                    if (!valid[index])
                    {
                        return false;
                    }

                    std::memcpy(d, entries[index], 4);
                    break;
                }
                case kPsmCt32:
                    Ct32ToRgba8(image.image + i * 4, d);
                    break;
                default:
                    Ct16ToRgba8(Rd16(image.image + i * 2), d);
                    break;
            }
        }
    }

    return true;
}

}// namespace MikuPanBake
