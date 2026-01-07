if(UNIX)
        find_package(SDL3)
        if(${SDL3_FOUND})
                message(STATUS "Found SDL3")
        endif()
else()
        include(FetchContent)

        FetchContent_Declare(
                sdl3
                GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
                GIT_TAG release-3.4.0
        )

        FetchContent_MakeAvailable(sdl3)
endif()