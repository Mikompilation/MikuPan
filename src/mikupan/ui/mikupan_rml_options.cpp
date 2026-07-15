#include "mikupan/ui/mikupan_rml_options.h"

#include "mikupan/ui/mikupan_ui.h"
#include "mikupan/mikupan_config.h"
#include "mikupan/io/mikupan_file.h"
#include "mikupan/io/mikupan_controller.h"
#include "mikupan/gameplay/mikupan_item_icon_hud.h"
#include "os/key_cnf.h"

#include "RmlUi/Core.h"
#include "RmlUi/Core/ElementDocument.h"
#include "RmlUi/Core/EventListener.h"
#include "RmlUi/Core/Elements/ElementFormControlInput.h"
#include "RmlUi/Core/Elements/ElementFormControlSelect.h"
#include "SDL3/SDL_gamepad.h"
#include "SDL3/SDL_keyboard.h"
#include "SDL3/SDL_scancode.h"
#include "SDL3/SDL_timer.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace
{
bool ElementUsesPageScroll(Rml::Element* element);
float GetPageAreaScrollTop(void);
void RestorePageAreaScrollTop(float scroll_top);

class MikuPanSelectListener final : public Rml::EventListener
{
public:
    explicit MikuPanSelectListener(std::function<void(int)> callback)
        : on_change(std::move(callback))
    {
    }

    void ProcessEvent(Rml::Event& event) override
    {
        auto* select = rmlui_dynamic_cast<Rml::ElementFormControlSelect*>(
            event.GetCurrentElement());
        if (select == nullptr)
        {
            select = rmlui_dynamic_cast<Rml::ElementFormControlSelect*>(
                event.GetTargetElement());
        }
        if (select == nullptr)
        {
            return;
        }

        const bool preserve_scroll = ElementUsesPageScroll(select);
        const float scroll_top = preserve_scroll ? GetPageAreaScrollTop() : 0.0f;
        on_change(select->GetSelection());
        if (preserve_scroll)
        {
            RestorePageAreaScrollTop(scroll_top);
        }
    }

private:
    std::function<void(int)> on_change;
};

class MikuPanInputListener final : public Rml::EventListener
{
public:
    explicit MikuPanInputListener(
        std::function<void(Rml::ElementFormControlInput*)> callback)
        : on_change(std::move(callback))
    {
    }

    void ProcessEvent(Rml::Event& event) override
    {
        auto* input = rmlui_dynamic_cast<Rml::ElementFormControlInput*>(
            event.GetCurrentElement());
        if (input == nullptr)
        {
            input = rmlui_dynamic_cast<Rml::ElementFormControlInput*>(
                event.GetTargetElement());
        }
        if (input == nullptr)
        {
            return;
        }

        const bool preserve_scroll = ElementUsesPageScroll(input);
        const float scroll_top = preserve_scroll ? GetPageAreaScrollTop() : 0.0f;
        on_change(input);
        if (preserve_scroll)
        {
            RestorePageAreaScrollTop(scroll_top);
        }
    }

private:
    std::function<void(Rml::ElementFormControlInput*)> on_change;
};

class MikuPanButtonListener final : public Rml::EventListener
{
public:
    explicit MikuPanButtonListener(std::function<void()> callback)
        : on_click(std::move(callback))
    {
    }

    void ProcessEvent(Rml::Event& event) override
    {
        Rml::Element* element = event.GetCurrentElement();
        const bool preserve_scroll = ElementUsesPageScroll(element);
        const float scroll_top = preserve_scroll ? GetPageAreaScrollTop() : 0.0f;
        on_click();
        if (preserve_scroll)
        {
            RestorePageAreaScrollTop(scroll_top);
        }
    }

private:
    std::function<void()> on_click;
};

class MikuPanEventListener final : public Rml::EventListener
{
public:
    explicit MikuPanEventListener(std::function<void(Rml::Event&)> callback)
        : on_event(std::move(callback))
    {
    }

    void ProcessEvent(Rml::Event& event) override
    {
        on_event(event);
    }

private:
    std::function<void(Rml::Event&)> on_event;
};

enum MikuPanStepControlKind
{
    MIKUPAN_STEP_BRIGHTNESS = 0,
    MIKUPAN_STEP_GAMMA,
    MIKUPAN_STEP_CONTRAST,
    MIKUPAN_STEP_SHADOW_DEPTH,
    MIKUPAN_STEP_HDR_PAPER_WHITE,
    MIKUPAN_STEP_HDR_PEAK_LUMINANCE,
    MIKUPAN_STEP_FONT_SCALE,
    MIKUPAN_STEP_CRT_FIELD,
    MIKUPAN_STEP_AUDIO_MASTER,
};

struct MikuPanStepControl
{
    MikuPanStepControlKind kind = MIKUPAN_STEP_BRIGHTNESS;
    int index = -1;
    Rml::Element* control = nullptr;
    Rml::Element* bars = nullptr;
    Rml::Element* value = nullptr;
    float min_value = 0.0f;
    float max_value = 1.0f;
    float step = 0.1f;
    int precision = 2;
    int segments = 12;
    float visual_min_value = 0.0f;
    float visual_max_value = 1.0f;
    float neutral_value = 1.0f;
    bool has_neutral = false;
    bool suffix_x = false;
    bool suffix_percent = false;
};

enum MikuPanChoicePickerKind
{
    MIKUPAN_PICKER_NONE = 0,
    MIKUPAN_PICKER_WINDOW_MODE,
    MIKUPAN_PICKER_WINDOW_SIZE,
    MIKUPAN_PICKER_RESOLUTION,
    MIKUPAN_PICKER_GPU_BACKEND,
    MIKUPAN_PICKER_MSAA,
    MIKUPAN_PICKER_SHADOW_RESOLUTION,
    MIKUPAN_PICKER_LIGHTING_MODE,
    MIKUPAN_PICKER_DITHER_MODE,
    MIKUPAN_PICKER_FINDER_SURROUND,
    MIKUPAN_PICKER_FLASHLIGHT_STYLE,
    MIKUPAN_PICKER_THEME,
    MIKUPAN_PICKER_FONT,
};

enum MikuPanDisplayConfirmKind
{
    MIKUPAN_DISPLAY_CONFIRM_NONE = 0,
    MIKUPAN_DISPLAY_CONFIRM_WINDOW_MODE,
    MIKUPAN_DISPLAY_CONFIRM_WINDOW_SIZE,
    MIKUPAN_DISPLAY_CONFIRM_RESOLUTION,
};

enum MikuPanControlScope
{
    MIKUPAN_CONTROL_SCOPE_SIDEBAR = 0,
    MIKUPAN_CONTROL_SCOPE_CONTENT,
};

enum MikuPanControlsTab
{
    MIKUPAN_CONTROLS_TAB_KEYBOARD = 0,
    MIKUPAN_CONTROLS_TAB_CONTROLLER,
};

enum MikuPanBindingCaptureKind
{
    MIKUPAN_BIND_CAPTURE_NONE = 0,
    MIKUPAN_BIND_CAPTURE_SPECIAL_ACTION,
    MIKUPAN_BIND_CAPTURE_KEYBOARD_BUTTON,
    MIKUPAN_BIND_CAPTURE_KEYBOARD_STICK_NEG,
    MIKUPAN_BIND_CAPTURE_KEYBOARD_STICK_POS,
    MIKUPAN_BIND_CAPTURE_CONTROLLER_BUTTON,
    MIKUPAN_BIND_CAPTURE_CONTROLLER_STICK_AXIS,
};

struct MikuPanRmlOptionsState
{
    Rml::Context* context = nullptr;
    std::vector<std::unique_ptr<Rml::EventListener>> listeners;
    Rml::ElementDocument* document = nullptr;
    Rml::Element* root = nullptr;
    Rml::Element* window = nullptr;
    Rml::Element* page_area = nullptr;
    Rml::Element* film_title = nullptr;
    Rml::Element* window_mode_picker = nullptr;
    Rml::Element* window_mode_choice = nullptr;
    Rml::Element* window_mode_value = nullptr;
    Rml::Element* window_mode_pending_note = nullptr;
    Rml::Element* window_size_picker = nullptr;
    Rml::Element* window_size_choice = nullptr;
    Rml::Element* window_size_value = nullptr;
    Rml::Element* window_size_pending_note = nullptr;
    Rml::Element* resolution_picker = nullptr;
    Rml::Element* resolution_choice = nullptr;
    Rml::Element* resolution_value = nullptr;
    Rml::Element* resolution_pending_note = nullptr;
    Rml::Element* gpu_backend_picker = nullptr;
    Rml::Element* gpu_backend_choice = nullptr;
    Rml::Element* msaa_picker = nullptr;
    Rml::Element* msaa_choice = nullptr;
    Rml::Element* shadow_resolution_picker = nullptr;
    Rml::Element* shadow_resolution_choice = nullptr;
    Rml::Element* lighting_mode_picker = nullptr;
    Rml::Element* lighting_mode_choice = nullptr;
    Rml::Element* dither_mode_picker = nullptr;
    Rml::Element* dither_mode_choice = nullptr;
    Rml::Element* finder_surround_picker = nullptr;
    Rml::Element* finder_surround_choice = nullptr;
    Rml::Element* flashlight_style_picker = nullptr;
    Rml::Element* flashlight_style_choice = nullptr;
    Rml::Element* theme_picker = nullptr;
    Rml::Element* theme_choice = nullptr;
    Rml::Element* font_picker = nullptr;
    Rml::Element* font_choice = nullptr;
    Rml::Element* choice_picker_modal = nullptr;
    Rml::Element* choice_picker_title = nullptr;
    Rml::Element* choice_picker_list = nullptr;
    Rml::Element* resolution_confirm_modal = nullptr;
    Rml::Element* resolution_confirm_title = nullptr;
    Rml::Element* resolution_confirm_countdown = nullptr;
    Rml::ElementFormControlSelect* gpu_backend_select = nullptr;
    Rml::ElementFormControlSelect* msaa_select = nullptr;
    Rml::ElementFormControlSelect* shadow_resolution_select = nullptr;
    Rml::ElementFormControlSelect* lighting_mode_select = nullptr;
    Rml::ElementFormControlSelect* dither_mode_select = nullptr;
    Rml::ElementFormControlSelect* finder_surround_select = nullptr;
    Rml::ElementFormControlSelect* theme_select = nullptr;
    Rml::ElementFormControlSelect* font_select = nullptr;
    Rml::ElementFormControlInput* vsync_input = nullptr;
    Rml::ElementFormControlInput* hdr_enabled_input = nullptr;
    Rml::ElementFormControlInput* calibration_hdr_enabled_input = nullptr;
    Rml::Element* active_gpu_label = nullptr;
    Rml::Element* gpu_restart_note = nullptr;
    Rml::ElementFormControlInput* crt_enabled_input = nullptr;
    Rml::ElementFormControlInput* minimap_enabled_input = nullptr;
    Rml::ElementFormControlInput* keep_finder_raised_input = nullptr;
    Rml::ElementFormControlInput* finder_dpad_film_swap_input = nullptr;
    Rml::ElementFormControlInput* mirror_stone_hud_input = nullptr;
    Rml::ElementFormControlInput* improved_movement_collisions_input = nullptr;
    Rml::ElementFormControlInput* controller_remap_input = nullptr;
    Rml::ElementFormControlInput* data_folder_input = nullptr;
    Rml::Element* save_status = nullptr;
    Rml::Element* panel_title = nullptr;
    Rml::Element* confirm_save_modal = nullptr;
    Rml::Element* calibration_panel = nullptr;
    Rml::Element* calibration_level_panel = nullptr;
    Rml::Element* crt_panel = nullptr;
    Rml::Element* controls_keyboard_tab = nullptr;
    Rml::Element* controls_controller_tab = nullptr;
    Rml::Element* controls_list = nullptr;
    Rml::Element* binding_capture_modal = nullptr;
    Rml::Element* binding_capture_title = nullptr;
    Rml::Element* binding_capture_text = nullptr;
    Rml::Element* binding_capture_countdown = nullptr;
    std::array<Rml::Element*, 16> calibration_level_swatches{};
    std::vector<MikuPanStepControl> step_controls;
    std::array<Rml::Element*, 5> category_buttons{};
    std::array<Rml::Element*, 5> category_pages{};
    int selected_category = 0;
    MikuPanControlScope control_scope = MIKUPAN_CONTROL_SCOPE_SIDEBAR;
    MikuPanChoicePickerKind active_picker = MIKUPAN_PICKER_NONE;
    MikuPanDisplayConfirmKind display_confirm_kind = MIKUPAN_DISPLAY_CONFIRM_NONE;
    int pending_window_mode = 0;
    int original_window_mode = 0;
    int pending_window_size_index = 0;
    int original_window_size_index = 0;
    int pending_resolution_index = 0;
    int original_resolution_index = 0;
    int pending_picker_index = 0;
    bool window_mode_pending_dirty = false;
    bool window_size_pending_dirty = false;
    bool resolution_pending_dirty = false;
    uint64_t input_block_until_ticks = 0;
    bool resolution_confirm_visible = false;
    bool display_dirty_before_confirm = false;
    uint64_t resolution_confirm_deadline_ticks = 0;
    bool initialized = false;
    bool visible = false;
    bool option_mode_open = false;
    bool option_exit_requested = false;
    bool has_unsaved_changes = false;
    bool exit_confirm_visible = false;
    bool calibration_visible = false;
    float calibration_original_brightness = 1.0f;
    float calibration_original_gamma = 1.0f;
    float calibration_original_contrast = 1.0f;
    float calibration_original_shadow_depth = 1.0f;
    int calibration_original_hdr_enabled = 0;
    float calibration_original_hdr_paper_white = 203.0f;
    float calibration_original_hdr_peak_luminance = 1000.0f;
    bool calibration_original_dirty = false;
    bool crt_visible = false;
    MikuPan_ConfigCrt crt_original = {};
    bool crt_original_dirty = false;
    MikuPanControlsTab controls_tab = MIKUPAN_CONTROLS_TAB_KEYBOARD;
    MikuPanBindingCaptureKind binding_capture_kind = MIKUPAN_BIND_CAPTURE_NONE;
    int binding_capture_target = -1;
    uint64_t binding_capture_after_ticks = 0;
    uint64_t binding_capture_deadline_ticks = 0;
    bool binding_capture_waiting_for_release = false;
    std::string binding_capture_restore_focus_id;
    std::string queued_focus_id;
    int queued_focus_attempts = 0;
    bool syncing = false;
    bool ui_move_sound_requested = false;
    Rml::Element* controller_focus_element = nullptr;
    Rml::Element* controller_focus_row = nullptr;
    Rml::ElementFormControlSelect* controller_open_select = nullptr;
};
MikuPanRmlOptionsState g_rml;

std::string FormatFloat(float value, int precision)
{
    char buffer[32];
    std::snprintf(buffer, sizeof(buffer), "%.*f", precision, value);
    return buffer;
}

float ParseFloat(const Rml::String& value, float fallback)
{
    char* end = nullptr;
    const float parsed = std::strtof(value.c_str(), &end);
    return end != value.c_str() ? parsed : fallback;
}

void SetElementText(Rml::Element* element, const std::string& text)
{
    if (element != nullptr)
    {
        element->SetInnerRML(text);
    }
}

void SetInputValue(Rml::ElementFormControlInput* input,
                   float value,
                   int precision)
{
    if (input != nullptr)
    {
        input->SetValue(FormatFloat(value, precision));
    }
}

void SetCheckbox(Rml::ElementFormControlInput* input, int enabled)
{
    if (input == nullptr)
    {
        return;
    }

    const bool should_check = enabled != 0;
    if (input->HasAttribute("checked") == should_check)
    {
        return;
    }

    if (should_check)
    {
        input->SetAttribute("checked", "");
    }
    else
    {
        input->RemoveAttribute("checked");
    }
}

int IsCheckboxChecked(Rml::ElementFormControlInput* input)
{
    return input != nullptr && input->HasAttribute("checked");
}

void SyncHdrCheckboxes(void)
{
    SetCheckbox(g_rml.hdr_enabled_input, MikuPan_IsHdrEnabled());
    SetCheckbox(g_rml.calibration_hdr_enabled_input, MikuPan_IsHdrEnabled());
}

void AddListener(Rml::Element* element,
                 Rml::EventId event_id,
                 std::unique_ptr<Rml::EventListener> listener,
                 bool capture = false)
{
    if (element == nullptr || listener == nullptr)
    {
        return;
    }

    element->AddEventListener(event_id, listener.get(), capture);
    g_rml.listeners.push_back(std::move(listener));
}

Rml::ElementFormControlSelect* GetSelect(const char* id)
{
    return rmlui_dynamic_cast<Rml::ElementFormControlSelect*>(
        g_rml.document->GetElementById(id));
}

Rml::ElementFormControlInput* GetInput(const char* id)
{
    return rmlui_dynamic_cast<Rml::ElementFormControlInput*>(
        g_rml.document->GetElementById(id));
}

Rml::Element* GetElement(const char* id)
{
    return g_rml.document->GetElementById(id);
}

static constexpr const char* kCategoryTitles[] = {
    "DISPLAY",
    "GRAPHICS",
    "AUDIO",
    "CONTROLS",
    "ADVANCED",
};

static constexpr const char* kCategoryButtonIds[] = {
    "cat-display",
    "cat-graphics",
    "cat-audio",
    "cat-controls",
    "cat-advanced",
};

static constexpr const char* kCategoryPageIds[] = {
    "page-display",
    "page-graphics",
    "page-audio",
    "page-controls",
    "page-advanced",
};

static constexpr const char* kCategoryFirstControlIds[] = {
    "window-mode-picker",
    "resolution-picker",
    "master-volume-stepper",
    "movement-dpad-mode",
    "theme-picker",
};

static constexpr const char* kWindowModeLabels[] = {
    "Windowed",
    "Fullscreen",
};

static constexpr int kWindowModeValues[] = {
    MIKUPAN_WINDOW_WINDOWED,
    MIKUPAN_WINDOW_BORDERLESS,
};

static constexpr int kWindowModeCount =
    static_cast<int>(sizeof(kWindowModeLabels) / sizeof(kWindowModeLabels[0]));

static constexpr const char* kLightingModeLabels[] = {
    "Pixel (Modern)",
    "Vertex (PS2)",
};

static constexpr int kLightingModeCount =
    static_cast<int>(sizeof(kLightingModeLabels) / sizeof(kLightingModeLabels[0]));

static constexpr const char* kFinderSurroundLabels[] = {
    "Black",
    "Blur",
};

static constexpr int kFinderSurroundCount =
    static_cast<int>(sizeof(kFinderSurroundLabels) / sizeof(kFinderSurroundLabels[0]));

static constexpr const char* kRmlThemeClasses[] = {
    "theme-moonlit",
    "theme-ghost-cyan",
    "theme-crimson",
    "theme-ff1-ritual",
    "theme-mist-teal",
    "theme-sepia",
    "theme-mikupan-classic",
};

static constexpr const char* kRmlFontClasses[] = {
    "rml-font-imgui-default",
    "rml-font-century",
    "rml-font-zapf",
};

int ClampRmlThemeIndex(int theme)
{
    const int count =
        static_cast<int>(sizeof(kRmlThemeClasses) / sizeof(kRmlThemeClasses[0]));
    return theme >= 0 && theme < count ? theme : 0;
}

int ClampRmlFontIndex(int font)
{
    const int count =
        static_cast<int>(sizeof(kRmlFontClasses) / sizeof(kRmlFontClasses[0]));
    return font >= 0 && font < count ? font : 1;
}

void ScrollFocusedContentRowIntoView(void);
void ScrollChoicePickerSelectionIntoView(void);
bool FocusElementById(const char* id);
bool FocusElementByIdPreservePageScroll(const char* id);
void QueueFocusElementById(const char* id);
void ApplyQueuedFocus(void);
const Rml::String GetFocusedElementId(void);
void FocusButtonGroup(const char* const* ids, int count, int direction);
bool HandleContentVerticalInput(int direction);
bool HandleCalibrationVerticalInput(int direction);
bool HandleCrtVerticalInput(int direction);
bool HandleControlsTabHorizontalInput(int direction);
bool HandleControlsVerticalInput(int direction);
bool HandleControlsBindingHorizontalInput(int direction);
void EnterSidebarScope(void);
void EnterContentScope(void);
bool AnySelectOpen(void);
bool FocusedControlWantsSpaceConfirm(void);
bool ActivateFocusedControl(void);
bool HandleHorizontalInput(int direction);
void HandleOptionsCancel(void);
void RequestOptionsExit(void);
void UpdateDisplayPickers(void);
void UpdateMsaaSelectState(void);
void UpdateStepControlVisuals(void);
void SyncRmlSettingsValues(void);
void SetChoicePickerVisible(bool visible);
void SetResolutionConfirmVisible(bool visible, MikuPanDisplayConfirmKind kind);
void UpdateResolutionConfirmTimeout(void);
bool IsOptionsInputBlocked(void);
void OpenChoicePicker(MikuPanChoicePickerKind kind);
void CloseChoicePicker(void);
void ApplyChoicePicker(void);
void SaveAndReturnOptions(void);
void SelectCategory(int category);
void RequestUiMoveSound(void);
void UpdateControllerFocusVisual(void);
void ClearControllerFocusVisual(void);
void ClearControllerFocusReferences(void);
std::string EscapeRmlText(const char* text);
void ApplyRmlThemeClass(int theme);
void ApplyRmlFontClass(int font);

void SetCalibrationVisible(bool visible);
void OpenCalibrationPanel(void);
void AcceptCalibrationPanel(void);
void CancelCalibrationPanel(void);
void ResetCalibrationDefaults(void);
bool CalibrationPanelIsFocused(void);
void SetCrtPanelVisible(bool visible);
void OpenCrtPanel(void);
void AcceptCrtPanel(void);
void CancelCrtPanel(void);
void ResetCrtDefaults(void);
bool CrtPanelIsFocused(void);
void SetBindingCaptureVisible(bool visible);
void BeginBindingCapture(MikuPanBindingCaptureKind kind, int target);
void CancelBindingCapture(void);
void UpdateBindingCapture(void);
void SelectControlsTab(MikuPanControlsTab tab);
void RebuildControlsList(void);
bool BindingCaptureIsVisible(void);

void MarkSettingsDirty(void)
{
    if (!g_rml.syncing)
    {
        g_rml.has_unsaved_changes = true;
    }
}

void RequestUiMoveSound(void)
{
    g_rml.ui_move_sound_requested = true;
}

void ApplyRmlThemeClass(int theme)
{
    if (g_rml.root == nullptr)
    {
        return;
    }

    theme = ClampRmlThemeIndex(theme);
    for (int i = 0;
         i < static_cast<int>(sizeof(kRmlThemeClasses) / sizeof(kRmlThemeClasses[0]));
         i++)
    {
        g_rml.root->SetClass(kRmlThemeClasses[i], i == theme);
    }

    if (g_rml.document != nullptr)
    {
        g_rml.document->UpdateDocument();
    }
}

void ApplyRmlFontClass(int font)
{
    if (g_rml.root == nullptr)
    {
        return;
    }

    font = ClampRmlFontIndex(font);
    for (int i = 0;
         i < static_cast<int>(sizeof(kRmlFontClasses) / sizeof(kRmlFontClasses[0]));
         i++)
    {
        g_rml.root->SetClass(kRmlFontClasses[i], i == font);
    }

    if (g_rml.document != nullptr)
    {
        g_rml.document->UpdateDocument();
    }
}

bool IsOptionsInputBlocked(void)
{
    return g_rml.option_mode_open
           && SDL_GetTicks() < g_rml.input_block_until_ticks;
}

void BlockOptionsInputBriefly(void)
{
    const uint64_t block_until = SDL_GetTicks() + 250u;
    if (g_rml.input_block_until_ticks < block_until)
    {
        g_rml.input_block_until_ticks = block_until;
    }
}

void SetExitConfirmVisible(bool visible)
{
    g_rml.exit_confirm_visible = visible;
    if (g_rml.confirm_save_modal != nullptr)
    {
        g_rml.confirm_save_modal->SetClass("hidden", !visible);
    }

    if (g_rml.visible)
    {
        FocusElementById(visible ? "save-changes-yes-button"
                                 : "cancel-options-button");
    }
}

bool SaveOptionsChanges(void)
{
    const int saved = MikuPan_SaveConfigurationFromUi();
    if (saved)
    {
        g_rml.has_unsaved_changes = false;
    }
    return saved != 0;
}

void SaveAndReturnOptions(void)
{
    if (SaveOptionsChanges())
    {
        SetExitConfirmVisible(false);
        g_rml.option_exit_requested = true;
    }
}

void UpdateMainWindowModalHidden(void)
{
    if (g_rml.window != nullptr)
    {
        g_rml.window->SetClass("calibration-hidden",
                               g_rml.calibration_visible || g_rml.crt_visible);
    }
}

void SetCalibrationVisible(bool visible)
{
    g_rml.calibration_visible = visible;
    if (g_rml.calibration_panel != nullptr)
    {
        g_rml.calibration_panel->SetClass("hidden", !visible);
    }
    if (g_rml.calibration_level_panel != nullptr)
    {
        g_rml.calibration_level_panel->SetClass("hidden", !visible);
    }

    UpdateMainWindowModalHidden();
}

void SetCrtPanelVisible(bool visible)
{
    g_rml.crt_visible = visible;
    if (g_rml.crt_panel != nullptr)
    {
        g_rml.crt_panel->SetClass("hidden", !visible);
    }

    UpdateMainWindowModalHidden();
}

void OpenCalibrationPanel(void)
{
    if (!g_rml.option_mode_open)
    {
        return;
    }

    g_rml.calibration_original_brightness = MikuPan_GetBrightness();
    g_rml.calibration_original_gamma = MikuPan_GetGamma();
    g_rml.calibration_original_contrast = MikuPan_GetContrast();
    g_rml.calibration_original_shadow_depth = MikuPan_GetShadowDepth();
    g_rml.calibration_original_hdr_enabled = MikuPan_IsHdrEnabled();
    g_rml.calibration_original_hdr_paper_white = MikuPan_GetHdrPaperWhite();
    g_rml.calibration_original_hdr_peak_luminance =
        MikuPan_GetHdrPeakLuminance();
    g_rml.calibration_original_dirty = g_rml.has_unsaved_changes;
    SetExitConfirmVisible(false);
    SetChoicePickerVisible(false);
    g_rml.active_picker = MIKUPAN_PICKER_NONE;
    SetResolutionConfirmVisible(false, MIKUPAN_DISPLAY_CONFIRM_NONE);
    SetCrtPanelVisible(false);
    SetCalibrationVisible(true);
    UpdateStepControlVisuals();
    SyncHdrCheckboxes();
    FocusElementById("calibration-brightness-stepper");
}

void AcceptCalibrationPanel(void)
{
    if (!g_rml.calibration_visible)
    {
        return;
    }

    MarkSettingsDirty();
    SetCalibrationVisible(false);
    SyncRmlSettingsValues();
    FocusElementById("open-calibration-button");
}

void CancelCalibrationPanel(void)
{
    if (!g_rml.calibration_visible)
    {
        return;
    }

    MikuPan_SetBrightness(g_rml.calibration_original_brightness);
    MikuPan_SetGamma(g_rml.calibration_original_gamma);
    MikuPan_SetContrast(g_rml.calibration_original_contrast);
    MikuPan_SetShadowDepth(g_rml.calibration_original_shadow_depth);
    MikuPan_SetHdrEnabled(g_rml.calibration_original_hdr_enabled);
    MikuPan_SetHdrPaperWhite(g_rml.calibration_original_hdr_paper_white);
    MikuPan_SetHdrPeakLuminance(
        g_rml.calibration_original_hdr_peak_luminance);
    g_rml.has_unsaved_changes = g_rml.calibration_original_dirty;
    SetCalibrationVisible(false);
    SyncRmlSettingsValues();
    FocusElementById("open-calibration-button");
}

void ResetCalibrationDefaults(void)
{
    MikuPan_SetBrightness(1.0f);
    MikuPan_SetGamma(1.0f);
    MikuPan_SetContrast(1.0f);
    MikuPan_SetShadowDepth(1.0f);
    MikuPan_SetHdrEnabled(0);
    MikuPan_SetHdrPaperWhite(203.0f);
    MikuPan_SetHdrPeakLuminance(1000.0f);
    MarkSettingsDirty();
    UpdateStepControlVisuals();
    SyncHdrCheckboxes();
    FocusElementById("calibration-reset-button");
}

bool CalibrationPanelIsFocused(void)
{
    if (!g_rml.calibration_visible || g_rml.context == nullptr)
    {
        return false;
    }

    Rml::Element* focus = g_rml.context->GetFocusElement();
    while (focus != nullptr)
    {
        if (focus == g_rml.calibration_panel)
        {
            return true;
        }
        focus = focus->GetParentNode();
    }

    return false;
}

void OpenCrtPanel(void)
{
    if (!g_rml.option_mode_open)
    {
        return;
    }

    const MikuPan_ConfigCrt* crt = MikuPan_GetCrtSettings();
    g_rml.crt_original = crt != nullptr ? *crt : MikuPan_ConfigCrt();
    g_rml.crt_original_dirty = g_rml.has_unsaved_changes;
    SetExitConfirmVisible(false);
    SetChoicePickerVisible(false);
    g_rml.active_picker = MIKUPAN_PICKER_NONE;
    SetResolutionConfirmVisible(false, MIKUPAN_DISPLAY_CONFIRM_NONE);
    SetCalibrationVisible(false);
    SetCrtPanelVisible(true);
    UpdateStepControlVisuals();
    SetCheckbox(g_rml.crt_enabled_input, crt != nullptr && crt->enabled);
    FocusElementById("crt-enabled-input");
}

void AcceptCrtPanel(void)
{
    if (!g_rml.crt_visible)
    {
        return;
    }

    MarkSettingsDirty();
    SetCrtPanelVisible(false);
    SyncRmlSettingsValues();
    FocusElementById("open-crt-button");
}

void CancelCrtPanel(void)
{
    if (!g_rml.crt_visible)
    {
        return;
    }

    MikuPan_SetCrtSettings(&g_rml.crt_original);
    g_rml.has_unsaved_changes = g_rml.crt_original_dirty;
    SetCrtPanelVisible(false);
    SyncRmlSettingsValues();
    FocusElementById("open-crt-button");
}

void ResetCrtDefaults(void)
{
    MikuPan_ResetCrtSettings();
    MarkSettingsDirty();
    const MikuPan_ConfigCrt* crt = MikuPan_GetCrtSettings();
    SetCheckbox(g_rml.crt_enabled_input, crt != nullptr && crt->enabled);
    UpdateStepControlVisuals();
    FocusElementById("reset-crt-button");
}

bool CrtPanelIsFocused(void)
{
    if (!g_rml.crt_visible || g_rml.context == nullptr)
    {
        return false;
    }

    Rml::Element* focus = g_rml.context->GetFocusElement();
    while (focus != nullptr)
    {
        if (focus == g_rml.crt_panel)
        {
            return true;
        }
        focus = focus->GetParentNode();
    }

    return false;
}

static constexpr const char* kControlsButtonLabels[MIKUPAN_CONTROLLER_LOGICAL_COUNT] = {
    "Triangle",
    "Cross / Confirm",
    "Square",
    "Circle / Cancel",
    "D-Pad Up",
    "D-Pad Down",
    "D-Pad Left",
    "D-Pad Right",
    "R3 / Right Stick",
    "Select",
    "Start / Pause",
    "L3 / Left Stick",
    "R1 / RB",
    "L2 / LT",
    "R2 / RT",
    "L1 / LB",
};

static constexpr const char* kControlsStickLabels[MIKUPAN_STICK_COUNT] = {
    "Move X",
    "Move Y",
    "Aim X",
    "Aim Y",
};

static constexpr const char* kControllerStickInvertLabels[MIKUPAN_STICK_COUNT] = {
    "Invert Move X",
    "Invert Move Y",
    "Invert Aim X",
    "Invert Aim Y",
};

std::string BuildBindingButton(const char* id, const char* text, bool small = false)
{
    std::string rml = "<button id=\"";
    rml += id;
    rml += "\" class=\"controls-bind-value";
    if (small)
    {
        rml += " small";
    }
    rml += "\">";
    rml += EscapeRmlText(text);
    rml += "</button>";
    return rml;
}

std::string BuildControlButtonRow(int index, const char* value, const char* bind_prefix, const char* clear_prefix)
{
    const std::string index_text = std::to_string(index);
    std::string rml = "<div class=\"controls-bind-row\"><span class=\"controls-bind-label\">";
    rml += EscapeRmlText(kControlsButtonLabels[index]);
    rml += "</span>";
    rml += BuildBindingButton((std::string(bind_prefix) + index_text).c_str(), value);
    rml += "<button id=\"";
    rml += clear_prefix;
    rml += index_text;
    rml += "\" class=\"controls-clear-button\">Clear</button></div>";
    return rml;
}

std::string BuildKeyboardDirectionRow(const char* label,
                                      const char* bind_id,
                                      const char* clear_id,
                                      int scancode)
{
    std::string rml = "<div class=\"controls-bind-row\"><span class=\"controls-bind-label\">";
    rml += EscapeRmlText(label);
    rml += "</span>";
    rml += BuildBindingButton(bind_id, MikuPan_ControllerScanCodeLabel(scancode));
    rml += "<button id=\"";
    rml += clear_id;
    rml += "\" class=\"controls-clear-button\">Clear</button></div>";
    return rml;
}

std::string BuildControllerInvertRow(int index)
{
    const std::string index_text = std::to_string(index);
    std::string rml = "<div class=\"controls-bind-row controls-option-row\"><span class=\"controls-bind-label\">";
    rml += EscapeRmlText(kControllerStickInvertLabels[index]);
    rml += "</span><button id=\"invert-pad-stick-";
    rml += index_text;
    rml += "\" class=\"controls-option-button\">";
    rml += mikupan_stick_controller_map[index].invert ? "On" : "Off";
    rml += "</button></div>";
    return rml;
}

std::string BuildControllerRumbleRow(void)
{
    std::string rml = "<div class=\"controls-bind-row controls-option-row\"><span class=\"controls-bind-label\">Controller Rumble</span>";
    rml += "<button id=\"controller-rumble-toggle\" class=\"controls-option-button\">";
    rml += MikuPan_ControllerRumbleEnabled() ? "On" : "Off";
    rml += "</button></div>";
    return rml;
}

std::string BuildSpecialActionRow(int index)
{
    const std::string index_text = std::to_string(index);
    std::string rml = "<div class=\"controls-bind-row\"><span class=\"controls-bind-label\">";
    rml += EscapeRmlText(MikuPan_SpecialActionLabel(index));
    rml += "</span>";
    rml += BuildBindingButton(("bind-special-" + index_text).c_str(),
                              MikuPan_InputBindingLabel(mikupan_special_action_map[index]));
    rml += "<button id=\"clear-special-" + index_text + "\" class=\"controls-clear-button\">Clear</button></div>";
    return rml;
}

std::string BuildCameraActivationRow(void)
{
    std::string rml = "<div class=\"controls-bind-row controls-option-row\"><span class=\"controls-bind-label\">Camera Activation</span>";
    rml += "<button id=\"camera-activation-toggle\" class=\"controls-option-button\">";
    rml += MikuPan_CameraActivationMode() == MIKUPAN_CAMERA_ACTIVATION_TOGGLE
               ? "Toggle"
               : "Hold";
    rml += "</button></div>";
    return rml;
}

int CurrentDpadMovementUsesSubjective(void)
{
    if (MikuPan_IsCustomActionProfileEnabled() || MikuPan_MovementStyleOverrideEnabled())
    {
        return MikuPan_CustomActionProfileUsesDpadSubjectiveMove();
    }

    return MikuPan_KeyProfileUsesSubjectiveMove();
}

int CurrentStickMovementUsesSubjective(void)
{
    if (MikuPan_IsCustomActionProfileEnabled() || MikuPan_MovementStyleOverrideEnabled())
    {
        return MikuPan_CustomActionProfileUsesStickSubjectiveMove();
    }

    return MikuPan_KeyProfileUsesSubjectiveMove();
}

const char* MovementStyleLabel(int subjective)
{
    return subjective ? "Subjective" : "Objective";
}

void SyncMovementStyleButtons(void)
{
    if (g_rml.document == nullptr)
    {
        return;
    }

    SetElementText(g_rml.document->GetElementById("movement-dpad-mode"),
                   MovementStyleLabel(CurrentDpadMovementUsesSubjective()));
    SetElementText(g_rml.document->GetElementById("movement-stick-mode"),
                   MovementStyleLabel(CurrentStickMovementUsesSubjective()));
}

void SeedMovementStyleOverride(void)
{
    if (!MikuPan_IsCustomActionProfileEnabled()
        && !MikuPan_MovementStyleOverrideEnabled())
    {
        const int subjective = MikuPan_KeyProfileUsesSubjectiveMove();
        MikuPan_SetCustomActionProfileDpadSubjectiveMove(subjective);
        MikuPan_SetCustomActionProfileStickSubjectiveMove(subjective);
    }
    MikuPan_SetMovementStyleOverrideEnabled(1);
}

void ToggleMovementStyle(int dpad)
{
    const float scroll_top = GetPageAreaScrollTop();
    SeedMovementStyleOverride();
    if (dpad)
    {
        MikuPan_SetCustomActionProfileDpadSubjectiveMove(
            !CurrentDpadMovementUsesSubjective());
        FocusElementById("movement-dpad-mode");
    }
    else
    {
        MikuPan_SetCustomActionProfileStickSubjectiveMove(
            !CurrentStickMovementUsesSubjective());
        FocusElementById("movement-stick-mode");
    }
    MarkSettingsDirty();
    SyncMovementStyleButtons();
    RestorePageAreaScrollTop(scroll_top);
    RequestUiMoveSound();
}

const char* FinderStickLayoutLabel(void)
{
    return MikuPan_CustomActionProfileSwapsFinderSticks() ? "Modern" : "Classic";
}

const char* FinderFilmSwapLabel(void)
{
    return MikuPan_FinderDpadFilmSwapEnabled() ? "On" : "Off";
}

void SyncViewfinderButtons(void)
{
    if (g_rml.document == nullptr)
    {
        return;
    }

    SetElementText(g_rml.document->GetElementById("finder-stick-layout-mode"),
                   FinderStickLayoutLabel());
    SetElementText(g_rml.document->GetElementById("finder-dpad-film-swap-mode"),
                   FinderFilmSwapLabel());
}

void ToggleFinderStickLayout(void)
{
    const float scroll_top = GetPageAreaScrollTop();
    MikuPan_SetCustomActionProfileFinderSwapSticks(
        !MikuPan_CustomActionProfileSwapsFinderSticks());
    MarkSettingsDirty();
    SyncViewfinderButtons();
    FocusElementById("finder-stick-layout-mode");
    RestorePageAreaScrollTop(scroll_top);
    RequestUiMoveSound();
}

void ToggleFinderDpadFilmSwap(void)
{
    const float scroll_top = GetPageAreaScrollTop();
    MikuPan_SetFinderDpadFilmSwapEnabled(
        !MikuPan_FinderDpadFilmSwapEnabled());
    MarkSettingsDirty();
    SyncViewfinderButtons();
    SetCheckbox(g_rml.finder_dpad_film_swap_input,
                MikuPan_FinderDpadFilmSwapEnabled());
    FocusElementById("finder-dpad-film-swap-mode");
    RestorePageAreaScrollTop(scroll_top);
    RequestUiMoveSound();
}

void RebuildControlsList(void)
{
    if (g_rml.controls_list == nullptr)
    {
        return;
    }

    const float scroll_top = GetPageAreaScrollTop();
    std::string rml;
    if (g_rml.controls_tab == MIKUPAN_CONTROLS_TAB_KEYBOARD)
    {
        rml += "<div class=\"controls-section-title\">Gameplay Actions</div>";
        for (int i = 0; i < MIKUPAN_SPECIAL_ACTION_COUNT; i++)
        {
            rml += BuildSpecialActionRow(i);
            if (i == MIKUPAN_SPECIAL_ACTION_RAISE_CAMERA)
            {
                rml += BuildCameraActivationRow();
            }
        }

        rml += "<div class=\"controls-section-title\">Keyboard Buttons</div>";
        for (int i = 0; i < MIKUPAN_CONTROLLER_LOGICAL_COUNT; i++)
        {
            rml += BuildControlButtonRow(i,
                                         MikuPan_ControllerScanCodeLabel(mikupan_keyboard_map[i]),
                                         "bind-key-",
                                         "clear-key-");
        }

        rml += "<div class=\"controls-section-title\">Movement</div>";
        rml += BuildKeyboardDirectionRow("Forward",
                                        "bind-key-stick-neg-1",
                                        "clear-key-stick-neg-1",
                                        mikupan_stick_keyboard_map[MIKUPAN_STICK_LY].neg_scancode);
        rml += BuildKeyboardDirectionRow("Backward",
                                        "bind-key-stick-pos-1",
                                        "clear-key-stick-pos-1",
                                        mikupan_stick_keyboard_map[MIKUPAN_STICK_LY].pos_scancode);
        rml += BuildKeyboardDirectionRow("Left",
                                        "bind-key-stick-neg-0",
                                        "clear-key-stick-neg-0",
                                        mikupan_stick_keyboard_map[MIKUPAN_STICK_LX].neg_scancode);
        rml += BuildKeyboardDirectionRow("Right",
                                        "bind-key-stick-pos-0",
                                        "clear-key-stick-pos-0",
                                        mikupan_stick_keyboard_map[MIKUPAN_STICK_LX].pos_scancode);

        rml += "<div class=\"controls-section-title\">Aim</div>";
        rml += BuildKeyboardDirectionRow("Aim Up",
                                        "bind-key-stick-neg-3",
                                        "clear-key-stick-neg-3",
                                        mikupan_stick_keyboard_map[MIKUPAN_STICK_RY].neg_scancode);
        rml += BuildKeyboardDirectionRow("Aim Down",
                                        "bind-key-stick-pos-3",
                                        "clear-key-stick-pos-3",
                                        mikupan_stick_keyboard_map[MIKUPAN_STICK_RY].pos_scancode);
        rml += BuildKeyboardDirectionRow("Aim Left",
                                        "bind-key-stick-neg-2",
                                        "clear-key-stick-neg-2",
                                        mikupan_stick_keyboard_map[MIKUPAN_STICK_RX].neg_scancode);
        rml += BuildKeyboardDirectionRow("Aim Right",
                                        "bind-key-stick-pos-2",
                                        "clear-key-stick-pos-2",
                                        mikupan_stick_keyboard_map[MIKUPAN_STICK_RX].pos_scancode);
    }
    else
    {
        SDL_Gamepad* gp = MikuPan_GetController();
        if (gp == nullptr)
        {
            rml += "<div class=\"placeholder\">No controller is currently open. Connect a controller, or select one in the ImGui device selector for now.</div>";
        }

        rml += "<div class=\"controls-section-title\">Controller</div>";
        rml += BuildControllerRumbleRow();

        rml += "<div class=\"controls-section-title\">Buttons</div>";
        for (int i = 0; i < MIKUPAN_CONTROLLER_LOGICAL_COUNT; i++)
        {
            rml += BuildControlButtonRow(i,
                                         MikuPan_ControllerBindingLabel(mikupan_controller_map[i]),
                                         "bind-pad-",
                                         "clear-pad-");
        }

        rml += "<div class=\"controls-section-title\">Sticks</div>";
        for (int i = 0; i < MIKUPAN_STICK_COUNT; i++)
        {
            const std::string index_text = std::to_string(i);
            rml += "<div class=\"controls-bind-row\"><span class=\"controls-bind-label\">";
            rml += EscapeRmlText(kControlsStickLabels[i]);
            rml += "</span>";
            rml += BuildBindingButton(("bind-pad-stick-" + index_text).c_str(),
                                      MikuPan_ControllerStickAxisLabel(mikupan_stick_controller_map[i].axis));
            rml += "<button id=\"clear-pad-stick-" + index_text + "\" class=\"controls-clear-button\">Clear</button></div>";
        }

        rml += "<div class=\"controls-section-title\">Stick Invert</div>";
        for (int i = 0; i < MIKUPAN_STICK_COUNT; i++)
        {
            rml += BuildControllerInvertRow(i);
        }
    }

    SetElementText(g_rml.controls_list, rml);
    RestorePageAreaScrollTop(scroll_top);
}

void SelectControlsTab(MikuPanControlsTab tab)
{
    g_rml.controls_tab = tab;
    if (g_rml.controls_keyboard_tab != nullptr)
    {
        g_rml.controls_keyboard_tab->SetClass("selected", tab == MIKUPAN_CONTROLS_TAB_KEYBOARD);
    }
    if (g_rml.controls_controller_tab != nullptr)
    {
        g_rml.controls_controller_tab->SetClass("selected", tab == MIKUPAN_CONTROLS_TAB_CONTROLLER);
    }
    RebuildControlsList();
}

bool BindingCaptureIsVisible(void)
{
    return g_rml.binding_capture_kind != MIKUPAN_BIND_CAPTURE_NONE;
}

void SetBindingCaptureVisible(bool visible)
{
    if (g_rml.binding_capture_modal != nullptr)
    {
        g_rml.binding_capture_modal->SetClass("hidden", !visible);
    }
}

void SetBindingCaptureCountdown(uint64_t now)
{
    if (g_rml.binding_capture_countdown == nullptr)
    {
        return;
    }

    int seconds_left = 0;
    if (g_rml.binding_capture_deadline_ticks > now)
    {
        seconds_left = static_cast<int>((g_rml.binding_capture_deadline_ticks - now + 999u) / 1000u);
    }

    SetElementText(g_rml.binding_capture_countdown, std::to_string(seconds_left));
}

const char* BindingCaptureDeviceText(MikuPanBindingCaptureKind kind)
{
    switch (kind)
    {
        case MIKUPAN_BIND_CAPTURE_SPECIAL_ACTION:
            return "Press a key, mouse button, or scroll wheel...";
        case MIKUPAN_BIND_CAPTURE_KEYBOARD_BUTTON:
        case MIKUPAN_BIND_CAPTURE_KEYBOARD_STICK_NEG:
        case MIKUPAN_BIND_CAPTURE_KEYBOARD_STICK_POS:
            return "Press a key...";
        case MIKUPAN_BIND_CAPTURE_CONTROLLER_BUTTON:
            return "Press a controller button or trigger...";
        case MIKUPAN_BIND_CAPTURE_CONTROLLER_STICK_AXIS:
            return "Move a controller stick...";
        default:
            return "Press input...";
    }
}

std::string BindingCaptureFocusId(MikuPanBindingCaptureKind kind, int target)
{
    if (target < 0)
    {
        return {};
    }

    const std::string index = std::to_string(target);
    switch (kind)
    {
        case MIKUPAN_BIND_CAPTURE_SPECIAL_ACTION:
            return "bind-special-" + index;
        case MIKUPAN_BIND_CAPTURE_KEYBOARD_BUTTON:
            return "bind-key-" + index;
        case MIKUPAN_BIND_CAPTURE_KEYBOARD_STICK_NEG:
            return "bind-key-stick-neg-" + index;
        case MIKUPAN_BIND_CAPTURE_KEYBOARD_STICK_POS:
            return "bind-key-stick-pos-" + index;
        case MIKUPAN_BIND_CAPTURE_CONTROLLER_BUTTON:
            return "bind-pad-" + index;
        case MIKUPAN_BIND_CAPTURE_CONTROLLER_STICK_AXIS:
            return "bind-pad-stick-" + index;
        default:
            return {};
    }
}

void RestoreBindingCaptureFocus(void)
{
    if (!g_rml.visible || !g_rml.option_mode_open)
    {
        g_rml.binding_capture_restore_focus_id.clear();
        g_rml.queued_focus_id.clear();
        g_rml.queued_focus_attempts = 0;
        return;
    }

    const std::string restore_id = g_rml.binding_capture_restore_focus_id;
    g_rml.binding_capture_restore_focus_id.clear();
    if (!restore_id.empty())
    {
        if (!FocusElementByIdPreservePageScroll(restore_id.c_str()))
        {
            QueueFocusElementById(restore_id.c_str());
        }
        return;
    }

    const char* tab_id = g_rml.controls_tab == MIKUPAN_CONTROLS_TAB_KEYBOARD
                             ? "controls-keyboard-tab"
                             : "controls-controller-tab";
    if (!FocusElementByIdPreservePageScroll(tab_id))
    {
        QueueFocusElementById(tab_id);
    }
}

void BeginBindingCapture(MikuPanBindingCaptureKind kind, int target)
{
    const uint64_t now = SDL_GetTicks();
    MikuPan_InputBindingCaptureReset();
    g_rml.binding_capture_kind = kind;
    g_rml.binding_capture_target = target;
    g_rml.binding_capture_restore_focus_id = BindingCaptureFocusId(kind, target);
    if (g_rml.binding_capture_restore_focus_id.empty())
    {
        g_rml.binding_capture_restore_focus_id = GetFocusedElementId().c_str();
    }
    g_rml.binding_capture_after_ticks = now + 180u;
    g_rml.binding_capture_deadline_ticks = now + 5000u;
    g_rml.binding_capture_waiting_for_release = true;
    SetElementText(g_rml.binding_capture_title, "Bind Control");
    SetElementText(g_rml.binding_capture_text, BindingCaptureDeviceText(kind));
    SetBindingCaptureCountdown(now);
    SetBindingCaptureVisible(true);
}

void CancelBindingCapture(void)
{
    const bool was_visible = BindingCaptureIsVisible();
    g_rml.binding_capture_kind = MIKUPAN_BIND_CAPTURE_NONE;
    g_rml.binding_capture_target = -1;
    g_rml.binding_capture_after_ticks = 0;
    g_rml.binding_capture_deadline_ticks = 0;
    g_rml.binding_capture_waiting_for_release = false;
    MikuPan_InputBindingCaptureReset();
    SetElementText(g_rml.binding_capture_countdown, "0");
    SetBindingCaptureVisible(false);
    if (was_visible)
    {
        BlockOptionsInputBriefly();
    }
    RestoreBindingCaptureFocus();
}

void FinishBindingCapture(void)
{
    MarkSettingsDirty();
    g_rml.binding_capture_kind = MIKUPAN_BIND_CAPTURE_NONE;
    g_rml.binding_capture_target = -1;
    g_rml.binding_capture_after_ticks = 0;
    g_rml.binding_capture_deadline_ticks = 0;
    g_rml.binding_capture_waiting_for_release = false;
    MikuPan_InputBindingCaptureReset();
    SetElementText(g_rml.binding_capture_countdown, "0");
    SetBindingCaptureVisible(false);
    BlockOptionsInputBriefly();
    RebuildControlsList();
    RestoreBindingCaptureFocus();
    RequestUiMoveSound();
}

bool AnyKeyboardCaptureInputDown(void)
{
    const bool* keys = SDL_GetKeyboardState(nullptr);
    if (keys == nullptr)
    {
        return false;
    }

    for (int i = 4; i < SDL_SCANCODE_COUNT; i++)
    {
        if (keys[i])
        {
            return true;
        }
    }

    return false;
}

int FindPressedKeyboardScancode(void)
{
    const bool* keys = SDL_GetKeyboardState(nullptr);
    if (keys == nullptr)
    {
        return -1;
    }

    if (keys[SDL_SCANCODE_ESCAPE])
    {
        return SDL_SCANCODE_ESCAPE;
    }
    for (int i = 4; i < SDL_SCANCODE_COUNT; i++)
    {
        if (keys[i])
        {
            return i;
        }
    }

    return -1;
}

bool AnyControllerCaptureInputDown(SDL_Gamepad* gp)
{
    if (gp == nullptr)
    {
        return false;
    }

    for (int b = 0; b < SDL_GAMEPAD_BUTTON_COUNT; b++)
    {
        if (SDL_GetGamepadButton(gp, static_cast<SDL_GamepadButton>(b)))
        {
            return true;
        }
    }

    const int axes[] = {
        SDL_GAMEPAD_AXIS_LEFTX,
        SDL_GAMEPAD_AXIS_LEFTY,
        SDL_GAMEPAD_AXIS_RIGHTX,
        SDL_GAMEPAD_AXIS_RIGHTY,
        SDL_GAMEPAD_AXIS_LEFT_TRIGGER,
        SDL_GAMEPAD_AXIS_RIGHT_TRIGGER,
    };

    for (int axis : axes)
    {
        const int value = SDL_GetGamepadAxis(gp, static_cast<SDL_GamepadAxis>(axis));
        if (value > 12000 || value < -12000)
        {
            return true;
        }
    }

    return false;
}

int FindPressedControllerInput(SDL_Gamepad* gp, int* out_kind, int* out_code)
{
    if (gp == nullptr || out_kind == nullptr || out_code == nullptr)
    {
        return 0;
    }

    for (int b = 0; b < SDL_GAMEPAD_BUTTON_COUNT; b++)
    {
        if (SDL_GetGamepadButton(gp, static_cast<SDL_GamepadButton>(b)))
        {
            *out_kind = MIKUPAN_CONTROLLER_BIND_BUTTON;
            *out_code = b;
            return 1;
        }
    }

    if (SDL_GetGamepadAxis(gp, SDL_GAMEPAD_AXIS_LEFT_TRIGGER) > 16384)
    {
        *out_kind = MIKUPAN_CONTROLLER_BIND_AXIS;
        *out_code = SDL_GAMEPAD_AXIS_LEFT_TRIGGER;
        return 1;
    }
    if (SDL_GetGamepadAxis(gp, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER) > 16384)
    {
        *out_kind = MIKUPAN_CONTROLLER_BIND_AXIS;
        *out_code = SDL_GAMEPAD_AXIS_RIGHT_TRIGGER;
        return 1;
    }

    return 0;
}

int FindMovedControllerStickAxis(SDL_Gamepad* gp, int* out_axis)
{
    if (gp == nullptr || out_axis == nullptr)
    {
        return 0;
    }

    const int threshold = 20000;
    const int axes[] = {
        SDL_GAMEPAD_AXIS_LEFTX,
        SDL_GAMEPAD_AXIS_LEFTY,
        SDL_GAMEPAD_AXIS_RIGHTX,
        SDL_GAMEPAD_AXIS_RIGHTY,
    };

    for (int axis : axes)
    {
        const int value = SDL_GetGamepadAxis(gp, static_cast<SDL_GamepadAxis>(axis));
        if (value > threshold || value < -threshold)
        {
            *out_axis = axis;
            return 1;
        }
    }

    return 0;
}

void UpdateBindingCapture(void)
{
    if (!BindingCaptureIsVisible())
    {
        return;
    }

    const uint64_t now = SDL_GetTicks();
    SetBindingCaptureCountdown(now);
    if (g_rml.binding_capture_deadline_ticks != 0
        && now >= g_rml.binding_capture_deadline_ticks)
    {
        CancelBindingCapture();
        return;
    }

    if (now < g_rml.binding_capture_after_ticks)
    {
        return;
    }

    if (g_rml.binding_capture_waiting_for_release)
    {
        const bool waiting_for_special =
            g_rml.binding_capture_kind == MIKUPAN_BIND_CAPTURE_SPECIAL_ACTION;
        const bool waiting_for_keyboard =
            g_rml.binding_capture_kind == MIKUPAN_BIND_CAPTURE_KEYBOARD_BUTTON
            || g_rml.binding_capture_kind == MIKUPAN_BIND_CAPTURE_KEYBOARD_STICK_NEG
            || g_rml.binding_capture_kind == MIKUPAN_BIND_CAPTURE_KEYBOARD_STICK_POS;
        const bool input_down = waiting_for_special
                                    ? MikuPan_InputBindingCaptureAnyDown() != 0
                                    : waiting_for_keyboard
                                          ? AnyKeyboardCaptureInputDown()
                                          : AnyControllerCaptureInputDown(MikuPan_GetController());
        if (input_down)
        {
            return;
        }
        g_rml.binding_capture_waiting_for_release = false;
        SetElementText(g_rml.binding_capture_text, BindingCaptureDeviceText(g_rml.binding_capture_kind));
    }

    const int target = g_rml.binding_capture_target;
    if (target < 0)
    {
        CancelBindingCapture();
        return;
    }

    switch (g_rml.binding_capture_kind)
    {
        case MIKUPAN_BIND_CAPTURE_SPECIAL_ACTION:
        {
            MikuPan_InputBinding binding = {};
            if (!MikuPan_InputBindingCapturePoll(&binding))
            {
                return;
            }
            if (target < MIKUPAN_SPECIAL_ACTION_COUNT)
            {
                mikupan_special_action_map[target] = binding;
            }
            FinishBindingCapture();
            break;
        }
        case MIKUPAN_BIND_CAPTURE_KEYBOARD_BUTTON:
        case MIKUPAN_BIND_CAPTURE_KEYBOARD_STICK_NEG:
        case MIKUPAN_BIND_CAPTURE_KEYBOARD_STICK_POS:
        {
            const int scancode = FindPressedKeyboardScancode();
            if (scancode < 0)
            {
                return;
            }
            if (g_rml.binding_capture_kind == MIKUPAN_BIND_CAPTURE_KEYBOARD_BUTTON
                && target < MIKUPAN_CONTROLLER_LOGICAL_COUNT)
            {
                mikupan_keyboard_map[target] = scancode;
            }
            else if (g_rml.binding_capture_kind == MIKUPAN_BIND_CAPTURE_KEYBOARD_STICK_NEG
                     && target < MIKUPAN_STICK_COUNT)
            {
                mikupan_stick_keyboard_map[target].neg_scancode = scancode;
            }
            else if (g_rml.binding_capture_kind == MIKUPAN_BIND_CAPTURE_KEYBOARD_STICK_POS
                     && target < MIKUPAN_STICK_COUNT)
            {
                mikupan_stick_keyboard_map[target].pos_scancode = scancode;
            }
            FinishBindingCapture();
            break;
        }
        case MIKUPAN_BIND_CAPTURE_CONTROLLER_BUTTON:
        {
            int kind = 0;
            int code = 0;
            if (FindPressedControllerInput(MikuPan_GetController(), &kind, &code))
            {
                if (target < MIKUPAN_CONTROLLER_LOGICAL_COUNT)
                {
                    mikupan_controller_map[target].kind = kind;
                    mikupan_controller_map[target].code = code;
                }
                FinishBindingCapture();
            }
            break;
        }
        case MIKUPAN_BIND_CAPTURE_CONTROLLER_STICK_AXIS:
        {
            int axis = 0;
            if (FindMovedControllerStickAxis(MikuPan_GetController(), &axis))
            {
                if (target < MIKUPAN_STICK_COUNT)
                {
                    mikupan_stick_controller_map[target].axis = axis;
                }
                FinishBindingCapture();
            }
            break;
        }
        default:
            break;
    }
}

int ParseGeneratedIndex(const Rml::String& id, const char* prefix)
{
    const size_t prefix_len = std::strlen(prefix);
    if (id.size() <= prefix_len || id.compare(0, prefix_len, prefix) != 0)
    {
        return -1;
    }

    char* end = nullptr;
    const long value = std::strtol(id.c_str() + prefix_len, &end, 10);
    if (end == id.c_str() + prefix_len || *end != '\0')
    {
        return -1;
    }
    return static_cast<int>(value);
}

Rml::String FindGeneratedId(Rml::Element* element)
{
    while (element != nullptr && element != g_rml.controls_list)
    {
        const Rml::String id = element->GetId();
        if (!id.empty())
        {
            return id;
        }
        element = element->GetParentNode();
    }
    return "";
}

void HandledControlsListEvent(Rml::Event& event)
{
    event.StopPropagation();
}

void FocusGeneratedControlsButton(const char* prefix, int index)
{
    const std::string id = std::string(prefix) + std::to_string(index);
    FocusElementByIdPreservePageScroll(id.c_str());
}

void HandleControlsListClick(Rml::Event& event)
{
    Rml::String id = FindGeneratedId(event.GetTargetElement());
    if (id.empty())
    {
        id = FindGeneratedId(event.GetCurrentElement());
    }
    if (id.empty())
    {
        return;
    }

    if (id == "controller-rumble-toggle")
    {
        HandledControlsListEvent(event);
        MikuPan_SetControllerRumbleEnabled(
            !MikuPan_ControllerRumbleEnabled());
        MarkSettingsDirty();
        RebuildControlsList();
        FocusElementByIdPreservePageScroll("controller-rumble-toggle");
        RequestUiMoveSound();
        return;
    }

    if (id == "camera-activation-toggle")
    {
        HandledControlsListEvent(event);
        MikuPan_SetCameraActivationMode(
            MikuPan_CameraActivationMode() == MIKUPAN_CAMERA_ACTIVATION_TOGGLE
                ? MIKUPAN_CAMERA_ACTIVATION_HOLD
                : MIKUPAN_CAMERA_ACTIVATION_TOGGLE);
        MarkSettingsDirty();
        RebuildControlsList();
        FocusElementByIdPreservePageScroll("camera-activation-toggle");
        RequestUiMoveSound();
        return;
    }

    int index = ParseGeneratedIndex(id, "bind-special-");
    if (index >= 0)
    {
        HandledControlsListEvent(event);
        BeginBindingCapture(MIKUPAN_BIND_CAPTURE_SPECIAL_ACTION, index);
        return;
    }
    index = ParseGeneratedIndex(id, "clear-special-");
    if (index >= 0 && index < MIKUPAN_SPECIAL_ACTION_COUNT)
    {
        HandledControlsListEvent(event);
        mikupan_special_action_map[index].kind = MIKUPAN_INPUT_BIND_NONE;
        mikupan_special_action_map[index].code = 0;
        MarkSettingsDirty();
        RebuildControlsList();
        FocusGeneratedControlsButton("clear-special-", index);
        RequestUiMoveSound();
        return;
    }

    index = ParseGeneratedIndex(id, "bind-key-stick-neg-");
    if (index >= 0)
    {
        HandledControlsListEvent(event);
        BeginBindingCapture(MIKUPAN_BIND_CAPTURE_KEYBOARD_STICK_NEG, index);
        return;
    }
    index = ParseGeneratedIndex(id, "bind-key-stick-pos-");
    if (index >= 0)
    {
        HandledControlsListEvent(event);
        BeginBindingCapture(MIKUPAN_BIND_CAPTURE_KEYBOARD_STICK_POS, index);
        return;
    }
    index = ParseGeneratedIndex(id, "clear-key-stick-neg-");
    if (index >= 0 && index < MIKUPAN_STICK_COUNT)
    {
        HandledControlsListEvent(event);
        mikupan_stick_keyboard_map[index].neg_scancode = 0;
        MarkSettingsDirty();
        RebuildControlsList();
        FocusGeneratedControlsButton("clear-key-stick-neg-", index);
        RequestUiMoveSound();
        return;
    }
    index = ParseGeneratedIndex(id, "clear-key-stick-pos-");
    if (index >= 0 && index < MIKUPAN_STICK_COUNT)
    {
        HandledControlsListEvent(event);
        mikupan_stick_keyboard_map[index].pos_scancode = 0;
        MarkSettingsDirty();
        RebuildControlsList();
        FocusGeneratedControlsButton("clear-key-stick-pos-", index);
        RequestUiMoveSound();
        return;
    }
    index = ParseGeneratedIndex(id, "clear-key-stick-");
    if (index >= 0 && index < MIKUPAN_STICK_COUNT)
    {
        HandledControlsListEvent(event);
        mikupan_stick_keyboard_map[index].neg_scancode = 0;
        mikupan_stick_keyboard_map[index].pos_scancode = 0;
        MarkSettingsDirty();
        RebuildControlsList();
        FocusGeneratedControlsButton("clear-key-stick-", index);
        RequestUiMoveSound();
        return;
    }
    index = ParseGeneratedIndex(id, "bind-key-");
    if (index >= 0)
    {
        HandledControlsListEvent(event);
        BeginBindingCapture(MIKUPAN_BIND_CAPTURE_KEYBOARD_BUTTON, index);
        return;
    }
    index = ParseGeneratedIndex(id, "clear-key-");
    if (index >= 0 && index < MIKUPAN_CONTROLLER_LOGICAL_COUNT)
    {
        HandledControlsListEvent(event);
        mikupan_keyboard_map[index] = 0;
        MarkSettingsDirty();
        RebuildControlsList();
        FocusGeneratedControlsButton("clear-key-", index);
        RequestUiMoveSound();
        return;
    }
    index = ParseGeneratedIndex(id, "bind-pad-stick-");
    if (index >= 0)
    {
        HandledControlsListEvent(event);
        BeginBindingCapture(MIKUPAN_BIND_CAPTURE_CONTROLLER_STICK_AXIS, index);
        return;
    }
    index = ParseGeneratedIndex(id, "invert-pad-stick-");
    if (index >= 0 && index < MIKUPAN_STICK_COUNT)
    {
        HandledControlsListEvent(event);
        mikupan_stick_controller_map[index].invert = mikupan_stick_controller_map[index].invert ? 0 : 1;
        MarkSettingsDirty();
        RebuildControlsList();
        FocusGeneratedControlsButton("invert-pad-stick-", index);
        RequestUiMoveSound();
        return;
    }
    index = ParseGeneratedIndex(id, "clear-pad-stick-");
    if (index >= 0 && index < MIKUPAN_STICK_COUNT)
    {
        HandledControlsListEvent(event);
        mikupan_stick_controller_map[index].axis = -1;
        MarkSettingsDirty();
        RebuildControlsList();
        FocusGeneratedControlsButton("clear-pad-stick-", index);
        RequestUiMoveSound();
        return;
    }
    index = ParseGeneratedIndex(id, "bind-pad-");
    if (index >= 0)
    {
        HandledControlsListEvent(event);
        BeginBindingCapture(MIKUPAN_BIND_CAPTURE_CONTROLLER_BUTTON, index);
        return;
    }
    index = ParseGeneratedIndex(id, "clear-pad-");
    if (index >= 0 && index < MIKUPAN_CONTROLLER_LOGICAL_COUNT)
    {
        HandledControlsListEvent(event);
        mikupan_controller_map[index].kind = MIKUPAN_CONTROLLER_BIND_NONE;
        mikupan_controller_map[index].code = 0;
        MarkSettingsDirty();
        RebuildControlsList();
        FocusGeneratedControlsButton("clear-pad-", index);
        RequestUiMoveSound();
    }
}

int ClampInt(int value, int minimum, int maximum)
{
    return std::max(minimum, std::min(value, maximum));
}

float ClampFloat(float value, float minimum, float maximum)
{
    return std::max(minimum, std::min(value, maximum));
}

float StepRound(float value, float minimum, float step)
{
    if (step <= 0.0f)
    {
        return value;
    }

    const float steps = std::round((value - minimum) / step);
    return minimum + steps * step;
}

std::string WindowSizeLabel(int index)
{
    const char* label = MikuPan_GetWindowSizeOptionLabel(index);
    return label != nullptr ? label : "Unavailable";
}

std::string ResolutionLabel(int index)
{
    const char* label = MikuPan_GetRenderResolutionOptionLabel(index);
    return label != nullptr ? label : "Unavailable";
}

std::string EscapeRmlText(const char* text)
{
    if (text == nullptr)
    {
        return "";
    }

    std::string output;
    for (const char* c = text; *c != '\0'; c++)
    {
        switch (*c)
        {
            case '&':
                output += "&amp;";
                break;
            case '<':
                output += "&lt;";
                break;
            case '>':
                output += "&gt;";
                break;
            case '"':
                output += "&quot;";
                break;
            default:
                output += *c;
                break;
        }
    }
    return output;
}

float GetStepValue(const MikuPanStepControl& control)
{
    switch (control.kind)
    {
        case MIKUPAN_STEP_BRIGHTNESS:
            return MikuPan_GetBrightness();
        case MIKUPAN_STEP_GAMMA:
            return MikuPan_GetGamma();
        case MIKUPAN_STEP_CONTRAST:
            return MikuPan_GetContrast();
        case MIKUPAN_STEP_SHADOW_DEPTH:
            return MikuPan_GetShadowDepth();
        case MIKUPAN_STEP_HDR_PAPER_WHITE:
            return MikuPan_GetHdrPaperWhite();
        case MIKUPAN_STEP_HDR_PEAK_LUMINANCE:
            return MikuPan_GetHdrPeakLuminance();
        case MIKUPAN_STEP_FONT_SCALE:
            return MikuPan_GetUiFontScale();
        case MIKUPAN_STEP_CRT_FIELD:
            return control.index >= 0
                       ? MikuPan_GetCrtFieldValue(control.index)
                       : 0.0f;
        case MIKUPAN_STEP_AUDIO_MASTER:
            return MikuPan_GetAudioMasterVolume();
        default:
            return 0.0f;
    }
}

void SetStepValue(const MikuPanStepControl& control, float value)
{
    const float clamped = ClampFloat(
        StepRound(value, control.min_value, control.step),
        control.min_value,
        control.max_value);

    switch (control.kind)
    {
        case MIKUPAN_STEP_BRIGHTNESS:
            MikuPan_SetBrightness(clamped);
            break;
        case MIKUPAN_STEP_GAMMA:
            MikuPan_SetGamma(clamped);
            break;
        case MIKUPAN_STEP_CONTRAST:
            MikuPan_SetContrast(clamped);
            break;
        case MIKUPAN_STEP_SHADOW_DEPTH:
            MikuPan_SetShadowDepth(clamped);
            break;
        case MIKUPAN_STEP_HDR_PAPER_WHITE:
            MikuPan_SetHdrPaperWhite(clamped);
            break;
        case MIKUPAN_STEP_HDR_PEAK_LUMINANCE:
            MikuPan_SetHdrPeakLuminance(clamped);
            break;
        case MIKUPAN_STEP_FONT_SCALE:
            MikuPan_SetUiFontScale(clamped);
            break;
        case MIKUPAN_STEP_CRT_FIELD:
            if (control.index >= 0)
            {
                MikuPan_SetCrtFieldValue(control.index, clamped);
            }
            break;
        case MIKUPAN_STEP_AUDIO_MASTER:
            MikuPan_SetAudioMasterVolume(clamped);
            break;
        default:
            break;
    }
}

float ApplyCalibrationOutputLevel(float value)
{
    float color = ClampFloat(value, 0.0f, 1.0f);
    color *= MikuPan_GetBrightness();
    color = (color - 0.5f) * std::max(MikuPan_GetContrast(), 0.0f) + 0.5f;
    color = std::pow(std::max(color, 0.0f),
                     1.0f / std::max(MikuPan_GetGamma(), 0.01f));
    return ClampFloat(color, 0.0f, 1.0f);
}

std::string FormatGrayColor(float value)
{
    const int channel = ClampInt(
        static_cast<int>(std::round(ClampFloat(value, 0.0f, 1.0f) * 255.0f)),
        0,
        255);
    char text[16];
    std::snprintf(text, sizeof(text), "#%02x%02x%02x", channel, channel, channel);
    return text;
}

void UpdateCalibrationLevelVisuals(void)
{
    const int count = static_cast<int>(g_rml.calibration_level_swatches.size());
    for (int i = 0; i < count; i++)
    {
        Rml::Element* swatch = g_rml.calibration_level_swatches[static_cast<size_t>(i)];
        if (swatch == nullptr)
        {
            continue;
        }

        const float t = count > 1
                            ? static_cast<float>(i) / static_cast<float>(count - 1)
                            : 0.0f;
        const float source = std::pow(t, 2.0f);
        const std::string color = FormatGrayColor(ApplyCalibrationOutputLevel(source));
        swatch->SetProperty("background-color", color);
    }
}

int GetStepVisualSegmentCount(float minimum,
                              float maximum,
                              float step,
                              int preferred_segments)
{
    if (step > 0.0f)
    {
        const int step_count = static_cast<int>(
            std::round((maximum - minimum) / step));
        if (step_count > 0 && step_count <= 39)
        {
            return step_count + 1;
        }
    }

    return std::max(1, preferred_segments);
}

int GetStepVisualIndex(float value,
                       float minimum,
                       float maximum,
                       float step,
                       int segments)
{
    if (step > 0.0f)
    {
        const int step_count = static_cast<int>(
            std::round((maximum - minimum) / step));
        if (step_count > 0 && step_count + 1 == segments)
        {
            return ClampInt(static_cast<int>(std::round((value - minimum) / step)),
                            0,
                            segments - 1);
        }
    }

    const float range = std::max(maximum - minimum, 0.0001f);
    const int last_segment = std::max(segments - 1, 1);
    return ClampInt(static_cast<int>(std::round(((value - minimum) / range)
                                                * static_cast<float>(last_segment))),
                    0,
                    segments - 1);
}

std::string BuildStepperBars(float value,
                             float minimum,
                             float maximum,
                             float step,
                             int segments,
                             float neutral_value,
                             bool has_neutral)
{
    segments = GetStepVisualSegmentCount(minimum, maximum, step, segments);
    const int active_index = GetStepVisualIndex(value,
                                                minimum,
                                                maximum,
                                                step,
                                                segments);
    const int neutral_index = GetStepVisualIndex(neutral_value,
                                                 minimum,
                                                 maximum,
                                                 step,
                                                 segments);

    std::string rml;
    rml.reserve(static_cast<size_t>(segments) * 56u);
    for (int i = 0; i < segments; i++)
    {
        bool filled = false;
        if (has_neutral)
        {
            filled = active_index >= neutral_index
                         ? i >= neutral_index && i <= active_index
                         : i <= neutral_index && i >= active_index;
        }
        else
        {
            filled = active_index > 0 && i <= active_index;
        }

        std::string classes = "stepper-segment step-index-" + std::to_string(i);
        if (filled)
        {
            classes += " filled";
        }
        if (has_neutral && i == neutral_index)
        {
            classes += " neutral";
        }
        rml += "<span class=\"" + classes + "\"></span>";
    }
    return rml;
}

std::string FormatStepValue(const MikuPanStepControl& control, float value)
{
    if (control.suffix_percent)
    {
        return FormatFloat(value * 100.0f, control.precision) + "%";
    }

    std::string text = FormatFloat(value, control.precision);
    if (control.suffix_x)
    {
        text += "x";
    }
    return text;
}

void UpdateStepControlVisual(MikuPanStepControl& control)
{
    const float value = ClampFloat(GetStepValue(control),
                                   control.min_value,
                                   control.max_value);
    SetElementText(control.value, FormatStepValue(control, value));
    SetElementText(control.bars,
                   BuildStepperBars(value,
                                    control.visual_min_value,
                                    control.visual_max_value,
                                    control.step,
                                    control.segments,
                                    control.neutral_value,
                                    control.has_neutral));
}

void UpdateStepControlVisuals(void)
{
    for (MikuPanStepControl& control : g_rml.step_controls)
    {
        if (control.control == nullptr)
        {
            continue;
        }
        if (control.kind == MIKUPAN_STEP_CRT_FIELD && !g_rml.crt_visible)
        {
            continue;
        }
        UpdateStepControlVisual(control);
    }
    if (g_rml.calibration_visible)
    {
        UpdateCalibrationLevelVisuals();
    }
}

MikuPanStepControl* FindFocusedStepControl(void)
{
    if (g_rml.context == nullptr)
    {
        return nullptr;
    }

    Rml::Element* focus = g_rml.context->GetFocusElement();
    if (focus == nullptr)
    {
        return nullptr;
    }

    for (MikuPanStepControl& control : g_rml.step_controls)
    {
        if (focus == control.control)
        {
            return &control;
        }
    }

    return nullptr;
}

void CommitStepControlValue(MikuPanStepControl& control, float value)
{
    const float old_value = GetStepValue(control);
    SetStepValue(control, value);
    const float new_value = GetStepValue(control);
    if (std::abs(new_value - old_value) < 0.00001f)
    {
        return;
    }

    MarkSettingsDirty();
    UpdateStepControlVisual(control);
    UpdateCalibrationLevelVisuals();
    RequestUiMoveSound();
}

void AdjustStepControl(MikuPanStepControl& control, int direction)
{
    if (direction == 0)
    {
        return;
    }

    CommitStepControlValue(control,
                           ClampFloat(GetStepValue(control) + control.step * direction,
                                      control.min_value,
                                      control.max_value));
}

MikuPanStepControl* FindStepControlByElement(Rml::Element* element)
{
    while (element != nullptr)
    {
        for (MikuPanStepControl& control : g_rml.step_controls)
        {
            if (element == control.control)
            {
                return &control;
            }
        }
        element = element->GetParentNode();
    }

    return nullptr;
}

int FindStepSegmentIndex(Rml::Element* element)
{
    while (element != nullptr)
    {
        for (int i = 0; i < 40; i++)
        {
            const std::string class_name = "step-index-" + std::to_string(i);
            if (element->IsClassSet(class_name.c_str()))
            {
                return i;
            }
        }
        element = element->GetParentNode();
    }

    return -1;
}

int FindStepEdgeDirection(Rml::Element* element)
{
    while (element != nullptr)
    {
        if (element->IsClassSet("stepper-left"))
        {
            return -1;
        }
        if (element->IsClassSet("stepper-right"))
        {
            return 1;
        }
        element = element->GetParentNode();
    }

    return 0;
}

float GetElementClickT(Rml::Event& event, Rml::Element* element)
{
    if (element == nullptr)
    {
        return -1.0f;
    }

    const float width = static_cast<float>(element->GetOffsetWidth());
    if (width <= 1.0f)
    {
        return -1.0f;
    }

    const Rml::Vector2f offset = element->GetAbsoluteOffset();
    const float mouse_x = event.GetParameter("mouse_x", -1.0f);
    if (mouse_x < offset.x || mouse_x > offset.x + width)
    {
        return -1.0f;
    }

    return ClampFloat((mouse_x - offset.x) / width, 0.0f, 1.0f);
}

float GetStepBarsClickT(Rml::Event& event, const MikuPanStepControl& control)
{
    if (control.bars == nullptr)
    {
        return -1.0f;
    }

    const int visual_segments = GetStepVisualSegmentCount(control.visual_min_value,
                                                          control.visual_max_value,
                                                          control.step,
                                                          control.segments);
    if (visual_segments <= 1)
    {
        return GetElementClickT(event, control.bars);
    }

    const float segment_width = 2.0f;
    const float segment_margin = 0.5f;
    const float visual_width = static_cast<float>(visual_segments) * (segment_width + segment_margin * 2.0f);
    const float container_width = static_cast<float>(control.bars->GetOffsetWidth());
    if (container_width <= 1.0f || visual_width <= 1.0f)
    {
        return -1.0f;
    }

    const Rml::Vector2f offset = control.bars->GetAbsoluteOffset();
    const float content_width = std::min(visual_width, container_width);
    const float visual_left = offset.x + std::max((container_width - content_width) * 0.5f, 0.0f);
    const float visual_right = visual_left + content_width;
    const float mouse_x = event.GetParameter("mouse_x", -1.0f);
    if (mouse_x < visual_left || mouse_x > visual_right)
    {
        return -1.0f;
    }

    return ClampFloat((mouse_x - visual_left) / content_width, 0.0f, 1.0f);
}

void HandleStepControlClick(Rml::Event& event)
{
    MikuPanStepControl* control = FindStepControlByElement(event.GetTargetElement());
    if (control == nullptr)
    {
        control = FindStepControlByElement(event.GetCurrentElement());
    }
    if (control == nullptr)
    {
        return;
    }

    if (control->control != nullptr)
    {
        control->control->Focus();
    }

    const int direction = FindStepEdgeDirection(event.GetTargetElement());
    if (direction != 0)
    {
        AdjustStepControl(*control, direction);
        return;
    }

    float t = GetStepBarsClickT(event, *control);
    if (t < 0.0f)
    {
        const int segment_index = FindStepSegmentIndex(event.GetTargetElement());
        if (segment_index >= 0)
        {
            const int visual_segments = GetStepVisualSegmentCount(control->visual_min_value,
                                                                  control->visual_max_value,
                                                                  control->step,
                                                                  control->segments);
            const int last_segment = std::max(visual_segments - 1, 1);
            t = ClampFloat(static_cast<float>(segment_index) / static_cast<float>(last_segment), 0.0f, 1.0f);
        }
    }

    if (t >= 0.0f)
    {
        CommitStepControlValue(*control,
                               control->visual_min_value
                                   + (control->visual_max_value - control->visual_min_value) * t);
    }
}

bool ApplyPendingResolution(void);
bool ApplyPendingWindowSize(void);
bool ApplyPendingWindowMode(void);

int WindowModeOptionFromValue(int mode)
{
    for (int i = 0; i < kWindowModeCount; i++)
    {
        if (kWindowModeValues[i] == mode)
        {
            return i;
        }
    }

    return 0;
}

int WindowModeValueFromOption(int index)
{
    if (index < 0 || index >= kWindowModeCount)
    {
        return MIKUPAN_WINDOW_WINDOWED;
    }

    return kWindowModeValues[index];
}

const char* WindowModeLabel(int index)
{
    if (index < 0 || index >= kWindowModeCount)
    {
        return "Windowed";
    }
    return kWindowModeLabels[index];
}

int GetFinderSurroundIndex(void)
{
    return mikupan_configuration.renderer.finder_viewport_mask_mode
                   == MIKUPAN_FINDER_VIEWPORT_MASK_BLACK
               ? MIKUPAN_FINDER_VIEWPORT_MASK_BLACK
               : MIKUPAN_FINDER_VIEWPORT_MASK_BLUR;
}

bool PickerUsesSharedPendingIndex(MikuPanChoicePickerKind kind)
{
    switch (kind)
    {
        case MIKUPAN_PICKER_GPU_BACKEND:
        case MIKUPAN_PICKER_MSAA:
        case MIKUPAN_PICKER_SHADOW_RESOLUTION:
        case MIKUPAN_PICKER_LIGHTING_MODE:
        case MIKUPAN_PICKER_DITHER_MODE:
        case MIKUPAN_PICKER_FINDER_SURROUND:
        case MIKUPAN_PICKER_FLASHLIGHT_STYLE:
        case MIKUPAN_PICKER_THEME:
        case MIKUPAN_PICKER_FONT:
            return true;
        default:
            return false;
    }
}

const char* SafePickerLabel(const char* label)
{
    return label != nullptr ? label : "";
}

int GetPickerCount(MikuPanChoicePickerKind kind)
{
    switch (kind)
    {
        case MIKUPAN_PICKER_WINDOW_MODE:
            return kWindowModeCount;
        case MIKUPAN_PICKER_WINDOW_SIZE:
            return MikuPan_GetWindowSizeOptionCount();
        case MIKUPAN_PICKER_RESOLUTION:
            return MikuPan_GetRenderResolutionOptionCount();
        case MIKUPAN_PICKER_GPU_BACKEND:
            return MikuPan_GetGpuDriverOptionCount();
        case MIKUPAN_PICKER_MSAA:
            return MikuPan_GetMSAAOptionCount();
        case MIKUPAN_PICKER_SHADOW_RESOLUTION:
            return MikuPan_GetShadowResolutionOptionCount();
        case MIKUPAN_PICKER_LIGHTING_MODE:
            return kLightingModeCount;
        case MIKUPAN_PICKER_DITHER_MODE:
            return MikuPan_GetDitherModeOptionCount();
        case MIKUPAN_PICKER_FINDER_SURROUND:
            return kFinderSurroundCount;
        case MIKUPAN_PICKER_FLASHLIGHT_STYLE:
            return MikuPan_GetFlashlightStyleOptionCount();
        case MIKUPAN_PICKER_THEME:
            return MikuPan_GetThemeOptionCount();
        case MIKUPAN_PICKER_FONT:
            return MikuPan_GetFontOptionCount();
        default:
            return 0;
    }
}

int GetPickerPendingIndex(void)
{
    switch (g_rml.active_picker)
    {
        case MIKUPAN_PICKER_WINDOW_MODE:
            return g_rml.pending_window_mode;
        case MIKUPAN_PICKER_WINDOW_SIZE:
            return g_rml.pending_window_size_index;
        case MIKUPAN_PICKER_RESOLUTION:
            return g_rml.pending_resolution_index;
        default:
            return g_rml.pending_picker_index;
    }
}

int GetPickerAppliedIndex(MikuPanChoicePickerKind kind)
{
    switch (kind)
    {
        case MIKUPAN_PICKER_WINDOW_MODE:
            return WindowModeOptionFromValue(MikuPan_GetWindowMode());
        case MIKUPAN_PICKER_WINDOW_SIZE:
            return MikuPan_GetSelectedWindowSizeOption();
        case MIKUPAN_PICKER_RESOLUTION:
            return MikuPan_GetSelectedRenderResolutionOption();
        case MIKUPAN_PICKER_GPU_BACKEND:
            return MikuPan_GetSelectedGpuDriverOption();
        case MIKUPAN_PICKER_MSAA:
            return MikuPan_GetSelectedMSAAOption();
        case MIKUPAN_PICKER_SHADOW_RESOLUTION:
            return MikuPan_GetSelectedShadowResolutionOption();
        case MIKUPAN_PICKER_LIGHTING_MODE:
            return MikuPan_GetLightingMode();
        case MIKUPAN_PICKER_DITHER_MODE:
            return MikuPan_GetSelectedDitherModeOption();
        case MIKUPAN_PICKER_FINDER_SURROUND:
            return GetFinderSurroundIndex();
        case MIKUPAN_PICKER_FLASHLIGHT_STYLE:
            return MikuPan_GetSelectedFlashlightStyleOption();
        case MIKUPAN_PICKER_THEME:
            return MikuPan_GetSelectedThemeOption();
        case MIKUPAN_PICKER_FONT:
            return MikuPan_GetSelectedFontOption();
        default:
            return 0;
    }
}

const char* GetPickerLabel(MikuPanChoicePickerKind kind, int index)
{
    switch (kind)
    {
        case MIKUPAN_PICKER_WINDOW_MODE:
            return WindowModeLabel(index);
        case MIKUPAN_PICKER_WINDOW_SIZE:
            return SafePickerLabel(MikuPan_GetWindowSizeOptionLabel(index));
        case MIKUPAN_PICKER_RESOLUTION:
            return SafePickerLabel(MikuPan_GetRenderResolutionOptionLabel(index));
        case MIKUPAN_PICKER_GPU_BACKEND:
            return SafePickerLabel(MikuPan_GetGpuDriverOptionLabel(index));
        case MIKUPAN_PICKER_MSAA:
            return SafePickerLabel(MikuPan_GetMSAAOptionLabel(index));
        case MIKUPAN_PICKER_SHADOW_RESOLUTION:
            return SafePickerLabel(MikuPan_GetShadowResolutionOptionLabel(index));
        case MIKUPAN_PICKER_LIGHTING_MODE:
            return index >= 0 && index < kLightingModeCount
                       ? kLightingModeLabels[index]
                       : "";
        case MIKUPAN_PICKER_DITHER_MODE:
            return SafePickerLabel(MikuPan_GetDitherModeOptionLabel(index));
        case MIKUPAN_PICKER_FINDER_SURROUND:
            return index >= 0 && index < kFinderSurroundCount
                       ? kFinderSurroundLabels[index]
                       : "";
        case MIKUPAN_PICKER_FLASHLIGHT_STYLE:
            return SafePickerLabel(MikuPan_GetFlashlightStyleOptionLabel(index));
        case MIKUPAN_PICKER_THEME:
            return SafePickerLabel(MikuPan_GetThemeOptionLabel(index));
        case MIKUPAN_PICKER_FONT:
            return SafePickerLabel(MikuPan_GetFontOptionLabel(index));
        default:
            return "";
    }
}

const char* GetPickerTitle(MikuPanChoicePickerKind kind)
{
    switch (kind)
    {
        case MIKUPAN_PICKER_WINDOW_MODE:
            return "DISPLAY MODE";
        case MIKUPAN_PICKER_WINDOW_SIZE:
            return "WINDOW SIZE";
        case MIKUPAN_PICKER_RESOLUTION:
            return "RENDER SCALE";
        case MIKUPAN_PICKER_GPU_BACKEND:
            return "GPU BACKEND";
        case MIKUPAN_PICKER_MSAA:
            return "MSAA";
        case MIKUPAN_PICKER_SHADOW_RESOLUTION:
            return "SHADOW RESOLUTION";
        case MIKUPAN_PICKER_LIGHTING_MODE:
            return "LIGHTING MODE";
        case MIKUPAN_PICKER_DITHER_MODE:
            return "DITHER FILTERING";
        case MIKUPAN_PICKER_FINDER_SURROUND:
            return "FINDER SURROUND";
        case MIKUPAN_PICKER_FLASHLIGHT_STYLE:
            return "FLASHLIGHT STYLE";
        case MIKUPAN_PICKER_THEME:
            return "THEME";
        case MIKUPAN_PICKER_FONT:
            return "FONT";
        default:
            return "SELECT";
    }
}

const char* GetPickerFocusId(MikuPanChoicePickerKind kind)
{
    switch (kind)
    {
        case MIKUPAN_PICKER_WINDOW_MODE:
            return "window-mode-picker";
        case MIKUPAN_PICKER_WINDOW_SIZE:
            return "window-size-picker";
        case MIKUPAN_PICKER_RESOLUTION:
            return "resolution-picker";
        case MIKUPAN_PICKER_GPU_BACKEND:
            return "gpu-backend-picker";
        case MIKUPAN_PICKER_MSAA:
            return "msaa-picker";
        case MIKUPAN_PICKER_SHADOW_RESOLUTION:
            return "shadow-resolution-picker";
        case MIKUPAN_PICKER_LIGHTING_MODE:
            return "lighting-mode-picker";
        case MIKUPAN_PICKER_DITHER_MODE:
            return "dither-mode-picker";
        case MIKUPAN_PICKER_FINDER_SURROUND:
            return "finder-surround-picker";
        case MIKUPAN_PICKER_FLASHLIGHT_STYLE:
            return "flashlight-style-picker";
        case MIKUPAN_PICKER_THEME:
            return "theme-picker";
        case MIKUPAN_PICKER_FONT:
            return "font-picker";
        default:
            return nullptr;
    }
}

Rml::Element* GetPickerElement(MikuPanChoicePickerKind kind)
{
    switch (kind)
    {
        case MIKUPAN_PICKER_WINDOW_MODE:
            return g_rml.window_mode_picker;
        case MIKUPAN_PICKER_WINDOW_SIZE:
            return g_rml.window_size_picker;
        case MIKUPAN_PICKER_RESOLUTION:
            return g_rml.resolution_picker;
        case MIKUPAN_PICKER_GPU_BACKEND:
            return g_rml.gpu_backend_picker;
        case MIKUPAN_PICKER_MSAA:
            return g_rml.msaa_picker;
        case MIKUPAN_PICKER_SHADOW_RESOLUTION:
            return g_rml.shadow_resolution_picker;
        case MIKUPAN_PICKER_LIGHTING_MODE:
            return g_rml.lighting_mode_picker;
        case MIKUPAN_PICKER_DITHER_MODE:
            return g_rml.dither_mode_picker;
        case MIKUPAN_PICKER_FINDER_SURROUND:
            return g_rml.finder_surround_picker;
        case MIKUPAN_PICKER_FLASHLIGHT_STYLE:
            return g_rml.flashlight_style_picker;
        case MIKUPAN_PICKER_THEME:
            return g_rml.theme_picker;
        case MIKUPAN_PICKER_FONT:
            return g_rml.font_picker;
        default:
            return nullptr;
    }
}

Rml::Element* GetPickerChoiceElement(MikuPanChoicePickerKind kind)
{
    switch (kind)
    {
        case MIKUPAN_PICKER_GPU_BACKEND:
            return g_rml.gpu_backend_choice;
        case MIKUPAN_PICKER_MSAA:
            return g_rml.msaa_choice;
        case MIKUPAN_PICKER_SHADOW_RESOLUTION:
            return g_rml.shadow_resolution_choice;
        case MIKUPAN_PICKER_LIGHTING_MODE:
            return g_rml.lighting_mode_choice;
        case MIKUPAN_PICKER_DITHER_MODE:
            return g_rml.dither_mode_choice;
        case MIKUPAN_PICKER_FINDER_SURROUND:
            return g_rml.finder_surround_choice;
        case MIKUPAN_PICKER_FLASHLIGHT_STYLE:
            return g_rml.flashlight_style_choice;
        case MIKUPAN_PICKER_THEME:
            return g_rml.theme_choice;
        case MIKUPAN_PICKER_FONT:
            return g_rml.font_choice;
        default:
            return nullptr;
    }
}

bool PickerIsDisabled(MikuPanChoicePickerKind kind)
{
    return kind == MIKUPAN_PICKER_MSAA && MikuPan_IsSuperSamplingEnabled();
}

void SetPickerPendingIndex(int index)
{
    const int count = GetPickerCount(g_rml.active_picker);
    if (count <= 0)
    {
        index = 0;
    }
    else
    {
        index = ClampInt(index, 0, count - 1);
    }

    switch (g_rml.active_picker)
    {
        case MIKUPAN_PICKER_WINDOW_MODE:
            g_rml.pending_window_mode = index;
            g_rml.window_mode_pending_dirty =
                g_rml.pending_window_mode
                != WindowModeOptionFromValue(MikuPan_GetWindowMode());
            break;
        case MIKUPAN_PICKER_WINDOW_SIZE:
            g_rml.pending_window_size_index = index;
            g_rml.window_size_pending_dirty =
                g_rml.pending_window_size_index
                != MikuPan_GetSelectedWindowSizeOption();
            break;
        case MIKUPAN_PICKER_RESOLUTION:
            g_rml.pending_resolution_index = index;
            g_rml.resolution_pending_dirty =
                g_rml.pending_resolution_index
                != MikuPan_GetSelectedRenderResolutionOption();
            break;
        default:
            if (PickerUsesSharedPendingIndex(g_rml.active_picker))
            {
                g_rml.pending_picker_index = index;
            }
            break;
    }
}

void ScrollChoicePickerSelectionIntoView(void)
{
    if (g_rml.document == nullptr || g_rml.choice_picker_list == nullptr
        || g_rml.active_picker == MIKUPAN_PICKER_NONE)
    {
        return;
    }

    const int count = GetPickerCount(g_rml.active_picker);
    if (count <= 0)
    {
        return;
    }

    g_rml.document->UpdateDocument();

    const int pending = ClampInt(GetPickerPendingIndex(), 0, count - 1);
    const std::string id = "choice-picker-option-" + std::to_string(pending);
    Rml::Element* option = g_rml.document->GetElementById(id);
    if (option == nullptr)
    {
        return;
    }

    const float list_top = g_rml.choice_picker_list->GetAbsoluteTop();
    const float list_bottom = list_top + g_rml.choice_picker_list->GetClientHeight();
    const float option_top = option->GetAbsoluteTop();
    const float option_bottom = option_top + option->GetOffsetHeight();
    constexpr float padding = 2.0f;
    float scroll_top = g_rml.choice_picker_list->GetScrollTop();

    if (option_top < list_top + padding)
    {
        scroll_top -= (list_top + padding) - option_top;
    }
    else if (option_bottom > list_bottom - padding)
    {
        scroll_top += option_bottom - (list_bottom - padding);
    }

    const float max_scroll_top = std::max(
        0.0f,
        g_rml.choice_picker_list->GetScrollHeight()
            - g_rml.choice_picker_list->GetClientHeight());
    g_rml.choice_picker_list->SetScrollTop(
        ClampFloat(scroll_top, 0.0f, max_scroll_top));
}

void UpdateChoicePickerVisual(void)
{
    if (g_rml.active_picker == MIKUPAN_PICKER_NONE)
    {
        return;
    }

    const int count = GetPickerCount(g_rml.active_picker);
    const int pending = ClampInt(GetPickerPendingIndex(), 0, std::max(count - 1, 0));
    const int applied = ClampInt(GetPickerAppliedIndex(g_rml.active_picker),
                                 0,
                                 std::max(count - 1, 0));
    SetPickerPendingIndex(pending);

    SetElementText(g_rml.choice_picker_title,
                   GetPickerTitle(g_rml.active_picker));

    if (count <= 0)
    {
        SetElementText(g_rml.choice_picker_list,
                       "<div class=\"picker-option selected\">Unavailable</div>");
        return;
    }

    std::string rml;
    for (int i = 0; i < count; i++)
    {
        const char* label = GetPickerLabel(g_rml.active_picker, i);
        rml += "<div id=\"choice-picker-option-" + std::to_string(i)
               + "\" class=\"picker-option";
        if (i == pending)
        {
            rml += " selected";
        }
        if (i == applied)
        {
            rml += " applied";
        }
        rml += "\">";
        rml += EscapeRmlText(label);
        if (i == applied)
        {
            rml += " *";
        }
        rml += "</div>";
    }

    SetElementText(g_rml.choice_picker_list, rml);
    ScrollChoicePickerSelectionIntoView();
}

void UpdateWindowModePicker(void)
{
    const int selected = WindowModeOptionFromValue(MikuPan_GetWindowMode());
    if (!g_rml.window_mode_pending_dirty
        && g_rml.active_picker != MIKUPAN_PICKER_WINDOW_MODE
        && !g_rml.resolution_confirm_visible)
    {
        g_rml.pending_window_mode = selected;
    }
    g_rml.pending_window_mode = ClampInt(g_rml.pending_window_mode,
                                         0,
                                         kWindowModeCount - 1);

    SetElementText(g_rml.window_mode_value, WindowModeLabel(selected));
    SetElementText(g_rml.window_mode_choice,
                   WindowModeLabel(g_rml.pending_window_mode));
    SetElementText(g_rml.window_mode_pending_note,
                   g_rml.pending_window_mode != selected
                       ? "Press confirm to apply"
                       : "");
}

void UpdateWindowSizePicker(void)
{
    const int count = MikuPan_GetWindowSizeOptionCount();
    if (count <= 0)
    {
        g_rml.pending_window_size_index = 0;
        g_rml.window_size_pending_dirty = false;
        SetElementText(g_rml.window_size_value, "Unavailable");
        SetElementText(g_rml.window_size_choice, "No window sizes");
        SetElementText(g_rml.window_size_pending_note, "");
        return;
    }

    const int selected = ClampInt(MikuPan_GetSelectedWindowSizeOption(),
                                  0,
                                  count - 1);
    if (!g_rml.window_size_pending_dirty
        && g_rml.active_picker != MIKUPAN_PICKER_WINDOW_SIZE
        && !g_rml.resolution_confirm_visible)
    {
        g_rml.pending_window_size_index = selected;
    }
    g_rml.pending_window_size_index = ClampInt(g_rml.pending_window_size_index,
                                               0,
                                               count - 1);

    SetElementText(g_rml.window_size_value, WindowSizeLabel(selected));
    SetElementText(g_rml.window_size_choice,
                   WindowSizeLabel(g_rml.pending_window_size_index));
    SetElementText(g_rml.window_size_pending_note,
                   g_rml.pending_window_size_index != selected
                       ? "Press confirm to apply"
                       : "");
}

void UpdateResolutionPicker(void)
{
    const int count = MikuPan_GetRenderResolutionOptionCount();
    if (count <= 0)
    {
        g_rml.pending_resolution_index = 0;
        g_rml.resolution_pending_dirty = false;
        SetElementText(g_rml.resolution_value, "Unavailable");
        SetElementText(g_rml.resolution_choice, "No render scales");
        SetElementText(g_rml.resolution_pending_note, "");
        return;
    }

    const int selected = ClampInt(MikuPan_GetSelectedRenderResolutionOption(),
                                  0,
                                  count - 1);
    if (!g_rml.resolution_pending_dirty
        && g_rml.active_picker != MIKUPAN_PICKER_RESOLUTION
        && !g_rml.resolution_confirm_visible)
    {
        g_rml.pending_resolution_index = selected;
    }
    g_rml.pending_resolution_index = ClampInt(g_rml.pending_resolution_index,
                                              0,
                                              count - 1);

    SetElementText(g_rml.resolution_value, ResolutionLabel(selected));
    SetElementText(g_rml.resolution_choice,
                   ResolutionLabel(g_rml.pending_resolution_index));
    SetElementText(g_rml.resolution_pending_note,
                   g_rml.pending_resolution_index != selected
                       ? "Press confirm to apply"
                       : "");
}

void SetPickerDisabled(Rml::Element* picker, bool disabled)
{
    if (picker == nullptr)
    {
        return;
    }

    picker->SetClass("disabled", disabled);
    if (disabled)
    {
        picker->SetAttribute("disabled", "");
    }
    else
    {
        picker->RemoveAttribute("disabled");
    }
}

void UpdateImmediateChoicePicker(MikuPanChoicePickerKind kind)
{
    Rml::Element* choice = GetPickerChoiceElement(kind);
    if (choice != nullptr)
    {
        const int count = GetPickerCount(kind);
        if (count <= 0)
        {
            SetElementText(choice, "Unavailable");
        }
        else
        {
            const int selected = ClampInt(GetPickerAppliedIndex(kind), 0, count - 1);
            SetElementText(choice, GetPickerLabel(kind, selected));
        }
    }

    SetPickerDisabled(GetPickerElement(kind), PickerIsDisabled(kind));
}

void UpdateImmediateChoicePickers(void)
{
    UpdateImmediateChoicePicker(MIKUPAN_PICKER_GPU_BACKEND);
    UpdateImmediateChoicePicker(MIKUPAN_PICKER_MSAA);
    UpdateImmediateChoicePicker(MIKUPAN_PICKER_SHADOW_RESOLUTION);
    UpdateImmediateChoicePicker(MIKUPAN_PICKER_LIGHTING_MODE);
    UpdateImmediateChoicePicker(MIKUPAN_PICKER_DITHER_MODE);
    UpdateImmediateChoicePicker(MIKUPAN_PICKER_FINDER_SURROUND);
    UpdateImmediateChoicePicker(MIKUPAN_PICKER_FLASHLIGHT_STYLE);
    UpdateImmediateChoicePicker(MIKUPAN_PICKER_THEME);
    UpdateImmediateChoicePicker(MIKUPAN_PICKER_FONT);
}

void UpdateDisplayPickers(void)
{
    UpdateWindowModePicker();
    UpdateWindowSizePicker();
    UpdateResolutionPicker();
    UpdateImmediateChoicePickers();
    UpdateChoicePickerVisual();
    UpdateMsaaSelectState();
}

void UpdateMsaaSelectState(void)
{
    if (g_rml.msaa_select != nullptr)
    {
        const int selected = MikuPan_GetSelectedMSAAOption();
        if (g_rml.msaa_select->GetSelection() != selected)
        {
            g_rml.msaa_select->SetSelection(selected);
        }

        if (MikuPan_IsSuperSamplingEnabled())
        {
            g_rml.msaa_select->SetAttribute("disabled", "");
        }
        else
        {
            g_rml.msaa_select->RemoveAttribute("disabled");
        }
    }

    UpdateImmediateChoicePicker(MIKUPAN_PICKER_MSAA);
}

void UpdateGpuDriverNotes(void)
{
    SetElementText(g_rml.active_gpu_label,
                   std::string("Active: ")
                       + MikuPan_GetActiveGpuDriverLabel());
    SetElementText(g_rml.gpu_restart_note,
                   MikuPan_IsGpuDriverRestartPending()
                       ? "Save Configuration and restart to apply"
                       : "");
}

void SetChoicePickerVisible(bool visible)
{
    if (g_rml.choice_picker_modal != nullptr)
    {
        g_rml.choice_picker_modal->SetClass("hidden", !visible);
    }
    if (!visible)
    {
        SetElementText(g_rml.choice_picker_list, "");
    }

    if (g_rml.visible)
    {
        if (visible)
        {
            FocusElementById("choice-picker-apply-button");
        }
        else
        {
            const char* focus_id = GetPickerFocusId(g_rml.active_picker);
            if (focus_id != nullptr)
            {
                FocusElementById(focus_id);
            }
        }
    }
}

void OpenChoicePicker(MikuPanChoicePickerKind kind)
{
    if (kind == MIKUPAN_PICKER_NONE)
    {
        return;
    }
    if (PickerIsDisabled(kind) || GetPickerCount(kind) <= 0)
    {
        return;
    }

    g_rml.active_picker = kind;
    if (kind == MIKUPAN_PICKER_WINDOW_MODE)
    {
        g_rml.pending_window_mode =
            WindowModeOptionFromValue(MikuPan_GetWindowMode());
        g_rml.window_mode_pending_dirty = false;
    }
    else if (kind == MIKUPAN_PICKER_WINDOW_SIZE)
    {
        const int count = MikuPan_GetWindowSizeOptionCount();
        g_rml.pending_window_size_index =
            count > 0 ? ClampInt(MikuPan_GetSelectedWindowSizeOption(),
                                 0,
                                 count - 1)
                      : 0;
        g_rml.window_size_pending_dirty = false;
    }
    else if (kind == MIKUPAN_PICKER_RESOLUTION)
    {
        const int count = MikuPan_GetRenderResolutionOptionCount();
        g_rml.pending_resolution_index =
            count > 0 ? ClampInt(MikuPan_GetSelectedRenderResolutionOption(),
                                 0,
                                 count - 1)
                      : 0;
        g_rml.resolution_pending_dirty = false;
    }
    else if (PickerUsesSharedPendingIndex(kind))
    {
        const int count = GetPickerCount(kind);
        g_rml.pending_picker_index =
            count > 0 ? ClampInt(GetPickerAppliedIndex(kind), 0, count - 1)
                      : 0;
    }

    SetChoicePickerVisible(true);
    UpdateChoicePickerVisual();
}

void CloseChoicePicker(void)
{
    const MikuPanChoicePickerKind old_picker = g_rml.active_picker;
    SetChoicePickerVisible(false);
    g_rml.active_picker = MIKUPAN_PICKER_NONE;
    if (old_picker == MIKUPAN_PICKER_WINDOW_MODE)
    {
        g_rml.pending_window_mode =
            WindowModeOptionFromValue(MikuPan_GetWindowMode());
        g_rml.window_mode_pending_dirty = false;
        FocusElementById("window-mode-picker");
    }
    else if (old_picker == MIKUPAN_PICKER_WINDOW_SIZE)
    {
        const int count = MikuPan_GetWindowSizeOptionCount();
        g_rml.pending_window_size_index =
            count > 0 ? ClampInt(MikuPan_GetSelectedWindowSizeOption(),
                                 0,
                                 count - 1)
                      : 0;
        g_rml.window_size_pending_dirty = false;
        FocusElementById("window-size-picker");
    }
    else if (old_picker == MIKUPAN_PICKER_RESOLUTION)
    {
        const int count = MikuPan_GetRenderResolutionOptionCount();
        g_rml.pending_resolution_index =
            count > 0 ? ClampInt(MikuPan_GetSelectedRenderResolutionOption(),
                                 0,
                                 count - 1)
                      : 0;
        g_rml.resolution_pending_dirty = false;
        FocusElementById("resolution-picker");
    }
    else if (PickerUsesSharedPendingIndex(old_picker))
    {
        const int count = GetPickerCount(old_picker);
        g_rml.pending_picker_index =
            count > 0 ? ClampInt(GetPickerAppliedIndex(old_picker), 0, count - 1)
                      : 0;
    }
    UpdateDisplayPickers();
}

bool AdjustChoicePicker(int direction)
{
    if (direction == 0 || g_rml.active_picker == MIKUPAN_PICKER_NONE)
    {
        return false;
    }

    const int count = GetPickerCount(g_rml.active_picker);
    if (count <= 0)
    {
        return true;
    }

    const int old_index = GetPickerPendingIndex();
    SetPickerPendingIndex(old_index + direction);
    if (GetPickerPendingIndex() != old_index)
    {
        RequestUiMoveSound();
    }
    UpdateDisplayPickers();
    return true;
}

bool HandleVerticalInput(int direction)
{
    if (direction == 0)
    {
        return false;
    }

    if (BindingCaptureIsVisible())
    {
        return true;
    }

    if (g_rml.calibration_visible)
    {
        return HandleCalibrationVerticalInput(direction);
    }

    if (g_rml.crt_visible)
    {
        return HandleCrtVerticalInput(direction);
    }

    if (g_rml.exit_confirm_visible || g_rml.resolution_confirm_visible)
    {
        return true;
    }

    if (g_rml.active_picker != MIKUPAN_PICKER_NONE)
    {
        return AdjustChoicePicker(direction);
    }

    if (AnySelectOpen())
    {
        return false;
    }

    if (g_rml.control_scope == MIKUPAN_CONTROL_SCOPE_CONTENT)
    {
        if (HandleControlsVerticalInput(direction))
        {
            return true;
        }
        return HandleContentVerticalInput(direction);
    }

    return false;
}

bool HandleHorizontalInput(int direction)
{
    if (direction == 0 || g_rml.context == nullptr)
    {
        return false;
    }

    static constexpr const char* kExitButtons[] = {
        "save-changes-yes-button",
        "save-changes-no-button",
        "save-changes-cancel-button",
    };
    static constexpr const char* kDisplayConfirmButtons[] = {
        "resolution-keep-button",
        "resolution-revert-button",
    };

    if (BindingCaptureIsVisible())
    {
        return true;
    }

    if (g_rml.calibration_visible || g_rml.crt_visible)
    {
        MikuPanStepControl* control = FindFocusedStepControl();
        if (control != nullptr)
        {
            AdjustStepControl(*control, direction);
            return true;
        }
        return false;
    }

    if (g_rml.exit_confirm_visible)
    {
        FocusButtonGroup(kExitButtons,
                         static_cast<int>(sizeof(kExitButtons) / sizeof(kExitButtons[0])),
                         direction);
        return true;
    }

    if (g_rml.resolution_confirm_visible)
    {
        FocusButtonGroup(kDisplayConfirmButtons,
                         static_cast<int>(sizeof(kDisplayConfirmButtons)
                                          / sizeof(kDisplayConfirmButtons[0])),
                         direction);
        return true;
    }

    if (g_rml.active_picker != MIKUPAN_PICKER_NONE)
    {
        return AdjustChoicePicker(direction);
    }

    if (AnySelectOpen())
    {
        return false;
    }

    if (HandleControlsTabHorizontalInput(direction))
    {
        return true;
    }

    if (HandleControlsBindingHorizontalInput(direction))
    {
        return true;
    }

    if (g_rml.control_scope == MIKUPAN_CONTROL_SCOPE_SIDEBAR)
    {
        return true;
    }

    MikuPanStepControl* control = FindFocusedStepControl();
    if (control != nullptr)
    {
        AdjustStepControl(*control, direction);
        return true;
    }

    if (direction < 0)
    {
        return true;
    }

    return false;
}

void SetResolutionConfirmVisible(bool visible,
                                 MikuPanDisplayConfirmKind kind = MIKUPAN_DISPLAY_CONFIRM_NONE)
{
    g_rml.resolution_confirm_visible = visible;
    g_rml.display_confirm_kind = visible ? kind : MIKUPAN_DISPLAY_CONFIRM_NONE;
    if (g_rml.resolution_confirm_modal != nullptr)
    {
        g_rml.resolution_confirm_modal->SetClass("hidden", !visible);
    }

    if (visible)
    {
        g_rml.resolution_confirm_deadline_ticks = SDL_GetTicks() + 15000u;
        const char* title = "Keep this render scale?";
        if (kind == MIKUPAN_DISPLAY_CONFIRM_WINDOW_MODE)
        {
            title = "Keep this display mode?";
        }
        else if (kind == MIKUPAN_DISPLAY_CONFIRM_WINDOW_SIZE)
        {
            title = "Keep this window size?";
        }
        SetElementText(g_rml.resolution_confirm_title, title);
    }
    else
    {
        g_rml.resolution_confirm_deadline_ticks = 0;
    }

    if (g_rml.visible)
    {
        const char* restore_focus = "resolution-picker";
        if (kind == MIKUPAN_DISPLAY_CONFIRM_WINDOW_MODE)
        {
            restore_focus = "window-mode-picker";
        }
        else if (kind == MIKUPAN_DISPLAY_CONFIRM_WINDOW_SIZE)
        {
            restore_focus = "window-size-picker";
        }
        FocusElementById(visible ? "resolution-keep-button" : restore_focus);
    }
}

void KeepResolutionChange(void)
{
    const MikuPanDisplayConfirmKind kind = g_rml.display_confirm_kind;
    SetResolutionConfirmVisible(false, kind);
    if (kind == MIKUPAN_DISPLAY_CONFIRM_WINDOW_MODE)
    {
        g_rml.original_window_mode = MikuPan_GetWindowMode();
        g_rml.window_mode_pending_dirty = false;
    }
    else if (kind == MIKUPAN_DISPLAY_CONFIRM_WINDOW_SIZE)
    {
        g_rml.original_window_size_index = MikuPan_GetSelectedWindowSizeOption();
        g_rml.window_size_pending_dirty = false;
    }
    else if (kind == MIKUPAN_DISPLAY_CONFIRM_RESOLUTION)
    {
        g_rml.original_resolution_index = MikuPan_GetSelectedRenderResolutionOption();
        g_rml.resolution_pending_dirty = false;
    }
    UpdateDisplayPickers();
}

void RevertResolutionChange(void)
{
    const MikuPanDisplayConfirmKind kind = g_rml.display_confirm_kind;
    if (kind == MIKUPAN_DISPLAY_CONFIRM_WINDOW_MODE)
    {
        MikuPan_SetWindowMode(g_rml.original_window_mode);
        g_rml.pending_window_mode =
            WindowModeOptionFromValue(g_rml.original_window_mode);
        g_rml.window_mode_pending_dirty = false;
    }
    else if (kind == MIKUPAN_DISPLAY_CONFIRM_WINDOW_SIZE)
    {
        const int count = MikuPan_GetWindowSizeOptionCount();
        if (count > 0)
        {
            g_rml.original_window_size_index = ClampInt(
                g_rml.original_window_size_index,
                0,
                count - 1);
            MikuPan_SelectWindowSizeOption(g_rml.original_window_size_index);
            g_rml.pending_window_size_index = g_rml.original_window_size_index;
        }
        g_rml.window_size_pending_dirty = false;
    }
    else if (kind == MIKUPAN_DISPLAY_CONFIRM_RESOLUTION)
    {
        const int count = MikuPan_GetRenderResolutionOptionCount();
        if (count > 0)
        {
            g_rml.original_resolution_index = ClampInt(
                g_rml.original_resolution_index,
                0,
                count - 1);
            MikuPan_SelectRenderResolutionOption(g_rml.original_resolution_index);
            g_rml.pending_resolution_index = g_rml.original_resolution_index;
        }
        g_rml.resolution_pending_dirty = false;
    }

    g_rml.has_unsaved_changes = g_rml.display_dirty_before_confirm;
    SetResolutionConfirmVisible(false, kind);
    UpdateDisplayPickers();
}

void UpdateResolutionConfirmTimeout(void)
{
    if (!g_rml.resolution_confirm_visible)
    {
        return;
    }

    const uint64_t now = SDL_GetTicks();
    if (now >= g_rml.resolution_confirm_deadline_ticks)
    {
        RevertResolutionChange();
        return;
    }

    const uint64_t remaining_ms = g_rml.resolution_confirm_deadline_ticks - now;
    const int remaining_seconds =
        std::max(1, static_cast<int>((remaining_ms + 999u) / 1000u));
    SetElementText(g_rml.resolution_confirm_countdown,
                   std::to_string(remaining_seconds));
}

bool ApplyPendingWindowMode(void)
{
    g_rml.pending_window_mode = ClampInt(g_rml.pending_window_mode,
                                         0,
                                         kWindowModeCount - 1);
    const int selected = WindowModeOptionFromValue(MikuPan_GetWindowMode());
    if (g_rml.pending_window_mode == selected)
    {
        g_rml.window_mode_pending_dirty = false;
        UpdateDisplayPickers();
        return true;
    }

    g_rml.original_window_mode = MikuPan_GetWindowMode();
    g_rml.display_dirty_before_confirm = g_rml.has_unsaved_changes;
    MikuPan_SetWindowMode(WindowModeValueFromOption(g_rml.pending_window_mode));
    MarkSettingsDirty();
    g_rml.window_mode_pending_dirty = false;
    UpdateDisplayPickers();
    SetResolutionConfirmVisible(true, MIKUPAN_DISPLAY_CONFIRM_WINDOW_MODE);
    return true;
}

bool ApplyPendingWindowSize(void)
{
    const int count = MikuPan_GetWindowSizeOptionCount();
    if (count <= 0)
    {
        return true;
    }

    g_rml.pending_window_size_index = ClampInt(g_rml.pending_window_size_index,
                                               0,
                                               count - 1);
    const int selected = MikuPan_GetSelectedWindowSizeOption();
    if (g_rml.pending_window_size_index == selected)
    {
        g_rml.window_size_pending_dirty = false;
        UpdateDisplayPickers();
        return true;
    }

    g_rml.original_window_size_index = selected;
    g_rml.display_dirty_before_confirm = g_rml.has_unsaved_changes;
    if (!MikuPan_SelectWindowSizeOption(g_rml.pending_window_size_index))
    {
        UpdateDisplayPickers();
        return false;
    }

    MarkSettingsDirty();
    g_rml.window_size_pending_dirty = false;
    UpdateDisplayPickers();
    SetResolutionConfirmVisible(true, MIKUPAN_DISPLAY_CONFIRM_WINDOW_SIZE);
    return true;
}

bool ApplyPendingResolution(void)
{
    const int count = MikuPan_GetRenderResolutionOptionCount();
    if (count <= 0)
    {
        return true;
    }

    g_rml.pending_resolution_index = ClampInt(g_rml.pending_resolution_index,
                                              0,
                                              count - 1);
    const int selected = MikuPan_GetSelectedRenderResolutionOption();
    if (g_rml.pending_resolution_index == selected)
    {
        g_rml.resolution_pending_dirty = false;
        UpdateDisplayPickers();
        return true;
    }

    g_rml.original_resolution_index = selected;
    g_rml.display_dirty_before_confirm = g_rml.has_unsaved_changes;
    if (!MikuPan_SelectRenderResolutionOption(g_rml.pending_resolution_index))
    {
        UpdateDisplayPickers();
        return false;
    }

    MarkSettingsDirty();
    g_rml.resolution_pending_dirty = false;
    UpdateDisplayPickers();
    SetResolutionConfirmVisible(true, MIKUPAN_DISPLAY_CONFIRM_RESOLUTION);
    return true;
}

bool ApplyImmediateChoicePicker(MikuPanChoicePickerKind kind)
{
    const int count = GetPickerCount(kind);
    if (count <= 0 || PickerIsDisabled(kind))
    {
        return false;
    }

    const int index = ClampInt(g_rml.pending_picker_index, 0, count - 1);
    switch (kind)
    {
        case MIKUPAN_PICKER_GPU_BACKEND:
            if (index == MikuPan_GetSelectedGpuDriverOption())
            {
                return true;
            }
            if (MikuPan_SelectGpuDriverOption(index))
            {
                MarkSettingsDirty();
                UpdateGpuDriverNotes();
                return true;
            }
            return false;
        case MIKUPAN_PICKER_MSAA:
            if (index == MikuPan_GetSelectedMSAAOption())
            {
                return true;
            }
            if (MikuPan_SelectMSAAOption(index))
            {
                MarkSettingsDirty();
                return true;
            }
            return false;
        case MIKUPAN_PICKER_SHADOW_RESOLUTION:
            if (index == MikuPan_GetSelectedShadowResolutionOption())
            {
                return true;
            }
            if (MikuPan_SelectShadowResolutionOption(index))
            {
                MarkSettingsDirty();
                return true;
            }
            return false;
        case MIKUPAN_PICKER_LIGHTING_MODE:
            if (index != MikuPan_GetLightingMode())
            {
                MikuPan_SetLightingMode(index);
                MarkSettingsDirty();
            }
            return true;
        case MIKUPAN_PICKER_DITHER_MODE:
            if (index == MikuPan_GetSelectedDitherModeOption())
            {
                return true;
            }
            if (MikuPan_SelectDitherModeOption(index))
            {
                MarkSettingsDirty();
                return true;
            }
            return false;
        case MIKUPAN_PICKER_FINDER_SURROUND:
            if (index != GetFinderSurroundIndex())
            {
                mikupan_configuration.renderer.finder_viewport_mask_mode =
                    index == MIKUPAN_FINDER_VIEWPORT_MASK_BLACK
                        ? MIKUPAN_FINDER_VIEWPORT_MASK_BLACK
                        : MIKUPAN_FINDER_VIEWPORT_MASK_BLUR;
                MarkSettingsDirty();
            }
            return true;
        case MIKUPAN_PICKER_FLASHLIGHT_STYLE:
            if (index == MikuPan_GetSelectedFlashlightStyleOption())
            {
                return true;
            }
            if (MikuPan_SelectFlashlightStyleOption(index))
            {
                MarkSettingsDirty();
                return true;
            }
            return false;
        case MIKUPAN_PICKER_THEME:
            if (index == MikuPan_GetSelectedThemeOption())
            {
                return true;
            }
            if (MikuPan_SelectThemeOption(index))
            {
                ApplyRmlThemeClass(index);
                MarkSettingsDirty();
                return true;
            }
            return false;
        case MIKUPAN_PICKER_FONT:
            if (index == MikuPan_GetSelectedFontOption())
            {
                return true;
            }
            if (MikuPan_SelectFontOption(index))
            {
                MarkSettingsDirty();
                return true;
            }
            return false;
        default:
            return false;
    }
}

void ApplyChoicePicker(void)
{
    const MikuPanChoicePickerKind kind = g_rml.active_picker;
    if (kind == MIKUPAN_PICKER_NONE)
    {
        return;
    }

    SetChoicePickerVisible(false);
    g_rml.active_picker = MIKUPAN_PICKER_NONE;
    if (kind == MIKUPAN_PICKER_WINDOW_MODE)
    {
        (void) ApplyPendingWindowMode();
    }
    else if (kind == MIKUPAN_PICKER_WINDOW_SIZE)
    {
        (void) ApplyPendingWindowSize();
    }
    else if (kind == MIKUPAN_PICKER_RESOLUTION)
    {
        (void) ApplyPendingResolution();
    }
    else if (PickerUsesSharedPendingIndex(kind))
    {
        (void) ApplyImmediateChoicePicker(kind);
    }
    UpdateDisplayPickers();
}

int FindChoicePickerOptionIndex(Rml::Element* element)
{
    static const std::string prefix = "choice-picker-option-";
    while (element != nullptr && element != g_rml.choice_picker_list)
    {
        const Rml::String id = element->GetId();
        if (id.size() > prefix.size()
            && id.compare(0, prefix.size(), prefix) == 0)
        {
            return std::atoi(id.c_str() + prefix.size());
        }
        element = element->GetParentNode();
    }
    return -1;
}

void HandleChoicePickerListClick(Rml::Event& event)
{
    if (g_rml.active_picker == MIKUPAN_PICKER_NONE)
    {
        return;
    }

    int index = FindChoicePickerOptionIndex(event.GetTargetElement());
    if (index < 0)
    {
        index = FindChoicePickerOptionIndex(event.GetCurrentElement());
    }
    if (index < 0)
    {
        return;
    }

    SetPickerPendingIndex(index);
    RequestUiMoveSound();
    ApplyChoicePicker();
}

void AddStepControl(const char* control_id,
                    const char* bars_id,
                    const char* value_id,
                    MikuPanStepControlKind kind,
                    int index,
                    float minimum,
                    float maximum,
                    float requested_step,
                    int precision,
                    bool suffix_x = false,
                    bool suffix_percent = false)
{
    MikuPanStepControl control;
    control.kind = kind;
    control.index = index;
    control.control = GetElement(control_id);
    control.bars = GetElement(bars_id);
    control.value = GetElement(value_id);
    control.min_value = minimum;
    control.max_value = maximum;
    control.visual_min_value = kind == MIKUPAN_STEP_GAMMA ? 0.0f : minimum;
    control.visual_max_value = maximum;
    control.step = requested_step > 0.0f
                       ? requested_step
                       : (maximum - minimum) / 20.0f;
    control.precision = precision;
    control.segments = 21;
    control.neutral_value = 1.0f;
    control.has_neutral = minimum <= control.neutral_value
                          && maximum >= control.neutral_value
                          && kind != MIKUPAN_STEP_AUDIO_MASTER;
    control.suffix_x = suffix_x;
    control.suffix_percent = suffix_percent;
    Rml::Element* control_element = control.control;
    g_rml.step_controls.push_back(control);
    AddListener(control_element,
                Rml::EventId::Click,
                std::make_unique<MikuPanEventListener>(
                    [](Rml::Event& event) { HandleStepControlClick(event); }));
}

Rml::Element* FindSettingRowAncestor(Rml::Element* element)
{
    while (element != nullptr)
    {
        if (element->IsClassSet("setting-row")
            || element->IsClassSet("controls-bind-row"))
        {
            return element;
        }
        element = element->GetParentNode();
    }
    return nullptr;
}

void ClearControllerFocusReferences(void)
{
    g_rml.controller_focus_element = nullptr;
    g_rml.controller_focus_row = nullptr;
}

void ClearControllerFocusVisual(void)
{
    Rml::Element* focus_element = g_rml.controller_focus_element;
    Rml::Element* focus_row = g_rml.controller_focus_row;
    ClearControllerFocusReferences();

    if (focus_element != nullptr)
    {
        focus_element->SetClass("controller-focus", false);
    }

    if (focus_row != nullptr && focus_row != focus_element)
    {
        focus_row->SetClass("controller-focus", false);
    }
}

void UpdateControllerFocusVisual(void)
{
    ClearControllerFocusVisual();
    if (g_rml.context == nullptr)
    {
        return;
    }

    Rml::Element* focus = g_rml.context->GetFocusElement();
    if (focus == nullptr)
    {
        return;
    }

    g_rml.controller_focus_element = focus;
    g_rml.controller_focus_element->SetClass("controller-focus", true);

    Rml::Element* row = FindSettingRowAncestor(focus);
    if (row != nullptr)
    {
        g_rml.controller_focus_row = row;
        g_rml.controller_focus_row->SetClass("controller-focus", true);
    }
}

void ScrollFocusedContentRowIntoView(void)
{
    if (g_rml.context == nullptr || g_rml.control_scope != MIKUPAN_CONTROL_SCOPE_CONTENT
        || g_rml.calibration_visible || g_rml.crt_visible
        || g_rml.exit_confirm_visible || g_rml.resolution_confirm_visible
        || g_rml.active_picker != MIKUPAN_PICKER_NONE)
    {
        return;
    }

    Rml::Element* focus = g_rml.context->GetFocusElement();
    Rml::Element* target = FindSettingRowAncestor(focus);
    if (target != nullptr)
    {
        target->ScrollIntoView(false);
    }
}

bool ElementUsesPageScroll(Rml::Element* element)
{
    while (element != nullptr)
    {
        if (element == g_rml.page_area)
        {
            return true;
        }
        element = element->GetParentNode();
    }
    return false;
}

float GetPageAreaScrollTop(void)
{
    return g_rml.page_area != nullptr ? g_rml.page_area->GetScrollTop() : 0.0f;
}

void RestorePageAreaScrollTop(float scroll_top)
{
    if (g_rml.page_area == nullptr)
    {
        return;
    }

    if (g_rml.document != nullptr)
    {
        g_rml.document->UpdateDocument();
    }
    g_rml.page_area->SetScrollTop(scroll_top);
}

bool FocusElementById(const char* id)
{
    if (g_rml.document == nullptr || g_rml.context == nullptr || id == nullptr)
    {
        return false;
    }

    Rml::Element* element = g_rml.document->GetElementById(id);
    if (element == nullptr)
    {
        return false;
    }

    element->Focus();
    UpdateControllerFocusVisual();
    ScrollFocusedContentRowIntoView();
    return g_rml.context->GetFocusElement() == element;
}

bool FocusElementByIdPreservePageScroll(const char* id)
{
    const float scroll_top = GetPageAreaScrollTop();
    const bool focused = FocusElementById(id);
    RestorePageAreaScrollTop(scroll_top);
    return focused;
}

void QueueFocusElementById(const char* id)
{
    if (id == nullptr || id[0] == '\0')
    {
        return;
    }

    g_rml.queued_focus_id = id;
    g_rml.queued_focus_attempts = 8;
}

void ApplyQueuedFocus(void)
{
    if (g_rml.queued_focus_id.empty() || g_rml.queued_focus_attempts <= 0)
    {
        return;
    }

    const std::string id = g_rml.queued_focus_id;
    if (FocusElementByIdPreservePageScroll(id.c_str()))
    {
        g_rml.queued_focus_id.clear();
        g_rml.queued_focus_attempts = 0;
        return;
    }

    g_rml.queued_focus_attempts--;
    if (g_rml.queued_focus_attempts <= 0)
    {
        g_rml.queued_focus_id.clear();
    }
}

const Rml::String GetFocusedElementId(void)
{
    if (g_rml.context == nullptr)
    {
        return "";
    }

    const Rml::Element* focus = g_rml.context->GetFocusElement();
    return focus != nullptr ? focus->GetId() : "";
}

bool FocusedElementIs(const char* id)
{
    return id != nullptr && GetFocusedElementId() == id;
}

int GetFocusedIdIndex(const char* const* ids, int count)
{
    const Rml::String focus_id = GetFocusedElementId();
    for (int i = 0; i < count; i++)
    {
        if (ids[i] != nullptr && focus_id == ids[i])
        {
            return i;
        }
    }
    return -1;
}

void FocusButtonGroup(const char* const* ids, int count, int direction)
{
    if (count <= 0 || ids == nullptr)
    {
        return;
    }

    int index = GetFocusedIdIndex(ids, count);
    if (index < 0)
    {
        index = 0;
    }
    else
    {
        index = (index + direction + count) % count;
    }

    const Rml::String old_focus_id = GetFocusedElementId();
    FocusElementById(ids[index]);
    if (old_focus_id != ids[index])
    {
        RequestUiMoveSound();
    }
}

bool FocusSubmenuControlList(const char* const* ids, int count, int direction)
{
    if (count <= 0 || ids == nullptr || direction == 0)
    {
        return false;
    }

    int index = GetFocusedIdIndex(ids, count);
    if (index < 0)
    {
        index = direction < 0 ? count - 1 : 0;
    }
    else
    {
        index = (index + direction + count) % count;
    }

    const Rml::String old_focus_id = GetFocusedElementId();
    FocusElementById(ids[index]);

    Rml::Element* focus = g_rml.context != nullptr
                              ? g_rml.context->GetFocusElement()
                              : nullptr;
    if (focus != nullptr)
    {
        focus->ScrollIntoView(false);
    }

    if (old_focus_id != ids[index])
    {
        RequestUiMoveSound();
    }

    return true;
}

bool IsFocusableContentElement(Rml::Element* element)
{
    if (element == nullptr || element->HasAttribute("disabled")
        || element->IsClassSet("hidden"))
    {
        return false;
    }

    const Rml::String tag = element->GetTagName();
    return tag == "button" || tag == "select" || tag == "input";
}

Rml::ElementFormControlSelect* ElementAsNativeSelect(Rml::Element* element)
{
    while (element != nullptr)
    {
        auto* select =
            rmlui_dynamic_cast<Rml::ElementFormControlSelect*>(element);
        if (select != nullptr)
        {
            return select;
        }
        element = element->GetParentNode();
    }

    return nullptr;
}

Rml::ElementFormControlSelect* GetFocusedNativeSelect(void)
{
    return g_rml.context != nullptr
               ? ElementAsNativeSelect(g_rml.context->GetFocusElement())
               : nullptr;
}

bool ControllerNativeSelectOpen(void)
{
    if (g_rml.controller_open_select == nullptr)
    {
        return false;
    }

    if (g_rml.context == nullptr)
    {
        g_rml.controller_open_select = nullptr;
        return false;
    }

    Rml::Element* focus = g_rml.context->GetFocusElement();
    while (focus != nullptr)
    {
        if (focus == g_rml.controller_open_select)
        {
            return true;
        }
        focus = focus->GetParentNode();
    }

    g_rml.controller_open_select = nullptr;
    return false;
}

void CollectFocusableContentElements(Rml::Element* element,
                                     std::vector<Rml::Element*>& elements)
{
    if (element == nullptr || element->IsClassSet("hidden"))
    {
        return;
    }

    if (IsFocusableContentElement(element))
    {
        elements.push_back(element);
    }

    const int child_count = element->GetNumChildren(true);
    for (int i = 0; i < child_count; i++)
    {
        CollectFocusableContentElements(element->GetChild(i), elements);
    }
}

bool ElementContainsDescendant(Rml::Element* ancestor, Rml::Element* element)
{
    while (element != nullptr)
    {
        if (element == ancestor)
        {
            return true;
        }
        element = element->GetParentNode();
    }
    return false;
}

Rml::Element* FindFocusableContentAncestor(Rml::Element* root,
                                           Rml::Element* element)
{
    while (element != nullptr && ElementContainsDescendant(root, element))
    {
        if (IsFocusableContentElement(element))
        {
            return element;
        }
        if (element == root)
        {
            break;
        }
        element = element->GetParentNode();
    }
    return nullptr;
}

float ElementCenterX(Rml::Element* element)
{
    const Rml::Vector2f offset = element->GetAbsoluteOffset();
    return offset.x + static_cast<float>(element->GetOffsetWidth()) * 0.5f;
}

float ElementCenterY(Rml::Element* element)
{
    const Rml::Vector2f offset = element->GetAbsoluteOffset();
    return offset.y + static_cast<float>(element->GetOffsetHeight()) * 0.5f;
}

Rml::Element* FindVisualEdgeElement(const std::vector<Rml::Element*>& elements,
                                    int direction)
{
    if (elements.empty())
    {
        return nullptr;
    }

    Rml::Element* best = elements.front();
    for (Rml::Element* element : elements)
    {
        const float y = ElementCenterY(element);
        const float best_y = ElementCenterY(best);
        const float x = ElementCenterX(element);
        const float best_x = ElementCenterX(best);
        if (direction > 0)
        {
            if (y < best_y - 1.0f
                || (std::abs(y - best_y) <= 1.0f && x < best_x))
            {
                best = element;
            }
        }
        else if (y > best_y + 1.0f
                 || (std::abs(y - best_y) <= 1.0f && x > best_x))
        {
            best = element;
        }
    }
    return best;
}

bool FocusElement(Rml::Element* element)
{
    if (element == nullptr || g_rml.context == nullptr)
    {
        return false;
    }

    Rml::Element* old_focus = g_rml.context->GetFocusElement();
    element->Focus();
    UpdateControllerFocusVisual();
    ScrollFocusedContentRowIntoView();
    element->ScrollIntoView(false);
    if (old_focus != element)
    {
        RequestUiMoveSound();
    }
    return true;
}

struct ControlsFocusRow
{
    std::string primary;
    std::string secondary;
};

void AddControlsFocusRow(std::vector<ControlsFocusRow>& rows,
                         const std::string& primary,
                         const std::string& secondary = {})
{
    if (!primary.empty())
    {
        rows.push_back({primary, secondary});
    }
}

std::vector<ControlsFocusRow> BuildControlsFocusRows(void)
{
    std::vector<ControlsFocusRow> rows;
    AddControlsFocusRow(rows, "movement-dpad-mode");
    AddControlsFocusRow(rows, "movement-stick-mode");
    AddControlsFocusRow(rows, "finder-stick-layout-mode");
    AddControlsFocusRow(rows, "finder-dpad-film-swap-mode");
    rows.push_back({"controls-keyboard-tab", "controls-controller-tab"});

    if (g_rml.controls_tab == MIKUPAN_CONTROLS_TAB_KEYBOARD)
    {
        for (int i = 0; i < MIKUPAN_SPECIAL_ACTION_COUNT; i++)
        {
            const std::string index = std::to_string(i);
            AddControlsFocusRow(rows,
                                "bind-special-" + index,
                                "clear-special-" + index);
            if (i == MIKUPAN_SPECIAL_ACTION_RAISE_CAMERA)
            {
                AddControlsFocusRow(rows, "camera-activation-toggle");
            }
        }

        for (int i = 0; i < MIKUPAN_CONTROLLER_LOGICAL_COUNT; i++)
        {
            const std::string index = std::to_string(i);
            AddControlsFocusRow(rows, "bind-key-" + index, "clear-key-" + index);
        }

        const int move_rows[] = {1, 1, 0, 0, 3, 3, 2, 2};
        const char* move_prefixes[] = {
            "bind-key-stick-neg-",
            "bind-key-stick-pos-",
            "bind-key-stick-neg-",
            "bind-key-stick-pos-",
            "bind-key-stick-neg-",
            "bind-key-stick-pos-",
            "bind-key-stick-neg-",
            "bind-key-stick-pos-",
        };
        const char* clear_prefixes[] = {
            "clear-key-stick-neg-",
            "clear-key-stick-pos-",
            "clear-key-stick-neg-",
            "clear-key-stick-pos-",
            "clear-key-stick-neg-",
            "clear-key-stick-pos-",
            "clear-key-stick-neg-",
            "clear-key-stick-pos-",
        };
        for (int i = 0; i < 8; i++)
        {
            const std::string index = std::to_string(move_rows[i]);
            AddControlsFocusRow(rows,
                                std::string(move_prefixes[i]) + index,
                                std::string(clear_prefixes[i]) + index);
        }
    }
    else
    {
        AddControlsFocusRow(rows, "controller-rumble-toggle");
        for (int i = 0; i < MIKUPAN_CONTROLLER_LOGICAL_COUNT; i++)
        {
            const std::string index = std::to_string(i);
            AddControlsFocusRow(rows, "bind-pad-" + index, "clear-pad-" + index);
        }
        for (int i = 0; i < MIKUPAN_STICK_COUNT; i++)
        {
            const std::string index = std::to_string(i);
            AddControlsFocusRow(rows, "bind-pad-stick-" + index, "clear-pad-stick-" + index);
        }
        for (int i = 0; i < MIKUPAN_STICK_COUNT; i++)
        {
            AddControlsFocusRow(rows, "invert-pad-stick-" + std::to_string(i));
        }
    }

    AddControlsFocusRow(rows, "controls-reset-button");
    return rows;
}

int FindControlsFocusRow(const std::vector<ControlsFocusRow>& rows,
                         const Rml::String& focus_id,
                         int* column)
{
    for (int i = 0; i < static_cast<int>(rows.size()); i++)
    {
        if (rows[i].primary == focus_id.c_str())
        {
            if (column != nullptr)
            {
                *column = 0;
            }
            return i;
        }
        if (!rows[i].secondary.empty() && rows[i].secondary == focus_id.c_str())
        {
            if (column != nullptr)
            {
                *column = 1;
            }
            return i;
        }
    }
    return -1;
}

bool ControlsFocusRowIsTabs(const ControlsFocusRow& row)
{
    return row.primary == "controls-keyboard-tab"
           && row.secondary == "controls-controller-tab";
}

bool HandleControlsVerticalInput(int direction)
{
    if (g_rml.selected_category != 3
        || g_rml.control_scope != MIKUPAN_CONTROL_SCOPE_CONTENT)
    {
        return false;
    }

    std::vector<ControlsFocusRow> rows = BuildControlsFocusRows();
    if (rows.empty())
    {
        return true;
    }

    int column = 0;
    int row_index = FindControlsFocusRow(rows, GetFocusedElementId(), &column);
    if (row_index < 0)
    {
        row_index = direction > 0 ? 0 : static_cast<int>(rows.size()) - 1;
    }
    else
    {
        const bool from_tabs = ControlsFocusRowIsTabs(rows[row_index]);
        row_index = ClampInt(row_index + direction,
                             0,
                             static_cast<int>(rows.size()) - 1);
        if (from_tabs && direction > 0)
        {
            column = 0;
        }
    }

    const ControlsFocusRow& row = rows[row_index];
    const std::string& target = column == 1 && !row.secondary.empty()
                                    ? row.secondary
                                    : row.primary;
    const Rml::String old_focus_id = GetFocusedElementId();
    FocusElementById(target.c_str());
    if (old_focus_id != target.c_str())
    {
        RequestUiMoveSound();
    }
    return true;
}

bool HandleControlsBindingHorizontalInput(int direction)
{
    if (g_rml.selected_category != 3
        || g_rml.control_scope != MIKUPAN_CONTROL_SCOPE_CONTENT)
    {
        return false;
    }

    std::vector<ControlsFocusRow> rows = BuildControlsFocusRows();
    int column = 0;
    const int row_index = FindControlsFocusRow(rows, GetFocusedElementId(), &column);
    if (row_index < 0 || ControlsFocusRowIsTabs(rows[row_index]))
    {
        return false;
    }

    const ControlsFocusRow& row = rows[row_index];
    if (direction > 0 && column == 0 && !row.secondary.empty())
    {
        FocusElementById(row.secondary.c_str());
        RequestUiMoveSound();
    }
    else if (direction < 0 && column == 1)
    {
        FocusElementById(row.primary.c_str());
        RequestUiMoveSound();
    }
    return true;
}

bool HandleContentVerticalInput(int direction)
{
    if (direction == 0 || g_rml.context == nullptr || g_rml.document == nullptr)
    {
        return false;
    }

    const int category = ClampInt(g_rml.selected_category,
                                  0,
                                  static_cast<int>(g_rml.category_pages.size()) - 1);
    Rml::Element* page = g_rml.category_pages[category];
    if (page == nullptr)
    {
        return false;
    }

    g_rml.document->UpdateDocument();

    std::vector<Rml::Element*> elements;
    CollectFocusableContentElements(page, elements);
    if (elements.empty())
    {
        return false;
    }

    Rml::Element* focus =
        FindFocusableContentAncestor(page, g_rml.context->GetFocusElement());
    if (ControllerNativeSelectOpen())
    {
        return false;
    }

    if (focus == nullptr)
    {
        return FocusElement(FindVisualEdgeElement(elements, direction));
    }

    constexpr float kSameRowTolerance = 1.0f;
    const float focus_x = ElementCenterX(focus);
    const float focus_y = ElementCenterY(focus);
    Rml::Element* best = nullptr;
    float best_score = 1000000000.0f;

    for (Rml::Element* element : elements)
    {
        if (element == focus)
        {
            continue;
        }

        const float candidate_x = ElementCenterX(element);
        const float candidate_y = ElementCenterY(element);
        const float delta_y = candidate_y - focus_y;
        if ((direction > 0 && delta_y <= kSameRowTolerance)
            || (direction < 0 && delta_y >= -kSameRowTolerance))
        {
            continue;
        }

        const float row_distance = std::abs(delta_y);
        const float column_distance = std::abs(candidate_x - focus_x);
        const float score = row_distance * 10000.0f + column_distance;
        if (score < best_score)
        {
            best_score = score;
            best = element;
        }
    }

    if (best == nullptr)
    {
        best = FindVisualEdgeElement(elements, direction);
    }

    return FocusElement(best);
}

bool HandleCalibrationVerticalInput(int direction)
{
    static constexpr const char* kCalibrationControls[] = {
        "calibration-brightness-stepper",
        "calibration-gamma-stepper",
        "calibration-contrast-stepper",
        "calibration-shadow-depth-stepper",
        "calibration-reset-button",
        "calibration-ok-button",
        "calibration-back-button",
    };

    return FocusSubmenuControlList(
        kCalibrationControls,
        static_cast<int>(sizeof(kCalibrationControls)
                         / sizeof(kCalibrationControls[0])),
        direction);
}

bool HandleCrtVerticalInput(int direction)
{
    static constexpr const char* kCrtControls[] = {
        "crt-enabled-input",
        "crt-0-stepper",
        "crt-1-stepper",
        "crt-2-stepper",
        "crt-3-stepper",
        "crt-4-stepper",
        "crt-5-stepper",
        "crt-6-stepper",
        "crt-7-stepper",
        "crt-8-stepper",
        "crt-9-stepper",
        "crt-10-stepper",
        "crt-11-stepper",
        "crt-12-stepper",
        "crt-13-stepper",
        "crt-14-stepper",
        "crt-15-stepper",
        "reset-crt-button",
        "crt-ok-button",
        "crt-back-button",
    };

    return FocusSubmenuControlList(
        kCrtControls,
        static_cast<int>(sizeof(kCrtControls) / sizeof(kCrtControls[0])),
        direction);
}

bool HandleControlsTabHorizontalInput(int direction)
{
    static constexpr const char* kControlsTabs[] = {
        "controls-keyboard-tab",
        "controls-controller-tab",
    };

    const int current = GetFocusedIdIndex(
        kControlsTabs,
        static_cast<int>(sizeof(kControlsTabs) / sizeof(kControlsTabs[0])));
    if (current < 0)
    {
        return false;
    }

    const int count = static_cast<int>(sizeof(kControlsTabs) / sizeof(kControlsTabs[0]));
    const int next = (current + direction + count) % count;
    if (next == current)
    {
        return true;
    }

    FocusElementById(kControlsTabs[next]);
    RequestUiMoveSound();
    return true;
}

void FocusSelectedCategory(void)
{
    const int category = ClampInt(g_rml.selected_category,
                                  0,
                                  static_cast<int>(g_rml.category_buttons.size()) - 1);
    FocusElementById(kCategoryButtonIds[category]);
}

void EnterSidebarScope(void)
{
    g_rml.control_scope = MIKUPAN_CONTROL_SCOPE_SIDEBAR;
    FocusSelectedCategory();
}

void EnterContentScope(void)
{
    g_rml.control_scope = MIKUPAN_CONTROL_SCOPE_CONTENT;
    const int category = ClampInt(g_rml.selected_category,
                                  0,
                                  static_cast<int>(g_rml.category_buttons.size()) - 1);
    FocusElementById(kCategoryFirstControlIds[category]);
}

bool AnySelectOpen(void)
{
    if (ControllerNativeSelectOpen())
    {
        return true;
    }

    Rml::ElementFormControlSelect* selects[] = {
        g_rml.gpu_backend_select,
        g_rml.msaa_select,
        g_rml.shadow_resolution_select,
        g_rml.lighting_mode_select,
        g_rml.dither_mode_select,
        g_rml.finder_surround_select,
        g_rml.theme_select,
        g_rml.font_select,
    };

    for (Rml::ElementFormControlSelect* select : selects)
    {
        if (select != nullptr && select->IsPseudoClassSet("checked"))
        {
            return true;
        }
    }

    return false;
}

bool FocusedControlWantsSpaceConfirm(void)
{
    if (g_rml.context == nullptr)
    {
        return false;
    }

    const Rml::Element* focus = g_rml.context->GetFocusElement();
    if (focus == nullptr)
    {
        return false;
    }

    if (focus == g_rml.vsync_input
        || focus == g_rml.crt_enabled_input
        || focus == g_rml.minimap_enabled_input
        || focus == g_rml.keep_finder_raised_input
        || focus == g_rml.finder_dpad_film_swap_input
        || focus == g_rml.mirror_stone_hud_input
        || focus == g_rml.improved_movement_collisions_input
        || focus == g_rml.controller_remap_input)
    {
        return true;
    }

    return false;
}

bool ActivateFocusedControl(void)
{
    if (g_rml.context == nullptr)
    {
        return false;
    }

    Rml::Element* focus = g_rml.context->GetFocusElement();
    if (focus == nullptr)
    {
        if (g_rml.crt_visible)
        {
            FocusElementById("crt-enabled-input");
            return true;
        }
        if (g_rml.calibration_visible)
        {
            FocusElementById("calibration-brightness-stepper");
            return true;
        }
        if (g_rml.exit_confirm_visible)
        {
            FocusElementById("save-changes-yes-button");
            return true;
        }
        if (g_rml.resolution_confirm_visible)
        {
            FocusElementById("resolution-keep-button");
            return true;
        }
        return false;
    }

    if (BindingCaptureIsVisible())
    {
        return true;
    }

    if (Rml::ElementFormControlSelect* select = GetFocusedNativeSelect())
    {
        if (g_rml.controller_open_select == select)
        {
            g_rml.controller_open_select = nullptr;
        }
        else
        {
            g_rml.controller_open_select = select;
        }
        return false;
    }

    if (g_rml.crt_visible)
    {
        if (CrtPanelIsFocused())
        {
            focus->Click();
        }
        else
        {
            FocusElementById("crt-enabled-input");
        }
        return true;
    }

    if (g_rml.calibration_visible)
    {
        if (CalibrationPanelIsFocused())
        {
            focus->Click();
        }
        else
        {
            FocusElementById("calibration-brightness-stepper");
        }
        return true;
    }

    if (g_rml.exit_confirm_visible)
    {
        if (FocusedElementIs("save-changes-yes-button")
            || FocusedElementIs("save-changes-no-button")
            || FocusedElementIs("save-changes-cancel-button"))
        {
            focus->Click();
        }
        else
        {
            FocusElementById("save-changes-yes-button");
        }
        return true;
    }

    if (g_rml.resolution_confirm_visible)
    {
        if (FocusedElementIs("resolution-keep-button")
            || FocusedElementIs("resolution-revert-button"))
        {
            focus->Click();
        }
        else
        {
            FocusElementById("resolution-keep-button");
        }
        return true;
    }

    if (g_rml.active_picker != MIKUPAN_PICKER_NONE)
    {
        if (FocusedElementIs("choice-picker-cancel-button"))
        {
            CloseChoicePicker();
        }
        else
        {
            ApplyChoicePicker();
        }
        return true;
    }

    if (g_rml.control_scope == MIKUPAN_CONTROL_SCOPE_SIDEBAR)
    {
        for (size_t i = 0; i < g_rml.category_buttons.size(); i++)
        {
            if (focus == g_rml.category_buttons[i])
            {
                SelectCategory(static_cast<int>(i));
                EnterContentScope();
                return true;
            }
        }
    }

    if (focus == g_rml.window_mode_picker)
    {
        OpenChoicePicker(MIKUPAN_PICKER_WINDOW_MODE);
        return true;
    }

    if (focus == g_rml.window_size_picker)
    {
        OpenChoicePicker(MIKUPAN_PICKER_WINDOW_SIZE);
        return true;
    }

    if (focus == g_rml.resolution_picker)
    {
        OpenChoicePicker(MIKUPAN_PICKER_RESOLUTION);
        return true;
    }

    if (focus == g_rml.gpu_backend_picker)
    {
        OpenChoicePicker(MIKUPAN_PICKER_GPU_BACKEND);
        return true;
    }

    if (focus == g_rml.msaa_picker)
    {
        OpenChoicePicker(MIKUPAN_PICKER_MSAA);
        return true;
    }

    if (focus == g_rml.shadow_resolution_picker)
    {
        OpenChoicePicker(MIKUPAN_PICKER_SHADOW_RESOLUTION);
        return true;
    }

    if (focus == g_rml.lighting_mode_picker)
    {
        OpenChoicePicker(MIKUPAN_PICKER_LIGHTING_MODE);
        return true;
    }

    if (focus == g_rml.dither_mode_picker)
    {
        OpenChoicePicker(MIKUPAN_PICKER_DITHER_MODE);
        return true;
    }

    if (focus == g_rml.finder_surround_picker)
    {
        OpenChoicePicker(MIKUPAN_PICKER_FINDER_SURROUND);
        return true;
    }

    if (focus == g_rml.flashlight_style_picker)
    {
        OpenChoicePicker(MIKUPAN_PICKER_FLASHLIGHT_STYLE);
        return true;
    }

    if (focus == g_rml.theme_picker)
    {
        OpenChoicePicker(MIKUPAN_PICKER_THEME);
        return true;
    }

    if (focus == g_rml.font_picker)
    {
        OpenChoicePicker(MIKUPAN_PICKER_FONT);
        return true;
    }

    if (FocusedElementIs("controls-keyboard-tab"))
    {
        SelectControlsTab(MIKUPAN_CONTROLS_TAB_KEYBOARD);
        RequestUiMoveSound();
        return true;
    }

    if (FocusedElementIs("controls-controller-tab"))
    {
        SelectControlsTab(MIKUPAN_CONTROLS_TAB_CONTROLLER);
        RequestUiMoveSound();
        return true;
    }

    if (FindFocusedStepControl() != nullptr)
    {
        return true;
    }

    if (focus->GetTagName() == "button" || FocusedControlWantsSpaceConfirm())
    {
        focus->Click();
        return true;
    }

    return false;
}

void HandleOptionsCancel(void)
{
    if (!g_rml.option_mode_open)
    {
        return;
    }

    if (BindingCaptureIsVisible())
    {
        return;
    }

    if (g_rml.crt_visible)
    {
        CancelCrtPanel();
        return;
    }

    if (g_rml.calibration_visible)
    {
        CancelCalibrationPanel();
        return;
    }

    if (g_rml.active_picker != MIKUPAN_PICKER_NONE)
    {
        CloseChoicePicker();
        return;
    }

    if (g_rml.resolution_confirm_visible)
    {
        RevertResolutionChange();
        return;
    }

    if (g_rml.exit_confirm_visible)
    {
        SetExitConfirmVisible(false);
        return;
    }

    if (AnySelectOpen())
    {
        return;
    }

    if (g_rml.control_scope == MIKUPAN_CONTROL_SCOPE_CONTENT)
    {
        EnterSidebarScope();
        return;
    }

    RequestOptionsExit();
}

void RequestOptionsExit(void)
{
    if (!g_rml.option_mode_open)
    {
        return;
    }

    if (g_rml.has_unsaved_changes)
    {
        SetExitConfirmVisible(true);
        return;
    }

    g_rml.option_exit_requested = true;
}

void SetSelectArrow(Rml::ElementFormControlSelect* select)
{
    if (select == nullptr)
    {
        return;
    }

    const char* arrow = select->IsPseudoClassSet("checked")
                            ? "<span class=\"picker-arrow-shape open\"></span>"
                            : "<span class=\"picker-arrow-shape\"></span>";
    for (int i = 0; i < select->GetNumChildren(true); i++)
    {
        Rml::Element* child = select->GetChild(i);
        if (child != nullptr && child->GetTagName() == "selectarrow"
            && child->GetInnerRML() != arrow)
        {
            child->SetInnerRML(arrow);
        }
    }
}

void UpdateSelectArrows(void)
{
    SetSelectArrow(g_rml.gpu_backend_select);
    SetSelectArrow(g_rml.msaa_select);
    SetSelectArrow(g_rml.shadow_resolution_select);
    SetSelectArrow(g_rml.lighting_mode_select);
    SetSelectArrow(g_rml.dither_mode_select);
    SetSelectArrow(g_rml.finder_surround_select);
    SetSelectArrow(g_rml.theme_select);
    SetSelectArrow(g_rml.font_select);
}

void SelectCategory(int category)
{
    if (category < 0
        || category >= static_cast<int>(g_rml.category_pages.size()))
    {
        return;
    }

    g_rml.selected_category = category;
    for (size_t i = 0; i < g_rml.category_pages.size(); i++)
    {
        if (g_rml.category_pages[i] != nullptr)
        {
            g_rml.category_pages[i]->SetClass(
                "active",
                i == static_cast<size_t>(category));
            g_rml.category_pages[i]->SetClass(
                "hidden",
                i != static_cast<size_t>(category));
        }
        if (g_rml.category_buttons[i] != nullptr)
        {
            g_rml.category_buttons[i]->SetClass(
                "selected",
                i == static_cast<size_t>(category));
        }
    }

    if (category < static_cast<int>(sizeof(kCategoryTitles) / sizeof(kCategoryTitles[0])))
    {
        SetElementText(g_rml.panel_title, kCategoryTitles[category]);
    }
}

void PopulateIndexedSelect(Rml::ElementFormControlSelect* select,
                           int count,
                           const char* (*get_label)(int),
                           int selected,
                           std::function<void(int)> on_change,
                           int (*is_enabled)(int) = nullptr)
{
    if (select == nullptr)
    {
        return;
    }

    for (int i = 0; i < count; i++)
    {
        const char* label = get_label != nullptr ? get_label(i) : nullptr;
        const bool enabled = is_enabled == nullptr || is_enabled(i);
        select->Add(label != nullptr ? label : "",
                    std::to_string(i),
                    -1,
                    enabled);
    }

    if (count > 0)
    {
        select->SetSelection(std::max(0, std::min(selected, count - 1)));
    }

    AddListener(select,
                Rml::EventId::Change,
                std::make_unique<MikuPanSelectListener>(
                    [on_change = std::move(on_change)](int index) {
                        MarkSettingsDirty();
                        on_change(index);
                    }));
}

void PopulateStaticSelect(Rml::ElementFormControlSelect* select,
                          const char* const* labels,
                          int count,
                          int selected,
                          std::function<void(int)> on_change)
{
    if (select == nullptr)
    {
        return;
    }

    for (int i = 0; i < count; i++)
    {
        select->Add(labels[i], std::to_string(i));
    }

    if (count > 0)
    {
        select->SetSelection(std::max(0, std::min(selected, count - 1)));
    }

    AddListener(select,
                Rml::EventId::Change,
                std::make_unique<MikuPanSelectListener>(
                    [on_change = std::move(on_change)](int index) {
                        MarkSettingsDirty();
                        on_change(index);
                    }));
}

void SyncRmlSettingsValues(void)
{
    if (g_rml.document == nullptr)
    {
        return;
    }

    g_rml.syncing = true;
    UpdateDisplayPickers();
    if (g_rml.gpu_backend_select != nullptr)
    {
        g_rml.gpu_backend_select->SetSelection(
            MikuPan_GetSelectedGpuDriverOption());
    }
    UpdateMsaaSelectState();
    if (g_rml.shadow_resolution_select != nullptr)
    {
        g_rml.shadow_resolution_select->SetSelection(
            MikuPan_GetSelectedShadowResolutionOption());
    }
    if (g_rml.lighting_mode_select != nullptr)
    {
        g_rml.lighting_mode_select->SetSelection(MikuPan_GetLightingMode());
    }
    if (g_rml.dither_mode_select != nullptr)
    {
        g_rml.dither_mode_select->SetSelection(
            MikuPan_GetSelectedDitherModeOption());
    }
    if (g_rml.finder_surround_select != nullptr)
    {
        g_rml.finder_surround_select->SetSelection(
            mikupan_configuration.renderer.finder_viewport_mask_mode ==
                    MIKUPAN_FINDER_VIEWPORT_MASK_BLACK
                ? MIKUPAN_FINDER_VIEWPORT_MASK_BLACK
                : MIKUPAN_FINDER_VIEWPORT_MASK_BLUR);
    }
    if (g_rml.theme_select != nullptr)
    {
        g_rml.theme_select->SetSelection(MikuPan_GetSelectedThemeOption());
    }
    ApplyRmlThemeClass(MikuPan_GetSelectedThemeOption());
    if (g_rml.font_select != nullptr)
    {
        g_rml.font_select->SetSelection(MikuPan_GetSelectedFontOption());
    }
    ApplyRmlFontClass(MikuPan_GetSelectedFontOption());

    UpdateStepControlVisuals();
    RebuildControlsList();
    SyncMovementStyleButtons();
    SyncViewfinderButtons();
    SetCheckbox(g_rml.vsync_input, MikuPan_IsVsync());
    UpdateGpuDriverNotes();
    const MikuPan_ConfigCrt* crt = MikuPan_GetCrtSettings();
    SyncHdrCheckboxes();
    SetCheckbox(g_rml.crt_enabled_input, crt != nullptr && crt->enabled);
    SetCheckbox(g_rml.minimap_enabled_input,
                mikupan_configuration.minimap_enabled);
    SetCheckbox(g_rml.keep_finder_raised_input,
                MikuPan_KeepFinderRaisedForApparitionsEnabled());
    SetCheckbox(g_rml.finder_dpad_film_swap_input,
                MikuPan_FinderDpadFilmSwapEnabled());
    SetCheckbox(g_rml.mirror_stone_hud_input,
                MikuPan_MirrorStoneHudEnabled());
    SetCheckbox(g_rml.improved_movement_collisions_input,
                MikuPan_ImprovedMovementCollisionsEnabled());

    SetCheckbox(g_rml.controller_remap_input,
                MikuPan_ShowControllerRemapWindow());

    if (g_rml.data_folder_input != nullptr
        && g_rml.context->GetFocusElement() != g_rml.data_folder_input)
    {
        g_rml.data_folder_input->SetValue(MikuPan_GetDataFolder());
    }

    SetElementText(g_rml.save_status, MikuPan_GetConfigSaveStatus());
    UpdateSelectArrows();
    g_rml.syncing = false;
}

bool LoadOptionsDocument(void)
{
    char document_path[1024];
    if (!MikuPan_ResolveBasePath("resources/rml/options.rml",
                                 document_path,
                                 sizeof(document_path)))
    {
        return false;
    }

    g_rml.document = g_rml.context->LoadDocument(document_path);
    if (g_rml.document == nullptr)
    {
        return false;
    }

    g_rml.root = GetElement("options-root");
    g_rml.page_area = GetElement("page-area");

    g_rml.window_mode_picker = GetElement("window-mode-picker");
    g_rml.window_mode_choice = GetElement("window-mode-choice");
    g_rml.window_mode_value = GetElement("window-mode-value");
    g_rml.window_mode_pending_note = GetElement("window-mode-pending-note");
    g_rml.pending_window_mode =
        WindowModeOptionFromValue(MikuPan_GetWindowMode());
    g_rml.original_window_mode = MikuPan_GetWindowMode();
    AddListener(g_rml.window_mode_picker,
                Rml::EventId::Click,
                std::make_unique<MikuPanButtonListener>(
                    []() { OpenChoicePicker(MIKUPAN_PICKER_WINDOW_MODE); }));

    g_rml.window_size_picker = GetElement("window-size-picker");
    g_rml.window_size_choice = GetElement("window-size-choice");
    g_rml.window_size_value = GetElement("window-size-value");
    g_rml.window_size_pending_note = GetElement("window-size-pending-note");
    g_rml.pending_window_size_index = MikuPan_GetSelectedWindowSizeOption();
    g_rml.original_window_size_index = g_rml.pending_window_size_index;
    AddListener(g_rml.window_size_picker,
                Rml::EventId::Click,
                std::make_unique<MikuPanButtonListener>(
                    []() { OpenChoicePicker(MIKUPAN_PICKER_WINDOW_SIZE); }));

    g_rml.resolution_picker = GetElement("resolution-picker");
    g_rml.resolution_choice = GetElement("resolution-choice");
    g_rml.resolution_value = GetElement("resolution-value");
    g_rml.resolution_pending_note = GetElement("resolution-pending-note");
    g_rml.pending_resolution_index = MikuPan_GetSelectedRenderResolutionOption();
    g_rml.original_resolution_index = g_rml.pending_resolution_index;
    AddListener(g_rml.resolution_picker,
                Rml::EventId::Click,
                std::make_unique<MikuPanButtonListener>(
                    []() { OpenChoicePicker(MIKUPAN_PICKER_RESOLUTION); }));

    g_rml.gpu_backend_picker = GetElement("gpu-backend-picker");
    g_rml.gpu_backend_choice = GetElement("gpu-backend-choice");
    AddListener(g_rml.gpu_backend_picker,
                Rml::EventId::Click,
                std::make_unique<MikuPanButtonListener>(
                    []() { OpenChoicePicker(MIKUPAN_PICKER_GPU_BACKEND); }));

    g_rml.msaa_picker = GetElement("msaa-picker");
    g_rml.msaa_choice = GetElement("msaa-choice");
    AddListener(g_rml.msaa_picker,
                Rml::EventId::Click,
                std::make_unique<MikuPanButtonListener>(
                    []() { OpenChoicePicker(MIKUPAN_PICKER_MSAA); }));

    g_rml.shadow_resolution_picker = GetElement("shadow-resolution-picker");
    g_rml.shadow_resolution_choice = GetElement("shadow-resolution-choice");
    AddListener(g_rml.shadow_resolution_picker,
                Rml::EventId::Click,
                std::make_unique<MikuPanButtonListener>([]() {
                    OpenChoicePicker(MIKUPAN_PICKER_SHADOW_RESOLUTION);
                }));

    g_rml.lighting_mode_picker = GetElement("lighting-mode-picker");
    g_rml.lighting_mode_choice = GetElement("lighting-mode-choice");
    AddListener(g_rml.lighting_mode_picker,
                Rml::EventId::Click,
                std::make_unique<MikuPanButtonListener>(
                    []() { OpenChoicePicker(MIKUPAN_PICKER_LIGHTING_MODE); }));

    g_rml.dither_mode_picker = GetElement("dither-mode-picker");
    g_rml.dither_mode_choice = GetElement("dither-mode-choice");
    AddListener(g_rml.dither_mode_picker,
                Rml::EventId::Click,
                std::make_unique<MikuPanButtonListener>(
                    []() { OpenChoicePicker(MIKUPAN_PICKER_DITHER_MODE); }));

    g_rml.finder_surround_picker = GetElement("finder-surround-picker");
    g_rml.finder_surround_choice = GetElement("finder-surround-choice");
    AddListener(g_rml.finder_surround_picker,
                Rml::EventId::Click,
                std::make_unique<MikuPanButtonListener>(
                    []() { OpenChoicePicker(MIKUPAN_PICKER_FINDER_SURROUND); }));

    AddStepControl("calibration-brightness-stepper",
                   "calibration-brightness-bars",
                   "calibration-brightness-value",
                   MIKUPAN_STEP_BRIGHTNESS,
                   -1,
                   0.0f,
                   2.0f,
                   0.1f,
                   2);
    AddStepControl("calibration-gamma-stepper",
                   "calibration-gamma-bars",
                   "calibration-gamma-value",
                   MIKUPAN_STEP_GAMMA,
                   -1,
                   0.1f,
                   2.0f,
                   0.1f,
                   2);
    AddStepControl("calibration-contrast-stepper",
                   "calibration-contrast-bars",
                   "calibration-contrast-value",
                   MIKUPAN_STEP_CONTRAST,
                   -1,
                   0.0f,
                   2.0f,
                   0.1f,
                   1);
    AddStepControl("calibration-shadow-depth-stepper",
                   "calibration-shadow-depth-bars",
                   "calibration-shadow-depth-value",
                   MIKUPAN_STEP_SHADOW_DEPTH,
                   -1,
                   0.0f,
                   2.0f,
                   0.1f,
                   1);
    g_rml.calibration_hdr_enabled_input =
        GetInput("calibration-hdr-enabled-input");
    AddListener(g_rml.calibration_hdr_enabled_input,
                Rml::EventId::Change,
                std::make_unique<MikuPanInputListener>(
                    [](Rml::ElementFormControlInput* input) {
                        MarkSettingsDirty();
                        MikuPan_SetHdrEnabled(IsCheckboxChecked(input));
                    }));
    AddStepControl("calibration-hdr-paper-white-stepper",
                   "calibration-hdr-paper-white-bars",
                   "calibration-hdr-paper-white-value",
                   MIKUPAN_STEP_HDR_PAPER_WHITE,
                   -1,
                   80.0f,
                   400.0f,
                   10.0f,
                   0);
    AddStepControl("calibration-hdr-peak-luminance-stepper",
                   "calibration-hdr-peak-luminance-bars",
                   "calibration-hdr-peak-luminance-value",
                   MIKUPAN_STEP_HDR_PEAK_LUMINANCE,
                   -1,
                   100.0f,
                   4000.0f,
                   50.0f,
                   0);

    AddListener(GetElement("open-calibration-button"),
                Rml::EventId::Click,
                std::make_unique<MikuPanButtonListener>(
                    []() { OpenCalibrationPanel(); }));
    AddListener(GetElement("open-crt-button"),
                Rml::EventId::Click,
                std::make_unique<MikuPanButtonListener>(
                    []() { OpenCrtPanel(); }));
    AddListener(GetElement("calibration-reset-button"),
                Rml::EventId::Click,
                std::make_unique<MikuPanButtonListener>(
                    []() { ResetCalibrationDefaults(); }));
    AddListener(GetElement("calibration-ok-button"),
                Rml::EventId::Click,
                std::make_unique<MikuPanButtonListener>(
                    []() { AcceptCalibrationPanel(); }));
    AddListener(GetElement("calibration-back-button"),
                Rml::EventId::Click,
                std::make_unique<MikuPanButtonListener>(
                    []() { CancelCalibrationPanel(); }));

    g_rml.vsync_input = GetInput("vsync-input");
    AddListener(g_rml.vsync_input,
                Rml::EventId::Change,
                std::make_unique<MikuPanInputListener>(
                    [](Rml::ElementFormControlInput* input) {
                        MarkSettingsDirty();
                        MikuPan_SetVsync(IsCheckboxChecked(input));
                    }));

    g_rml.hdr_enabled_input = GetInput("hdr-enabled-input");
    AddListener(g_rml.hdr_enabled_input,
                Rml::EventId::Change,
                std::make_unique<MikuPanInputListener>(
                    [](Rml::ElementFormControlInput* input) {
                        MarkSettingsDirty();
                        MikuPan_SetHdrEnabled(IsCheckboxChecked(input));
                    }));

    g_rml.gpu_backend_select = GetSelect("gpu-backend-select");
    PopulateIndexedSelect(g_rml.gpu_backend_select,
                          MikuPan_GetGpuDriverOptionCount(),
                          MikuPan_GetGpuDriverOptionLabel,
                          MikuPan_GetSelectedGpuDriverOption(),
                          [](int index) { MikuPan_SelectGpuDriverOption(index); },
                          MikuPan_IsGpuDriverOptionSupported);
    g_rml.active_gpu_label = GetElement("active-gpu-label");
    g_rml.gpu_restart_note = GetElement("gpu-restart-note");

    g_rml.msaa_select = GetSelect("msaa-select");
    PopulateIndexedSelect(g_rml.msaa_select,
                          MikuPan_GetMSAAOptionCount(),
                          MikuPan_GetMSAAOptionLabel,
                          MikuPan_GetSelectedMSAAOption(),
                          [](int index) {
                              MikuPan_SelectMSAAOption(index);
                              UpdateMsaaSelectState();
                          });

    g_rml.shadow_resolution_select = GetSelect("shadow-resolution-select");
    PopulateIndexedSelect(g_rml.shadow_resolution_select,
                          MikuPan_GetShadowResolutionOptionCount(),
                          MikuPan_GetShadowResolutionOptionLabel,
                          MikuPan_GetSelectedShadowResolutionOption(),
                          [](int index) {
                              MikuPan_SelectShadowResolutionOption(index);
                          });

    g_rml.lighting_mode_select = GetSelect("lighting-mode-select");
    PopulateStaticSelect(g_rml.lighting_mode_select,
                         kLightingModeLabels,
                         2,
                         MikuPan_GetLightingMode(),
                         [](int index) { MikuPan_SetLightingMode(index); });

    g_rml.dither_mode_select = GetSelect("dither-mode-select");
    PopulateIndexedSelect(g_rml.dither_mode_select,
                          MikuPan_GetDitherModeOptionCount(),
                          MikuPan_GetDitherModeOptionLabel,
                          MikuPan_GetSelectedDitherModeOption(),
                          [](int index) { MikuPan_SelectDitherModeOption(index); });

    g_rml.finder_surround_select = GetSelect("finder-surround-select");
    PopulateStaticSelect(
        g_rml.finder_surround_select,
        kFinderSurroundLabels,
        2,
        mikupan_configuration.renderer.finder_viewport_mask_mode ==
                MIKUPAN_FINDER_VIEWPORT_MASK_BLACK
            ? MIKUPAN_FINDER_VIEWPORT_MASK_BLACK
            : MIKUPAN_FINDER_VIEWPORT_MASK_BLUR,
        [](int index) {
            mikupan_configuration.renderer.finder_viewport_mask_mode =
                index == MIKUPAN_FINDER_VIEWPORT_MASK_BLACK
                    ? MIKUPAN_FINDER_VIEWPORT_MASK_BLACK
                    : MIKUPAN_FINDER_VIEWPORT_MASK_BLUR;
        });

    AddStepControl("master-volume-stepper",
                   "master-volume-bars",
                   "master-volume-value",
                   MIKUPAN_STEP_AUDIO_MASTER,
                   -1,
                   0.0f,
                   1.0f,
                   0.05f,
                   0,
                   false,
                   true);

    g_rml.flashlight_style_picker = GetElement("flashlight-style-picker");
    g_rml.flashlight_style_choice = GetElement("flashlight-style-choice");
    AddListener(g_rml.flashlight_style_picker,
                Rml::EventId::Click,
                std::make_unique<MikuPanButtonListener>([]() {
                    OpenChoicePicker(MIKUPAN_PICKER_FLASHLIGHT_STYLE);
                }));

    g_rml.theme_picker = GetElement("theme-picker");
    g_rml.theme_choice = GetElement("theme-choice");
    AddListener(g_rml.theme_picker,
                Rml::EventId::Click,
                std::make_unique<MikuPanButtonListener>(
                    []() { OpenChoicePicker(MIKUPAN_PICKER_THEME); }));

    g_rml.font_picker = GetElement("font-picker");
    g_rml.font_choice = GetElement("font-choice");
    AddListener(g_rml.font_picker,
                Rml::EventId::Click,
                std::make_unique<MikuPanButtonListener>(
                    []() { OpenChoicePicker(MIKUPAN_PICKER_FONT); }));

    g_rml.theme_select = GetSelect("theme-select");
    PopulateIndexedSelect(g_rml.theme_select,
                          MikuPan_GetThemeOptionCount(),
                          MikuPan_GetThemeOptionLabel,
                          MikuPan_GetSelectedThemeOption(),
                          [](int index) { MikuPan_SelectThemeOption(index); });

    g_rml.font_select = GetSelect("font-select");
    PopulateIndexedSelect(g_rml.font_select,
                          MikuPan_GetFontOptionCount(),
                          MikuPan_GetFontOptionLabel,
                          MikuPan_GetSelectedFontOption(),
                          [](int index) { MikuPan_SelectFontOption(index); });

    AddStepControl("font-scale-stepper",
                   "font-scale-bars",
                   "font-scale-value",
                   MIKUPAN_STEP_FONT_SCALE,
                   -1,
                   0.75f,
                   1.5f,
                   0.05f,
                   2,
                   true);

    g_rml.crt_enabled_input = GetInput("crt-enabled-input");
    AddListener(g_rml.crt_enabled_input,
                Rml::EventId::Change,
                std::make_unique<MikuPanInputListener>(
                    [](Rml::ElementFormControlInput* input) {
                        MarkSettingsDirty();
                        MikuPan_SetCrtEnabled(IsCheckboxChecked(input));
                    }));

    const int crt_count = MikuPan_GetCrtFieldCount();
    for (int i = 0; i < crt_count; i++)
    {
        const std::string control_id = "crt-" + std::to_string(i) + "-stepper";
        const std::string bars_id = "crt-" + std::to_string(i) + "-bars";
        const std::string value_id = "crt-" + std::to_string(i) + "-value";
        const float minimum = MikuPan_GetCrtFieldMin(i);
        const float maximum = MikuPan_GetCrtFieldMax(i);
        const float field_step = MikuPan_GetCrtFieldStep(i);
        const int precision = field_step < 0.01f ? 3 : 2;
        AddStepControl(control_id.c_str(),
                       bars_id.c_str(),
                       value_id.c_str(),
                       MIKUPAN_STEP_CRT_FIELD,
                       i,
                       minimum,
                       maximum,
                       field_step,
                       precision);
    }

    AddListener(GetElement("reset-crt-button"),
                Rml::EventId::Click,
                std::make_unique<MikuPanButtonListener>(
                    []() { ResetCrtDefaults(); }));
    AddListener(GetElement("crt-ok-button"),
                Rml::EventId::Click,
                std::make_unique<MikuPanButtonListener>(
                    []() { AcceptCrtPanel(); }));
    AddListener(GetElement("crt-back-button"),
                Rml::EventId::Click,
                std::make_unique<MikuPanButtonListener>(
                    []() { CancelCrtPanel(); }));

    g_rml.minimap_enabled_input = GetInput("minimap-enabled-input");
    AddListener(g_rml.minimap_enabled_input,
                Rml::EventId::Change,
                std::make_unique<MikuPanInputListener>(
                    [](Rml::ElementFormControlInput* input) {
                        MarkSettingsDirty();
                        mikupan_configuration.minimap_enabled =
                            IsCheckboxChecked(input) ? 1 : 0;
                    }));

    g_rml.keep_finder_raised_input = GetInput("keep-finder-raised-input");
    AddListener(g_rml.keep_finder_raised_input,
                Rml::EventId::Change,
                std::make_unique<MikuPanInputListener>(
                    [](Rml::ElementFormControlInput* input) {
                        MarkSettingsDirty();
                        MikuPan_SetKeepFinderRaisedForApparitionsEnabled(
                            IsCheckboxChecked(input));
                    }));

    g_rml.finder_dpad_film_swap_input = GetInput("finder-dpad-film-swap-input");
    AddListener(g_rml.finder_dpad_film_swap_input,
                Rml::EventId::Change,
                std::make_unique<MikuPanInputListener>(
                    [](Rml::ElementFormControlInput* input) {
                        const float scroll_top = GetPageAreaScrollTop();
                        MarkSettingsDirty();
                        MikuPan_SetFinderDpadFilmSwapEnabled(
                            IsCheckboxChecked(input));
                        SyncViewfinderButtons();
                        RestorePageAreaScrollTop(scroll_top);
                    }));

    g_rml.mirror_stone_hud_input = GetInput("mirror-stone-hud-input");
    AddListener(g_rml.mirror_stone_hud_input,
                Rml::EventId::Change,
                std::make_unique<MikuPanInputListener>(
                    [](Rml::ElementFormControlInput* input) {
                        MarkSettingsDirty();
                        MikuPan_SetMirrorStoneHudEnabled(
                            IsCheckboxChecked(input));
                    }));

    g_rml.improved_movement_collisions_input =
        GetInput("improved-movement-collisions-input");
    AddListener(g_rml.improved_movement_collisions_input,
                Rml::EventId::Change,
                std::make_unique<MikuPanInputListener>(
                    [](Rml::ElementFormControlInput* input) {
                        MarkSettingsDirty();
                        MikuPan_SetImprovedMovementCollisionsEnabled(
                            IsCheckboxChecked(input));
                    }));

    g_rml.controls_keyboard_tab = GetElement("controls-keyboard-tab");
    g_rml.controls_controller_tab = GetElement("controls-controller-tab");
    g_rml.controls_list = GetElement("controls-list");
    AddListener(g_rml.controls_keyboard_tab,
                Rml::EventId::Click,
                std::make_unique<MikuPanButtonListener>([]() {
                    SelectControlsTab(MIKUPAN_CONTROLS_TAB_KEYBOARD);
                    FocusElementByIdPreservePageScroll("controls-keyboard-tab");
                    RequestUiMoveSound();
                }));
    AddListener(g_rml.controls_controller_tab,
                Rml::EventId::Click,
                std::make_unique<MikuPanButtonListener>([]() {
                    SelectControlsTab(MIKUPAN_CONTROLS_TAB_CONTROLLER);
                    FocusElementByIdPreservePageScroll("controls-controller-tab");
                    RequestUiMoveSound();
                }));
    AddListener(GetElement("movement-dpad-mode"),
                Rml::EventId::Click,
                std::make_unique<MikuPanButtonListener>(
                    []() { ToggleMovementStyle(1); }));
    AddListener(GetElement("movement-stick-mode"),
                Rml::EventId::Click,
                std::make_unique<MikuPanButtonListener>(
                    []() { ToggleMovementStyle(0); }));
    AddListener(GetElement("finder-stick-layout-mode"),
                Rml::EventId::Click,
                std::make_unique<MikuPanButtonListener>(
                    []() { ToggleFinderStickLayout(); }));
    AddListener(GetElement("finder-dpad-film-swap-mode"),
                Rml::EventId::Click,
                std::make_unique<MikuPanButtonListener>(
                    []() { ToggleFinderDpadFilmSwap(); }));
    AddListener(g_rml.controls_list,
                Rml::EventId::Click,
                std::make_unique<MikuPanEventListener>(
                    [](Rml::Event& event) { HandleControlsListClick(event); }),
                true);
    AddListener(GetElement("controls-reset-button"),
                Rml::EventId::Click,
                std::make_unique<MikuPanButtonListener>([]() {
                    const float scroll_top = GetPageAreaScrollTop();
                    MarkSettingsDirty();
                    MikuPan_ResetControllerBindingsFromUi();
                    SyncMovementStyleButtons();
                    SyncViewfinderButtons();
                    SetCheckbox(g_rml.finder_dpad_film_swap_input,
                                MikuPan_FinderDpadFilmSwapEnabled());
                    SetCheckbox(g_rml.mirror_stone_hud_input,
                                MikuPan_MirrorStoneHudEnabled());
                    RebuildControlsList();
                    RestorePageAreaScrollTop(scroll_top);
                    RequestUiMoveSound();
                }));

    g_rml.data_folder_input = GetInput("data-folder-input");
    AddListener(g_rml.data_folder_input,
                Rml::EventId::Change,
                std::make_unique<MikuPanInputListener>(
                    [](Rml::ElementFormControlInput* input) {
                        MarkSettingsDirty();
                        MikuPan_SetDataFolder(input->GetValue().c_str());
                    }));
    AddListener(GetElement("browse-folder-button"),
                Rml::EventId::Click,
                std::make_unique<MikuPanButtonListener>([]() {
                    MarkSettingsDirty();
                    MikuPan_BrowseDataFolderFromUi();
                }));
    AddListener(GetElement("save-return-button"),
                Rml::EventId::Click,
                std::make_unique<MikuPanButtonListener>(
                    []() { SaveAndReturnOptions(); }));
    AddListener(GetElement("cancel-options-button"),
                Rml::EventId::Click,
                std::make_unique<MikuPanButtonListener>(
                    []() { RequestOptionsExit(); }));
    AddListener(GetElement("save-changes-yes-button"),
                Rml::EventId::Click,
                std::make_unique<MikuPanButtonListener>([]() {
                    if (SaveOptionsChanges())
                    {
                        SetExitConfirmVisible(false);
                        g_rml.option_exit_requested = true;
                    }
                }));
    AddListener(GetElement("save-changes-no-button"),
                Rml::EventId::Click,
                std::make_unique<MikuPanButtonListener>([]() {
                    g_rml.has_unsaved_changes = false;
                    SetExitConfirmVisible(false);
                    g_rml.option_exit_requested = true;
                }));
    AddListener(GetElement("save-changes-cancel-button"),
                Rml::EventId::Click,
                std::make_unique<MikuPanButtonListener>(
                    []() { SetExitConfirmVisible(false); }));
    g_rml.window = GetElement("window");
    g_rml.film_title = GetElement("options-film-title");
    g_rml.calibration_panel = GetElement("calibration-panel");
    g_rml.calibration_level_panel = GetElement("calibration-level-panel");
    for (size_t i = 0; i < g_rml.calibration_level_swatches.size(); i++)
    {
        const std::string id = "calibration-level-" + std::to_string(i);
        g_rml.calibration_level_swatches[i] = GetElement(id.c_str());
    }
    g_rml.save_status = GetElement("save-status");
    g_rml.panel_title = GetElement("panel-title");
    g_rml.confirm_save_modal = GetElement("confirm-save-modal");
    g_rml.choice_picker_modal = GetElement("choice-picker-modal");
    g_rml.choice_picker_title = GetElement("choice-picker-title");
    g_rml.choice_picker_list = GetElement("choice-picker-list");
    g_rml.binding_capture_modal = GetElement("binding-capture-modal");
    g_rml.binding_capture_title = GetElement("binding-capture-title");
    g_rml.binding_capture_text = GetElement("binding-capture-text");
    g_rml.binding_capture_countdown = GetElement("binding-capture-countdown");
    AddListener(g_rml.choice_picker_list,
                Rml::EventId::Click,
                std::make_unique<MikuPanEventListener>(
                    [](Rml::Event& event) { HandleChoicePickerListClick(event); }),
                true);
    AddListener(GetElement("choice-picker-apply-button"),
                Rml::EventId::Click,
                std::make_unique<MikuPanButtonListener>([]() {
                    ApplyChoicePicker();
                }));
    AddListener(GetElement("choice-picker-cancel-button"),
                Rml::EventId::Click,
                std::make_unique<MikuPanButtonListener>([]() {
                    CloseChoicePicker();
                }));
    g_rml.crt_panel = GetElement("crt-panel");
    g_rml.resolution_confirm_modal = GetElement("resolution-confirm-modal");
    g_rml.resolution_confirm_title = GetElement("resolution-confirm-title");
    g_rml.resolution_confirm_countdown =
        GetElement("resolution-confirm-countdown");
    AddListener(GetElement("resolution-keep-button"),
                Rml::EventId::Click,
                std::make_unique<MikuPanButtonListener>(
                    []() { KeepResolutionChange(); }));
    AddListener(GetElement("resolution-revert-button"),
                Rml::EventId::Click,
                std::make_unique<MikuPanButtonListener>(
                    []() { RevertResolutionChange(); }));

    for (size_t i = 0; i < g_rml.category_buttons.size(); i++)
    {
        g_rml.category_buttons[i] = GetElement(kCategoryButtonIds[i]);
        g_rml.category_pages[i] = GetElement(kCategoryPageIds[i]);
        AddListener(g_rml.category_buttons[i],
                    Rml::EventId::Click,
                    std::make_unique<MikuPanButtonListener>(
                        [i]() {
                            SelectCategory(static_cast<int>(i));
                            EnterContentScope();
                        }));
    }
    SelectControlsTab(g_rml.controls_tab);
    SelectCategory(g_rml.selected_category);

    SyncRmlSettingsValues();
    g_rml.document->Hide();
    return true;
}
} // namespace

