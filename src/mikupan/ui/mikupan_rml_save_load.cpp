#include "mikupan/ui/mikupan_rml_save_load.h"
#include "mikupan/ui/mikupan_rmlui.h"

#include "common.h"
#include "enums.h"
#include "graphics/graph2d/message.h"
#include "graphics/graph2d/tim2.h"
#include "ingame/info/inf_disp.h"
#include "ingame/menu/ig_menu.h"
#include "main/glob.h"
#include "mc/mc.h"
#include "mc/mc_exec.h"
#include "mc/mc_main.h"
#include "mikupan/mikupan_memory.h"
#include "mikupan/mikupan_screenshot.h"
#include "mikupan/io/mikupan_file.h"

#include "RmlUi/Core.h"
#include "RmlUi/Core/ElementDocument.h"
#include "RmlUi/Core/EventListener.h"

#include <algorithm>
#include <array>
#include <charconv>
#include <chrono>
#include <cstdio>
#include <ctime>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <system_error>
#include <vector>

int MikuPan_ReadResolvedFramebufferRGBA8TopLeft(int width,
                                                int height,
                                                unsigned char* out_rgba);

struct MikuPanSaveGameplayData
{
    INGAME_WRK ingame{};
    CAM_CUSTOM_WRK camera{};
    std::array<u_char, sizeof(poss_item)> items{};
    bool available = false;
};

struct MikuPanSaveEntry
{
    std::filesystem::path path;
    std::filesystem::path thumbnail_path;
    std::string night_name;
    std::string room_name;
    std::string saved_at;
    MC_GAME_HEADER header{};
    MikuPanSaveGameplayData gameplay{};
    std::time_t modified_time = 0;
    int file_number = -1;
    int display_slot_number = -1;
    bool mikupan_save = false;
};

class MikuPanSaveLoadSelectionListener final : public Rml::EventListener
{
public:
    MikuPanSaveLoadSelectionListener(int in_selection, bool in_activate)
        : selection(in_selection), activate(in_activate)
    {
    }

    void ProcessEvent(Rml::Event& event) override;

private:
    int selection = 0;
    bool activate = false;
};

class MikuPanSaveLoadCancelListener final : public Rml::EventListener
{
public:
    void ProcessEvent(Rml::Event& event) override;
};

class MikuPanSaveLoadExitSelectionListener final : public Rml::EventListener
{
public:
    void ProcessEvent(Rml::Event& event) override;
};

class MikuPanSaveLoadConfirmListener final : public Rml::EventListener
{
public:
    explicit MikuPanSaveLoadConfirmListener(bool in_confirm)
        : confirm(in_confirm)
    {
    }

    void ProcessEvent(Rml::Event& event) override;

private:
    bool confirm = false;
};

enum class MikuPanSaveLoadDialogMode
{
    None,
    Entry,
    NewSave,
    SaveComplete
};

struct MikuPanRmlSaveLoadState
{
    Rml::Context* context = nullptr;
    Rml::ElementDocument* document = nullptr;
    Rml::Element* root = nullptr;
    Rml::Element* header_board = nullptr;
    Rml::Element* title = nullptr;
    Rml::Element* exit_button = nullptr;
    Rml::Element* exit_label = nullptr;
    Rml::Element* list = nullptr;
    Rml::Element* detail = nullptr;
    Rml::Element* status = nullptr;
    Rml::Element* confirm_dialog = nullptr;
    Rml::Element* confirm_message = nullptr;
    Rml::Element* confirm_yes = nullptr;
    Rml::Element* confirm_yes_label = nullptr;
    Rml::Element* confirm_no = nullptr;
    std::vector<MikuPanSaveEntry> entries;
    std::vector<std::unique_ptr<Rml::EventListener>> static_listeners;
    std::vector<std::unique_ptr<Rml::EventListener>> dynamic_listeners;
    std::vector<unsigned char> preview_pixels;
    int selected = 0;
    int detail_selection = -2;
    int confirm_entry = -1;
    int confirm_selection = 1;
    int queued_result = MIKUPAN_RML_SAVE_LOAD_RESULT_NONE;
    bool initialized = false;
    bool open = false;
    bool save_mode = false;
    MikuPanSaveLoadDialogMode dialog_mode = MikuPanSaveLoadDialogMode::None;
    bool exit_selected = false;
    bool mc_active = false;
    bool detail_icons_ready = false;
};

static MikuPanRmlSaveLoadState g_save_load;
static constexpr int kPreviewWidth = 320;
static constexpr int kPreviewHeight = 180;
static bool MikuPan_RmlSaveLoadPreviewUsable(
    const std::vector<unsigned char>& pixels)
{
    const size_t expected_size = static_cast<size_t>(
        kPreviewWidth * kPreviewHeight * 4);
    if (pixels.size() != expected_size)
    {
        return false;
    }

    size_t sampled = 0;
    size_t visible = 0;
    for (size_t pixel = 0; pixel < expected_size; pixel += 64)
    {
        const unsigned char red = pixels[pixel];
        const unsigned char green = pixels[pixel + 1];
        const unsigned char blue = pixels[pixel + 2];
        sampled++;
        if (std::max({red, green, blue}) > 10)
        {
            visible++;
        }
    }

    return sampled > 0 && visible * 200 >= sampled;
}
#ifdef BUILD_EU_VERSION
static constexpr int kSavePreviewPackageAddress = 0x1d23680;
#else
static constexpr int kSavePreviewPackageAddress = 0x1d28c80;
#endif

static Rml::Element* MikuPan_RmlSaveLoadGetElement(const char* id)
{
    return g_save_load.document != nullptr
        ? g_save_load.document->GetElementById(id)
        : nullptr;
}

static void MikuPan_RmlSaveLoadSetHidden(Rml::Element* element, bool hidden)
{
    if (element != nullptr)
    {
        element->SetClass("hidden", hidden);
    }
}

static void MikuPan_RmlSaveLoadApplyHeaderTexture(void)
{
    if (g_save_load.header_board != nullptr)
    {
        g_save_load.header_board->SetProperty(
            "decorator",
            "image(\"mikupan-tm2-header://pl_stts/0005.tm2\")");
    }
}

static std::string MikuPan_RmlSaveLoadEscape(const std::string& value,
                                             bool attribute)
{
    std::string escaped;
    escaped.reserve(value.size() + 16);
    for (char ch : value)
    {
        switch (ch)
        {
        case '&':
            escaped += "&amp;";
            break;
        case '<':
            escaped += "&lt;";
            break;
        case '>':
            escaped += "&gt;";
            break;
        case '"':
            escaped += attribute ? "&quot;" : "\"";
            break;
        default:
            escaped += ch;
            break;
        }
    }
    return escaped;
}

static std::time_t MikuPan_RmlSaveLoadToTimeT(
    std::filesystem::file_time_type file_time)
{
    const auto system_time = std::chrono::time_point_cast<
        std::chrono::system_clock::duration>(
            file_time - std::filesystem::file_time_type::clock::now()
            + std::chrono::system_clock::now());
    return std::chrono::system_clock::to_time_t(system_time);
}

static std::string MikuPan_RmlSaveLoadFormatDate(std::time_t time)
{
    std::tm local{};
#if defined(_WIN32)
    localtime_s(&local, &time);
#else
    localtime_r(&time, &local);
#endif
    char buffer[64];
    std::snprintf(buffer,
                  sizeof(buffer),
                  "%04d/%02d/%02d %02d:%02d",
                  local.tm_year + 1900,
                  local.tm_mon + 1,
                  local.tm_mday,
                  local.tm_hour,
                  local.tm_min);
    return buffer;
}

static std::filesystem::path MikuPan_RmlSaveLoadDirectory(void)
{
    mcSetPathDir(0);
    return std::filesystem::path(MikuPan_GetRelativePath(mc_ctrl.rw.path));
}

static bool MikuPan_RmlSaveLoadIsMikuPanSaveFilename(
    const std::filesystem::path& path)
{
    const std::string filename = path.filename().string();
    return filename.rfind("MikuPanSave", 0) == 0
        && path.extension() == ".dat";
}

