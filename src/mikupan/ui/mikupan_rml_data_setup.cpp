#include "mikupan/ui/mikupan_rml_data_setup.h"

#include "mikupan/io/mikupan_file.h"
#include "mikupan/ui/mikupan_ui.h"

#include "RmlUi/Core.h"
#include "RmlUi/Core/ElementDocument.h"
#include "RmlUi/Core/EventListener.h"
#include "RmlUi/Core/StringUtilities.h"

#include <algorithm>
#include <array>
#include <memory>
#include <string>
#include <vector>

namespace
{

constexpr int kExitRow = MIKUPAN_DATA_SOURCE_COUNT;
constexpr int kRowCount = kExitRow + 1;

/// What fits in a row's value area at its font size.
constexpr size_t kMaxShownNameLength = 34;

constexpr std::array<const char*, kRowCount> kRowHelp = {
    "Use a disc image of the game. It is read as it is, so nothing is copied, "
    "and the cutscenes come with it.",
    "Use IMG_HD.BIN from the disc. If IMG_BD.BIN sits beside it, that one is "
    "found too.",
    "Use IMG_BD.BIN from the disc. Cutscenes play if the MOVIE folders sit "
    "beside it.",
    "Leave MikuPan.",
};

class RowListener final : public Rml::EventListener
{
public:
    RowListener(int in_row, bool in_activate)
        : row(in_row), activate(in_activate)
    {
    }

    void ProcessEvent(Rml::Event& event) override;

private:
    int row = 0;
    bool activate = false;
};

struct DataSetupState
{
    Rml::Context* context = nullptr;
    Rml::ElementDocument* document = nullptr;
    Rml::Element* root = nullptr;
    Rml::Element* status = nullptr;
    Rml::Element* message = nullptr;
    std::array<Rml::Element*, kRowCount> rows{};
    std::array<Rml::Element*, MIKUPAN_DATA_SOURCE_COUNT> values{};
    std::array<std::string, MIKUPAN_DATA_SOURCE_COUNT> shown_values;
    std::string shown_status;
    std::vector<std::unique_ptr<Rml::EventListener>> listeners;
    int selected = 0;
    int shown_selected = -1;
    bool activate_queued = false;
    bool initialized = false;
    bool visible = false;
    bool shown_last_frame = false;
};

DataSetupState g_data_setup;

void RowListener::ProcessEvent(Rml::Event& event)
{
    (void) event;
    g_data_setup.selected = row;

    /// Handled from the frame, not from inside RmlUi's event dispatch: on
    /// some platforms the file picker blocks until it is closed.
    if (activate)
    {
        g_data_setup.activate_queued = true;
    }
}

Rml::Element* GetElement(const std::string& id)
{
    return g_data_setup.document->GetElementById(id);
}

void AddListener(Rml::Element* element, Rml::EventId event_id, int row,
                 bool activate)
{
    auto listener = std::make_unique<RowListener>(row, activate);
    element->AddEventListener(event_id, listener.get());
    g_data_setup.listeners.push_back(std::move(listener));
}

bool LoadDocument()
{
    char path[1024];
    if (!MikuPan_ResolveBasePath("resources/rml/data_setup.rml", path,
                                 sizeof(path)))
    {
        return false;
    }

    g_data_setup.document = g_data_setup.context->LoadDocument(path);
    if (g_data_setup.document == nullptr)
    {
        return false;
    }

    g_data_setup.root = GetElement("data-setup-root");
    g_data_setup.status = GetElement("data-setup-status");
    g_data_setup.message = GetElement("data-setup-message-text");

    if (g_data_setup.root == nullptr || g_data_setup.status == nullptr
        || g_data_setup.message == nullptr)
    {
        return false;
    }

    for (int row = 0; row < kRowCount; row++)
    {
        const std::string index = std::to_string(row);
        g_data_setup.rows[row] = GetElement("data-setup-row-" + index);
        Rml::Element* hit = GetElement("data-setup-hit-" + index);

        if (g_data_setup.rows[row] == nullptr || hit == nullptr)
        {
            return false;
        }

        AddListener(hit, Rml::EventId::Mouseover, row, false);
        AddListener(hit, Rml::EventId::Click, row, true);

        if (row < MIKUPAN_DATA_SOURCE_COUNT)
        {
            g_data_setup.values[row] = GetElement("data-setup-value-" + index);
            if (g_data_setup.values[row] == nullptr)
            {
                return false;
            }
        }
    }

    g_data_setup.document->Hide();
    return true;
}

/// The file name of a path, shortened from the middle so its end, which is
/// what tells two dumps apart, stays readable.
std::string GetShownName(const std::string& path)
{
    const size_t separator = path.find_last_of("/\\");
    std::string name =
        separator == std::string::npos ? path : path.substr(separator + 1);

    if (name.size() > kMaxShownNameLength)
    {
        const size_t kept = kMaxShownNameLength - 3;
        name = name.substr(0, kept / 2) + "..."
            + name.substr(name.size() - (kept - kept / 2));
    }

    return name;
}

void SyncValues(bool has_problem)
{
    for (int source = 0; source < MIKUPAN_DATA_SOURCE_COUNT; source++)
    {
        const std::string path = MikuPan_GetDataSourcePath(source);
        const std::string shown =
            path.empty() ? "Browse..." : GetShownName(path);

        if (shown != g_data_setup.shown_values[source])
        {
            g_data_setup.values[source]->SetInnerRML(
                Rml::StringUtilities::EncodeRml(shown));
            g_data_setup.shown_values[source] = shown;
        }

        g_data_setup.values[source]->SetClass("ready",
                                              !path.empty() && !has_problem);
    }
}

void SyncStatus(const std::string& problem)
{
    const std::string status =
        problem.empty() ? std::string("This build plays the ")
                + MikuPan_GetRequiredDiscName() + " disc."
                        : problem;

    if (status != g_data_setup.shown_status)
    {
        g_data_setup.status->SetInnerRML(
            Rml::StringUtilities::EncodeRml(status));
        g_data_setup.shown_status = status;
    }

    g_data_setup.status->SetClass("problem", !problem.empty());
}

void SyncSelection()
{
    if (g_data_setup.shown_selected == g_data_setup.selected)
    {
        return;
    }

    for (int row = 0; row < kRowCount; row++)
    {
        g_data_setup.rows[row]->SetClass("selected",
                                         row == g_data_setup.selected);
    }

    g_data_setup.message->SetInnerRML(kRowHelp[g_data_setup.selected]);
    g_data_setup.shown_selected = g_data_setup.selected;
}

} // namespace

