#include "mikupan/ui/mikupan_rml_options.h"

#include "mikupan/ui/mikupan_ui.h"
#include "mikupan/io/mikupan_file.h"

#include "RmlUi/Core.h"
#include "RmlUi/Core/ElementDocument.h"
#include "RmlUi/Core/EventListener.h"
#include "RmlUi/Core/Elements/ElementFormControlInput.h"
#include "RmlUi/Core/Elements/ElementFormControlSelect.h"
#include "SDL3/SDL_timer.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace
{
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

        on_change(select->GetSelection());
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

        on_change(input);
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
        (void) event;
        on_click();
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
    bool suffix_x = false;
    bool suffix_percent = false;
};

enum MikuPanChoicePickerKind
{
    MIKUPAN_PICKER_NONE = 0,
    MIKUPAN_PICKER_WINDOW_MODE,
    MIKUPAN_PICKER_WINDOW_SIZE,
    MIKUPAN_PICKER_RESOLUTION,
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

struct MikuPanRmlOptionsState
{
    Rml::Context* context = nullptr;
    std::vector<std::unique_ptr<Rml::EventListener>> listeners;
    Rml::ElementDocument* document = nullptr;
    Rml::Element* window = nullptr;
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
    Rml::ElementFormControlSelect* theme_select = nullptr;
    Rml::ElementFormControlSelect* font_select = nullptr;
    Rml::ElementFormControlInput* vsync_input = nullptr;
    Rml::Element* active_gpu_label = nullptr;
    Rml::Element* gpu_restart_note = nullptr;
    Rml::ElementFormControlInput* crt_enabled_input = nullptr;
    Rml::ElementFormControlInput* controller_remap_input = nullptr;
    Rml::ElementFormControlInput* data_folder_input = nullptr;
    Rml::Element* save_status = nullptr;
    Rml::Element* panel_title = nullptr;
    Rml::Element* confirm_save_modal = nullptr;
    std::vector<MikuPanStepControl> step_controls;
    std::array<Rml::Element*, 4> category_buttons{};
    std::array<Rml::Element*, 4> category_pages{};
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
    bool syncing = false;
    bool ui_move_sound_requested = false;
    Rml::Element* controller_focus_element = nullptr;
    Rml::Element* controller_focus_row = nullptr;
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

