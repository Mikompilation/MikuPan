#include "mikupan_file.h"
#include "gs/texture_manager.h"
#include "mikupan_logging.h"
#include "spdlog/spdlog.h"
#include <algorithm>
#include <filesystem>
#include <fstream>

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#include <sys/stat.h>
#include <sys/types.h>
#else
#include <fcntl.h>
#include <unistd.h>
#endif

#include <errno.h>

extern "C" {
#include "mikupan_memory.h"
}

static inline std::vector<int> file_loaded_address;
static inline int fd;

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

u_char MikuPan_OpenFile(const char *filename)
{
    std::filesystem::path filePath(filename);
    filePath = std::filesystem::absolute(filePath);

    if (!std::filesystem::exists(filePath))
    {
        MikuPan_WriteFile(filename, fd, 0x0, 0);
    }

#ifdef _WIN32
    fd = _open(filePath, _O_CREAT | _O_BINARY);

    if (fd < 0)
    {
        printf("File Open Error: \n", errno);
        return -1
    }
#else
    fd = open(filePath.c_str(), O_CREAT | O_RDWR, S_IRWXU);

    if (fd < 0)
    {
        printf("File Open Error: %s\n", strerror(errno));
        return -1;
    }
#endif

    return 1;
}

int MikuPan_WriteFile(const char *filename, int fd, void *buffer, int size)
{
    int amtWritten;
    std::filesystem::path filePath(filename);
    if (!std::filesystem::is_empty(filePath))
    {
        std::filesystem::path fileName(filename);
        filePath = std::filesystem::absolute(filePath);
        fileName = fileName.filename();

        if (!std::filesystem::exists(filePath.remove_filename()))
        {
            std::filesystem::create_directories(filePath);
        }

        filePath.replace_filename(fileName);
    }
#ifdef _WIN32
    if (fd == 0 && !std::filesystem::is_empty(filePath))
    {
        fd = _open(filePath, _O_CREAT | _O_BINARY);
        if (fd < 0)
        {
            printf("File Open Error: \n", errno);
            return -1;
        }
    }
    amtWritten = _write(fd, buffer, size);
    if (amtWritten < 0)
    {
        printf("File Write Error: \n", errno);
        return -1;
    }

    else
    {
        printf("No File Provided! \n");
        return 0;
    }
#else

    if (fd == 0 && !std::filesystem::is_empty(filePath))
    {
        fd = open(filePath.c_str(), O_CREAT | O_RDWR, S_IRWXU);

        if (fd < 0)
        {
            printf("File Open Error: %s\n", strerror(errno));
            return -1;
        }
    }
    amtWritten = write(fd, buffer, size);
    if (amtWritten < 0)
    {
        printf("File Write Error: %s\n", strerror(errno));
        return -1;
    }
    else
    {
        printf("No File Provided! \n");
        return 0;
    }

    return amtWritten;
#endif
}

int MikuPan_CreateDirectory(const char *path)
{
    std::filesystem::path folderPath(path);
    folderPath = std::filesystem::absolute(folderPath);
    if (std::filesystem::exists(folderPath))
    {
        return 0;
    }
    return std::filesystem::create_directories(folderPath);
}

int MikuPan_ReadFile(int fd, void *buffer, int size)
{
    int amtRead;
#ifdef _WIN32
    amtRead = _read(fd, buffer, size);
    if (amtRead < 0)
    {
        printf("File Read Error\n");
        return -1;
    }

#else
    amtRead = read(fd, buffer, size);
    if (amtRead < 0)
    {
        printf("File Read Error: %s\n", strerror(errno));
        return -1;
    }
#endif

    return amtRead;
}

void MikuPan_CloseFD()
{
#ifdef _WIN32
    _close(fd);
#else
    close(fd);
#endif
}

int GetFD()
{
    return fd;
}

u_int MikuPan_GetFileSize(const char *filename)
{
    if (!std::filesystem::exists(filename))
    {
        return 0;
    }

    return std::filesystem::file_size(filename);
}