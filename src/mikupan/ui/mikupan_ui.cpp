// C++ wrapper that compiles imgui backends and exposes them with C linkage
// so they can be called from mikupan_ui.c (pure C).
#include "SDL3/SDL.h"
#include "imgui.h"
#include "imgui_internal.h"
#include "imgui_impl_sdl3.h"
#include "imgui_impl_sdlgpu3.h"
#include "mikupan/rendering/mikupan_gpu.h"

// Remap Dear ImGui's gamepad "window menu" trigger (the button that lets you
// move/cycle windows) away from its hardcoded default of Square / Xbox-X
// (ImGuiKey_NavGamepadMenu == ImGuiKey_GamepadFaceLeft) onto an L1 + R1 chord,
// freeing Square for in-game use.
//
// Gesture model:
//   * Press L1 + R1 together           -> open the windowing menu.
//   * Keep one shoulder held, tap the   -> cycle the focused/target window
//     other                               (one step per tap, no repeat).
//   * Release BOTH shoulders            -> confirm: ImGui applies focus to the
//                                          highlighted window and closes.
//   * D-pad / left stick while open     -> move the focused window.
//
// Why a state machine: ImGui keeps the menu open only while FaceLeft (the menu
// key) is down, and it cycles the target with IsKeyPressed(L1/R1) which has
// key-repeat enabled.  If we fed both shoulders straight through, a held chord
// would repeat-cycle every frame, and you could never tap a shoulder without
// breaking the both-held chord.  So we synthesise the ImGui inputs:
//   FaceLeft = "menu open" latch (held while at least one shoulder is down
//              after the chord opened it)
//   L1 / R1  = single one-frame pulses, emitted only on a fresh physical press
//              while the menu is already open (so cycling is one step per tap).
//
// FaceLeft has no other role in ImGui gamepad nav, and L1/R1's only role is
// this cycling, so fully synthesising them is safe.  We rewrite the backend's
// queued events in place (rather than appending) so input-event trickling
// can't leak a one-frame physical press through.
static void MikuPan_RemapGamepadWindowingTrigger(void)
{
    ImGuiContext *ctx = ImGui::GetCurrentContext();
    if (ctx == nullptr)
        return;

    ImGuiIO &io = ImGui::GetIO();

    // OR-merge L1/R1 across every gamepad the ImGui backend has opened, matching
    // the backend's own multi-pad merge behaviour.
    bool l1 = false;
    bool r1 = false;
    int gamepad_count = 0;
    SDL_JoystickID *gamepad_ids = SDL_GetGamepads(&gamepad_count);
    if (gamepad_ids != nullptr)
    {
        for (int i = 0; i < gamepad_count; ++i)
        {
            SDL_Gamepad *gamepad = SDL_GetGamepadFromID(gamepad_ids[i]);
            if (gamepad == nullptr)
                continue; // not opened by the backend (e.g. AutoFirst mode)
            l1 |= SDL_GetGamepadButton(gamepad, SDL_GAMEPAD_BUTTON_LEFT_SHOULDER);
            r1 |= SDL_GetGamepadButton(gamepad, SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER);
        }
        SDL_free(gamepad_ids);
    }

    // --- Windowing-menu state machine -----------------------------------
    static bool s_prev_l1   = false;
    static bool s_prev_r1   = false;
    static bool s_menu_open = false;

    const bool menu_was_open = s_menu_open;
    const bool l1_edge = (l1 && !s_prev_l1); // fresh physical press this frame
    const bool r1_edge = (r1 && !s_prev_r1);

    if (l1 && r1)
        s_menu_open = true;          // chord opens (or keeps) the menu
    else if (!l1 && !r1)
        s_menu_open = false;         // releasing both confirms & closes

    const bool menu_open = s_menu_open;

    // Cycle only on a fresh tap while the menu was already open the previous
    // frame — this skips the chord-open frame (where both go down together) so
    // opening the menu never also cycles.
    const bool allow_cycle = menu_open && menu_was_open;
    const bool l1_pulse = (l1_edge && allow_cycle);
    const bool r1_pulse = (r1_edge && allow_cycle);

    s_prev_l1 = l1;
    s_prev_r1 = r1;

    const ImGuiKey override_keys[] = { ImGuiKey_GamepadFaceLeft,
                                       ImGuiKey_GamepadL1,
                                       ImGuiKey_GamepadR1 };
    const bool override_down[] = { menu_open, l1_pulse, r1_pulse };

    for (int slot = 0; slot < (int)IM_ARRAYSIZE(override_keys); ++slot)
    {
        const ImGuiKey key = override_keys[slot];
        const bool want_down = override_down[slot];

        // Rewrite any backend-queued event for this key so only one exists this
        // frame (avoids input-trickle leaking a one-frame physical press).
        bool found = false;
        for (ImGuiInputEvent &e : ctx->InputEventsQueue)
        {
            if (e.Type == ImGuiInputEventType_Key && e.Key.Key == key)
            {
                e.Key.Down = want_down;
                e.Key.AnalogValue = want_down ? 1.0f : 0.0f;
                found = true;
            }
        }

        // No event queued this frame (state unchanged): inject one only if the
        // current ImGui state disagrees with what we want.
        if (!found && ImGui::IsKeyDown(key) != want_down)
            io.AddKeyEvent(key, want_down);
    }
}

extern "C"
{

void MikuPan_ImGui_ImplInit(SDL_Window *window)
{
    ImGui_ImplSDL3_InitForSDLGPU(window);
    ImGui_ImplSDLGPU3_InitInfo init_info = {};
    init_info.Device = MikuPan_GPUGetDevice();
    init_info.ColorTargetFormat = MikuPan_GPUGetSwapchainFormat();
    init_info.MSAASamples = SDL_GPU_SAMPLECOUNT_1;
    init_info.SwapchainComposition = SDL_GPU_SWAPCHAINCOMPOSITION_SDR;
    init_info.PresentMode = SDL_GPU_PRESENTMODE_VSYNC;
    ImGui_ImplSDLGPU3_Init(&init_info);
}

void MikuPan_ImGui_ImplShutdown(void)
{
    ImGui_ImplSDLGPU3_Shutdown();
    ImGui_ImplSDL3_Shutdown();
}

void MikuPan_ImGui_ImplNewFrame(void)
{
    ImGui_ImplSDLGPU3_NewFrame();
    ImGui_ImplSDL3_NewFrame();
    MikuPan_RemapGamepadWindowingTrigger();
}

void MikuPan_ImGui_ImplProcessEvent(SDL_Event *event)
{
    ImGui_ImplSDL3_ProcessEvent(event);
}

void MikuPan_ImGui_ImplRenderDrawData(void)
{
    ImDrawData *draw_data = ImGui::GetDrawData();
    SDL_GPUCommandBuffer *cmd = MikuPan_GPUGetCommandBuffer();
    if (draw_data == nullptr || cmd == nullptr)
        return;

    MikuPan_GPUFlushRenderPass();
    ImGui_ImplSDLGPU3_PrepareDrawData(draw_data, cmd);
    MikuPan_GPUSetTarget(MIKUPAN_GPU_TARGET_WINDOW, 0);
    MikuPan_GPUBeginRenderPass();
    SDL_GPURenderPass *pass = MikuPan_GPUGetRenderPass();
    if (pass == nullptr)
        return;

    ImGui_ImplSDLGPU3_RenderDrawData(draw_data, cmd, pass);
}

}
