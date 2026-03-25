#find_package(PkgConfig REQUIRED)
#pkg_check_modules(AVFORMAT REQUIRED libavformat)
#pkg_check_modules(AVCODEC REQUIRED libavcodec)
#pkg_check_modules(SWSCALE REQUIRED libswscale)
#pkg_check_modules(AVUTIL REQUIRED libavutil)

include(FetchContent)

FetchContent_Declare(
        ffmpeg
        GIT_REPOSITORY https://github.com/FFmpeg/FFmpeg.git
        GIT_TAG n8.1
)

FetchContent_MakeAvailable(ffmpeg)

include(ExternalProject)

ExternalProject_Add(ffmpeg_build
        SOURCE_DIR ${ffmpeg_SOURCE_DIR}
        CONFIGURE_COMMAND
        powershell ${ffmpeg_SOURCE_DIR}/configure
        --prefix=${CMAKE_BINARY_DIR}/ffmpeg
        --disable-shared
        --enable-static
        --disable-programs
        --disable-doc
        --disable-everything
        --enable-avcodec
        --enable-avformat
        --enable-avutil
        --enable-swscale

        BUILD_COMMAND make -j
        INSTALL_COMMAND make install
        BUILD_IN_SOURCE 1
        BUILD_BYPRODUCTS
        ${CMAKE_BINARY_DIR}/ffmpeg/lib/libavcodec.a
        ${CMAKE_BINARY_DIR}/ffmpeg/lib/libavformat.a
        ${CMAKE_BINARY_DIR}/ffmpeg/lib/libavutil.a
        ${CMAKE_BINARY_DIR}/ffmpeg/lib/swscale.a
)

add_library(avcodec STATIC IMPORTED)
set_target_properties(avcodec PROPERTIES
        IMPORTED_LOCATION ${CMAKE_BINARY_DIR}/ffmpeg/lib/libavcodec.a
)

add_library(avformat STATIC IMPORTED)
set_target_properties(avformat PROPERTIES
        IMPORTED_LOCATION ${CMAKE_BINARY_DIR}/ffmpeg/lib/libavformat.a
)

add_library(avutil STATIC IMPORTED)
set_target_properties(avutil PROPERTIES
        IMPORTED_LOCATION ${CMAKE_BINARY_DIR}/ffmpeg/lib/libavutil.a
)

add_library(swscale STATIC IMPORTED)
set_target_properties(swscale PROPERTIES
        IMPORTED_LOCATION ${CMAKE_BINARY_DIR}/ffmpeg/lib/swscale.a
)

add_dependencies(avcodec ffmpeg_build)
add_dependencies(avformat ffmpeg_build)
add_dependencies(avutil ffmpeg_build)
add_dependencies(swscale ffmpeg_build)