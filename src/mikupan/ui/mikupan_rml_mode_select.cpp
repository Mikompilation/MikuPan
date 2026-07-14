#include "mikupan/ui/mikupan_rml_mode_select.h"

#include "mikupan/io/mikupan_file.h"

#include "RmlUi/Core.h"
#include "RmlUi/Core/ElementDocument.h"
#include "RmlUi/Core/EventListener.h"

#include <algorithm>
#include <array>
#include <cstdio>
#include <memory>
#include <string>
#include <vector>

namespace
{

class ModeSelectListener final : public Rml::EventListener
{
public:
    ModeSelectListener(int in_selection, int in_command)
        : selection(in_selection), command(in_command)
    {
    }

    void ProcessEvent(Rml::Event& event) override;

private:
    int selection = -1;
    int command = MIKUPAN_RML_MODE_SELECT_COMMAND_NONE;
};

struct ModeSelectState
{
    Rml::Context* context = nullptr;
    Rml::ElementDocument* document = nullptr;
    Rml::Element* root = nullptr;
    Rml::Element* main_panel = nullptr;
    Rml::Element* story_panel = nullptr;
    Rml::Element* main_menu = nullptr;
    Rml::Element* story_menu = nullptr;
    Rml::Element* message_box = nullptr;
    Rml::Element* message_text = nullptr;
    std::array<Rml::Element*, 5> main_buttons{};
    std::array<Rml::Element*, 5> story_rows{};
    std::array<Rml::Element*, 5> story_hits{};
    std::array<Rml::Element*, 5> main_messages{};
    std::array<Rml::Element*, 5> story_messages{};
    std::array<std::array<Rml::Element*, 6>, 3> story_value_options{};
    std::array<Rml::Element*, 3> story_left_arrows{};
    std::array<Rml::Element*, 3> story_right_arrows{};
    std::vector<std::unique_ptr<Rml::EventListener>> listeners;
    int queued_selection = -1;
    int queued_command = MIKUPAN_RML_MODE_SELECT_COMMAND_NONE;
    bool initialized = false;
    bool visible = false;
    bool shown_last_frame = false;
};

ModeSelectState g_mode_select;

constexpr std::array<int, 3> story_value_counts = {6, 2, 4};

Rml::Element* GetElement(const char* id)
{
    return g_mode_select.document != nullptr
        ? g_mode_select.document->GetElementById(id)
        : nullptr;
}

void SetHidden(Rml::Element* element, bool hidden)
{
    if (element != nullptr)
    {
        element->SetClass("hidden", hidden);
    }
}

void SetOpacity(Rml::Element* element, float opacity)
{
    if (element == nullptr)
    {
        return;
    }

    char value[32];
    std::snprintf(value, sizeof(value), "%.4f", std::clamp(opacity, 0.0f, 1.0f));
    element->SetProperty("opacity", value);
}

void AddListener(Rml::Element* element,
                 Rml::EventId event_id,
                 int selection,
                 int command)
{
    if (element == nullptr)
    {
        return;
    }

    auto listener = std::make_unique<ModeSelectListener>(selection, command);
    element->AddEventListener(event_id, listener.get());
    g_mode_select.listeners.push_back(std::move(listener));
}

void QueueAction(int selection, int command)
{
    if (selection >= 0)
    {
        g_mode_select.queued_selection = selection;
    }
    if (command != MIKUPAN_RML_MODE_SELECT_COMMAND_NONE)
    {
        g_mode_select.queued_command = command;
    }
}

void ModeSelectListener::ProcessEvent(Rml::Event& event)
{
    (void) event;
    QueueAction(selection, command);
}

void RequestVisible()
{
    g_mode_select.shown_last_frame = true;
    if (g_mode_select.document == nullptr || g_mode_select.root == nullptr)
    {
        return;
    }

    SetHidden(g_mode_select.root, false);
    if (!g_mode_select.visible)
    {
        g_mode_select.document->Show(Rml::ModalFlag::None, Rml::FocusFlag::None);
        g_mode_select.visible = true;
    }
}

void HideDocument()
{
    if (!g_mode_select.visible || g_mode_select.document == nullptr)
    {
        return;
    }

    g_mode_select.visible = false;
    g_mode_select.document->Hide();
}

template <std::size_t N>
void SetActiveElement(const std::array<Rml::Element*, N>& elements, int active_index)
{
    for (int index = 0; index < static_cast<int>(N); index++)
    {
        SetHidden(elements[index], index != active_index);
    }
}

void SetMessage(bool story, int selection)
{
    SetActiveElement(g_mode_select.main_messages, story ? -1 : selection);
    SetActiveElement(g_mode_select.story_messages, story ? selection : -1);
}

void SetStoryValue(int row, int value)
{
    if (row < 0 || row >= static_cast<int>(g_mode_select.story_value_options.size()))
    {
        return;
    }

    const int count = story_value_counts[row];
    const int selected = std::clamp(value, 0, count - 1);
    for (int index = 0; index < count; index++)
    {
        SetHidden(g_mode_select.story_value_options[row][index], index != selected);
    }
}

bool LoadDocument()
{
    char path[1024];
    if (!MikuPan_ResolveBasePath("resources/rml/mode_select.rml",
                                 path,
                                 sizeof(path)))
    {
        return false;
    }

    g_mode_select.document = g_mode_select.context->LoadDocument(path);
    if (g_mode_select.document == nullptr)
    {
        return false;
    }

    g_mode_select.root = GetElement("mode-select-root");
    g_mode_select.main_panel = GetElement("mode-select-main-panel");
    g_mode_select.story_panel = GetElement("mode-select-story-panel");
    g_mode_select.main_menu = GetElement("mode-select-main-menu");
    g_mode_select.story_menu = GetElement("mode-select-story-menu");
    g_mode_select.message_box = GetElement("mode-select-message-box");
    g_mode_select.message_text = GetElement("mode-select-message-text");

    for (int index = 0; index < 5; index++)
    {
        const std::string main_id = "mode-select-main-" + std::to_string(index);
        const std::string story_row_id = "mode-select-story-row-" + std::to_string(index);
        const std::string story_hit_id = "mode-select-story-hit-" + std::to_string(index);
        const std::string main_message_id = "mode-select-message-main-" + std::to_string(index);
        const std::string story_message_id = "mode-select-message-story-" + std::to_string(index);
        g_mode_select.main_buttons[index] = GetElement(main_id.c_str());
        g_mode_select.story_rows[index] = GetElement(story_row_id.c_str());
        g_mode_select.story_hits[index] = GetElement(story_hit_id.c_str());
        g_mode_select.main_messages[index] = GetElement(main_message_id.c_str());
        g_mode_select.story_messages[index] = GetElement(story_message_id.c_str());

        AddListener(g_mode_select.main_buttons[index],
                    Rml::EventId::Mouseover,
                    index,
                    MIKUPAN_RML_MODE_SELECT_COMMAND_NONE);
        AddListener(g_mode_select.main_buttons[index],
                    Rml::EventId::Click,
                    index,
                    MIKUPAN_RML_MODE_SELECT_COMMAND_CONFIRM);
        AddListener(g_mode_select.story_hits[index],
                    Rml::EventId::Mouseover,
                    index,
                    MIKUPAN_RML_MODE_SELECT_COMMAND_NONE);
        AddListener(g_mode_select.story_hits[index],
                    Rml::EventId::Click,
                    index,
                    MIKUPAN_RML_MODE_SELECT_COMMAND_CONFIRM);
    }

    for (int index = 0; index < 3; index++)
    {
        const std::string left_id = "mode-select-story-left-" + std::to_string(index);
        const std::string right_id = "mode-select-story-right-" + std::to_string(index);
        g_mode_select.story_left_arrows[index] = GetElement(left_id.c_str());
        g_mode_select.story_right_arrows[index] = GetElement(right_id.c_str());

        for (int value_index = 0; value_index < story_value_counts[index]; value_index++)
        {
            const std::string value_id = "mode-select-story-value-"
                + std::to_string(index) + "-" + std::to_string(value_index);
            g_mode_select.story_value_options[index][value_index] = GetElement(value_id.c_str());
        }

        AddListener(g_mode_select.story_left_arrows[index],
                    Rml::EventId::Mouseover,
                    index,
                    MIKUPAN_RML_MODE_SELECT_COMMAND_NONE);
        AddListener(g_mode_select.story_left_arrows[index],
                    Rml::EventId::Click,
                    index,
                    MIKUPAN_RML_MODE_SELECT_COMMAND_LEFT);
        AddListener(g_mode_select.story_right_arrows[index],
                    Rml::EventId::Mouseover,
                    index,
                    MIKUPAN_RML_MODE_SELECT_COMMAND_NONE);
        AddListener(g_mode_select.story_right_arrows[index],
                    Rml::EventId::Click,
                    index,
                    MIKUPAN_RML_MODE_SELECT_COMMAND_RIGHT);
    }

    const bool messages_ready = std::all_of(
        g_mode_select.main_messages.begin(),
        g_mode_select.main_messages.end(),
        [](Rml::Element* element) { return element != nullptr; })
        && std::all_of(
            g_mode_select.story_messages.begin(),
            g_mode_select.story_messages.end(),
            [](Rml::Element* element) { return element != nullptr; });

    bool values_ready = true;
    for (int row = 0; row < 3; row++)
    {
        for (int value = 0; value < story_value_counts[row]; value++)
        {
            values_ready = values_ready
                && g_mode_select.story_value_options[row][value] != nullptr;
        }
    }

    if (g_mode_select.root == nullptr
        || g_mode_select.main_panel == nullptr
        || g_mode_select.story_panel == nullptr
        || g_mode_select.message_box == nullptr
        || g_mode_select.message_text == nullptr
        || !messages_ready
        || !values_ready)
    {
        return false;
    }

    SetHidden(g_mode_select.root, true);
    g_mode_select.document->Hide();
    return true;
}

void SyncMain(int selected_index, float opacity)
{
    const int selection = std::clamp(selected_index, 0, 4);
    RequestVisible();
    SetHidden(g_mode_select.main_panel, false);
    SetHidden(g_mode_select.story_panel, true);
    SetOpacity(g_mode_select.main_menu, opacity);
    SetOpacity(g_mode_select.message_box, opacity);

    for (int index = 0; index < 5; index++)
    {
        if (g_mode_select.main_buttons[index] != nullptr)
        {
            g_mode_select.main_buttons[index]->SetClass("selected", index == selection);
        }
    }

    SetMessage(false, selection);
}

void SyncStory(int selected_index,
               int editing,
               int first_enabled_index,
               int chapter,
               int difficulty,
               int costume,
               float opacity)
{
    const int first_enabled = std::clamp(first_enabled_index, 0, 4);
    const int selection = std::clamp(selected_index, first_enabled, 4);
    RequestVisible();
    SetHidden(g_mode_select.main_panel, true);
    SetHidden(g_mode_select.story_panel, false);
    SetOpacity(g_mode_select.story_menu, opacity);
    SetOpacity(g_mode_select.message_box, opacity);

    for (int index = 0; index < 5; index++)
    {
        const bool disabled = index < first_enabled;
        const bool selected = index == selection;
        const bool is_editing = selected && editing != 0 && index < 3;
        if (g_mode_select.story_rows[index] != nullptr)
        {
            g_mode_select.story_rows[index]->SetClass("disabled", disabled);
            g_mode_select.story_rows[index]->SetClass("selected", selected);
            g_mode_select.story_rows[index]->SetClass("editing", is_editing);
        }
        if (g_mode_select.story_hits[index] != nullptr)
        {
            g_mode_select.story_hits[index]->SetClass("disabled", disabled);
        }
    }

    SetStoryValue(0, chapter);
    SetStoryValue(1, difficulty);
    SetStoryValue(2, costume);

    for (int index = 0; index < 3; index++)
    {
        const bool active = index == selection && editing != 0;
        if (g_mode_select.story_left_arrows[index] != nullptr)
        {
            g_mode_select.story_left_arrows[index]->SetClass("active", active);
            g_mode_select.story_left_arrows[index]->SetClass("disabled", index < first_enabled);
        }
        if (g_mode_select.story_right_arrows[index] != nullptr)
        {
            g_mode_select.story_right_arrows[index]->SetClass("active", active);
            g_mode_select.story_right_arrows[index]->SetClass("disabled", index < first_enabled);
        }
    }

    SetMessage(true, selection);
}

}

