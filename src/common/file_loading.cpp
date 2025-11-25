#include "file_loading.h"

#include "gs/texture_manager.h"
#include "logging.h"

#include <filesystem>
#include <fstream>
#include <string.h>

unsigned char* LoadImgHdFile()
{
    return ReadFullFile("./IMG_HD.BIN");
}

unsigned char *ReadFullFile(const char *filename)
{
    if (!std::filesystem::exists(filename))
    {
        return NULL;
    }

    auto fileSize = std::filesystem::file_size(filename);

    char *buffer = new char[fileSize];
    std::ifstream infile(filename, std::ios::binary);
    infile.read(buffer, fileSize);

    infile.close();

    return (unsigned char*)buffer;
}

void ReadFileInArchive(int sector, int size, int64_t address)
{
    if (!std::filesystem::exists("./IMG_BD.BIN"))
    {
        return;
    }

    if (address == 0)
    {
        return;
    }

    info_log("File load pointer 0x%x", address);

    if ((int64_t*)*(int64_t*)address != nullptr)
    {
        //MikuPan_FlushTextureCache();
        MikuPan_RequestFlushTextureCache();
        free((int64_t*)*(int64_t*)address);
    }

    void* file_ptr = malloc(size);

    *((int64_t*)address) = (int64_t)file_ptr;

    std::ifstream infile("./IMG_BD.BIN", std::ios::binary);
    infile.seekg(sector * 0x800, std::ios::beg);
    infile.read((char*)file_ptr, size);

    infile.close();
}
