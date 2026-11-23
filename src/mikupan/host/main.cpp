#define SDL_MAIN_USE_CALLBACKS 1
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include "asset_check.h"
#include <cstdio>
#include <cstring>
#include "mikupan_version.h"
#include <mutex>

static Region selected_region;
static std::string data_root;
static std::string startup_error;
static bool engine_entered;
static bool picker_pending;
static bool cancelled;
static bool host_failure;
static void* engine_state;
static const MikuPanHost::Engine* engine;
static int game_argc;
static char** game_argv;
static std::mutex picker_mutex;
static bool picker_done;
static std::string picker_path;
static std::string picker_error;

const char* MikuPanHost::DataRoot() { return data_root.empty() ? nullptr : data_root.c_str(); }
static bool IsEU() { return selected_region == Region::EU; }

static bool ValidateSelection()
{
    return InspectAssets(data_root, selected_region, startup_error);
}

static void SDLCALL FolderSelected(void*, const char* const* files, int)
{
    std::lock_guard<std::mutex> lock(picker_mutex);
    if (!files) picker_error = SDL_GetError();
    else if (files[0]) picker_path = files[0];
    picker_done = true;
}

static void ShowSelectionError()
{
    const SDL_MessageBoxButtonData buttons[] = {
        {SDL_MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT, 1, "Choose data folder"},
        {0, 2, "Retry"},
        {SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT, 0, "Quit"}
    };
    const std::string message = startup_error + "\n\nFolder: " + data_root;
    const SDL_MessageBoxData box = {SDL_MESSAGEBOX_ERROR, nullptr, "MikuPan startup", message.c_str(), 3, buttons, nullptr};
    int button = 0;
    if (!SDL_ShowMessageBox(&box, &button))
    {
        std::fprintf(stderr, "%s\nCannot display startup dialog: %s\n", message.c_str(), SDL_GetError());
        host_failure = true;
        cancelled = true;
        return;
    }
    if (button == 0)
    {
        cancelled = true;
        return;
    }
    if (button == 1)
    {
        {
            std::lock_guard<std::mutex> lock(picker_mutex);
            picker_done = false;
            picker_path.clear();
            picker_error.clear();
        }
        picker_pending = true;
        SDL_ShowOpenFolderDialog(FolderSelected, nullptr, nullptr, data_root.c_str(), false);
    }
}

static bool HasUiResources()
{
    const auto resources = std::filesystem::path(MikuPanHost::GetUsEngine().base_directory()) / "resources/fonts";
    for (const char* name : {"CenturyOldStyle.ttf", "URW Chancery L Medium Italic U.ttf"})
    {
        std::error_code error;
        if (!std::filesystem::is_regular_file(resources / name, error)) return false;
    }
    return true;
}

static SDL_AppResult StartEngine()
{
    if (!HasUiResources())
    {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "MikuPan startup",
            "Required UI resources are missing. Place the complete resources folder beside the executable.", nullptr);
        return SDL_APP_FAILURE;
    }
    engine_entered = true;
    engine = IsEU() ? &MikuPanHost::GetEuEngine() : &MikuPanHost::GetUsEngine();
    const auto result = engine->init(&engine_state, game_argc, game_argv);
    if (result == SDL_APP_FAILURE)
    {
        const std::string detail = SDL_GetError();
        const std::string error = "The selected engine could not initialize.\n\n" + detail;
        std::fprintf(stderr, "%s\n", error.c_str());
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "MikuPan startup", error.c_str(), nullptr);
    }
    return result;
}

SDL_AppResult SDL_AppInit(void**, int argc, char** argv)
{
    game_argc = argc;
    game_argv = argv;
    SDL_SetAppMetadata("MikuPan", MIKUPAN_GIT_DESCRIBE, "MikuPan");
    if (!SDL_Init(SDL_INIT_VIDEO))
    {
        std::fprintf(stderr, "SDL startup failed: %s\n", SDL_GetError());
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "MikuPan startup", SDL_GetError(), nullptr);
        return SDL_APP_FAILURE;
    }
    const char* base = MikuPanHost::GetUsEngine().base_directory();
    if (base) data_root = base;
    if (ValidateSelection()) return StartEngine();
    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppEvent(void*, SDL_Event* event)
{
    if (engine_entered)
        return engine->event(engine_state, event);
    return event->type == SDL_EVENT_QUIT ? SDL_APP_SUCCESS : SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppIterate(void*)
{
    if (engine_entered)
        return engine->iterate(engine_state);
    if (cancelled) return host_failure ? SDL_APP_FAILURE : SDL_APP_SUCCESS;
    bool picker_failed = false;
    if (picker_pending)
    {
        std::lock_guard<std::mutex> lock(picker_mutex);
        if (picker_done)
        {
            picker_pending = false;
            if (!picker_error.empty()) { startup_error = "The folder picker failed: " + picker_error; picker_failed = true; }
            else if (!picker_path.empty()) data_root = picker_path;
        }
    }
    if (!picker_pending)
    {
        if (!picker_failed && ValidateSelection()) return StartEngine();
        ShowSelectionError();
    }
    SDL_Delay(10);
    return cancelled ? (host_failure ? SDL_APP_FAILURE : SDL_APP_SUCCESS) : SDL_APP_CONTINUE;
}

void SDL_AppQuit(void*, SDL_AppResult result)
{
    if (engine_entered)
    {
        engine->quit(engine_state, result);
    }
    SDL_Quit();
}
