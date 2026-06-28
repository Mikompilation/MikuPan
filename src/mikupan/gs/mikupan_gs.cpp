#include "mikupan_gs.h"
#include "SDL3/SDL_timer.h"

#include "mikupan_texture_manager.h"
#include "spdlog/spdlog.h"
#include "xxhash.h"

#include <atomic>
#include <cstring>

extern "C" {
#include "mikupan/ui/mikupan_ui.h"
#include "mikupan/mikupan_utils.h"
#include "mikupan/ui/mikupan_ui_debug.h"
}

GS::GSHelper gsHelper;
std::vector<uint8_t> texture_buffer = std::vector<uint8_t>(4096 * 4096 * 8);

struct GsFrameMetrics
{
    int    upload_count;
    int    upload_bytes;
    double upload_ms;
    int    download_count;
    int    download_bytes;
    double download_ms;
};

static GsFrameMetrics g_gs_frame_metrics = {0, 0, 0.0, 0, 0, 0.0};
static GsFrameMetrics g_gs_last_frame    = {0, 0, 0.0, 0, 0, 0.0};

struct GsUploadRegion { int addr; int size; };
static std::vector<GsUploadRegion> g_pending_uploads;

int MIKUPAN_OPTIMIZE_O3 GetBlockIdPSMCT32(int block, int x, int y)
{
    const int block_y = (y >> 3) & 0x03;
    const int block_x = (x >> 3) & 0x07;
    return block + ((x >> 1) & ~0x1F)
           + GS::kBlockTablePSMCT32[(block_y << 3) | block_x];
}

int MIKUPAN_OPTIMIZE_O3 GetPixelAddressPSMCT32(int block, int width, int x, int y)
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

int MIKUPAN_OPTIMIZE_O3 GetBlockIdPSMCT16(int block, int x, int y)
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

int MIKUPAN_OPTIMIZE_O3 GetPixelAddressPSMCT16(int block, int width, int x, int y)
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

int MIKUPAN_OPTIMIZE_O3 GetBlockIdPSMT8(int block, int x, int y)
{
    const int block_y = (y >> 4) & 0x03;
    const int block_x = (x >> 4) & 0x07;
    return block + ((x >> 2) & ~0x1F)
           + GS::kBlockTablePSMT8[(block_y << 3) | block_x];
}

int MIKUPAN_OPTIMIZE_O3 GetPixelAddressPSMT8(int block, int width, int x, int y)
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

int  MIKUPAN_OPTIMIZE_O3 GetBlockIdPSMT4(int block, int x, int y)
{
    const int block_base = ((y >> 6) & 0x01) << 4;
    const int block_y = (y >> 4) & 0x03;
    const int block_x = (x >> 5) & 0x03;
    return block + ((x >> 2) & ~0x1F) + block_base
           + GS::kBlockTablePSMT4[(block_y << 2) | block_x];
}

int MIKUPAN_OPTIMIZE_O3 GetPixelAddressPSMT4(int block, int width, int x, int y)
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

static int MikuPan_GetRawTextureRegion(sceGsTex0 *tex0, int *out_addr, int *out_size)
{
    int width;
    int height;
    int addr;
    int size;

    if (tex0 == nullptr || out_addr == nullptr || out_size == nullptr)
    {
        return 0;
    }

    width = 1 << tex0->TW;
    height = 1 << tex0->TH;

    switch (static_cast<PixelStorageFormat>(tex0->PSM))
    {
        case PSMZ32:
        case PSMCT32:
            addr = GetPixelAddressPSMCT32(tex0->TBP0, tex0->TBW, 0, 0);
            size = width * height * 4;
            break;

        case PSMCT16:
        case PSMCT16S:
        case PSMZ16:
        case PSMZ16S:
            addr = GetPixelAddressPSMCT16(tex0->TBP0, tex0->TBW, 0, 0);
            size = width * height * 2;
            break;

        case PSMT8:
            addr = GetPixelAddressPSMT8(tex0->TBP0, tex0->TBW, 0, 0);
            size = width * height;
            break;

        case PSMT4:
            addr = GetPixelAddressPSMT4(tex0->TBP0, tex0->TBW, 0, 0) >> 1;
            size = (width * height + 1) >> 1;
            break;

        default:
            return 0;
    }

    if (addr < 0 || size <= 0 || addr + size > static_cast<int>(gsHelper.mem_.size()))
    {
        return 0;
    }

    *out_addr = addr;
    *out_size = size;
    return 1;
}

