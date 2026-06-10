include(FetchContent)

FetchContent_Declare(
        cimgui_src
        GIT_REPOSITORY https://github.com/cimgui/cimgui.git
        GIT_TAG        master
        GIT_SUBMODULES "imgui"
        # Point SOURCE_SUBDIR at a path with no CMakeLists.txt so
        # FetchContent_MakeAvailable downloads the sources WITHOUT running
        # cimgui's own CMakeLists.txt — we compile exactly the files we want into
        # our own `cimgui` target below. This is the CMP0169-compliant
        # replacement for the now-deprecated bare FetchContent_Populate().
        SOURCE_SUBDIR  cmake-do-not-configure
)

# Populates and sets cimgui_src_SOURCE_DIR; the SOURCE_SUBDIR above keeps it from
# adding cimgui as a subproject.
FetchContent_MakeAvailable(cimgui_src)

add_library(cimgui STATIC
        ${cimgui_src_SOURCE_DIR}/cimgui.cpp
        ${cimgui_src_SOURCE_DIR}/imgui/imgui.cpp
        ${cimgui_src_SOURCE_DIR}/imgui/imgui_demo.cpp
        ${cimgui_src_SOURCE_DIR}/imgui/imgui_draw.cpp
        ${cimgui_src_SOURCE_DIR}/imgui/imgui_tables.cpp
        ${cimgui_src_SOURCE_DIR}/imgui/imgui_widgets.cpp
        ${cimgui_src_SOURCE_DIR}/imgui/backends/imgui_impl_sdl3.cpp
        ${cimgui_src_SOURCE_DIR}/imgui/backends/imgui_impl_sdlgpu3.cpp
)

target_include_directories(cimgui PUBLIC
        ${cimgui_src_SOURCE_DIR}
        ${cimgui_src_SOURCE_DIR}/imgui
        ${cimgui_src_SOURCE_DIR}/imgui/backends
)

target_link_libraries(cimgui PUBLIC SDL3::SDL3)
