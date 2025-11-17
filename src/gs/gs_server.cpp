#include "gs_server.h"

#include "texture_manager.h"
#include "spdlog/spdlog.h"

#include <cstring>

GS::GSHelper gsHelper;

int GetBlockIdPSMCT32(int block, int x, int y) {
  const int block_y = (y >> 3) & 0x03;
  const int block_x = (x >> 3) & 0x07;
  return block + ((x >> 1) & ~0x1F) + GS::kBlockTablePSMCT32[(block_y << 3) | block_x];
}

int GetPixelAddressPSMCT32(int block, int width, int x, int y) {
  const int page = (block >> 5) + (y >> 5) * width + (x >> 6);
  const int column_base = ((y >> 1) & 0x03) << 4;
  const int column_y = y & 0x01;
  const int column_x = x & 0x07;
  const int column = column_base + GS::kColumnTablePSMCT32[(column_y << 3) | column_x];
  const int addr = ((page << 11) + (GetBlockIdPSMCT32(block & 0x1F, x & 0x3F, y & 0x1F) << 6) + column);
  return (addr << 2) & 0x003FFFFC;
}

int GetBlockIdPSMT8(int block, int x, int y) {
  const int block_y = (y >> 4) & 0x03;
  const int block_x = (x >> 4) & 0x07;
  return block + ((x >> 2) & ~0x1F) + GS::kBlockTablePSMT8[(block_y << 3) | block_x];

}

int GetPixelAddressPSMT8(int block, int width, int x, int y) {
  const int page = (block >> 5) + (y >> 6) * (width >> 1) + (x >> 7);
  const int column_y = y & 0x0F;
  const int column_x = x & 0x0F;
  const int column = GS::kColumnTablePSMT8[(column_y << 4) | column_x];
  const int addr = (page << 13) + (GetBlockIdPSMT8(block & 0x1F, x & 0x7F, y & 0x3F) << 8) + column;
  return addr;
}

int GetBlockIdPSMT4(int block, int x, int y) {
  const int block_base = ((y >> 6) & 0x01) << 4;
  const int block_y = (y >> 4) & 0x03;
  const int block_x = (x >> 5) & 0x03;
  return block + ((x >> 2) & ~0x1F) + block_base + GS::kBlockTablePSMT4[(block_y << 2) | block_x];
}

int GetPixelAddressPSMT4(int block, int width, int x, int y) {
  const int page = ((block >> 5) + (y >> 7) * (width >> 1) + (x >> 7));
  const int column_y = y & 0x0F;
  const int column_x = x & 0x1F;
  const int column = GS::kColumnTablePSMT4[(column_y << 5) | column_x];
  const int addr = (page << 14) + (GetBlockIdPSMT4(block & 0x1F, x & 0x7F, y & 0x7F) << 9) + column;
  return addr;
}

GS::GSHelper::GSHelper() {
  mem_.resize(4 * 1024 * 1024);  // 4 MB
}

void GS::GSHelper::UploadPSMCT32(int dbp, int dbw, int dsax, int dsay, int rrw, int rrh, const uint8_t* inbuf)
{
  int src_addr = 0;
  for (int y = dsay; y < dsay + rrh; ++y) {
    for (int x = dsax; x < dsax + rrw; ++x) {
      const int addr = GetPixelAddressPSMCT32(dbp, dbw, x, y);
      mem_[addr + 0x00] = inbuf[src_addr + 0x00];
      mem_[addr + 0x01] = inbuf[src_addr + 0x01];
      mem_[addr + 0x02] = inbuf[src_addr + 0x02];
      mem_[addr + 0x03] = inbuf[src_addr + 0x03];
      src_addr += 0x04;
    }
  }
}

void GS::GSHelper::UploadPSMT8(int dbp, int dbw, int dsax, int dsay, int rrw, int rrh, const uint8_t* inbuf) {
  int src_addr = 0;
  for (int y = dsay; y < dsay + rrh; ++y) {
    for (int x = dsax; x < dsax + rrw; ++x) {
      const int addr = GetPixelAddressPSMT8(dbp, dbw, x, y);
      mem_[addr] = inbuf[src_addr++];
    }
  }
}