static bool MikuPan_RmlSaveLoadIsSaveFilename(
    const std::filesystem::path& directory,
    const std::filesystem::path& path)
{
    const std::string filename = path.filename().string();
    if (MikuPan_RmlSaveLoadIsMikuPanSaveFilename(path))
    {
        return true;
    }

    std::string legacy_prefix = directory.filename().string();
    if (!legacy_prefix.empty() && legacy_prefix.back() == '0')
    {
        legacy_prefix.pop_back();
    }

    if (legacy_prefix.empty() || filename.rfind(legacy_prefix, 0) != 0)
    {
        return false;
    }

    const std::string suffix = filename.substr(legacy_prefix.size());
    return !suffix.empty()
        && std::all_of(suffix.begin(), suffix.end(), [](unsigned char ch) {
               return ch >= '0' && ch <= '9';
           });
}

static int MikuPan_RmlSaveLoadFileNumber(
    const std::filesystem::path& directory,
    const std::filesystem::path& path)
{
    const std::string stem = path.stem().string();
    size_t begin = 0;
    const std::string mikupan_prefix = "MikuPanSave";
    if (stem.rfind(mikupan_prefix, 0) == 0)
    {
        begin = mikupan_prefix.size();
    }
    else
    {
        std::string legacy_prefix = directory.filename().string();
        if (!legacy_prefix.empty() && legacy_prefix.back() == '0')
        {
            legacy_prefix.pop_back();
        }
        if (legacy_prefix.empty() || stem.rfind(legacy_prefix, 0) != 0)
        {
            return -1;
        }
        begin = legacy_prefix.size();
    }

    if (begin >= stem.size())
    {
        return -1;
    }

    int slot_number = -1;
    const char* first = stem.data() + begin;
    const char* last = stem.data() + stem.size();
    const auto result = std::from_chars(first, last, slot_number);
    return result.ec == std::errc() && result.ptr == last
        ? slot_number
        : -1;
}

static bool MikuPan_RmlSaveLoadReadHeader(const std::filesystem::path& path,
                                          MC_GAME_HEADER& header)
{
    std::ifstream input(path, std::ios::binary);
    if (!input)
    {
        return false;
    }

    input.seekg(16, std::ios::beg);
    input.read(reinterpret_cast<char*>(&header), sizeof(header));
    return input.good() && header.map_flg == 1;
}

static bool MikuPan_RmlSaveLoadReadGameplayData(
    const std::filesystem::path& path,
    MikuPanSaveGameplayData& gameplay)
{
    gameplay = MikuPanSaveGameplayData();

    std::ifstream input(path, std::ios::binary);
    if (!input)
    {
        return false;
    }

    input.seekg(16, std::ios::beg);
    if (!input)
    {
        return false;
    }

    bool found_ingame = false;
    bool found_camera = false;
    bool found_items = false;

    for (u_long index = 0; index < mc_gamedata_str_num; index++)
    {
        const MC_DATA_STR& section = mc_gamedata_str[index];
        if (section.size < 0)
        {
            return false;
        }

        void* destination = nullptr;
        size_t destination_size = 0;
        if (section.addr == reinterpret_cast<uintptr_t>(&ingame_wrk))
        {
            destination = &gameplay.ingame;
            destination_size = sizeof(gameplay.ingame);
            found_ingame = true;
        }
        else if (section.addr == reinterpret_cast<uintptr_t>(&cam_custom_wrk))
        {
            destination = &gameplay.camera;
            destination_size = sizeof(gameplay.camera);
            found_camera = true;
        }
        else if (section.addr == reinterpret_cast<uintptr_t>(poss_item))
        {
            destination = gameplay.items.data();
            destination_size = gameplay.items.size();
            found_items = true;
        }

        if (destination == nullptr)
        {
            input.seekg(section.size, std::ios::cur);
        }
        else
        {
            const size_t copy_size = std::min(
                static_cast<size_t>(section.size), destination_size);
            input.read(static_cast<char*>(destination),
                       static_cast<std::streamsize>(copy_size));
            if (static_cast<size_t>(section.size) > copy_size)
            {
                input.seekg(
                    static_cast<std::streamoff>(
                        static_cast<size_t>(section.size) - copy_size),
                    std::ios::cur);
            }
        }

        if (!input)
        {
            return false;
        }

        if (found_ingame && found_camera && found_items)
        {
            gameplay.available = true;
            return true;
        }
    }

    return false;
}

static char MikuPan_RmlSaveLoadGlyphToAscii(u_char glyph)
{
    if (glyph == 0)
    {
        return ' ';
    }

    for (int character = 32; character <= 126; character++)
    {
        if (ascii_font_tbl[character - 32] == glyph)
        {
            return static_cast<char>(character);
        }
    }

    return '\0';
}

static std::string MikuPan_RmlSaveLoadGameText(u_char kind, int number)
{
    const auto* source = reinterpret_cast<const u_char*>(
        GetIngameMSGAddr(kind, number));
    if (source == nullptr)
    {
        return {};
    }

    std::string result;
    bool previous_space = true;
    for (int consumed = 0; consumed < 512;)
    {
        const u_char code = *source++;
        consumed++;

        if (code == 0xff)
        {
            break;
        }

        switch (code)
        {
        case 0xf0:
        case 0xf1:
        case 0xf2:
        case 0xf3:
        case 0xf7:
        case 0xf8:
            source++;
            consumed++;
            continue;
        case 0xf5:
        case 0xf9:
        case 0xfd:
            source += 3;
            consumed += 3;
            continue;
        case 0xf6:
            source += 4;
            consumed += 4;
            continue;
        case 0xfb:
            source += 2;
            consumed += 2;
            continue;
        case 0xfa:
        case 0xfe:
            if (!previous_space)
            {
                result.push_back(' ');
                previous_space = true;
            }
            continue;
        case 0xfc:
            continue;
        default:
            break;
        }

        const char character = MikuPan_RmlSaveLoadGlyphToAscii(code);
        if (character == '\0')
        {
            continue;
        }

        if (character == ' ')
        {
            if (!previous_space)
            {
                result.push_back(character);
                previous_space = true;
            }
        }
        else
        {
            result.push_back(character);
            previous_space = false;
        }
    }

    while (!result.empty() && result.back() == ' ')
    {
        result.pop_back();
    }
    return result;
}

static std::string MikuPan_RmlSaveLoadNightName(
    const MC_GAME_HEADER& header)
{
    int mission = header.msn_no;
    if (header.msn_flg != 0)
    {
        mission = std::min(mission, 5);
    }

    std::string name = MikuPan_RmlSaveLoadGameText(
        8, mission + 101);
    if (!name.empty())
    {
        return name;
    }

    if (mission == 0)
    {
        return "Prologue";
    }
    return "Night " + std::to_string(mission);
}

static std::string MikuPan_RmlSaveLoadRoomName(
    const MC_GAME_HEADER& header)
{
    std::string name = MikuPan_RmlSaveLoadGameText(30, header.room_no);
    if (!name.empty())
    {
        return name;
    }

    char buffer[64];
    std::snprintf(buffer, sizeof(buffer), "Room %03u", header.room_no);
    return buffer;
}

static int MikuPan_RmlSaveLoadOriginalPreviewSprite(
    const MC_GAME_HEADER& header)
{
    if (header.msn_flg == 0)
    {
        switch (header.room_no)
        {
        case 0:
            return SP_R000;
        case 6:
            return SP_R006;
        case 14:
            return SP_R014;
        case 15:
            return SP_R015;
        case 21:
            return SP_R021;
        case 31:
            return SP_R031;
        case 41:
            return SP_R041;
        default:
            return -1;
        }
    }

    switch (std::min<int>(header.msn_no, 5))
    {
    case 0:
        return SP_R046;
    case 1:
        return SP_R042;
    case 2:
        return SP_R043;
    case 3:
        return SP_R044;
    case 4:
        return SP_R045;
    case 5:
        return SP_R046;
    default:
        return -1;
    }
}

