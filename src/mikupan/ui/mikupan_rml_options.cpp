#include "mikupan/ui/mikupan_rml_options.h"
#include "mikupan/ui/mikupan_rml_save_load.h"

#include "main/glob.h"
#include "mikupan/ui/mikupan_ui.h"
#include "mikupan/mikupan_config.h"
#include "mikupan/mikupan_utils.h"
#include "mikupan/io/mikupan_file.h"
#include "mikupan/io/mikupan_controller.h"
#include "mikupan/gameplay/mikupan_item_icon_hud.h"
#include "mikupan/mikupan_i18n.h"
#include "os/key_cnf.h"

#include "RmlUi/Core.h"
#include "SDL3/SDL_gamepad.h"
#include "SDL3/SDL_keyboard.h"
#include "SDL3/SDL_scancode.h"
#include "SDL3/SDL_timer.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

int SeStartFix(int se_no, unsigned short fin_spd, unsigned short vol_max,
               unsigned short pitch, unsigned char menu);

/*
 * The options screen, in the manner of MioPan's: one column of rows in
 * sections, each row one table entry -- a choice stepped with left and
 * right, a slider, an action, or a binding -- with the help for the row under
 * the cursor and the button prompts below.  Changes apply at once and are
 * saved to mikupan.ini when the screen is left.
 *
 * Three things keep their own flow:
 *  - the window mode, window size and render resolution wait for Enter and
 *    then ask to be kept, reverting by themselves after 15 seconds;
 *  - image calibration and the CRT filter open a side panel with the room
 *    left in view, and Back there puts every value back;
 *  - a binding row waits for the key or button to bind.
 */

namespace
{
constexpr const char* kDocumentPath = "resources/rml/options.rml";

/* UTF-8 for the step arrows either side of a value. */
constexpr const char* kArrowLeft = "\xE2\x80\xB9";
constexpr const char* kArrowRight = "\xE2\x80\xBA";

/* The stock menu sounds from iop/se/iopse.h, as mikupan_rmlui.cpp plays
 * them. */
constexpr int kSeCursor = 0; /* SE_CSR0 */
constexpr int kSeDecide = 1; /* SE_CLIC */
constexpr int kSeCancel = 3; /* SE_CANCEL */

constexpr uint64_t kDisplayRevertMs = 15000u;
constexpr uint64_t kCaptureTimeoutMs = 5000u;
constexpr uint64_t kOpenInputBlockMs = 250u;

using Text = std::function<std::string()>;

void PlaySe(int se_no)
{
    (void) SeStartFix(se_no, 0, 0x1000, 0x1000, 1);
}

void PlayCursor(void)
{
    PlaySe(kSeCursor);
}

void PlayDecide(void)
{
    PlaySe(kSeDecide);
}

void PlayCancel(void)
{
    PlaySe(kSeCancel);
}

std::string T(const char* msgid)
{
    const char* text = MikuPan_Translate(msgid);
    return text != nullptr ? text : "";
}

std::string Escape(const std::string& text)
{
    std::string out;
    out.reserve(text.size());
    for (const char c : text)
    {
        switch (c)
        {
            case '&':
                out += "&amp;";
                break;
            case '<':
                out += "&lt;";
                break;
            case '>':
                out += "&gt;";
                break;
            case '"':
                out += "&quot;";
                break;
            default:
                out += c;
                break;
        }
    }
    return out;
}

std::string Printf(const char* format, double value)
{
    char text[96];
    std::snprintf(text, sizeof(text), format, value);
    return text;
}

/* ---------------------------------------------------------------------------
 *  Rows.
 * ------------------------------------------------------------------------ */

enum class RowKind
{
    Heading,
    Note,
    Choice,
    Slider,
    Action,
    Binding,
};

struct Row
{
    RowKind kind = RowKind::Heading;
    std::string label;
    Text help;
    /* Greyed and passed over by the cursor while this says no. */
    std::function<bool()> enabled;
    /* A note is only there while this says yes. */
    std::function<bool()> shown;

    /* Choice: an index into names, or name_of / count_of when the choices
     * come from the settings code.  choosable skips the ones that cannot be
     * picked (a GPU driver this machine lacks). */
    std::vector<std::string> names;
    std::function<std::string(int)> name_of;
    std::function<int()> count_of;
    std::function<bool(int)> choosable;
    std::function<int()> get;
    std::function<void(int)> set;
    /* The window and render size: the arrows move a pending choice, Enter
     * applies it and asks for it to be kept. */
    bool deferred = false;
    int pending = -1;

    /* Slider.  `neutral` puts a tick on the track where the setting sits by
     * default. */
    std::function<float()> get_value;
    std::function<void(float)> set_value;
    float min = 0.0f;
    float max = 1.0f;
    float step = 0.05f;
    float neutral = -1.0f;
    bool has_neutral = false;
    std::function<std::string(float)> format;

    /* Action.  action_text, when set, is shown in place of `action` and
     * follows whatever it reports; plain shows it as text, not a button. */
    std::string action;
    Text action_text;
    bool plain = false;
    std::function<void()> run;

    /* Binding: what it is bound to, and how to bind or clear it. */
    Text binding_text;
    std::function<bool()> bound;
    std::function<void()> capture;
    std::function<void()> clear;

    /* Built elements, and what they show, so a refresh only touches what
     * changed. */
    Rml::Element* element = nullptr;
    Rml::Element* value = nullptr;
    Rml::Element* track = nullptr;
    Rml::Element* fill = nullptr;
    std::string shown_value;
    float shown_fill = -1.0f;
    int shown_enabled = -1;
    int shown_flag = -1;

    bool IsEnabled() const
    {
        return !enabled || enabled();
    }
    bool Selectable() const
    {
        return kind != RowKind::Heading && kind != RowKind::Note && IsEnabled();
    }
    int Count() const
    {
        return count_of ? count_of() : static_cast<int>(names.size());
    }
    std::string NameOf(int index) const
    {
        if (name_of)
        {
            return name_of(index);
        }
        return index >= 0 && index < static_cast<int>(names.size())
                   ? names[static_cast<size_t>(index)]
                   : "";
    }
    bool Choosable(int index) const
    {
        return !choosable || choosable(index);
    }
};

struct Page
{
    const char* id;
    const char* name;
    std::vector<Row> rows;
    Rml::Element* element = nullptr;
    Rml::Element* tab = nullptr;
    int selected = -1;
};

struct RowRef
{
    Page* page = nullptr;
    int row = -1;
};

enum PanelKind
{
    PANEL_NONE = -1,
    PANEL_CALIBRATION = 0,
    PANEL_CRT = 1,
};

enum CaptureKind
{
    CAPTURE_NONE = 0,
    CAPTURE_SPECIAL_ACTION,
    CAPTURE_KEYBOARD_BUTTON,
    CAPTURE_KEYBOARD_STICK_NEG,
    CAPTURE_KEYBOARD_STICK_POS,
    CAPTURE_CONTROLLER_BUTTON,
    CAPTURE_CONTROLLER_STICK_AXIS,
};

/* The yes / no window, which also carries the display countdown and the
 * binding capture (with the choices hidden). */
struct Confirm
{
    Rml::Element* root = nullptr;
    Rml::Element* text = nullptr;
    Rml::Element* note = nullptr;
    Rml::Element* choices = nullptr;
    Rml::Element* yes = nullptr;
    Rml::Element* no = nullptr;
    bool open = false;
    bool yes_selected = false;
    std::function<void()> on_yes;
    std::function<void()> on_no;
    /* Answers no by itself when this passes, the seconds left in the note. */
    uint64_t deadline = 0;
    std::string note_text;
    std::string shown_note;
};

struct Capture
{
    CaptureKind kind = CAPTURE_NONE;
    int target = -1;
    std::string label;
    uint64_t after_ticks = 0;
    uint64_t deadline_ticks = 0;
    bool waiting_for_release = false;
};

class Listener final : public Rml::EventListener
{
public:
    void ProcessEvent(Rml::Event& event) override;
};

struct OptionsState
{
    Rml::Context* context = nullptr;
    Rml::ElementDocument* document = nullptr;
    Rml::Element* body = nullptr;
    Rml::Element* film_strip = nullptr;
    bool film_applied = false;
    Rml::Element* title = nullptr;
    Rml::Element* tabs = nullptr;
    Rml::Element* pages_root = nullptr;
    Rml::Element* help = nullptr;
    Rml::Element* prompts = nullptr;
    Rml::Element* panel = nullptr;
    Rml::Element* panel_title = nullptr;
    Rml::Element* panel_rows = nullptr;
    Rml::Element* panel_help = nullptr;
    Rml::Element* panel_prompts = nullptr;
    Rml::Element* levels = nullptr;
    std::array<Rml::Element*, 16> level_swatches{};
    std::vector<Page> pages;
    std::vector<Page> panels;
    std::unordered_map<Rml::Element*, RowRef> row_of;
    int page = 0;
    int panel_kind = PANEL_NONE;
    Confirm confirm;
    Capture capture;
    bool initialized = false;
    bool visible = false;
    bool open = false;
    bool in_game = false;
    bool exit_requested = false;
    bool dirty = false;
    bool using_pad = false;
    bool pointer_active = false;
    uint64_t input_block_until = 0;
    RowRef dragging;
    std::string shown_help;
    std::string shown_prompts;
    std::string shown_panel_help;
    std::string shown_panel_prompts;

