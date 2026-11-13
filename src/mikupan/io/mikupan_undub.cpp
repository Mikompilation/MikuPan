#include "mikupan_undub.h"

#include "mikupan/io/mikupan_disc_image.h"
#include "mikupan/io/mikupan_file.h"
#include "mikupan/io/mikupan_undub_data.h"
#include "mikupan/mikupan_config.h"
#include "mikupan/ui/mikupan_ui.h"
#include "os/eeiop/cdvd/eecdvd.h"
#include "spdlog/spdlog.h"

#include <SDL3/SDL_dialog.h>
#include <SDL3/SDL_error.h>
#include <SDL3/SDL_iostream.h>
#include <SDL3/SDL_stdinc.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cctype>
#include <cstdint>
#include <cstring>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace
{

constexpr uint64_t kSectorSize = 0x800;

/// Swapped entries start this many sectors into the archive (2 GiB): past the
/// end of IMG_BD.BIN of every English release, while the Japanese one still
/// ends below 4 GiB, which the 32-bit byte offsets of the IOP code reach.
constexpr uint32_t kJpSectorBase = 0x100000;
constexpr uint64_t kJpOffset = kJpSectorBase * kSectorSize;
constexpr uint64_t kMaxArchiveSize = 0xFFFFFFFFull;

/// Paths the file layer hands out for the undub's files.
constexpr const char* kUndubPrefix = "undub://";
constexpr const char* kHdName = "IMG_HD.BIN";
constexpr const char* kBdName = "IMG_BD.BIN";

constexpr const char* kNotJapaneseDisc =
    "That is not the Japanese release of the game (Zero, SLPS-25074).";
constexpr const char* kIncompleteDisc =
    "The Japanese disc is incomplete. Select it again.";

/// "movie\\scn0010.pss" and "MOVIE/SCN0010.PSS" name the same file.
std::string NormalizeName(const std::string& name)
{
    std::string normalized = name;
    std::replace(normalized.begin(), normalized.end(), '\\', '/');

    while (normalized.rfind("./", 0) == 0)
    {
        normalized.erase(0, 2);
    }

    while (!normalized.empty() && normalized.front() == '/')
    {
        normalized.erase(0, 1);
    }

    std::transform(normalized.begin(), normalized.end(), normalized.begin(),
                   [](unsigned char ch) {
                       return static_cast<char>(std::toupper(ch));
                   });
    return normalized;
}

bool EndsWithNoCase(const std::string& text, const char* suffix)
{
    const size_t length = std::strlen(suffix);
    if (text.size() < length)
    {
        return false;
    }

    for (size_t i = 0; i < length; i++)
    {
        if (std::tolower(static_cast<unsigned char>(text[text.size() - length + i]))
            != std::tolower(static_cast<unsigned char>(suffix[i])))
        {
            return false;
        }
    }

    return true;
}

size_t ReadAt(SDL_IOStream* io, uint64_t offset, void* buffer, size_t size)
{
    if (SDL_SeekIO(io, static_cast<Sint64>(offset), SDL_IO_SEEK_SET) < 0)
    {
        return 0;
    }

    auto* dst = static_cast<uint8_t*>(buffer);
    size_t done = 0;

    while (done < size)
    {
        const size_t got = SDL_ReadIO(io, dst + done, size - done);
        if (got == 0)
        {
            break;
        }
        done += got;
    }

    return done;
}

bool ReadAll(SDL_IOStream* io, std::vector<uint8_t>* out)
{
    if (io == nullptr)
    {
        return false;
    }

    const Sint64 size = SDL_GetIOSize(io);
    bool read = size > 0;

    if (read)
    {
        out->resize(static_cast<size_t>(size));
        read = ReadAt(io, 0, out->data(), out->size()) == out->size();
    }

    SDL_CloseIO(io);
    return read;
}

int64_t StreamSize(SDL_IOStream* io)
{
    if (io == nullptr)
    {
        return -1;
    }

    const Sint64 size = SDL_GetIOSize(io);
    SDL_CloseIO(io);
    return size;
}

/// The Japanese disc: an image, or a folder holding the disc's files.
struct JpSource
{
    std::shared_ptr<const MikuPan_DiscIndex> image;
    /// With a trailing separator; empty for an image.
    std::string folder;

    SDL_IOStream* Open(const std::string& name) const
    {
        if (image != nullptr)
        {
            return MikuPan_DiscIndexOpenFile(*image, name);
        }

        /// Discs name their files in upper case; a copy on a case-sensitive
        /// file system may not.
        SDL_IOStream* io = SDL_IOFromFile((folder + name).c_str(), "rb");
        if (io == nullptr)
        {
            std::string lower = name;
            std::transform(lower.begin(), lower.end(), lower.begin(),
                           [](unsigned char ch) {
                               return static_cast<char>(std::tolower(ch));
                           });
            io = SDL_IOFromFile((folder + lower).c_str(), "rb");
        }

        return io;
    }

    bool Has(const std::string& name) const
    {
        if (image != nullptr)
        {
            return MikuPan_DiscIndexHasFile(*image, name);
        }

        SDL_IOStream* io = Open(name);
        if (io == nullptr)
        {
            return false;
        }

        SDL_CloseIO(io);
        return true;
    }
};

/// Opens the disc at `path` and checks that it is the Japanese release, with
/// every entry the undub takes from it. `hd` gets its IMG_HD.BIN.
bool OpenJpSource(const std::string& path, JpSource* source,
                  std::vector<uint8_t>* hd, uint64_t* bd_size,
                  std::string* problem)
{
    if (EndsWithNoCase(path, ".bin"))
    {
        const size_t separator = path.find_last_of("/\\");
        if (path.rfind("content://", 0) == 0 || separator == std::string::npos)
        {
            *problem = "Select the Japanese disc image, or IMG_HD.BIN in a "
                       "folder that holds the disc's files.";
            return false;
        }

        source->folder = path.substr(0, separator + 1);
    }
    else
    {
        source->image = MikuPan_DiscImageIndex(path.c_str(), problem);
        if (source->image == nullptr)
        {
            return false;
        }
    }

    /// One entry per file, and each release has its own number of files.
    if (!ReadAll(source->Open(kHdName), hd)
        || hd->size() != MIKUPAN_UNDUB_JP_FILE_COUNT * 8u)
    {
        *problem = kNotJapaneseDisc;
        return false;
    }

    const int64_t size = StreamSize(source->Open(kBdName));
    if (size <= 0 || size % kSectorSize != 0)
    {
        *problem = kIncompleteDisc;
        return false;
    }

    *bd_size = static_cast<uint64_t>(size);

    for (const MikuPan_UndubFile& file : mikupan_undub_files)
    {
        uint32_t start;
        uint32_t length;
        std::memcpy(&start, &(*hd)[file.jp_file_no * 8], 4);
        std::memcpy(&length, &(*hd)[file.jp_file_no * 8 + 4], 4);

        if (length == 0 || start * kSectorSize + length > *bd_size)
        {
            *problem = kIncompleteDisc;
            return false;
        }
    }

    return true;
}

/// What the undub plays this run. Made once, before anything reads the
/// archive, and never changed or freed after, so every thread may read it.
struct UndubState
{
    JpSource source;
    /// This release's archive, as the file layer finds it without the undub.
    std::string local_bd_path;
    uint64_t local_bd_size = 0;
    uint64_t jp_bd_size = 0;
    /// This release's IMG_HD.BIN, with the swapped entries pointing at
    /// kJpSectorBase + their sector on the Japanese disc.
    std::vector<uint8_t> hd;

    uint64_t ArchiveSize() const
    {
        return kJpOffset + jp_bd_size;
    }
};

std::atomic<const UndubState*> g_state{nullptr};

const UndubState* GetState()
{
    return g_state.load(std::memory_order_acquire);
}

/// What this run started with, for the options screen. Main thread only.
struct RunInfo
{
    bool activated = false;
    bool enabled = false;
    std::string disc;
    std::string problem;
};

RunInfo g_run;

/// The European release keeps 50 Hz copies of its movies in MOVIE3 and
/// MOVIE4 ("SCN0010P.PSS"); the Japanese disc has the 60 Hz ones only.
std::string JapaneseMovieName(const std::string& name)
{
    if (!EndsWithNoCase(name, ".PSS"))
    {
        return std::string();
    }

    const size_t slash = name.find('/');
    if (slash == std::string::npos || name.find('/', slash + 1) != std::string::npos)
    {
        return std::string();
    }

    const std::string folder = name.substr(0, slash);
    const std::string file = name.substr(slash + 1);

    if (folder == "MOVIE" || folder == "MOVIE2")
    {
        return name;
    }

    if ((folder == "MOVIE3" || folder == "MOVIE4") && file.size() > 5
        && file[file.size() - 5] == 'P')
    {
        return (folder == "MOVIE3" ? "MOVIE/" : "MOVIE2/")
               + file.substr(0, file.size() - 5) + ".PSS";
    }

    return std::string();
}

/// IMG_BD.BIN of the undub: this release's archive, then the Japanese one
/// from kJpOffset. Each stream opens the files it needs on its first read
/// of them, since the IOP opens one per sector read.
struct ArchiveStream
{
    const UndubState* state;
    SDL_IOStream* local;
    SDL_IOStream* jp;
    Sint64 position;
};

Sint64 SDLCALL ArchiveStreamSize(void* userdata)
{
    return static_cast<Sint64>(
        static_cast<ArchiveStream*>(userdata)->state->ArchiveSize());
}

Sint64 SDLCALL ArchiveStreamSeek(void* userdata, Sint64 offset,
                                 SDL_IOWhence whence)
{
    auto* stream = static_cast<ArchiveStream*>(userdata);
    Sint64 position = offset;

    if (whence == SDL_IO_SEEK_CUR)
    {
        position += stream->position;
    }
    else if (whence == SDL_IO_SEEK_END)
    {
        position += static_cast<Sint64>(stream->state->ArchiveSize());
    }

    if (position < 0)
    {
        SDL_SetError("Seek before the start of the archive");
        return -1;
    }

    stream->position = position;
    return position;
}

size_t SDLCALL ArchiveStreamRead(void* userdata, void* buffer, size_t size,
                                 SDL_IOStatus* status)
{
    auto* stream = static_cast<ArchiveStream*>(userdata);
    const UndubState& state = *stream->state;
    const uint64_t end = state.ArchiveSize();
    auto* dst = static_cast<uint8_t*>(buffer);
    size_t done = 0;

    while (done < size && static_cast<uint64_t>(stream->position) < end)
    {
        const uint64_t at = static_cast<uint64_t>(stream->position);
        size_t got;

        if (at < state.local_bd_size)
        {
            if (stream->local == nullptr)
            {
                stream->local = MikuPan_OpenStream(state.local_bd_path.c_str());
            }

            const size_t want = static_cast<size_t>(
                std::min<uint64_t>(size - done, state.local_bd_size - at));
            got = stream->local != nullptr
                      ? ReadAt(stream->local, at, dst + done, want)
                      : 0;
        }
        else if (at < kJpOffset)
        {
            /// Between the two archives: nothing points here.
            got = static_cast<size_t>(
                std::min<uint64_t>(size - done, kJpOffset - at));
            std::memset(dst + done, 0, got);
        }
        else
        {
            if (stream->jp == nullptr)
            {
                stream->jp = state.source.Open(kBdName);
            }

            const size_t want = static_cast<size_t>(
                std::min<uint64_t>(size - done, end - at));
            got = stream->jp != nullptr
                      ? ReadAt(stream->jp, at - kJpOffset, dst + done, want)
                      : 0;
        }

        if (got == 0)
        {
            *status = SDL_IO_STATUS_ERROR;
            return done;
        }

        done += got;
        stream->position += static_cast<Sint64>(got);
    }

    if (done < size)
    {
        *status = SDL_IO_STATUS_EOF;
    }

    return done;
}

bool SDLCALL ArchiveStreamClose(void* userdata)
{
    auto* stream = static_cast<ArchiveStream*>(userdata);

    if (stream->local != nullptr)
    {
        SDL_CloseIO(stream->local);
    }

    if (stream->jp != nullptr)
    {
        SDL_CloseIO(stream->jp);
    }

    delete stream;
    return true;
}

SDL_IOStream* OpenArchive(const UndubState* state)
{
    auto* stream = new ArchiveStream{state, nullptr, nullptr, 0};

    SDL_IOStreamInterface iface;
    SDL_INIT_INTERFACE(&iface);
    iface.size = ArchiveStreamSize;
    iface.seek = ArchiveStreamSeek;
    iface.read = ArchiveStreamRead;
    iface.close = ArchiveStreamClose;

    SDL_IOStream* opened = SDL_OpenIO(&iface, stream);
    if (opened == nullptr)
    {
        delete stream;
    }

    return opened;
}

/// Builds what this run plays, or says why it cannot.
std::unique_ptr<UndubState> MakeState(const std::string& disc,
                                      std::string* problem)
{
    auto state = std::make_unique<UndubState>();
    std::vector<uint8_t> jp_hd;

    if (!OpenJpSource(disc, &state->source, &jp_hd, &state->jp_bd_size,
                      problem))
    {
        return nullptr;
    }

    const std::string local_hd_path = MikuPan_GetGameDataPath(kHdName);
    state->local_bd_path = MikuPan_GetGameDataPath(kBdName);

    const int64_t local_bd_size =
        StreamSize(MikuPan_OpenStream(state->local_bd_path.c_str()));

    if (!ReadAll(MikuPan_OpenStream(local_hd_path.c_str()), &state->hd)
        || state->hd.size() != static_cast<size_t>(GetFileNameCount()) * 8u
        || local_bd_size <= 0)
    {
        *problem = "The game data could not be read.";
        return nullptr;
    }

    state->local_bd_size = static_cast<uint64_t>(local_bd_size);

    if (state->local_bd_size > kJpOffset
        || state->ArchiveSize() > kMaxArchiveSize)
    {
        spdlog::error("Undub: archives of {} and {} bytes do not fit",
                      state->local_bd_size, state->jp_bd_size);
        *problem = "The game data is too large to add the Japanese disc to.";
        return nullptr;
    }

    std::array<int, MIKUPAN_UNDUB_KIND_COUNT> counts{};

    for (const MikuPan_UndubFile& file : mikupan_undub_files)
    {
        uint32_t start;
        uint32_t size;
        std::memcpy(&start, &jp_hd[file.jp_file_no * 8], 4);
        std::memcpy(&size, &jp_hd[file.jp_file_no * 8 + 4], 4);

        start += kJpSectorBase;
        std::memcpy(&state->hd[file.file_no * 8], &start, 4);
        std::memcpy(&state->hd[file.file_no * 8 + 4], &size, 4);
        counts[file.kind]++;
    }

    spdlog::info("Undub: {} voices, {} sound banks and {} model files from {}",
                 counts[MIKUPAN_UNDUB_VOICE],
                 counts[MIKUPAN_UNDUB_SOUND_BANK],
                 counts[MIKUPAN_UNDUB_MODEL], disc);
    return state;
}

/// The file picker answers on a thread of its own.
std::mutex g_browser_mutex;
bool g_browser_open = false;
bool g_browser_picked = false;
std::string g_browser_path;
/// Why the last disc picked cannot be used. Main thread only.
std::string g_pick_problem;

void SDLCALL DiscBrowsed(void* userdata, const char* const* filelist,
                         int filter)
{
    (void) userdata;
    (void) filter;

    std::lock_guard<std::mutex> lock(g_browser_mutex);
    g_browser_open = false;

    if (filelist != nullptr && filelist[0] != nullptr && filelist[0][0] != '\0')
    {
        g_browser_picked = true;
        g_browser_path = filelist[0];
    }
}

} // namespace

