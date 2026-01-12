#include "mikupan_file.h"
#include "gs/texture_manager.h"
#include "mikupan_logging.h"
#include "spdlog/spdlog.h"
#include <algorithm>
#include <filesystem>
#include <fstream>

extern "C" {
#include "mikupan_memory.h"
}

static inline std::vector<int> file_loaded_address;

void MikuPan_LoadImgHdFile()
{
    return MikuPan_ReadFullFile("./IMG_HD.BIN",
                        (char *) MikuPan_GetHostPointer(ImgHdAddress));
}

void MikuPan_ReadFullFile(const char *filename, char *buffer)
{
    const std::filesystem::path a(filename);

    if (!std::filesystem::exists(a))
    {
        return;
    }

    if (buffer == nullptr)
    {
        return;
    }

    auto fileSize = std::filesystem::file_size(a);
    std::ifstream infile(a, std::ios::binary);
    infile.read(buffer, fileSize);
    infile.close();
}

void MikuPan_ReadFileInArchive(int sector, int size, u_int *address)
{
    if (!std::filesystem::exists("./IMG_BD.BIN"))
    {
        spdlog::critical("IMG_BD.BIN not found!");
        return;
    }

    if (address == nullptr)
    {
        spdlog::critical("File loading address is NULL, abort load request!");
        return;
    }

    if (!MikuPan_IsPs2MemoryPointer((int64_t) address))
    {
        spdlog::critical("File loading address is not in PS2 memory range!");
        return;
    }

    auto ps2_address = MikuPan_GetPs2OffsetFromHostPointer(address);

    if (std::find(file_loaded_address.begin(), file_loaded_address.end(),
                  ps2_address)
        != file_loaded_address.end())
    {
        MikuPan_RequestFlushTextureCache();
    }

    file_loaded_address.push_back(ps2_address);
    info_log("PS2 Address 0x%x", ps2_address);

    std::ifstream infile("./IMG_BD.BIN", std::ios::binary);
    infile.seekg(sector * 0x800, std::ios::beg);
    infile.read((char *) address, size);

    infile.close();
}

u_int MikuPan_GetFileSize(const char *filename)
{
    if (!std::filesystem::exists(filename))
    {
        return 0;
    }

    return std::filesystem::file_size(filename);
}