void GS::GSHelper::UploadPSMT4(int dbp, int dbw, int dsax, int dsay, int rrw, int rrh, const uint8_t* inbuf) {
  int src_addr = 0;
  for (int y = dsay; y < dsay + rrh; ++y) {
    for (int x = dsax; x < dsax + rrw; ++x) {
      const int addr = GetPixelAddressPSMT4(dbp, dbw, x, y);
      const int src_nibble = (inbuf[src_addr >> 1] >> ((src_addr & 0x01) << 2)) & 0x0F;
      mem_[addr >> 1] = (src_nibble << ((addr & 0x01) << 2)) | (mem_[addr >> 1] & (0xF0 >> ((addr & 0x01) << 2)));
      src_addr++;
    }
  }
}

std::vector<uint8_t> GS::GSHelper::DownloadPSMCT32(int dbp, int dbw, int dsax, int dsay, int rrw, int rrh) {
  std::vector<uint8_t> outbuf(rrw * rrh * 4);
  int dst_addr = 0;
  for (int y = dsay; y < dsay + rrh; ++y) {
    for (int x = dsax; x < dsax + rrw; ++x) {
      const int addr = GetPixelAddressPSMCT32(dbp, dbw, x, y);
      outbuf[dst_addr + 0x00] = mem_[addr + 0x00];
      outbuf[dst_addr + 0x01] = mem_[addr + 0x01];
      outbuf[dst_addr + 0x02] = mem_[addr + 0x02];
      outbuf[dst_addr + 0x03] = mem_[addr + 0x03];
      dst_addr += 0x04;
    }
  }
  return outbuf;
}

std::vector<uint8_t> GS::GSHelper::DownloadPSMT8(int dbp, int dbw, int dsax, int dsay, int rrw, int rrh) {
  // Not implemented
  return std::vector<uint8_t>();
}

std::vector<uint8_t> GS::GSHelper::DownloadPSMT4(int dbp, int dbw, int dsax, int dsay, int rrw, int rrh) {
  // Not implemented
  return std::vector<uint8_t>();
}

std::vector<uint8_t> GS::GSHelper::DownloadImagePSMT8(int dbp, int dbw, int dsax, int dsay, int rrw, int rrh, int cbp, int cbw, char alpha_reg) {
  std::vector<uint8_t> outbuf(rrw * rrh * 4);
  int dst_addr = 0;
  for (int y = dsay; y < dsay + rrh; ++y) {
    for (int x = dsax; x < dsax + rrw; ++x) {
      const int addr = GetPixelAddressPSMT8(dbp, dbw, x, y);
      const int clut_index = mem_[addr];

      //int cx = clut_index % 16;
      //int cy = clut_index / 16;

      int cy = (clut_index & 0xE0) >> 4;
      int cx = clut_index & 0x07;
      if (clut_index & 0x08) cy++;
      if (clut_index & 0x10) cx += 8;

      const int p = GetPixelAddressPSMCT32(cbp, cbw, cx, cy);
      outbuf[dst_addr + 0x00] = mem_[p + 0x00];
      outbuf[dst_addr + 0x01] = mem_[p + 0x01];
      outbuf[dst_addr + 0x02] = mem_[p + 0x02];
      if (alpha_reg >= 0) {
        outbuf[dst_addr + 0x03] = alpha_reg;
      } else {
        const char src_alpha = mem_[p + 0x03];
        outbuf[dst_addr + 0x03] = src_alpha;
      }
      dst_addr += 4;
    }
  }
  return outbuf;
}

std::vector<uint8_t> GS::GSHelper::DownloadImagePSMT4(int dbp, int dbw, int dsax, int dsay, int rrw, int rrh, int cbp, int cbw, int csa, char alpha_reg) {
  std::vector<uint8_t> outbuf(rrw * rrh * 4);
  int dst_addr = 0;
  for (int y = dsay; y < dsay + rrh; ++y) {
    for (int x = dsax; x < dsax + rrw; ++x) {
      const int addr = GetPixelAddressPSMT4(dbp, dbw, x, y);
      const int clut_index = (mem_[addr >> 1] >> ((addr & 0x01) << 2)) & 0x0F;

      const int cy = ((clut_index >> 3) & 0x01) + (csa & 0x0E);
      const int cx = (clut_index & 0x07) + ((csa & 0x01) << 3);

      const int p = GetPixelAddressPSMCT32(cbp, cbw, cx, cy);
      outbuf[dst_addr + 0x00] = mem_[p + 0x00];
      outbuf[dst_addr + 0x01] = mem_[p + 0x01];
      outbuf[dst_addr + 0x02] = mem_[p + 0x02];
      if (alpha_reg >= 0) {
        outbuf[dst_addr + 0x03] = alpha_reg;
      } else {
        const char src_alpha = mem_[p + 0x03];
        outbuf[dst_addr + 0x03] = src_alpha;// >= 0 ? (src_alpha << 1) : 0xFF;
      }
      dst_addr += 4;
    }
  }
  return outbuf;
}