extern "C" void MikuPan_Undub_Activate(void)
{
    if (g_run.activated)
    {
        return;
    }

    g_run.activated = true;
    g_run.enabled = mikupan_configuration.undub_enabled != 0;
    g_run.disc = mikupan_configuration.undub_disc_path;

    if (!g_run.enabled)
    {
        return;
    }

    if (g_run.disc.empty())
    {
        g_run.problem = "Select the Japanese disc to play the undub.";
        return;
    }

    for (const MikuPan_UndubFile& file : mikupan_undub_files)
    {
        if (file.file_no < 0 || file.file_no >= GetFileNameCount()
            || file.jp_file_no < 0
            || file.jp_file_no >= MIKUPAN_UNDUB_JP_FILE_COUNT)
        {
            spdlog::error("Undub: entry {} is not in this release", file.file_no);
            g_run.problem = "The undub does not know this release.";
            return;
        }
    }

    std::unique_ptr<UndubState> state = MakeState(g_run.disc, &g_run.problem);

    if (state == nullptr)
    {
        spdlog::warn("Undub is off: {}", g_run.problem);
        return;
    }

    /// Kept for the rest of the run: streams point into it.
    g_state.store(state.release(), std::memory_order_release);
}

extern "C" void MikuPan_Undub_Deactivate(const char* reason)
{
    if (GetState() == nullptr)
    {
        return;
    }

    /// Not freed: a stream opened before may still read through it.
    g_state.store(nullptr, std::memory_order_release);
    g_run.problem = reason != nullptr ? reason : "";
    spdlog::warn("Undub is off: {}", g_run.problem);
}

