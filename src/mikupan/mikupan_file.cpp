#include "mikupan_file.h"
#include "gs/mikupan_texture_manager.h"
#include "mikupan_logging.h"
#include "spdlog/spdlog.h"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <string>
#include <cctype>
#include <limits>
#include <cstring>

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

void MikuPan_ReadFileInArchive64(int sector, int size, int64_t address)
{
    if (!std::filesystem::exists("./IMG_BD.BIN"))
    {
        return;
    }

    if (address == 0)
    {
        return;
    }

    if ((int64_t *) *(int64_t *) address != nullptr)
    {
        free((int64_t *) *(int64_t *) address);
    }

    void *file_ptr = malloc(size);

    *((int64_t *) address) = (int64_t) file_ptr;

    std::ifstream infile("./IMG_BD.BIN", std::ios::binary);
    infile.seekg(sector * 0x800, std::ios::beg);
    infile.read((char *) file_ptr, size);

    infile.close();
}

u_char MikuPan_OpenFile(const char *filename, void *buffer, int size)
{
    std::filesystem::path filePath(filename);
    filePath = filePath.relative_path();
    std::ifstream inFile;

    if (!std::filesystem::exists(filePath))
    {
        MikuPan_SaveFile(filename, buffer, 0);
    }
    inFile.open(filePath);
    inFile.read((char *) buffer, size);
    inFile.close();
    return inFile.bad() == 0;
}

u_char MikuPan_SaveFile(const char *filename, void *buffer, int size)
{
    std::filesystem::path filePath(filename);
    filePath = std::filesystem::absolute(filePath);
    filePath = filePath.remove_filename();
    std::ofstream outFile;

    if (!std::filesystem::exists(filePath))
    {
        std::filesystem::create_directories(filePath);
    }

    std::filesystem::path fileName(filename);
    fileName = std::filesystem::absolute(fileName);

    outFile.open(fileName, std::ios::binary);
    outFile.write((char *) buffer, size);
    outFile.close();

    return outFile.bad() == 0;
}

u_int MikuPan_GetFileSize(const char *filename)
{
    if (!std::filesystem::exists(filename))
    {
        return 0;
    }

    return std::filesystem::file_size(filename);
}

bool MikuPan_ResolveCdPath(const char* path, char* buffer, size_t buffer_size)
{
    if (!path) {
        spdlog::error("MikuPan_ResolveCdPath: path is null");
        return false;
    }
    
    if (!buffer) {
        spdlog::error("MikuPan_ResolveCdPath: buffer is null");
        return false;
    }
    
    if (buffer_size == 0) {
        spdlog::error("MikuPan_ResolveCdPath: buffer_size is 0");
        return false;
    }
    
    std::string s(path);
    
    auto ltrim = [](std::string& x) {
        x.erase(x.begin(), std::find_if(x.begin(), x.end(), [](unsigned char ch) {
            return !std::isspace(ch);
        }));
    };
    
    auto rtrim = [](std::string& x) {
        x.erase(std::find_if(x.rbegin(), x.rend(), [](unsigned char ch) {
            return !std::isspace(ch);
        }).base(), x.end());
    };
    
    ltrim(s);
    rtrim(s);
    
    if (auto semi = s.find(';'); semi != std::string::npos) {
        s.erase(semi);
    }
    
    std::replace(s.begin(), s.end(), '\\', '/');
    
    if (s.rfind("./", 0) != 0) {
        if (auto colon = s.find(':'); colon != std::string::npos) {
            s.erase(0, colon + 1);
            if (!s.empty() && s.front() == '/') {
                s.erase(0, 1);
            }
        }
        if (!s.empty()) {
            s.insert(0, "./");
        }
    }
    
    if (s.length() + 1 > buffer_size) {
        spdlog::error("MikuPan_ResolveCdPath: Buffer too small. Need {} bytes, got {}", 
                      s.length() + 1, buffer_size);
        return false;
    }
    
    std::strcpy(buffer, s.c_str());
    
    return true;
}