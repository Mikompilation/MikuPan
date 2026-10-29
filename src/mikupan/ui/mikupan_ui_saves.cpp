#include "mikupan_ui_saves.h"
#include "mikupan/save/mikupan_save_paths.h"
#include "mikupan/save/mikupan_save_transfer.h"
#include "mikupan_ui_save_preview.h"
#include "mc/mc_main.h"
#include "mikupan_rml_save_load.h"
#include <SDL3/SDL.h>
#include <algorithm>
#include <array>
#include <atomic>
#include <cstdio>
#include <mutex>
#include <set>

namespace MikuPanUiSaves
{
namespace
{
namespace fs = std::filesystem;
using namespace MikuPanSave;
bool show = false, exporting = false, initialized = false, pending = false, path_dirty = false;
enum class Step
{
    Operation,
    Location,
    Slots,
    Format
};
Step step = Step::Operation;
StorageKind ps2_kind = StorageKind::Card;
char external[4096]{}, output[4096]{};
Storage source, target;
bool source_ok = false, target_ok = false;
bool output_ecc = true;
SaveArchiveFormat output_archive = SaveArchiveFormat::None;
const char *OutputSuffix()
{
    if (output_archive == SaveArchiveFormat::Psu)
        return ".psu";
    if (output_archive == SaveArchiveFormat::Max)
        return ".max";
    if (output_archive == SaveArchiveFormat::Xps)
        return ".xps";
    if (output_archive == SaveArchiveFormat::Cbs)
        return ".cbs";
    return output_ecc ? ".ps2" : ".bin";
}
std::string source_error, target_error, message;
std::array<bool, 3> selected{};
std::array<int, 3> destination{-1, -1, -1};
std::atomic<bool> picking{false};
std::mutex pick_mutex;
std::string picked, pick_error;
bool pick_ready = false;
enum class Dialog
{
    Card,
    Folder,
    Output,
    OutputFolder
};
Dialog picked_kind = Dialog::Card;

void SetText(char *to, size_t capacity, const std::string &text)
{
    std::snprintf(to, capacity, "%s", text.c_str());
}
fs::path ExternalFolder()
{
    fs::path p = fs::u8path(external);
    std::error_code ec;
    if (p.filename() == FolderName || fs::is_regular_file(p / FolderName, ec))
        return p;
    return p / FolderName;
}
void ClearSelection()
{
    selected.fill(false);
    destination.fill(-1);
}
void SuggestOutput()
{
    fs::path p = external[0] ? fs::u8path(external) : fs::path("MikuPan.ps2");
    auto base = p.parent_path() / p.stem();
    base += "-converted";
    for (int i = 0; i < 1000; ++i)
    {
        auto name = base;
        if (i)
            name += "-" + std::to_string(i + 1);
        name += OutputSuffix();
        std::error_code ec;
        if (!fs::exists(name, ec) && !ec)
        {
            SetText(output, sizeof(output), PathUtf8(name));
            return;
        }
    }
    output[0] = 0;
}
void Refresh()
{
    source = {};
    target = {};
    source_error.clear();
    target_error.clear();
    const bool valid_external = external[0] || ps2_kind == StorageKind::BlankCard;
    const auto external_path =
        ps2_kind == StorageKind::Folder ? ExternalFolder() : fs::u8path(external);
    if (exporting)
    {
        source_ok = ReadStorage(PcFolder(), StorageKind::Folder, true, source, source_error);
        target_ok =
            valid_external && ReadStorage(external_path, ps2_kind, false, target, target_error);
        if (!valid_external)
            target_error = "Open a card/folder or choose New blank card.";
    }
    else
    {
        source_ok = valid_external && ps2_kind != StorageKind::BlankCard &&
                    ReadStorage(external_path, ps2_kind, false, source, source_error);
        if (!valid_external || ps2_kind == StorageKind::BlankCard)
            source_error = "Open a PS2 card or extracted save folder.";
        target_ok = ReadStorage(PcFolder(), StorageKind::Folder, true, target, target_error);
    }
    if (source_ok && !source.has_save)
        source_error = "No EU MikuPan saves found at this source.";
    initialized = true;
}
void AssignEmptySlots()
{
    std::set<int> used;
    for (int i = 0; i < 3; ++i)
        if (selected[i] && destination[i] >= 0)
            used.insert(destination[i]);
    for (int i = 0; i < 3; ++i)
    {
        if (!selected[i])
        {
            destination[i] = -1;
            continue;
        }
        if (destination[i] >= 0)
            continue;
        for (int j = 0; j < 3; ++j)
            if (!target.save.slots[j].occupied && !used.count(j))
            {
                destination[i] = j;
                used.insert(j);
                break;
            }
    }
}
std::vector<SlotMapping> Mappings()
{
    std::vector<SlotMapping> slots;
    for (int i = 0; i < 3; ++i)
        if (selected[i])
            slots.push_back({i, destination[i]});
    return slots;
}
bool WritesCard() { return exporting && ps2_kind != StorageKind::Folder; }
fs::path CardOutputPath()
{
    auto path = fs::u8path(output);
    if (!path.empty() && (!path.has_extension() || path.extension() == "."))
        path.replace_extension(OutputSuffix());
    return path;
}
bool ApplyOutputExtension()
{
    const auto text = PathUtf8(CardOutputPath());
    if (text.size() >= sizeof(output))
    {
        message = "Output path is too long.";
        return false;
    }
    SetText(output, sizeof(output), text);
    return true;
}
std::string Unavailable(bool require_selection = true, bool require_output = true,
                        bool check_busy = true)
{
    if (pending)
        return "Transferring saves...";
    if (path_dirty)
        return "Finish editing the path to open its saves.";
    if (picking.load())
        return "Finish choosing the file or folder.";
    if (check_busy && (mcIsInUse() || MikuPan_RmlSaveLoadIsOpen()))
        return "Close the game's save/load screen; startup card access must also finish.";
    if (!source_ok || !source.has_save)
        return source_error.empty() ? "Open a source containing saves." : source_error;
    if (!target_ok)
        return target_error;
    if (!require_selection)
        return {};
    const auto slots = Mappings();
    if (slots.empty())
        return "Select one or more source saves.";
    std::set<int> used;
    for (const auto &s : slots)
    {
        if (s.destination < 0)
            return "Assign a destination slot to each selected save.";
        if (!used.insert(s.destination).second)
            return "Each destination slot can be used only once.";
    }
    if (require_output && WritesCard())
    {
        if (!output[0])
            return "Choose a filename for the new card.";
        if (PathUtf8(CardOutputPath()).size() >= sizeof(output))
            return "Output path is too long.";
        std::error_code ec;
        if (fs::exists(CardOutputPath(), ec) || ec)
            return "Choose a new filename; existing cards are never overwritten.";
    }
    return {};
}
void SDLCALL Picked(void *userdata, const char *const *files, int)
{
    std::lock_guard<std::mutex> lock(pick_mutex);
    picked = files && files[0] ? files[0] : "";
    pick_error = files ? "" : SDL_GetError();
    picked_kind = static_cast<Dialog>(reinterpret_cast<intptr_t>(userdata));
    pick_ready = true;
    picking.store(false, std::memory_order_release);
}
void Browse(SDL_Window *window, Dialog dialog)
{
    picking.store(true, std::memory_order_release);
    struct Request
    {
        SDL_Window *window;
        Dialog dialog;
        const char *path;
    } request{window, dialog,
              dialog == Dialog::OutputFolder ? nullptr
              : dialog == Dialog::Output     ? (output[0] ? output : nullptr)
                                             : (external[0] ? external : nullptr)};
    if (!SDL_RunOnMainThread(
            [](void *data)
            {
                const auto &r = *static_cast<Request *>(data);
                static const SDL_DialogFileFilter filter{"PS2 cards or save archives",
                                                         "ps2;mcd;bin;vmc;psu;max;xps;sps;cbs"};
                static const SDL_DialogFileFilter outputs[] = {
                    {"PCSX2 / ECC memory card", "ps2"}, {"OPL / raw memory card", "bin"},
                    {"PSU / EMS - uLaunchELF", "psu"},  {"Action Replay MAX", "max"},
                    {"X-Port / SharkPort", "xps;sps"},  {"CodeBreaker", "cbs"}};
                const auto *save_filter = &outputs[output_archive == SaveArchiveFormat::Cbs   ? 5
                                                   : output_archive == SaveArchiveFormat::Xps ? 4
                                                   : output_archive == SaveArchiveFormat::Psu ? 2
                                                   : output_archive == SaveArchiveFormat::Max ? 3
                                                   : output_ecc                               ? 0
                                                                                              : 1];
                void *context = reinterpret_cast<void *>(static_cast<intptr_t>(r.dialog));
                if (r.dialog == Dialog::Folder || r.dialog == Dialog::OutputFolder)
                    SDL_ShowOpenFolderDialog(Picked, context, r.window, r.path, false);
                else if (r.dialog == Dialog::Output)
                    SDL_ShowSaveFileDialog(Picked, context, r.window, save_filter, 1, r.path);
                else
                    SDL_ShowOpenFileDialog(Picked, context, r.window, &filter, 1, r.path, false);
            },
            &request, true))
    {
        picking.store(false, std::memory_order_release);
        message = std::string("Cannot open file picker: ") + SDL_GetError();
    }
}
void AdoptPicker()
{
    std::lock_guard<std::mutex> lock(pick_mutex);
    if (!pick_ready)
        return;
    if (!picked.empty())
    {
        if (picked.size() >= sizeof(external))
            message = "Selected path is too long.";
        else if (picked_kind == Dialog::Output)
        {
            SetText(output, sizeof(output), picked);
            if (ApplyOutputExtension())
                pending = true;
        }
        else if (picked_kind == Dialog::OutputFolder)
        {
            const auto path = fs::u8path(picked) / FolderName;
            Storage folder;
            std::string error;
            if (!ReadStorage(path, StorageKind::Folder, false, folder, error))
                message = error;
            else if (folder.exists)
                message = "A save folder already exists there; choose another output folder.";
            else
            {
                target = std::move(folder);
                target_ok = true;
                ps2_kind = StorageKind::Folder;
                pending = true;
            }
        }
        else
        {
            SetText(external, sizeof(external), picked);
            ps2_kind = picked_kind == Dialog::Folder ? StorageKind::Folder : StorageKind::Card;
            ClearSelection();
            initialized = false;
            path_dirty = false;
            if (ps2_kind == StorageKind::Card)
            {
                std::error_code ec;
                output_ecc = fs::file_size(fs::u8path(external), ec) != 8388608;
                SuggestOutput();
            }
            Refresh();
            message.clear();
            if (Unavailable(false, false, false).empty())
                step = Step::Slots;
        }
    }
    if (!pick_error.empty())
        message = pick_error;
    pick_ready = false;
}
void Commit()
{
    pending = false;
    const auto reason = Unavailable();
    if (!reason.empty())
    {
        message = reason;
        return;
    }
    if (WritesCard() && !ApplyOutputExtension())
        return;
    const auto slots = Mappings();
    std::string error;
    if (TransferBatch(source, target, slots, exporting, fs::u8path(output), error, output_ecc,
                      output_archive))
    {
        message =
            (exporting ? "Exported " : "Imported ") + std::to_string(slots.size()) + " save(s).";
        if (exporting && ps2_kind != StorageKind::Folder)
        {
            message += " New card: " + std::string(output);
            SetText(external, sizeof(external), output);
            ps2_kind = StorageKind::Card;
            SuggestOutput();
        }
        if (!exporting)
            message += " Current session unchanged; load the imported save normally.";
        if (!error.empty())
            message += " " + error;
        ClearSelection();
        Refresh();
        step = Step::Operation;
        show = false;
    }
    else
        message = "Transfer failed: " + error;
}
void DestinationChoice(int i)
{
    char label[64];
    std::snprintf(label, sizeof(label), "Destination##%d", i);
    std::string preview =
        destination[i] < 0 ? "Choose slot" : ("Slot " + std::to_string(destination[i] + 1));
    ImGui::SetNextItemWidth(145);
    if (ImGui::BeginCombo(label, preview.c_str()))
    {
        for (int slot = 0; slot < 3; ++slot)
        {
            const bool occupied = target.save.slots[slot].occupied;
            const std::string text =
                "Slot " + std::to_string(slot + 1) + (occupied ? " (replace)" : " (empty)");
            if (ImGui::Selectable(text.c_str(), destination[i] == slot))
                destination[i] = slot;
            if (occupied && ImGui::IsItemHovered())
                ImGui::SetTooltip("%s", target.save.slots[slot].description.c_str());
        }
        ImGui::EndCombo();
    }
}
void Rows(bool is_source)
{
    const auto &storage = is_source ? source : target;
    for (int i = 0; i < 3; ++i)
    {
        const auto &slot = storage.save.slots[i];
        ImGui::PushID((is_source ? 0 : 10) + i);
        int incoming = -1;
        if (!is_source)
            for (int j = 0; j < 3; ++j)
                if (selected[j] && destination[j] == i)
                    incoming = j;
        const bool highlighted = is_source ? selected[i] : incoming >= 0;
        if (highlighted)
            ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.10f, 0.18f, 0.24f, 1));
        ImGui::BeginChild("slot", ImVec2(0, 104), ImGuiChildFlags_Borders,
                          ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
        const float row_top = ImGui::GetCursorPosY();
        ImGui::BeginGroup();
        if (is_source)
        {
            ImGui::BeginDisabled(!slot.occupied || !source_ok);
            if (ImGui::Checkbox("##select", &selected[i]))
            {
                if (!selected[i])
                    destination[i] = -1;
                AssignEmptySlots();
            }
            ImGui::EndDisabled();
            ImGui::SameLine();
        }
        else
        {
            ImGui::Dummy(ImVec2(ImGui::GetFrameHeight(), ImGui::GetFrameHeight()));
            ImGui::SameLine();
        }
        ImGui::Text("%d", i + 1);
        ImGui::EndGroup();
        ImGui::SameLine();
        const auto &preview = incoming >= 0 ? source.save.slots[incoming] : slot;
        MikuPanUiSavePreview::Draw(preview, ImVec2(116, 58));
        ImGui::SameLine();
        ImGui::BeginGroup();
        if (preview.occupied)
        {
            ImGui::TextUnformatted(preview.player.c_str());
            ImGui::Text("Chapter %u / Room %d", preview.chapter, preview.area);
            ImGui::TextDisabled("%02u:%02u:%02u", preview.hours, preview.minutes, preview.seconds);
        }
        else
            ImGui::TextDisabled("Empty slot");
        ImGui::EndGroup();
        ImGui::SetCursorPosY(row_top + 66);
        if (is_source)
        {
            ImGui::BeginDisabled(!selected[i]);
            DestinationChoice(i);
            ImGui::EndDisabled();
        }
        else if (incoming >= 0)
            ImGui::TextColored(slot.occupied ? ImVec4(1, .7f, .3f, 1) : ImVec4(.5f, .9f, .6f, 1),
                               "%s <- source %d", slot.occupied ? "Replace" : "Add", incoming + 1);
        else
            ImGui::TextDisabled("%s", slot.occupied ? "Keep existing save" : "Empty slot");
        if (is_source && slot.occupied && source_ok && ImGui::IsWindowHovered() &&
            ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !ImGui::IsAnyItemHovered() &&
            !ImGui::IsAnyItemActive())
        {
            selected[i] = !selected[i];
            if (!selected[i])
                destination[i] = -1;
            AssignEmptySlots();
        }
        ImGui::EndChild();
        if (highlighted)
            ImGui::PopStyleColor();
        ImGui::PopID();
    }
}
} // namespace
void Init(SDL_GPUDevice *device) { MikuPanUiSavePreview::Init(device); }
void Shutdown() { MikuPanUiSavePreview::Shutdown(); }
bool IsOpen() { return show || picking.load(); }
void MenuItem()
{
    if (ImGui::MenuItem("PS2 save transfer..."))
    {
        show = true;
        step = Step::Operation;
        initialized = false;
        ClearSelection();
    }
}
namespace
{
bool ChoiceCard(const char *label, float width, float height = 110.f)
{
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 8.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.0f);
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0, 0, 0, 0));
    const bool pressed = ImGui::Button(label, ImVec2(width, height));
    ImGui::PopStyleColor();
    ImGui::PopStyleVar(2);
    const ImVec2 origin = ImGui::GetItemRectMin();
    const std::string text(label);
    const size_t split = text.find("\n\n");
    const std::string title = text.substr(0, split);
    const std::string description = split == std::string::npos ? "" : text.substr(split + 2);
    auto *draw = ImGui::GetWindowDrawList();
    const ImU32 colour = ImGui::GetColorU32(ImGuiCol_Text);
    for (int line = 0; line < 2; ++line)
    {
        const auto &value = line == 0 ? title : description;
        const float x = origin.x + (width - ImGui::CalcTextSize(value.c_str()).x) * .5f;
        draw->AddText(ImVec2(x, origin.y + (line == 0 ? height * .26f : height * .60f)), colour,
                      value.c_str());
    }
    return pressed;
}
void ChooseDirection(bool to_ps2)
{
    exporting = to_ps2;
    output_archive = SaveArchiveFormat::None;
    ps2_kind = exporting ? StorageKind::BlankCard : StorageKind::Card;
    external[0] = output[0] = 0;
    path_dirty = false;
    initialized = false;
    ClearSelection();
    message.clear();
    if (exporting)
        SuggestOutput();
    step = exporting ? Step::Slots : Step::Location;
}
void ChooseStorage(SDL_Window *window, StorageKind kind)
{
    ps2_kind = kind;
    external[0] = 0;
    path_dirty = false;
    initialized = false;
    ClearSelection();
    message.clear();
    Browse(window, kind == StorageKind::Folder ? Dialog::Folder : Dialog::Card);
}
} // namespace
void Draw(SDL_Window *window)
{
    AdoptPicker();
    if (!show)
        return;
    if (pending)
    {
        Commit();
        if (!show)
            return;
    }
    if (!initialized && step != Step::Operation)
        Refresh();
    const auto *viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowSize(ImVec2(std::min(1080.f, viewport->WorkSize.x * .96f),
                                    std::min(800.f, viewport->WorkSize.y * .94f)),
                             ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("PS2 save transfer", &show))
    {
        ImGui::End();
        return;
    }
    const char *titles[] = {"Choose direction", "Choose PS2 storage", "Select saves and slots",
                            "Choose output format"};
    const int page = step == Step::Operation ? 1 : exporting ? int(step) : int(step) + 1;
    ImGui::Text("Step %d of %d: %s", page, 3, titles[int(step)]);
    ImGui::Separator();
    ImGui::BeginDisabled(picking.load());
    const float footer_height =
        ImGui::GetFrameHeightWithSpacing() + ImGui::GetTextLineHeightWithSpacing() * 4;
    ImGui::BeginChild("wizard-page", ImVec2(0, -footer_height), 0, ImGuiWindowFlags_NoBackground);
    const bool centered = step != Step::Slots;
    if (centered)
    {
        const ImVec2 available = ImGui::GetContentRegionAvail();
        const float width = std::min(860.f, available.x);
        const float height = step == Step::Format
                                 ? available.y
                                 : std::min(step == Step::Operation ? 300.f : 430.f, available.y);
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (available.x - width) * .5f);
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + (available.y - height) * .5f);
        ImGui::BeginChild("choice-panel", ImVec2(width, height), 0, ImGuiWindowFlags_NoBackground);
    }
    if (step == Step::Operation)
    {
        ImGui::TextUnformatted("Where would you like to send your saves?");
        ImGui::TextDisabled("Project Zero II - EU / SLES_523.84");
        ImGui::Spacing();
        const float available = ImGui::GetContentRegionAvail().x;
        const bool side_by_side = available >= 650.f;
        const float width =
            side_by_side ? (available - ImGui::GetStyle().ItemSpacing.x) * .5f : available;
        if (ChoiceCard("Import to PC\n\nBring your PS2 progress to this PC", width))
            ChooseDirection(false);
        if (side_by_side)
            ImGui::SameLine();
        if (ChoiceCard("Export to PS2\n\nTake your PC progress to a memory card", width))
            ChooseDirection(true);
    }
    else if (step == Step::Location)
    {
        ImGui::TextUnformatted("Choose where your PS2 saves are stored");
        ImGui::Spacing();
        const float width = ImGui::GetContentRegionAvail().x;
        if (ChoiceCard(
                "Open a card or save archive\n\nPCSX2 / OPL card or single-game save archive",
                width))
            ChooseStorage(window, StorageKind::Card);
        if (ChoiceCard("Open a save folder\n\nUse extracted PS2 save files", width))
            ChooseStorage(window, StorageKind::Folder);
    }
    else if (step == Step::Slots)
    {
        ImGui::TextUnformatted("Select source saves, then choose a destination slot for each.");
        const int count = int(std::count(selected.begin(), selected.end(), true));
        const int available =
            int(std::count_if(source.save.slots.begin(), source.save.slots.end(),
                              [](const SlotInfo &slot) { return slot.occupied; }));
        ImGui::Text("%d of %d saves selected", count, available);
        ImGui::SameLine();
        if (ImGui::SmallButton("Select all"))
        {
            for (int i = 0; i < 3; ++i)
                selected[i] = source_ok && source.save.slots[i].occupied;
            AssignEmptySlots();
        }
        ImGui::SameLine();
        if (ImGui::SmallButton("Clear selection"))
            ClearSelection();
        if (ImGui::BeginTable("saves", 2,
                              ImGuiTableFlags_Resizable | ImGuiTableFlags_BordersInnerV))
        {
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(exporting ? "Source: PC saves" : "Source: PS2 saves");
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(exporting ? "Destination: PS2 saves" : "Destination: PC saves");
            ImGui::TableNextRow();
            const float height = std::max(120.f, ImGui::GetContentRegionAvail().y);
            ImGui::TableSetColumnIndex(0);
            ImGui::BeginChild("source-list", ImVec2(0, height));
            Rows(true);
            ImGui::EndChild();
            ImGui::TableSetColumnIndex(1);
            ImGui::BeginChild("destination-list", ImVec2(0, height));
            Rows(false);
            ImGui::EndChild();
            ImGui::EndTable();
        }
    }
    else if (step == Step::Format)
    {
        ImGui::TextUnformatted("Choose the output format");
        ImGui::TextWrapped("Choose a format, then confirm the destination in the file picker to "
                           "export immediately.");
        ImGui::Spacing();
        const float width = ImGui::GetContentRegionAvail().x;
        const float height = std::clamp((ImGui::GetContentRegionAvail().y - 40.f) / 7.f -
                                            ImGui::GetStyle().ItemSpacing.y,
                                        60.f, 110.f);
        int format = -1;
        if (ChoiceCard("PCSX2 / ECC\n\nStandard .ps2 memory card", width, height))
            format = 1;
        if (ChoiceCard("OPL / raw\n\nNon-ECC .bin memory card", width, height))
            format = 0;
        if (ChoiceCard("Extracted save folder\n\nCreate save files inside the chosen folder", width,
                       height))
            format = 2;
        if (ChoiceCard("PSU / EMS\n\nuLaunchELF and PS2 save-transfer tools", width, height))
            format = 3;
        if (ChoiceCard("Action Replay MAX\n\nSingle-game .max save archive", width, height))
            format = 4;
        if (ChoiceCard("X-Port / SharkPort\n\nSingle-game .xps / .sps save archive", width, height))
            format = 5;
        if (ChoiceCard("CodeBreaker\n\nSingle-game .cbs save archive", width, height))
            format = 6;
        ImGui::TextWrapped(
            "Save archives contain this game's selected saves, not a whole memory card.");
        if (format >= 0)
        {
            message.clear();
            ps2_kind = StorageKind::BlankCard;
            output_archive = SaveArchiveFormat::None;
            target_ok = ReadStorage({}, StorageKind::BlankCard, false, target, target_error);
            if (format == 2)
                Browse(window, Dialog::OutputFolder);
            else
            {
                output_ecc = format == 1;
                output_archive = format == 3   ? SaveArchiveFormat::Psu
                                 : format == 4 ? SaveArchiveFormat::Max
                                 : format == 5 ? SaveArchiveFormat::Xps
                                 : format == 6 ? SaveArchiveFormat::Cbs
                                               : SaveArchiveFormat::None;
                if (output[0])
                {
                    auto path = fs::u8path(output);
                    path.replace_extension(OutputSuffix());
                    const auto text = PathUtf8(path);
                    if (text.size() >= sizeof(output))
                        message = "Output path is too long.";
                    else
                        SetText(output, sizeof(output), text);
                }
                else
                    SuggestOutput();
                if (message.empty())
                    Browse(window, Dialog::Output);
            }
        }
    }
    if (centered)
        ImGui::EndChild();
    ImGui::EndChild();
    ImGui::Separator();
    const bool final_step = step == Step::Slots && !exporting;
    const auto slots = Mappings();
    std::string reason;
    if (step == Step::Location)
        reason = Unavailable(false, false, false);
    else if (step == Step::Slots || step == Step::Format)
        reason = Unavailable(true, false, final_step || step == Step::Format);
    ImGui::BeginChild("transfer-status", ImVec2(0, ImGui::GetTextLineHeightWithSpacing() * 3));
    if (!reason.empty())
        ImGui::TextWrapped("Cannot %s: %s",
                           final_step ? (exporting ? "export" : "import") : "continue",
                           reason.c_str());
    else if (!final_step && (mcIsInUse() || MikuPan_RmlSaveLoadIsOpen()))
        ImGui::TextWrapped("You can continue setup. Transfer must wait until the game's save/load "
                           "screen and card access finish.");
    if (!message.empty())
        ImGui::TextWrapped("%s", message.c_str());
    ImGui::EndChild();
    ImGui::BeginDisabled(step == Step::Operation);
    if (ImGui::Button("Back", ImVec2(120, 0)))
    {
        step = exporting && step == Step::Slots ? Step::Operation : Step(int(step) - 1);
        message.clear();
    }
    ImGui::EndDisabled();
    if (step == Step::Slots)
    {
        ImGui::SameLine();
        ImGui::SetCursorPosX(
            std::max(ImGui::GetCursorPosX(), ImGui::GetWindowContentRegionMax().x - 180.f));
        const std::string action = final_step ? (exporting ? "Export " : "Import ") +
                                                    std::to_string(slots.size()) + " save(s)"
                                              : "Next";
        ImGui::BeginDisabled(!reason.empty());
        if (ImGui::Button(action.c_str(), ImVec2(180, 0)))
        {
            message.clear();
            if (final_step)
            {
                if (!WritesCard() || ApplyOutputExtension())
                    pending = true;
            }
            else
                step = Step::Format;
        }
        ImGui::EndDisabled();
    }
    ImGui::EndDisabled();
    ImGui::End();
}
} // namespace MikuPanUiSaves
