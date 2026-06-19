#include "typedefs.h"

#define CIMGUI_DEFINE_ENUMS_AND_STRUCTS
#include "cimgui.h"
#include "main/glob.h"
#include "mikupan/mikupan_controller.h"
#include "mikupan/mikupan_file_c.h"
#include "mikupan/mikupan_screenshot.h"
#include "mikupan/rendering/mikupan_gpu.h"
#include "mikupan/ui/mikupan_ui.h"
#include "mikupan_ui_cheats.h"
#include "mikupan_ui_debug.h"
#include "mikupan_ui_settings.h"
#include "mikupan_ui_theme.h"
#include "mikupan_version.h"
#include <stdio.h>

// -- Backend wrappers (defined in mikupan_ui.cpp) ----------------------------
void MikuPan_ImGui_ImplInit(SDL_Window* window);
void MikuPan_ImGui_ImplShutdown(void);
void MikuPan_ImGui_ImplNewFrame(void);
void MikuPan_ImGui_ImplProcessEvent(SDL_Event* event);
void MikuPan_ImGui_ImplRenderDrawData(void);

int show_menu_bar = 0;
static SDL_Window* ui_window = NULL;

const int no_navigation_window =
    ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoDecoration
    | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_AlwaysAutoResize
    | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoInputs;

SDL_Window* MikuPan_GetUiWindow(void)
{
    return ui_window;
}

void MikuPan_InitUi(SDL_Window* window)
{
    ui_window = window;
    igCreateContext(NULL);
    ImGuiIO* io = igGetIO_Nil();
    io->ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard | ImGuiConfigFlags_NavEnableGamepad;

    MikuPan_UiThemeInit(window);
    MikuPan_UiSettingsInit();

    MikuPan_ImGui_ImplInit(window);
}

void MikuPan_RenderUi(void)
{
    igRender();
    ImGuiIO* io = igGetIO_Nil();
    MikuPan_GPUSetViewport(0, 0, (int) io->DisplaySize.x,
                           (int) io->DisplaySize.y);
    MikuPan_ImGui_ImplRenderDrawData();
}

void MikuPan_StartFrameUi(void)
{
    MikuPan_ImGui_ImplNewFrame();
    igNewFrame();
}

void MikuPan_DrawUi(void)
{
    MikuPan_UiHandleShortcuts();
    MikuPan_UiMenuBar();

    MikuPan_UiDebugWindowsRender();
}

void MikuPan_DrawMissingDataUi(const char *missing_file)
{
    ImGuiIO* io = igGetIO_Nil();
    const float ui_scale = MikuPan_UiThemeGetDisplayScale();
    const float scale = ui_scale > 0.0f ? ui_scale : 1.0f;
    const float width = 520.0f * scale;
    const float height = 0.0f;
    const char *file = (missing_file != NULL && missing_file[0] != '\0')
                           ? missing_file
                           : "IMG_HD.BIN";

    igSetNextWindowPos(
        (ImVec2){io->DisplaySize.x * 0.5f, io->DisplaySize.y * 0.5f},
        ImGuiCond_Always,
        (ImVec2){0.5f, 0.5f});
    igSetNextWindowSize((ImVec2){width, height}, ImGuiCond_Always);
    if (!igBegin("Game Data Required", NULL,
                 ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize
                 | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings))
    {
        igEnd();
        return;
    }

    igTextWrapped("MikuPan needs the extracted Fatal Frame disc data before it can start.");
    igSpacing();
    igText("Missing: %s", file);
    igSpacing();
    igTextWrapped("Select the folder that contains IMG_HD.BIN and IMG_BD.BIN.");
    igSpacing();

    if (igButton("Select Folder", (ImVec2){160.0f * scale, 0.0f}))
    {
        MikuPan_RequestDataFolderSelection(file);
    }

    igEnd();
}

void MikuPan_ShutDownUi(void)
{
    MikuPan_ImGui_ImplShutdown();
    igDestroyContext(NULL);
}

void MikuPan_ProcessEventUi(SDL_Event* event)
{
    MikuPan_ImGui_ImplProcessEvent(event);
}

void MikuPan_RequestQuit(void)
{
    SDL_Event quit_event;
    SDL_zero(quit_event);
    quit_event.type = SDL_EVENT_QUIT;
    SDL_PushEvent(&quit_event);
}

static int MikuPan_UiGamepadMenuShortcutPressed(void)
{
    static int was_down = 0;
    int down = 0;

    SDL_Gamepad *gamepad = MikuPan_GetController();
    if (gamepad != NULL)
    {
        down = SDL_GetGamepadButton(gamepad, SDL_GAMEPAD_BUTTON_LEFT_STICK)
            && SDL_GetGamepadButton(gamepad, SDL_GAMEPAD_BUTTON_START);
    }
    else
    {
        int gamepad_count = 0;
        SDL_JoystickID *gamepad_ids = SDL_GetGamepads(&gamepad_count);
        if (gamepad_ids != NULL)
        {
            for (int i = 0; i < gamepad_count && !down; i++)
            {
                SDL_Gamepad *opened_gamepad =
                    SDL_GetGamepadFromID(gamepad_ids[i]);
                if (opened_gamepad != NULL)
                {
                    down = SDL_GetGamepadButton(
                               opened_gamepad,
                               SDL_GAMEPAD_BUTTON_LEFT_STICK)
                        && SDL_GetGamepadButton(
                               opened_gamepad,
                               SDL_GAMEPAD_BUTTON_START);
                }
            }
            SDL_free(gamepad_ids);
        }
    }

    const int pressed = down && !was_down;
    was_down = down;
    return pressed;
}

void MikuPan_UiHandleShortcuts(void)
{
    if (igIsKeyPressed_Bool(ImGuiKey_F1, 0)
        || MikuPan_UiGamepadMenuShortcutPressed())
    {
        show_menu_bar = !show_menu_bar;
    }

    if (igIsKeyPressed_Bool(ImGuiKey_F2, 0))
    {
        dbg_wrk.mode_on = !dbg_wrk.mode_on;
    }

    if (igIsKeyPressed_Bool(ImGuiKey_F12, 0))
    {
        MikuPan_ScreenshotRequest();
    }
}

void MikuPan_UiMenuBar(void)
{
    MikuPan_CheatSyncPhotoMode();

    if (!show_menu_bar || !igBeginMainMenuBar())
    {
        return;
    }

    MikuPan_UiSettingsRender();
    MikuPan_UiDebugMenuRender();
    MikuPan_UiCheatsRender();

    if (igBeginMenu("Info", 1))
    {
        igText("MikuPan");
        igSeparator();
        igText("Tag:      %s", MIKUPAN_GIT_TAG);
        igText("Commit:   %s", MIKUPAN_GIT_COMMIT);
        igText("Version:  %s", MIKUPAN_GIT_DESCRIBE);
        igText("Branch:   %s", MIKUPAN_GIT_BRANCH);
        igText("Built:    %s", MIKUPAN_BUILD_DATE);

        igSeparator();
        char version_line[256];
        snprintf(version_line, sizeof(version_line), "%s (%s)",
                 MIKUPAN_GIT_DESCRIBE, MIKUPAN_GIT_COMMIT);
        if (igButton("Copy version to clipboard", (ImVec2) {0.0f, 0.0f}))
        {
            igSetClipboardText(version_line);
        }

        igEndMenu();
    }

    igEndMainMenuBar();
}
