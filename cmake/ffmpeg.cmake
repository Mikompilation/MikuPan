if(WIN32)
    set(FFMPEG_ROOT "${CMAKE_SOURCE_DIR}/extern/ffmpeg")

    add_library(avcodec SHARED IMPORTED)
    set_target_properties(avcodec PROPERTIES
            IMPORTED_IMPLIB "${FFMPEG_ROOT}/lib/avcodec.lib"
            INTERFACE_INCLUDE_DIRECTORIES "${FFMPEG_ROOT}/include"
    )

    add_library(avformat SHARED IMPORTED)
    set_target_properties(avformat PROPERTIES
            IMPORTED_IMPLIB "${FFMPEG_ROOT}/lib/avformat.lib"
    )

    add_library(avutil SHARED IMPORTED)
    set_target_properties(avutil PROPERTIES
            IMPORTED_IMPLIB "${FFMPEG_ROOT}/lib/avutil.lib"
    )

    add_library(swscale SHARED IMPORTED)
    set_target_properties(swscale PROPERTIES
            IMPORTED_IMPLIB "${FFMPEG_ROOT}/lib/swscale.lib"
    )

    add_library(swresample SHARED IMPORTED)
    set_target_properties(swresample PROPERTIES
            IMPORTED_IMPLIB "${FFMPEG_ROOT}/lib/swresample.lib"
    )
else()
    include(FetchContent)
    include(ExternalProject)
    include(ProcessorCount)

    FetchContent_Declare(
            ffmpeg
            GIT_REPOSITORY https://github.com/FFmpeg/FFmpeg.git
            GIT_TAG n8.1
    )

    FetchContent_GetProperties(ffmpeg)
    if(NOT ffmpeg_POPULATED)
        FetchContent_Populate(ffmpeg)
    endif()

    find_program(BASH_EXECUTABLE bash REQUIRED)
    find_program(MAKE_EXECUTABLE make REQUIRED)

    ProcessorCount(FFMPEG_BUILD_JOBS)
    if(NOT FFMPEG_BUILD_JOBS OR FFMPEG_BUILD_JOBS LESS 1)
        set(FFMPEG_BUILD_JOBS 1)
    endif()

    set(FFMPEG_ROOT "${CMAKE_BINARY_DIR}/ffmpeg")
    set(FFMPEG_LIB_DIR "${FFMPEG_ROOT}/lib")
    file(MAKE_DIRECTORY "${FFMPEG_ROOT}/include")

    ExternalProject_Add(ffmpeg_build
            SOURCE_DIR "${ffmpeg_SOURCE_DIR}"
            CONFIGURE_COMMAND
                ${BASH_EXECUTABLE} ./configure
                --prefix=${FFMPEG_ROOT}
                --disable-shared
                --enable-static
                --disable-programs
                --disable-doc
                --disable-debug
                --disable-autodetect
                --disable-x86asm
                --enable-pic
                --enable-avcodec
                --enable-avformat
                --enable-avutil
                --enable-swscale
                --enable-swresample
            BUILD_COMMAND ${MAKE_EXECUTABLE} -j${FFMPEG_BUILD_JOBS}
            INSTALL_COMMAND ${MAKE_EXECUTABLE} install
            BUILD_IN_SOURCE 1
            BUILD_BYPRODUCTS
                "${FFMPEG_LIB_DIR}/libavcodec.a"
                "${FFMPEG_LIB_DIR}/libavformat.a"
                "${FFMPEG_LIB_DIR}/libavutil.a"
                "${FFMPEG_LIB_DIR}/libswscale.a"
                "${FFMPEG_LIB_DIR}/libswresample.a"
    )

    find_package(Threads REQUIRED)
    set(FFMPEG_SYSTEM_LIBS Threads::Threads m)
    if(CMAKE_DL_LIBS)
        list(APPEND FFMPEG_SYSTEM_LIBS ${CMAKE_DL_LIBS})
    endif()

    add_library(avutil STATIC IMPORTED)
    set_target_properties(avutil PROPERTIES
            IMPORTED_LOCATION "${FFMPEG_LIB_DIR}/libavutil.a"
            INTERFACE_INCLUDE_DIRECTORIES "${FFMPEG_ROOT}/include"
            INTERFACE_LINK_LIBRARIES "${FFMPEG_SYSTEM_LIBS}"
    )

    add_library(avcodec STATIC IMPORTED)
    set_target_properties(avcodec PROPERTIES
            IMPORTED_LOCATION "${FFMPEG_LIB_DIR}/libavcodec.a"
            INTERFACE_INCLUDE_DIRECTORIES "${FFMPEG_ROOT}/include"
            INTERFACE_LINK_LIBRARIES "avutil;${FFMPEG_SYSTEM_LIBS}"
    )

    add_library(avformat STATIC IMPORTED)
    set_target_properties(avformat PROPERTIES
            IMPORTED_LOCATION "${FFMPEG_LIB_DIR}/libavformat.a"
            INTERFACE_INCLUDE_DIRECTORIES "${FFMPEG_ROOT}/include"
            INTERFACE_LINK_LIBRARIES "avcodec;avutil;${FFMPEG_SYSTEM_LIBS}"
    )

    add_library(swscale STATIC IMPORTED)
    set_target_properties(swscale PROPERTIES
            IMPORTED_LOCATION "${FFMPEG_LIB_DIR}/libswscale.a"
            INTERFACE_INCLUDE_DIRECTORIES "${FFMPEG_ROOT}/include"
            INTERFACE_LINK_LIBRARIES "avutil;${FFMPEG_SYSTEM_LIBS}"
    )

    add_library(swresample STATIC IMPORTED)
    set_target_properties(swresample PROPERTIES
            IMPORTED_LOCATION "${FFMPEG_LIB_DIR}/libswresample.a"
            INTERFACE_INCLUDE_DIRECTORIES "${FFMPEG_ROOT}/include"
            INTERFACE_LINK_LIBRARIES "avutil;${FFMPEG_SYSTEM_LIBS}"
    )

    add_dependencies(avutil ffmpeg_build)
    add_dependencies(avcodec ffmpeg_build)
    add_dependencies(avformat ffmpeg_build)
    add_dependencies(swscale ffmpeg_build)
    add_dependencies(swresample ffmpeg_build)
endif()
