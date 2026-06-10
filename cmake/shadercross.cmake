# ---------------------------------------------------------------------------
# Locate a prebuilt SDL_shadercross CLI and compile the HLSL shaders to SPIR-V
# as part of the build.
#
# The SDL_GPU renderer loads pre-compiled SPIR-V from resources/shaders/spirv/
# at runtime. This module runs `shadercross` over resources/shaders/hlsl/*.hlsl
# into that directory (incrementally — only when a .hlsl or the shared .hlsli
# changes). The existing POST_BUILD `copy_directory resources` then deploys the
# fresh .spv next to the executable.
#
# shadercross itself is NOT built from source: its DirectXShaderCompiler
# dependency does not compile under MinGW/GCC. Instead a prebuilt CLI (plus its
# runtime DLLs/libs) is committed to the repo under:
#     extern/shadercross/windows/   (shadercross.exe + dxcompiler.dll + ...)
#     extern/shadercross/linux/
#     extern/shadercross/macos/
# Override with -DSHADERCROSS_EXECUTABLE=<path> to use a different binary.
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

if(NOT SHADERCROSS_EXECUTABLE)
    find_program(SHADERCROSS_EXECUTABLE
            NAMES shadercross
            HINTS
                ${MIKUPAN_SHADERCROSS_DIR}
                ${CMAKE_BINARY_DIR}/tools/shadercross/bin
            PATHS ENV PATH
            DOC "Path to the SDL_shadercross CLI used to compile HLSL shaders to SPIR-V."
    )
endif()

set(MIKUPAN_HLSL_DIR  ${CMAKE_SOURCE_DIR}/resources/shaders/hlsl)
set(MIKUPAN_SPIRV_DIR ${CMAKE_SOURCE_DIR}/resources/shaders/spirv)

if(NOT SHADERCROSS_EXECUTABLE)
    message(WARNING
            "shadercross not found: HLSL shaders will NOT be recompiled automatically.\n"
            "  Expected a prebuilt CLI under ${MIKUPAN_SHADERCROSS_DIR}\n"
            "  or pass -DSHADERCROSS_EXECUTABLE=<path>. The build will use the\n"
            "  existing ${MIKUPAN_SPIRV_DIR}/*.spv as-is.")
    # Empty target so add_dependencies(MikuPan compile_shaders) stays valid.
    add_custom_target(compile_shaders)
    return()
endif()

message(STATUS "shadercross: ${SHADERCROSS_EXECUTABLE}")

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
    add_custom_command(
            OUTPUT ${spv_src}
            COMMAND ${SHADERCROSS_EXECUTABLE} ${hlsl} -o ${spv_src} -I ${MIKUPAN_HLSL_DIR}
            DEPENDS ${hlsl} ${MIKUPAN_HLSL_INCLUDES}
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
