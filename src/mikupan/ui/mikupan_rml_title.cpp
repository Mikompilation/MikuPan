#include "mikupan/ui/mikupan_rml_title.h"

#include "mikupan/io/mikupan_file.h"

#include "mikupan_version.h"

#include "RmlUi/Core.h"
#include "RmlUi/Core/ElementDocument.h"
#include "RmlUi/Core/EventListener.h"
#include "SDL3/SDL_timer.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <memory>
#include <string>
#include <vector>

class MikuPanTitleButtonListener final : public Rml::EventListener
{
public:
    MikuPanTitleButtonListener(int in_selection, int in_command)
        : selection(in_selection), command(in_command)
    {
    }

    void ProcessEvent(Rml::Event& event) override;

private:
    int selection = -1;
    int command = MIKUPAN_RML_TITLE_COMMAND_NONE;
};

class MikuPanTitleHoverListener final : public Rml::EventListener
{
public:
    explicit MikuPanTitleHoverListener(int in_selection)
        : selection(in_selection)
    {
    }

    void ProcessEvent(Rml::Event& event) override;

private:
    int selection = -1;
};

struct MikuPanRmlTitleState
{
    Rml::Context* context = nullptr;
    Rml::ElementDocument* document = nullptr;
    Rml::Element* root = nullptr;
    Rml::Element* focus_group = nullptr;
    Rml::Element* logo_group = nullptr;
    Rml::Element* smoke_a = nullptr;
    Rml::Element* smoke_b = nullptr;
    Rml::Element* ring_glow_a = nullptr;
    Rml::Element* ring_glow_b = nullptr;
    Rml::Element* ring = nullptr;
    Rml::Element* logo = nullptr;
    Rml::Element* press_button = nullptr;
    Rml::Element* menu_panel = nullptr;
    Rml::Element* exit_dialog = nullptr;
    Rml::Element* exit_yes_button = nullptr;
    Rml::Element* exit_no_button = nullptr;
    Rml::Element* build_info = nullptr;
    Rml::Element* settings_button = nullptr;
    Rml::Element* exit_button = nullptr;
    std::array<Rml::Element*, 5> menu_buttons{};
    std::vector<std::unique_ptr<Rml::EventListener>> listeners;
    int queued_command = MIKUPAN_RML_TITLE_COMMAND_NONE;
    int queued_selection = -1;
    int active_selection = 0;
    int active_menu_count = 0;
    bool initialized = false;
    bool visible = false;
    bool shown_last_frame = false;
};

static MikuPanRmlTitleState g_title;

static void MikuPan_RmlTitleSetFloatProperty(Rml::Element* element,
                                             const char* property,
                                             float value);
static void MikuPan_RmlTitleSetDpProperty(Rml::Element* element,
                                          const char* property,
                                          float value);

static int g_title_layout_initialized = 0;
static int g_title_restore_from_settings = 0;
static int g_title_restored_selection = 2;
static int g_title_input_cooldown_frames = 0;

static float g_title_focus_margin_top_dp = -202.0f;
static float g_title_focus_width_dp = 510.0f;
static float g_title_focus_height_dp = 404.0f;
static float g_title_logo_group_top_dp = 32.0f;
static float g_title_logo_group_width_dp = 510.0f;
static float g_title_logo_group_height_dp = 232.0f;
static float g_title_ring_top_dp = -15.0f;
static float g_title_ring_size_dp = 180.0f;
static float g_title_ring_glow_opacity = 0.6f;
static float g_title_ring_glow_degrees_per_second = 15.0f;
static float g_title_logo_top_dp = 12.0f;
static float g_title_logo_width_dp = 400.0f;
static float g_title_logo_height_dp = 130.0f;
static float g_title_smoke_width_dp = 620.0f;
static float g_title_smoke_height_dp = 246.0f;
static float g_title_smoke_a_left_dp = -60.0f;
static float g_title_smoke_a_top_dp = -30.0f;
static float g_title_smoke_b_left_dp = -50.0f;
static float g_title_smoke_b_top_dp = -30.0f;
static float g_title_press_top_dp = 306.0f;
static float g_title_menu_top_dp = 276.0f;