static bool MikuPan_RmlSaveLoadTextureMatches(
    const TIM2_PICTUREHEADER& picture,
    const SPRT_DAT& sprite)
{
    const auto* picture_tex0 = reinterpret_cast<const sceGsTex0*>(
        &picture.GsTex0);
    const auto* sprite_tex0 = reinterpret_cast<const sceGsTex0*>(
        &sprite.tex0);

    return picture_tex0->TBP0 == sprite_tex0->TBP0
        && picture_tex0->TBW == sprite_tex0->TBW
        && picture_tex0->PSM == sprite_tex0->PSM
        && picture_tex0->TW == sprite_tex0->TW
        && picture_tex0->TH == sprite_tex0->TH
        && picture_tex0->CBP == sprite_tex0->CBP;
}

static TIM2_PICTUREHEADER* MikuPan_RmlSaveLoadFindPreviewPicture(
    const SPRT_DAT& sprite)
{
    const int64_t package_address = MikuPan_GetHostAddress(
        kSavePreviewPackageAddress);
    if (package_address <= 0)
    {
        return nullptr;
    }

    const auto* package = reinterpret_cast<const int*>(package_address);
    const int texture_count = package[0];
    if (texture_count <= 0 || texture_count > 128)
    {
        return nullptr;
    }

    const int* offsets = package + 4;
    for (int index = 0; index < texture_count; index++)
    {
        const int offset = offsets[index];
        if (offset < 16 || offset > 16 * 1024 * 1024)
        {
            continue;
        }

        void* tim2 = reinterpret_cast<void*>(package_address + offset);
        if (Tim2CheckFileHeaer(tim2) == 0)
        {
            continue;
        }

        auto* picture = Tim2GetPictureHeader(tim2, 0);
        if (picture != nullptr
            && MikuPan_RmlSaveLoadTextureMatches(*picture, sprite))
        {
            return picture;
        }
    }

    return nullptr;
}

static bool MikuPan_RmlSaveLoadWriteOriginalPreview(
    const std::filesystem::path& path,
    const MC_GAME_HEADER& header)
{
    const int sprite_index = MikuPan_RmlSaveLoadOriginalPreviewSprite(header);
    if (sprite_index < 0)
    {
        return false;
    }

    const SPRT_DAT& sprite = spr_dat[sprite_index];
    TIM2_PICTUREHEADER* picture =
        MikuPan_RmlSaveLoadFindPreviewPicture(sprite);
    if (picture == nullptr || sprite.w <= 0 || sprite.h <= 0)
    {
        return false;
    }

    std::vector<unsigned char> source_pixels(
        static_cast<size_t>(sprite.w * sprite.h * 4));
    for (int y = 0; y < sprite.h; y++)
    {
        for (int x = 0; x < sprite.w; x++)
        {
            const u_int color = Tim2GetTextureColor(
                picture, 0, 0, sprite.u + x, sprite.v + y);
            const size_t destination = static_cast<size_t>(
                (y * sprite.w + x) * 4);
            source_pixels[destination + 0] =
                static_cast<unsigned char>(color & 0xff);
            source_pixels[destination + 1] =
                static_cast<unsigned char>((color >> 16) & 0xff);
            source_pixels[destination + 2] =
                static_cast<unsigned char>((color >> 8) & 0xff);
            source_pixels[destination + 3] =
                static_cast<unsigned char>(
                    std::min(255u, ((color >> 24) & 0xff) * 2u));
        }
    }

    int preview_width = kPreviewWidth;
    int preview_height = sprite.h * kPreviewWidth / sprite.w;
    if (preview_height > kPreviewHeight)
    {
        preview_height = kPreviewHeight;
        preview_width = sprite.w * kPreviewHeight / sprite.h;
    }
    preview_width = std::max(preview_width, 1);
    preview_height = std::max(preview_height, 1);

    const int preview_x = (kPreviewWidth - preview_width) / 2;
    const int preview_y = (kPreviewHeight - preview_height) / 2;
    std::vector<unsigned char> pixels(
        static_cast<size_t>(kPreviewWidth * kPreviewHeight * 4), 0);
    for (size_t index = 3; index < pixels.size(); index += 4)
    {
        pixels[index] = 255;
    }

    for (int y = 0; y < preview_height; y++)
    {
        const int source_y = y * sprite.h / preview_height;
        for (int x = 0; x < preview_width; x++)
        {
            const int source_x = x * sprite.w / preview_width;
            const size_t source = static_cast<size_t>(
                (source_y * sprite.w + source_x) * 4);
            const size_t destination = static_cast<size_t>(
                ((preview_y + y) * kPreviewWidth + preview_x + x) * 4);
            std::copy_n(source_pixels.data() + source,
                        4,
                        pixels.data() + destination);
        }
    }

    std::error_code error;
    std::filesystem::create_directories(path.parent_path(), error);
    if (error)
    {
        return false;
    }

    return MikuPan_ScreenshotWritePng(path.string().c_str(),
                                      pixels.data(),
                                      kPreviewWidth,
                                      kPreviewHeight) != 0;
}

static std::filesystem::path MikuPan_RmlSaveLoadOriginalPreviewPath(
    const std::filesystem::path& directory,
    const MC_GAME_HEADER& header)
{
    char filename[64];
    if (header.msn_flg == 0)
    {
        std::snprintf(filename,
                      sizeof(filename),
                      "room_%03u.png",
                      header.room_no);
    }
    else
    {
        std::snprintf(filename,
                      sizeof(filename),
                      "ending_%02u.png",
                      std::min<int>(header.msn_no, 5));
    }
    return directory / "MikuPanPreviewCache" / filename;
}

static void MikuPan_RmlSaveLoadScan(void)
{
    g_save_load.entries.clear();

    const std::filesystem::path directory = MikuPan_RmlSaveLoadDirectory();
    std::error_code error;
    std::filesystem::create_directories(directory, error);
    error.clear();

    std::filesystem::directory_iterator iterator(directory, error);
    std::filesystem::directory_iterator end;
    for (; !error && iterator != end; iterator.increment(error))
    {
        const std::filesystem::directory_entry& directory_entry = *iterator;
        if (!directory_entry.is_regular_file(error)
            || !MikuPan_RmlSaveLoadIsSaveFilename(directory,
                                                  directory_entry.path()))
        {
            error.clear();
            continue;
        }

        MikuPanSaveEntry entry;
        if (!MikuPan_RmlSaveLoadReadHeader(directory_entry.path(), entry.header))
        {
            continue;
        }

        entry.path = directory_entry.path();
        MikuPan_RmlSaveLoadReadGameplayData(entry.path, entry.gameplay);
        entry.mikupan_save = MikuPan_RmlSaveLoadIsMikuPanSaveFilename(
            entry.path);
        entry.file_number = MikuPan_RmlSaveLoadFileNumber(directory, entry.path);
        entry.night_name = MikuPan_RmlSaveLoadNightName(entry.header);
        entry.room_name = MikuPan_RmlSaveLoadRoomName(entry.header);

        std::filesystem::path captured_preview = entry.path;
        captured_preview.replace_extension(".png");

        error.clear();
        if (entry.mikupan_save
            && std::filesystem::is_regular_file(captured_preview, error))
        {
            entry.thumbnail_path = captured_preview;
        }
        else
        {
            const std::filesystem::path original_preview =
                MikuPan_RmlSaveLoadOriginalPreviewPath(directory, entry.header);
            error.clear();
            if (std::filesystem::is_regular_file(original_preview, error)
                || MikuPan_RmlSaveLoadWriteOriginalPreview(original_preview,
                                                           entry.header))
            {
                entry.thumbnail_path = original_preview;
            }
        }
        error.clear();

        const auto modified = directory_entry.last_write_time(error);
        if (!error)
        {
            entry.modified_time = MikuPan_RmlSaveLoadToTimeT(modified);
            entry.saved_at = MikuPan_RmlSaveLoadFormatDate(entry.modified_time);
        }
        else
        {
            entry.saved_at = "Unknown date";
            error.clear();
        }

        g_save_load.entries.push_back(std::move(entry));
    }

    for (MikuPanSaveEntry& entry : g_save_load.entries)
    {
        entry.display_slot_number = entry.mikupan_save && entry.file_number >= 0
            ? entry.file_number
            : -1;
    }

    std::sort(g_save_load.entries.begin(),
              g_save_load.entries.end(),
              [](const MikuPanSaveEntry& left, const MikuPanSaveEntry& right) {
                  if (left.mikupan_save != right.mikupan_save)
                  {
                      return left.mikupan_save;
                  }
                  if (left.mikupan_save
                      && left.file_number != right.file_number)
                  {
                      return left.file_number > right.file_number;
                  }
                  return left.modified_time > right.modified_time;
              });
}

