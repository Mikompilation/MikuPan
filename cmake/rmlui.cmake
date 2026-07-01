include(FetchContent)

FetchContent_Declare(
        freetype
        GIT_REPOSITORY https://github.com/freetype/freetype.git
        GIT_TAG        VER-2-13-3
)

set(FT_DISABLE_ZLIB ON CACHE BOOL "Disable zlib for FreeType." FORCE)
set(FT_DISABLE_BZIP2 ON CACHE BOOL "Disable bzip2 for FreeType." FORCE)
set(FT_DISABLE_PNG ON CACHE BOOL "Disable png for FreeType." FORCE)
set(FT_DISABLE_HARFBUZZ ON CACHE BOOL "Disable harfbuzz for FreeType." FORCE)
set(FT_DISABLE_BROTLI ON CACHE BOOL "Disable brotli for FreeType." FORCE)

FetchContent_MakeAvailable(freetype)

if(TARGET freetype AND NOT TARGET Freetype::Freetype)
    add_library(Freetype::Freetype ALIAS freetype)
endif()

set(RMLUI_FONT_ENGINE "freetype" CACHE STRING "Font engine used by RmlUi." FORCE)
set(RMLUI_SAMPLES OFF CACHE BOOL "Disable RmlUi samples." FORCE)
set(RMLUI_LUA_BINDINGS OFF CACHE BOOL "Disable RmlUi Lua bindings." FORCE)
set(RMLUI_LOTTIE_PLUGIN OFF CACHE BOOL "Disable RmlUi Lottie plugin." FORCE)
set(RMLUI_SVG_PLUGIN OFF CACHE BOOL "Disable RmlUi SVG plugin." FORCE)
set(RMLUI_HARFBUZZ_SAMPLE OFF CACHE BOOL "Disable RmlUi HarfBuzz sample." FORCE)
set(RMLUI_PRECOMPILED_HEADERS OFF CACHE BOOL "Disable RmlUi precompiled headers." FORCE)

FetchContent_Declare(
        rmlui
        GIT_REPOSITORY https://github.com/mikke89/RmlUi.git
        GIT_TAG        6.2
)

FetchContent_MakeAvailable(rmlui)
