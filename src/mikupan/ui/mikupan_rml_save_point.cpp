#include "mikupan/ui/mikupan_rml_save_point.h"

#include "enums.h"
#include "mikupan/io/mikupan_file.h"

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


int SeStartFix(int se_no, unsigned short fin_spd, unsigned short vol_max,
               unsigned short pitch, unsigned char menu);

class MikuPanSavePointOptionListener final : public Rml::EventListener
{
public:
    MikuPanSavePointOptionListener(int in_selection, bool in_activate)
        : selection(in_selection), activate(in_activate)
    {
    }

    void ProcessEvent(Rml::Event& event) override;

private:
    int selection = 0;
    bool activate = false;
};

class MikuPanSavePointConfirmListener final : public Rml::EventListener
{
public:
    explicit MikuPanSavePointConfirmListener(int in_selection)
        : selection(in_selection)
    {
    }

    void ProcessEvent(Rml::Event& event) override;

private:
    int selection = 1;
};

struct MikuPanRmlSavePointState
{
    Rml::Context* context = nullptr;
    Rml::ElementDocument* document = nullptr;
    Rml::Element* root = nullptr;
    Rml::Element* menu = nullptr;
    Rml::Element* selection_frame = nullptr;
    Rml::Element* message = nullptr;
    Rml::Element* confirm = nullptr;
    std::array<Rml::Element*, 2> confirm_messages{};
    Rml::Element* confirm_yes = nullptr;
    Rml::Element* confirm_no = nullptr;
    std::array<Rml::Element*, 4> buttons{};
    std::array<Rml::Element*, 4> icons{};
    std::array<Rml::Element*, 4> labels{};
    std::array<Rml::Element*, 4> corners{};
    std::array<Rml::Element*, 4> messages{};
    std::vector<std::unique_ptr<Rml::EventListener>> listeners;
    int availability_mode = 0;
    int mission_mode = 0;
    int selection = 0;
    int confirm_selection = 1;
    int queued_action = MIKUPAN_RML_SAVE_POINT_ACTION_NONE;
    bool initialized = false;
    bool visible = false;
    bool interactive = false;
    bool dialog_open = false;
    bool textures_assigned = false;
    float opacity = 1.0f;
};

static MikuPanRmlSavePointState g_save_point;

static Rml::Element* MikuPan_RmlSavePointGetElement(const char* id)
{
    return g_save_point.document != nullptr
        ? g_save_point.document->GetElementById(id)
        : nullptr;
}

static void MikuPan_RmlSavePointSetHidden(Rml::Element* element, bool hidden)
{
    if (element != nullptr)
    {
        element->SetClass("hidden", hidden);
    }
}

static void MikuPan_RmlSavePointAddListener(
    Rml::Element* element,
    Rml::EventId event_id,
    std::unique_ptr<Rml::EventListener> listener)
{
    if (element == nullptr || listener == nullptr)
    {
        return;
    }

    element->AddEventListener(event_id, listener.get());
    g_save_point.listeners.push_back(std::move(listener));
}

static bool MikuPan_RmlSavePointFilmVisible(void)
{
    return g_save_point.availability_mode != 0;
}

static bool MikuPan_RmlSavePointFilmEnabled(void)
{
    return g_save_point.availability_mode >= 2;
}

static bool MikuPan_RmlSavePointSelectionAvailable(int selection)
{
    if (selection < MIKUPAN_RML_SAVE_POINT_ACTION_SAVE
        || selection > MIKUPAN_RML_SAVE_POINT_ACTION_EXIT)
    {
        return false;
    }

    if (selection == MIKUPAN_RML_SAVE_POINT_ACTION_FILM)
    {
        return MikuPan_RmlSavePointFilmEnabled();
    }

    return true;
}

static int MikuPan_RmlSavePointNormalizeSelection(int selection)
{
    if (MikuPan_RmlSavePointSelectionAvailable(selection))
    {
        return selection;
    }

    return MIKUPAN_RML_SAVE_POINT_ACTION_EXIT;
}