void GS::GSHelper::Clear() {
  memset(mem_.data(), 0, mem_.size() * sizeof(char));
}

void GsUpload(sceGsLoadImage* image_load, unsigned char* image)
{
    spdlog::debug("GS upload request for DBP {:#x} DPSM {} ", (unsigned long long)image_load->bitbltbuf.DBP, (unsigned long long)image_load->bitbltbuf.DPSM);

    FirstUploadDone();

    switch ((PixelStorageFormat)image_load->bitbltbuf.DPSM)
    {
        case PSMCT32:
            gsHelper.UploadPSMCT32(image_load->bitbltbuf.DBP,
                             image_load->bitbltbuf.DBW,
                             image_load->trxpos.DSAX,
                             image_load->trxpos.DSAY,
                             image_load->trxreg.RRW,
                             image_load->trxreg.RRH,
                             image);
            break;
        case PSMT4: {
            gsHelper.UploadPSMT4(image_load->bitbltbuf.DBP,
                       image_load->bitbltbuf.DBW,
                       image_load->trxpos.DSAX,
                       image_load->trxpos.DSAY,
                       image_load->trxreg.RRW,
                       image_load->trxreg.RRH,
                       image);
            break;
        }
        case PSMT8: {
            gsHelper.UploadPSMT8(image_load->bitbltbuf.DBP,
                             image_load->bitbltbuf.DBW,
                             image_load->trxpos.DSAX,
                             image_load->trxpos.DSAY,
                             image_load->trxreg.RRW,
                             image_load->trxreg.RRH,
                             image);
            break;
        }
        default: spdlog::debug("Texture Transfer Upload Info: DPSM {:#x}", (int)image_load->bitbltbuf.DPSM); break;
    }
}

unsigned char* DownloadGsTexture(sceGsTex0* tex0)
{
    if (!IsFirstUploadDone())
    {
        return nullptr;
    }

    std::vector<uint8_t> img;
    int width = (1<<tex0->TW);
    int height = (1<<tex0->TH);

    spdlog::debug("GS download request for DBP {:#x} CBP {:#x} DPSM {} ", (unsigned long long)tex0->TBP0, (unsigned long long)tex0->CBP, (unsigned long long)tex0->PSM);

    auto texture = GetTexture(tex0);
    if (texture != nullptr)
    {
        return texture;
    }

    switch (tex0->PSM)
    {
        case PSMCT32:
            img = gsHelper.DownloadPSMCT32(
              tex0->TBP0, tex0->TBW,
              0, 0,
              width, height
            );
            break;
        case PSMT4:
            img = gsHelper.DownloadImagePSMT4(
              tex0->TBP0, tex0->TBW,
              0, 0,
              width, height,
              tex0->CBP, tex0->TBW,
              tex0->CSA, -1
              );
            break;
        case PSMT8:
            img = gsHelper.DownloadImagePSMT8(
              tex0->TBP0, tex0->TBW,
              0, 0,
              width, height,
              tex0->CBP, tex0->TBW,
              -1
              );
            break;
        default: spdlog::debug("Texture Transfer Upload Failed, Unsupported Format: DPSM {:#x}", (int)tex0->PSM); break;
    }

    /// TODO: Move code somewhere prettier
    /// TODO: Add TextureManager code here once implemented or else...
    auto image_data = new unsigned int[width * height];
    auto rawPixel = (unsigned int*) img.data();

    struct RGBA
    {
        unsigned char r;
        unsigned char g;
        unsigned char b;
        unsigned char a;
    };

    for (auto i = 0; i < height; i++)
    {
        for (auto k = 0; k < width; k++)
        {
            auto pixel = (RGBA*) &rawPixel[(i * width + k)];
            pixel->a = (char) (255.0f * (pixel->a / 128.0f));
            image_data[(i * width + k)] = *(unsigned int*) pixel;
        }
    }

    AddTexture(tex0, (unsigned char*)image_data);

    return (unsigned char*)image_data;
}