extern "C" bool MikuPan_Undub_IsActive(void)
{
    return GetState() != nullptr;
}

extern "C" bool MikuPan_Undub_GetAdpcmVolume(int file_no, unsigned short* vol)
{
    if (GetState() == nullptr || vol == nullptr)
    {
        return false;
    }

    for (const MikuPan_UndubVolume& entry : mikupan_undub_volumes)
    {
        if (entry.file_no == file_no)
        {
            *vol = entry.vol;
            return true;
        }
    }

    return false;
}

extern "C" void MikuPan_Undub_BrowseForDisc(void)
{
    /// SDL reads these until the picker closes.
    static const SDL_DialogFileFilter filters[] = {
        {"Disc images", "iso"},
        {"Game data", "bin"},
        {"All files", "*"},
    };

    {
        std::lock_guard<std::mutex> lock(g_browser_mutex);
        if (g_browser_open)
        {
            return;
        }
        g_browser_open = true;
    }

    /// Start where the disc picked last time is.
    static std::string start;
    start = mikupan_configuration.undub_disc_path;
    const size_t separator = start.find_last_of("/\\");
    start = separator != std::string::npos && start.rfind("content://", 0) != 0
                ? start.substr(0, separator + 1)
                : std::string();

    SDL_ShowOpenFileDialog(DiscBrowsed, nullptr, MikuPan_GetUiWindow(), filters,
                           SDL_arraysize(filters),
                           start.empty() ? nullptr : start.c_str(), false);
}

