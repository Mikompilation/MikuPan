#include "typedefs.h"
#include "mikupan_file.h"
#include "mikupan_config.h"
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

static std::filesystem::path MikuPan_GetDataRoot()
{
    const char *folder = mikupan_configuration.data_folder;
    if (folder != nullptr && folder[0] != '\0')
    {
        return std::filesystem::path(folder);
    }

    return std::filesystem::path(".");
}

void MikuPan_LoadImgHdFile()
{
    const auto path = (MikuPan_GetDataRoot() / "IMG_HD.BIN").generic_string();
    MikuPan_ReadFullFile(path.c_str(),
        static_cast<char *>(MikuPan_GetHostPointer(ImgHdAddress)));
}

void MikuPan_ReadFullFile(const char *filename, char *buffer)
{
    const std::filesystem::path path_filename(filename);

    if (!std::filesystem::exists(path_filename))
    {
        return;
    }

    if (buffer == nullptr)
    {
        return;
    }

    auto file_size = std::filesystem::file_size(path_filename);
    std::ifstream infile(path_filename, std::ios::binary);
    infile.read(buffer, file_size);
    infile.close();
}

void MikuPan_NotifyPs2MemoryLoad(int ps2_address)
{
    if (ps2_address < 0)
    {
        return;
    }

    if (std::find(file_loaded_address.begin(), file_loaded_address.end(),
                  ps2_address)
        != file_loaded_address.end())
    {
        MikuPan_RequestFlushTextureCache();
        return;
    }

    file_loaded_address.push_back(ps2_address);
}

void MikuPan_ReadFileInArchive(int sector, int size, u_int *address)
{
    const auto archive = MikuPan_GetDataRoot() / "IMG_BD.BIN";
    if (!std::filesystem::exists(archive))
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

    std::ifstream infile(archive, std::ios::binary);
    infile.seekg(sector * 0x800, std::ios::beg);
    infile.read(reinterpret_cast<char *>(address), size);

    infile.close();
}

void MikuPan_BufferFile(int sector, int size, int64_t address)
{
    const auto archive = MikuPan_GetDataRoot() / "IMG_BD.BIN";
    if (!std::filesystem::exists(archive))
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

    std::ifstream infile(archive, std::ios::binary);
    infile.seekg(sector * 0x800, std::ios::beg);
    infile.read(static_cast<char *>(file_ptr), size);

    infile.close();
}

u_char MikuPan_ReadFile(const char *filename, void *buffer, int size)
{
    auto relative_path = std::filesystem::path(MikuPan_GetRelativePath(filename));

    if (!std::filesystem::exists(relative_path))
    {
        return 0;
    }

    std::ifstream inFile;
    inFile.open(relative_path);
    inFile.read(static_cast<char *>(buffer), size);
    inFile.close();
    return inFile.bad() == 0;
}

u_char MikuPan_WriteFile(const char *filename, const void *buffer, int size)
{
    auto relative_path = std::filesystem::path(MikuPan_GetRelativePath(filename));

    std::ofstream outFile;
    outFile.open(relative_path, std::ios::binary);
    outFile.write(static_cast<const char *>(buffer), size);
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
    
    if (s.rfind("./", 0) == 0) {
        s.erase(0, 2);
    } else if (auto colon = s.find(':'); colon != std::string::npos) {
        s.erase(0, colon + 1);
        if (!s.empty() && s.front() == '/') {
            s.erase(0, 1);
        }
    }

    while (!s.empty() && s.front() == '/') {
        s.erase(0, 1);
    }

    if (!s.empty()) {
        s = (MikuPan_GetDataRoot() / s).generic_string();
    }

    if (s.length() + 1 > buffer_size) {
        spdlog::error("MikuPan_ResolveCdPath: Buffer too small. Need {} bytes, got {}", 
                      s.length() + 1, buffer_size);
        return false;
    }
    
    std::strcpy(buffer, s.c_str());
    
    return true;
}
u_char MikuPan_FolderExists(const char *folder)
{
    auto relative_path = std::filesystem::path(MikuPan_GetRelativePath(folder));

    if (!std::filesystem::is_directory(relative_path))
    {
        relative_path = relative_path.remove_filename();
    }

    return std::filesystem::exists(relative_path);
}

u_char MikuPan_CreateFolder(const char *folder)
{
    auto relative_path = std::filesystem::path(MikuPan_GetRelativePath(folder));

    if (!std::filesystem::exists(relative_path))
    {
        return std::filesystem::create_directories(relative_path);
    }

    return 1;
}

int MikuPan_GetListFiles(const char *folder, MikuPan_McTblGetDir *table)
{
    auto relative_path = std::filesystem::path(MikuPan_GetRelativePath(folder));

    if (!std::filesystem::is_directory(relative_path))
    {
        relative_path = relative_path.remove_filename();
    }

    int i = 0;
    for (const auto& entry : std::filesystem::directory_iterator(relative_path))
    {
        table[i].FileSizeByte = entry.file_size();
        strncpy(reinterpret_cast<char *>(table[i].EntryName), entry.path().filename().generic_u8string().c_str(), sizeof(table[i].EntryName));

        if (i++, i > 18)
        {
            break;
        }
    }

    return i;
}

std::string MikuPan_GetRelativePath(const char *path)
{
    auto relative_path = std::filesystem::path(path).relative_path();
    return (MikuPan_GetDataRoot() / relative_path).generic_u8string();
}
