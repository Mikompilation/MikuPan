#include "file_loading.h"

#include "../gs/texture_manager.h"
#include "logging.h"
#include "spdlog/spdlog.h"

extern "C"
{
#include "mikupan_memory.h"
}

#include <filesystem>
#include <fstream>
#include <algorithm>

static inline std::vector<int> file_loaded_address;

void LoadImgHdFile()
{
    return ReadFullFile("./IMG_HD.BIN", (char*)MikuPan_GetHostPointer(ImgHdAddress));
}

void ReadFullFile(const char *filename, char* buffer)
{
    if (!std::filesystem::exists(filename))
    {
        return;
    }

    if (buffer == nullptr)
    {
        return;
    }

    auto fileSize = std::filesystem::file_size(filename);
    std::ifstream infile(filename, std::ios::binary);
    infile.read(buffer, fileSize);
    infile.close();
}

void ReadFileInArchive(int sector, int size, u_int *address)
{
    if (!std::filesystem::exists("./IMG_BD.BIN"))
    {
        spdlog::critical("IMG_BD.BIN not found!");
        return;
    }

    if (address == nullptr)
    {
        return;
    }

    if (!MikuPan_IsPs2MemoryPointer( (int64_t)address ))
    {
        spdlog::critical("File loading address is not in PS2 memory range!");
        return;
    }

    auto ps2_address = MikuPan_GetPs2OffsetFromHostPointer(address);

    if (std::find(file_loaded_address.begin(), file_loaded_address.end(), ps2_address)
    != file_loaded_address.end())
    {
        MikuPan_RequestFlushTextureCache();
    }

    file_loaded_address.push_back(ps2_address);
    info_log("PS2 Address 0x%x", ps2_address);

    //if ((int64_t*)*(int64_t*)address != nullptr)
    //{
    //    MikuPan_RequestFlushTextureCache();
    //    free((int64_t*)*(int64_t*)address);
    //}
    //void* file_ptr = malloc(size);
    //*((int64_t*)address) = (int64_t)file_ptr;

    std::ifstream infile("./IMG_BD.BIN", std::ios::binary);
    infile.seekg(sector * 0x800, std::ios::beg);
    infile.read((char*)address, size);

    infile.close();
}
