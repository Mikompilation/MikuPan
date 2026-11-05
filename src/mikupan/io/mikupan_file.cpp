#include "typedefs.h"
#include "mikupan_file.h"
#include "mikupan/io/mikupan_disc_image.h"
#include "mikupan/mikupan_config.h"
#include "mikupan/debug/mikupan_logging.h"
#include "mikupan/ui/mikupan_ui.h"
#include "os/eeiop/cdvd/eecdvd.h"
#include "spdlog/spdlog.h"
#include <SDL3/SDL_dialog.h>
#include <SDL3/SDL_events.h>
#include <SDL3/SDL_filesystem.h>
#include <SDL3/SDL_hints.h>
#include <SDL3/SDL_init.h>
#include <SDL3/SDL_iostream.h>
#include <SDL3/SDL_messagebox.h>
#include <SDL3/SDL_platform.h>
#include <SDL3/SDL_stdinc.h>
#include <SDL3/SDL_timer.h>
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <string>
#include <cctype>
#include <limits>
#include <cstring>
#include <condition_variable>
#include <mutex>
#include <system_error>
#include <vector>

#include "mikupan/mikupan_memory.h"

static std::mutex missing_data_dialog_mutex;
static std::condition_variable missing_data_dialog_cv;
static bool missing_data_dialog_requested = false;
static bool missing_data_dialog_open = false;
static bool missing_data_dialog_selected = false;
static std::string missing_data_path;

/// Paths this layer hands out for files on the mounted disc image.
static constexpr const char *kDiscPathPrefix = "disc://";

/// A choice made in the file picker, which answers from a thread of its own,
/// waiting for the main thread.
static std::mutex data_source_browser_mutex;
static bool data_source_browser_open = false;
static int picked_data_source = -1;
static std::string picked_data_source_path;

/// Main thread only.
static std::string synced_disc_image_path;
static std::string disc_image_problem;
static std::string picked_data_source_problem;
static std::string required_data_problem;

extern "C" const char *MikuPan_GetBaseDirectory(void)
{
    static std::string cached;
    if (!cached.empty())
    {
        return cached.c_str();
    }

    const char *base = SDL_GetBasePath();
    cached = (base != nullptr && base[0] != '\0') ? base : "./";

    /* On macOS SDL_GetBasePath() returns the main bundle's resource path.
     * That is right for a .app bundle, but for a bare executable (what the
     * CMake build produces) it comes back as "<exe dir>/Resources/", a
     * folder that does not exist. On the default case-insensitive APFS
     * volume that name also aliases our own "resources/" folder, so every
     * asset lookup turns into "<exe dir>/resources/resources/..." and fails.
     * Detect that by probing for a directory that must exist under a valid
     * base and, if it is missing, fall back to the parent of "Resources/". */
    if (SDL_strcmp(SDL_GetPlatform(), "macOS") == 0)
    {
        namespace fs = std::filesystem;
        std::error_code ec;
        const fs::path probe_dir = fs::path("resources") / "shaders";
        if (!fs::is_directory(fs::path(cached) / probe_dir, ec))
        {
            /* "<dir>/Resources/" -> parent_path() twice strips the trailing
             * separator component and then "Resources". */
            const fs::path parent =
                fs::path(cached).parent_path().parent_path();
            if (!parent.empty() && fs::is_directory(parent / probe_dir, ec))
            {
                cached = parent.string();
                if (cached.back() != '/')
                {
                    cached += '/';
                }
            }
        }
    }

    return cached.c_str();
}

static std::filesystem::path MikuPan_GetApplicationBasePath()
{
    return std::filesystem::path(MikuPan_GetBaseDirectory());
}

static std::filesystem::path MikuPan_GetDataRoot()
{
    const char *folder = mikupan_configuration.data_folder;
    if (folder != nullptr && folder[0] != '\0')
    {
        return std::filesystem::path(folder);
    }

    return MikuPan_GetApplicationBasePath();
}

#ifdef __ANDROID__
static bool MikuPan_IsAndroidContentUriString(const std::string& path)
{
    return path.rfind("content://", 0) == 0;
}

static bool MikuPan_IsAndroidTreeUriString(const std::string& path)
{
    return MikuPan_IsAndroidContentUriString(path)
        && path.find("/tree/") != std::string::npos;
}

static bool MikuPan_IsUriUnreserved(unsigned char ch)
{
    return (ch >= 'A' && ch <= 'Z')
        || (ch >= 'a' && ch <= 'z')
        || (ch >= '0' && ch <= '9')
        || ch == '-' || ch == '.' || ch == '_' || ch == '~';
}

static std::string MikuPan_EncodeAndroidDocumentPath(
    const std::string& relative_path)
{
    static const char hex[] = "0123456789ABCDEF";
    std::string encoded;
    encoded.reserve(relative_path.size() * 3);

    for (unsigned char ch : relative_path)
    {
        if (ch == '/' || ch == '\\')
        {
            encoded += "%2F";
        }
        else if (MikuPan_IsUriUnreserved(ch))
        {
            encoded += static_cast<char>(ch);
        }
        else
        {
            encoded += '%';
            encoded += hex[ch >> 4];
            encoded += hex[ch & 0x0f];
        }
    }

    return encoded;
}

static std::string MikuPan_NormalizeRelativePathString(
    const std::filesystem::path& path)
{
    std::filesystem::path relative_path;
    for (const auto& part : path.relative_path())
    {
        if (part.empty() || part == "." || part == "..")
        {
            continue;
        }

        relative_path /= part;
    }

    return relative_path.generic_string();
}