static void MikuPan_RmlTitleApplyLayout(void)
{
    if (g_title.logo_group == nullptr)
    {
        return;
    }

    MikuPan_RmlTitleSetDpProperty(g_title.focus_group, "margin-left", -g_title_focus_width_dp * 0.5f);
    MikuPan_RmlTitleSetDpProperty(g_title.focus_group, "margin-top", g_title_focus_margin_top_dp);
    MikuPan_RmlTitleSetDpProperty(g_title.focus_group, "width", g_title_focus_width_dp);
    MikuPan_RmlTitleSetDpProperty(g_title.focus_group, "height", g_title_focus_height_dp);

    MikuPan_RmlTitleSetDpProperty(g_title.press_button, "top", g_title_press_top_dp);
    MikuPan_RmlTitleSetDpProperty(g_title.menu_panel, "top", g_title_menu_top_dp);
    MikuPan_RmlTitleSetDpProperty(g_title.smoke_a, "width", g_title_smoke_width_dp);
    MikuPan_RmlTitleSetDpProperty(g_title.smoke_a, "height", g_title_smoke_height_dp);
    MikuPan_RmlTitleSetDpProperty(g_title.smoke_b, "width", g_title_smoke_width_dp);
    MikuPan_RmlTitleSetDpProperty(g_title.smoke_b, "height", g_title_smoke_height_dp);

    MikuPan_RmlTitleSetDpProperty(g_title.logo_group, "top", g_title_logo_group_top_dp);
    MikuPan_RmlTitleSetDpProperty(g_title.logo_group, "margin-left", -g_title_logo_group_width_dp * 0.5f);
    MikuPan_RmlTitleSetDpProperty(g_title.logo_group, "width", g_title_logo_group_width_dp);
    MikuPan_RmlTitleSetDpProperty(g_title.logo_group, "height", g_title_logo_group_height_dp);

    MikuPan_RmlTitleSetDpProperty(g_title.ring_glow_a, "top", g_title_ring_top_dp);
    MikuPan_RmlTitleSetDpProperty(g_title.ring_glow_a, "margin-left", -g_title_ring_size_dp * 0.5f);
    MikuPan_RmlTitleSetDpProperty(g_title.ring_glow_a, "width", g_title_ring_size_dp);
    MikuPan_RmlTitleSetDpProperty(g_title.ring_glow_a, "height", g_title_ring_size_dp);
    MikuPan_RmlTitleSetFloatProperty(g_title.ring_glow_a, "opacity", g_title_ring_glow_opacity);
    if (g_title.ring_glow_a != nullptr)
    {
        g_title.ring_glow_a->SetProperty("transform-origin", "50% 50%");
    }

    MikuPan_RmlTitleSetDpProperty(g_title.ring_glow_b, "top", g_title_ring_top_dp);
    MikuPan_RmlTitleSetDpProperty(g_title.ring_glow_b, "margin-left", -g_title_ring_size_dp * 0.5f);
    MikuPan_RmlTitleSetDpProperty(g_title.ring_glow_b, "width", g_title_ring_size_dp);
    MikuPan_RmlTitleSetDpProperty(g_title.ring_glow_b, "height", g_title_ring_size_dp);
    MikuPan_RmlTitleSetFloatProperty(g_title.ring_glow_b, "opacity", g_title_ring_glow_opacity);
    if (g_title.ring_glow_b != nullptr)
    {
        g_title.ring_glow_b->SetProperty("transform-origin", "50% 50%");
    }

    MikuPan_RmlTitleSetDpProperty(g_title.ring, "top", g_title_ring_top_dp);
    MikuPan_RmlTitleSetDpProperty(g_title.ring, "margin-left", -g_title_ring_size_dp * 0.5f);
    MikuPan_RmlTitleSetDpProperty(g_title.ring, "width", g_title_ring_size_dp);
    MikuPan_RmlTitleSetDpProperty(g_title.ring, "height", g_title_ring_size_dp);

    MikuPan_RmlTitleSetDpProperty(g_title.logo, "top", g_title_logo_top_dp);
    MikuPan_RmlTitleSetDpProperty(g_title.logo, "margin-left", -g_title_logo_width_dp * 0.5f);
    MikuPan_RmlTitleSetDpProperty(g_title.logo, "width", g_title_logo_width_dp);
    MikuPan_RmlTitleSetDpProperty(g_title.logo, "height", g_title_logo_height_dp);
}