    /* What a panel puts back when it is left with Back. */
    float calibration_brightness = 1.0f;
    float calibration_gamma = 1.0f;
    float calibration_contrast = 1.0f;
    float calibration_shadow_depth = 1.0f;
    int calibration_hdr = 0;
    float calibration_paper_white = 203.0f;
    float calibration_peak = 1000.0f;
    MikuPan_ConfigCrt crt = {};
    bool panel_dirty_before = false;
};

OptionsState g_opt;
Listener g_listener;

Row Heading(const char* label)
{
    Row row;
    row.kind = RowKind::Heading;
    row.label = label[0] != '\0' ? T(label) : "";
    return row;
}

Row Note(const char* text, std::function<bool()> shown)
{
    Row row;
    row.kind = RowKind::Note;
    row.label = T(text);
    row.shown = std::move(shown);
    return row;
}

Text Fixed(const char* msgid)
{
    return [msgid] { return T(msgid); };
}

Row Choice(const char* label, Text help, std::vector<std::string> names,
           std::function<int()> get, std::function<void(int)> set)
{
    Row row;
    row.kind = RowKind::Choice;
    row.label = T(label);
    row.help = std::move(help);
    for (std::string& name : names)
    {
        name = T(name.c_str());
    }
    row.names = std::move(names);
    row.get = std::move(get);
    row.set = std::move(set);
    return row;
}

/* A choice whose names and count come from the settings code. */
Row ListChoice(const char* label, Text help, std::function<int()> count,
               std::function<const char*(int)> name, std::function<int()> get,
               std::function<void(int)> set)
{
    Row row = Choice(label, std::move(help), {}, std::move(get), std::move(set));
    row.count_of = std::move(count);
    row.name_of = [name = std::move(name)](int index) {
        const char* text = name(index);
        return text != nullptr ? T(text) : std::string();
    };
    return row;
}

Row Toggle(const char* label, Text help, std::function<bool()> get,
           std::function<void(bool)> set)
{
    return Choice(label, std::move(help), {"Off", "On"},
                  [get = std::move(get)] { return get() ? 1 : 0; },
                  [set = std::move(set)](int value) { set(value != 0); });
}

Row Slider(const char* label, Text help, float min, float max, float step,
           std::function<float()> get, std::function<void(float)> set,
           std::function<std::string(float)> format)
{
    Row row;
    row.kind = RowKind::Slider;
    row.label = T(label);
    row.help = std::move(help);
    row.min = min;
    row.max = max;
    row.step = step;
    row.get_value = std::move(get);
    row.set_value = std::move(set);
    row.format = std::move(format);
    return row;
}

Row Action(const char* label, Text help, const char* action,
           std::function<void()> run)
{
    Row row;
    row.kind = RowKind::Action;
    row.label = T(label);
    row.help = std::move(help);
    row.action = T(action);
    row.run = std::move(run);
    return row;
}

Row Binding(std::string label, Text text, std::function<bool()> bound,
            std::function<void()> capture, std::function<void()> clear)
{
    Row row;
    row.kind = RowKind::Binding;
    row.label = std::move(label);
    row.help = Fixed("Select it, then press the input to bind to it.");
    row.binding_text = std::move(text);
    row.bound = std::move(bound);
    row.capture = std::move(capture);
    row.clear = std::move(clear);
    return row;
}

Row When(Row&& row, std::function<bool()> enabled)
{
    row.enabled = std::move(enabled);
    return std::move(row);
}

Row Neutral(Row&& row, float neutral)
{
    row.neutral = neutral;
    row.has_neutral = true;
    return std::move(row);
}

std::function<std::string(float)> Format(const char* format)
{
    return [format](float value) { return Printf(format, value); };
}

std::function<std::string(float)> Percent(float scale)
{
    return [scale](float value) {
        return Printf("%.0f%%", static_cast<double>(value * scale));
    };
}

void MarkDirty(void)
{
    g_opt.dirty = true;
}

/* ---------------------------------------------------------------------------
 *  The yes / no window.
 * ------------------------------------------------------------------------ */

void ConfirmSelect(bool yes)
{
    Confirm& c = g_opt.confirm;
    c.yes_selected = yes;
    if (c.yes != nullptr && c.no != nullptr)
    {
        c.yes->SetClass("selected", yes);
        c.no->SetClass("selected", !yes);
    }
}

void ConfirmShowNote(const std::string& note)
{
    Confirm& c = g_opt.confirm;
    if (c.note == nullptr || note == c.shown_note)
    {
        return;
    }
    c.note->SetInnerRML(Escape(note));
    c.note->SetClass("hidden", note.empty());
    c.shown_note = note;
}

std::string SecondsLeft(uint64_t deadline)
{
    const uint64_t now = SDL_GetTicks();
    const uint64_t left = deadline > now ? deadline - now : 0;
    return std::to_string(static_cast<int>((left + 999u) / 1000u));
}

/* The text, the note under it (with the seconds left when there is a
 * countdown), and what Yes and No do.  No is the default answer. */
void ConfirmOpen(const std::string& text, const std::string& note,
                 std::function<void()> on_yes, std::function<void()> on_no,
                 uint64_t countdown_ms = 0)
{
    Confirm& c = g_opt.confirm;
    if (c.root == nullptr)
    {
        if (on_no)
        {
            on_no();
        }
        return;
    }
    if (c.text != nullptr)
    {
        c.text->SetInnerRML(Escape(text));
    }
    c.note_text = note;
    c.shown_note.clear();
    c.deadline = countdown_ms != 0 ? SDL_GetTicks() + countdown_ms : 0;
    ConfirmShowNote(note);
    if (c.choices != nullptr)
    {
        c.choices->SetClass("hidden", false);
    }
    c.on_yes = std::move(on_yes);
    c.on_no = std::move(on_no);
    c.open = true;
    ConfirmSelect(false);
    c.root->SetClass("hidden", false);
}

void ConfirmClose(void)
{
    Confirm& c = g_opt.confirm;
    if (c.root != nullptr)
    {
        c.root->SetClass("hidden", true);
    }
    c.open = false;
    c.deadline = 0;
    c.on_yes = nullptr;
    c.on_no = nullptr;
}

void ConfirmAnswer(bool yes)
{
    Confirm& c = g_opt.confirm;
    std::function<void()> on_yes = std::move(c.on_yes);
    std::function<void()> on_no = std::move(c.on_no);
    ConfirmClose();
    if (yes)
    {
        PlayDecide();
        if (on_yes)
        {
            on_yes();
        }
    }
    else
    {
        PlayCancel();
        if (on_no)
        {
            on_no();
        }
    }
}

void ConfirmUpdate(void)
{
    Confirm& c = g_opt.confirm;
    if (!c.open || c.deadline == 0)
    {
        return;
    }
    if (SDL_GetTicks() >= c.deadline)
    {
        ConfirmAnswer(false);
        return;
    }
    const std::string countdown =
        T("Reverting in ") + SecondsLeft(c.deadline) + T(" seconds");
    ConfirmShowNote(c.note_text.empty() ? countdown : c.note_text + " " + countdown);
}

/* ---------------------------------------------------------------------------
 *  Binding capture.
 * ------------------------------------------------------------------------ */

bool CaptureActive(void)
{
    return g_opt.capture.kind != CAPTURE_NONE;
}

const char* CaptureDeviceText(CaptureKind kind)
{
    switch (kind)
    {
        case CAPTURE_SPECIAL_ACTION:
            return "Press a key, mouse button, or scroll wheel...";
        case CAPTURE_KEYBOARD_BUTTON:
        case CAPTURE_KEYBOARD_STICK_NEG:
        case CAPTURE_KEYBOARD_STICK_POS:
            return "Press a key...";
        case CAPTURE_CONTROLLER_BUTTON:
            return "Press a controller button or trigger...";
        case CAPTURE_CONTROLLER_STICK_AXIS:
            return "Move a controller stick...";
        default:
            return "Press input...";
    }
}

void ShowCaptureNote(void)
{
    ConfirmShowNote(T(CaptureDeviceText(g_opt.capture.kind)) + " "
                    + T("Times out in ")
                    + SecondsLeft(g_opt.capture.deadline_ticks)
                    + T(" seconds."));
}

void BeginCapture(CaptureKind kind, int target, const std::string& label)
{
    Confirm& c = g_opt.confirm;
    if (c.root == nullptr)
    {
        return;
    }
    const uint64_t now = SDL_GetTicks();
    MikuPan_InputBindingCaptureReset();
    g_opt.capture.kind = kind;
    g_opt.capture.target = target;
    g_opt.capture.label = label;
    g_opt.capture.after_ticks = now + 180u;
    g_opt.capture.deadline_ticks = now + kCaptureTimeoutMs;
    g_opt.capture.waiting_for_release = true;

    if (c.text != nullptr)
    {
        c.text->SetInnerRML(Escape(T("Bind Control") + ": " + label));
    }
    if (c.choices != nullptr)
    {
        c.choices->SetClass("hidden", true);
    }
    c.shown_note.clear();
    ShowCaptureNote();
    c.root->SetClass("hidden", false);
}

void EndCapture(bool bound)
{
    g_opt.capture = Capture();
    MikuPan_InputBindingCaptureReset();
    if (g_opt.confirm.root != nullptr && !g_opt.confirm.open)
    {
        g_opt.confirm.root->SetClass("hidden", true);
    }
    if (g_opt.confirm.choices != nullptr)
    {
        g_opt.confirm.choices->SetClass("hidden", false);
    }
    /* The key that bound, or the one that timed it out, must not reach the
     * menu as well. */
    g_opt.input_block_until = SDL_GetTicks() + 250u;
    if (bound)
    {
        MarkDirty();
        PlayDecide();
    }
    else
    {
        PlayCancel();
    }
}

bool AnyKeyboardDown(void)
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

int FindPressedScancode(void)
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

bool AnyControllerDown(SDL_Gamepad* gp)
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
    static const SDL_GamepadAxis kAxes[] = {
        SDL_GAMEPAD_AXIS_LEFTX,        SDL_GAMEPAD_AXIS_LEFTY,
        SDL_GAMEPAD_AXIS_RIGHTX,       SDL_GAMEPAD_AXIS_RIGHTY,
        SDL_GAMEPAD_AXIS_LEFT_TRIGGER, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER,
    };
    for (SDL_GamepadAxis axis : kAxes)
    {
        const int value = SDL_GetGamepadAxis(gp, axis);
        if (value > 12000 || value < -12000)
        {
            return true;
        }
    }
    return false;
}

bool FindPressedControllerInput(SDL_Gamepad* gp, int* out_kind, int* out_code)
{
    if (gp == nullptr)
    {
        return false;
    }
    for (int b = 0; b < SDL_GAMEPAD_BUTTON_COUNT; b++)
    {
        if (SDL_GetGamepadButton(gp, static_cast<SDL_GamepadButton>(b)))
        {
            *out_kind = MIKUPAN_CONTROLLER_BIND_BUTTON;
            *out_code = b;
            return true;
        }
    }
    for (SDL_GamepadAxis axis :
         {SDL_GAMEPAD_AXIS_LEFT_TRIGGER, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER})
    {
        if (SDL_GetGamepadAxis(gp, axis) > 16384)
        {
            *out_kind = MIKUPAN_CONTROLLER_BIND_AXIS;
            *out_code = axis;
            return true;
        }
    }
    return false;
}

bool FindMovedStickAxis(SDL_Gamepad* gp, int* out_axis)
{
    if (gp == nullptr)
    {
        return false;
    }
    for (SDL_GamepadAxis axis : {SDL_GAMEPAD_AXIS_LEFTX, SDL_GAMEPAD_AXIS_LEFTY,
                                 SDL_GAMEPAD_AXIS_RIGHTX, SDL_GAMEPAD_AXIS_RIGHTY})
    {
        const int value = SDL_GetGamepadAxis(gp, axis);
        if (value > 20000 || value < -20000)
        {
            *out_axis = axis;
            return true;
        }
    }
    return false;
}

void UpdateCapture(void)
{
    Capture& cap = g_opt.capture;
    if (cap.kind == CAPTURE_NONE)
    {
        return;
    }

    const uint64_t now = SDL_GetTicks();
    if (now >= cap.deadline_ticks)
    {
        EndCapture(false);
        return;
    }
    ShowCaptureNote();
    if (now < cap.after_ticks)
    {
        return;
    }

    /* The button that started the capture is still down: wait for it to be
     * let go, or it would bind itself. */
    if (cap.waiting_for_release)
    {
        bool down = false;
        switch (cap.kind)
        {
            case CAPTURE_SPECIAL_ACTION:
                down = MikuPan_InputBindingCaptureAnyDown() != 0;
                break;
            case CAPTURE_KEYBOARD_BUTTON:
            case CAPTURE_KEYBOARD_STICK_NEG:
            case CAPTURE_KEYBOARD_STICK_POS:
                down = AnyKeyboardDown();
                break;
            default:
                down = AnyControllerDown(MikuPan_GetController());
                break;
        }
        if (down)
        {
            return;
        }
        cap.waiting_for_release = false;
    }

    const int target = cap.target;
    switch (cap.kind)
    {
        case CAPTURE_SPECIAL_ACTION:
        {
            MikuPan_InputBinding binding = {};
            if (!MikuPan_InputBindingCapturePoll(&binding))
            {
                return;
            }
            if (target >= 0 && target < MIKUPAN_SPECIAL_ACTION_COUNT)
            {
                mikupan_special_action_map[target] = binding;
            }
            EndCapture(true);
            break;
        }
        case CAPTURE_KEYBOARD_BUTTON:
        case CAPTURE_KEYBOARD_STICK_NEG:
        case CAPTURE_KEYBOARD_STICK_POS:
        {
            const int scancode = FindPressedScancode();
            if (scancode < 0)
            {
                return;
            }
            if (cap.kind == CAPTURE_KEYBOARD_BUTTON && target >= 0
                && target < MIKUPAN_CONTROLLER_LOGICAL_COUNT)
            {
                mikupan_keyboard_map[target] = scancode;
            }
            else if (cap.kind == CAPTURE_KEYBOARD_STICK_NEG && target >= 0
                     && target < MIKUPAN_STICK_COUNT)
            {
                mikupan_stick_keyboard_map[target].neg_scancode = scancode;
            }
            else if (cap.kind == CAPTURE_KEYBOARD_STICK_POS && target >= 0
                     && target < MIKUPAN_STICK_COUNT)
            {
                mikupan_stick_keyboard_map[target].pos_scancode = scancode;
            }
            EndCapture(true);
            break;
        }
        case CAPTURE_CONTROLLER_BUTTON:
        {
            int kind = 0;
            int code = 0;
            if (!FindPressedControllerInput(MikuPan_GetController(), &kind, &code))
            {
                return;
            }
            if (target >= 0 && target < MIKUPAN_CONTROLLER_LOGICAL_COUNT)
            {
                mikupan_controller_map[target].kind = kind;
                mikupan_controller_map[target].code = code;
            }
            EndCapture(true);
            break;
        }
        case CAPTURE_CONTROLLER_STICK_AXIS:
        {
            int axis = 0;
            if (!FindMovedStickAxis(MikuPan_GetController(), &axis))
            {
                return;
            }
            if (target >= 0 && target < MIKUPAN_STICK_COUNT)
            {
                mikupan_stick_controller_map[target].axis = axis;
            }
            EndCapture(true);
            break;
        }
        default:
            EndCapture(false);
            break;
    }
}

/* ---------------------------------------------------------------------------
 *  The side panels: image calibration and the CRT filter.
 * ------------------------------------------------------------------------ */

void BuildPanelRows(void);
void ShowPage(int index, bool sound);
void Select(int index, bool scroll);

void SetBodyClass(const char* name, bool on)
{
    if (g_opt.body != nullptr)
    {
        g_opt.body->SetClass(name, on);
    }
}