static std::string MikuPan_GetAndroidTreeChildUri(
    const std::string& tree_uri,
    const std::filesystem::path& relative_path)
{
    const std::string marker = "/tree/";
    const size_t tree_pos = tree_uri.find(marker);
    if (tree_pos == std::string::npos)
    {
        return tree_uri;
    }

    const size_t tree_doc_id_start = tree_pos + marker.size();
    const size_t tree_doc_id_end = tree_uri.find('/', tree_doc_id_start);
    const std::string tree_doc_id =
        tree_uri.substr(tree_doc_id_start,
                        tree_doc_id_end == std::string::npos
                            ? std::string::npos
                            : tree_doc_id_end - tree_doc_id_start);
    const std::string child =
        MikuPan_NormalizeRelativePathString(relative_path);
    if (tree_doc_id.empty() || child.empty())
    {
        return tree_uri;
    }

    return tree_uri.substr(0, tree_doc_id_start)
        + tree_doc_id
        + "/document/"
        + tree_doc_id
        + "%2F"
        + MikuPan_EncodeAndroidDocumentPath(child);
}
#endif

static std::string MikuPan_GetDataFolderPathString(
    const std::filesystem::path& relative_path)
{
    const char *folder = mikupan_configuration.data_folder;
#ifdef __ANDROID__
    if (folder != nullptr && MikuPan_IsAndroidTreeUriString(folder))
    {
        return MikuPan_GetAndroidTreeChildUri(folder, relative_path);
    }
#endif

    return (MikuPan_GetDataRoot() / relative_path).generic_string();
}

static std::string MikuPan_NormalizeSdlAssetPathString(const std::string& path)
{
    if (path.rfind("assets://", 0) == 0)
    {
        return path;
    }

    if (path.rfind("assets:/", 0) == 0)
    {
        return "assets://" + path.substr(8);
    }

    return path;
}

static bool MikuPan_IsDiscPathString(const std::string& path)
{
    return path.rfind(kDiscPathPrefix, 0) == 0;
}

/// Files picked on the data setup screen count too: their paths arrive from
/// the file picker as UTF-8, which narrow file APIs misread on Windows.
static bool MikuPan_IsSdlAssetPathString(const std::string& path)
{
    return path.rfind("assets://", 0) == 0
        || MikuPan_IsDiscPathString(path)
        || (!path.empty()
            && (path == mikupan_configuration.img_hd_path
                || path == mikupan_configuration.img_bd_path))
#ifdef __ANDROID__
        || MikuPan_IsAndroidContentUriString(path)
#endif
        ;
}

static bool MikuPan_IsSdlAssetPath(const char *filename,
                                   std::string *normalized = nullptr)
{
    if (filename == nullptr)
    {
        return false;
    }

    std::string path = MikuPan_NormalizeSdlAssetPathString(filename);
    const bool result = MikuPan_IsSdlAssetPathString(path);
    if (result && normalized != nullptr)
    {
        *normalized = path;
    }
    return result;
}

static u_int MikuPan_GetSdlFileSize(const char *filename)
{
    std::string path;
    if (!MikuPan_IsSdlAssetPath(filename, &path))
    {
        return 0;
    }

    SDL_IOStream *stream = MikuPan_OpenStream(path.c_str());
    if (stream == nullptr)
    {
        return 0;
    }

    const Sint64 size = SDL_GetIOSize(stream);
    SDL_CloseIO(stream);

    if (size <= 0
        || static_cast<uint64_t>(size) > std::numeric_limits<u_int>::max())
    {
        return 0;
    }

    return static_cast<u_int>(size);
}

static bool MikuPan_ReadSdlFileRange(const char *filename,
                                     Sint64 offset,
                                     void *buffer,
                                     size_t size)
{
    if (buffer == nullptr)
    {
        return false;
    }

    std::string path;
    if (!MikuPan_IsSdlAssetPath(filename, &path))
    {
        return false;
    }

    SDL_IOStream *stream = MikuPan_OpenStream(path.c_str());
    if (stream == nullptr)
    {
        return false;
    }

    if (offset != 0 && SDL_SeekIO(stream, offset, SDL_IO_SEEK_SET) < 0)
    {
        SDL_CloseIO(stream);
        return false;
    }

    size_t total = 0;
    char *dst = static_cast<char *>(buffer);
    while (total < size)
    {
        const size_t read =
            SDL_ReadIO(stream, dst + total, size - total);
        if (read == 0)
        {
            break;
        }
        total += read;
    }

    SDL_CloseIO(stream);
    return total == size;
}

static std::string MikuPan_ToLowerPathString(const std::filesystem::path& path)
{
    std::string value = path.lexically_normal().generic_string();
    std::transform(value.begin(), value.end(), value.begin(),
                   [](unsigned char ch) {
                       return static_cast<char>(std::tolower(ch));
                   });
    return value;
}

static bool MikuPan_PathHasContent(const std::string& path)
{
    if (MikuPan_IsSdlAssetPathString(path))
    {
        return MikuPan_GetSdlFileSize(path.c_str()) > 0;
    }

    std::error_code error;
    if (!std::filesystem::is_regular_file(path, error) || error)
    {
        return false;
    }

    const uintmax_t file_size = std::filesystem::file_size(path, error);
    return !error && file_size > 0;
}

