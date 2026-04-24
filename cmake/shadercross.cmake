include(FetchContent)

FetchContent_Declare(
        sdl_shadercross
        GIT_REPOSITORY https://github.com/libsdl-org/SDL_shadercross.git
        GIT_TAG main
)

set(SDLSHADERCROSS_SHARED    OFF)
set(SDLSHADERCROSS_STATIC    ON)
set(SDLSHADERCROSS_CLI       ON)
set(SDLSHADERCROSS_INSTALL   OFF)
set(SDLSHADERCROSS_VENDORED  ON)
set(SDLSHADERCROSS_DXC       ON)

# DXC's bundled LLVM specializes std::is_nothrow_constructible, which
# Apple libc++ (Xcode 16+ / macOS 26+) now prohibits. Suppress the diagnostic.
if(APPLE)
    add_compile_options(-Wno-invalid-specialization)
endif()

FetchContent_MakeAvailable(sdl_shadercross)
