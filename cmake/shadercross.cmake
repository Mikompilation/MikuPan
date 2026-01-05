include(FetchContent)

set(SDLSHADERCROSS_DXC OFF CACHE BOOL "")
set(SDLSHADERCROSS_CLI OFF CACHE BOOL "")
set(SDLSHADERCROSS_VENDORED ON CACHE BOOL "")
set(SDLSHADERCROSS_STATIC_DEFAULT ON CACHE BOOL "")

FetchContent_Declare(
        shadercross
        GIT_REPOSITORY https://github.com/libsdl-org/SDL_shadercross.git
        GIT_TAG f1ca8cfefba8f32095861bbcf2a4f4d773f0fbb4
)

#set(SDL3_DIR ${FETCHCONTENT_BASE_DIR}/sdl3-build)

FetchContent_MakeAvailable(shadercross)

set(SHADERCROSS_INCLUDE_DIR ${FETCHCONTENT_BASE_DIR}/shadercross-src/include CACHE PATH "Path to ShaderCross include directory")