    if (enabled)
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

void AddListener(Rml::Element* element,
                 Rml::EventId event_id,
                 std::unique_ptr<Rml::EventListener> listener)
{
    if (element == nullptr || listener == nullptr)
    {
        return;
    }

    element->AddEventListener(event_id, listener.get());
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
    "ADVANCED",
};

static constexpr const char* kCategoryButtonIds[] = {
    "cat-display",
    "cat-graphics",
    "cat-audio",
    "cat-advanced",
};

static constexpr const char* kCategoryPageIds[] = {
    "page-display",
    "page-graphics",
    "page-audio",
    "page-advanced",
};

static constexpr const char* kCategoryFirstControlIds[] = {
    "window-mode-picker",
    "resolution-picker",
    "master-volume-stepper",
    "theme-select",
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

void ScrollFocusedContentRowIntoView(void);
void FocusElementById(const char* id);
void FocusButtonGroup(const char* const* ids, int count, int direction);
void EnterSidebarScope(void);
void EnterContentScope(void);
bool AnySelectOpen(void);
bool FocusedControlWantsSpaceConfirm(void);
bool ActivateFocusedControl(void);
bool HandleHorizontalInput(int direction);
void HandleOptionsCancel(void);
void RequestOptionsExit(void);
void UpdateDisplayPickers(void);
void UpdateStepControlVisuals(void);
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

bool IsOptionsInputBlocked(void)
{
    return g_rml.option_mode_open
           && SDL_GetTicks() < g_rml.input_block_until_ticks;
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

float GetStepValue(const MikuPanStepControl& control)
{
    switch (control.kind)
    {
        case MIKUPAN_STEP_BRIGHTNESS:
            return MikuPan_GetBrightness();
        case MIKUPAN_STEP_GAMMA:
            return MikuPan_GetGamma();
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

std::string BuildStepperBars(float value,
                             float minimum,
                             float maximum,
                             int segments)
{
    segments = std::max(1, segments);
    const float range = std::max(maximum - minimum, 0.0001f);
    const float normalised = ClampFloat((value - minimum) / range, 0.0f, 1.0f);
    const int filled = ClampInt(
        static_cast<int>(std::round(normalised * static_cast<float>(segments))),
        0,
        segments);

    std::string rml;
    rml.reserve(static_cast<size_t>(segments) * 44u);
    for (int i = 0; i < segments; i++)
    {
        rml += i < filled ? "<span class=\"stepper-segment filled\">|</span>"
                          : "<span class=\"stepper-segment\">|</span>";
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
                                    control.min_value,
                                    control.max_value,
                                    control.segments));
}

void UpdateStepControlVisuals(void)
{
    for (MikuPanStepControl& control : g_rml.step_controls)
    {
        UpdateStepControlVisual(control);
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

void AdjustStepControl(MikuPanStepControl& control, int direction)
{
    if (direction == 0)
    {
        return;
    }

    const float old_value = GetStepValue(control);
    const float new_value = ClampFloat(old_value + control.step * direction,
                                       control.min_value,
                                       control.max_value);
    if (std::abs(new_value - old_value) < 0.00001f)
    {
        return;
    }

    MarkSettingsDirty();
    SetStepValue(control, new_value);
    UpdateStepControlVisual(control);
    RequestUiMoveSound();
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
            return 0;
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
            return MikuPan_GetWindowSizeOptionLabel(index);
        case MIKUPAN_PICKER_RESOLUTION:
            return MikuPan_GetRenderResolutionOptionLabel(index);
        default:
            return "";
    }
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
            break;
    }
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

    const char* picker_title = "RENDER SCALE";
    if (g_rml.active_picker == MIKUPAN_PICKER_WINDOW_MODE)
    {
        picker_title = "DISPLAY MODE";
    }
    else if (g_rml.active_picker == MIKUPAN_PICKER_WINDOW_SIZE)
    {
        picker_title = "WINDOW SIZE";
    }
    SetElementText(g_rml.choice_picker_title, picker_title);

    if (count <= 0)
    {
        SetElementText(g_rml.choice_picker_list,
                       "<button class=\"picker-option selected\">Unavailable</button>");
        return;
    }

    int first = std::max(0, pending - 2);
    int last = std::min(count - 1, first + 4);
    first = std::max(0, last - 4);

    std::string rml;
    if (first > 0)
    {
        rml += "<div class=\"picker-option ellipsis\">...</div>";
    }
    for (int i = first; i <= last; i++)
    {
        const char* label = GetPickerLabel(g_rml.active_picker, i);
        rml += "<button id=\"choice-picker-option-" + std::to_string(i)
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
        rml += label != nullptr ? label : "";
        if (i == applied)
        {
            rml += " *";
        }
        rml += "</button>";
    }
    if (last < count - 1)
    {
        rml += "<div class=\"picker-option ellipsis\">...</div>";
    }

    SetElementText(g_rml.choice_picker_list, rml);
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

void UpdateDisplayPickers(void)
{
    UpdateWindowModePicker();
    UpdateWindowSizePicker();
    UpdateResolutionPicker();
    UpdateChoicePickerVisual();
}

void SetChoicePickerVisible(bool visible)
{
    if (g_rml.choice_picker_modal != nullptr)
    {
        g_rml.choice_picker_modal->SetClass("hidden", !visible);
    }

    if (g_rml.visible)
    {
        if (visible)
        {
            FocusElementById("choice-picker-apply-button");
        }
        else if (g_rml.active_picker == MIKUPAN_PICKER_WINDOW_MODE)
        {
            FocusElementById("window-mode-picker");
        }
        else if (g_rml.active_picker == MIKUPAN_PICKER_WINDOW_SIZE)
        {
            FocusElementById("window-size-picker");
        }
        else if (g_rml.active_picker == MIKUPAN_PICKER_RESOLUTION)
        {
            FocusElementById("resolution-picker");
        }
    }
}

void OpenChoicePicker(MikuPanChoicePickerKind kind)
{
    if (kind == MIKUPAN_PICKER_NONE)
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

    UpdateChoicePickerVisual();
    SetChoicePickerVisible(true);
    UpdateDisplayPickers();
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
    UpdateDisplayPickers();
}

void HandleChoicePickerListClick(Rml::Event& event)
{
    if (g_rml.active_picker == MIKUPAN_PICKER_NONE)
    {
        return;
    }

    Rml::Element* target = event.GetTargetElement();
    if (target == nullptr)
    {
        return;
    }

    const Rml::String id = target->GetId();
    static const std::string prefix = "choice-picker-option-";
    if (id.size() <= prefix.size() || id.compare(0, prefix.size(), prefix) != 0)
    {
        return;
    }

    const int index = std::atoi(id.c_str() + prefix.size());
    SetPickerPendingIndex(index);
    UpdateDisplayPickers();
    FocusElementById("choice-picker-apply-button");
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
    control.step = std::max(requested_step,
                            (maximum - minimum) / 20.0f);
    control.precision = precision;
    control.segments = 12;
    control.suffix_x = suffix_x;
    control.suffix_percent = suffix_percent;
    g_rml.step_controls.push_back(control);
}

Rml::Element* FindSettingRowAncestor(Rml::Element* element)
{
    while (element != nullptr)
    {
        if (element->IsClassSet("setting-row"))
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

void FocusElementById(const char* id)
{
    if (g_rml.document == nullptr || id == nullptr)
    {
        return;
    }

    Rml::Element* element = g_rml.document->GetElementById(id);
    if (element != nullptr)
    {
        element->Focus();
        UpdateControllerFocusVisual();
        ScrollFocusedContentRowIntoView();
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
    Rml::ElementFormControlSelect* selects[] = {
        g_rml.gpu_backend_select,
        g_rml.msaa_select,
        g_rml.shadow_resolution_select,
        g_rml.lighting_mode_select,
        g_rml.dither_mode_select,
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

    const char* arrow = select->IsPseudoClassSet("checked") ? "^" : "v";
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
    if (g_rml.msaa_select != nullptr)
    {
        g_rml.msaa_select->SetSelection(MikuPan_GetSelectedMSAAOption());
    }
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
    if (g_rml.theme_select != nullptr)
    {
        g_rml.theme_select->SetSelection(MikuPan_GetSelectedThemeOption());
    }
    if (g_rml.font_select != nullptr)
    {
        g_rml.font_select->SetSelection(MikuPan_GetSelectedFontOption());
    }

    UpdateStepControlVisuals();
    SetCheckbox(g_rml.vsync_input, MikuPan_IsVsync());
    SetElementText(g_rml.active_gpu_label,
                   std::string("Active: ")
                       + MikuPan_GetActiveGpuDriverLabel());
    SetElementText(g_rml.gpu_restart_note,
                   MikuPan_IsGpuDriverRestartPending()
                       ? "Save Configuration and restart to apply"
                       : "");
    const MikuPan_ConfigCrt* crt = MikuPan_GetCrtSettings();
    SetCheckbox(g_rml.crt_enabled_input, crt != nullptr && crt->enabled);

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

    static constexpr const char* kLightingModeLabels[] = {
        "Pixel (Modern)",
        "Vertex (PS2)",
    };

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

    AddStepControl("brightness-stepper",
                   "brightness-bars",
                   "brightness-value",
                   MIKUPAN_STEP_BRIGHTNESS,
                   -1,
                   0.0f,
                   2.0f,
                   0.1f,
                   2);
    AddStepControl("gamma-stepper",
                   "gamma-bars",
                   "gamma-value",
                   MIKUPAN_STEP_GAMMA,
                   -1,
                   0.1f,
                   3.0f,
                   0.1f,
                   2);

    g_rml.vsync_input = GetInput("vsync-input");
    AddListener(g_rml.vsync_input,
                Rml::EventId::Change,
                std::make_unique<MikuPanInputListener>(
                    [](Rml::ElementFormControlInput* input) {
                        MarkSettingsDirty();
                        MikuPan_SetVsync(IsCheckboxChecked(input));
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
                          [](int index) { MikuPan_SelectMSAAOption(index); });

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
                std::make_unique<MikuPanButtonListener>([]() {
                    MarkSettingsDirty();
                    MikuPan_ResetCrtSettings();
                }));

    g_rml.controller_remap_input = GetInput("controller-remap-input");
    AddListener(g_rml.controller_remap_input,
                Rml::EventId::Change,
                std::make_unique<MikuPanInputListener>(
                    [](Rml::ElementFormControlInput* input) {
                        MarkSettingsDirty();
                        MikuPan_SetControllerRemapWindowVisible(
                            IsCheckboxChecked(input));
                    }));
    AddListener(GetElement("reset-bindings-button"),
                Rml::EventId::Click,
                std::make_unique<MikuPanButtonListener>([]() {
                    MarkSettingsDirty();
                    MikuPan_ResetControllerBindingsFromUi();
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
    g_rml.save_status = GetElement("save-status");
    g_rml.panel_title = GetElement("panel-title");
    g_rml.confirm_save_modal = GetElement("confirm-save-modal");
    g_rml.choice_picker_modal = GetElement("choice-picker-modal");
    g_rml.choice_picker_title = GetElement("choice-picker-title");
    g_rml.choice_picker_list = GetElement("choice-picker-list");
    AddListener(g_rml.choice_picker_list,
                Rml::EventId::Click,
                std::make_unique<MikuPanEventListener>(
                    [](Rml::Event& event) { HandleChoicePickerListClick(event); }));
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
        SyncRmlSettingsValues();
        UpdateResolutionConfirmTimeout();
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
    g_rml.control_scope = MIKUPAN_CONTROL_SCOPE_SIDEBAR;
    g_rml.window_mode_pending_dirty = false;
    g_rml.window_size_pending_dirty = false;
    g_rml.resolution_pending_dirty = false;
    g_rml.resolution_confirm_visible = false;
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
    g_rml.control_scope = MIKUPAN_CONTROL_SCOPE_SIDEBAR;
    g_rml.window_mode_pending_dirty = false;
    g_rml.window_size_pending_dirty = false;
    g_rml.resolution_pending_dirty = false;
    SetExitConfirmVisible(false);
    SetChoicePickerVisible(false);
    SetResolutionConfirmVisible(false);
    ClearControllerFocusVisual();
    g_rml.visible = false;
    if (g_rml.window != nullptr)
    {
        g_rml.window->SetClass("ingame", false);
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

int MikuPan_RmlOptionsConsumeMoveSoundRequest(void)
{
    const int requested = g_rml.ui_move_sound_requested ? 1 : 0;
    g_rml.ui_move_sound_requested = false;
    return requested;
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