void OpenPanel(int kind)
{
    if (kind < 0 || kind >= static_cast<int>(g_opt.panels.size()))
    {
        return;
    }

    g_opt.panel_dirty_before = g_opt.dirty;
    if (kind == PANEL_CALIBRATION)
    {
        g_opt.calibration_brightness = MikuPan_GetBrightness();
        g_opt.calibration_gamma = MikuPan_GetGamma();
        g_opt.calibration_contrast = MikuPan_GetContrast();
        g_opt.calibration_shadow_depth = MikuPan_GetShadowDepth();
        g_opt.calibration_hdr = MikuPan_IsHdrEnabled();
        g_opt.calibration_paper_white = MikuPan_GetHdrPaperWhite();
        g_opt.calibration_peak = MikuPan_GetHdrPeakLuminance();
    }
    else
    {
        const MikuPan_ConfigCrt* crt = MikuPan_GetCrtSettings();
        g_opt.crt = crt != nullptr ? *crt : MikuPan_ConfigCrt();
    }

    g_opt.panel_kind = kind;
    g_opt.panels[static_cast<size_t>(kind)].selected = -1;
    g_opt.shown_panel_help.clear();
    g_opt.shown_panel_prompts.clear();
    BuildPanelRows();
    SetBodyClass("panel-open", true);
    if (g_opt.panel != nullptr)
    {
        g_opt.panel->SetClass("hidden", false);
    }
    if (g_opt.levels != nullptr)
    {
        g_opt.levels->SetClass("hidden", kind != PANEL_CALIBRATION);
    }
    ShowPage(g_opt.page, false);
}

void ClosePanel(void)
{
    if (g_opt.panel_kind == PANEL_NONE)
    {
        return;
    }
    g_opt.panel_kind = PANEL_NONE;
    g_opt.dragging = RowRef();
    SetBodyClass("panel-open", false);
    if (g_opt.panel != nullptr)
    {
        g_opt.panel->SetClass("hidden", true);
    }
    if (g_opt.levels != nullptr)
    {
        g_opt.levels->SetClass("hidden", true);
    }
    if (g_opt.panel_rows != nullptr)
    {
        g_opt.panel_rows->SetInnerRML("");
    }
    for (Page& panel : g_opt.panels)
    {
        for (Row& row : panel.rows)
        {
            if (row.element != nullptr)
            {
                g_opt.row_of.erase(row.element);
            }
            row.element = row.value = row.track = row.fill = nullptr;
        }
    }
    ShowPage(g_opt.page, false);
}

/* Back from a panel: everything it changed goes back as it was. */
void CancelPanel(void)
{
    if (g_opt.panel_kind == PANEL_CALIBRATION)
    {
        MikuPan_SetBrightness(g_opt.calibration_brightness);
        MikuPan_SetGamma(g_opt.calibration_gamma);
        MikuPan_SetContrast(g_opt.calibration_contrast);
        MikuPan_SetShadowDepth(g_opt.calibration_shadow_depth);
        MikuPan_SetHdrEnabled(g_opt.calibration_hdr);
        MikuPan_SetHdrPaperWhite(g_opt.calibration_paper_white);
        MikuPan_SetHdrPeakLuminance(g_opt.calibration_peak);
    }
    else if (g_opt.panel_kind == PANEL_CRT)
    {
        MikuPan_SetCrtSettings(&g_opt.crt);
    }
    g_opt.dirty = g_opt.panel_dirty_before;
    PlayCancel();
    ClosePanel();
}

void KeepPanel(void)
{
    ClosePanel();
}

float CalibrationOutputLevel(float value)
{
    float color = std::clamp(value, 0.0f, 1.0f);
    color *= MikuPan_GetBrightness();
    color = (color - 0.5f) * std::max(MikuPan_GetContrast(), 0.0f) + 0.5f;
    color = std::pow(std::max(color, 0.0f),
                     1.0f / std::max(MikuPan_GetGamma(), 0.01f));
    return std::clamp(color, 0.0f, 1.0f);
}

/* The ramp beside the calibration panel, put through the same grade as the
 * picture: the dark end squared, so the shadows get most of the steps. */
void RefreshLevels(void)
{
    const int count = static_cast<int>(g_opt.level_swatches.size());
    for (int i = 0; i < count; i++)
    {
        Rml::Element* swatch = g_opt.level_swatches[static_cast<size_t>(i)];
        if (swatch == nullptr)
        {
            continue;
        }
        const float t = static_cast<float>(i) / static_cast<float>(count - 1);
        const int level = std::clamp(
            static_cast<int>(std::lround(CalibrationOutputLevel(t * t) * 255.0f)),
            0, 255);
        char color[16];
        std::snprintf(color, sizeof(color), "#%02x%02x%02x", level, level, level);
        swatch->SetProperty("background-color", color);
    }
}

/* ---------------------------------------------------------------------------
 *  Settings the rows need a little help with.
 * ------------------------------------------------------------------------ */

int WindowModeIndex(void)
{
    return MikuPan_GetWindowMode() == MIKUPAN_WINDOW_WINDOWED ? 0 : 1;
}

bool IsWindowed(void)
{
    return MikuPan_GetWindowMode() == MIKUPAN_WINDOW_WINDOWED;
}

int CurrentDpadMovementUsesSubjective(void)
{
    if (MikuPan_IsCustomActionProfileEnabled()
        || MikuPan_MovementStyleOverrideEnabled())
    {
        return MikuPan_CustomActionProfileUsesDpadSubjectiveMove();
    }
    return MikuPan_KeyProfileUsesSubjectiveMove();
}

int CurrentStickMovementUsesSubjective(void)
{
    if (MikuPan_IsCustomActionProfileEnabled()
        || MikuPan_MovementStyleOverrideEnabled())
    {
        return MikuPan_CustomActionProfileUsesStickSubjectiveMove();
    }
    return MikuPan_KeyProfileUsesSubjectiveMove();
}

/* The first change to either movement style takes both over from the key
 * profile, so the one not being changed keeps what it was. */
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

bool HdrOn(void)
{
    return MikuPan_IsHdrEnabled() != 0;
}

bool CrtOn(void)
{
    const MikuPan_ConfigCrt* crt = MikuPan_GetCrtSettings();
    return crt != nullptr && crt->enabled != 0;
}

std::string RenderSizeNote(void)
{
    char text[96];
    std::snprintf(text, sizeof(text), "%d x %d",
                  MikuPan_GetRenderResolutionWidth(),
                  MikuPan_GetRenderResolutionHeight());
    return " " + T("The game is drawn at") + " " + text + ".";
}

/* The last part of a long path, which is the part that tells folders apart. */
std::string TailOf(const std::string& text, size_t max_bytes)
{
    if (text.size() <= max_bytes)
    {
        return text;
    }
    size_t start = text.size() - max_bytes;
    while (start < text.size()
           && (static_cast<unsigned char>(text[start]) & 0xC0u) == 0x80u)
    {
        start++;
    }
    return "\xE2\x80\xA6" + text.substr(start);
}

/* ---------------------------------------------------------------------------
 *  The pages.
 * ------------------------------------------------------------------------ */

void ApplyDeferred(Row& row);

Row DisplayChoice(const char* label, Text help, std::function<int()> count,
                  std::function<const char*(int)> name,
                  std::function<int()> get, std::function<void(int)> set)
{
    Row row = ListChoice(label, std::move(help), std::move(count),
                         std::move(name), std::move(get), std::move(set));
    row.deferred = true;
    return row;
}

Page DisplayPage(void)
{
    Page page{"display", "Display", {}};
    std::vector<Row>& rows = page.rows;

    {
        Row mode = Choice(
            "Display Mode", Fixed("Windowed or borderless fullscreen."),
            {"Windowed", "Fullscreen"}, [] { return WindowModeIndex(); },
            [](int index) {
                MikuPan_SetWindowMode(index == 0 ? MIKUPAN_WINDOW_WINDOWED
                                                 : MIKUPAN_WINDOW_BORDERLESS);
            });
        mode.deferred = true;
        rows.push_back(std::move(mode));
    }
    rows.push_back(When(
        DisplayChoice(
            "Windowed Resolution",
            Fixed("Window size used in Windowed mode. Fullscreen uses borderless desktop size."),
            [] { return MikuPan_GetWindowSizeOptionCount(); },
            [](int index) { return MikuPan_GetWindowSizeOptionLabel(index); },
            [] { return MikuPan_GetSelectedWindowSizeOption(); },
            [](int index) { (void) MikuPan_SelectWindowSizeOption(index); }),
        IsWindowed));
    rows.push_back(Toggle(
        "VSync", Fixed("Synchronise presentation to the display refresh."),
        [] { return MikuPan_IsVsync() != 0; },
        [](bool on) { MikuPan_SetVsync(on ? 1 : 0); }));

    rows.push_back(Heading("Picture"));
    rows.push_back(Choice(
        "Aspect Ratio",
        [] {
            return T("Auto fills the window and widens the view to its sides. "
                     "Original keeps the PS2's 640x448 frame; a fixed ratio "
                     "adds bars where the window differs.")
                   + RenderSizeNote();
        },
        {"Auto", "Original (PS2)", "4:3", "16:10", "16:9", "21:9", "Custom"},
        [] { return MikuPan_GetAspectMode(); },
        [](int mode) { MikuPan_SetAspectMode(mode); }));
    rows.push_back(When(
        Slider("Custom Ratio", Fixed("Width over height, for Custom."), 1.0f,
               4.0f, 0.01f, [] { return MikuPan_GetAspectCustom(); },
               [](float ratio) { MikuPan_SetAspectCustom(ratio); },
               Format("%.2f : 1")),
        [] { return MikuPan_GetAspectMode() == MIKUPAN_ASPECT_CUSTOM; }));
    rows.push_back(Action(
        "Image Calibration",
        Fixed("Open a scene preview with all image controls in one place."),
        "Adjust", [] { OpenPanel(PANEL_CALIBRATION); }));

    rows.push_back(Heading("HDR"));
    rows.push_back(Toggle(
        "HDR Output",
        Fixed("Use extended-linear HDR presentation when the display supports it."),
        HdrOn, [](bool on) { MikuPan_SetHdrEnabled(on ? 1 : 0); }));
    rows.push_back(When(
        Slider("Paper White", Fixed("How bright the game's white is in HDR, in nits."),
               80.0f, 400.0f, 10.0f, [] { return MikuPan_GetHdrPaperWhite(); },
               [](float nits) { MikuPan_SetHdrPaperWhite(nits); },
               Format("%.0f nits")),
        HdrOn));
    rows.push_back(When(
        Slider("Peak Brightness",
               Fixed("The brightest the picture may go in HDR, in nits."),
               100.0f, 4000.0f, 50.0f,
               [] { return MikuPan_GetHdrPeakLuminance(); },
               [](float nits) { MikuPan_SetHdrPeakLuminance(nits); },
               Format("%.0f nits")),
        HdrOn));
    return page;
}

Page GraphicsPage(void)
{
    Page page{"graphics", "Graphics", {}};
    std::vector<Row>& rows = page.rows;

    rows.push_back(DisplayChoice(
        "Render Resolution",
        [] {
            return T("Match the window, scale from it, or use exact PS2 multiples.")
                   + RenderSizeNote();
        },
        [] { return MikuPan_GetRenderResolutionOptionCount(); },
        [](int index) { return MikuPan_GetRenderResolutionOptionLabel(index); },
        [] { return MikuPan_GetSelectedRenderResolutionOption(); },
        [](int index) { (void) MikuPan_SelectRenderResolutionOption(index); }));
    {
        Row gpu = ListChoice(
            "GPU Backend",
            [] {
                std::string text =
                    T("Renderer backend selection. A restart may be required.");
                text += " " + T("Active:") + " "
                        + MikuPan_GetActiveGpuDriverLabel() + ".";
                if (MikuPan_IsGpuDriverRestartPending())
                {
                    text += " " + T("Restart the game to apply.");
                }
                return text;
            },
            [] { return MikuPan_GetGpuDriverOptionCount(); },
            [](int index) { return MikuPan_GetGpuDriverOptionLabel(index); },
            [] { return MikuPan_GetSelectedGpuDriverOption(); },
            [](int index) { (void) MikuPan_SelectGpuDriverOption(index); });
        gpu.choosable = [](int index) {
            return MikuPan_IsGpuDriverOptionSupported(index) != 0;
        };
        rows.push_back(std::move(gpu));
    }
    rows.push_back(When(
        ListChoice(
            "MSAA",
            [] {
                std::string text = T("Smooth geometry edges using multisampling.");
                if (MikuPan_IsSuperSamplingEnabled())
                {
                    text += " " + T("Off while the render resolution is above the window's.");
                }
                return text;
            },
            [] { return MikuPan_GetMSAAOptionCount(); },
            [](int index) { return MikuPan_GetMSAAOptionLabel(index); },
            [] { return MikuPan_GetSelectedMSAAOption(); },
            [](int index) { (void) MikuPan_SelectMSAAOption(index); }),
        [] { return MikuPan_IsSuperSamplingEnabled() == 0; }));
    rows.push_back(ListChoice(
        "Shadow Resolution",
        Fixed("Higher values sharpen shadows at a performance cost."),
        [] { return MikuPan_GetShadowResolutionOptionCount(); },
        [](int index) { return MikuPan_GetShadowResolutionOptionLabel(index); },
        [] { return MikuPan_GetSelectedShadowResolutionOption(); },
        [](int index) { (void) MikuPan_SelectShadowResolutionOption(index); }));
    rows.push_back(Choice(
        "Lighting Mode", Fixed("Modern pixel lighting or PS2-style vertex lighting."),
        {"Pixel (Modern)", "Vertex (PS2)"}, [] { return MikuPan_GetLightingMode(); },
        [](int mode) { MikuPan_SetLightingMode(mode); }));
    rows.push_back(ListChoice(
        "Dither Filtering",
        Fixed("Native keeps the PS2 dither texture crisp. Soft smooths it for modern displays."),
        [] { return MikuPan_GetDitherModeOptionCount(); },
        [](int index) { return MikuPan_GetDitherModeOptionLabel(index); },
        [] { return MikuPan_GetSelectedDitherModeOption(); },
        [](int index) { (void) MikuPan_SelectDitherModeOption(index); }));

    rows.push_back(Heading("Camera Obscura"));
    rows.push_back(Choice(
        "Finder Surround", Fixed("Choose the area outside the Camera Obscura frame."),
        {"Black", "Blur"},
        [] {
            return mikupan_configuration.renderer.finder_viewport_mask_mode
                           == MIKUPAN_FINDER_VIEWPORT_MASK_BLACK
                       ? 0
                       : 1;
        },
        [](int index) {
            mikupan_configuration.renderer.finder_viewport_mask_mode =
                index == 0 ? MIKUPAN_FINDER_VIEWPORT_MASK_BLACK
                           : MIKUPAN_FINDER_VIEWPORT_MASK_BLUR;
        }));

    rows.push_back(Heading("Video Filters"));
    {
        Row crt = Action(
            "CRT Filter",
            [] {
                return T("Open the CRT post-process tuning panel.") + " "
                       + T(CrtOn() ? "The filter is on." : "The filter is off.");
            },
            "Adjust", [] { OpenPanel(PANEL_CRT); });
        rows.push_back(std::move(crt));
    }
    return page;
}