static float MikuPan_RmlSavePointFrameLeft(void)
{
    static constexpr std::array<float, 4> four_option_positions = {
        32.0f,
        176.0f,
        320.0f,
        464.0f
    };
    static constexpr std::array<float, 4> three_option_positions = {
        72.0f,
        248.0f,
        320.0f,
        424.0f
    };

    const int selection = std::clamp(g_save_point.selection, 0, 3);
    return g_save_point.availability_mode == 0
        ? three_option_positions[selection]
        : four_option_positions[selection];
}

static void MikuPan_RmlSavePointSetDpProperty(Rml::Element* element,
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

static void MikuPan_RmlSavePointSetOpacity(Rml::Element* element, float value)
{
    if (element == nullptr)
    {
        return;
    }

    char buffer[32];
    std::snprintf(buffer, sizeof(buffer), "%.3f", std::clamp(value, 0.0f, 1.0f));
    element->SetProperty("opacity", buffer);
}

static void MikuPan_RmlSavePointAssignSprite(Rml::Element* element,
                                              int sprite_index)
{
    if (element == nullptr)
    {
        return;
    }

    char source[64];
    std::snprintf(source, sizeof(source), "mikupan-sprite://%d", sprite_index);
    element->SetAttribute("src", source);
}

static void MikuPan_RmlSavePointAssignTextures(void)
{
    if (g_save_point.textures_assigned)
    {
        return;
    }

    MikuPan_RmlSavePointAssignSprite(g_save_point.icons[0], PSP_ICN_MEMOCA);
    MikuPan_RmlSavePointAssignSprite(g_save_point.icons[1], PSP_ICN_ALBUM);
    MikuPan_RmlSavePointAssignSprite(g_save_point.icons[2], PSP_ICN_FILM);
    MikuPan_RmlSavePointAssignSprite(g_save_point.icons[3], PSP_ICN_RETURN);
    MikuPan_RmlSavePointAssignSprite(g_save_point.corners[0], PSP_KAKKO_LU);
    MikuPan_RmlSavePointAssignSprite(g_save_point.corners[1], PSP_KAKKO_LD);
    MikuPan_RmlSavePointAssignSprite(g_save_point.corners[2], PSP_KAKKO_RU);
    MikuPan_RmlSavePointAssignSprite(g_save_point.corners[3], PSP_KAKKO_RD);
    g_save_point.textures_assigned = true;
}

static void MikuPan_RmlSavePointSyncDialog(void)
{
    MikuPan_RmlSavePointSetHidden(g_save_point.message,
                                  g_save_point.dialog_open);
    MikuPan_RmlSavePointSetHidden(g_save_point.confirm,
                                  !g_save_point.dialog_open);

    MikuPan_RmlSavePointSetHidden(
        g_save_point.confirm_messages[0],
        g_save_point.selection != MIKUPAN_RML_SAVE_POINT_ACTION_FILM);
    MikuPan_RmlSavePointSetHidden(
        g_save_point.confirm_messages[1],
        g_save_point.selection != MIKUPAN_RML_SAVE_POINT_ACTION_EXIT);

    if (g_save_point.confirm_yes != nullptr)
    {
        g_save_point.confirm_yes->SetClass(
            "selected", g_save_point.confirm_selection == 0);
    }

    if (g_save_point.confirm_no != nullptr)
    {
        g_save_point.confirm_no->SetClass(
            "selected", g_save_point.confirm_selection != 0);
    }
}

static void MikuPan_RmlSavePointSyncSelection(void)
{
    g_save_point.selection = MikuPan_RmlSavePointNormalizeSelection(
        g_save_point.selection);

    for (int i = 0; i < 4; i++)
    {
        if (g_save_point.buttons[i] != nullptr)
        {
            g_save_point.buttons[i]->SetClass(
                "selected", i == g_save_point.selection);
            g_save_point.buttons[i]->SetClass(
                "disabled",
                i == MIKUPAN_RML_SAVE_POINT_ACTION_FILM
                    && !MikuPan_RmlSavePointFilmEnabled());
        }
    }

    MikuPan_RmlSavePointSetHidden(
        g_save_point.buttons[MIKUPAN_RML_SAVE_POINT_ACTION_FILM],
        !MikuPan_RmlSavePointFilmVisible());

    if (g_save_point.root != nullptr)
    {
        g_save_point.root->SetClass(
            "three-options", g_save_point.availability_mode == 0);
        g_save_point.root->SetClass("locked", !g_save_point.interactive);
    }

    MikuPan_RmlSavePointSetDpProperty(
        g_save_point.selection_frame,
        "left",
        MikuPan_RmlSavePointFrameLeft());

    for (int i = 0; i < 4; i++)
    {
        MikuPan_RmlSavePointSetHidden(
            g_save_point.messages[i],
            i != g_save_point.selection);
    }

    MikuPan_RmlSavePointSyncDialog();
}

static void MikuPan_RmlSavePointSelect(int selection)
{
    if (!g_save_point.interactive
        || !MikuPan_RmlSavePointSelectionAvailable(selection))
    {
        return;
    }

    g_save_point.selection = selection;
    if (!g_save_point.dialog_open)
    {
        MikuPan_RmlSavePointSyncSelection();
    }
}

static bool MikuPan_RmlSavePointLoadDocument(void)
{
    char document_path[1024];
    if (!MikuPan_ResolveBasePath("resources/rml/save_point.rml",
                                 document_path,
                                 sizeof(document_path)))
    {
        return false;
    }

    g_save_point.document = g_save_point.context->LoadDocument(document_path);
    if (g_save_point.document == nullptr)
    {
        return false;
    }

    g_save_point.root = MikuPan_RmlSavePointGetElement("save-point-root");
    g_save_point.menu = MikuPan_RmlSavePointGetElement("save-point-menu");
    g_save_point.selection_frame =
        MikuPan_RmlSavePointGetElement("save-point-selection-frame");
    g_save_point.message =
        MikuPan_RmlSavePointGetElement("save-point-message");
    g_save_point.confirm =
        MikuPan_RmlSavePointGetElement("save-point-confirm");
    g_save_point.confirm_messages = {
        MikuPan_RmlSavePointGetElement("save-point-confirm-message-film"),
        MikuPan_RmlSavePointGetElement("save-point-confirm-message-exit")
    };
    g_save_point.confirm_yes =
        MikuPan_RmlSavePointGetElement("save-point-confirm-yes");
    g_save_point.confirm_no =
        MikuPan_RmlSavePointGetElement("save-point-confirm-no");

    g_save_point.buttons = {
        MikuPan_RmlSavePointGetElement("save-point-save"),
        MikuPan_RmlSavePointGetElement("save-point-album"),
        MikuPan_RmlSavePointGetElement("save-point-film"),
        MikuPan_RmlSavePointGetElement("save-point-exit")
    };
    g_save_point.icons = {
        MikuPan_RmlSavePointGetElement("save-point-icon-save"),
        MikuPan_RmlSavePointGetElement("save-point-icon-album"),
        MikuPan_RmlSavePointGetElement("save-point-icon-film"),
        MikuPan_RmlSavePointGetElement("save-point-icon-exit")
    };
    g_save_point.labels = {
        MikuPan_RmlSavePointGetElement("save-point-label-save"),
        MikuPan_RmlSavePointGetElement("save-point-label-album"),
        MikuPan_RmlSavePointGetElement("save-point-label-film"),
        MikuPan_RmlSavePointGetElement("save-point-label-exit")
    };
    g_save_point.corners = {
        MikuPan_RmlSavePointGetElement("save-point-corner-lu"),
        MikuPan_RmlSavePointGetElement("save-point-corner-ru"),
        MikuPan_RmlSavePointGetElement("save-point-corner-ld"),
        MikuPan_RmlSavePointGetElement("save-point-corner-rd")
    };
    g_save_point.messages = {
        MikuPan_RmlSavePointGetElement("save-point-message-save"),
        MikuPan_RmlSavePointGetElement("save-point-message-album"),
        MikuPan_RmlSavePointGetElement("save-point-message-film"),
        MikuPan_RmlSavePointGetElement("save-point-message-exit")
    };

    if (g_save_point.root == nullptr || g_save_point.menu == nullptr
        || g_save_point.selection_frame == nullptr
        || g_save_point.message == nullptr || g_save_point.confirm == nullptr
        || g_save_point.confirm_messages[0] == nullptr
        || g_save_point.confirm_messages[1] == nullptr)
    {
        return false;
    }

    for (int i = 0; i < 4; i++)
    {
        MikuPan_RmlSavePointAddListener(
            g_save_point.buttons[i],
            Rml::EventId::Mouseover,
            std::make_unique<MikuPanSavePointOptionListener>(i, false));
        MikuPan_RmlSavePointAddListener(
            g_save_point.buttons[i],
            Rml::EventId::Click,
            std::make_unique<MikuPanSavePointOptionListener>(i, true));
    }

    MikuPan_RmlSavePointAddListener(
        g_save_point.confirm_yes,
        Rml::EventId::Mouseover,
        std::make_unique<MikuPanSavePointConfirmListener>(0));
    MikuPan_RmlSavePointAddListener(
        g_save_point.confirm_yes,
        Rml::EventId::Click,
        std::make_unique<MikuPanSavePointConfirmListener>(0));
    MikuPan_RmlSavePointAddListener(
        g_save_point.confirm_no,
        Rml::EventId::Mouseover,
        std::make_unique<MikuPanSavePointConfirmListener>(1));
    MikuPan_RmlSavePointAddListener(
        g_save_point.confirm_no,
        Rml::EventId::Click,
        std::make_unique<MikuPanSavePointConfirmListener>(1));

    MikuPan_RmlSavePointSetHidden(g_save_point.root, true);
    MikuPan_RmlSavePointSetHidden(g_save_point.confirm, true);
    return true;
}

void MikuPanSavePointOptionListener::ProcessEvent(Rml::Event& event)
{
    if (!g_save_point.interactive || g_save_point.dialog_open)
    {
        return;
    }

    const int previous_selection = g_save_point.selection;
    MikuPan_RmlSavePointSelect(selection);
    if (event.GetId() == Rml::EventId::Mouseover
        && g_save_point.selection != previous_selection)
    {
        SeStartFix(SE_CSR0, 0, 0x1000, 0x1000, 1);
    }

    if (activate && event.GetId() == Rml::EventId::Click
        && g_save_point.selection == selection)
    {
        SeStartFix(SE_CLIC, 0, 0x1000, 0x1000, 1);
        MikuPan_RmlSavePointActivate();
    }
}

void MikuPanSavePointConfirmListener::ProcessEvent(Rml::Event& event)
{
    if (!g_save_point.interactive || !g_save_point.dialog_open)
    {
        return;
    }

    const int previous_selection = g_save_point.confirm_selection;
    g_save_point.confirm_selection = selection;
    MikuPan_RmlSavePointSyncDialog();
    if (event.GetId() == Rml::EventId::Mouseover
        && g_save_point.confirm_selection != previous_selection)
    {
        SeStartFix(SE_CSR0, 0, 0x1000, 0x1000, 1);
    }
    if (event.GetId() == Rml::EventId::Click)
    {
        SeStartFix(SE_CLIC, 0, 0x1000, 0x1000, 1);
        MikuPan_RmlSavePointActivate();
    }
}

bool MikuPan_RmlSavePointInit(Rml::Context* context)
{
    if (g_save_point.initialized)
    {
        return true;
    }

    g_save_point.context = context;
    if (context == nullptr || !MikuPan_RmlSavePointLoadDocument())
    {
        g_save_point = MikuPanRmlSavePointState();
        return false;
    }

    g_save_point.initialized = true;
    return true;
}

extern "C" {

void MikuPan_RmlSavePointStartFrame(void)
{
    if (!g_save_point.initialized || !g_save_point.visible)
    {
        return;
    }

    const float time = static_cast<float>(SDL_GetTicks()) * 0.001f;
    const float phase = std::sin(time * 4.6f);
    const float frame_pulse = 0.78f + phase * 0.14f;
    const float icon_pulse = 0.78f + phase * 0.10f;
    MikuPan_RmlSavePointSetOpacity(
        g_save_point.selection_frame,
        frame_pulse);

    for (int i = 0; i < 4; i++)
    {
        float opacity = 0.58f;
        if (i == MIKUPAN_RML_SAVE_POINT_ACTION_FILM
            && !MikuPan_RmlSavePointFilmEnabled())
        {
            opacity = 0.42f;
        }
        else if (i == g_save_point.selection)
        {
            opacity = icon_pulse;
        }
        MikuPan_RmlSavePointSetOpacity(g_save_point.icons[i], opacity);
    }
}

void MikuPan_RmlSavePointPrepareShutdown(void)
{
    MikuPan_RmlSavePointClose();
    if (g_save_point.document != nullptr)
    {
        g_save_point.document->Close();
        g_save_point.document = nullptr;
    }
}

void MikuPan_RmlSavePointShutdown(void)
{
    g_save_point = MikuPanRmlSavePointState();
}

int MikuPan_RmlSavePointOpen(int availability_mode,
                             int mission_mode,
                             int selection)
{
    if (!g_save_point.initialized || g_save_point.document == nullptr
        || g_save_point.root == nullptr)
    {
        return 0;
    }

    g_save_point.availability_mode = std::clamp(availability_mode, 0, 2);
    g_save_point.mission_mode = mission_mode;
    g_save_point.selection = MikuPan_RmlSavePointNormalizeSelection(selection);
    g_save_point.confirm_selection = 1;
    g_save_point.queued_action = MIKUPAN_RML_SAVE_POINT_ACTION_NONE;
    g_save_point.dialog_open = false;
    g_save_point.interactive = false;
    g_save_point.opacity = 0.0f;

    MikuPan_RmlSavePointAssignTextures();
    MikuPan_RmlSavePointSetHidden(g_save_point.root, false);
    if (!g_save_point.visible)
    {
        g_save_point.document->Show(Rml::ModalFlag::None, Rml::FocusFlag::None);
        g_save_point.visible = true;
    }

    MikuPan_RmlSavePointSyncSelection();
    return 1;
}

void MikuPan_RmlSavePointClose(void)
{
    if (!g_save_point.visible)
    {
        return;
    }

    g_save_point.visible = false;
    g_save_point.interactive = false;
    g_save_point.dialog_open = false;
    g_save_point.queued_action = MIKUPAN_RML_SAVE_POINT_ACTION_NONE;
    MikuPan_RmlSavePointSetHidden(g_save_point.root, true);
    if (g_save_point.document != nullptr)
    {
        g_save_point.document->Hide();
    }
}

int MikuPan_RmlSavePointIsOpen(void)
{
    return g_save_point.visible ? 1 : 0;
}

int MikuPan_RmlSavePointInputEnabled(void)
{
    return g_save_point.visible && g_save_point.interactive ? 1 : 0;
}

void MikuPan_RmlSavePointSetPresentation(int availability_mode,
                                         int mission_mode,
                                         int interactive,
                                         float opacity)
{
    if (!g_save_point.visible)
    {
        return;
    }

    g_save_point.availability_mode = std::clamp(availability_mode, 0, 2);
    g_save_point.mission_mode = mission_mode;
    g_save_point.interactive = interactive != 0;
    g_save_point.opacity = std::clamp(opacity, 0.0f, 1.0f);
    MikuPan_RmlSavePointSetHidden(g_save_point.root,
                                  g_save_point.opacity <= 0.001f);
    MikuPan_RmlSavePointSetOpacity(g_save_point.root, g_save_point.opacity);
    MikuPan_RmlSavePointSyncSelection();
}

void MikuPan_RmlSavePointHandleHorizontalInput(int direction)
{
    if (!g_save_point.visible || !g_save_point.interactive || direction == 0)
    {
        return;
    }

    if (g_save_point.dialog_open)
    {
        g_save_point.confirm_selection = direction < 0 ? 0 : 1;
        MikuPan_RmlSavePointSyncDialog();
        return;
    }

    static constexpr std::array<int, 4> all_options = {0, 1, 2, 3};
    static constexpr std::array<int, 3> reduced_options = {0, 1, 3};

    if (MikuPan_RmlSavePointFilmEnabled())
    {
        auto it = std::find(all_options.begin(), all_options.end(),
                            g_save_point.selection);
        int index = it != all_options.end()
            ? static_cast<int>(std::distance(all_options.begin(), it))
            : 0;
        index = (index + (direction > 0 ? 1 : 3)) % 4;
        g_save_point.selection = all_options[index];
    }
    else
    {
        auto it = std::find(reduced_options.begin(), reduced_options.end(),
                            g_save_point.selection);
        int index = it != reduced_options.end()
            ? static_cast<int>(std::distance(reduced_options.begin(), it))
            : 0;
        index = (index + (direction > 0 ? 1 : 2)) % 3;
        g_save_point.selection = reduced_options[index];
    }

    MikuPan_RmlSavePointSyncSelection();
}

void MikuPan_RmlSavePointHandleVerticalInput(int direction)
{
    MikuPan_RmlSavePointHandleHorizontalInput(direction);
}

void MikuPan_RmlSavePointActivate(void)
{
    if (!g_save_point.visible || !g_save_point.interactive)
    {
        return;
    }

    if (!g_save_point.dialog_open)
    {
        if (!MikuPan_RmlSavePointSelectionAvailable(g_save_point.selection))
        {
            return;
        }

        if (g_save_point.selection == MIKUPAN_RML_SAVE_POINT_ACTION_SAVE
            || g_save_point.selection == MIKUPAN_RML_SAVE_POINT_ACTION_ALBUM)
        {
            g_save_point.queued_action = g_save_point.selection;
            return;
        }

        g_save_point.dialog_open = true;
        g_save_point.confirm_selection = 1;
        MikuPan_RmlSavePointSyncDialog();
        return;
    }

    if (g_save_point.confirm_selection == 0)
    {
        g_save_point.queued_action = g_save_point.selection;
    }

    g_save_point.dialog_open = false;
    MikuPan_RmlSavePointSyncSelection();
}

void MikuPan_RmlSavePointHandleCancel(void)
{
    if (!g_save_point.visible || !g_save_point.interactive)
    {
        return;
    }

    if (g_save_point.dialog_open)
    {
        g_save_point.dialog_open = false;
        g_save_point.confirm_selection = 1;
        MikuPan_RmlSavePointSyncSelection();
        return;
    }

    g_save_point.selection = MIKUPAN_RML_SAVE_POINT_ACTION_EXIT;
    g_save_point.dialog_open = true;
    g_save_point.confirm_selection = 1;
    MikuPan_RmlSavePointSyncSelection();
}

int MikuPan_RmlSavePointConsumeAction(void)
{
    const int action = g_save_point.queued_action;
    g_save_point.queued_action = MIKUPAN_RML_SAVE_POINT_ACTION_NONE;
    return action;
}

int MikuPan_RmlSavePointGetSelection(void)
{
    return g_save_point.selection;
}

}
