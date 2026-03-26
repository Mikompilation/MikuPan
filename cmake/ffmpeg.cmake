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


#include(FetchContent)
#
#FetchContent_Declare(
#        ffmpeg
#        GIT_REPOSITORY https://github.com/FFmpeg/FFmpeg.git
#        GIT_TAG n8.1
#)
#
#FetchContent_MakeAvailable(ffmpeg)
#include(ExternalProject)
#
#ExternalProject_Add(ffmpeg_build
#        SOURCE_DIR ${ffmpeg_SOURCE_DIR}
#        CONFIGURE_COMMAND
#        bash ./configure
#        --prefix=${CMAKE_BINARY_DIR}/ffmpeg
#        --disable-shared
#        --enable-static
#        --disable-programs
#        --disable-doc
#        --disable-everything
#        --enable-avcodec
#        --enable-avformat
#        --enable-avutil
#        --enable-swscale
#
#        BUILD_COMMAND make -j
#        INSTALL_COMMAND make install
#        BUILD_IN_SOURCE 1
#        BUILD_BYPRODUCTS
#        ${CMAKE_BINARY_DIR}/ffmpeg/lib/libavcodec.a
#        ${CMAKE_BINARY_DIR}/ffmpeg/lib/libavformat.a
#        ${CMAKE_BINARY_DIR}/ffmpeg/lib/libavutil.a
#        ${CMAKE_BINARY_DIR}/ffmpeg/lib/swscale.a
#)
#
#add_library(avcodec STATIC IMPORTED)
#set_target_properties(avcodec PROPERTIES
#        IMPORTED_LOCATION ${CMAKE_BINARY_DIR}/ffmpeg/lib/libavcodec.a
#)
#
#add_library(avformat STATIC IMPORTED)
#set_target_properties(avformat PROPERTIES
#        IMPORTED_LOCATION ${CMAKE_BINARY_DIR}/ffmpeg/lib/libavformat.a
#)
#
#add_library(avutil STATIC IMPORTED)
#set_target_properties(avutil PROPERTIES
#        IMPORTED_LOCATION ${CMAKE_BINARY_DIR}/ffmpeg/lib/libavutil.a
#)
#
#add_library(swscale STATIC IMPORTED)
#set_target_properties(swscale PROPERTIES
#        IMPORTED_LOCATION ${CMAKE_BINARY_DIR}/ffmpeg/lib/swscale.a
#)
#
#add_dependencies(avcodec ffmpeg_build)
#add_dependencies(avformat ffmpeg_build)
#add_dependencies(avutil ffmpeg_build)
#add_dependencies(swscale ffmpeg_build)