Page AudioPage(void)
{
    Page page{"audio", "Audio", {}};
    page.rows.push_back(Slider(
        "Master Volume", Fixed("Set the overall game volume."), 0.0f, 1.0f, 0.05f,
        [] { return MikuPan_GetAudioMasterVolume(); },
        [](float volume) { MikuPan_SetAudioMasterVolume(volume); },
        Percent(100.0f)));
    return page;
}

Page GamePage(void)
{
    Page page{"game", "Game", {}};
    std::vector<Row>& rows = page.rows;

    rows.push_back(ListChoice(
        "Flashlight Style",
        Fixed("Choose the original PS2 light, the simpler Xbox light, or disable it."),
        [] { return MikuPan_GetFlashlightStyleOptionCount(); },
        [](int index) { return MikuPan_GetFlashlightStyleOptionLabel(index); },
        [] { return MikuPan_GetSelectedFlashlightStyleOption(); },
        [](int index) { (void) MikuPan_SelectFlashlightStyleOption(index); }));
    rows.push_back(Toggle(
        "Keep Finder Raised",
        Fixed("Do not lower the Camera Obscura when an apparition event begins."),
        [] { return MikuPan_KeepFinderRaisedForApparitionsEnabled() != 0; },
        [](bool on) { MikuPan_SetKeepFinderRaisedForApparitionsEnabled(on ? 1 : 0); }));
    rows.push_back(Toggle(
        "Fix Number Door Localization",
        Fixed("Use clockwise numerical ordering and show inset numbers over the original kanji buttons."),
        [] { return mikupan_configuration.number_door_fix_localization != 0; },
        [](bool on) { mikupan_configuration.number_door_fix_localization = on ? 1 : 0; }));
    rows.push_back(Toggle(
        "Improved Movement Collisions",
        Fixed("Uses an improved movement collision method to reduce snagging."),
        [] { return MikuPan_ImprovedMovementCollisionsEnabled() != 0; },
        [](bool on) { MikuPan_SetImprovedMovementCollisionsEnabled(on ? 1 : 0); }));

    rows.push_back(Heading("Interface"));
    rows.push_back(Toggle(
        "Mini Map", Fixed("Show the corner map while exploring."),
        [] { return mikupan_configuration.minimap_enabled != 0; },
        [](bool on) { mikupan_configuration.minimap_enabled = on ? 1 : 0; }));
    rows.push_back(Toggle(
        "Mirror Stone Indicator", Fixed("Show a Mirror Stone icon next to the filament."),
        [] { return MikuPan_MirrorStoneHudEnabled() != 0; },
        [](bool on) { MikuPan_SetMirrorStoneHudEnabled(on ? 1 : 0); }));
    rows.push_back(Toggle(
        "Auto-hide Cursor", Fixed("Hide the mouse cursor after 5 seconds of inactivity."),
        [] { return mikupan_configuration.cursor_auto_hide_enabled != 0; },
        [](bool on) { mikupan_configuration.cursor_auto_hide_enabled = on ? 1 : 0; }));
    rows.push_back(ListChoice(
        "Font", Fixed("Choose the options-menu font."),
        [] { return MikuPan_GetFontOptionCount(); },
        [](int index) { return MikuPan_GetFontOptionLabel(index); },
        [] { return MikuPan_GetSelectedFontOption(); },
        [](int index) { (void) MikuPan_SelectFontOption(index); }));
    rows.push_back(ListChoice(
        "Overlay Theme", Fixed("Colour palette of the ImGui overlay."),
        [] { return MikuPan_GetThemeOptionCount(); },
        [](int index) { return MikuPan_GetThemeOptionLabel(index); },
        [] { return MikuPan_GetSelectedThemeOption(); },
        [](int index) { (void) MikuPan_SelectThemeOption(index); }));
    rows.push_back(Neutral(
        Slider("Overlay Font Scale", Fixed("Text size of the ImGui overlay."), 0.75f,
               1.5f, 0.05f, [] { return MikuPan_GetUiFontScale(); },
               [](float scale) { MikuPan_SetUiFontScale(scale); }, Format("%.2fx")),
        1.0f));

    rows.push_back(Heading("Album"));
    rows.push_back(Toggle(
        "Save Album Photos as PNG",
        Fixed("Also write album photos as PNG files next to the game."),
        [] { return mikupan_configuration.album_photo_png_export_enabled != 0; },
        [](bool on) { mikupan_configuration.album_photo_png_export_enabled = on ? 1 : 0; }));

    rows.push_back(Heading("Files"));
    {
        Row folder = Action(
            "Data Folder",
            [] {
                const char* path = MikuPan_GetDataFolder();
                std::string text = T("Folder containing extracted MikuPan game data.");
                if (path != nullptr && path[0] != '\0')
                {
                    text += " " + std::string(path);
                }
                text += " " + T("Select it to browse; a change applies on the next launch.");
                return text;
            },
            "Browse", [] {
                MikuPan_BrowseDataFolderFromUi();
                MarkDirty();
            });
        folder.plain = true;
        folder.action_text = [] {
            const char* path = MikuPan_GetDataFolder();
            return path != nullptr && path[0] != '\0'
                       ? TailOf(path, 34)
                       : T("Not set");
        };
        rows.push_back(std::move(folder));
    }
    return page;
}

const char* ButtonLabel(int index)
{
    static const char* const kLabels[MIKUPAN_CONTROLLER_LOGICAL_COUNT] = {
        "Triangle", "Cross / Confirm", "Square", "Circle / Cancel",
        "D-Pad Up", "D-Pad Down", "D-Pad Left", "D-Pad Right",
        "R3 / Right Stick", "Select", "Start / Pause", "L3 / Left Stick",
        "R1 / RB", "L2 / LT", "R2 / RT", "L1 / LB",
    };
    return kLabels[index];
}

Page ControlsPage(void)
{
    Page page{"controls", "Controls", {}};
    std::vector<Row>& rows = page.rows;

    rows.push_back(Heading("Movement Style"));
    rows.push_back(Choice(
        "D-Pad / Arrow Keys",
        Fixed("Objective walks Miku where you push, as the camera sees it. Subjective turns her and walks forward."),
        {"Objective", "Subjective"}, [] { return CurrentDpadMovementUsesSubjective(); },
        [](int subjective) {
            SeedMovementStyleOverride();
            MikuPan_SetCustomActionProfileDpadSubjectiveMove(subjective);
        }));
    rows.push_back(Choice(
        "Left Stick / WASD",
        Fixed("Objective walks Miku where you push, as the camera sees it. Subjective turns her and walks forward."),
        {"Objective", "Subjective"}, [] { return CurrentStickMovementUsesSubjective(); },
        [](int subjective) {
            SeedMovementStyleOverride();
            MikuPan_SetCustomActionProfileStickSubjectiveMove(subjective);
        }));

    rows.push_back(Heading("Viewfinder"));
    rows.push_back(Choice(
        "Stick Layout",
        Fixed("Classic aims the Camera Obscura with the left stick, Modern with the right."),
        {"Classic", "Modern"},
        [] { return MikuPan_CustomActionProfileSwapsFinderSticks() ? 1 : 0; },
        [](int modern) { MikuPan_SetCustomActionProfileFinderSwapSticks(modern); }));
    rows.push_back(Toggle(
        "D-Pad Film Swap",
        Fixed("Use D-Pad Up / Down in finder mode to switch film. D-Pad finder aim is disabled while enabled."),
        [] { return MikuPan_FinderDpadFilmSwapEnabled() != 0; },
        [](bool on) { MikuPan_SetFinderDpadFilmSwapEnabled(on ? 1 : 0); }));
    rows.push_back(Choice(
        "Camera Activation",
        Fixed("Hold raises the Camera Obscura while the button is held; Toggle raises and lowers it with each press."),
        {"Hold", "Toggle"},
        [] {
            return MikuPan_CameraActivationMode() == MIKUPAN_CAMERA_ACTIVATION_TOGGLE ? 1 : 0;
        },
        [](int toggle) {
            MikuPan_SetCameraActivationMode(toggle ? MIKUPAN_CAMERA_ACTIVATION_TOGGLE
                                                   : MIKUPAN_CAMERA_ACTIVATION_HOLD);
        }));

    rows.push_back(Heading("Controller"));
    rows.push_back(Toggle(
        "Controller Rumble", Fixed("Let the controller shake when the game calls for it."),
        [] { return MikuPan_ControllerRumbleEnabled() != 0; },
        [](bool on) { MikuPan_SetControllerRumbleEnabled(on ? 1 : 0); }));
    static const char* const kInvertLabels[MIKUPAN_STICK_COUNT] = {
        "Invert Move X", "Invert Move Y", "Invert Aim X", "Invert Aim Y",
    };
    for (int i = 0; i < MIKUPAN_STICK_COUNT; i++)
    {
        rows.push_back(Toggle(
            kInvertLabels[i], Fixed("Reverse this stick axis on the controller."),
            [i] { return mikupan_stick_controller_map[i].invert != 0; },
            [i](bool on) { mikupan_stick_controller_map[i].invert = on ? 1 : 0; }));
    }

    rows.push_back(Heading(""));
    rows.push_back(Action(
        "Defaults", Fixed("Restore keyboard, mouse, and controller bindings."),
        "Restore", [] {
            ConfirmOpen(T("Restore every control and binding to its default?"), "",
                        [] {
                            MikuPan_ResetControllerBindingsFromUi();
                            MarkDirty();
                        },
                        nullptr);
        }));
    return page;
}

Row KeyboardStickRow(const char* label, int stick, bool positive)
{
    return Binding(
        T(label),
        [stick, positive] {
            const int code = positive ? mikupan_stick_keyboard_map[stick].pos_scancode
                                      : mikupan_stick_keyboard_map[stick].neg_scancode;
            return std::string(MikuPan_ControllerScanCodeLabel(code));
        },
        [stick, positive] {
            return (positive ? mikupan_stick_keyboard_map[stick].pos_scancode
                             : mikupan_stick_keyboard_map[stick].neg_scancode)
                   != 0;
        },
        [stick, positive, label] {
            BeginCapture(positive ? CAPTURE_KEYBOARD_STICK_POS
                                  : CAPTURE_KEYBOARD_STICK_NEG,
                         stick, T(label));
        },
        [stick, positive] {
            if (positive)
            {
                mikupan_stick_keyboard_map[stick].pos_scancode = 0;
            }
            else
            {
                mikupan_stick_keyboard_map[stick].neg_scancode = 0;
            }
        });
}

