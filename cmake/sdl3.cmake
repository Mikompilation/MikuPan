include(FetchContent)

FetchContent_Declare(
        sdl3
        GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
        GIT_TAG release-3.4.0
)
set(SDL_STATIC ON)
set(SDL_SHARED OFF)
FetchContent_MakeAvailable(sdl3)

FetchContent_Declare(
        sdl_mixer
        GIT_REPOSITORY https://github.com/libsdl-org/SDL_mixer.git
        GIT_TAG release-3.2.0
)

FetchContent_MakeAvailable(sdl_mixer)