bool MikuPan_RmlOptionsInit(Rml::Context* context)
{
    if (g_rml.initialized)
    {
        return true;
    }

    g_rml.context = context;
    if (g_rml.context == nullptr || !LoadOptionsDocument())
    {
        g_rml = MikuPanRmlOptionsState();
        return false;
    }

    g_rml.initialized = true;
    return true;
}

void MikuPan_RmlOptionsStartFrame(void)
{
    if (!g_rml.initialized || g_rml.context == nullptr)
    {
        return;
    }

    if (g_rml.visible)
    {
        ApplyQueuedFocus();
        UpdateResolutionConfirmTimeout();
        UpdateSelectArrows();
        UpdateBindingCapture();
    }
}

void MikuPan_RmlOptionsPrepareShutdown(void)
{
    // The OS window-close path may enter Rml::Shutdown() while the options
    // document is still open. Cached Rml::Element pointers become invalid as
    // the document tree is destroyed, so only drop our references here.
    //
    // Keep g_rml.listeners alive until after Rml::Shutdown(). RmlUi calls
    // EventListener::OnDetach() while destroying elements, so deleting our
    // listener objects before that point leaves dangling listener pointers in
    // RmlUi's EventDispatcher.
    ClearControllerFocusReferences();
    g_rml.visible = false;
    g_rml.option_mode_open = false;
    g_rml.option_exit_requested = false;
    g_rml.active_picker = MIKUPAN_PICKER_NONE;
    g_rml.resolution_confirm_visible = false;
    g_rml.crt_visible = false;
    g_rml.controller_open_select = nullptr;
}