static void MikuPan_RmlTitleSetElementHidden(Rml::Element* element,
                                             bool hidden)
{
    if (element != nullptr)
    {
        element->SetClass("hidden", hidden);
    }
}

static void MikuPan_RmlTitleAddListener(
    Rml::Element* element,
    Rml::EventId event_id,
    std::unique_ptr<Rml::EventListener> listener)
{
    if (element == nullptr || listener == nullptr)
    {
        return;
    }

    element->AddEventListener(event_id, listener.get());
    g_title.listeners.push_back(std::move(listener));
}

static Rml::Element* MikuPan_RmlTitleGetElement(const char* id)
{
    return g_title.document != nullptr ? g_title.document->GetElementById(id)
                                      : nullptr;
}

static void MikuPan_RmlTitleUpdateBuildInfo(void)
{
    if (g_title.build_info == nullptr)
    {
        return;
    }

    char buffer[512];
    std::snprintf(buffer,
                  sizeof(buffer),
                  "MikuPan Build: %s<br/>Version: %s (%s)<br/>https://github.com/Mikompilation/MikuPan",
                  MIKUPAN_BUILD_DATE,
                  MIKUPAN_GIT_DESCRIBE,
                  MIKUPAN_GIT_COMMIT);
    g_title.build_info->SetInnerRML(buffer);
}

static void MikuPan_RmlTitleRequestVisible(void)
{
    g_title.shown_last_frame = true;
    if (g_title.document == nullptr || g_title.root == nullptr)
    {
        return;
    }

    g_title.root->SetClass("hidden", false);
    if (!g_title.visible)
    {
        g_title.document->Show(Rml::ModalFlag::None, Rml::FocusFlag::None);
        g_title.visible = true;
    }
}

static void MikuPan_RmlTitleHideDocument(void)
{
    if (!g_title.visible || g_title.document == nullptr)
    {
        return;
    }

    g_title.visible = false;
    g_title.document->Hide();
}

static void MikuPan_RmlTitleSetFloatProperty(Rml::Element* element,
                                             const char* property,
                                             float value)
{
    if (element == nullptr)
    {
        return;
    }

    char buffer[32];
    std::snprintf(buffer, sizeof(buffer), "%.3f", value);
    element->SetProperty(property, buffer);
}

static void MikuPan_RmlTitleSetDpProperty(Rml::Element* element,
                                          const char* property,
                                          float value)
{
    if (element == nullptr)
    {
        return;
    }

    char buffer[32];
    std::snprintf(buffer, sizeof(buffer), "%.3fdp", value);
    element->SetProperty(property, buffer);
}

