include(FetchContent)

FetchContent_Declare(
        implot_src
        GIT_REPOSITORY https://github.com/epezent/implot.git
        GIT_TAG v1.0
        # Keep ImPlot from adding itself as a subproject. We compile the source
        # files directly so it uses the same Dear ImGui instance as cimgui.
        SOURCE_SUBDIR cmake-do-not-configure
)

FetchContent_MakeAvailable(implot_src)

add_library(implot STATIC
        ${implot_src_SOURCE_DIR}/implot.cpp
        ${implot_src_SOURCE_DIR}/implot_items.cpp
)

target_include_directories(implot PUBLIC
        ${implot_src_SOURCE_DIR}
)

target_link_libraries(implot PUBLIC cimgui)