Page KeyboardPage(void)
{
    Page page{"keyboard", "Keyboard", {}};
    std::vector<Row>& rows = page.rows;

    rows.push_back(Heading("Gameplay Actions"));
    for (int i = 0; i < MIKUPAN_SPECIAL_ACTION_COUNT; i++)
    {
        const std::string label = T(MikuPan_SpecialActionLabel(i));
        rows.push_back(Binding(
            label,
            [i] {
                return std::string(
                    MikuPan_InputBindingLabel(mikupan_special_action_map[i]));
            },
            [i] { return mikupan_special_action_map[i].kind != MIKUPAN_INPUT_BIND_NONE; },
            [i, label] { BeginCapture(CAPTURE_SPECIAL_ACTION, i, label); },
            [i] {
                mikupan_special_action_map[i].kind = MIKUPAN_INPUT_BIND_NONE;
                mikupan_special_action_map[i].code = 0;
            }));
        rows.back().help = Fixed("Select it, then press a key, mouse button or the scroll wheel.");
    }

    rows.push_back(Heading("Keyboard Buttons"));
    for (int i = 0; i < MIKUPAN_CONTROLLER_LOGICAL_COUNT; i++)
    {
        const std::string label = T(ButtonLabel(i));
        rows.push_back(Binding(
            label,
            [i] { return std::string(MikuPan_ControllerScanCodeLabel(mikupan_keyboard_map[i])); },
            [i] { return mikupan_keyboard_map[i] != 0; },
            [i, label] { BeginCapture(CAPTURE_KEYBOARD_BUTTON, i, label); },
            [i] { mikupan_keyboard_map[i] = 0; }));
    }

    rows.push_back(Heading("Movement"));
    rows.push_back(KeyboardStickRow("Forward", MIKUPAN_STICK_LY, false));
    rows.push_back(KeyboardStickRow("Backward", MIKUPAN_STICK_LY, true));
    rows.push_back(KeyboardStickRow("Left", MIKUPAN_STICK_LX, false));
    rows.push_back(KeyboardStickRow("Right", MIKUPAN_STICK_LX, true));

    rows.push_back(Heading("Aim"));
    rows.push_back(KeyboardStickRow("Aim Up", MIKUPAN_STICK_RY, false));
    rows.push_back(KeyboardStickRow("Aim Down", MIKUPAN_STICK_RY, true));
    rows.push_back(KeyboardStickRow("Aim Left", MIKUPAN_STICK_RX, false));
    rows.push_back(KeyboardStickRow("Aim Right", MIKUPAN_STICK_RX, true));
    return page;
}

Page ControllerPage(void)
{
    Page page{"controller", "Controller", {}};
    std::vector<Row>& rows = page.rows;

    rows.push_back(Note(
        "No controller is currently open. Connect a controller, or select one in the ImGui device selector for now.",
        [] { return MikuPan_GetController() == nullptr; }));

    rows.push_back(Heading("Buttons"));
    for (int i = 0; i < MIKUPAN_CONTROLLER_LOGICAL_COUNT; i++)
    {
        const std::string label = T(ButtonLabel(i));
        rows.push_back(Binding(
            label,
            [i] { return std::string(MikuPan_ControllerBindingLabel(mikupan_controller_map[i])); },
            [i] { return mikupan_controller_map[i].kind != MIKUPAN_CONTROLLER_BIND_NONE; },
            [i, label] { BeginCapture(CAPTURE_CONTROLLER_BUTTON, i, label); },
            [i] {
                mikupan_controller_map[i].kind = MIKUPAN_CONTROLLER_BIND_NONE;
                mikupan_controller_map[i].code = 0;
            }));
    }

    rows.push_back(Heading("Sticks"));
    static const char* const kStickLabels[MIKUPAN_STICK_COUNT] = {
        "Move X", "Move Y", "Aim X", "Aim Y",
    };
    for (int i = 0; i < MIKUPAN_STICK_COUNT; i++)
    {
        const std::string label = T(kStickLabels[i]);
        rows.push_back(Binding(
            label,
            [i] {
                return std::string(
                    MikuPan_ControllerStickAxisLabel(mikupan_stick_controller_map[i].axis));
            },
            [i] { return mikupan_stick_controller_map[i].axis >= 0; },
            [i, label] { BeginCapture(CAPTURE_CONTROLLER_STICK_AXIS, i, label); },
            [i] { mikupan_stick_controller_map[i].axis = -1; }));
        rows.back().help = Fixed("Select it, then move a stick to bind its axis.");
    }
    return page;
}

Page CalibrationPanel(void)
{
    Page page{"calibration", "Image Calibration", {}};
    std::vector<Row>& rows = page.rows;

    rows.push_back(Neutral(
        Slider("Brightness",
               Fixed("Raise it until the darkest step of the ramp just parts from black."),
               0.0f, 2.0f, 0.05f, [] { return MikuPan_GetBrightness(); },
               [](float value) { MikuPan_SetBrightness(value); }, Format("%.2f")),
        1.0f));
    rows.push_back(Neutral(
        Slider("Gamma", Fixed("Lifts or deepens the mid-tones."), 0.1f, 2.0f, 0.05f,
               [] { return MikuPan_GetGamma(); },
               [](float value) { MikuPan_SetGamma(value); }, Format("%.2f")),
        1.0f));
    rows.push_back(Neutral(
        Slider("Contrast", Fixed("Spread between the darkest and lightest tones."),
               0.0f, 2.0f, 0.05f, [] { return MikuPan_GetContrast(); },
               [](float value) { MikuPan_SetContrast(value); }, Format("%.2f")),
        1.0f));
    rows.push_back(Neutral(
        Slider("Shadow Depth", Fixed("How dark the shadows cast in the rooms are."),
               0.0f, 2.0f, 0.05f, [] { return MikuPan_GetShadowDepth(); },
               [](float value) { MikuPan_SetShadowDepth(value); }, Format("%.2f")),
        1.0f));
    rows.push_back(Toggle(
        "HDR Output",
        Fixed("Use extended-linear HDR presentation when the display supports it."),
        HdrOn, [](bool on) { MikuPan_SetHdrEnabled(on ? 1 : 0); }));
    rows.push_back(When(
        Slider("Paper White", Fixed("How bright the game's white is in HDR, in nits."),
               80.0f, 400.0f, 10.0f, [] { return MikuPan_GetHdrPaperWhite(); },
               [](float nits) { MikuPan_SetHdrPaperWhite(nits); }, Format("%.0f")),
        HdrOn));
    rows.push_back(When(
        Slider("Peak", Fixed("The brightest the picture may go in HDR, in nits."),
               100.0f, 4000.0f, 50.0f, [] { return MikuPan_GetHdrPeakLuminance(); },
               [](float nits) { MikuPan_SetHdrPeakLuminance(nits); }, Format("%.0f")),
        HdrOn));

    rows.push_back(Heading(""));
    rows.push_back(Action(
        "Defaults", Fixed("Put every value back to its default."), "Reset", [] {
            MikuPan_SetBrightness(1.0f);
            MikuPan_SetGamma(1.0f);
            MikuPan_SetContrast(1.0f);
            MikuPan_SetShadowDepth(1.0f);
            MikuPan_SetHdrEnabled(0);
            MikuPan_SetHdrPaperWhite(203.0f);
            MikuPan_SetHdrPeakLuminance(1000.0f);
            MarkDirty();
        }));
    rows.push_back(Action("Done", Fixed("Keep these values. Back puts them all back as they were."),
                          "OK", KeepPanel));
    return page;
}

Page CrtPanel(void)
{
    Page page{"crt", "CRT Filter", {}};
    std::vector<Row>& rows = page.rows;

    rows.push_back(Toggle(
        "Enable", Fixed("Run the picture through the CRT filter."), CrtOn,
        [](bool on) { MikuPan_SetCrtEnabled(on ? 1 : 0); }));
    const int count = MikuPan_GetCrtFieldCount();
    for (int i = 0; i < count; i++)
    {
        const float step = MikuPan_GetCrtFieldStep(i);
        rows.push_back(When(
            Slider(MikuPan_GetCrtFieldLabel(i), Text(), MikuPan_GetCrtFieldMin(i),
                   MikuPan_GetCrtFieldMax(i), step,
                   [i] { return MikuPan_GetCrtFieldValue(i); },
                   [i](float value) { MikuPan_SetCrtFieldValue(i, value); },
                   Format(step < 0.01f ? "%.3f" : "%.2f")),
            CrtOn));
    }

    rows.push_back(Heading(""));
    rows.push_back(Action(
        "Defaults", Fixed("Put every value back to its default."), "Reset", [] {
            MikuPan_ResetCrtSettings();
            MarkDirty();
        }));
    rows.push_back(Action("Done", Fixed("Keep these values. Back puts them all back as they were."),
                          "OK", KeepPanel));
    return page;
}

/* ---------------------------------------------------------------------------
 *  Building the document.
 * ------------------------------------------------------------------------ */

std::string RowRml(const Row& row, const std::string& id)
{
    if (row.kind == RowKind::Heading)
    {
        return "<div class=\"heading\" id=\"" + id + "\">" + Escape(row.label)
               + "</div>";
    }
    if (row.kind == RowKind::Note)
    {
        return "<div class=\"note\" id=\"" + id + "\">" + Escape(row.label)
               + "</div>";
    }

    const char* kind = row.kind == RowKind::Choice   ? "choice"
                       : row.kind == RowKind::Slider ? "slider"
                       : row.kind == RowKind::Binding ? "binding"
                                                       : "action";
    std::string rml = "<div class=\"row " + std::string(kind) + "\" id=\"" + id
                      + "\"><span class=\"row-label\">" + Escape(row.label)
                      + "</span><span class=\"row-value\">";
    if (row.kind == RowKind::Action)
    {
        rml += "<span class=\"value action-value";
        rml += row.plain ? " plain" : "";
        rml += "\" id=\"" + id + "-value\"></span>";
    }
    else if (row.kind == RowKind::Binding)
    {
        rml += "<span class=\"value binding-value\" id=\"" + id + "-value\"></span>";
    }
    else
    {
        rml += "<span class=\"arrow left\">" + std::string(kArrowLeft) + "</span>";
        if (row.kind == RowKind::Slider)
        {
            rml += "<span class=\"track\" id=\"" + id + "-track\"><span class=\"fill\" id=\""
                   + id + "-fill\"></span>";
            if (row.has_neutral && row.max > row.min)
            {
                const float at = (row.neutral - row.min) / (row.max - row.min);
                rml += "<span class=\"mark\" style=\"left: "
                       + Printf("%.2f%%", static_cast<double>(at * 100.0f))
                       + ";\"></span>";
            }
            rml += "</span>";
        }
        rml += "<span class=\"value\" id=\"" + id + "-value\"></span>";
        rml += "<span class=\"arrow right\">" + std::string(kArrowRight) + "</span>";
    }
    rml += "</span></div>";
    return rml;
}

void BindRows(Page& page, const std::string& prefix)
{
    page.selected = std::min(page.selected,
                             static_cast<int>(page.rows.size()) - 1);
    for (int r = 0; r < static_cast<int>(page.rows.size()); r++)
    {
        Row& row = page.rows[static_cast<size_t>(r)];
        const std::string id = prefix + std::to_string(r);
        row.element = g_opt.document->GetElementById(id);
        row.value = g_opt.document->GetElementById(id + "-value");
        row.track = g_opt.document->GetElementById(id + "-track");
        row.fill = g_opt.document->GetElementById(id + "-fill");
        row.shown_value.clear();
        row.shown_fill = -1.0f;
        row.shown_enabled = -1;
        row.shown_flag = -1;
        if (row.element != nullptr)
        {
            g_opt.row_of[row.element] = RowRef{&page, r};
        }
    }
}

void BuildPages(void)
{
    g_opt.pages.clear();
    g_opt.pages.push_back(DisplayPage());
    g_opt.pages.push_back(GraphicsPage());
    g_opt.pages.push_back(AudioPage());
    g_opt.pages.push_back(GamePage());
    g_opt.pages.push_back(ControlsPage());
    g_opt.pages.push_back(KeyboardPage());
    g_opt.pages.push_back(ControllerPage());

    g_opt.panels.clear();
    g_opt.panels.push_back(CalibrationPanel());
    g_opt.panels.push_back(CrtPanel());

    std::string tabs;
    std::string pages;
    for (int p = 0; p < static_cast<int>(g_opt.pages.size()); p++)
    {
        const Page& page = g_opt.pages[static_cast<size_t>(p)];
        tabs += "<span class=\"tab\" id=\"tab-" + std::to_string(p) + "\">"
                + Escape(T(page.name)) + "</span>";
        pages += "<div class=\"page hidden\" id=\"page-" + std::string(page.id) + "\">";
        const std::string prefix = "row-" + std::to_string(p) + "-";
        for (int r = 0; r < static_cast<int>(page.rows.size()); r++)
        {
            pages += RowRml(page.rows[static_cast<size_t>(r)], prefix + std::to_string(r));
        }
        pages += "</div>";
    }
    g_opt.tabs->SetInnerRML(tabs);
    g_opt.pages_root->SetInnerRML(pages);

    g_opt.row_of.clear();
    g_opt.dragging = RowRef();
    for (int p = 0; p < static_cast<int>(g_opt.pages.size()); p++)
    {
        Page& page = g_opt.pages[static_cast<size_t>(p)];
        page.element = g_opt.document->GetElementById(std::string("page-") + page.id);
        page.tab = g_opt.document->GetElementById("tab-" + std::to_string(p));
        page.selected = -1;
        BindRows(page, "row-" + std::to_string(p) + "-");
    }
}

