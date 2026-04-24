#include "mikupan_gs.h"
#include "mikupan_texture_manager.h"
#include "spdlog/spdlog.h"
#include "xxhash.h"
#include <cstring>

extern "C" {
#include "mikupan/mikupan_utils.h"
#include "mikupan/rendering/mikupan_renderer.h"
}

GS::GSHelper gsHelper;
std::vector<uint8_t> texture_buffer = std::vector<uint8_t>(4096 * 4096 * 4);

int __attribute__((optimize("O3"))) GetBlockIdPSMCT32(int block, int x, int y)
{
    const int block_y = (y >> 3) & 0x03;
    const int block_x = (x >> 3) & 0x07;
    return block + ((x >> 1) & ~0x1F)
           + GS::kBlockTablePSMCT32[(block_y << 3) | block_x];
}

int __attribute__((optimize("O3"))) GetPixelAddressPSMCT32(int block, int width, int x, int y)
{
    const int page = (block >> 5) + (y >> 5) * width + (x >> 6);
    const int column_base = ((y >> 1) & 0x03) << 4;
    const int column_y = y & 0x01;
    const int column_x = x & 0x07;
    const int column =
        column_base + GS::kColumnTablePSMCT32[(column_y << 3) | column_x];
    const int addr =
        ((page << 11)
         + (GetBlockIdPSMCT32(block & 0x1F, x & 0x3F, y & 0x1F) << 6) + column);
    return (addr << 2) & 0x003FFFFC;
}

int __attribute__((optimize("O3"))) GetBlockIdPSMT8(int block, int x, int y)
{
    const int block_y = (y >> 4) & 0x03;
    const int block_x = (x >> 4) & 0x07;
    return block + ((x >> 2) & ~0x1F)
           + GS::kBlockTablePSMT8[(block_y << 3) | block_x];
}

int __attribute__((optimize("O3"))) GetPixelAddressPSMT8(int block, int width, int x, int y)
{
    const int page = (block >> 5) + (y >> 6) * (width >> 1) + (x >> 7);
    const int column_y = y & 0x0F;
    const int column_x = x & 0x0F;
    const int column = GS::kColumnTablePSMT8[(column_y << 4) | column_x];
    const int addr = (page << 13)
                     + (GetBlockIdPSMT8(block & 0x1F, x & 0x7F, y & 0x3F) << 8)
                     + column;
    return addr;
}

int  __attribute__((optimize("O3"))) GetBlockIdPSMT4(int block, int x, int y)
{
    const int block_base = ((y >> 6) & 0x01) << 4;
    const int block_y = (y >> 4) & 0x03;
    const int block_x = (x >> 5) & 0x03;
    return block + ((x >> 2) & ~0x1F) + block_base
           + GS::kBlockTablePSMT4[(block_y << 2) | block_x];
}

int __attribute__((optimize("O3"))) GetPixelAddressPSMT4(int block, int width, int x, int y)
{
    const int page = ((block >> 5) + (y >> 7) * (width >> 1) + (x >> 7));
    const int column_y = y & 0x0F;
    const int column_x = x & 0x1F;
    const int column = GS::kColumnTablePSMT4[(column_y << 5) | column_x];
    const int addr = (page << 14)
                     + (GetBlockIdPSMT4(block & 0x1F, x & 0x7F, y & 0x7F) << 9)
                     + column;
    return addr;
}

GS::GSHelper::GSHelper()
{
    mem_.resize(4 * 1024 * 1024);// 4 MB
}

void __attribute__((optimize("O3"))) GS::GSHelper::UploadPSMCT32(int dbp, int dbw, int dsax, int dsay, int rrw,
                                 int rrh, const uint8_t *inbuf)
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

