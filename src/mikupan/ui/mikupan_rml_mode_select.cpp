#include "mikupan/ui/mikupan_rml_mode_select.h"

#include "main/glob.h"
#include "mikupan/io/mikupan_file.h"
#include "mikupan/mikupan_i18n.h"
#include "mikupan/mikupan_utils.h"

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
    int active_panel = -1;
    int main_selection = -1;
    int story_selection = -1;
    int story_editing = -1;
    int story_first_enabled = -1;
    int message_panel = -1;
    int message_selection = -1;
    std::array<int, 3> story_values = {-1, -1, -1};
    float main_opacity = -1.0f;
    float story_opacity = -1.0f;
    float message_opacity = -1.0f;
};

ModeSelectState g_mode_select;

constexpr std::array<int, 3> story_value_counts = {6, 2, 4};

Rml::Element* GetElement(const char* id)
{
    return g_mode_select.document != nullptr
        ? g_mode_select.document->GetElementById(id)
        : nullptr;
}

void SetClass(Rml::Element* element, const char* class_name, bool enabled)
{
    if (element != nullptr && element->IsClassSet(class_name) != enabled)
    {
        element->SetClass(class_name, enabled);
    }
}

void SetHidden(Rml::Element* element, bool hidden)
{
    SetClass(element, "hidden", hidden);
}

void SetPanelHidden(Rml::Element* element, bool hidden)
{
    SetClass(element, "mode-select-panel-hidden", hidden);
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
    const int panel = story ? 1 : 0;
    if (g_mode_select.message_panel == panel
        && g_mode_select.message_selection == selection)
    {
        return;
    }

    SetActiveElement(g_mode_select.main_messages, story ? -1 : selection);
    SetActiveElement(g_mode_select.story_messages, story ? selection : -1);
    g_mode_select.message_panel = panel;
    g_mode_select.message_selection = selection;
}