/// Where a data file is read from. The data folder comes first, so loose
/// files keep overriding everything else, then what was picked on the data
/// setup screen, then the mounted disc image. A file that is nowhere gets its
/// data folder path, which is what the missing file prompts work from.
static std::string MikuPan_GetDataPathString(
    const std::filesystem::path& relative_path)
{
    const std::string folder_path =
        MikuPan_GetDataFolderPathString(relative_path);
    if (MikuPan_PathHasContent(folder_path))
    {
        return folder_path;
    }

    const std::string name = MikuPan_ToLowerPathString(relative_path);
    if (name == "img_hd.bin" || name == "img_bd.bin")
    {
        const std::string picked_path = name == "img_hd.bin"
            ? mikupan_configuration.img_hd_path
            : mikupan_configuration.img_bd_path;

        if (!picked_path.empty() && MikuPan_PathHasContent(picked_path))
        {
            return picked_path;
        }
    }

    const std::string disc_name = relative_path.generic_string();
    if (MikuPan_DiscImageHasFile(disc_name))
    {
        return kDiscPathPrefix + disc_name;
    }

    return folder_path;
}

static bool MikuPan_IsExcludedRuntimeAsset(const std::filesystem::path& path)
{
    std::string value = MikuPan_ToLowerPathString(path);
    while (value.rfind("./", 0) == 0)
    {
        value.erase(0, 2);
    }

    return value.rfind("resources/", 0) == 0
        || value.find("/resources/") != std::string::npos
        || value == "mikupan.ini"
        || (value.size() >= 12
            && value.compare(value.size() - 12, 12, "/mikupan.ini") == 0);
}

static bool MikuPan_IsPathInsideDataRoot(const std::filesystem::path& path)
{
    std::string root =
        MikuPan_ToLowerPathString(std::filesystem::absolute(MikuPan_GetDataRoot()));
    std::string target =
        MikuPan_ToLowerPathString(std::filesystem::absolute(path));

    while (root.size() > 1 && root.back() == '/')
    {
        root.pop_back();
    }

    if (target == root)
    {
        return true;
    }

    root += '/';
    return target.rfind(root, 0) == 0;
}

static bool MikuPan_IsRequiredArchiveDataFile(
    const std::filesystem::path& path)
{
    const std::string filename =
        MikuPan_ToLowerPathString(path.filename());

    return filename == "img_hd.bin" || filename == "img_bd.bin";
}

static bool MikuPan_ShouldPromptForMissingDataFile(
    const std::filesystem::path& path)
{
    if (MikuPan_IsRequiredArchiveDataFile(path))
    {
        return true;
    }

    if (MikuPan_IsExcludedRuntimeAsset(path))
    {
        return false;
    }

    return MikuPan_IsPathInsideDataRoot(path);
}

static void MikuPan_ApplySelectedDataFolder(const std::string& folder)
{
    if (folder.empty())
    {
        return;
    }

    std::strncpy(mikupan_configuration.data_folder,
                 folder.c_str(),
                 sizeof(mikupan_configuration.data_folder) - 1);
    mikupan_configuration
        .data_folder[sizeof(mikupan_configuration.data_folder) - 1] = '\0';

    if (!MikuPan_SaveConfiguration(nullptr))
    {
        spdlog::error("Failed to save selected data folder: {}", folder);
    }
    else
    {
        spdlog::info("Selected data folder: {}", folder);
    }
}

static bool MikuPan_MakePathRelativeToRoot(
    const std::filesystem::path& path,
    const std::filesystem::path& root,
    std::filesystem::path& relative)
{
    std::error_code ec;
    std::filesystem::path abs_path = std::filesystem::absolute(path, ec);
    if (ec)
    {
        return false;
    }

    std::filesystem::path abs_root = std::filesystem::absolute(root, ec);
    if (ec)
    {
        return false;
    }

    relative = abs_path.lexically_normal().lexically_relative(
        abs_root.lexically_normal());
    if (relative.empty())
    {
        return false;
    }

    for (const auto& part : relative)
    {
        if (part == "..")
        {
            return false;
        }
    }

    return true;
}

static std::filesystem::path MikuPan_RebasePathAfterDataFolderSelection(
    const std::filesystem::path& path,
    const std::filesystem::path& old_root)
{
    if (MikuPan_IsRequiredArchiveDataFile(path))
    {
        return MikuPan_GetDataRoot() / path.filename();
    }

    std::filesystem::path relative;
    if (MikuPan_MakePathRelativeToRoot(path, old_root, relative))
    {
        return MikuPan_GetDataRoot() / relative;
    }

    return path;
}

static void SDLCALL MikuPan_ServiceMissingDataFolderDialogOnMainThread(
    void *userdata)
{
    (void)userdata;
    MikuPan_ServiceMissingDataFolderDialog();
}

static bool MikuPan_WaitForMissingDataFolderDialogOnMainThread()
{
    for (;;)
    {
        {
            std::lock_guard<std::mutex> lock(missing_data_dialog_mutex);
            if (!missing_data_dialog_open && !missing_data_dialog_requested)
            {
                return missing_data_dialog_selected;
            }
        }

        MikuPan_ServiceMissingDataFolderDialog();
        SDL_PumpEvents();
        SDL_Delay(10);
    }
}