void __attribute__((optimize("O3"))) GS::GSHelper::UploadPSMT8(int dbp, int dbw, int dsax, int dsay, int rrw,
                               int rrh, const uint8_t *inbuf)
{
    int src_addr = 0;

    for (int y = dsay; y < dsay + rrh; ++y)
    {
        for (int x = dsax; x < dsax + rrw; ++x)
        {
            const int addr = GetPixelAddressPSMT8(dbp, dbw, x, y);
            mem_[addr] = inbuf[src_addr++];
        }
    }
}

void __attribute__((optimize("O3"))) GS::GSHelper::UploadPSMT4(int dbp, int dbw, int dsax, int dsay, int rrw,
                               int rrh, const uint8_t *inbuf)
{
    int src_addr = 0;

    for (int y = dsay; y < dsay + rrh; ++y)
    {
        for (int x = dsax; x < dsax + rrw; ++x)
        {
            const int addr = GetPixelAddressPSMT4(dbp, dbw, x, y);
            const int src_nibble = (inbuf[src_addr >> 1] >> ((src_addr & 0x01) << 2)) & 0x0F;
            mem_[addr >> 1] = (src_nibble << ((addr & 0x01) << 2)) | (mem_[addr >> 1] & (0xF0 >> ((addr & 0x01) << 2)));
            src_addr++;
        }
    }
}

void __attribute__((optimize("O3"))) GS::GSHelper::DownloadPSMCT32(unsigned char* outbuf, int dbp, int dbw, int dsax,
                                                   int dsay, int rrw, int rrh)
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

void __attribute__((optimize("O3"))) GS::GSHelper::DownloadImagePSMT8(unsigned char* outbuf, int dbp, int dbw,
                                                      int dsax, int dsay,
                                                      int rrw, int rrh, int cbp,
                                                      int cbw, char alpha_reg)
{
    int dst_addr = 0;

    for (int y = dsay; y < dsay + rrh; ++y)
    {
        for (int x = dsax; x < dsax + rrw; ++x)
        {
            const int addr = GetPixelAddressPSMT8(dbp, dbw, x, y);
            const int clut_index = mem_[addr];

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
                const char src_alpha = mem_[p + 0x03];
                outbuf[dst_addr + 0x03] = src_alpha;
            }

            dst_addr += 4;
        }
    }
}

void __attribute__((optimize("O3"))) GS::GSHelper::DownloadImagePSMT4(unsigned char* outbuf, int dbp, int dbw,
                                                      int dsax, int dsay,
                                                      int rrw, int rrh, int cbp,
                                                      int cbw, int csa,
                                                      char alpha_reg)
{
    int dst_addr = 0;

    for (int y = dsay; y < dsay + rrh; ++y)
    {
        for (int x = dsax; x < dsax + rrw; ++x)
        {
            const int addr = GetPixelAddressPSMT4(dbp, dbw, x, y);
            const int clut_index = (mem_[addr >> 1] >> ((addr & 0x01) << 2)) & 0x0F;

            const int cy = ((clut_index >> 3) & 0x01) + (csa & 0x0E);
            const int cx = (clut_index & 0x07) + ((csa & 0x01) << 3);

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
                const char src_alpha = mem_[p + 0x03];
                outbuf[dst_addr + 0x03] = src_alpha;// >= 0 ? (src_alpha << 1) : 0xFF;
            }

            dst_addr += 4;
        }
    }
}

void GS::GSHelper::Clear()
{
    memset(mem_.data(), 0, mem_.size() * sizeof(char));
}