static void MikuPan_RmlTitleUpdateSmoke(void)
{
    if (g_title.smoke_a == nullptr && g_title.smoke_b == nullptr
        && g_title.ring_glow_a == nullptr && g_title.ring_glow_b == nullptr)
    {
        return;
    }

    const float time = static_cast<float>(SDL_GetTicks()) * 0.001f;

    const float smoke_drift_a = std::sin(time * 0.47f) * 4.0f;
    const float smoke_lift_a = std::sin(time * 0.31f + 0.8f) * 2.0f;
    const float smoke_drift_b = std::sin(time * 0.39f + 2.1f) * 5.0f;
    const float smoke_lift_b = std::sin(time * 0.53f + 1.4f) * 2.5f;
    const float smoke_pulse_a = 0.26f + std::sin(time * 0.72f) * 0.04f;
    const float smoke_pulse_b = 0.18f + std::sin(time * 0.61f + 1.7f) * 0.035f;
    const float glow_angle = std::fmod(time * g_title_ring_glow_degrees_per_second, 360.0f);
    const float glow_counter_angle = std::fmod(-time * g_title_ring_glow_degrees_per_second, 360.0f);

    MikuPan_RmlTitleSetDpProperty(g_title.smoke_a, "left", g_title_smoke_a_left_dp + smoke_drift_a);
    MikuPan_RmlTitleSetDpProperty(g_title.smoke_a, "top", g_title_smoke_a_top_dp + smoke_lift_a);
    MikuPan_RmlTitleSetFloatProperty(
        g_title.smoke_a, "opacity", std::clamp(smoke_pulse_a, 0.14f, 0.36f));

    MikuPan_RmlTitleSetDpProperty(g_title.smoke_b, "left", g_title_smoke_b_left_dp + smoke_drift_b);
    MikuPan_RmlTitleSetDpProperty(g_title.smoke_b, "top", g_title_smoke_b_top_dp + smoke_lift_b);
    MikuPan_RmlTitleSetFloatProperty(
        g_title.smoke_b, "opacity", std::clamp(smoke_pulse_b, 0.10f, 0.28f));

    if (g_title.ring_glow_a != nullptr)
    {
        char transform[64];
        std::snprintf(transform, sizeof(transform), "rotate(%.3fdeg)", glow_angle);
        g_title.ring_glow_a->SetProperty("transform", transform);
    }

    if (g_title.ring_glow_b != nullptr)
    {
        char transform[96];
        std::snprintf(transform, sizeof(transform), "rotate(%.3fdeg)", glow_counter_angle);
        g_title.ring_glow_b->SetProperty("transform", transform);
    }
}

static void MikuPan_RmlTitleSyncMenu(void)
{
    const int count = std::clamp(g_title.active_menu_count, 0, 5);
    for (int i = 0; i < 5; i++)
    {
        Rml::Element* button = g_title.menu_buttons[i];
        if (button == nullptr)
        {
            continue;
        }

        const bool visible = i < count;
        button->SetClass("hidden", !visible);
        button->SetClass("selected", visible && i == g_title.active_selection);
    }
}

static void MikuPan_RmlTitleSyncExitPrompt(int selected_index)
{
    if (g_title.exit_yes_button != nullptr)
    {
        g_title.exit_yes_button->SetClass("selected", selected_index == 0);
    }

    if (g_title.exit_no_button != nullptr)
    {
        g_title.exit_no_button->SetClass("selected", selected_index != 0);
    }
}

static void MikuPan_RmlTitleQueueCommand(int selection, int command)
{
    g_title.queued_selection = selection;
    g_title.queued_command = command;
}

void MikuPanTitleButtonListener::ProcessEvent(Rml::Event& event)
{
    (void) event;
    MikuPan_RmlTitleQueueCommand(selection, command);
}

void MikuPanTitleHoverListener::ProcessEvent(Rml::Event& event)
{
    (void) event;
    g_title.queued_selection = selection;
}