extern "C" void MikuPan_Undub_ServiceBrowser(void)
{
    std::string path;

    {
        std::lock_guard<std::mutex> lock(g_browser_mutex);
        if (!g_browser_picked)
        {
            return;
        }

        g_browser_picked = false;
        path.swap(g_browser_path);
    }

    JpSource source;
    std::vector<uint8_t> hd;
    uint64_t bd_size = 0;

    if (!OpenJpSource(path, &source, &hd, &bd_size, &g_pick_problem))
    {
        spdlog::warn("Undub: {} cannot be used: {}", path, g_pick_problem);
        return;
    }

    g_pick_problem.clear();

    if (path.size() >= sizeof(mikupan_configuration.undub_disc_path))
    {
        g_pick_problem = "The path of that disc is too long.";
        return;
    }

    std::strncpy(mikupan_configuration.undub_disc_path, path.c_str(),
                 sizeof(mikupan_configuration.undub_disc_path) - 1);
    mikupan_configuration
        .undub_disc_path[sizeof(mikupan_configuration.undub_disc_path) - 1] =
        '\0';
    mikupan_configuration.undub_enabled = 1;

    if (!MikuPan_SaveConfiguration(nullptr))
    {
        spdlog::error("Failed to save the Japanese disc: {}", path);
    }
}