// Evict cached GPU textures whose texture data lives at block address dbp.
//
// Only TBP0 is checked — CBP (palette pointer) is intentionally excluded.
// A GS upload at dbp overwrites texture pixel data (addressed by TBP0), so
// any cached texture at that block must be re-sampled.  CLUTs (CBP) are stored
// via GetPixelAddressPSMCT32 into the same memory, but a PSMCT32 data upload
// at an address that coincides with a paletted texture's CBP does not mean the
// palette was intentionally replaced — it is just as likely to be unrelated
// texture data landing at that block number.  Re-creating a paletted texture
// from a stale-but-correct pixel-data sample while the CLUT block was
// incidentally overwritten produces badly wrong colours, whereas keeping the
// cached texture one upload behind is a harmless visual artefact.
static void invalidate_textures_at_dbp(int dbp)
{
    auto it = mikupan_render_texture_atlas.begin();
    while (it != mikupan_render_texture_atlas.end())
    {
        MikuPan_TextureInfo *info = it->second;
        if (MikuPan_IsFontTexture(info))
        {
            ++it;
            continue;
        }
        const sceGsTex0 *tex0 = reinterpret_cast<const sceGsTex0 *>(&info->tex0);
        if (static_cast<int>(tex0->TBP0) == dbp)
        {
            MikuPan_DeleteTexture(info);
            it = mikupan_render_texture_atlas.erase(it);
        }
        else
        {
            ++it;
        }
    }
}

void MikuPan_GsUpload(sceGsLoadImage *image_load, unsigned char *image)
{
    spdlog::debug("GS upload request for DBP {:#x} DPSM {} X: {} Y: {}",
                  static_cast<int>(image_load->bitbltbuf.DBP),
                  static_cast<int>(image_load->bitbltbuf.DPSM),
                  static_cast<int>(image_load->trxreg.RRW),
                  static_cast<int>(image_load->trxreg.RRH));

    invalidate_textures_at_dbp(image_load->bitbltbuf.DBP);
    MikuPan_FirstUploadDone();

    switch (static_cast<PixelStorageFormat>(image_load->bitbltbuf.DPSM))
    {
        case PSMZ32:
        case PSMCT32:
            gsHelper.UploadPSMCT32(
                image_load->bitbltbuf.DBP, image_load->bitbltbuf.DBW,
                image_load->trxpos.DSAX, image_load->trxpos.DSAY,
                image_load->trxreg.RRW, image_load->trxreg.RRH, image);
            break;
        case PSMT4:
        {
            gsHelper.UploadPSMT4(
                image_load->bitbltbuf.DBP, image_load->bitbltbuf.DBW,
                image_load->trxpos.DSAX, image_load->trxpos.DSAY,
                image_load->trxreg.RRW, image_load->trxreg.RRH, image);
            break;
        }
        case PSMT8:
        {
            gsHelper.UploadPSMT8(
                image_load->bitbltbuf.DBP, image_load->bitbltbuf.DBW,
                image_load->trxpos.DSAX, image_load->trxpos.DSAY,
                image_load->trxreg.RRW, image_load->trxreg.RRH, image);
            break;
        }
        default:
            spdlog::info("Texture Transfer Upload Info FAILED: DPSM {:#x}", static_cast<int>(image_load->bitbltbuf.DPSM));
            break;
    }
}

unsigned char *MikuPan_GsDownloadTexture(sceGsTex0 *tex0, uint64_t* hash)
{
    if (!MikuPan_IsFirstUploadDone())
    {
        return nullptr;
    }

    int width = (1 << tex0->TW);
    int height = (1 << tex0->TH);

    spdlog::debug("GS download request for DBP {:#x} CBP {:#x} DPSM {} ",
                  static_cast<int>(tex0->TBP0),
                  static_cast<int>(tex0->CBP),
                  static_cast<int>(tex0->PSM));

    *hash = MikuPan_GetTextureHash(tex0);
    auto texture = MikuPan_GetTextureInfo(*hash);
    if (texture != nullptr)
    {
        return nullptr;
    }

    switch (tex0->PSM)
    {
        case PSMCT32:
            gsHelper.DownloadPSMCT32(
                texture_buffer.data(),
                tex0->TBP0, tex0->TBW,
                0, 0, width, height);
            break;

        case PSMT4:
            gsHelper.DownloadImagePSMT4(
                texture_buffer.data(),
                tex0->TBP0, tex0->TBW, 0, 0,
                width, height, tex0->CBP, tex0->TBW, tex0->CSA, -1);
            break;
        case PSMT8:
            gsHelper.DownloadImagePSMT8(
                texture_buffer.data(),
                tex0->TBP0, tex0->TBW, 0, 0,
                width, height, tex0->CBP, tex0->TBW, -1);
            break;
        default:
            spdlog::debug(
                "Texture Transfer Upload Failed, Unsupported Format: DPSM {:#x}", static_cast<int>(tex0->PSM));
            *hash = 0;
            return nullptr;
    }

    unsigned char* image_data = MikuPan_ConvertImageAlpha(texture_buffer.data(), width, height);

    return image_data;
}