static bool MikuPan_RequestMissingDataFolderDialog(
    const std::filesystem::path& path)
{
    if (!MikuPan_ShouldPromptForMissingDataFile(path))
    {
        return false;
    }

    if (SDL_WasInit(SDL_INIT_VIDEO) == 0)
    {
        return false;
    }

    const bool is_main_thread = SDL_IsMainThread();

    {
        std::unique_lock<std::mutex> lock(missing_data_dialog_mutex);
        if (missing_data_dialog_open || missing_data_dialog_requested)
        {
            if (!is_main_thread)
            {
                missing_data_dialog_cv.wait(lock, [] {
                    return !missing_data_dialog_open
                        && !missing_data_dialog_requested;
                });
                return missing_data_dialog_selected;
            }

            lock.unlock();
            return MikuPan_WaitForMissingDataFolderDialogOnMainThread();
        }

        missing_data_path = path.lexically_normal().generic_string();
        missing_data_dialog_requested = true;
        missing_data_dialog_selected = false;
    }

    if (is_main_thread)
    {
        MikuPan_ServiceMissingDataFolderDialog();
    }
    else if (!SDL_RunOnMainThread(
                 MikuPan_ServiceMissingDataFolderDialogOnMainThread,
                 nullptr,
                 false))
    {
        {
            std::lock_guard<std::mutex> lock(missing_data_dialog_mutex);
            missing_data_dialog_requested = false;
            missing_data_dialog_selected = false;
        }
        missing_data_dialog_cv.notify_all();
        return false;
    }

    if (is_main_thread)
    {
        return MikuPan_WaitForMissingDataFolderDialogOnMainThread();
    }
    else
    {
        std::unique_lock<std::mutex> lock(missing_data_dialog_mutex);
        missing_data_dialog_cv.wait(lock, [] {
            return !missing_data_dialog_open && !missing_data_dialog_requested;
        });
        return missing_data_dialog_selected;
    }
}

static bool MikuPan_RequestMissingDataFolderDialogAsync(
    const std::filesystem::path& path)
{
    if (!MikuPan_ShouldPromptForMissingDataFile(path))
    {
        return false;
    }

    if (SDL_WasInit(SDL_INIT_VIDEO) == 0)
    {
        return false;
    }

    {
        std::lock_guard<std::mutex> lock(missing_data_dialog_mutex);
        if (missing_data_dialog_open || missing_data_dialog_requested)
        {
            return true;
        }

        missing_data_path = path.lexically_normal().generic_string();
        missing_data_dialog_requested = true;
        missing_data_dialog_selected = false;
    }

    return true;
}

extern "C" void MikuPan_RequestDataFolderSelection(const char *missing_path)
{
    const char *path = (missing_path != nullptr && missing_path[0] != '\0')
                           ? missing_path
                           : "IMG_HD.BIN";
    MikuPan_RequestMissingDataFolderDialogAsync(std::filesystem::path(path));
}

/// IMG_HD.BIN has one 8-byte entry per file, and every release of the game has
/// its own number of files, so its size tells the releases apart.
static bool MikuPan_IsRequiredImgHdSize(int64_t size)
{
    return size == static_cast<int64_t>(GetFileNameCount()) * 8;
}

static std::string MikuPan_GetWrongDiscProblem(const char *subject)
{
    return std::string(subject) + " is not from the "
        + MikuPan_GetRequiredDiscName() + " disc.";
}

extern "C" const char *MikuPan_GetRequiredDiscName()
{
#ifdef BUILD_EU_VERSION
    return "Project Zero (PAL)";
#else
    return "Fatal Frame (NTSC-U)";
#endif
}

/// Keeps the mounted image in step with the configuration. This runs every
/// frame while the data is missing, so a path is only tried once.
static void MikuPan_SyncDiscImage()
{
    const char *path = mikupan_configuration.iso_path;
    if (synced_disc_image_path == path)
    {
        return;
    }

    synced_disc_image_path = path;
    disc_image_problem.clear();

    std::string error;
    if (path[0] == '\0')
    {
        MikuPan_DiscImageUnmount();
    }
    else if (!MikuPan_DiscImageMount(path, &error))
    {
        MikuPan_DiscImageUnmount();
        disc_image_problem =
            "The saved disc image could not be read. Select it again.";
    }
}

extern "C" bool MikuPan_HasRequiredDataFiles(char *missing_file,
                                             size_t missing_file_size)
{
    MikuPan_SyncDiscImage();
    required_data_problem = disc_image_problem;

    const char *required_files[] = {
        "IMG_HD.BIN",
        "IMG_BD.BIN",
    };

    const char *missing = nullptr;
    for (const char *required_file : required_files)
    {
        if (!MikuPan_PathHasContent(MikuPan_GetDataPathString(required_file)))
        {
            missing = required_file;
            break;
        }
    }

    if (missing == nullptr
        && !MikuPan_IsRequiredImgHdSize(MikuPan_GetDataFileSize("IMG_HD.BIN")))
    {
        missing = "IMG_HD.BIN";
        required_data_problem =
            MikuPan_GetWrongDiscProblem("The game data that was found");
    }

    if (missing_file != nullptr && missing_file_size > 0)
    {
        std::strncpy(missing_file, missing != nullptr ? missing : "",
                     missing_file_size - 1);
        missing_file[missing_file_size - 1] = '\0';
    }
    return missing == nullptr;
}

template <size_t N>
static void MikuPan_SetConfigPath(char (&destination)[N],
                                  const std::string& path)
{
    std::strncpy(destination, path.c_str(), N - 1);
    destination[N - 1] = '\0';
}

static int64_t MikuPan_GetStreamSize(const std::string& path)
{
    SDL_IOStream *stream = MikuPan_OpenStream(path.c_str());
    if (stream == nullptr)
    {
        return -1;
    }

    const Sint64 size = SDL_GetIOSize(stream);
    SDL_CloseIO(stream);
    return size;
}