static std::string MikuPan_RmlSaveLoadPlayTime(const MC_GAME_HEADER& header)
{
    char buffer[64];
    std::snprintf(buffer,
                  sizeof(buffer),
                  "%02u:%02u:%02u",
                  header.hour,
                  header.minute,
                  header.sec);
    return buffer;
}

static std::string MikuPan_RmlSaveLoadDifficulty(const MC_GAME_HEADER& header)
{
    switch (header.difficult)
    {
    case 0:
        return "Normal";
    case 1:
        return "Nightmare";
    default:
        return "Difficulty " + std::to_string(header.difficult);
    }
}

static std::string MikuPan_RmlSaveLoadFormatNumber(uint64_t value)
{
    std::string result = std::to_string(value);
    for (size_t position = result.size(); position > 3; position -= 3)
    {
        result.insert(position - 3, 1, ',');
    }
    return result;
}

static unsigned int MikuPan_RmlSaveLoadItemCount(
    const MikuPanSaveEntry& entry,
    int item_no)
{
    if (!entry.gameplay.available || item_no < 0
        || item_no >= static_cast<int>(entry.gameplay.items.size()))
    {
        return 0;
    }
    return entry.gameplay.items[static_cast<size_t>(item_no)];
}

static std::string MikuPan_RmlSaveLoadItemCell(
    const MikuPanSaveEntry& entry,
    int item_no,
    bool icons_ready)
{
    const unsigned int count = MikuPan_RmlSaveLoadItemCount(entry, item_no);
    std::string classes = "save-detail-item";
    if (count == 0)
    {
        classes += " empty";
    }

    std::string rml = "<span class=\"" + classes + "\">";
    if (icons_ready)
    {
        rml += "<img class=\"save-detail-item-icon\" src=\""
            "mikupan-item-icon://"
            + std::to_string(item_no)
            + "\"/>";
    }
    else
    {
        rml += "<span class=\"save-detail-item-icon loading\"></span>";
    }

    rml += "<span class=\"save-detail-item-count\">&#215;"
        + std::to_string(count)
        + "</span></span>";
    return rml;
}

static std::string MikuPan_RmlSaveLoadItemRows(
    const MikuPanSaveEntry& entry,
    bool icons_ready)
{
    std::string rml =
        "<div class=\"save-detail-items-panel\">"
        "<span class=\"save-detail-items-backing "
        "save-detail-items-backing-left\"></span>"
        "<span class=\"save-detail-items-backing "
        "save-detail-items-backing-center\"></span>"
        "<span class=\"save-detail-items-backing "
        "save-detail-items-backing-right\"></span>"
        "<div class=\"save-detail-items-row\">";
    rml += MikuPan_RmlSaveLoadItemCell(entry, 6, icons_ready);
    rml += MikuPan_RmlSaveLoadItemCell(entry, 7, icons_ready);
    rml += MikuPan_RmlSaveLoadItemCell(entry, 8, icons_ready);
    rml += MikuPan_RmlSaveLoadItemCell(entry, 1, icons_ready);
    rml += MikuPan_RmlSaveLoadItemCell(entry, 2, icons_ready);
    rml += MikuPan_RmlSaveLoadItemCell(entry, 3, icons_ready);
    rml += MikuPan_RmlSaveLoadItemCell(entry, 4, icons_ready);
    rml += "</div></div>";
    return rml;
}

static std::string MikuPan_RmlSaveLoadThumbnailSource(
    const MikuPanSaveEntry& entry)
{
    std::error_code error;
    if (!std::filesystem::is_regular_file(entry.thumbnail_path, error))
    {
        return {};
    }

    std::error_code absolute_error;
    const std::filesystem::path absolute = std::filesystem::absolute(
        entry.thumbnail_path, absolute_error);
    std::string source = "mikupan-save-preview://"
        + (absolute_error ? entry.thumbnail_path : absolute).generic_string();
    const auto modified = std::filesystem::last_write_time(entry.thumbnail_path,
                                                            error);
    if (!error)
    {
        source += "?v="
            + std::to_string(MikuPan_RmlSaveLoadToTimeT(modified));
    }
    return source;
}

static void MikuPan_RmlSaveLoadAddDynamicListener(
    Rml::Element* element,
    Rml::EventId event_id,
    std::unique_ptr<Rml::EventListener> listener)
{
    if (element == nullptr || listener == nullptr)
    {
        return;
    }

    element->AddEventListener(event_id, listener.get());
    g_save_load.dynamic_listeners.push_back(std::move(listener));
}

static std::string MikuPan_RmlSaveLoadEntryBacking(void)
{
    return
        "<span class=\"save-entry-backing save-entry-backing-left\"></span>"
        "<span class=\"save-entry-backing save-entry-backing-center\"></span>"
        "<span class=\"save-entry-backing save-entry-backing-right\"></span>"
        "<span class=\"save-entry-divider save-entry-divider-left\"></span>"
        "<span class=\"save-entry-divider save-entry-divider-center\"></span>"
        "<span class=\"save-entry-divider save-entry-divider-right\"></span>";
}

static void MikuPan_RmlSaveLoadBuildList(void)
{
    if (g_save_load.list == nullptr)
    {
        return;
    }

    const std::string entry_backing = MikuPan_RmlSaveLoadEntryBacking();
    std::string rml;
    int selection = 0;
    if (g_save_load.save_mode)
    {
        rml +=
            "<button id=\"save-entry-0\" class=\"save-list-item save-create-item\">"
            + entry_backing
            + "<div class=\"save-create-copy\">"
            "<div class=\"save-create-title\">Create a new save</div>"
            "</div></button>";
        selection = 1;
    }

    for (const MikuPanSaveEntry& entry : g_save_load.entries)
    {
        std::string save_label;
        if (entry.mikupan_save && entry.display_slot_number >= 0)
        {
            char slot_label[32];
            std::snprintf(slot_label,
                          sizeof(slot_label),
                          "Save %03d",
                          entry.display_slot_number);
            save_label = slot_label;
        }

        const std::string item_class = entry.mikupan_save
            ? "save-list-item"
            : "save-list-item legacy-save";
        rml += "<button id=\"save-entry-"
            + std::to_string(selection)
            + "\" class=\""
            + item_class
            + "\">"
            + entry_backing;
        if (!save_label.empty())
        {
            rml += "<span class=\"save-entry-index\">"
                + save_label
                + "</span>";
        }
        rml += "<span class=\"save-entry-night-row\">"
            + MikuPan_RmlSaveLoadEscape(entry.night_name, false)
            + "</span><span class=\"save-entry-room-row\">"
            + MikuPan_RmlSaveLoadEscape(entry.room_name, false)
            + "</span><span class=\"save-entry-date-row\">"
            + MikuPan_RmlSaveLoadEscape(entry.saved_at, false)
            + "</span></button>";
        selection++;
    }

    if (g_save_load.entries.empty() && !g_save_load.save_mode)
    {
        rml += "<div class=\"save-empty-list\">No save files found.</div>";
    }

    g_save_load.list->SetInnerRML(rml);
    g_save_load.dynamic_listeners.clear();
    g_save_load.detail_selection = -2;

    const int total = static_cast<int>(g_save_load.entries.size())
        + (g_save_load.save_mode ? 1 : 0);
    g_save_load.selected = total > 0
        ? std::clamp(g_save_load.selected, 0, total - 1)
        : 0;

    for (int i = 0; i < total; i++)
    {
        Rml::Element* element = MikuPan_RmlSaveLoadGetElement(
            ("save-entry-" + std::to_string(i)).c_str());
        MikuPan_RmlSaveLoadAddDynamicListener(
            element,
            Rml::EventId::Mouseover,
            std::make_unique<MikuPanSaveLoadSelectionListener>(i, false));
        MikuPan_RmlSaveLoadAddDynamicListener(
            element,
            Rml::EventId::Click,
            std::make_unique<MikuPanSaveLoadSelectionListener>(i, true));
    }
}

