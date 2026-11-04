#pragma once
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

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
};

const Engine& GetUsEngine();
const Engine& GetEuEngine();
const char* DataRoot();
}
