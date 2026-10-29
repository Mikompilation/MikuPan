#include "mikupan_gs_memory.h"

#include <cstring>

/// Same per-function optimisation the game applies to its other hot GS
/// loops (sdk/typedefs.h MIKUPAN_OPTIMIZE_O3), without depending on the SDK.
#if defined(__GNUC__) && !defined(__clang__)
#define MIKUPAN_GS_HOT __attribute__((optimize("O3")))
#else
#define MIKUPAN_GS_HOT
#endif

namespace GS
{
namespace
{

/// Every memory access goes through this mask. Valid data never reaches it
/// (see kVramAllocSize); it only keeps a malformed TEX0 or transfer inside
/// the allocation.
constexpr int kVramAllocMask = kVramAllocSize - 1;

int MIKUPAN_GS_HOT GetBlockIdPSMCT32(int block, int x, int y)
{
    const int block_y = (y >> 3) & 0x03;
    const int block_x = (x >> 3) & 0x07;
    return block + ((x >> 1) & ~0x1F)
           + kBlockTablePSMCT32[(block_y << 3) | block_x];
}

int MIKUPAN_GS_HOT GetBlockIdPSMCT16(int block, int x, int y)
{
    static constexpr int block_table[] = {
        0,  2,  8,  10,
        1,  3,  9,  11,
        4,  6,  12, 14,
        5,  7,  13, 15,
        16, 18, 24, 26,
        17, 19, 25, 27,
        20, 22, 28, 30,
        21, 23, 29, 31,
    };
    const int block_y = (y >> 3) & 0x07;
    const int block_x = (x >> 4) & 0x03;
    return block + block_table[(block_y << 2) | block_x];
}

int MIKUPAN_GS_HOT GetBlockIdPSMT8(int block, int x, int y)
{
    const int block_y = (y >> 4) & 0x03;
    const int block_x = (x >> 4) & 0x07;
    return block + ((x >> 2) & ~0x1F)
           + kBlockTablePSMT8[(block_y << 3) | block_x];
}

int MIKUPAN_GS_HOT GetBlockIdPSMT4(int block, int x, int y)
{
    const int block_base = ((y >> 6) & 0x01) << 4;
    const int block_y = (y >> 4) & 0x03;
    const int block_x = (x >> 5) & 0x03;
    return block + ((x >> 2) & ~0x1F) + block_base
           + kBlockTablePSMT4[(block_y << 2) | block_x];
}

void ExpandPSMCT16Pixel(uint16_t p, unsigned char *out)
{
    const unsigned char r5 = (unsigned char)(p & 0x1F);
    const unsigned char g5 = (unsigned char)((p >> 5) & 0x1F);
    const unsigned char b5 = (unsigned char)((p >> 10) & 0x1F);

    out[0] = (unsigned char)((r5 << 3) | (r5 >> 2));
    out[1] = (unsigned char)((g5 << 3) | (g5 >> 2));
    out[2] = (unsigned char)((b5 << 3) | (b5 >> 2));
    out[3] = (p & 0x8000) ? 0x80 : 0x00;
}

}// namespace

int MIKUPAN_GS_HOT GetPixelAddressPSMCT32(int block, int width, int x, int y)
{
    const int page = (block >> 5) + (y >> 5) * width + (x >> 6);
    const int column_base = ((y >> 1) & 0x03) << 4;
    const int column_y = y & 0x01;
    const int column_x = x & 0x07;
    const int column =
        column_base + kColumnTablePSMCT32[(column_y << 3) | column_x];
    const int addr =
        ((page << 11)
         + (GetBlockIdPSMCT32(block & 0x1F, x & 0x3F, y & 0x1F) << 6) + column);
    return (addr << 2) & 0x003FFFFC;
}

int MIKUPAN_GS_HOT GetPixelAddressPSMCT16(int block, int width, int x, int y)
{
    static constexpr int column_table[] = {
        0,  2,  8,  10, 16, 18, 24, 26,
        1,  3,  9,  11, 17, 19, 25, 27,
        4,  6,  12, 14, 20, 22, 28, 30,
        5,  7,  13, 15, 21, 23, 29, 31,
    };
    const int page = (block >> 5) + (y >> 6) * width + (x >> 6);
    const int column_base = ((y >> 1) & 0x03) << 5;
    const int column_y = y & 0x01;
    const int column_x = x & 0x0F;
    const int column =
        column_base + column_table[(column_y << 4) | column_x];
    const int addr =
        (page << 13)
        + (GetBlockIdPSMCT16(block & 0x1F, x & 0x3F, y & 0x3F) << 8)
        + (column << 1);
    return addr & 0x003FFFFE;
}

int MIKUPAN_GS_HOT GetPixelAddressPSMT8(int block, int width, int x, int y)
{
    const int page = (block >> 5) + (y >> 6) * (width >> 1) + (x >> 7);
    const int column_y = y & 0x0F;
    const int column_x = x & 0x0F;
    const int column = kColumnTablePSMT8[(column_y << 4) | column_x];
    const int addr = (page << 13)
                     + (GetBlockIdPSMT8(block & 0x1F, x & 0x7F, y & 0x3F) << 8)
                     + column;
    return addr;
}

int MIKUPAN_GS_HOT GetPixelAddressPSMT4(int block, int width, int x, int y)
{
    const int page = ((block >> 5) + (y >> 7) * (width >> 1) + (x >> 7));
    const int column_y = y & 0x0F;
    const int column_x = x & 0x1F;
    const int column = kColumnTablePSMT4[(column_y << 5) | column_x];
    const int addr = (page << 14)
                     + (GetBlockIdPSMT4(block & 0x1F, x & 0x7F, y & 0x7F) << 9)
                     + column;
    return addr;
}

Tex0 DecodeTex0(uint64_t value)
{
    Tex0 t;
    t.tbp0 = (int)(value & 0x3FFF);
    t.tbw = (int)((value >> 14) & 0x3F);
    t.psm = (int)((value >> 20) & 0x3F);
    t.tw = (int)((value >> 26) & 0x0F);
    t.th = (int)((value >> 30) & 0x0F);
    t.cbp = (int)((value >> 37) & 0x3FFF);
    t.csa = (int)((value >> 56) & 0x1F);
    return t;
}

bool GetRawTextureRegion(const Tex0& tex0, int* out_addr, int* out_size)
{
    const int width = 1 << tex0.tw;
    const int height = 1 << tex0.th;
    int addr;
    int size;

    switch (tex0.psm)
    {
        case PSMZ32:
        case PSMCT32:
            addr = GetPixelAddressPSMCT32(tex0.tbp0, tex0.tbw, 0, 0);
            size = width * height * 4;
            break;

        case PSMCT16:
        case PSMCT16S:
        case PSMZ16:
        case PSMZ16S:
            addr = GetPixelAddressPSMCT16(tex0.tbp0, tex0.tbw, 0, 0);
            size = width * height * 2;
            break;

        case PSMT8:
            addr = GetPixelAddressPSMT8(tex0.tbp0, tex0.tbw, 0, 0);
            size = width * height;
            break;

        case PSMT4:
            addr = GetPixelAddressPSMT4(tex0.tbp0, tex0.tbw, 0, 0) >> 1;
            size = (width * height + 1) >> 1;
            break;

        default:
            return false;
    }

    if (addr < 0 || size <= 0 || addr + size > kVramSize)
    {
        return false;
    }

    *out_addr = addr;
    *out_size = size;
    return true;
}

bool IsDecodableTexture(const Tex0& tex0)
{
    if ((1 << tex0.tw) > 4096 || (1 << tex0.th) > 4096)
    {
        return false;
    }

    switch (tex0.psm)
    {
        case PSMCT32:
        case PSMCT16:
        case PSMCT16S:
        case PSMT8:
        case PSMT4:
            return true;
        default:
            return false;
    }
}

unsigned char AdjustPS2Alpha(unsigned char alpha)
{
    if (alpha <= 127)
    {
        return alpha << 1;
    }

    return 0xFF;
}

void MIKUPAN_GS_HOT ConvertImageAlpha(unsigned char* rgba, int width,
                                      int height)
{
    const size_t count = (size_t)width * (size_t)height;

    for (size_t i = 0; i < count; i++)
    {
        rgba[i * 4 + 3] = AdjustPS2Alpha(rgba[i * 4 + 3]);
    }
}

GSHelper::GSHelper()
{
    mem_.resize(kVramAllocSize);
}

bool GSHelper::Upload(int dpsm, int dbp, int dbw, int dsax, int dsay, int rrw,
                      int rrh, const uint8_t* inbuf)
{
    switch (dpsm)
    {
        case PSMZ32:
        case PSMCT32:
            UploadPSMCT32(dbp, dbw, dsax, dsay, rrw, rrh, inbuf);
            return true;
        case PSMZ16:
        case PSMZ16S:
        case PSMCT16:
        case PSMCT16S:
            UploadPSMCT16(dbp, dbw, dsax, dsay, rrw, rrh, inbuf);
            return true;
        case PSMT4:
            UploadPSMT4(dbp, dbw, dsax, dsay, rrw, rrh, inbuf);
            return true;
        case PSMT8:
            UploadPSMT8(dbp, dbw, dsax, dsay, rrw, rrh, inbuf);
            return true;
        default:
            return false;
    }
}

void MIKUPAN_GS_HOT GSHelper::UploadPSMCT32(int dbp, int dbw, int dsax,
                                            int dsay, int rrw, int rrh,
                                            const uint8_t *inbuf)
{
    int src_addr = 0;

    for (int y = dsay; y < dsay + rrh; ++y)
    {
        for (int x = dsax; x < dsax + rrw; ++x)
        {
            const int addr = GetPixelAddressPSMCT32(dbp, dbw, x, y);
            mem_[addr + 0x00] = inbuf[src_addr + 0x00];
            mem_[addr + 0x01] = inbuf[src_addr + 0x01];
            mem_[addr + 0x02] = inbuf[src_addr + 0x02];
            mem_[addr + 0x03] = inbuf[src_addr + 0x03];
            src_addr += 0x04;
        }
    }
}

void MIKUPAN_GS_HOT GSHelper::UploadPSMCT16(int dbp, int dbw, int dsax,
                                            int dsay, int rrw, int rrh,
                                            const uint8_t *inbuf)
{
    int src_addr = 0;

    for (int y = dsay; y < dsay + rrh; ++y)
    {
        for (int x = dsax; x < dsax + rrw; ++x)
        {
            const int addr = GetPixelAddressPSMCT16(dbp, dbw, x, y);
            mem_[addr + 0x00] = inbuf[src_addr + 0x00];
            mem_[addr + 0x01] = inbuf[src_addr + 0x01];
            src_addr += 0x02;
        }
    }
}

void MIKUPAN_GS_HOT GSHelper::UploadPSMT8(int dbp, int dbw, int dsax, int dsay,
                                          int rrw, int rrh,
                                          const uint8_t *inbuf)
{
    int src_addr = 0;

    for (int y = dsay; y < dsay + rrh; ++y)
    {
        for (int x = dsax; x < dsax + rrw; ++x)
        {
            const int addr =
                GetPixelAddressPSMT8(dbp, dbw, x, y) & kVramAllocMask;
            mem_[addr] = inbuf[src_addr++];
        }
    }
}

void MIKUPAN_GS_HOT GSHelper::UploadPSMT4(int dbp, int dbw, int dsax, int dsay,
                                          int rrw, int rrh,
                                          const uint8_t *inbuf)
{
    int src_addr = 0;

    for (int y = dsay; y < dsay + rrh; ++y)
    {
        for (int x = dsax; x < dsax + rrw; ++x)
        {
            const int addr = GetPixelAddressPSMT4(dbp, dbw, x, y)
                             & (2 * kVramAllocSize - 1);
            const int src_nibble = (inbuf[src_addr >> 1] >> ((src_addr & 0x01) << 2)) & 0x0F;
            mem_[addr >> 1] = (src_nibble << ((addr & 0x01) << 2)) | (mem_[addr >> 1] & (0xF0 >> ((addr & 0x01) << 2)));
            src_addr++;
        }
    }
}

void MIKUPAN_GS_HOT GSHelper::DownloadPSMCT32(unsigned char* outbuf, int dbp,
                                              int dbw, int dsax, int dsay,
                                              int rrw, int rrh) const
{
    int dst_addr = 0;

    for (int y = dsay; y < dsay + rrh; ++y)
    {
        for (int x = dsax; x < dsax + rrw; ++x)
        {
            const int addr = GetPixelAddressPSMCT32(dbp, dbw, x, y);
            outbuf[dst_addr + 0x00] = mem_[addr + 0x00];
            outbuf[dst_addr + 0x01] = mem_[addr + 0x01];
            outbuf[dst_addr + 0x02] = mem_[addr + 0x02];
            outbuf[dst_addr + 0x03] = mem_[addr + 0x03];
            dst_addr += 0x04;
        }
    }
}

void MIKUPAN_GS_HOT GSHelper::DownloadPSMCT16(unsigned char* outbuf, int dbp,
                                              int dbw, int dsax, int dsay,
                                              int rrw, int rrh) const
{
    int dst_addr = 0;

    for (int y = dsay; y < dsay + rrh; ++y)
    {
        for (int x = dsax; x < dsax + rrw; ++x)
        {
            const int addr = GetPixelAddressPSMCT16(dbp, dbw, x, y);
            const uint16_t p =
                (uint16_t)(unsigned char)mem_[addr + 0x00] |
                ((uint16_t)(unsigned char)mem_[addr + 0x01] << 8);
            ExpandPSMCT16Pixel(p, &outbuf[dst_addr]);
            dst_addr += 0x04;
        }
    }
}

void MIKUPAN_GS_HOT GSHelper::StorePSMCT16(unsigned char* outbuf, int dbp,
                                           int dbw, int dsax, int dsay,
                                           int rrw, int rrh) const
{
    int dst_addr = 0;

    for (int y = dsay; y < dsay + rrh; ++y)
    {
        for (int x = dsax; x < dsax + rrw; ++x)
        {
            const int addr = GetPixelAddressPSMCT16(dbp, dbw, x, y);
            outbuf[dst_addr + 0x00] = mem_[addr + 0x00];
            outbuf[dst_addr + 0x01] = mem_[addr + 0x01];
            dst_addr += 0x02;
        }
    }
}

void MIKUPAN_GS_HOT GSHelper::DownloadImagePSMT8(unsigned char* outbuf,
                                                 int dbp, int dbw, int dsax,
                                                 int dsay, int rrw, int rrh,
                                                 int cbp, int cbw,
                                                 int alpha_reg) const
{
    int dst_addr = 0;

    for (int y = dsay; y < dsay + rrh; ++y)
    {
        for (int x = dsax; x < dsax + rrw; ++x)
        {
            const int addr =
                GetPixelAddressPSMT8(dbp, dbw, x, y) & kVramAllocMask;
            const int clut_index = mem_[addr];

            /* CSM1 stores a 256-colour CLUT as a 16x16 CT32 block whose
             * index bits 3 and 4 are swapped. */
            int cy = (clut_index & 0xE0) >> 4;
            int cx = clut_index & 0x07;
            if (clut_index & 0x08)
                cy++;
            if (clut_index & 0x10)
                cx += 8;

            const int p = GetPixelAddressPSMCT32(cbp, cbw, cx, cy);
            outbuf[dst_addr + 0x00] = mem_[p + 0x00];
            outbuf[dst_addr + 0x01] = mem_[p + 0x01];
            outbuf[dst_addr + 0x02] = mem_[p + 0x02];

            if (alpha_reg >= 0)
            {
                outbuf[dst_addr + 0x03] = alpha_reg;
            }
            else
            {
                const unsigned char src_alpha =
                    (unsigned char)mem_[p + 0x03];
                outbuf[dst_addr + 0x03] = src_alpha;
            }

            dst_addr += 4;
        }
    }
}

void MIKUPAN_GS_HOT GSHelper::DownloadImagePSMT4(unsigned char* outbuf,
                                                 int dbp, int dbw, int dsax,
                                                 int dsay, int rrw, int rrh,
                                                 int cbp, int cbw, int csa,
                                                 int alpha_reg) const
{
    int dst_addr = 0;

    for (int y = dsay; y < dsay + rrh; ++y)
    {
        for (int x = dsax; x < dsax + rrw; ++x)
        {
            const int addr = GetPixelAddressPSMT4(dbp, dbw, x, y)
                             & (2 * kVramAllocSize - 1);
            const int clut_index = (mem_[addr >> 1] >> ((addr & 0x01) << 2)) & 0x0F;

            /* A 16-colour CLUT is 8x2 entries; CSA picks one of the 16
             * such CLUTs inside the 16x16 CLUT block. */
            const int cy = ((clut_index >> 3) & 0x01) + (csa & 0x0E);
            const int cx = (clut_index & 0x07) + ((csa & 0x01) << 3);

            const int p = GetPixelAddressPSMCT32(cbp, cbw, cx, cy);
            outbuf[dst_addr + 0x00] = mem_[p + 0x00];
            outbuf[dst_addr + 0x01] = mem_[p + 0x01];
            outbuf[dst_addr + 0x02] = mem_[p + 0x02];

            /* Keep alpha_reg as int: -1 is the "use CLUT alpha" sentinel, and
             * plain char is unsigned on Android/ARM. */
            if (alpha_reg >= 0)
            {
                outbuf[dst_addr + 0x03] = alpha_reg;
            }
            else
            {
                const unsigned char src_alpha =
                    (unsigned char)mem_[p + 0x03];
                outbuf[dst_addr + 0x03] = src_alpha;
            }

            dst_addr += 4;
        }
    }
}

bool GSHelper::DecodeTexture(const Tex0& tex0, unsigned char* outbuf) const
{
    if (!IsDecodableTexture(tex0))
    {
        return false;
    }

    const int width = 1 << tex0.tw;
    const int height = 1 << tex0.th;

    switch (tex0.psm)
    {
        case PSMCT32:
            DownloadPSMCT32(outbuf, tex0.tbp0, tex0.tbw, 0, 0, width, height);
            break;

        case PSMCT16:
        case PSMCT16S:
            DownloadPSMCT16(outbuf, tex0.tbp0, tex0.tbw, 0, 0, width, height);
            break;

        case PSMT4:
            DownloadImagePSMT4(outbuf, tex0.tbp0, tex0.tbw, 0, 0, width,
                               height, tex0.cbp, tex0.tbw, tex0.csa, -1);
            break;

        case PSMT8:
            DownloadImagePSMT8(outbuf, tex0.tbp0, tex0.tbw, 0, 0, width,
                               height, tex0.cbp, tex0.tbw, -1);
            break;

        default:
            return false;
    }

    ConvertImageAlpha(outbuf, width, height);
    return true;
}

void GSHelper::Clear()
{
    memset(mem_.data(), 0, mem_.size() * sizeof(char));
}

}// namespace GS