static void MikuPan_RmlSaveLoadSetDetailPlaceholder(
    const std::string& title,
    const std::string& copy)
{
    if (g_save_load.detail == nullptr)
    {
        return;
    }

    std::string classes = "save-detail-placeholder";
    if (copy.empty())
    {
        classes += " single-line";
    }

    std::string rml =
        "<div class=\"" + classes + "\">"
        "<div class=\"save-detail-placeholder-title\">"
        + MikuPan_RmlSaveLoadEscape(title, false)
        + "</div>";
    if (!copy.empty())
    {
        rml +=
            "<div class=\"save-detail-placeholder-copy\">"
            + MikuPan_RmlSaveLoadEscape(copy, false)
            + "</div>";
    }
    rml += "</div>";
    g_save_load.detail->SetInnerRML(rml);
}

static void MikuPan_RmlSaveLoadSyncDetail(void)
{
    if (g_save_load.detail == nullptr
        || g_save_load.detail_selection == g_save_load.selected)
    {
        return;
    }

    g_save_load.detail_selection = g_save_load.selected;
    if (g_save_load.save_mode && g_save_load.selected == 0)
    {
        MikuPan_RmlSaveLoadSetDetailPlaceholder(
            "Create a new save file",
            {});
        return;
    }

    const int entry_index = g_save_load.selected
        - (g_save_load.save_mode ? 1 : 0);
    if (entry_index < 0
        || entry_index >= static_cast<int>(g_save_load.entries.size()))
    {
        MikuPan_RmlSaveLoadSetDetailPlaceholder(
            "No save selected",
            "Select a save from the list above.");
        return;
    }

    const MikuPanSaveEntry& entry = g_save_load.entries[entry_index];
    const std::string thumbnail = MikuPan_RmlSaveLoadThumbnailSource(entry);
    const std::string new_game_plus = entry.header.clear_flg != 0
        ? " new-game-plus"
        : "";
    const std::string legacy_class = entry.mikupan_save
        ? ""
        : " legacy-save";

    std::string save_label = "Legacy Save";
    if (entry.mikupan_save && entry.display_slot_number >= 0)
    {
        char slot_label[32];
        std::snprintf(slot_label,
                      sizeof(slot_label),
                      "Save %03d",
                      entry.display_slot_number);
        save_label = slot_label;
    }

    std::string thumbnail_rml;
    if (thumbnail.empty())
    {
        thumbnail_rml =
            "<div class=\"save-detail-thumbnail-crop "
            "save-detail-thumbnail-empty\">"
            + MikuPan_RmlSaveLoadEscape(entry.room_name, false)
            + "</div>";
    }
    else
    {
        thumbnail_rml =
            "<div class=\"save-detail-thumbnail-crop\"><img src=\""
            + MikuPan_RmlSaveLoadEscape(thumbnail, true)
            + "\"/></div>";
    }

    const unsigned int clear_count = entry.gameplay.available
        ? entry.gameplay.ingame.clear_count
        : entry.header.clear_flg;
    const std::string clear_rml = clear_count != 0
        ? "<span class=\"save-detail-clear\">Clear &#215;"
            + std::to_string(clear_count)
            + "</span>"
        : "";

    const std::string shots = entry.gameplay.available
        ? MikuPan_RmlSaveLoadFormatNumber(entry.gameplay.ingame.pht_cnt)
        : "--";
    const std::string points = entry.gameplay.available
        ? MikuPan_RmlSaveLoadFormatNumber(entry.gameplay.camera.point)
        : "--";

    const std::string item_rows = entry.gameplay.available
        ? MikuPan_RmlSaveLoadItemRows(entry, g_save_load.detail_icons_ready)
        : "<div class=\"save-detail-items-unavailable\">"
          "Item data unavailable"
          "</div>";

    g_save_load.detail->SetInnerRML(
        "<div class=\"save-detail-content"
        + legacy_class
        + "\"><div class=\"save-detail-thumbnail-frame"
        + new_game_plus
        + "\">"
        + thumbnail_rml
        + "<span class=\"save-detail-thumbnail-border\"></span></div>"
        "<div class=\"save-detail-copy\">"
        "<div class=\"save-detail-heading\">"
        "<span class=\"save-detail-label\">"
        + MikuPan_RmlSaveLoadEscape(save_label, false)
        + "</span><span class=\"save-detail-date\">"
        + MikuPan_RmlSaveLoadEscape(entry.saved_at, false)
        + "</span></div>"
        "<div class=\"save-detail-location-line\">"
        "<span class=\"save-detail-location\">"
        + MikuPan_RmlSaveLoadEscape(entry.night_name, false)
        + "  -  "
        + MikuPan_RmlSaveLoadEscape(entry.room_name, false)
        + "</span>"
        + clear_rml
        + "</div>"
        "<div class=\"save-detail-stats\">"
        "<span class=\"save-detail-difficulty\">"
        + MikuPan_RmlSaveLoadDifficulty(entry.header)
        + "</span>"
        "<span class=\"save-detail-time\">Play "
        + MikuPan_RmlSaveLoadPlayTime(entry.header)
        + "</span>"
        "<span class=\"save-detail-shots\">Shots "
        + shots
        + "</span>"
        "<span class=\"save-detail-points\">Points "
        + points
        + "</span>"
        "</div><div class=\"save-detail-rule\"></div>"
        + item_rows
        + "</div></div>");
}

static int MikuPan_RmlSaveLoadTotalItems(void)
{
    return static_cast<int>(g_save_load.entries.size())
        + (g_save_load.save_mode ? 1 : 0);
}

static void MikuPan_RmlSaveLoadSyncSelection(bool scroll)
{
    const int total = MikuPan_RmlSaveLoadTotalItems();
    for (int i = 0; i < total; i++)
    {
        Rml::Element* element = MikuPan_RmlSaveLoadGetElement(
            ("save-entry-" + std::to_string(i)).c_str());
        if (element != nullptr)
        {
            element->SetClass(
                "selected", !g_save_load.exit_selected
                    && i == g_save_load.selected);
        }
    }

    if (g_save_load.exit_button != nullptr)
    {
        g_save_load.exit_button->SetClass("selected",
                                          g_save_load.exit_selected);
    }

    MikuPan_RmlSaveLoadSyncDetail();

    if (scroll && total > 0 && !g_save_load.exit_selected)
    {
        Rml::Element* selected = MikuPan_RmlSaveLoadGetElement(
            ("save-entry-" + std::to_string(g_save_load.selected)).c_str());
        if (selected != nullptr)
        {
            selected->ScrollIntoView(false);
        }
    }
}

static void MikuPan_RmlSaveLoadSetStatus(const std::string& text,
                                         bool error)
{
    if (g_save_load.status != nullptr)
    {
        g_save_load.status->SetInnerRML(MikuPan_RmlSaveLoadEscape(text, false));
        g_save_load.status->SetClass("error", error);
    }
}