void MikuPan_RmlOptionsShutdown(void)
{
    ClearControllerFocusReferences();
    g_rml = MikuPanRmlOptionsState();
}

static void MikuPan_RmlOptionsOpenInternal(bool in_game)
{
    if (!g_rml.initialized || g_rml.document == nullptr)
    {
        return;
    }

    g_rml.option_mode_open = true;
    g_rml.input_block_until_ticks = SDL_GetTicks() + 250u;
    g_rml.option_exit_requested = false;
    g_rml.has_unsaved_changes = false;
    g_rml.active_picker = MIKUPAN_PICKER_NONE;
    g_rml.controller_open_select = nullptr;
    g_rml.control_scope = MIKUPAN_CONTROL_SCOPE_SIDEBAR;
    g_rml.window_mode_pending_dirty = false;
    g_rml.window_size_pending_dirty = false;
    g_rml.resolution_pending_dirty = false;
    g_rml.resolution_confirm_visible = false;
    SetCalibrationVisible(false);
    SetCrtPanelVisible(false);
    CancelBindingCapture();
    g_rml.controls_tab = MIKUPAN_CONTROLS_TAB_KEYBOARD;
    SelectControlsTab(g_rml.controls_tab);
    g_rml.selected_category = 0;
    g_rml.pending_window_mode =
        WindowModeOptionFromValue(MikuPan_GetWindowMode());
    g_rml.original_window_mode = MikuPan_GetWindowMode();
    g_rml.pending_window_size_index = MikuPan_GetSelectedWindowSizeOption();
    g_rml.original_window_size_index = g_rml.pending_window_size_index;
    g_rml.pending_resolution_index = MikuPan_GetSelectedRenderResolutionOption();
    g_rml.original_resolution_index = g_rml.pending_resolution_index;
    SetExitConfirmVisible(false);
    SetChoicePickerVisible(false);
    SetResolutionConfirmVisible(false);
    if (g_rml.window != nullptr)
    {
        g_rml.window->SetClass("ingame", in_game);
    }
    if (g_rml.film_title != nullptr)
    {
        g_rml.film_title->SetClass("ingame", in_game);
    }
    SelectCategory(0);
    g_rml.control_scope = MIKUPAN_CONTROL_SCOPE_SIDEBAR;
    g_rml.visible = true;
    SyncRmlSettingsValues();
    g_rml.document->Show(Rml::ModalFlag::None, Rml::FocusFlag::Auto);
    EnterSidebarScope();
}

