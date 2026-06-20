set(MIKUPAN_FFMPEG_LIBS
        avcodec
        avformat
        avutil
        swscale
        swresample)

if(ANDROID)
    set(MIKUPAN_ANDROID_FFMPEG_ROOT
            "${CMAKE_SOURCE_DIR}/extern/ffmpeg/android"
            CACHE PATH
            "Root containing Android FFmpeg prebuilt libraries, grouped by ABI.")
    set(MIKUPAN_ANDROID_FFMPEG_INCLUDE_DIR ""
            CACHE PATH
            "Optional Android FFmpeg include directory containing libavcodec/avcodec.h.")
    set(MIKUPAN_ANDROID_FFMPEG_LIBRARY ""
            CACHE FILEPATH
            "Optional path to a merged Android libffmpeg.so.")

    set(FFMPEG_ROOT "${MIKUPAN_ANDROID_FFMPEG_ROOT}/${ANDROID_ABI}")
    set(FFMPEG_INCLUDE_DIR)
    foreach(ffmpeg_include_dir
            "${MIKUPAN_ANDROID_FFMPEG_INCLUDE_DIR}"
            "${FFMPEG_ROOT}/include"
            "${MIKUPAN_ANDROID_FFMPEG_ROOT}/include"
            "${MIKUPAN_ANDROID_FFMPEG_ROOT}/ffmpeg/include"
            "${CMAKE_SOURCE_DIR}/extern/ffmpeg/include")
        if(ffmpeg_include_dir
                AND EXISTS "${ffmpeg_include_dir}/libavcodec/avcodec.h")
            set(FFMPEG_INCLUDE_DIR "${ffmpeg_include_dir}")
            break()
        endif()
    endforeach()
    if(NOT FFMPEG_INCLUDE_DIR)
        file(GLOB_RECURSE FFMPEG_AVCODEC_HEADER_CANDIDATES
                "${MIKUPAN_ANDROID_FFMPEG_ROOT}/libavcodec/avcodec.h"
                "${MIKUPAN_ANDROID_FFMPEG_ROOT}/*/libavcodec/avcodec.h"
                "${MIKUPAN_ANDROID_FFMPEG_ROOT}/*/*/libavcodec/avcodec.h"
                "${MIKUPAN_ANDROID_FFMPEG_ROOT}/*/*/*/libavcodec/avcodec.h")
        if(FFMPEG_AVCODEC_HEADER_CANDIDATES)
            list(SORT FFMPEG_AVCODEC_HEADER_CANDIDATES)
            list(GET FFMPEG_AVCODEC_HEADER_CANDIDATES 0
                    FFMPEG_AVCODEC_HEADER)
            get_filename_component(FFMPEG_AVCODEC_HEADER_DIR
                    "${FFMPEG_AVCODEC_HEADER}" DIRECTORY)
            get_filename_component(FFMPEG_INCLUDE_DIR
                    "${FFMPEG_AVCODEC_HEADER_DIR}" DIRECTORY)
        endif()
    endif()

    set(FFMPEG_LIB_SEARCH_DIRS
            "${FFMPEG_ROOT}/lib"
            "${FFMPEG_ROOT}"
            "${CMAKE_SOURCE_DIR}/extern/ffmpeg-android/${ANDROID_ABI}/lib"
            "${CMAKE_SOURCE_DIR}/extern/ffmpeg-android/${ANDROID_ABI}")
    set(FFMPEG_SINGLE_SHARED_LIBRARY)
    if(MIKUPAN_ANDROID_FFMPEG_LIBRARY)
        if(EXISTS "${MIKUPAN_ANDROID_FFMPEG_LIBRARY}")
            set(FFMPEG_SINGLE_SHARED_LIBRARY
                    "${MIKUPAN_ANDROID_FFMPEG_LIBRARY}")
        else()
            message(FATAL_ERROR
                    "MIKUPAN_ANDROID_FFMPEG_LIBRARY does not exist: "
                    "${MIKUPAN_ANDROID_FFMPEG_LIBRARY}")
        endif()
    else()
        foreach(ffmpeg_lib_dir IN LISTS FFMPEG_LIB_SEARCH_DIRS)
            if(EXISTS "${ffmpeg_lib_dir}/libffmpeg.so")
                set(FFMPEG_SINGLE_SHARED_LIBRARY
                        "${ffmpeg_lib_dir}/libffmpeg.so")
                break()
            endif()
        endforeach()
        if(NOT FFMPEG_SINGLE_SHARED_LIBRARY)
            file(GLOB_RECURSE FFMPEG_SINGLE_SHARED_LIBRARY_CANDIDATES
                    "${MIKUPAN_ANDROID_FFMPEG_ROOT}/libffmpeg.so"
                    "${MIKUPAN_ANDROID_FFMPEG_ROOT}/*/libffmpeg.so"
                    "${MIKUPAN_ANDROID_FFMPEG_ROOT}/*/*/libffmpeg.so"
                    "${MIKUPAN_ANDROID_FFMPEG_ROOT}/*/*/*/libffmpeg.so"
                    "${CMAKE_SOURCE_DIR}/extern/ffmpeg-android/libffmpeg.so"
                    "${CMAKE_SOURCE_DIR}/extern/ffmpeg-android/*/libffmpeg.so"
                    "${CMAKE_SOURCE_DIR}/extern/ffmpeg-android/*/*/libffmpeg.so")
            if(FFMPEG_SINGLE_SHARED_LIBRARY_CANDIDATES)
                list(SORT FFMPEG_SINGLE_SHARED_LIBRARY_CANDIDATES)
                list(GET FFMPEG_SINGLE_SHARED_LIBRARY_CANDIDATES 0
                        FFMPEG_SINGLE_SHARED_LIBRARY)
            endif()
        endif()
    endif()

    if(NOT FFMPEG_INCLUDE_DIR)
        message(FATAL_ERROR
                "Android FFmpeg headers were not found for ${ANDROID_ABI}.\n"
                "Set MIKUPAN_ANDROID_FFMPEG_INCLUDE_DIR to the directory "
                "that contains libavcodec/avcodec.h.")
    endif()

    if(FFMPEG_SINGLE_SHARED_LIBRARY)
        message(STATUS
                "Using Android FFmpeg library: ${FFMPEG_SINGLE_SHARED_LIBRARY}")
        message(STATUS
                "Prebuilt Android FFmpeg libraries must be linked with "
                "-Wl,-z,max-page-size=16384 for 16 KB page-size devices.")

        add_library(ffmpeg SHARED IMPORTED)
        set_target_properties(ffmpeg PROPERTIES
                IMPORTED_LOCATION "${FFMPEG_SINGLE_SHARED_LIBRARY}"
                INTERFACE_INCLUDE_DIRECTORIES "${FFMPEG_INCLUDE_DIR}"
                INTERFACE_LINK_LIBRARIES "android;log"
        )

        foreach(ffmpeg_lib IN LISTS MIKUPAN_FFMPEG_LIBS)
            add_library(${ffmpeg_lib} INTERFACE IMPORTED)
            set_target_properties(${ffmpeg_lib} PROPERTIES
                    INTERFACE_INCLUDE_DIRECTORIES "${FFMPEG_INCLUDE_DIR}"
                    INTERFACE_LINK_LIBRARIES ffmpeg
            )
        endforeach()

        set(MIKUPAN_ANDROID_SHARED_LIB_TARGETS ffmpeg)
    else()
        set(FFMPEG_SPLIT_SHARED_LIBRARIES)
        foreach(ffmpeg_lib IN LISTS MIKUPAN_FFMPEG_LIBS)
            set(ffmpeg_lib_path)
            foreach(ffmpeg_lib_dir IN LISTS FFMPEG_LIB_SEARCH_DIRS)
                if(EXISTS "${ffmpeg_lib_dir}/lib${ffmpeg_lib}.so")
                    set(ffmpeg_lib_path
                            "${ffmpeg_lib_dir}/lib${ffmpeg_lib}.so")
                    break()
                endif()
            endforeach()

            if(NOT ffmpeg_lib_path)
                message(FATAL_ERROR
                        "Android FFmpeg library was not found for ${ANDROID_ABI}.\n"
                        "Expected merged library:\n"
                        "  ${FFMPEG_ROOT}/lib/libffmpeg.so\n"
                        "  ${FFMPEG_ROOT}/libffmpeg.so\n"
                        "  ${CMAKE_SOURCE_DIR}/extern/ffmpeg-android/${ANDROID_ABI}/libffmpeg.so\n"
                        "Or set MIKUPAN_ANDROID_FFMPEG_LIBRARY directly to "
                        "your libffmpeg.so path.")
            endif()

            list(APPEND FFMPEG_SPLIT_SHARED_LIBRARIES "${ffmpeg_lib_path}")
        endforeach()

        foreach(ffmpeg_lib ffmpeg_lib_path IN ZIP_LISTS
                MIKUPAN_FFMPEG_LIBS FFMPEG_SPLIT_SHARED_LIBRARIES)
            add_library(${ffmpeg_lib} SHARED IMPORTED)
            set_target_properties(${ffmpeg_lib} PROPERTIES
                    IMPORTED_LOCATION "${ffmpeg_lib_path}"
                    INTERFACE_INCLUDE_DIRECTORIES "${FFMPEG_INCLUDE_DIR}"
                    INTERFACE_LINK_LIBRARIES "android;log"
            )
        endforeach()

        set(MIKUPAN_ANDROID_SHARED_LIB_TARGETS ${MIKUPAN_FFMPEG_LIBS})
    endif()
elseif(WIN32)
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

    # FFmpeg ships no CMakeLists.txt, so MakeAvailable just populates the
    # sources and sets ffmpeg_SOURCE_DIR.
    FetchContent_MakeAvailable(ffmpeg)

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
    set(FFMPEG_SYSTEM_LIBS Threads::Threads)
    if(NOT MSVC)
        list(APPEND FFMPEG_SYSTEM_LIBS m)
    endif()
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