static void MikuPan_RmlSaveLoadSyncConfirm(void)
{
    const bool open = g_save_load.dialog_mode
        != MikuPanSaveLoadDialogMode::None;
    const bool notice = g_save_load.dialog_mode
        == MikuPanSaveLoadDialogMode::SaveComplete;

    if (g_save_load.root != nullptr)
    {
        g_save_load.root->SetClass("modal-open", open);
    }
    MikuPan_RmlSaveLoadSetHidden(g_save_load.confirm_dialog, !open);
    if (g_save_load.confirm_dialog != nullptr)
    {
        g_save_load.confirm_dialog->SetClass("notice", notice);
    }
    if (g_save_load.confirm_yes_label != nullptr)
    {
        g_save_load.confirm_yes_label->SetInnerRML(notice ? "OK" : "Yes");
    }
    if (g_save_load.confirm_yes != nullptr)
    {
        g_save_load.confirm_yes->SetClass(
            "selected", notice || g_save_load.confirm_selection == 0);
    }
    if (g_save_load.confirm_no != nullptr)
    {
        MikuPan_RmlSaveLoadSetHidden(g_save_load.confirm_no, notice);
        g_save_load.confirm_no->SetClass(
            "selected", !notice && g_save_load.confirm_selection != 0);
    }
    if (open)
    {
        if (notice || g_save_load.confirm_selection == 0)
        {
            if (g_save_load.confirm_yes != nullptr)
            {
                g_save_load.confirm_yes->Focus();
            }
        }
        else if (g_save_load.confirm_no != nullptr)
        {
            g_save_load.confirm_no->Focus();
        }
    }
}

static std::filesystem::path MikuPan_RmlSaveLoadNewPath(void)
{
    const std::filesystem::path directory = MikuPan_RmlSaveLoadDirectory();
    int highest = 0;
    std::error_code error;
    std::filesystem::directory_iterator iterator(directory, error);
    std::filesystem::directory_iterator end;
    for (; !error && iterator != end; iterator.increment(error))
    {
        const std::filesystem::directory_entry& entry = *iterator;
        if (!entry.is_regular_file(error)
            || !MikuPan_RmlSaveLoadIsMikuPanSaveFilename(entry.path()))
        {
            error.clear();
            continue;
        }

        highest = std::max(
            highest, MikuPan_RmlSaveLoadFileNumber(directory, entry.path()));
    }

    char filename[64];
    std::snprintf(filename, sizeof(filename), "MikuPanSave%04d.dat", highest + 1);
    return directory / filename;
}

static void MikuPan_RmlSaveLoadCloseInternal(void)
{
    if (!g_save_load.open)
    {
        return;
    }

    g_save_load.open = false;
    g_save_load.dialog_mode = MikuPanSaveLoadDialogMode::None;
    g_save_load.confirm_entry = -1;
    g_save_load.exit_selected = false;
    if (g_save_load.root != nullptr)
    {
        g_save_load.root->SetClass("modal-open", false);
    }
    MikuPan_RmlSaveLoadSetHidden(g_save_load.root, true);
    if (g_save_load.document != nullptr)
    {
        g_save_load.document->Hide();
    }
    if (g_save_load.mc_active)
    {
        mcEnd();
        g_save_load.mc_active = false;
    }
}

static bool MikuPan_RmlSaveLoadWrite(const std::filesystem::path& path)
{
    if (!g_save_load.mc_active || mc_ctrl.work_addr == nullptr
        || mc_game_size == 0)
    {
        MikuPan_RmlSaveLoadSetStatus("Save system is not ready.", true);
        return false;
    }

    std::error_code error;
    std::filesystem::create_directories(path.parent_path(), error);
    if (error)
    {
        MikuPan_RmlSaveLoadSetStatus("Could not create the save folder.", true);
        return false;
    }

    mcMakeHeaderFile();
    mcMakeSaveFile(mc_ctrl.work_addr, MC_FILE_GAMEDATA1);

    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    if (!output)
    {
        MikuPan_RmlSaveLoadSetStatus("Could not open the save file.", true);
        return false;
    }

    output.write(reinterpret_cast<const char*>(mc_ctrl.work_addr), mc_game_size);
    output.close();
    if (!output)
    {
        MikuPan_RmlSaveLoadSetStatus("Could not write the save file.", true);
        return false;
    }

    std::filesystem::path thumbnail = path;
    thumbnail.replace_extension(".png");
    bool thumbnail_written = false;
    if (MikuPan_RmlSaveLoadPreviewUsable(g_save_load.preview_pixels))
    {
        thumbnail_written = MikuPan_ScreenshotWritePng(
            thumbnail.string().c_str(),
            g_save_load.preview_pixels.data(),
            kPreviewWidth,
            kPreviewHeight) != 0;
    }
    if (!thumbnail_written)
    {
        std::error_code thumbnail_error;
        std::filesystem::remove(thumbnail, thumbnail_error);
    }

    const std::filesystem::path saved_path = path.lexically_normal();
    MikuPan_RmlSaveLoadScan();
    g_save_load.selected = 0;
    for (size_t index = 0; index < g_save_load.entries.size(); index++)
    {
        if (g_save_load.entries[index].path.lexically_normal() == saved_path)
        {
            g_save_load.selected = static_cast<int>(index) + 1;
            break;
        }
    }

    MikuPan_RmlSaveLoadBuildList();
    MikuPan_RmlSaveLoadSyncSelection(true);
    MikuPan_RmlSaveLoadSetStatus("Save data updated.", false);
    g_save_load.dialog_mode = MikuPanSaveLoadDialogMode::SaveComplete;
    g_save_load.confirm_entry = -1;
    g_save_load.confirm_selection = 0;
    if (g_save_load.confirm_message != nullptr)
    {
        g_save_load.confirm_message->SetInnerRML("Save Complete.");
    }
    MikuPan_RmlSaveLoadSyncConfirm();
    return true;
}

static bool MikuPan_RmlSaveLoadRead(const MikuPanSaveEntry& entry)
{
    if (!g_save_load.mc_active || mc_ctrl.work_addr == nullptr
        || mc_game_size == 0)
    {
        MikuPan_RmlSaveLoadSetStatus("Load system is not ready.", true);
        return false;
    }

    std::error_code error;
    const uintmax_t size = std::filesystem::file_size(entry.path, error);
    if (error || size == 0 || size > mc_game_size)
    {
        MikuPan_RmlSaveLoadSetStatus("This save file has an invalid size.", true);
        return false;
    }

    std::memset(mc_ctrl.work_addr, 0, mc_game_size);
    std::ifstream input(entry.path, std::ios::binary);
    if (!input)
    {
        MikuPan_RmlSaveLoadSetStatus("Could not open the save file.", true);
        return false;
    }

    input.read(reinterpret_cast<char*>(mc_ctrl.work_addr),
               static_cast<std::streamsize>(size));
    if (!input)
    {
        MikuPan_RmlSaveLoadSetStatus("Could not read the save file.", true);
        return false;
    }

    mc_ctrl.rw.header_flg = 0;
    mc_ctrl.rw.size = static_cast<int>(size);
    if (mcSetLoadFile(mc_ctrl.work_addr, MC_FILE_GAMEDATA1) != 0)
    {
        MikuPan_RmlSaveLoadSetStatus("The save file is corrupt.", true);
        return false;
    }

    mcSetHeaderFile();
    g_save_load.queued_result = MIKUPAN_RML_SAVE_LOAD_RESULT_LOADED;
    MikuPan_RmlSaveLoadCloseInternal();
    return true;
}

static int MikuPan_RmlSaveLoadSelectedEntry(void)
{
    const int index = g_save_load.selected - (g_save_load.save_mode ? 1 : 0);
    return index >= 0 && index < static_cast<int>(g_save_load.entries.size())
        ? index
        : -1;
}