static int MikuPan_GetClutRegion(sceGsTex0 *tex0, int *out_addr, int *out_size)
{
    int min_addr;
    int max_addr;

    if (tex0 == nullptr || out_addr == nullptr || out_size == nullptr)
    {
        return 0;
    }

    if (tex0->PSM != PSMT8 && tex0->PSM != PSMT4)
    {
        return 0;
    }

    min_addr = static_cast<int>(gsHelper.mem_.size());
    max_addr = 0;

    // MikuPan's indexed texture decoder samples a 16x16 PSMCT32 CLUT page.
    // Include that page in hashes and invalidation so black/white CLUT swaps
    // cannot keep reusing a stale color GL texture.
    for (int cy = 0; cy < 16; cy++)
    {
        for (int cx = 0; cx < 16; cx++)
        {
            const int addr = GetPixelAddressPSMCT32(tex0->CBP, tex0->TBW, cx, cy);

            if (addr < min_addr)
            {
                min_addr = addr;
            }

            if (addr + 4 > max_addr)
            {
                max_addr = addr + 4;
            }
        }
    }

    if (min_addr < 0 || max_addr <= min_addr ||
        max_addr > static_cast<int>(gsHelper.mem_.size()))
    {
        return 0;
    }

    *out_addr = min_addr;
    *out_size = max_addr - min_addr;
    return 1;
}

static int MikuPan_GetTextureInvalidationRegion(sceGsTex0 *tex0,
                                                int *out_addr,
                                                int *out_size)
{
    int tex_addr;
    int tex_size;
    int clut_addr;
    int clut_size;

    if (!MikuPan_GetRawTextureRegion(tex0, &tex_addr, &tex_size))
    {
        return 0;
    }

    if (MikuPan_GetClutRegion(tex0, &clut_addr, &clut_size))
    {
        const int tex_end = tex_addr + tex_size;
        const int clut_end = clut_addr + clut_size;
        const int region_addr = tex_addr < clut_addr ? tex_addr : clut_addr;
        const int region_end = tex_end > clut_end ? tex_end : clut_end;

        *out_addr = region_addr;
        *out_size = region_end - region_addr;
        return 1;
    }

    *out_addr = tex_addr;
    *out_size = tex_size;
    return 1;
}

static int MikuPan_GetUploadRegion(sceGsLoadImage *image_load, int *out_addr, int *out_size)
{
    int width;
    int height;
    int addr;
    int size;

    if (image_load == nullptr || out_addr == nullptr || out_size == nullptr)
    {
        return 0;
    }

    width = static_cast<int>(image_load->trxreg.RRW);
    height = static_cast<int>(image_load->trxreg.RRH);

    switch (static_cast<PixelStorageFormat>(image_load->bitbltbuf.DPSM))
    {
        case PSMZ32:
        case PSMCT32:
            addr = GetPixelAddressPSMCT32(
                image_load->bitbltbuf.DBP,
                image_load->bitbltbuf.DBW,
                image_load->trxpos.DSAX,
                image_load->trxpos.DSAY);
            size = width * height * 4;
            break;

        case PSMCT16:
        case PSMCT16S:
        case PSMZ16:
        case PSMZ16S:
            addr = GetPixelAddressPSMCT16(
                image_load->bitbltbuf.DBP,
                image_load->bitbltbuf.DBW,
                image_load->trxpos.DSAX,
                image_load->trxpos.DSAY);
            size = width * height * 2;
            break;

        case PSMT8:
            addr = GetPixelAddressPSMT8(
                image_load->bitbltbuf.DBP,
                image_load->bitbltbuf.DBW,
                image_load->trxpos.DSAX,
                image_load->trxpos.DSAY);
            size = width * height;
            break;

        case PSMT4:
            addr = GetPixelAddressPSMT4(
                image_load->bitbltbuf.DBP,
                image_load->bitbltbuf.DBW,
                image_load->trxpos.DSAX,
                image_load->trxpos.DSAY) >> 1;
            size = (width * height + 1) >> 1;
            break;

        default:
            return 0;
    }

    if (addr < 0 || size <= 0 || addr + size > static_cast<int>(gsHelper.mem_.size()))
    {
        return 0;
    }

    *out_addr = addr;
    *out_size = size;
    return 1;
}