static bool MikuPan_RmlTitleLoadDocument(void)
{
    char document_path[1024];
    if (!MikuPan_ResolveBasePath("resources/rml/title.rml",
                                 document_path,
                                 sizeof(document_path)))
    {
        return false;
    }

    g_title.document = g_title.context->LoadDocument(document_path);
    if (g_title.document == nullptr)
    {
        return false;
    }

    g_title.root = MikuPan_RmlTitleGetElement("title-root");
    g_title.focus_group = MikuPan_RmlTitleGetElement("title-focus-group");
    g_title.logo_group = MikuPan_RmlTitleGetElement("title-logo-group");
    g_title.smoke_a = MikuPan_RmlTitleGetElement("title-smoke-a");
    g_title.smoke_b = MikuPan_RmlTitleGetElement("title-smoke-b");
    g_title.ring_glow_a = MikuPan_RmlTitleGetElement("title-ring-glow-a");
    g_title.ring_glow_b = MikuPan_RmlTitleGetElement("title-ring-glow-b");
    g_title.ring = MikuPan_RmlTitleGetElement("title-ring");
    g_title.logo = MikuPan_RmlTitleGetElement("title-logo");
    g_title.press_button = MikuPan_RmlTitleGetElement("title-press-button");
    g_title.menu_panel = MikuPan_RmlTitleGetElement("title-menu-panel");
    g_title.exit_dialog = MikuPan_RmlTitleGetElement("title-exit-dialog");
    g_title.exit_yes_button = MikuPan_RmlTitleGetElement("title-exit-yes");
    g_title.exit_no_button = MikuPan_RmlTitleGetElement("title-exit-no");
    g_title.build_info = MikuPan_RmlTitleGetElement("title-build-info");
    g_title.settings_button = MikuPan_RmlTitleGetElement("title-menu-settings");
    g_title.exit_button = MikuPan_RmlTitleGetElement("title-menu-exit");

    g_title.menu_buttons[0] = MikuPan_RmlTitleGetElement("title-menu-new");
    g_title.menu_buttons[1] = MikuPan_RmlTitleGetElement("title-menu-load");
    g_title.menu_buttons[2] = MikuPan_RmlTitleGetElement("title-menu-album");
    g_title.menu_buttons[3] = g_title.settings_button;
    g_title.menu_buttons[4] = g_title.exit_button;

    MikuPan_RmlTitleAddListener(
        g_title.press_button,
        Rml::EventId::Click,
        std::make_unique<MikuPanTitleButtonListener>(
            -1, MIKUPAN_RML_TITLE_COMMAND_PRESS_START));

    static constexpr int kCommands[5] = {
        MIKUPAN_RML_TITLE_COMMAND_NEW_GAME,
        MIKUPAN_RML_TITLE_COMMAND_LOAD_GAME,
        MIKUPAN_RML_TITLE_COMMAND_ALBUM,
        MIKUPAN_RML_TITLE_COMMAND_SETTINGS,
        MIKUPAN_RML_TITLE_COMMAND_EXIT_GAME,
    };

    for (int i = 0; i < 5; i++)
    {
        MikuPan_RmlTitleAddListener(
            g_title.menu_buttons[i],
            Rml::EventId::Mouseover,
            std::make_unique<MikuPanTitleHoverListener>(i));
        MikuPan_RmlTitleAddListener(
            g_title.menu_buttons[i],
            Rml::EventId::Click,
            std::make_unique<MikuPanTitleButtonListener>(i, kCommands[i]));
    }

    MikuPan_RmlTitleAddListener(
        g_title.exit_yes_button,
        Rml::EventId::Mouseover,
        std::make_unique<MikuPanTitleHoverListener>(0));
    MikuPan_RmlTitleAddListener(
        g_title.exit_yes_button,
        Rml::EventId::Click,
        std::make_unique<MikuPanTitleButtonListener>(
            0, MIKUPAN_RML_TITLE_COMMAND_EXIT_YES));
    MikuPan_RmlTitleAddListener(
        g_title.exit_no_button,
        Rml::EventId::Mouseover,
        std::make_unique<MikuPanTitleHoverListener>(1));
    MikuPan_RmlTitleAddListener(
        g_title.exit_no_button,
        Rml::EventId::Click,
        std::make_unique<MikuPanTitleButtonListener>(
            1, MIKUPAN_RML_TITLE_COMMAND_EXIT_NO));

    MikuPan_RmlTitleApplyLayout();
    MikuPan_RmlTitleUpdateBuildInfo();
    g_title_layout_initialized = 1;
    MikuPan_RmlTitleSetElementHidden(g_title.root, true);
    MikuPan_RmlTitleSetElementHidden(g_title.menu_panel, true);
    g_title.document->Hide();
    return true;
}

bool MikuPan_RmlTitleInit(Rml::Context* context)
{
    if (g_title.initialized)
    {
        return true;
    }

    g_title.context = context;
    if (g_title.context == nullptr || !MikuPan_RmlTitleLoadDocument())
    {
        g_title = MikuPanRmlTitleState();
        return false;
    }

    g_title.initialized = true;
    return true;
}