/* Only the open panel is in the document. */
void BuildPanelRows(void)
{
    if (g_opt.panel_kind == PANEL_NONE || g_opt.panel_rows == nullptr)
    {
        return;
    }
    Page& panel = g_opt.panels[static_cast<size_t>(g_opt.panel_kind)];
    if (g_opt.panel_title != nullptr)
    {
        g_opt.panel_title->SetInnerRML(Escape(T(panel.name)));
    }
    const std::string prefix = "panel-row-";
    std::string rml;
    for (int r = 0; r < static_cast<int>(panel.rows.size()); r++)
    {
        rml += RowRml(panel.rows[static_cast<size_t>(r)], prefix + std::to_string(r));
    }
    g_opt.panel_rows->SetInnerRML(rml);
    BindRows(panel, prefix);
}

/* ---------------------------------------------------------------------------
 *  Values.
 * ------------------------------------------------------------------------ */

float SliderFraction(const Row& row, float value)
{
    return row.max > row.min ? (value - row.min) / (row.max - row.min) : 0.0f;
}

float SliderFromFraction(const Row& row, float fraction)
{
    fraction = std::clamp(fraction, 0.0f, 1.0f);
    const float value = row.min + fraction * (row.max - row.min);
    return row.min + std::round((value - row.min) / row.step) * row.step;
}

void Changed(void)
{
    MarkDirty();
}

bool SetSlider(Row& row, float value)
{
    value = std::clamp(value, row.min, row.max);
    if (std::fabs(value - row.get_value()) < row.step * 0.01f)
    {
        return false;
    }
    row.set_value(value);
    Changed();
    return true;
}

int StepChoice(const Row& row, int from, int direction)
{
    const int count = row.Count();
    int index = from;
    for (int i = 0; i < count; i++)
    {
        index = (index + direction + count) % count;
        if (row.Choosable(index))
        {
            return index;
        }
    }
    return from;
}

/* Left (-1) or right (+1).  Choices go round; sliders stop at their ends,
 * and the tick only sounds when something moved. */
void Adjust(Row& row, int direction)
{
    if (!row.IsEnabled())
    {
        return;
    }
    if (row.kind == RowKind::Choice)
    {
        const int count = row.Count();
        if (count <= 1)
        {
            return;
        }
        const int current = std::clamp(row.get(), 0, count - 1);
        if (row.deferred)
        {
            const int from = row.pending >= 0 ? row.pending : current;
            const int next = StepChoice(row, from, direction);
            row.pending = next == current ? -1 : next;
            PlayCursor();
            return;
        }
        const int next = StepChoice(row, current, direction);
        if (next != current)
        {
            row.set(next);
            Changed();
            PlayCursor();
        }
    }
    else if (row.kind == RowKind::Slider)
    {
        const float before = row.get_value();
        /* On the step grid, so repeated steps do not drift. */
        const float steps = std::round((before - row.min) / row.step);
        const float value = row.min + (steps + static_cast<float>(direction)) * row.step;
        if (SetSlider(row, value))
        {
            PlayCursor();
        }
    }
}

/* A window or render size: applied, then kept only if the player says so
 * within 15 seconds. */
void ApplyDeferred(Row& row)
{
    const int target = row.pending;
    row.pending = -1;
    const int before = row.get();
    if (target < 0 || target == before)
    {
        return;
    }
    row.set(target);
    Changed();
    PlayDecide();
    std::function<void(int)> set = row.set;
    ConfirmOpen(T("Keep this display setting?"), "", nullptr,
                [set, before] { set(before); }, kDisplayRevertMs);
}

void Activate(Row& row)
{
    if (!row.IsEnabled())
    {
        return;
    }
    switch (row.kind)
    {
        case RowKind::Choice:
            if (row.deferred && row.pending >= 0)
            {
                ApplyDeferred(row);
            }
            else
            {
                Adjust(row, 1);
            }
            break;
        case RowKind::Action:
            PlayDecide();
            row.run();
            break;
        case RowKind::Binding:
            PlayDecide();
            row.capture();
            break;
        default:
            break;
    }
}

void ClearBinding(Row& row)
{
    if (row.kind != RowKind::Binding || !row.IsEnabled() || !row.bound())
    {
        return;
    }
    row.clear();
    Changed();
    PlayCancel();
}

std::string ValueText(const Row& row)
{
    switch (row.kind)
    {
        case RowKind::Choice:
        {
            const int count = std::max(row.Count(), 1);
            const int index = row.deferred && row.pending >= 0
                                  ? row.pending
                                  : std::clamp(row.get(), 0, count - 1);
            return row.NameOf(index);
        }
        case RowKind::Slider:
            return row.format(row.get_value());
        case RowKind::Action:
            return row.action_text ? row.action_text() : row.action;
        case RowKind::Binding:
            return row.binding_text();
        default:
            return "";
    }
}

void RefreshRow(Row& row)
{
    if (row.element == nullptr || row.kind == RowKind::Heading)
    {
        return;
    }
    if (row.kind == RowKind::Note)
    {
        const int shown = !row.shown || row.shown() ? 1 : 0;
        if (shown != row.shown_flag)
        {
            row.element->SetClass("hidden", shown == 0);
            row.shown_flag = shown;
        }
        return;
    }

    const int enabled = row.IsEnabled() ? 1 : 0;
    if (enabled != row.shown_enabled)
    {
        row.element->SetClass("disabled", enabled == 0);
        row.shown_enabled = enabled;
    }

    /* The one class a row kind keeps for itself: a choice waiting for Enter,
     * a binding with nothing in it. */
    int flag = 0;
    if (row.kind == RowKind::Choice)
    {
        flag = row.deferred && row.pending >= 0 ? 1 : 0;
        if (flag != row.shown_flag)
        {
            row.element->SetClass("pending", flag != 0);
        }
    }
    else if (row.kind == RowKind::Binding)
    {
        flag = row.bound() ? 0 : 1;
        if (flag != row.shown_flag)
        {
            row.element->SetClass("unbound", flag != 0);
        }
    }
    row.shown_flag = flag;

    const std::string text = ValueText(row);
    if (row.value != nullptr && text != row.shown_value)
    {
        row.value->SetInnerRML(Escape(text));
        row.shown_value = text;
    }
    if (row.fill != nullptr)
    {
        const float fraction =
            std::clamp(SliderFraction(row, row.get_value()), 0.0f, 1.0f);
        if (std::fabs(fraction - row.shown_fill) > 0.001f)
        {
            row.fill->SetProperty("width",
                                  Printf("%.2f%%", static_cast<double>(fraction * 100.0f)));
            row.shown_fill = fraction;
        }
    }
}

/* ---------------------------------------------------------------------------
 *  The cursor and the sections.
 * ------------------------------------------------------------------------ */

Page& CurrentPage(void)
{
    if (g_opt.panel_kind != PANEL_NONE)
    {
        return g_opt.panels[static_cast<size_t>(g_opt.panel_kind)];
    }
    return g_opt.pages[static_cast<size_t>(g_opt.page)];
}

Row* SelectedRow(void)
{
    if (g_opt.pages.empty())
    {
        return nullptr;
    }
    Page& page = CurrentPage();
    if (page.selected < 0 || page.selected >= static_cast<int>(page.rows.size()))
    {
        return nullptr;
    }
    return &page.rows[static_cast<size_t>(page.selected)];
}

void Select(int index, bool scroll)
{
    Page& page = CurrentPage();
    if (page.selected >= 0 && page.selected < static_cast<int>(page.rows.size()))
    {
        Row& previous = page.rows[static_cast<size_t>(page.selected)];
        if (page.selected != index)
        {
            /* A display change not applied goes when the cursor leaves it. */
            previous.pending = -1;
        }
        if (previous.element != nullptr)
        {
            previous.element->SetClass("selected", false);
        }
    }
    page.selected = index;
    if (index < 0 || index >= static_cast<int>(page.rows.size()))
    {
        return;
    }
    Rml::Element* element = page.rows[static_cast<size_t>(index)].element;
    if (element == nullptr)
    {
        return;
    }
    element->SetClass("selected", true);
    if (scroll)
    {
        /* The heading above a section's first row comes into view with it,
         * so the cursor never sits under an unlabelled group. */
        if (index > 0)
        {
            const Row& above = page.rows[static_cast<size_t>(index - 1)];
            if (above.kind == RowKind::Heading && above.element != nullptr)
            {
                above.element->ScrollIntoView(Rml::ScrollAlignment::Nearest);
            }
        }
        element->ScrollIntoView(Rml::ScrollAlignment::Nearest);
    }
}

int NextSelectable(const Page& page, int from, int direction)
{
    for (int i = from + direction; i >= 0 && i < static_cast<int>(page.rows.size());
         i += direction)
    {
        if (page.rows[static_cast<size_t>(i)].Selectable())
        {
            return i;
        }
    }
    return -1;
}

void MoveCursor(int direction)
{
    Page& page = CurrentPage();
    int next = NextSelectable(page, page.selected, direction);
    if (next < 0)
    {
        /* Round from the last row to the first and back. */
        next = NextSelectable(page,
                              direction > 0 ? -1 : static_cast<int>(page.rows.size()),
                              direction);
    }
    if (next >= 0 && next != page.selected)
    {
        Select(next, true);
        PlayCursor();
    }
}

void ShowPage(int index, bool sound)
{
    const int count = static_cast<int>(g_opt.pages.size());
    if (count == 0)
    {
        return;
    }
    index = (index % count + count) % count;
    if (index != g_opt.page && g_opt.page >= 0 && g_opt.page < count)
    {
        for (Row& row : g_opt.pages[static_cast<size_t>(g_opt.page)].rows)
        {
            row.pending = -1;
        }
    }
    for (int p = 0; p < count; p++)
    {
        Page& page = g_opt.pages[static_cast<size_t>(p)];
        if (page.element != nullptr)
        {
            page.element->SetClass("hidden", p != index);
        }
        if (page.tab != nullptr)
        {
            page.tab->SetClass("active", p == index);
        }
    }
    g_opt.page = index;

    Page& page = CurrentPage();
    for (Row& row : page.rows)
    {
        RefreshRow(row);
    }
    int selected = page.selected;
    if (selected < 0 || !page.rows[static_cast<size_t>(selected)].Selectable())
    {
        selected = NextSelectable(page, -1, 1);
    }
    Select(selected, true);
    if (selected >= 0 && selected == NextSelectable(page, -1, 1)
        && page.element != nullptr)
    {
        /* Back to the top, headings and notes included. */
        page.element->ScrollIntoView(Rml::ScrollAlignment::Start);
    }
    if (sound)
    {
        PlayCursor();
    }
}

/* Leaving saves, when anything changed. */
void RequestExit(void)
{
    if (g_opt.dirty && !MikuPan_SaveConfigurationFromUi())
    {
        ConfirmOpen(T("The settings could not be saved."), T("Leave anyway?"),
                    [] { g_opt.exit_requested = true; }, nullptr);
        return;
    }
    g_opt.dirty = false;
    g_opt.exit_requested = true;
}

/* ---------------------------------------------------------------------------
 *  Input.
 * ------------------------------------------------------------------------ */

bool InputBlocked(void)
{
    return g_opt.open && SDL_GetTicks() < g_opt.input_block_until;
}

void HandleVertical(int direction)
{
    if (g_opt.confirm.open)
    {
        return;
    }
    MoveCursor(direction);
}

void HandleHorizontal(int direction)
{
    if (g_opt.confirm.open)
    {
        ConfirmSelect(!g_opt.confirm.yes_selected);
        PlayCursor();
        return;
    }
    if (Row* row = SelectedRow())
    {
        Adjust(*row, direction);
    }
}

void HandleActivate(void)
{
    if (g_opt.confirm.open)
    {
        ConfirmAnswer(g_opt.confirm.yes_selected);
        return;
    }
    if (Row* row = SelectedRow())
    {
        Activate(*row);
    }
}

void HandleClear(void)
{
    if (g_opt.confirm.open)
    {
        return;
    }
    if (Row* row = SelectedRow())
    {
        ClearBinding(*row);
    }
}

void HandleSection(int direction)
{
    if (g_opt.confirm.open || g_opt.panel_kind != PANEL_NONE)
    {
        return;
    }
    ShowPage(g_opt.page + direction, true);
}