static bool MikuPan_RmlSaveLoadLoadDocument(void)
{
    char document_path[1024];
    if (!MikuPan_ResolveBasePath("resources/rml/save_load.rml",
                                 document_path,
                                 sizeof(document_path)))
    {
        return false;
    }

    g_save_load.document = g_save_load.context->LoadDocument(document_path);
    if (g_save_load.document == nullptr)
    {
        return false;
    }

    g_save_load.root = MikuPan_RmlSaveLoadGetElement("save-load-root");
    g_save_load.header_board =
        MikuPan_RmlSaveLoadGetElement("save-load-header-board");
    g_save_load.title = MikuPan_RmlSaveLoadGetElement("save-load-title");
    g_save_load.exit_button =
        MikuPan_RmlSaveLoadGetElement("save-load-exit");
    g_save_load.exit_label =
        MikuPan_RmlSaveLoadGetElement("save-load-exit-label");
    g_save_load.list = MikuPan_RmlSaveLoadGetElement("save-load-list");
    g_save_load.detail = MikuPan_RmlSaveLoadGetElement("save-load-detail");
    g_save_load.status = MikuPan_RmlSaveLoadGetElement("save-load-status");
    g_save_load.confirm_dialog =
        MikuPan_RmlSaveLoadGetElement("save-load-confirm");
    g_save_load.confirm_message =
        MikuPan_RmlSaveLoadGetElement("save-load-confirm-message");
    g_save_load.confirm_yes =
        MikuPan_RmlSaveLoadGetElement("save-load-confirm-yes");
    g_save_load.confirm_yes_label =
        MikuPan_RmlSaveLoadGetElement("save-load-confirm-yes-label");
    g_save_load.confirm_no =
        MikuPan_RmlSaveLoadGetElement("save-load-confirm-no");

    auto cancel_listener = std::make_unique<MikuPanSaveLoadCancelListener>();
    g_save_load.exit_button->AddEventListener(Rml::EventId::Click,
                                              cancel_listener.get());
    g_save_load.static_listeners.push_back(std::move(cancel_listener));

    auto exit_selection_listener =
        std::make_unique<MikuPanSaveLoadExitSelectionListener>();
    g_save_load.exit_button->AddEventListener(Rml::EventId::Mouseover,
                                              exit_selection_listener.get());
    g_save_load.static_listeners.push_back(
        std::move(exit_selection_listener));

    auto yes_listener = std::make_unique<MikuPanSaveLoadConfirmListener>(true);
    g_save_load.confirm_yes->AddEventListener(Rml::EventId::Click,
                                              yes_listener.get());
    g_save_load.static_listeners.push_back(std::move(yes_listener));

    auto no_listener = std::make_unique<MikuPanSaveLoadConfirmListener>(false);
    g_save_load.confirm_no->AddEventListener(Rml::EventId::Click,
                                             no_listener.get());
    g_save_load.static_listeners.push_back(std::move(no_listener));

    MikuPan_RmlSaveLoadSetHidden(g_save_load.root, true);
    MikuPan_RmlSaveLoadSetHidden(g_save_load.confirm_dialog, true);
    g_save_load.document->Hide();
    return true;
}

void MikuPanSaveLoadSelectionListener::ProcessEvent(Rml::Event& event)
{
    (void) event;
    if (g_save_load.dialog_mode != MikuPanSaveLoadDialogMode::None)
    {
        return;
    }

    g_save_load.selected = selection;
    g_save_load.exit_selected = false;
    MikuPan_RmlSaveLoadSyncSelection(false);
    if (activate)
    {
        MikuPan_RmlSaveLoadActivate();
    }
}

void MikuPanSaveLoadCancelListener::ProcessEvent(Rml::Event& event)
{
    (void) event;
    if (g_save_load.dialog_mode != MikuPanSaveLoadDialogMode::None)
    {
        return;
    }

    MikuPan_RmlSaveLoadHandleCancel();
}

void MikuPanSaveLoadExitSelectionListener::ProcessEvent(Rml::Event& event)
{
    (void) event;
    if (g_save_load.dialog_mode != MikuPanSaveLoadDialogMode::None)
    {
        return;
    }

    g_save_load.exit_selected = true;
    MikuPan_RmlSaveLoadSyncSelection(false);
}

void MikuPanSaveLoadConfirmListener::ProcessEvent(Rml::Event& event)
{
    (void) event;
    if (g_save_load.dialog_mode == MikuPanSaveLoadDialogMode::None)
    {
        return;
    }

    g_save_load.confirm_selection = confirm ? 0 : 1;
    MikuPan_RmlSaveLoadActivate();
}

bool MikuPan_RmlSaveLoadInit(Rml::Context* context)
{
    if (g_save_load.initialized)
    {
        return true;
    }

    g_save_load.context = context;
    if (context == nullptr || !MikuPan_RmlSaveLoadLoadDocument())
    {
        g_save_load = MikuPanRmlSaveLoadState();
        return false;
    }

    g_save_load.initialized = true;
    return true;
}

void MikuPan_RmlSaveLoadStartFrame(void)
{
    if (!g_save_load.initialized || !g_save_load.open)
    {
        return;
    }

    const bool icons_ready = MikuPan_RmlUiItemIconsReady() != 0;
    if (icons_ready != g_save_load.detail_icons_ready)
    {
        g_save_load.detail_icons_ready = icons_ready;
        g_save_load.detail_selection = -2;
    }

    MikuPan_RmlSaveLoadSyncSelection(false);
    MikuPan_RmlSaveLoadSyncConfirm();
}

void MikuPan_RmlSaveLoadPrepareShutdown(void)
{
    if (g_save_load.open)
    {
        MikuPan_RmlSaveLoadCloseInternal();
    }
}

void MikuPan_RmlSaveLoadShutdown(void)
{
    g_save_load = MikuPanRmlSaveLoadState();
}