bool MikuPan_RmlDataSetupInit(Rml::Context* context)
{
    if (g_data_setup.initialized)
    {
        return true;
    }

    g_data_setup.context = context;
    if (context == nullptr || !LoadDocument())
    {
        g_data_setup = DataSetupState();
        return false;
    }

    g_data_setup.initialized = true;
    return true;
}

void MikuPan_RmlDataSetupStartFrame(void)
{
    if (!g_data_setup.initialized)
    {
        return;
    }

    if (!g_data_setup.shown_last_frame && g_data_setup.visible)
    {
        g_data_setup.visible = false;
        g_data_setup.document->Hide();
    }
    g_data_setup.shown_last_frame = false;
}

void MikuPan_RmlDataSetupPrepareShutdown(void)
{
    g_data_setup.visible = false;
    g_data_setup.shown_last_frame = false;
}

void MikuPan_RmlDataSetupShutdown(void)
{
    g_data_setup = DataSetupState();
}

int MikuPan_RmlDataSetupIsOpen(void)
{
    return g_data_setup.visible ? 1 : 0;
}

void MikuPan_RmlDataSetupHandleVerticalInput(int direction)
{
    g_data_setup.selected =
        std::clamp(g_data_setup.selected + direction, 0, kExitRow);
}

void MikuPan_RmlDataSetupActivate(void)
{
    g_data_setup.activate_queued = true;
}

void MikuPan_RmlDataSetupHandleCancel(void)
{
    g_data_setup.selected = kExitRow;
}

extern "C" {

int MikuPan_RmlDataSetupIsAvailable(void)
{
    return g_data_setup.initialized ? 1 : 0;
}

void MikuPan_RmlDataSetupShow(void)
{
    if (!g_data_setup.initialized)
    {
        return;
    }

    g_data_setup.shown_last_frame = true;

    if (!g_data_setup.visible)
    {
        g_data_setup.root->SetClass("hidden", false);
        g_data_setup.document->Show(Rml::ModalFlag::None, Rml::FocusFlag::None);
        g_data_setup.visible = true;
    }

    if (g_data_setup.activate_queued)
    {
        g_data_setup.activate_queued = false;

        if (g_data_setup.selected == kExitRow)
        {
            MikuPan_RequestQuit();
        }
        else
        {
            MikuPan_BrowseForDataSource(g_data_setup.selected);
        }
    }

    const std::string problem = MikuPan_GetDataProblem();
    SyncValues(!problem.empty());
    SyncStatus(problem);
    SyncSelection();
}

}
