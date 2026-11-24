#pragma once
#include <SDL3/SDL.h>

/// Both releases of the game are compiled into the executable, each as an
/// engine in a namespace of its own (mikupan_region.h). The host (main.cpp)
/// runs the one that plays the game data the player has.
namespace MikuPanHost
{
enum class Region { US, EU };

struct Engine
{
    Region region;
    int (*file_count)();
    int message_file;
    SDL_AppResult (*init)(void**, int, char**);
    SDL_AppResult (*event)(void*, SDL_Event*);
    SDL_AppResult (*iterate)(void*);
    void (*quit)(void*, SDL_AppResult);
    const char* (*base_directory)();
    /// Loads the configuration and finds the game data it stands for, before
    /// the engine has started. False when there is none that can be played.
    bool (*find_game_data)(Region*);
};

const Engine& GetUsEngine();
const Engine& GetEuEngine();

/// For the running engine, before its game has started: the game data that
/// was found is the other release's. The host ends this engine after the
/// frame and starts `region`'s in the same window.
void RequestRegion(Region region);

/// The window outlives the engine that made it when another one follows.
/// KeepWindow() is false when none does, and the window is the engine's to
/// destroy. TakeWindow() is NULL when the engine has to make its own.
bool KeepWindow(SDL_Window* window);
SDL_Window* TakeWindow();
}