void HandleBack(void)
{
    if (g_opt.confirm.open)
    {
        ConfirmAnswer(false);
        return;
    }
    if (Row* row = SelectedRow(); row != nullptr && row->pending >= 0)
    {
        row->pending = -1;
        PlayCancel();
        return;
    }
    if (g_opt.panel_kind != PANEL_NONE)
    {
        CancelPanel();
        return;
    }
    RequestExit();
}

void HandleKey(Rml::Event& event)
{
    const int key = event.GetParameter<int>("key_identifier", Rml::Input::KI_UNKNOWN);
    const bool shift = event.GetParameter<int>("shift_key", 0) != 0;
    switch (key)
    {
        case Rml::Input::KI_UP:
            HandleVertical(-1);
            break;
        case Rml::Input::KI_DOWN:
            HandleVertical(1);
            break;
        case Rml::Input::KI_LEFT:
            HandleHorizontal(-1);
            break;
        case Rml::Input::KI_RIGHT:
            HandleHorizontal(1);
            break;
        case Rml::Input::KI_RETURN:
        case Rml::Input::KI_NUMPADENTER:
        case Rml::Input::KI_SPACE:
            HandleActivate();
            break;
        case Rml::Input::KI_DELETE:
        case Rml::Input::KI_BACK:
            HandleClear();
            break;
        case Rml::Input::KI_HOME:
            if (!g_opt.confirm.open)
            {
                Select(NextSelectable(CurrentPage(), -1, 1), true);
            }
            break;
        case Rml::Input::KI_END:
            if (!g_opt.confirm.open)
            {
                Select(NextSelectable(CurrentPage(),
                                      static_cast<int>(CurrentPage().rows.size()), -1),
                       true);
            }
            break;
        case Rml::Input::KI_PRIOR:
        case Rml::Input::KI_Q:
            HandleSection(-1);
            break;
        case Rml::Input::KI_NEXT:
        case Rml::Input::KI_E:
            HandleSection(1);
            break;
        case Rml::Input::KI_TAB:
            HandleSection(shift ? -1 : 1);
            break;
        case Rml::Input::KI_ESCAPE:
            HandleBack();
            break;
        default:
            return;
    }
    event.StopPropagation();
}

bool HasAncestorClass(Rml::Element* element, const char* name, Rml::Element** found)
{
    for (; element != nullptr; element = element->GetParentNode())
    {
        if (element->IsClassSet(name))
        {
            if (found != nullptr)
            {
                *found = element;
            }
            return true;
        }
    }
    return false;
}

Rml::Element* AncestorWithId(Rml::Element* element, const char* id)
{
    for (; element != nullptr; element = element->GetParentNode())
    {
        if (element->GetId() == id)
        {
            return element;
        }
    }
    return nullptr;
}

/* The row an event happened in, walking up from its target. */
Row* RowAt(Rml::Element* element, RowRef* ref_out)
{
    for (; element != nullptr; element = element->GetParentNode())
    {
        auto found = g_opt.row_of.find(element);
        if (found != g_opt.row_of.end())
        {
            if (ref_out != nullptr)
            {
                *ref_out = found->second;
            }
            return &found->second.page->rows[static_cast<size_t>(found->second.row)];
        }
    }
    return nullptr;
}

void DragTo(Row& row, float mouse_x)
{
    if (row.track == nullptr)
    {
        return;
    }
    const float left = row.track->GetAbsoluteOffset(Rml::BoxArea::Border).x;
    const float width = row.track->GetBox().GetSize(Rml::BoxArea::Border).x;
    if (width <= 0.0f)
    {
        return;
    }
    (void) SetSlider(row, SliderFromFraction(row, (mouse_x - left) / width));
}

void HandlePointer(Rml::Event& event)
{
    Rml::Element* target = event.GetTargetElement();
    const float mouse_x = event.GetParameter<float>("mouse_x", 0.0f);

    if (event == Rml::EventId::Mouseup)
    {
        g_opt.dragging = RowRef();
        return;
    }
    bool hover = event == Rml::EventId::Mouseover;
    if (event == Rml::EventId::Mousemove)
    {
        if (g_opt.dragging.page == &CurrentPage() && g_opt.dragging.row >= 0)
        {
            DragTo(g_opt.dragging.page->rows[static_cast<size_t>(g_opt.dragging.row)],
                   mouse_x);
        }
        /* The first move after the keys or the pad takes the cursor to the
         * row under the mouse; after that only entering a row does. */
        if (g_opt.pointer_active)
        {
            return;
        }
        g_opt.pointer_active = true;
        hover = true;
    }
    if (CaptureActive() || InputBlocked())
    {
        return;
    }

    if (g_opt.confirm.open)
    {
        Rml::Element* yes = AncestorWithId(target, "confirm-yes");
        Rml::Element* no = AncestorWithId(target, "confirm-no");
        if (yes == nullptr && no == nullptr)
        {
            return;
        }
        if (hover)
        {
            if (g_opt.pointer_active && (yes != nullptr) != g_opt.confirm.yes_selected)
            {
                ConfirmSelect(yes != nullptr);
            }
        }
        else if (event == Rml::EventId::Click)
        {
            ConfirmAnswer(yes != nullptr);
        }
        return;
    }

    RowRef ref;
    Row* row = RowAt(target, &ref);
    const bool on_current = row != nullptr && ref.page == &CurrentPage();
    if (hover)
    {
        if (g_opt.pointer_active && on_current && row->Selectable()
            && ref.row != ref.page->selected)
        {
            Select(ref.row, false);
        }
        return;
    }
    if (event == Rml::EventId::Mousedown)
    {
        const int button = event.GetParameter<int>("button", 0);
        if (on_current && button == 1)
        {
            /* The right button clears a binding. */
            ClearBinding(*row);
        }
        else if (on_current && button == 0 && row->kind == RowKind::Slider
                 && row->IsEnabled() && HasAncestorClass(target, "track", nullptr))
        {
            g_opt.dragging = ref;
            DragTo(*row, mouse_x);
        }
        return;
    }

    /* Click. */
    Rml::Element* tagged = nullptr;
    if (HasAncestorClass(target, "tab", &tagged))
    {
        const int index = std::atoi(tagged->GetId().c_str() + 4);
        if (index != g_opt.page)
        {
            ShowPage(index, true);
        }
        return;
    }
    if (HasAncestorClass(target, "prompt", &tagged))
    {
        const Rml::String& id = tagged->GetId();
        if (id == "prompt-back")
        {
            HandleBack();
        }
        else if (id == "prompt-section")
        {
            HandleSection(1);
        }
        else if (id == "prompt-clear")
        {
            HandleClear();
        }
        else if (id == "prompt-select")
        {
            HandleActivate();
        }
        return;
    }
    if (!on_current)
    {
        return;
    }
    if (ref.row != ref.page->selected && row->Selectable())
    {
        Select(ref.row, false);
    }
    if (HasAncestorClass(target, "arrow", &tagged))
    {
        Adjust(*row, tagged->IsClassSet("left") ? -1 : 1);
    }
    else if (row->kind != RowKind::Slider)
    {
        Activate(*row);
    }
}

void Listener::ProcessEvent(Rml::Event& event)
{
    if (!g_opt.open)
    {
        return;
    }
    if (event == Rml::EventId::Keydown)
    {
        if (!CaptureActive() && !InputBlocked())
        {
            g_opt.pointer_active = false;
            HandleKey(event);
        }
        return;
    }
    HandlePointer(event);
}

/* ---------------------------------------------------------------------------
 *  Help and prompts.
 * ------------------------------------------------------------------------ */

std::string PadButtonName(SDL_GamepadButton button)
{
    SDL_Gamepad* gp = MikuPan_GetController();
    if (button == SDL_GAMEPAD_BUTTON_LEFT_SHOULDER
        || button == SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER)
    {
        const bool left = button == SDL_GAMEPAD_BUTTON_LEFT_SHOULDER;
        switch (gp != nullptr ? SDL_GetGamepadType(gp) : SDL_GAMEPAD_TYPE_UNKNOWN)
        {
            case SDL_GAMEPAD_TYPE_PS3:
            case SDL_GAMEPAD_TYPE_PS4:
            case SDL_GAMEPAD_TYPE_PS5:
                return left ? "L1" : "R1";
            case SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_PRO:
            case SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_JOYCON_PAIR:
                return left ? "L" : "R";
            default:
                return left ? "LB" : "RB";
        }
    }

    switch (gp != nullptr ? SDL_GetGamepadButtonLabel(gp, button)
                          : SDL_GAMEPAD_BUTTON_LABEL_UNKNOWN)
    {
        case SDL_GAMEPAD_BUTTON_LABEL_A:
            return "A";
        case SDL_GAMEPAD_BUTTON_LABEL_B:
            return "B";
        case SDL_GAMEPAD_BUTTON_LABEL_X:
            return "X";
        case SDL_GAMEPAD_BUTTON_LABEL_Y:
            return "Y";
        case SDL_GAMEPAD_BUTTON_LABEL_CROSS:
            return T("Cross");
        case SDL_GAMEPAD_BUTTON_LABEL_CIRCLE:
            return T("Circle");
        case SDL_GAMEPAD_BUTTON_LABEL_SQUARE:
            return T("Square");
        case SDL_GAMEPAD_BUTTON_LABEL_TRIANGLE:
            return T("Triangle");
        default:
            break;
    }
    switch (button)
    {
        case SDL_GAMEPAD_BUTTON_SOUTH:
            return "A";
        case SDL_GAMEPAD_BUTTON_EAST:
            return "B";
        case SDL_GAMEPAD_BUTTON_WEST:
            return "X";
        default:
            return "Y";
    }
}

struct Prompt
{
    const char* key;
    SDL_GamepadButton pad;
    SDL_GamepadButton pad_pair;
    const char* action;
    const char* id;
};

std::string PromptsRml(const std::vector<Prompt>& prompts)
{
    std::string rml;
    for (const Prompt& prompt : prompts)
    {
        std::string key = g_opt.using_pad ? PadButtonName(prompt.pad) : T(prompt.key);
        if (g_opt.using_pad && prompt.pad_pair != SDL_GAMEPAD_BUTTON_INVALID)
        {
            key += " / " + PadButtonName(prompt.pad_pair);
        }
        rml += "<span class=\"prompt\"";
        if (prompt.id != nullptr)
        {
            rml += " id=\"" + std::string(prompt.id) + "\"";
        }
        rml += "><span class=\"key\">" + Escape(key) + "</span>"
               + Escape(T(prompt.action)) + "</span>";
    }
    return rml;
}

void SetIfChanged(Rml::Element* element, std::string& shown, const std::string& rml)
{
    if (element != nullptr && rml != shown)
    {
        element->SetInnerRML(rml);
        shown = rml;
    }
}

void RefreshHelpAndPrompts(void)
{
    std::string help;
    const Row* row = SelectedRow();
    if (!g_opt.confirm.open && !CaptureActive() && row != nullptr && row->help)
    {
        help = row->help();
        if (row->kind == RowKind::Choice && row->deferred && row->pending >= 0)
        {
            help += " " + T("Confirm to apply.");
        }
        if (!row->IsEnabled() && !help.empty())
        {
            help += " " + T("(Not available with the current settings.)");
        }
    }

    std::vector<Prompt> prompts;
    if (g_opt.confirm.open)
    {
        prompts.push_back({"Enter", SDL_GAMEPAD_BUTTON_SOUTH, SDL_GAMEPAD_BUTTON_INVALID,
                           "Confirm", "prompt-select"});
        prompts.push_back({"Esc", SDL_GAMEPAD_BUTTON_EAST, SDL_GAMEPAD_BUTTON_INVALID,
                           "Cancel", "prompt-back"});
    }
    else if (!CaptureActive())
    {
        if (g_opt.panel_kind == PANEL_NONE)
        {
            prompts.push_back({"Q / E", SDL_GAMEPAD_BUTTON_LEFT_SHOULDER,
                               SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER, "Section",
                               "prompt-section"});
        }
        if (row != nullptr && row->kind == RowKind::Binding)
        {
            prompts.push_back({"Del", SDL_GAMEPAD_BUTTON_WEST, SDL_GAMEPAD_BUTTON_INVALID,
                               "Unbind", "prompt-clear"});
        }
        const bool apply = row != nullptr && row->deferred && row->pending >= 0;
        prompts.push_back({"Enter", SDL_GAMEPAD_BUTTON_SOUTH, SDL_GAMEPAD_BUTTON_INVALID,
                           apply ? "Apply" : "Select", "prompt-select"});
        prompts.push_back({"Esc", SDL_GAMEPAD_BUTTON_EAST, SDL_GAMEPAD_BUTTON_INVALID,
                           g_opt.panel_kind != PANEL_NONE ? "Cancel" : "Back",
                           "prompt-back"});
    }
    const std::string prompts_rml = PromptsRml(prompts);

    if (g_opt.panel_kind != PANEL_NONE)
    {
        SetIfChanged(g_opt.panel_help, g_opt.shown_panel_help, Escape(help));
        SetIfChanged(g_opt.panel_prompts, g_opt.shown_panel_prompts, prompts_rml);
    }
    else
    {
        SetIfChanged(g_opt.help, g_opt.shown_help, Escape(help));
        SetIfChanged(g_opt.prompts, g_opt.shown_prompts, prompts_rml);
    }
}

