#define SDL_MAIN_USE_CALLBACKS 1
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include "engine_api.h"
#include <cstdio>
#include <string>
#include <utility>
#include "mikupan_version.h"

using MikuPanHost::Engine;
using MikuPanHost::Region;

/// The running engine.
static const Engine* engine;
static void* engine_state;
static int game_argc;
static char** game_argv;

static bool region_requested;
static Region requested_region;
/// Set while an engine is ended to start the other one.
static bool switching;
static SDL_Window* kept_window;

void MikuPanHost::RequestRegion(Region region)
{
    region_requested = true;
    requested_region = region;
}

bool MikuPanHost::KeepWindow(SDL_Window* window)
{
    if (!switching)
    {
        return false;
    }

    kept_window = window;
    return true;
}

SDL_Window* MikuPanHost::TakeWindow()
{
    return std::exchange(kept_window, nullptr);
}

/// Without its fonts RmlUi has no screen to ask for the game data on.
static bool HasUiResources()
{
    const std::string fonts =
        std::string(MikuPanHost::GetUsEngine().base_directory())
        + "resources/fonts/";

    for (const char* name :
         {"CenturyOldStyle.ttf", "URW Chancery L Medium Italic U.ttf"})
    {
        SDL_IOStream* font = SDL_IOFromFile((fonts + name).c_str(), "rb");
        if (font == nullptr)
        {
            return false;
        }
        SDL_CloseIO(font);
    }

    return true;
}

static SDL_AppResult StartEngine(Region region)
{
    engine = region == Region::EU ? &MikuPanHost::GetEuEngine()
                                  : &MikuPanHost::GetUsEngine();

    const SDL_AppResult result =
        engine->init(&engine_state, game_argc, game_argv);

    if (result == SDL_APP_FAILURE)
    {
        const std::string error =
            std::string("The selected engine could not initialize.\n\n")
            + SDL_GetError();
        std::fprintf(stderr, "%s\n", error.c_str());
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "MikuPan startup",
                                 error.c_str(), nullptr);
    }

    return result;
}

/// Starts the engine the running one asked for, if it did.
static SDL_AppResult ServiceRegionRequest()
{
    if (!region_requested)
    {
        return SDL_APP_CONTINUE;
    }

    region_requested = false;
    if (requested_region == engine->region)
    {
        return SDL_APP_CONTINUE;
    }

    switching = true;
    engine->quit(engine_state, SDL_APP_SUCCESS);
    switching = false;
    engine_state = nullptr;
    return StartEngine(requested_region);
}

SDL_AppResult SDL_AppInit(void**, int argc, char** argv)
{
    game_argc = argc;
    game_argv = argv;
    SDL_SetAppMetadata("MikuPan", MIKUPAN_GIT_DESCRIBE, "MikuPan");

    if (!SDL_Init(SDL_INIT_VIDEO))
    {
        std::fprintf(stderr, "SDL startup failed: %s\n", SDL_GetError());
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "MikuPan startup",
                                 SDL_GetError(), nullptr);
        return SDL_APP_FAILURE;
    }

    if (!HasUiResources())
    {
        SDL_ShowSimpleMessageBox(
            SDL_MESSAGEBOX_ERROR, "MikuPan startup",
            "Required UI resources are missing. Place the complete resources "
            "folder beside the executable.", nullptr);
        return SDL_APP_FAILURE;
    }

    /// Without game data, either engine can ask for it.
    Region region = Region::US;
    if (!MikuPanHost::GetUsEngine().find_game_data(&region))
    {
        region = Region::US;
    }

    return StartEngine(region);
}

SDL_AppResult SDL_AppEvent(void*, SDL_Event* event)
{
    return engine != nullptr ? engine->event(engine_state, event)
                             : SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppIterate(void*)
{
    const SDL_AppResult result = engine->iterate(engine_state);
    return result == SDL_APP_CONTINUE ? ServiceRegionRequest() : result;
}

void SDL_AppQuit(void*, SDL_AppResult result)
{
    if (engine != nullptr)
    {
        engine->quit(engine_state, result);
    }
    SDL_Quit();
}