extern "C" {

void MikuPan_RmlOptionsClose(void)
{
    if (!g_rml.initialized || g_rml.document == nullptr)
    {
        return;
    }

    g_rml.option_mode_open = false;
    g_rml.option_exit_requested = false;
    g_rml.has_unsaved_changes = false;
    g_rml.active_picker = MIKUPAN_PICKER_NONE;
    g_rml.controller_open_select = nullptr;
    g_rml.control_scope = MIKUPAN_CONTROL_SCOPE_SIDEBAR;
    g_rml.window_mode_pending_dirty = false;
    g_rml.window_size_pending_dirty = false;
    g_rml.resolution_pending_dirty = false;
    SetCalibrationVisible(false);
    SetCrtPanelVisible(false);
    CancelBindingCapture();
    SetExitConfirmVisible(false);
    SetChoicePickerVisible(false);
    SetResolutionConfirmVisible(false);
    ClearControllerFocusVisual();
    g_rml.visible = false;
    if (g_rml.window != nullptr)
    {
        g_rml.window->SetClass("ingame", false);
    }
    if (g_rml.film_title != nullptr)
    {
        g_rml.film_title->SetClass("ingame", false);
    }
    g_rml.document->Hide();
}

void MikuPan_RmlOptionsOpen(void)
{
    MikuPan_RmlOptionsOpenInternal(false);
}

void MikuPan_RmlOptionsOpenInGame(void)
{
    MikuPan_RmlOptionsOpenInternal(true);
}

int MikuPan_RmlOptionsConsumeExitRequest(void)
{
    const int requested = g_rml.option_exit_requested ? 1 : 0;
    g_rml.option_exit_requested = false;
    return requested;
}

int MikuPan_RmlOptionsIsOpen(void)
{
    return g_rml.option_mode_open ? 1 : 0;
}

int MikuPan_RmlOptionsIsCalibrationOpen(void)
{
    return g_rml.calibration_visible ? 1 : 0;
}

void MikuPan_RmlOptionsRequestExit(void)
{
    if (g_rml.option_mode_open)
    {
        RequestOptionsExit();
    }
}

void MikuPan_RmlOptionsHandleCancel(void)
{
    if (IsOptionsInputBlocked())
    {
        return;
    }
    HandleOptionsCancel();
}

int MikuPan_RmlOptionsAnySelectOpen(void)
{
    if (g_rml.exit_confirm_visible
        || g_rml.resolution_confirm_visible
        || g_rml.active_picker != MIKUPAN_PICKER_NONE)
    {
        return 0;
    }

    return AnySelectOpen() ? 1 : 0;
}

void MikuPan_RmlOptionsClearNativeSelectOpen(void)
{
    g_rml.controller_open_select = nullptr;
}


int MikuPan_RmlOptionsActivateFocusedControl(void)
{
    if (IsOptionsInputBlocked())
    {
        return 1;
    }
    return ActivateFocusedControl() ? 1 : 0;
}

int MikuPan_RmlOptionsHandleHorizontalInput(int direction)
{
    if (IsOptionsInputBlocked())
    {
        return 1;
    }
    return HandleHorizontalInput(direction) ? 1 : 0;
}

int MikuPan_RmlOptionsHandleVerticalInput(int direction)
{
    if (IsOptionsInputBlocked())
    {
        return 1;
    }
    return HandleVerticalInput(direction) ? 1 : 0;
}

void MikuPan_RmlOptionsEnsureFocusedControlVisible(void)
{
    UpdateControllerFocusVisual();
    ScrollFocusedContentRowIntoView();
}

int MikuPan_RmlOptionsInputBlocked(void)
{
    return IsOptionsInputBlocked() ? 1 : 0;
}

int MikuPan_RmlOptionsBindingCaptureActive(void)
{
    return BindingCaptureIsVisible() ? 1 : 0;
}

int MikuPan_RmlOptionsConsumeMoveSoundRequest(void)
{
    const int requested = g_rml.ui_move_sound_requested ? 1 : 0;
    g_rml.ui_move_sound_requested = false;
    return requested;
}

void MikuPan_RmlOptionsApplyTheme(int theme)
{
    ApplyRmlThemeClass(theme);
}

void MikuPan_RmlOptionsApplyFont(int font)
{
    ApplyRmlFontClass(font);
}

void MikuPan_RmlOptionsToggleDebug(void)
{
    if (!g_rml.initialized || g_rml.document == nullptr
        || g_rml.option_mode_open)
    {
        return;
    }

    if (g_rml.visible)
    {
        MikuPan_RmlOptionsClose();
    }
    else
    {
        MikuPan_RmlOptionsOpen();
        g_rml.option_mode_open = false;
    }
}
}