extern "C" {

void MikuPan_RmlSaveLoadOpenSave(int mission_flag)
{
    if (!g_save_load.initialized || g_save_load.document == nullptr)
    {
        return;
    }

    if (g_save_load.open)
    {
        MikuPan_RmlSaveLoadCloseInternal();
    }

    mcInit(MC_MODE_GAMESAVE,
           static_cast<u_int*>(MikuPan_GetHostPointer(MC_WORK_ADDRESS)),
           static_cast<u_char>(mission_flag));
    g_save_load.mc_active = true;
    MikuPan_RmlUiRequestItemIcons();
    g_save_load.detail_icons_ready = MikuPan_RmlUiItemIconsReady() != 0;
    g_save_load.save_mode = true;
    g_save_load.open = true;
    g_save_load.selected = 0;
    g_save_load.confirm_entry = -1;
    g_save_load.dialog_mode = MikuPanSaveLoadDialogMode::None;
    g_save_load.confirm_selection = 1;
    g_save_load.exit_selected = false;
    g_save_load.queued_result = MIKUPAN_RML_SAVE_LOAD_RESULT_NONE;
    MikuPan_RmlSaveLoadApplyHeaderTexture();
    MikuPan_RmlSaveLoadScan();
    MikuPan_RmlSaveLoadBuildList();
    MikuPan_RmlSaveLoadSetStatus(
        "Create a new save or select an existing save to replace.", false);
    if (g_save_load.title != nullptr)
    {
        g_save_load.title->SetInnerRML("Save File");
    }
    if (g_save_load.exit_label != nullptr)
    {
        g_save_load.exit_label->SetInnerRML("Back: Y / Esc");
    }
    MikuPan_RmlSaveLoadSetHidden(g_save_load.root, false);
    g_save_load.document->Show(Rml::ModalFlag::None, Rml::FocusFlag::None);
    g_save_load.document->PullToFront();
    MikuPan_RmlSaveLoadSyncSelection(true);
}

void MikuPan_RmlSaveLoadOpenLoad(void)
{
    if (!g_save_load.initialized || g_save_load.document == nullptr)
    {
        return;
    }

    if (g_save_load.open)
    {
        MikuPan_RmlSaveLoadCloseInternal();
    }

    mcInit(MC_MODE_GAMELOAD,
           static_cast<u_int*>(MikuPan_GetHostPointer(MC_WORK_ADDRESS)),
           0);
    g_save_load.mc_active = true;
    MikuPan_RmlUiRequestItemIcons();
    g_save_load.detail_icons_ready = MikuPan_RmlUiItemIconsReady() != 0;
    g_save_load.save_mode = false;
    g_save_load.open = true;
    g_save_load.selected = 0;
    g_save_load.confirm_entry = -1;
    g_save_load.dialog_mode = MikuPanSaveLoadDialogMode::None;
    g_save_load.confirm_selection = 1;
    g_save_load.exit_selected = false;
    g_save_load.queued_result = MIKUPAN_RML_SAVE_LOAD_RESULT_NONE;
    MikuPan_RmlSaveLoadApplyHeaderTexture();
    MikuPan_RmlSaveLoadScan();
    MikuPan_RmlSaveLoadBuildList();
    MikuPan_RmlSaveLoadSetStatus(
        g_save_load.entries.empty()
            ? "No save files were found."
            : "Select a save to continue.",
        false);
    if (g_save_load.title != nullptr)
    {
        g_save_load.title->SetInnerRML("Load File");
    }
    if (g_save_load.exit_label != nullptr)
    {
        g_save_load.exit_label->SetInnerRML("Back: Y / Esc");
    }
    MikuPan_RmlSaveLoadSetHidden(g_save_load.root, false);
    g_save_load.document->Show(Rml::ModalFlag::None, Rml::FocusFlag::None);
    g_save_load.document->PullToFront();
    MikuPan_RmlSaveLoadSyncSelection(true);
}

void MikuPan_RmlSaveLoadClose(void)
{
    MikuPan_RmlSaveLoadCloseInternal();
}

int MikuPan_RmlSaveLoadIsOpen(void)
{
    return g_save_load.open ? 1 : 0;
}

int MikuPan_RmlSaveLoadHandleVerticalInput(int direction)
{
    if (!g_save_load.open)
    {
        return 0;
    }

    if (g_save_load.dialog_mode != MikuPanSaveLoadDialogMode::None)
    {
        if (g_save_load.dialog_mode != MikuPanSaveLoadDialogMode::SaveComplete)
        {
            g_save_load.confirm_selection = direction < 0 ? 0 : 1;
            MikuPan_RmlSaveLoadSyncConfirm();
        }
        return 1;
    }

    const int total = MikuPan_RmlSaveLoadTotalItems();
    if (total <= 0)
    {
        g_save_load.exit_selected = true;
        MikuPan_RmlSaveLoadSyncSelection(false);
        return 1;
    }
    if (g_save_load.exit_selected)
    {
        if (direction < 0)
        {
            g_save_load.exit_selected = false;
            g_save_load.selected = total - 1;
            MikuPan_RmlSaveLoadSyncSelection(true);
        }
        return 1;
    }

    if (direction > 0 && g_save_load.selected >= total - 1)
    {
        g_save_load.exit_selected = true;
        MikuPan_RmlSaveLoadSyncSelection(false);
        return 1;
    }

    const int next = std::clamp(g_save_load.selected + (direction < 0 ? -1 : 1),
                                0,
                                total - 1);
    if (next != g_save_load.selected)
    {
        g_save_load.selected = next;
        MikuPan_RmlSaveLoadSyncSelection(true);
    }
    return 1;
}

int MikuPan_RmlSaveLoadHandleHorizontalInput(int direction)
{
    if (!g_save_load.open)
    {
        return 0;
    }

    if (g_save_load.dialog_mode != MikuPanSaveLoadDialogMode::None)
    {
        if (g_save_load.dialog_mode != MikuPanSaveLoadDialogMode::SaveComplete)
        {
            g_save_load.confirm_selection = direction < 0 ? 0 : 1;
            MikuPan_RmlSaveLoadSyncConfirm();
        }
        return 1;
    }

    if (direction > 0)
    {
        g_save_load.exit_selected = true;
        MikuPan_RmlSaveLoadSyncSelection(false);
    }
    else if (direction < 0 && g_save_load.exit_selected)
    {
        g_save_load.exit_selected = false;
        MikuPan_RmlSaveLoadSyncSelection(true);
    }
    return 1;
}

int MikuPan_RmlSaveLoadActivate(void)
{
    if (!g_save_load.open)
    {
        return 0;
    }

    if (g_save_load.dialog_mode != MikuPanSaveLoadDialogMode::None)
    {
        const MikuPanSaveLoadDialogMode dialog_mode = g_save_load.dialog_mode;
        const int entry_index = g_save_load.confirm_entry;
        const bool confirmed = g_save_load.confirm_selection == 0;

        if (dialog_mode == MikuPanSaveLoadDialogMode::SaveComplete)
        {
            g_save_load.queued_result = MIKUPAN_RML_SAVE_LOAD_RESULT_SAVED;
            g_save_load.dialog_mode = MikuPanSaveLoadDialogMode::None;
            MikuPan_RmlSaveLoadSyncConfirm();
            MikuPan_RmlSaveLoadCloseInternal();
            return 1;
        }

        g_save_load.dialog_mode = MikuPanSaveLoadDialogMode::None;
        g_save_load.confirm_entry = -1;
        MikuPan_RmlSaveLoadSyncConfirm();

        if (!confirmed)
        {
            return 1;
        }

        if (dialog_mode == MikuPanSaveLoadDialogMode::NewSave)
        {
            MikuPan_RmlSaveLoadWrite(MikuPan_RmlSaveLoadNewPath());
            return 1;
        }

        if (entry_index < 0
            || entry_index >= static_cast<int>(g_save_load.entries.size()))
        {
            return 1;
        }

        if (g_save_load.save_mode)
        {
            MikuPan_RmlSaveLoadWrite(g_save_load.entries[entry_index].path);
        }
        else
        {
            MikuPan_RmlSaveLoadRead(g_save_load.entries[entry_index]);
        }
        return 1;
    }

    if (g_save_load.exit_selected)
    {
        return MikuPan_RmlSaveLoadHandleCancel();
    }

    if (g_save_load.save_mode && g_save_load.selected == 0)
    {
        g_save_load.confirm_entry = -1;
        g_save_load.confirm_selection = 1;
        g_save_load.dialog_mode = MikuPanSaveLoadDialogMode::NewSave;
        if (g_save_load.confirm_message != nullptr)
        {
            g_save_load.confirm_message->SetInnerRML(
                "Create a new save file?");
        }
        MikuPan_RmlSaveLoadSyncConfirm();
        return 1;
    }

    const int entry_index = MikuPan_RmlSaveLoadSelectedEntry();
    if (entry_index < 0)
    {
        return 1;
    }

    g_save_load.confirm_entry = entry_index;
    g_save_load.confirm_selection = 1;
    g_save_load.dialog_mode = MikuPanSaveLoadDialogMode::Entry;
    if (g_save_load.confirm_message != nullptr)
    {
        if (g_save_load.save_mode)
        {
            g_save_load.confirm_message->SetInnerRML(
                "Replace this save?");
        }
        else
        {
            g_save_load.confirm_message->SetInnerRML(
                "Load this save?");
        }
    }
    MikuPan_RmlSaveLoadSyncConfirm();
    return 1;
}

int MikuPan_RmlSaveLoadHandleCancel(void)
{
    if (!g_save_load.open)
    {
        return 0;
    }

    if (g_save_load.dialog_mode != MikuPanSaveLoadDialogMode::None)
    {
        if (g_save_load.dialog_mode == MikuPanSaveLoadDialogMode::SaveComplete)
        {
            return 1;
        }
        g_save_load.dialog_mode = MikuPanSaveLoadDialogMode::None;
        g_save_load.confirm_entry = -1;
        MikuPan_RmlSaveLoadSyncConfirm();
        return 1;
    }

    g_save_load.queued_result = MIKUPAN_RML_SAVE_LOAD_RESULT_CANCELLED;
    MikuPan_RmlSaveLoadCloseInternal();
    return 1;
}

int MikuPan_RmlSaveLoadConsumeResult(void)
{
    const int result = g_save_load.queued_result;
    g_save_load.queued_result = MIKUPAN_RML_SAVE_LOAD_RESULT_NONE;
    return result;
}

void MikuPan_RmlSaveLoadCapturePreview(void)
{
    g_save_load.preview_pixels.resize(kPreviewWidth * kPreviewHeight * 4);
    if (!MikuPan_ReadResolvedFramebufferRGBA8TopLeft(
            kPreviewWidth,
            kPreviewHeight,
            g_save_load.preview_pixels.data()))
    {
        g_save_load.preview_pixels.clear();
        return;
    }

    for (size_t alpha = 3; alpha < g_save_load.preview_pixels.size();
         alpha += 4)
    {
        g_save_load.preview_pixels[alpha] = 255;
    }

    if (!MikuPan_RmlSaveLoadPreviewUsable(g_save_load.preview_pixels))
    {
        g_save_load.preview_pixels.clear();
    }
}

}
