include(FetchContent)

FetchContent_Declare(
        cimgui_src
        GIT_REPOSITORY https://github.com/cimgui/cimgui.git
        GIT_TAG        master
        GIT_SUBMODULES "imgui"
)

# FetchContent_Populate downloads without running cimgui's own CMakeLists.txt,
# so we control exactly what gets built. cimgui_src_SOURCE_DIR is set afterwards.
FetchContent_GetProperties(cimgui_src)
if (NOT cimgui_src_POPULATED)
    FetchContent_Populate(cimgui_src)
endif()

add_library(cimgui STATIC
        ${cimgui_src_SOURCE_DIR}/cimgui.cpp
        ${cimgui_src_SOURCE_DIR}/imgui/imgui.cpp
        ${cimgui_src_SOURCE_DIR}/imgui/imgui_demo.cpp
        ${cimgui_src_SOURCE_DIR}/imgui/imgui_draw.cpp
        ${cimgui_src_SOURCE_DIR}/imgui/imgui_tables.cpp
        ${cimgui_src_SOURCE_DIR}/imgui/imgui_widgets.cpp
        ${cimgui_src_SOURCE_DIR}/imgui/backends/imgui_impl_sdl3.cpp
        ${cimgui_src_SOURCE_DIR}/imgui/backends/imgui_impl_opengl3.cpp
)

target_include_directories(cimgui PUBLIC
        ${cimgui_src_SOURCE_DIR}
        ${cimgui_src_SOURCE_DIR}/imgui
        ${cimgui_src_SOURCE_DIR}/imgui/backends
)

target_link_libraries(cimgui PUBLIC SDL3::SDL3)