void ApplyStaticText(void)
{
    if (g_opt.document == nullptr)
    {
        return;
    }
    if (g_opt.title != nullptr)
    {
        g_opt.title->SetInnerRML(Escape(T("Options")));
    }
    static const std::pair<const char*, const char*> kTexts[] = {
        {"levels-title", "Grey Levels"},
        {"levels-black", "Black"},
        {"levels-white", "White"},
        {"confirm-yes", "Yes"},
        {"confirm-no", "No"},
    };
    for (const auto& [id, text] : kTexts)
    {
        if (Rml::Element* element = g_opt.document->GetElementById(id))
        {
            element->SetInnerRML(Escape(T(text)));
        }
    }
}

/* The film is built from the disc the first time it is asked for, so it can
 * arrive a few frames after the screen opens.  Its source is only given
 * while the screen is shown: an <img> handed one while its document is
 * hidden is never drawn. */
void UpdateFilm(void)
{
    if (g_opt.film_applied || g_opt.film_strip == nullptr || !g_opt.visible)
    {
        return;
    }
    const char* source = MikuPan_RmlFilmHeaderSource();
    if (source == nullptr || source[0] == '\0')
    {
        return;
    }
    g_opt.film_strip->SetAttribute("src", source);
    g_opt.film_strip->SetClass("hidden", false);
    g_opt.film_applied = true;
}

void ApplyFontClass(int font)
{
    /* The third font is URW Chancery, standing in for the PS2 menus'
     * Zapf Chancery; the others are Century Old Style. */
    SetBodyClass("font-zapf", font == 2);
}

void ResetForOpen(bool in_game)
{
    g_opt.in_game = in_game;
    g_opt.exit_requested = false;
    g_opt.dirty = false;
    g_opt.dragging = RowRef();
    g_opt.capture = Capture();
    g_opt.shown_help.clear();
    g_opt.shown_prompts.clear();
    g_opt.shown_panel_help.clear();
    g_opt.shown_panel_prompts.clear();
    g_opt.panel_kind = PANEL_NONE;
    ConfirmClose();
    SetBodyClass("ingame", in_game);
    SetBodyClass("panel-open", false);
    if (g_opt.panel != nullptr)
    {
        g_opt.panel->SetClass("hidden", true);
    }
    if (g_opt.levels != nullptr)
    {
        g_opt.levels->SetClass("hidden", true);
    }
    ApplyFontClass(MikuPan_GetSelectedFontOption());
    ApplyStaticText();
}

void OpenInternal(bool in_game, bool interactive)
{
    if (!g_opt.initialized || g_opt.document == nullptr)
    {
        return;
    }
    ResetForOpen(in_game);
    g_opt.open = interactive;
    g_opt.visible = true;
    g_opt.input_block_until = SDL_GetTicks() + kOpenInputBlockMs;
    g_opt.page = 0;
    /* Rebuilt on every open: the choices depend on the window, the display
     * and the language. */
    MikuPan_RefreshWindowSizeListLabels();
    MikuPan_RefreshRenderResolutionListLabels();
    MikuPan_RefreshGpuDriverListLabels();
    BuildPages();
    g_opt.document->Show(Rml::ModalFlag::None, Rml::FocusFlag::Document);
    UpdateFilm();
    ShowPage(0, false);
    RefreshHelpAndPrompts();
}

void CloseInternal(void)
{
    if (CaptureActive())
    {
        g_opt.capture = Capture();
        MikuPan_InputBindingCaptureReset();
    }
    ConfirmClose();
    if (g_opt.panel_kind != PANEL_NONE)
    {
        g_opt.panel_kind = PANEL_NONE;
        SetBodyClass("panel-open", false);
            if (g_opt.panel != nullptr)
        {
            g_opt.panel->SetClass("hidden", true);
        }
        if (g_opt.levels != nullptr)
        {
            g_opt.levels->SetClass("hidden", true);
        }
    }
    g_opt.open = false;
    g_opt.visible = false;
    g_opt.exit_requested = false;
    g_opt.dragging = RowRef();
    if (g_opt.film_strip != nullptr)
    {
        g_opt.film_strip->RemoveAttribute("src");
        g_opt.film_strip->SetClass("hidden", true);
        g_opt.film_applied = false;
    }
    if (g_opt.document != nullptr)
    {
        g_opt.document->Hide();
    }
}

bool LoadOptionsDocument(void)
{
    char document_path[1024];
    if (!MikuPan_ResolveBasePath(kDocumentPath, document_path, sizeof(document_path)))
    {
        return false;
    }
    g_opt.document = g_opt.context->LoadDocument(document_path);
    if (g_opt.document == nullptr)
    {
        return false;
    }

    auto get = [](const char* id) { return g_opt.document->GetElementById(id); };
    g_opt.body = g_opt.document;
    g_opt.film_strip = get("film-strip");
    g_opt.title = get("title");
    g_opt.tabs = get("tabs");
    g_opt.pages_root = get("pages");
    g_opt.help = get("help");
    g_opt.prompts = get("prompts");
    g_opt.panel = get("panel");
    g_opt.panel_title = get("panel-title");
    g_opt.panel_rows = get("panel-rows");
    g_opt.panel_help = get("panel-help");
    g_opt.panel_prompts = get("panel-prompts");
    g_opt.levels = get("levels");
    g_opt.confirm.root = get("confirm");
    g_opt.confirm.text = get("confirm-text");
    g_opt.confirm.note = get("confirm-note");
    g_opt.confirm.choices = get("confirm-choices");
    g_opt.confirm.yes = get("confirm-yes");
    g_opt.confirm.no = get("confirm-no");
    if (g_opt.tabs == nullptr || g_opt.pages_root == nullptr)
    {
        g_opt.document->Close();
        g_opt.document = nullptr;
        return false;
    }

    if (Rml::Element* strip = get("levels-strip"))
    {
        size_t swatch = 0;
        for (int i = 0; i < strip->GetNumChildren() && swatch < g_opt.level_swatches.size(); i++)
        {
            Rml::Element* child = strip->GetChild(i);
            if (child != nullptr && child->IsClassSet("level"))
            {
                g_opt.level_swatches[swatch++] = child;
            }
        }
    }

    for (Rml::EventId id : {Rml::EventId::Keydown, Rml::EventId::Click,
                            Rml::EventId::Mouseover, Rml::EventId::Mousedown,
                            Rml::EventId::Mousemove, Rml::EventId::Mouseup})
    {
        g_opt.document->AddEventListener(id, &g_listener);
    }
    g_opt.document->Hide();
    return true;
}
} // namespace

bool MikuPan_RmlOptionsInit(Rml::Context* context)
{
    if (g_opt.initialized)
    {
        return true;
    }
    g_opt.context = context;
    if (g_opt.context == nullptr || !LoadOptionsDocument())
    {
        g_opt = OptionsState();
        return false;
    }
    g_opt.initialized = true;
    ApplyStaticText();
    return true;
}

void MikuPan_RmlOptionsStartFrame(void)
{
    if (!g_opt.initialized || !g_opt.visible || g_opt.pages.empty())
    {
        return;
    }

    UpdateFilm();
    UpdateCapture();
    ConfirmUpdate();

    /* Every frame, because a setting can move under the screen -- Alt+Enter,
     * the overlay -- and one that enables another greys or frees it. */
    Page& page = CurrentPage();
    for (Row& row : page.rows)
    {
        RefreshRow(row);
    }
    if (const Row* row = SelectedRow(); row == nullptr || !row->Selectable())
    {
        const int next = NextSelectable(page, page.selected, 1);
        Select(next >= 0 ? next : NextSelectable(page, page.selected, -1), true);
    }
    if (g_opt.panel_kind == PANEL_CALIBRATION)
    {
        RefreshLevels();
    }
    RefreshHelpAndPrompts();
}

void MikuPan_RmlOptionsPrepareShutdown(void)
{
    /* Rml::Shutdown() is about to take the document and every element with
     * it: only drop the references here.  The listener is a static and
     * outlives it. */
    g_opt.row_of.clear();
    g_opt.pages.clear();
    g_opt.panels.clear();
    g_opt.film_strip = nullptr;
    g_opt.confirm = Confirm();
    g_opt.capture = Capture();
    g_opt.dragging = RowRef();
    g_opt.open = false;
    g_opt.visible = false;
    g_opt.exit_requested = false;
}

void MikuPan_RmlOptionsShutdown(void)
{
    g_opt = OptionsState();
}

extern "C" {

void MikuPan_RmlOptionsOpen(void)
{
    OpenInternal(false, true);
}

void MikuPan_RmlOptionsOpenInGame(void)
{
    OpenInternal(true, true);
}

void MikuPan_RmlOptionsClose(void)
{
    if (!g_opt.initialized)
    {
        return;
    }
    CloseInternal();
}

int MikuPan_RmlOptionsConsumeExitRequest(void)
{
    const int requested = g_opt.exit_requested ? 1 : 0;
    g_opt.exit_requested = false;
    return requested;
}

int MikuPan_RmlOptionsIsOpen(void)
{
    return g_opt.open ? 1 : 0;
}

int MikuPan_RmlOptionsIsCalibrationOpen(void)
{
    return g_opt.open && g_opt.panel_kind == PANEL_CALIBRATION ? 1 : 0;
}

int MikuPan_RmlOptionsInputBlocked(void)
{
    return InputBlocked() ? 1 : 0;
}

int MikuPan_RmlOptionsBindingCaptureActive(void)
{
    return g_opt.open && CaptureActive() ? 1 : 0;
}

void MikuPan_RmlOptionsRequestExit(void)
{
    if (g_opt.open)
    {
        RequestExit();
    }
}

void MikuPan_RmlOptionsHandleCancel(void)
{
    if (!g_opt.open || InputBlocked() || CaptureActive())
    {
        return;
    }
    g_opt.pointer_active = false;
    HandleBack();
}

int MikuPan_RmlOptionsActivateFocusedControl(void)
{
    if (g_opt.open && !InputBlocked() && !CaptureActive())
    {
        g_opt.pointer_active = false;
        HandleActivate();
    }
    return 1;
}

void MikuPan_RmlOptionsHandleAltConfirm(void)
{
    if (g_opt.open && !InputBlocked() && !CaptureActive())
    {
        g_opt.pointer_active = false;
        HandleClear();
    }
}

void MikuPan_RmlOptionsHandleSection(int direction)
{
    if (g_opt.open && !InputBlocked() && !CaptureActive())
    {
        HandleSection(direction < 0 ? -1 : 1);
    }
}

int MikuPan_RmlOptionsHandleHorizontalInput(int direction)
{
    if (g_opt.open && !InputBlocked() && !CaptureActive())
    {
        g_opt.pointer_active = false;
        HandleHorizontal(direction < 0 ? -1 : 1);
    }
    return 1;
}

int MikuPan_RmlOptionsHandleVerticalInput(int direction)
{
    if (g_opt.open && !InputBlocked() && !CaptureActive())
    {
        g_opt.pointer_active = false;
        HandleVertical(direction < 0 ? -1 : 1);
    }
    return 1;
}

void MikuPan_RmlOptionsNoteInputDevice(int pad)
{
    g_opt.using_pad = pad != 0;
}

/* The screen has the one look; the theme is the ImGui overlay's alone. */
void MikuPan_RmlOptionsApplyTheme(int theme)
{
    (void) theme;
}

void MikuPan_RmlOptionsApplyFont(int font)
{
    ApplyFontClass(font);
}

void MikuPan_RmlOptionsApplyLanguage(int language)
{
    (void) language;
    if (!g_opt.initialized)
    {
        return;
    }
    MikuPan_RefreshWindowSizeListLabels();
    MikuPan_RefreshRenderResolutionListLabels();
    MikuPan_RefreshGpuDriverListLabels();
    ApplyStaticText();
    if (g_opt.visible && !g_opt.pages.empty())
    {
        const int page = g_opt.page;
        const int panel = g_opt.panel_kind;
        BuildPages();
        g_opt.panel_kind = panel;
        BuildPanelRows();
        g_opt.shown_help.clear();
        g_opt.shown_prompts.clear();
        g_opt.shown_panel_help.clear();
        g_opt.shown_panel_prompts.clear();
        ShowPage(page, false);
    }
}

/* The hi-window debug toggle: the screen shown without taking the input. */
void MikuPan_RmlOptionsToggleDebug(void)
{
    if (!g_opt.initialized || g_opt.open)
    {
        return;
    }
    if (g_opt.visible)
    {
        CloseInternal();
    }
    else
    {
        OpenInternal(false, false);
    }
}
}