static bool MikuPan_UseDiscImage(const std::string& path, std::string *problem)
{
    if (!MikuPan_DiscImageMount(path.c_str(), problem))
    {
        return false;
    }

    /// Whatever the configuration holds comes back on the next sync if this
    /// image turns out to be the wrong one.
    synced_disc_image_path = path;

    if (!MikuPan_DiscImageHasFile("IMG_HD.BIN")
        || !MikuPan_DiscImageHasFile("IMG_BD.BIN")
        || !MikuPan_IsRequiredImgHdSize(MikuPan_GetStreamSize(
            kDiscPathPrefix + std::string("IMG_HD.BIN"))))
    {
        *problem = MikuPan_GetWrongDiscProblem("That disc image");
        return false;
    }

    MikuPan_SetConfigPath(mikupan_configuration.iso_path, path);
    mikupan_configuration.img_hd_path[0] = '\0';
    mikupan_configuration.img_bd_path[0] = '\0';
    return true;
}

static bool MikuPan_UseArchiveFile(int source, const std::string& path,
                                   std::string *problem)
{
    const bool is_hd = source == MIKUPAN_DATA_SOURCE_IMG_HD;
    const int64_t size = MikuPan_GetStreamSize(path);

    if (size <= 0)
    {
        *problem = "That file could not be read.";
        return false;
    }

    if (is_hd && !MikuPan_IsRequiredImgHdSize(size))
    {
        *problem = MikuPan_GetWrongDiscProblem("That IMG_HD.BIN");
        return false;
    }

    /// IMG_BD.BIN is whole disc sectors. IMG_HD.BIN never is, which catches
    /// the two being mixed up.
    if (!is_hd && size % 0x800 != 0)
    {
        *problem = "That file is not IMG_BD.BIN.";
        return false;
    }

    if (is_hd)
    {
        MikuPan_SetConfigPath(mikupan_configuration.img_hd_path, path);
    }
    else
    {
        MikuPan_SetConfigPath(mikupan_configuration.img_bd_path, path);
    }

    /// Its folder becomes the data folder, so that the other archive and the
    /// MOVIE folders are found if they sit beside it, as they do on the disc.
    const size_t separator = path.find_last_of("/\\");
    if (path.rfind("content://", 0) != 0 && separator != std::string::npos)
    {
        MikuPan_SetConfigPath(mikupan_configuration.data_folder,
                              path.substr(0, separator + 1));
    }

    mikupan_configuration.iso_path[0] = '\0';
    return true;
}

static void SDLCALL MikuPan_DataSourceBrowsed(
    void *userdata, const char *const *filelist, int filter)
{
    (void)filter;

    std::lock_guard<std::mutex> lock(data_source_browser_mutex);
    data_source_browser_open = false;

    if (filelist != nullptr && filelist[0] != nullptr)
    {
        picked_data_source = static_cast<int>(reinterpret_cast<intptr_t>(userdata));
        picked_data_source_path = filelist[0];
    }
}

extern "C" void MikuPan_BrowseForDataSource(int source)
{
    /// SDL reads these until the picker closes.
    static const SDL_DialogFileFilter disc_filters[] = {
        {"Disc images", "iso"},
        {"All files", "*"},
    };
    static const SDL_DialogFileFilter archive_filters[] = {
        {"Game data", "bin"},
        {"All files", "*"},
    };

    {
        std::lock_guard<std::mutex> lock(data_source_browser_mutex);
        if (data_source_browser_open)
        {
            return;
        }
        data_source_browser_open = true;
    }

    const char *folder = mikupan_configuration.data_folder;
    const bool is_disc = source == MIKUPAN_DATA_SOURCE_DISC_IMAGE;

    SDL_ShowOpenFileDialog(MikuPan_DataSourceBrowsed,
                           reinterpret_cast<void *>(static_cast<intptr_t>(source)),
                           MikuPan_GetUiWindow(),
                           is_disc ? disc_filters : archive_filters,
                           SDL_arraysize(disc_filters),
                           folder[0] != '\0' ? folder : nullptr,
                           false);
}

extern "C" void MikuPan_ServiceDataSourceBrowser()
{
    int source = -1;
    std::string path;

    {
        std::lock_guard<std::mutex> lock(data_source_browser_mutex);
        std::swap(source, picked_data_source);
        path.swap(picked_data_source_path);
    }

    if (source < 0)
    {
        return;
    }

    picked_data_source_problem.clear();

    const bool used =
        source == MIKUPAN_DATA_SOURCE_DISC_IMAGE
            ? MikuPan_UseDiscImage(path, &picked_data_source_problem)
            : MikuPan_UseArchiveFile(source, path, &picked_data_source_problem);

    if (used && !MikuPan_SaveConfiguration(nullptr))
    {
        spdlog::error("Failed to save the selected game data: {}", path);
    }
}

std::string MikuPan_GetDataSourcePath(int source)
{
    if (source == MIKUPAN_DATA_SOURCE_DISC_IMAGE)
    {
        return mikupan_configuration.iso_path;
    }

    const std::string path = MikuPan_GetDataPathString(
        source == MIKUPAN_DATA_SOURCE_IMG_HD ? "IMG_HD.BIN" : "IMG_BD.BIN");

    return !MikuPan_IsDiscPathString(path) && MikuPan_PathHasContent(path)
        ? path
        : std::string();
}

std::string MikuPan_GetDataProblem()
{
    return !picked_data_source_problem.empty() ? picked_data_source_problem
                                               : required_data_problem;
}

static void SDLCALL MikuPan_MissingDataFolderSelected(
    void *userdata, const char *const *filelist, int filter)
{
    (void)userdata;
    (void)filter;

    std::string selected_folder;
    if (filelist != nullptr && filelist[0] != nullptr
        && filelist[0][0] != '\0')
    {
        selected_folder = filelist[0];
        MikuPan_ApplySelectedDataFolder(selected_folder);
    }

    {
        std::lock_guard<std::mutex> lock(missing_data_dialog_mutex);
        missing_data_dialog_selected = !selected_folder.empty();
        missing_data_dialog_open = false;
    }
    missing_data_dialog_cv.notify_all();
}

