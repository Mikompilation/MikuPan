# ---------------------------------------------------------------------------
# Provide the SDL_shadercross CLI and compile the HLSL shaders to SPIR-V as
# part of the build.
#
# The SDL_GPU renderer loads pre-compiled SPIR-V from resources/shaders/spirv/
# at runtime. This module runs `shadercross` over resources/shaders/hlsl/*.hlsl
# into that directory (incrementally — only when a .hlsl or the shared .hlsli
# changes). The existing POST_BUILD `copy_directory resources` then deploys the
# fresh .spv next to the executable.
#
# How the CLI is obtained:
#   - Windows: a prebuilt CLI (plus its runtime DLLs) is committed under
#     extern/shadercross/windows/. It is NOT built from source there because
#     its DirectXShaderCompiler dependency does not compile under MinGW/GCC.
#   - Linux/macOS: no prebuilt downloads are available, so SDL_shadercross is
#     compiled from source (vendored DirectXShaderCompiler + SPIRV-Cross) as
#     an isolated ExternalProject. Everything — sources, build tree, installed
#     tool — lives under ${CMAKE_BINARY_DIR}/tools/shadercross/ so none of its
#     artifacts pollute the MikuPan executable's output directory
#     (${CMAKE_RUNTIME_OUTPUT_DIRECTORY}). Requires git, python3 and a C++17
#     compiler; the first build is slow (DXC is LLVM-based) but is cached and
#     never rebuilt afterwards.
#
# A prebuilt binary dropped into extern/shadercross/<platform>/ or found on
# PATH skips the source build. Override with -DSHADERCROSS_EXECUTABLE=<path>
# to use a specific binary.
# ---------------------------------------------------------------------------

if(WIN32)
    set(_shadercross_platform windows)
elseif(APPLE)
    set(_shadercross_platform macos)
else()
    set(_shadercross_platform linux)
endif()
set(MIKUPAN_SHADERCROSS_DIR ${CMAKE_SOURCE_DIR}/extern/shadercross/${_shadercross_platform})

# IMPORTANT: do NOT pre-seed SHADERCROSS_EXECUTABLE with an empty cache value.
# find_program() only re-searches when the result holds a *-NOTFOUND value; an
# empty string counts as "already found", so it would skip searching the HINTS
# entirely and always warn that shadercross is missing. Clear any stale empty
# value left by an earlier configure, then let find_program populate it. A real
# -DSHADERCROSS_EXECUTABLE=<path> override is non-empty and is kept as-is.
if(DEFINED SHADERCROSS_EXECUTABLE AND SHADERCROSS_EXECUTABLE STREQUAL "")
    unset(SHADERCROSS_EXECUTABLE CACHE)
endif()

# Deliberately do NOT search the from-source install location
# (${CMAKE_BINARY_DIR}/tools/shadercross/bin): once installed, find_program
# would cache that path on the next configure and skip defining the external
# project that owns and updates it.
if(NOT SHADERCROSS_EXECUTABLE)
    find_program(SHADERCROSS_EXECUTABLE
            NAMES shadercross
            HINTS ${MIKUPAN_SHADERCROSS_DIR}
            PATHS ENV PATH
            DOC "Path to the SDL_shadercross CLI used to compile HLSL shaders to SPIR-V."
    )
endif()

# Extra target-level dependency for the .spv custom commands; set to the
# ExternalProject name when shadercross is built from source.
set(MIKUPAN_SHADERCROSS_DEP "")

if(NOT SHADERCROSS_EXECUTABLE)
    if(WIN32)
        message(WARNING
                "shadercross not found: HLSL shaders will NOT be recompiled automatically.\n"
                "  Expected a prebuilt CLI under ${MIKUPAN_SHADERCROSS_DIR}\n"
                "  or pass -DSHADERCROSS_EXECUTABLE=<path>. The build will use the\n"
                "  existing resources/shaders/spirv/*.spv as-is.")
        # Empty target so add_dependencies(MikuPan compile_shaders) stays valid.
        add_custom_target(compile_shaders)
        return()
    endif()

    include(ExternalProject)

    set(MIKUPAN_SHADERCROSS_PREFIX ${CMAKE_BINARY_DIR}/tools/shadercross)

    # The CLI links libdxcompiler/libdxil as shared libraries installed under
    # <prefix>/lib; give every installed binary an RPATH relative to itself so
    # the tool runs from the install tree without environment setup.
    # (| is converted back to ; by LIST_SEPARATOR.)
    if(APPLE)
        set(_shadercross_install_rpath "@loader_path/../lib|@loader_path")
    else()
        set(_shadercross_install_rpath "\$ORIGIN/../lib|\$ORIGIN")
    endif()

    # Same SDL_shadercross commit the prebuilt Windows CLI was produced from.
    # SDL3 is NOT vendored by SDL_shadercross: point its find_package(SDL3) at
    # the build tree of the SDL3 we already fetch (cmake/sdl3.cmake), and order
    # after it via DEPENDS. SDLSHADERCROSS_VENDORED builds DirectXShaderCompiler
    # and SPIRV-Cross from submodules instead of downloading prebuilts.
    # CMAKE_POLICY_VERSION_MINIMUM: the vendored LLVM subprojects declare
    # cmake_minimum_required versions that CMake 4 otherwise refuses to run.
    ExternalProject_Add(shadercross_tool
            GIT_REPOSITORY https://github.com/libsdl-org/SDL_shadercross.git
            GIT_TAG 6b06e55c7c5d7e7a09a8a14f76e866dcfad5ab99
            GIT_PROGRESS TRUE
            UPDATE_DISCONNECTED TRUE
            PREFIX ${MIKUPAN_SHADERCROSS_PREFIX}
            LIST_SEPARATOR |
            CMAKE_ARGS
                -DCMAKE_BUILD_TYPE=Release
                -DCMAKE_INSTALL_PREFIX=<INSTALL_DIR>
                -DCMAKE_C_COMPILER=${CMAKE_C_COMPILER}
                -DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}
                -DCMAKE_MAKE_PROGRAM=${CMAKE_MAKE_PROGRAM}
                -DCMAKE_INSTALL_RPATH=${_shadercross_install_rpath}
                -DCMAKE_POLICY_VERSION_MINIMUM=3.5
                -DSDL3_DIR=${sdl3_BINARY_DIR}
                -DSDLSHADERCROSS_VENDORED=ON
                -DSDLSHADERCROSS_DXC=ON
                -DSDLSHADERCROSS_CLI=ON
                -DSDLSHADERCROSS_CLI_STATIC=ON
                -DSDLSHADERCROSS_SHARED=OFF
                -DSDLSHADERCROSS_STATIC=ON
                -DSDLSHADERCROSS_SPIRVCROSS_SHARED=OFF
                -DSDLSHADERCROSS_INSTALL=ON
            DEPENDS SDL3-static
            EXCLUDE_FROM_ALL TRUE
    )

    set(SHADERCROSS_EXECUTABLE ${MIKUPAN_SHADERCROSS_PREFIX}/bin/shadercross)
    set(MIKUPAN_SHADERCROSS_DEP shadercross_tool)
    message(STATUS "shadercross: not found, building from source -> ${SHADERCROSS_EXECUTABLE}")
