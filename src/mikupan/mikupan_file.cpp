#include "mikupan_file.h"
#include "typedefs.h"
#include "gs/mikupan_texture_manager.h"
#include "mikupan_logging.h"
#include "spdlog/spdlog.h"
#include <algorithm>
#include <filesystem>
#include <fstream>

extern "C" {
#include "mikupan_memory.h"
}

std::vector<int> file_loaded_address;

void MikuPan_LoadImgHdFile()
{
    return MikuPan_ReadFullFile("./IMG_HD.BIN",
                                static_cast<char *>(MikuPan_GetHostPointer(ImgHdAddress)));
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
        spdlog::info("IMG_BD.BIN not found!");
        return;
    }

    if (address == nullptr)
    {
        spdlog::info("File loading address is NULL, abort load request!");
        return;
    }

    if (!MikuPan_IsPs2MemoryPointer((int64_t) address))
    {
        spdlog::info("File loading address is not in PS2 memory range!");
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
    infile.read(reinterpret_cast<char *>(address), size);

    infile.close();
}

void MikuPan_ReadFileInArchive64(int sector, int size, int64_t address)
{
    if (!std::filesystem::exists("./IMG_BD.BIN"))
    {
        spdlog::info("Failed to locate the IMG_BD.BIN file!");
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

u_char MikuPan_WriteFile(const char *filename, void *buffer, int size)
{
    auto relative_path = std::filesystem::path(MikuPan_GetRelativePath(filename));

    std::ofstream outFile;
    outFile.open(relative_path, std::ios::binary);
    outFile.write(static_cast<char *>(buffer), size);
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
        return std::filesystem::create_directory(relative_path);
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

std::string MikuPan_GetRelativePath(const char* path)
{
    auto relative_path = std::filesystem::path("./");
    relative_path += path;
    return relative_path.generic_u8string();
}