extern "C" void MikuPan_ServiceMissingDataFolderDialog()
{
    std::string missing_path_to_show;

    {
        std::lock_guard<std::mutex> lock(missing_data_dialog_mutex);

        if (missing_data_dialog_requested && !missing_data_dialog_open)
        {
            missing_path_to_show = missing_data_path;
            missing_data_dialog_requested = false;
            missing_data_dialog_open = true;
        }
    }

    if (missing_path_to_show.empty())
    {
        return;
    }

    const std::string message =
        "The game data file was not found:\n\n" + missing_path_to_show
        + "\n\nSelect the folder that contains the Fatal Frame data files. "
          "The selected folder will be saved to the configuration file.";

#ifndef __ANDROID__
    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR,
                             "File not found",
                             message.c_str(),
                             nullptr);
#endif

    const char *start = mikupan_configuration.data_folder[0] != '\0'
                            ? mikupan_configuration.data_folder
                            : nullptr;
#ifdef __ANDROID__
    SDL_SetHint("SDL_ANDROID_ALLOW_PERSISTENT_FOLDER_ACCESS", "1");
#endif
    SDL_ShowOpenFolderDialog(MikuPan_MissingDataFolderSelected,
                             nullptr,
                             nullptr,
                             start,
                             false);
}

void MikuPan_LoadImgHdFile()
{
    std::string data_path_string = MikuPan_GetDataPathString("IMG_HD.BIN");
    if (MikuPan_IsSdlAssetPathString(data_path_string))
    {
        if (MikuPan_GetSdlFileSize(data_path_string.c_str()) == 0)
        {
            if (MikuPan_RequestMissingDataFolderDialog("IMG_HD.BIN"))
            {
                data_path_string = MikuPan_GetDataPathString("IMG_HD.BIN");
            }
        }

        if (MikuPan_GetSdlFileSize(data_path_string.c_str()) == 0)
        {
            return;
        }

        MikuPan_ReadFullFile(data_path_string.c_str(),
            static_cast<char *>(MikuPan_GetHostPointer(ImgHdAddress)));
        return;
    }

    auto data_path = std::filesystem::path(data_path_string);
    if (!std::filesystem::exists(data_path))
    {
        const auto old_root = MikuPan_GetDataRoot();
        if (MikuPan_RequestMissingDataFolderDialog(data_path))
        {
            data_path_string = MikuPan_GetDataPathString("IMG_HD.BIN");
            if (MikuPan_IsSdlAssetPathString(data_path_string))
            {
                MikuPan_LoadImgHdFile();
                return;
            }
            data_path = MikuPan_RebasePathAfterDataFolderSelection(data_path,
                                                                   old_root);
        }

        if (!std::filesystem::exists(data_path))
        {
            return;
        }
    }

    const auto path = data_path.generic_string();
    MikuPan_ReadFullFile(path.c_str(),
        static_cast<char *>(MikuPan_GetHostPointer(ImgHdAddress)));
}