void MikuPan_RmlTitleStartFrame(void)
{
    if (!g_title.initialized)
    {
        return;
    }

    if (!g_title.shown_last_frame)
    {
        MikuPan_RmlTitleHideDocument();
    }

    g_title.shown_last_frame = false;
    if (g_title.visible)
    {
        if (g_title_layout_initialized != 0)
        {
            MikuPan_RmlTitleApplyLayout();
        }
        MikuPan_RmlTitleUpdateSmoke();
    }
}

void MikuPan_RmlTitlePrepareShutdown(void)
{
    g_title.visible = false;
    g_title.shown_last_frame = false;
    g_title_input_cooldown_frames = 0;
}

void MikuPan_RmlTitleShutdown(void)
{
    g_title = MikuPanRmlTitleState();
}

extern "C" {

int MikuPan_RmlTitleIsAvailable(void)
{
    return g_title.initialized && g_title.document != nullptr ? 1 : 0;
}

void MikuPan_RmlTitleShowPress(float prompt_alpha)
{
    if (!g_title.initialized)
    {
        return;
    }

    MikuPan_RmlTitleRequestVisible();
    MikuPan_RmlTitleSetElementHidden(g_title.logo_group, false);
    MikuPan_RmlTitleSetElementHidden(g_title.press_button, false);
    MikuPan_RmlTitleSetElementHidden(g_title.menu_panel, true);
    MikuPan_RmlTitleSetElementHidden(g_title.exit_dialog, true);
    MikuPan_RmlTitleSetFloatProperty(
        g_title.press_button, "opacity", std::clamp(prompt_alpha, 0.0f, 1.0f));
    MikuPan_RmlTitleUpdateSmoke();
}

void MikuPan_RmlTitleShowMenu(int selected_index, int menu_count)
{
    if (!g_title.initialized)
    {
        return;
    }

    const int count = std::clamp(menu_count, 0, 5);
    g_title.active_menu_count = count;
    g_title.active_selection = count > 0 ? std::clamp(selected_index, 0, count - 1) : 0;

    MikuPan_RmlTitleRequestVisible();
    MikuPan_RmlTitleSetElementHidden(g_title.logo_group, false);
    MikuPan_RmlTitleSetElementHidden(g_title.press_button, true);
    MikuPan_RmlTitleSetElementHidden(g_title.menu_panel, false);
    MikuPan_RmlTitleSetElementHidden(g_title.exit_dialog, true);
    MikuPan_RmlTitleSyncMenu();
    MikuPan_RmlTitleUpdateSmoke();
}

void MikuPan_RmlTitleShowExitPrompt(int selected_index)
{
    if (!g_title.initialized)
    {
        return;
    }

    MikuPan_RmlTitleRequestVisible();
    MikuPan_RmlTitleSetElementHidden(g_title.logo_group, false);
    MikuPan_RmlTitleSetElementHidden(g_title.press_button, true);
    MikuPan_RmlTitleSetElementHidden(g_title.menu_panel, true);
    MikuPan_RmlTitleSetElementHidden(g_title.exit_dialog, false);
    MikuPan_RmlTitleSyncExitPrompt(selected_index);
    MikuPan_RmlTitleUpdateSmoke();
}

int MikuPan_RmlTitleConsumeCommand(void)
{
    const int command = g_title.queued_command;
    g_title.queued_command = MIKUPAN_RML_TITLE_COMMAND_NONE;
    return command;
}

int MikuPan_RmlTitleConsumeSelection(void)
{
    const int selection = g_title.queued_selection;
    g_title.queued_selection = -1;
    return selection;
}

void MikuPan_RmlTitleNotifySettingsOpened(int selection)
{
    g_title_restore_from_settings = 1;
    g_title_restored_selection = selection;
}

int MikuPan_RmlTitleConsumeSettingsReturnSelection(void)
{
    if (g_title_restore_from_settings == 0)
    {
        return -1;
    }

    g_title_restore_from_settings = 0;
    g_title_input_cooldown_frames = 10;
    return g_title_restored_selection;
}

int MikuPan_RmlTitleIsInputCooldownActive(void)
{
    if (g_title_input_cooldown_frames <= 0)
    {
        return 0;
    }

    g_title_input_cooldown_frames--;
    return 1;
}

}