else()
    message(STATUS "shadercross: ${SHADERCROSS_EXECUTABLE}")
endif()

set(MIKUPAN_HLSL_DIR  ${CMAKE_SOURCE_DIR}/resources/shaders/hlsl)
set(MIKUPAN_SPIRV_DIR ${CMAKE_SOURCE_DIR}/resources/shaders/spirv)

file(MAKE_DIRECTORY ${MIKUPAN_SPIRV_DIR})

# CONFIGURE_DEPENDS re-globs on build so newly added shaders are picked up
# without a manual re-configure.
file(GLOB MIKUPAN_HLSL_SHADERS  CONFIGURE_DEPENDS ${MIKUPAN_HLSL_DIR}/*.hlsl)
file(GLOB MIKUPAN_HLSL_INCLUDES CONFIGURE_DEPENDS ${MIKUPAN_HLSL_DIR}/*.hlsli)

# Where the running executable loads shaders from (next to MikuPan.exe). The
# POST_BUILD `copy_directory resources` only refreshes this on a relink, so a
# shader-only change wouldn't reach the app — deploy each rebuilt .spv here too.
set(MIKUPAN_RUNTIME_SPIRV_DIR ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/resources/shaders/spirv)

set(MIKUPAN_SPIRV_OUTPUTS "")
foreach(hlsl ${MIKUPAN_HLSL_SHADERS})
    get_filename_component(hlsl_name ${hlsl} NAME)                # heat_haze.frag.hlsl
    string(REGEX REPLACE "\\.hlsl$" ".spv" spv_name ${hlsl_name})  # heat_haze.frag.spv
    set(spv_src ${MIKUPAN_SPIRV_DIR}/${spv_name})
    set(spv_run ${MIKUPAN_RUNTIME_SPIRV_DIR}/${spv_name})

    # 1. Compile HLSL -> SPIR-V into the source tree (recompiles whenever this
    #    shader OR the shared mikupan_common.hlsli changes). shadercross infers
    #    source (HLSL), destination (SPIRV) and stage from the file names. Do NOT
    #    pass --cull: the fragment shaders rely on both samplers (uTexture +
    #    uAuxTexture) staying declared so the runtime's num_samplers=2 matches.
    #    MIKUPAN_SHADERCROSS_DEP is a target-level dependency only: it makes the
    #    from-source tool build first but doesn't retrigger shader compiles.
    add_custom_command(
            OUTPUT ${spv_src}
            COMMAND ${SHADERCROSS_EXECUTABLE} ${hlsl} -o ${spv_src} -I ${MIKUPAN_HLSL_DIR}
            DEPENDS ${hlsl} ${MIKUPAN_HLSL_INCLUDES} ${MIKUPAN_SHADERCROSS_DEP}
            COMMENT "shadercross ${hlsl_name} -> ${spv_name}"
            VERBATIM
    )

    # 2. Deploy the rebuilt .spv next to the executable so the change takes
    #    effect on the next build without needing MikuPan to relink.
    add_custom_command(
            OUTPUT ${spv_run}
            COMMAND ${CMAKE_COMMAND} -E make_directory ${MIKUPAN_RUNTIME_SPIRV_DIR}
            COMMAND ${CMAKE_COMMAND} -E copy ${spv_src} ${spv_run}
            DEPENDS ${spv_src}
            VERBATIM
    )

    list(APPEND MIKUPAN_SPIRV_OUTPUTS ${spv_run})
endforeach()

# Built on every MikuPan build (add_dependencies in CMakeLists.txt), so any
# changed shader is recompiled and redeployed each time.
add_custom_target(compile_shaders DEPENDS ${MIKUPAN_SPIRV_OUTPUTS})