uint64_t MikuPan_GetTextureHash(sceGsTex0 *tex0)
{
    if (!MikuPan_IsFirstUploadDone())
    {
        return 0;
    }

    // Hash strategy: tex0 register as seed + fixed-size GS memory sample at TBP0.
    //
    // Requirements:
    //   Stable  – same tex0 + same VRAM content  → same hash → cache hit.
    //   Unique  – different slot OR different content → different hash.
    //
    // Why not pure tex0 hash?
    //   Ignores VRAM content: when a slot is reused for a different texture
    //   (same TBP0/CBP/format) the hash is identical and the stale cached GPU
    //   texture is returned → wrong texture shown (e.g. font instead of noise).
    //
    // Why not pure content hash over width×height bytes?
    //   The window overruns the 4 MB GS boundary for textures at high VRAM
    //   addresses → bounds check returns hash=0 → no GPU texture is ever created
    //   → previous pipeline binding stays active → wrong texture shown.
    //
    // Why not CBP-based hash (original code)?
    //   CBP is the palette pointer, not the texture data pointer.  The sampled
    //   window at CBP is much larger than the actual palette and overlaps GS
    //   regions modified by other uploads every frame → new unique hash each
    //   frame → unbounded GPU texture allocation → 3+ GB memory leak.
    //
    // Combined approach: use the full 64-bit tex0 register as the XXH3 seed
    // (encodes TBP0, CBP, PSM, TW, TH, etc. — all slot identity fields) and
    // hash a small fixed-size sample of GS memory at TBP0 as a content
    // fingerprint.  Different logical textures always have different seeds even
    // if they happen to occupy the same VRAM address.  The 256-byte sample is
    // large enough to detect content changes on slot reuse, and small enough to
    // never exceed the 4 MB GS address space.
    uint64_t seed = *(const uint64_t *)tex0;

    // Resolve the byte address of pixel (0,0) using the format-appropriate
    // block layout.  MikuPan_GsUpload writes PSMT8 data with GetPixelAddressPSMT8
    // and PSMT4 data with GetPixelAddressPSMT4 — using PSMCT32 for those formats
    // samples bytes that were never written by their uploads (zeroes or stale
    // data from a different texture), breaking the content fingerprint.
    int addr;
    switch (static_cast<PixelStorageFormat>(tex0->PSM))
    {
        case PSMT8:
            addr = GetPixelAddressPSMT8(tex0->TBP0, tex0->TBW, 0, 0);
            break;
        case PSMT4:
            // GetPixelAddressPSMT4 returns a nibble index; shift to byte address.
            addr = GetPixelAddressPSMT4(tex0->TBP0, tex0->TBW, 0, 0) >> 1;
            break;
        default:    // PSMCT32, PSMCT24, PSMCT16, PSMZ32, …
            addr = GetPixelAddressPSMCT32(tex0->TBP0, tex0->TBW, 0, 0);
            break;
    }

    const int gs_size  = 4 * 1024 * 1024;
    const int max_samp = 256;

    if (addr < 0 || addr >= gs_size)
        return 0;

    int sample = (addr + max_samp <= gs_size) ? max_samp : (gs_size - addr);

    return XXH3_64bits_withSeed(&gsHelper.mem_[addr], sample, seed);
}