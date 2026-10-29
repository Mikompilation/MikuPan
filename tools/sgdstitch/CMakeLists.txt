cmake_minimum_required(VERSION 3.20)
project(sgdstitch LANGUAGES CXX)

# Standalone offline tool: no dependency on the game code or SDL. It can be
# built on its own (cmake -S tools/sgdstitch -B build-sgdstitch) or from the
# main project with -DMIKUPAN_BUILD_TOOLS=ON.

# The texture baker and the GS memory emulation are shared with the game,
# which runs the same bake on first launch.
set(MIKUPAN_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../src")

add_executable(sgdstitch
        src/main.cpp
        src/sgd_container.cpp
        src/sgd_layout.cpp
        src/sgd_stitch.cpp
        src/sgd_units.cpp
        src/sgd_verify.cpp
        ${MIKUPAN_SOURCE_DIR}/mikupan/bake/mikupan_bake.cpp
        ${MIKUPAN_SOURCE_DIR}/mikupan/bake/mikupan_bake_data.cpp
        ${MIKUPAN_SOURCE_DIR}/mikupan/bake/mikupan_tim2.cpp
        ${MIKUPAN_SOURCE_DIR}/mikupan/gs/mikupan_gs_memory.cpp
)

target_include_directories(sgdstitch PRIVATE ${MIKUPAN_SOURCE_DIR})

target_compile_features(sgdstitch PRIVATE cxx_std_17)

if(MSVC)
    target_compile_options(sgdstitch PRIVATE /W4)
else()
    target_compile_options(sgdstitch PRIVATE -Wall -Wextra)
endif()
