include(FetchContent)

set(ASSIMP_BUILD_TESTS OFF CACHE BOOL "Disable assimp tests" FORCE)
set(ASSIMP_BUILD_ASSIMP_TOOLS OFF CACHE BOOL "Disable assimp command line tools" FORCE)
set(ASSIMP_INSTALL OFF CACHE BOOL "Disable assimp install targets" FORCE)
set(ASSIMP_WARNINGS_AS_ERRORS OFF CACHE BOOL "Do not treat assimp warnings as errors" FORCE)

set(ASSIMP_BUILD_ALL_IMPORTERS_BY_DEFAULT OFF CACHE BOOL "Disable all assimp importers by default" FORCE)
set(ASSIMP_BUILD_ALL_EXPORTERS_BY_DEFAULT OFF CACHE BOOL "Disable all assimp exporters by default" FORCE)
set(ASSIMP_BUILD_GLTF_IMPORTER ON CACHE BOOL "Enable assimp glTF importer" FORCE)
set(ASSIMP_BUILD_OBJ_IMPORTER ON CACHE BOOL "Enable assimp OBJ importer" FORCE)

FetchContent_Declare(
        assimp
        GIT_REPOSITORY https://github.com/assimp/assimp.git
        GIT_TAG v6.0.5
)

FetchContent_MakeAvailable(assimp)