void MikuPan_ReadFullFile(const char *filename, char *buffer)
{
    std::string asset_path;
    if (MikuPan_IsSdlAssetPath(filename, &asset_path))
    {
        const u_int file_size = MikuPan_GetSdlFileSize(asset_path.c_str());
        if (file_size == 0 || buffer == nullptr)
        {
            return;
        }

        MikuPan_ReadSdlFileRange(asset_path.c_str(), 0, buffer, file_size);
        return;
    }

    std::filesystem::path path_filename(filename);

    if (!std::filesystem::exists(path_filename))
    {
        const auto old_root = MikuPan_GetDataRoot();
        if (MikuPan_RequestMissingDataFolderDialog(path_filename))
        {
            path_filename =
                MikuPan_RebasePathAfterDataFolderSelection(path_filename,
                                                           old_root);
        }

        if (!std::filesystem::exists(path_filename))
        {
            return;
        }
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


void MikuPan_ReadFileInArchive(int sector, int size, u_int *address)
{
    std::string archive_string = MikuPan_GetDataPathString("IMG_BD.BIN");

    if (MikuPan_IsSdlAssetPathString(archive_string))
    {
        if (MikuPan_GetSdlFileSize(archive_string.c_str()) == 0)
        {
            if (MikuPan_RequestMissingDataFolderDialog("IMG_BD.BIN"))
            {
                archive_string = MikuPan_GetDataPathString("IMG_BD.BIN");
            }
        }

        if (MikuPan_GetSdlFileSize(archive_string.c_str()) == 0)
        {
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

        info_log("PS2 Address 0x%x",
                 MikuPan_GetPs2OffsetFromHostPointer(address));

        MikuPan_ReadSdlFileRange(archive_string.c_str(),
                                 static_cast<Sint64>(sector) * 0x800,
                                 address,
                                 static_cast<size_t>(size));
        return;
    }

    auto archive = std::filesystem::path(archive_string);
    if (!std::filesystem::exists(archive))
    {
        spdlog::critical("IMG_BD.BIN not found!");
        const auto old_root = MikuPan_GetDataRoot();
        if (MikuPan_RequestMissingDataFolderDialog(archive))
        {
            archive_string = MikuPan_GetDataPathString("IMG_BD.BIN");
            if (MikuPan_IsSdlAssetPathString(archive_string))
            {
                MikuPan_ReadFileInArchive(sector, size, address);
                return;
            }
            archive = MikuPan_RebasePathAfterDataFolderSelection(archive,
                                                                 old_root);
        }

        if (!std::filesystem::exists(archive))
        {
            return;
        }
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

    info_log("PS2 Address 0x%x", MikuPan_GetPs2OffsetFromHostPointer(address));

    std::ifstream infile(archive, std::ios::binary);
    infile.seekg(sector * 0x800, std::ios::beg);
    infile.read(reinterpret_cast<char *>(address), size);

    infile.close();
}

u_int MikuPan_GetDataFileSize(const char *relative_name)
{
    const std::string path_string = MikuPan_GetDataPathString(relative_name);

    if (MikuPan_IsSdlAssetPathString(path_string))
    {
        return MikuPan_GetSdlFileSize(path_string.c_str());
    }

    std::error_code ec;
    const auto size = std::filesystem::file_size(path_string, ec);
    if (ec)
    {
        return 0;
    }

    return static_cast<u_int>(size);
}

bool MikuPan_ReadDataFileRange(const char *relative_name, int64_t offset,
                               void *buffer, size_t size)
{
    void *file = MikuPan_OpenDataFile(relative_name);
    const bool read = MikuPan_ReadOpenDataFile(file, offset, buffer, size);
    MikuPan_CloseDataFile(file);
    return read;
}

void *MikuPan_OpenDataFile(const char *relative_name)
{
    const std::string path_string = MikuPan_GetDataPathString(relative_name);
    return MikuPan_OpenStream(path_string.c_str());
}

SDL_IOStream *MikuPan_OpenStream(const char *path)
{
    if (MikuPan_IsDiscPathString(path))
    {
        return MikuPan_DiscImageOpenFile(path + std::strlen(kDiscPathPrefix));
    }

    return SDL_IOFromFile(path, "rb");
}

bool MikuPan_ReadOpenDataFile(void *file, int64_t offset, void *buffer,
                              size_t size)
{
    SDL_IOStream *io = static_cast<SDL_IOStream *>(file);

    if (io == nullptr || buffer == nullptr
        || SDL_SeekIO(io, offset, SDL_IO_SEEK_SET) < 0)
    {
        return false;
    }

    auto *dst = static_cast<uint8_t *>(buffer);
    size_t remaining = size;

    while (remaining > 0)
    {
        const size_t got = SDL_ReadIO(io, dst, remaining);

        if (got == 0)
        {
            return false;
        }

        dst += got;
        remaining -= got;
    }

    return true;
}

void MikuPan_CloseDataFile(void *file)
{
    if (file != nullptr)
    {
        SDL_CloseIO(static_cast<SDL_IOStream *>(file));
    }
}

void MikuPan_BufferFile(int sector, int size, int64_t address)
{
    std::string archive_string = MikuPan_GetDataPathString("IMG_BD.BIN");

    if (MikuPan_IsSdlAssetPathString(archive_string))
    {
        if (MikuPan_GetSdlFileSize(archive_string.c_str()) == 0)
        {
            if (MikuPan_RequestMissingDataFolderDialog("IMG_BD.BIN"))
            {
                archive_string = MikuPan_GetDataPathString("IMG_BD.BIN");
            }
        }

        if (MikuPan_GetSdlFileSize(archive_string.c_str()) == 0)
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

        MikuPan_ReadSdlFileRange(archive_string.c_str(),
                                 static_cast<Sint64>(sector) * 0x800,
                                 file_ptr,
                                 static_cast<size_t>(size));
        return;
    }

    auto archive = std::filesystem::path(archive_string);
    if (!std::filesystem::exists(archive))
    {
        const auto old_root = MikuPan_GetDataRoot();
        if (MikuPan_RequestMissingDataFolderDialog(archive))
        {
            archive_string = MikuPan_GetDataPathString("IMG_BD.BIN");
            if (MikuPan_IsSdlAssetPathString(archive_string))
            {
                MikuPan_BufferFile(sector, size, address);
                return;
            }
            archive = MikuPan_RebasePathAfterDataFolderSelection(archive,
                                                                 old_root);
        }

        if (!std::filesystem::exists(archive))
        {
            return;
        }
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
    inFile.open(relative_path, std::ios::binary);
    inFile.read(static_cast<char *>(buffer), size);
    inFile.close();
    return inFile.bad() == 0;
}

u_char MikuPan_WriteFile(const char *filename, const void *buffer, int size)
{
    if (size < 0 || (size && !buffer)) return 0;
    const auto path = std::filesystem::path(MikuPan_GetRelativePath(filename));
    std::error_code error;
    if (!path.parent_path().empty())
    {
        std::filesystem::create_directories(path.parent_path(), error);
        if (error)
        {
            spdlog::error("Failed to create user data directory {}: {}",
                          path.parent_path().generic_string(), error.message());
            return 0;
        }
    }
    std::ofstream file(path, std::ios::binary);
    if (!file) return 0;
    if (size) file.write(static_cast<const char *>(buffer), size);
    file.close();
    return !file.fail();
}

u_int MikuPan_GetFileSize(const char *filename)
{
    std::string asset_path;
    if (MikuPan_IsSdlAssetPath(filename, &asset_path))
    {
        return MikuPan_GetSdlFileSize(asset_path.c_str());
    }

    std::filesystem::path path(filename);
    if (!std::filesystem::exists(path))
    {
        const auto old_root = MikuPan_GetDataRoot();
        if (MikuPan_RequestMissingDataFolderDialog(path))
        {
            path = MikuPan_RebasePathAfterDataFolderSelection(path, old_root);
        }

        if (!std::filesystem::exists(path))
        {
            return 0;
        }
    }

    return std::filesystem::file_size(path);
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

    /// The disc image and picked files live outside the data root that the
    /// lookups below are built on.
    const std::string data_path = MikuPan_GetDataPathString(s);

    if (!s.empty() && MikuPan_PathHasContent(data_path)) {
        s = data_path;
    } else if (!s.empty()) {
#ifdef __ANDROID__
        const std::filesystem::path relative_data_path(s);
        const bool required_archive =
            MikuPan_IsRequiredArchiveDataFile(relative_data_path);
        const char *folder = mikupan_configuration.data_folder;
        if (folder != nullptr && MikuPan_IsAndroidTreeUriString(folder))
        {
            std::string resolved_path = MikuPan_GetDataPathString(s);
            if (MikuPan_GetSdlFileSize(resolved_path.c_str()) == 0)
            {
                if (!required_archive)
                {
                    return false;
                }

                if (MikuPan_RequestMissingDataFolderDialog(relative_data_path))
                {
                    resolved_path = MikuPan_GetDataPathString(s);
                }

                if (MikuPan_GetSdlFileSize(resolved_path.c_str()) == 0)
                {
                    return false;
                }
            }
            s = resolved_path;
        }
        else
#endif
        {
        const std::filesystem::path relative_data_path(s);
        std::filesystem::path resolved_path =
            MikuPan_GetDataRoot() / relative_data_path;
        if (!std::filesystem::exists(resolved_path)) {
            if (!MikuPan_IsRequiredArchiveDataFile(relative_data_path))
            {
                return false;
            }

            if (MikuPan_RequestMissingDataFolderDialog(resolved_path)) {
                resolved_path = MikuPan_GetDataRoot() / relative_data_path;
            }

            if (!std::filesystem::exists(resolved_path))
            {
                return false;
            }
        }
        s = resolved_path.generic_string();
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

bool MikuPan_ResolveBasePath(const char* path, char* buffer, size_t buffer_size)
{
    if (!path)
    {
        spdlog::error("MikuPan_ResolveBasePath: path is null");
        return false;
    }

    if (!buffer)
    {
        spdlog::error("MikuPan_ResolveBasePath: buffer is null");
        return false;
    }

    if (buffer_size == 0)
    {
        spdlog::error("MikuPan_ResolveBasePath: buffer_size is 0");
        return false;
    }

    std::string asset_path;
    if (MikuPan_IsSdlAssetPath(path, &asset_path))
    {
        if (asset_path.length() + 1 > buffer_size)
        {
            spdlog::error(
                "MikuPan_ResolveBasePath: Buffer too small. Need {} bytes, got {}",
                asset_path.length() + 1, buffer_size);
            return false;
        }

        std::strcpy(buffer, asset_path.c_str());
        return true;
    }

    std::filesystem::path resolved_path(path);
    if (resolved_path.is_relative())
    {
        const char *base = SDL_GetBasePath();
        if (base != nullptr && std::strncmp(base, "assets://", 9) == 0)
        {
            std::string relative =
                resolved_path.lexically_normal().generic_string();
            while (relative.rfind("./", 0) == 0)
            {
                relative.erase(0, 2);
            }
            while (!relative.empty() && relative.front() == '/')
            {
                relative.erase(0, 1);
            }

            const std::string resolved = std::string(base) + relative;
            if (resolved.length() + 1 > buffer_size)
            {
                spdlog::error(
                    "MikuPan_ResolveBasePath: Buffer too small. Need {} bytes, got {}",
                    resolved.length() + 1, buffer_size);
                return false;
            }

            std::strcpy(buffer, resolved.c_str());
            return true;
        }

        resolved_path = MikuPan_GetApplicationBasePath()
            / resolved_path.relative_path();
    }

    const std::string resolved =
        resolved_path.lexically_normal().generic_string();
    if (resolved.length() + 1 > buffer_size)
    {
        spdlog::error(
            "MikuPan_ResolveBasePath: Buffer too small. Need {} bytes, got {}",
            resolved.length() + 1, buffer_size);
        return false;
    }

    std::strcpy(buffer, resolved.c_str());
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
    const auto path = std::filesystem::path(MikuPan_GetRelativePath(folder));
    std::error_code error;
    std::filesystem::create_directories(path, error);
    return !error && std::filesystem::is_directory(path, error);
}

int MikuPan_GetListFiles(const char *folder, MikuPan_McTblGetDir *table, int max_entries)
{
    std::string regex_folder(folder);
    size_t wildcard = regex_folder.find('*');

    if (wildcard != std::string::npos)
    {
        regex_folder = regex_folder.substr(0, wildcard);
    }

    std::filesystem::path regex_folder_path(regex_folder);

    auto relative_path = std::filesystem::path(MikuPan_GetRelativePath(regex_folder.c_str()));

    if (!std::filesystem::is_directory(relative_path))
    {
        relative_path = relative_path.remove_filename();
    }

    int i = 0;
    for (const auto& entry : std::filesystem::recursive_directory_iterator(relative_path))
    {
        if (entry.path().compare(regex_folder_path) == std::string::npos)
        {
            continue;
        }

        if (entry.is_directory())
        {
            continue;
        }

        table[i].FileSizeByte = entry.file_size();
        strncpy(reinterpret_cast<char *>(table[i].EntryName), entry.path().filename().generic_string().c_str(), sizeof(table[i].EntryName));

        if (++i > max_entries)
        {
            break;
        }
    }

    return i;
}

std::string MikuPan_GetRelativePath(const char *path)
{
    char resolved_path[1024];
    if (MikuPan_ResolveUserPath(path, resolved_path, sizeof(resolved_path)))
    {
        return resolved_path;
    }

    return std::filesystem::path(path).relative_path().generic_string();
}
