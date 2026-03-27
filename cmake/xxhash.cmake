include(FetchContent)

FetchContent_Declare(
        xxhash
        GIT_REPOSITORY https://github.com/Cyan4973/xxHash.git
        GIT_TAG 82cead715cbfddd9e6093db8df95155077ce6e64
        SOURCE_SUBDIR build/cmake
)

set(XXHASH_BUILD_ENABLE_INLINE_API OFF CACHE BOOL "" FORCE)
set(XXHASH_BUILD_XXHSUM OFF CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(xxhash)