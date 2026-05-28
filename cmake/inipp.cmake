include(FetchContent)

FetchContent_Declare(
        inipp
        GIT_REPOSITORY https://github.com/mcmtroffaes/inipp.git
        GIT_TAG 1.0.13
)

FetchContent_MakeAvailable(inipp)