GS::GSHelper::GSHelper()
{
    mem_.resize(4 * 1024 * 1024);// 4 MB
}

void MIKUPAN_OPTIMIZE_O3 GS::GSHelper::UploadPSMCT32(int dbp, int dbw, int dsax, int dsay, int rrw,
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

void MIKUPAN_OPTIMIZE_O3 GS::GSHelper::UploadPSMCT16(int dbp, int dbw, int dsax, int dsay, int rrw,
                                 int rrh, const uint8_t *inbuf)
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

void MIKUPAN_OPTIMIZE_O3 GS::GSHelper::UploadPSMT8(int dbp, int dbw, int dsax, int dsay, int rrw,
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

void MIKUPAN_OPTIMIZE_O3 GS::GSHelper::UploadPSMT4(int dbp, int dbw, int dsax, int dsay, int rrw,
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

void MIKUPAN_OPTIMIZE_O3 GS::GSHelper::DownloadPSMCT32(unsigned char* outbuf, int dbp, int dbw, int dsax,
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

static inline void ExpandPSMCT16Pixel(uint16_t p, unsigned char *out)
{
    const unsigned char r5 = (unsigned char)(p & 0x1F);
    const unsigned char g5 = (unsigned char)((p >> 5) & 0x1F);
    const unsigned char b5 = (unsigned char)((p >> 10) & 0x1F);

    out[0] = (unsigned char)((r5 << 3) | (r5 >> 2));
    out[1] = (unsigned char)((g5 << 3) | (g5 >> 2));
    out[2] = (unsigned char)((b5 << 3) | (b5 >> 2));
    out[3] = (p & 0x8000) ? 0x80 : 0x00;
}

void MIKUPAN_OPTIMIZE_O3 GS::GSHelper::DownloadPSMCT16(unsigned char* outbuf, int dbp, int dbw, int dsax,
                                                   int dsay, int rrw, int rrh)
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

void MIKUPAN_OPTIMIZE_O3 GS::GSHelper::StorePSMCT16(unsigned char* outbuf, int dbp, int dbw, int dsax,
                                                int dsay, int rrw, int rrh)
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

void MIKUPAN_OPTIMIZE_O3 GS::GSHelper::DownloadImagePSMT8(unsigned char* outbuf, int dbp, int dbw,
                                                      int dsax, int dsay,
                                                      int rrw, int rrh, int cbp,
                                                      int cbw, int alpha_reg)
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
                const unsigned char src_alpha =
                    (unsigned char)mem_[p + 0x03];
                outbuf[dst_addr + 0x03] = src_alpha;
            }

            dst_addr += 4;
        }
    }
}

void MIKUPAN_OPTIMIZE_O3 GS::GSHelper::DownloadImagePSMT4(unsigned char* outbuf, int dbp, int dbw,
                                                      int dsax, int dsay,
                                                      int rrw, int rrh, int cbp,
                                                      int cbw, int csa,
                                                      int alpha_reg)
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