void SetStoryValue(int row, int value)
{
    if (row < 0 || row >= static_cast<int>(g_mode_select.story_value_options.size()))
    {
        return;
    }

    const int count = story_value_counts[row];
    const int selected = std::clamp(value, 0, count - 1);
    if (g_mode_select.story_values[row] == selected)
    {
        return;
    }

    for (int index = 0; index < count; index++)
    {
        SetHidden(g_mode_select.story_value_options[row][index], index != selected);
    }
    g_mode_select.story_values[row] = selected;
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

    if (g_mode_select.active_panel != 0)
    {
        for (int index = 0; index < 5; index++)
        {
            SetClass(g_mode_select.story_rows[index], "selected", false);
            SetClass(g_mode_select.story_rows[index], "editing", false);
        }
        for (int index = 0; index < 3; index++)
        {
            SetClass(g_mode_select.story_left_arrows[index], "active", false);
            SetClass(g_mode_select.story_right_arrows[index], "active", false);
        }
        SetPanelHidden(g_mode_select.main_panel, false);
        SetPanelHidden(g_mode_select.story_panel, true);
        g_mode_select.active_panel = 0;
        g_mode_select.main_selection = -1;
        g_mode_select.story_selection = -1;
        g_mode_select.story_editing = -1;
    }

    if (g_mode_select.main_opacity != opacity)
    {
        SetOpacity(g_mode_select.main_menu, opacity);
        g_mode_select.main_opacity = opacity;
    }
    if (g_mode_select.message_opacity != opacity)
    {
        SetOpacity(g_mode_select.message_box, opacity);
        g_mode_select.message_opacity = opacity;
    }

    if (g_mode_select.main_selection != selection)
    {
        for (int index = 0; index < 5; index++)
        {
            SetClass(g_mode_select.main_buttons[index], "selected", index == selection);
        }
        g_mode_select.main_selection = selection;
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
    const int editing_state = editing != 0 ? 1 : 0;
    RequestVisible();

    if (g_mode_select.active_panel != 1)
    {
        for (int index = 0; index < 5; index++)
        {
            SetClass(g_mode_select.main_buttons[index], "selected", false);
        }
        SetPanelHidden(g_mode_select.main_panel, true);
        SetPanelHidden(g_mode_select.story_panel, false);
        g_mode_select.active_panel = 1;
        g_mode_select.main_selection = -1;
        g_mode_select.story_selection = -1;
        g_mode_select.story_editing = -1;
        g_mode_select.story_first_enabled = -1;
    }

    if (g_mode_select.story_opacity != opacity)
    {
        SetOpacity(g_mode_select.story_menu, opacity);
        g_mode_select.story_opacity = opacity;
    }
    if (g_mode_select.message_opacity != opacity)
    {
        SetOpacity(g_mode_select.message_box, opacity);
        g_mode_select.message_opacity = opacity;
    }

    if (g_mode_select.story_selection != selection
        || g_mode_select.story_editing != editing_state
        || g_mode_select.story_first_enabled != first_enabled)
    {
        for (int index = 0; index < 5; index++)
        {
            const bool disabled = index < first_enabled;
            const bool selected = index == selection;
            const bool is_editing = selected && editing_state != 0 && index < 3;
            SetClass(g_mode_select.story_rows[index], "disabled", disabled);
            SetClass(g_mode_select.story_rows[index], "selected", selected);
            SetClass(g_mode_select.story_rows[index], "editing", is_editing);
            SetClass(g_mode_select.story_hits[index], "disabled", disabled);
        }

        for (int index = 0; index < 3; index++)
        {
            const bool active = index == selection && editing_state != 0;
            const bool disabled = index < first_enabled;
            SetClass(g_mode_select.story_left_arrows[index], "active", active);
            SetClass(g_mode_select.story_left_arrows[index], "disabled", disabled);
            SetClass(g_mode_select.story_right_arrows[index], "active", active);
            SetClass(g_mode_select.story_right_arrows[index], "disabled", disabled);
        }

        g_mode_select.story_selection = selection;
        g_mode_select.story_editing = editing_state;
        g_mode_select.story_first_enabled = first_enabled;
    }

    SetStoryValue(0, chapter);
    SetStoryValue(1, difficulty);
    SetStoryValue(2, costume);
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
    MikuPan_RmlModeSelectApplyLanguage(MikuPan_GetUiLanguage());
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

void MikuPan_RmlModeSelectApplyLanguage(int language)
{
    if (!g_mode_select.initialized)
    {
        return;
    }

    (void)language;

    static const char* const kFilmTitles[2] = {
        "MODE SELECT", "STORY MODE",
    };
    if (g_mode_select.main_panel != nullptr)
    {
        if (Rml::Element* title = g_mode_select.main_panel->QuerySelector(".mode-select-film-title"))
        {
            title->SetInnerRML(MikuPan_Translate(kFilmTitles[0]));
        }
    }
    if (g_mode_select.story_panel != nullptr)
    {
        if (Rml::Element* title = g_mode_select.story_panel->QuerySelector(".mode-select-film-title"))
        {
            title->SetInnerRML(MikuPan_Translate(kFilmTitles[1]));
        }
    }

    static const char* const kMainLabels[5] = {
        "Story Mode", "Battle Mode", "Option", "Sound Test", "Exit",
    };
    for (int i = 0; i < 5; i++)
    {
        Rml::Element* button = g_mode_select.main_buttons[i];
        Rml::Element* label = button != nullptr ? button->QuerySelector(".mode-select-main-label") : nullptr;
        if (label != nullptr)
        {
            label->SetInnerRML(MikuPan_Translate(kMainLabels[i]));
        }
    }

    static const char* const kStoryRowLabels[5] = {
        "Chapter", "Difficulty", "Costume", "Game Start", "Exit",
    };
    static const char* const kStoryRowSelectors[5] = {
        ".mode-select-story-label",
        ".mode-select-story-label",
        ".mode-select-story-label",
        ".mode-select-story-action",
        ".mode-select-story-action",
    };
    for (int i = 0; i < 5; i++)
    {
        Rml::Element* row = g_mode_select.story_rows[i];
        Rml::Element* label = row != nullptr ? row->QuerySelector(kStoryRowSelectors[i]) : nullptr;
        if (label != nullptr)
        {
            label->SetInnerRML(MikuPan_Translate(kStoryRowLabels[i]));
        }
    }

    static const char* const kChapterValues[6] = {
        "Continue", "Prologue", "Night 1", "Night 2", "Night 3", "Final Night",
    };
    static const char* const kDifficultyValues[2] = {
        "Normal", "Nightmare",
    };
    static const char* const kCostumeValues[4] = {
        "Normal", "Special 1", "Special 2", "Special 3",
    };
    for (int value = 0; value < 6; value++)
    {
        if (Rml::Element* element = g_mode_select.story_value_options[0][value])
        {
            element->SetInnerRML(MikuPan_Translate(kChapterValues[value]));
        }
    }
    for (int value = 0; value < 2; value++)
    {
        if (Rml::Element* element = g_mode_select.story_value_options[1][value])
        {
            element->SetInnerRML(MikuPan_Translate(kDifficultyValues[value]));
        }
    }
    for (int value = 0; value < 4; value++)
    {
        if (Rml::Element* element = g_mode_select.story_value_options[2][value])
        {
            element->SetInnerRML(MikuPan_Translate(kCostumeValues[value]));
        }
    }

    static const char* const kMainMessages[5] = {
        "Regular game mode. Start from the chapter of your choice.",
        "Battle against ghosts in a series of missions.",
        "Change the game settings.",
        "Listen to music and sound effects.",
        "Return to the title screen.",
    };
    static const char* const kStoryMessages[5] = {
        "Choose the chapter to start from.",
        "Choose the game difficulty.",
        "Choose Miku's costume.",
        "Start the game with these settings.",
        "Return to Mode Select.",
    };
    for (int i = 0; i < 5; i++)
    {
        if (g_mode_select.main_messages[i] != nullptr)
        {
            g_mode_select.main_messages[i]->SetInnerRML(MikuPan_Translate(kMainMessages[i]));
        }
        if (g_mode_select.story_messages[i] != nullptr)
        {
            g_mode_select.story_messages[i]->SetInnerRML(MikuPan_Translate(kStoryMessages[i]));
        }
    }
}

}