std::string MikuPan_Undub_GetDataPath(const std::string& relative)
{
    const UndubState* state = GetState();
    if (state == nullptr)
    {
        return std::string();
    }

    const std::string name = NormalizeName(relative);

    if (name == kHdName || name == kBdName)
    {
        return kUndubPrefix + name;
    }

    const std::string movie = JapaneseMovieName(name);
    if (!movie.empty() && state->source.Has(movie))
    {
        return kUndubPrefix + movie;
    }

    return std::string();
}

bool MikuPan_Undub_IsUndubPath(const std::string& path)
{
    return path.rfind(kUndubPrefix, 0) == 0;
}

SDL_IOStream* MikuPan_Undub_OpenStream(const std::string& path)
{
    const UndubState* state = GetState();
    if (state == nullptr || !MikuPan_Undub_IsUndubPath(path))
    {
        return nullptr;
    }

    const std::string name = path.substr(std::strlen(kUndubPrefix));

    if (name == kHdName)
    {
        return SDL_IOFromConstMem(state->hd.data(), state->hd.size());
    }

    if (name == kBdName)
    {
        return OpenArchive(state);
    }

    return state->source.Open(name);
}

std::string MikuPan_Undub_GetStatus()
{
    if (!g_pick_problem.empty())
    {
        return g_pick_problem;
    }

    if (MikuPan_Undub_IsRestartPending())
    {
        return "Restart MikuPan to apply the change.";
    }

    if (MikuPan_Undub_IsActive())
    {
        return "Playing the Japanese voices, sound effects, movies and "
               "Miku's original model.";
    }

    return g_run.enabled ? g_run.problem : std::string();
}

bool MikuPan_Undub_IsRestartPending()
{
    if (!g_run.activated)
    {
        return false;
    }

    const bool enabled = mikupan_configuration.undub_enabled != 0;
    return enabled != g_run.enabled
           || (enabled && g_run.disc != mikupan_configuration.undub_disc_path);
}