bool MikuPan_RmlModeSelectInit(Rml::Context* context)
{
    if (g_mode_select.initialized)
    {
        return true;
    }

    g_mode_select.context = context;
    if (context == nullptr || !LoadDocument())
    {
        g_mode_select = ModeSelectState();
        return false;
    }

    g_mode_select.initialized = true;
    return true;
}

void MikuPan_RmlModeSelectStartFrame(void)
{
    if (!g_mode_select.initialized)
    {
        return;
    }

    if (!g_mode_select.shown_last_frame)
    {
        HideDocument();
    }
    g_mode_select.shown_last_frame = false;
}

void MikuPan_RmlModeSelectPrepareShutdown(void)
{
    g_mode_select.visible = false;
    g_mode_select.shown_last_frame = false;
}

void MikuPan_RmlModeSelectShutdown(void)
{
    g_mode_select = ModeSelectState();
}

extern "C" {

int MikuPan_RmlModeSelectIsAvailable(void)
{
    return g_mode_select.initialized && g_mode_select.document != nullptr ? 1 : 0;
}

void MikuPan_RmlModeSelectShowMain(int selected_index, float opacity)
{
    if (!g_mode_select.initialized)
    {
        return;
    }
    SyncMain(selected_index, opacity);
}

void MikuPan_RmlModeSelectShowStory(int selected_index,
                                    int editing,
                                    int first_enabled_index,
                                    int chapter,
                                    int difficulty,
                                    int costume,
                                    float opacity)
{
    if (!g_mode_select.initialized)
    {
        return;
    }
    SyncStory(selected_index,
              editing,
              first_enabled_index,
              chapter,
              difficulty,
              costume,
              opacity);
}

int MikuPan_RmlModeSelectConsumeSelection(void)
{
    const int selection = g_mode_select.queued_selection;
    g_mode_select.queued_selection = -1;
    return selection;
}

int MikuPan_RmlModeSelectConsumeCommand(void)
{
    const int command = g_mode_select.queued_command;
    g_mode_select.queued_command = MIKUPAN_RML_MODE_SELECT_COMMAND_NONE;
    return command;
}

}