void MikuPan_GsUpload(sceGsLoadImage *image_load, unsigned char *image)
{
    if (MikuPan_IsGsUploadsDisabled())
    {
        return;
    }

    SPDLOG_DEBUG("GS upload request for DBP {:#x} DPSM {} X: {} Y: {}",
                 static_cast<int>(image_load->bitbltbuf.DBP),
                 static_cast<int>(image_load->bitbltbuf.DPSM),
                 static_cast<int>(image_load->trxreg.RRW),
                 static_cast<int>(image_load->trxreg.RRH));

    MikuPan_FirstUploadDone();

    {
        int up_addr;
        int up_size;

        if (MikuPan_GetUploadRegion(image_load, &up_addr, &up_size))
        {
            MikuPan_TextureCache_InvalidateGsRange(up_addr, up_size);
            g_pending_uploads.push_back({up_addr, up_size});
        }
    }

    Uint64 t0 = SDL_GetPerformanceCounter();
    int upload_w = static_cast<int>(image_load->trxreg.RRW);
    int upload_h = static_cast<int>(image_load->trxreg.RRH);

    switch (static_cast<PixelStorageFormat>(image_load->bitbltbuf.DPSM))
    {
        case PSMZ32:
        case PSMCT32:
            gsHelper.UploadPSMCT32(
                image_load->bitbltbuf.DBP, image_load->bitbltbuf.DBW,
                image_load->trxpos.DSAX, image_load->trxpos.DSAY,
                image_load->trxreg.RRW, image_load->trxreg.RRH, image);
            break;
        case PSMZ16:
        case PSMZ16S:
        case PSMCT16:
        case PSMCT16S:
            gsHelper.UploadPSMCT16(
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

    Uint64 t1 = SDL_GetPerformanceCounter();
    g_gs_frame_metrics.upload_count++;
    g_gs_frame_metrics.upload_bytes += upload_w * upload_h * 4; // approximate
    g_gs_frame_metrics.upload_ms += (double)(t1 - t0) * 1000.0 / (double)SDL_GetPerformanceFrequency();
}

unsigned char *MikuPan_GsDownloadTexture(sceGsTex0 *tex0, uint64_t* hash)
{
    if (!MikuPan_IsFirstUploadDone())
    {
        return nullptr;
    }

    int width = (1 << tex0->TW);
    int height = (1 << tex0->TH);

    if (width > 4096 || height > 4096)
    {
        return nullptr;
    }

    SPDLOG_DEBUG("GS download request for DBP {:#x} CBP {:#x} DPSM {} ",
                 static_cast<int>(tex0->TBP0),
                 static_cast<int>(tex0->CBP),
                 static_cast<int>(tex0->PSM));

    *hash = MikuPan_GetTextureHash(tex0);
    auto texture = MikuPan_GetTextureInfo(*hash);
    if (texture != nullptr)
    {
        return nullptr;
    }

    Uint64 t0 = SDL_GetPerformanceCounter();

    switch (tex0->PSM)
    {
        case PSMCT32:
            gsHelper.DownloadPSMCT32(
                texture_buffer.data(),
                tex0->TBP0, tex0->TBW,
                0, 0, width, height);
            break;

        case PSMCT16:
        case PSMCT16S:
            gsHelper.DownloadPSMCT16(
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
            SPDLOG_DEBUG(
                "Texture Transfer Upload Failed, Unsupported Format: DPSM {:#x}", static_cast<int>(tex0->PSM));
            *hash = 0;
            return nullptr;
    }

    unsigned char* image_data = MikuPan_ConvertImageAlpha(texture_buffer.data(), width, height);

    Uint64 t1 = SDL_GetPerformanceCounter();
    g_gs_frame_metrics.download_count++;
    g_gs_frame_metrics.download_bytes += width * height * 4;
    g_gs_frame_metrics.download_ms += (double)(t1 - t0) * 1000.0 / (double)SDL_GetPerformanceFrequency();

    return image_data;
}

uint64_t MikuPan_GetTextureHash(sceGsTex0 *tex0)
{
    int addr;
    int size;
    int clut_addr;
    int clut_size;

    if (!MikuPan_IsFirstUploadDone())
    {
        return 0;
    }

    if (!MikuPan_GetRawTextureRegion(tex0, &addr, &size))
    {
        return 0;
    }

    XXH64_hash_t hash = XXH3_64bits(&gsHelper.mem_[addr], size);

    if (MikuPan_GetClutRegion(tex0, &clut_addr, &clut_size))
    {
        uint64_t tex0_value = 0;
        const XXH64_hash_t clut_hash =
            XXH3_64bits(&gsHelper.mem_[clut_addr], clut_size);
        memcpy(&tex0_value, tex0, sizeof(tex0_value));
        const uint64_t parts[3] = {
            tex0_value,
            static_cast<uint64_t>(hash),
            static_cast<uint64_t>(clut_hash),
        };
        hash = XXH3_64bits(parts, sizeof(parts));
    }

    return hash;
}

extern "C"
{

void MikuPan_GsResetFrameMetrics(void)
{
    g_gs_last_frame = g_gs_frame_metrics;
    g_gs_frame_metrics.upload_count   = 0;
    g_gs_frame_metrics.upload_bytes   = 0;
    g_gs_frame_metrics.upload_ms      = 0.0;
    g_gs_frame_metrics.download_count = 0;
    g_gs_frame_metrics.download_bytes = 0;
    g_gs_frame_metrics.download_ms    = 0.0;
}

int    MikuPan_GsGetUploadCount(void)    { return g_gs_last_frame.upload_count; }
int    MikuPan_GsGetUploadBytes(void)    { return g_gs_last_frame.upload_bytes; }
float  MikuPan_GsGetUploadMs(void)       { return (float)g_gs_last_frame.upload_ms; }
int    MikuPan_GsGetDownloadCount(void)  { return g_gs_last_frame.download_count; }
int    MikuPan_GsGetDownloadBytes(void)  { return g_gs_last_frame.download_bytes; }
float  MikuPan_GsGetDownloadMs(void)     { return (float)g_gs_last_frame.download_ms; }

int MikuPan_GsHasPendingUploads(void)
{
    return g_pending_uploads.empty() ? 0 : 1;
}

void MikuPan_GsConsumePendingUploads(
    void (*cb)(int addr, int size, void *ud), void *ud)
{
    if (cb != NULL)
    {
        for (const auto &r : g_pending_uploads)
        {
            cb(r.addr, r.size, ud);
        }
    }

    g_pending_uploads.clear();
}

void MikuPan_GetTextureGsRegion(sceGsTex0 *tex0, int *out_addr, int *out_size)
{
    if (!MikuPan_GetTextureInvalidationRegion(tex0, out_addr, out_size))
    {
        *out_addr = 0;
        *out_size = 0;
    }
}

void MikuPan_GsStore(sceGsStoreImage *sp, unsigned char *out)
{
    const int sbp  = static_cast<int>(sp->bitbltbuf.SBP);
    const int sbw  = static_cast<int>(sp->bitbltbuf.SBW);
    const int spsm = static_cast<int>(sp->bitbltbuf.SPSM);

    const int x = static_cast<int>(sp->trxpos.SSAX);
    const int y = static_cast<int>(sp->trxpos.SSAY);
    const int w = static_cast<int>(sp->trxreg.RRW);
    const int h = static_cast<int>(sp->trxreg.RRH);

    switch (static_cast<PixelStorageFormat>(spsm))
    {
        case PSMZ32:
        case PSMCT32:
            gsHelper.DownloadPSMCT32(out, sbp, sbw, x, y, w, h);
            break;

        case PSMZ16:
        case PSMZ16S:
        case PSMCT16:
        case PSMCT16S:
            gsHelper.StorePSMCT16(out, sbp, sbw, x, y, w, h);
            break;

        default:
            spdlog::warn("Unsupported sceGsExecStoreImage SPSM: {:#x}", spsm);
            memset(out, 0, w * h * 4);
            break;
    }
}

}
