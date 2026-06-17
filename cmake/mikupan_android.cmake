if(NOT ANDROID)
    return()
endif()

option(MIKUPAN_ANDROID_APK
        "Create a debug-signed Android APK target named MikuPan-apk."
        ON)

set(MIKUPAN_ANDROID_PACKAGE "io.github.mikompilation.mikupan"
        CACHE STRING "Android application package name.")
set(MIKUPAN_ANDROID_LABEL "MikuPan"
        CACHE STRING "Android launcher label.")
set(MIKUPAN_ANDROID_VERSION_CODE "1"
        CACHE STRING "Android application versionCode.")
set(MIKUPAN_ANDROID_VERSION_NAME "0.1"
        CACHE STRING "Android application versionName.")
set(MIKUPAN_ANDROID_MIN_SDK "21"
        CACHE STRING "Android minimum SDK version.")
set(MIKUPAN_ANDROID_TARGET_SDK "35"
        CACHE STRING "Android target SDK version.")

if(NOT MIKUPAN_ANDROID_APK)
    return()
endif()

if(NOT sdl3_SOURCE_DIR)
    message(FATAL_ERROR
            "MIKUPAN_ANDROID_APK requires SDL3 to be provided by cmake/sdl3.cmake.")
endif()

if(NOT TARGET SDL3::SDL3-shared)
    message(FATAL_ERROR
            "MIKUPAN_ANDROID_APK requires the shared SDL3 Android library.")
endif()

list(APPEND CMAKE_MODULE_PATH "${sdl3_SOURCE_DIR}/cmake/android")

if(NOT SDL_ANDROID_HOME)
    if(DEFINED ENV{ANDROID_HOME})
        set(SDL_ANDROID_HOME "$ENV{ANDROID_HOME}")
    elseif(DEFINED ENV{ANDROID_SDK_ROOT})
        set(SDL_ANDROID_HOME "$ENV{ANDROID_SDK_ROOT}")
    endif()
endif()
file(TO_CMAKE_PATH "${SDL_ANDROID_HOME}" SDL_ANDROID_HOME)

if(NOT SDL_ANDROID_HOME OR NOT IS_DIRECTORY "${SDL_ANDROID_HOME}")
    message(FATAL_ERROR
            "Android APK packaging requires SDL_ANDROID_HOME or ANDROID_HOME.")
endif()

if(NOT SDL_ANDROID_BUILD_TOOLS_ROOT
        OR NOT IS_DIRECTORY "${SDL_ANDROID_BUILD_TOOLS_ROOT}")
    file(GLOB MIKUPAN_ANDROID_BUILD_TOOL_DIRS
            LIST_DIRECTORIES TRUE
            "${SDL_ANDROID_HOME}/build-tools/*")
    if(NOT MIKUPAN_ANDROID_BUILD_TOOL_DIRS)
        message(FATAL_ERROR
                "No Android build-tools found under ${SDL_ANDROID_HOME}/build-tools.")
    endif()
    list(SORT MIKUPAN_ANDROID_BUILD_TOOL_DIRS
            COMPARE NATURAL
            ORDER DESCENDING)
    list(GET MIKUPAN_ANDROID_BUILD_TOOL_DIRS 0 SDL_ANDROID_BUILD_TOOLS_ROOT)
endif()

if(NOT SDL_ANDROID_PLATFORM_ANDROID_JAR
        OR NOT EXISTS "${SDL_ANDROID_PLATFORM_ANDROID_JAR}")
    set(MIKUPAN_ANDROID_PLATFORM_JAR
            "${SDL_ANDROID_HOME}/platforms/android-${MIKUPAN_ANDROID_TARGET_SDK}/android.jar")
    if(EXISTS "${MIKUPAN_ANDROID_PLATFORM_JAR}")
        set(SDL_ANDROID_PLATFORM_ANDROID_JAR
                "${MIKUPAN_ANDROID_PLATFORM_JAR}")
    else()
        file(GLOB MIKUPAN_ANDROID_PLATFORM_JARS
                "${SDL_ANDROID_HOME}/platforms/android-*/android.jar")
        if(NOT MIKUPAN_ANDROID_PLATFORM_JARS)
            message(FATAL_ERROR
                    "No Android platform android.jar found under ${SDL_ANDROID_HOME}/platforms.")
        endif()
        list(SORT MIKUPAN_ANDROID_PLATFORM_JARS
                COMPARE NATURAL
                ORDER DESCENDING)
        list(GET MIKUPAN_ANDROID_PLATFORM_JARS 0
                SDL_ANDROID_PLATFORM_ANDROID_JAR)
    endif()
endif()

foreach(tool_var AAPT2_BIN APKSIGNER_BIN D8_BIN ZIPALIGN_BIN
                 Java_JAVAC_EXECUTABLE Java_JAR_EXECUTABLE
                 KEYTOOL_BIN ZIP_BIN ADB_BIN)
    if(DEFINED ${tool_var}
            AND ${tool_var}
            AND NOT EXISTS "${${tool_var}}")
        unset(${tool_var} CACHE)
        unset(${tool_var})
    endif()
endforeach()

foreach(java_tool_var Java_JAVAC_EXECUTABLE Java_JAR_EXECUTABLE KEYTOOL_BIN)
    if(DEFINED ${java_tool_var} AND ${java_tool_var})
        file(TO_CMAKE_PATH "${${java_tool_var}}"
                MIKUPAN_ANDROID_JAVA_TOOL_PATH)
        string(FIND "${MIKUPAN_ANDROID_JAVA_TOOL_PATH}"
                "Common Files/Oracle/Java/javapath"
                MIKUPAN_ANDROID_JAVA_SHIM_POS)
        if(NOT MIKUPAN_ANDROID_JAVA_SHIM_POS EQUAL -1)
            unset(${java_tool_var} CACHE)
            unset(${java_tool_var})
        endif()
    endif()
endforeach()

find_program(AAPT2_BIN
        NAMES aapt2 aapt2.exe
        PATHS "${SDL_ANDROID_BUILD_TOOLS_ROOT}"
        NO_DEFAULT_PATH)
find_program(APKSIGNER_BIN
        NAMES apksigner apksigner.bat
        PATHS "${SDL_ANDROID_BUILD_TOOLS_ROOT}"
        NO_DEFAULT_PATH)
find_program(D8_BIN
        NAMES d8 d8.bat
        PATHS "${SDL_ANDROID_BUILD_TOOLS_ROOT}"
        NO_DEFAULT_PATH)
find_program(ZIPALIGN_BIN
        NAMES zipalign zipalign.exe
        PATHS "${SDL_ANDROID_BUILD_TOOLS_ROOT}"
        NO_DEFAULT_PATH)
find_program(ADB_BIN
        NAMES adb adb.exe
        PATHS "${SDL_ANDROID_HOME}/platform-tools"
        NO_DEFAULT_PATH)

find_program(Java_JAVAC_EXECUTABLE
        NAMES javac javac.exe
        PATHS
            "C:/Program Files/Android/Android Studio/jbr/bin"
            "$ENV{JAVA_HOME}/bin"
        NO_DEFAULT_PATH)
find_program(Java_JAVAC_EXECUTABLE
        NAMES javac javac.exe)
find_program(Java_JAR_EXECUTABLE
        NAMES jar jar.exe
        PATHS
            "C:/Program Files/Android/Android Studio/jbr/bin"
            "$ENV{JAVA_HOME}/bin"
        NO_DEFAULT_PATH)
find_program(Java_JAR_EXECUTABLE
        NAMES jar jar.exe)
find_program(KEYTOOL_BIN
        NAMES keytool keytool.exe
        PATHS
            "C:/Program Files/Android/Android Studio/jbr/bin"
            "$ENV{JAVA_HOME}/bin"
        NO_DEFAULT_PATH)
find_program(KEYTOOL_BIN
        NAMES keytool keytool.exe)
find_program(ZIP_BIN
        NAMES zip zip.exe
        PATHS
            "C:/msys64/usr/bin"
            "C:/cygwin64/bin"
            "C:/Program Files/Git/usr/bin"
        NO_DEFAULT_PATH)
find_program(ZIP_BIN
        NAMES zip zip.exe)

foreach(tool AAPT2_BIN APKSIGNER_BIN D8_BIN ZIPALIGN_BIN
             Java_JAVAC_EXECUTABLE Java_JAR_EXECUTABLE KEYTOOL_BIN ZIP_BIN)
    if(NOT ${tool} OR NOT EXISTS "${${tool}}")
        message(FATAL_ERROR "Required Android packaging tool not found: ${tool}")
    endif()
endforeach()

function(mikupan_import_android_tool target path)
    if(NOT TARGET ${target})
        add_executable(${target} IMPORTED)
        set_property(TARGET ${target} PROPERTY IMPORTED_LOCATION "${path}")
    endif()
endfunction()

mikupan_import_android_tool(SdlAndroid::aapt2 "${AAPT2_BIN}")
mikupan_import_android_tool(SdlAndroid::apksigner "${APKSIGNER_BIN}")
mikupan_import_android_tool(SdlAndroid::d8 "${D8_BIN}")
mikupan_import_android_tool(SdlAndroid::zipalign "${ZIPALIGN_BIN}")
mikupan_import_android_tool(SdlAndroid::keytool "${KEYTOOL_BIN}")
mikupan_import_android_tool(SdlAndroid::zip "${ZIP_BIN}")
if(ADB_BIN)
    mikupan_import_android_tool(SdlAndroid::adb "${ADB_BIN}")
endif()

include(SdlAndroidFunctions)

set(MIKUPAN_ANDROID_GENERATED_DIR "${CMAKE_BINARY_DIR}/android")
set(MIKUPAN_ANDROID_INTERMEDIATES_DIR "${CMAKE_BINARY_DIR}/intermediates")
set(MIKUPAN_ANDROID_APK_WORK_DIR
        "${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/MikuPan-unaligned-apk.dir")
file(MAKE_DIRECTORY
        "${MIKUPAN_ANDROID_GENERATED_DIR}"
        "${MIKUPAN_ANDROID_INTERMEDIATES_DIR}"
        "${MIKUPAN_ANDROID_APK_WORK_DIR}")

set(MIKUPAN_ANDROID_MANIFEST
        "${MIKUPAN_ANDROID_GENERATED_DIR}/AndroidManifest.xml")
configure_file(
        "${CMAKE_SOURCE_DIR}/android/AndroidManifest.xml.in"
        "${MIKUPAN_ANDROID_MANIFEST}"
        @ONLY)

set(MIKUPAN_ANDROID_ICON_RESOURCE
        "${MIKUPAN_ANDROID_GENERATED_DIR}/res/drawable/mikupan.png")
set(MIKUPAN_ANDROID_ICON_FLAT
        "${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/MikuPan-apk-res.dir/drawable_mikupan.png.flat")
set(MIKUPAN_ANDROID_ICON_AAPT2_INPUT "android/res/drawable/mikupan.png")
if(CMAKE_HOST_WIN32)
    string(REPLACE "/" "\\" MIKUPAN_ANDROID_ICON_AAPT2_INPUT
            "${MIKUPAN_ANDROID_ICON_AAPT2_INPUT}")
endif()
add_custom_command(
        OUTPUT "${MIKUPAN_ANDROID_ICON_RESOURCE}"
        COMMAND "${CMAKE_COMMAND}" -E make_directory
                "${MIKUPAN_ANDROID_GENERATED_DIR}/res/drawable"
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                "${CMAKE_SOURCE_DIR}/resources/mikupan.png"
                "${MIKUPAN_ANDROID_ICON_RESOURCE}"
        DEPENDS "${CMAKE_SOURCE_DIR}/resources/mikupan.png"
        VERBATIM)
add_custom_command(
        OUTPUT "${MIKUPAN_ANDROID_ICON_FLAT}"
        COMMAND "${CMAKE_COMMAND}" -E make_directory
                "${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/MikuPan-apk-res.dir"
        COMMAND SdlAndroid::aapt2 compile
                -o "${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/MikuPan-apk-res.dir"
                "${MIKUPAN_ANDROID_ICON_AAPT2_INPUT}"
        DEPENDS "${MIKUPAN_ANDROID_ICON_RESOURCE}"
        WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
        VERBATIM)
add_custom_target(MikuPan-apk-res
        DEPENDS "${MIKUPAN_ANDROID_ICON_FLAT}")
set_property(TARGET MikuPan-apk-res PROPERTY OUTPUTS
        "${MIKUPAN_ANDROID_ICON_FLAT}")
set_property(TARGET MikuPan-apk-res PROPERTY SOURCES
        "${MIKUPAN_ANDROID_ICON_RESOURCE}")

if(TARGET SDL3::Jar)
    set(MIKUPAN_ANDROID_SDL_JAR "$<TARGET_PROPERTY:SDL3::Jar,JAR_FILE>")
    set(MIKUPAN_ANDROID_SDL_JAR_DEPENDS "${MIKUPAN_ANDROID_SDL_JAR}")
else()
    set(MIKUPAN_ANDROID_SDL_JAVA_ROOT
            "${sdl3_SOURCE_DIR}/android-project/app/src/main/java")
    file(GLOB MIKUPAN_ANDROID_SDL_JAVA_SOURCES
            "${MIKUPAN_ANDROID_SDL_JAVA_ROOT}/org/libsdl/app/*.java")
    set(MIKUPAN_ANDROID_SDL_JAVA_CLASSES
            "${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/MikuPan-SDL3-jar.dir/classes")
    set(MIKUPAN_ANDROID_SDL_JAR
            "${CMAKE_BINARY_DIR}/android/SDL3.jar")

    add_custom_command(
            OUTPUT "${MIKUPAN_ANDROID_SDL_JAR}"
            COMMAND "${CMAKE_COMMAND}" -E remove_directory
                    "${MIKUPAN_ANDROID_SDL_JAVA_CLASSES}"
            COMMAND "${CMAKE_COMMAND}" -E make_directory
                    "${MIKUPAN_ANDROID_SDL_JAVA_CLASSES}"
            COMMAND "${Java_JAVAC_EXECUTABLE}"
                    -source 1.8
                    -target 1.8
                    -encoding utf-8
                    -bootclasspath "${SDL_ANDROID_PLATFORM_ANDROID_JAR}"
                    -cp "${SDL_ANDROID_PLATFORM_ANDROID_JAR}"
                    -d "${MIKUPAN_ANDROID_SDL_JAVA_CLASSES}"
                    ${MIKUPAN_ANDROID_SDL_JAVA_SOURCES}
            COMMAND "${Java_JAR_EXECUTABLE}" cf
                    "${MIKUPAN_ANDROID_SDL_JAR}"
                    -C "${MIKUPAN_ANDROID_SDL_JAVA_CLASSES}" .
            DEPENDS ${MIKUPAN_ANDROID_SDL_JAVA_SOURCES}
            VERBATIM)
    add_custom_target(MikuPan-SDL3-jar
            DEPENDS "${MIKUPAN_ANDROID_SDL_JAR}")
    set(MIKUPAN_ANDROID_SDL_JAR_DEPENDS MikuPan-SDL3-jar)
endif()

set(MIKUPAN_ANDROID_DEBUG_KEYSTORE
        "${CMAKE_BINARY_DIR}/MikuPan-debug-keystore_debug.keystore")
add_custom_command(
        OUTPUT "${MIKUPAN_ANDROID_DEBUG_KEYSTORE}"
        COMMAND "${CMAKE_COMMAND}" -E rm -f
                "${MIKUPAN_ANDROID_DEBUG_KEYSTORE}"
        COMMAND "${KEYTOOL_BIN}" -genkeypair
                -keystore "${MIKUPAN_ANDROID_DEBUG_KEYSTORE}"
                -storepass android
                -alias androiddebugkey
                -keypass android
                -keyalg RSA
                -keysize 2048
                -validity 10000
                -dname "C=US, O=Android, CN=Android Debug"
                -noprompt
        VERBATIM)
add_custom_target(MikuPan-debug-keystore
        DEPENDS "${MIKUPAN_ANDROID_DEBUG_KEYSTORE}")
set_property(TARGET MikuPan-debug-keystore PROPERTY OUTPUT
        "${MIKUPAN_ANDROID_DEBUG_KEYSTORE}")

sdl_android_link_resources(MikuPan-apk-linked
        MANIFEST "${MIKUPAN_ANDROID_MANIFEST}"
        PACKAGE "${MIKUPAN_ANDROID_PACKAGE}"
        MIN_SDK_VERSION "${MIKUPAN_ANDROID_MIN_SDK}"
        TARGET_SDK_VERSION "${MIKUPAN_ANDROID_TARGET_SDK}"
        RES_TARGETS MikuPan-apk-res
)

set(MIKUPAN_ANDROID_DEX_DIR
        "${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/MikuPan-dex.dir")
set(MIKUPAN_ANDROID_CLASSES_DEX
        "${MIKUPAN_ANDROID_DEX_DIR}/classes.dex")
file(MAKE_DIRECTORY "${MIKUPAN_ANDROID_DEX_DIR}")

add_custom_command(
        OUTPUT "${MIKUPAN_ANDROID_CLASSES_DEX}"
        COMMAND "${CMAKE_COMMAND}" -E remove_directory
                "${MIKUPAN_ANDROID_DEX_DIR}"
        COMMAND "${CMAKE_COMMAND}" -E make_directory
                "${MIKUPAN_ANDROID_DEX_DIR}"
        COMMAND "${D8_BIN}"
                "${MIKUPAN_ANDROID_SDL_JAR}"
                --lib "${SDL_ANDROID_PLATFORM_ANDROID_JAR}"
                --output "${MIKUPAN_ANDROID_DEX_DIR}"
        DEPENDS ${MIKUPAN_ANDROID_SDL_JAR_DEPENDS}
        VERBATIM)
add_custom_target(MikuPan-dex DEPENDS "${MIKUPAN_ANDROID_CLASSES_DEX}")
set_property(TARGET MikuPan-dex PROPERTY OUTPUT
        "${MIKUPAN_ANDROID_CLASSES_DEX}")

set(MIKUPAN_ANDROID_UNALIGNED_APK
        "${MIKUPAN_ANDROID_INTERMEDIATES_DIR}/MikuPan-unaligned.apk")

# The shared C++ runtime (libc++_shared.so) is an NDK file, not a build target,
# so nothing we build produces it. It becomes a load-time dependency in two ways:
#   1. ANDROID_STL=c++_shared makes our own libraries (libmain.so, libSDL3.so)
#      carry a DT_NEEDED on libc++_shared.so.
#   2. A prebuilt third-party library we package can carry that DT_NEEDED even
#      when our own code is built with c++_static -- the prebuilt FFmpeg
#      libffmpeg.so is built against c++_shared and needs it.
# Either way, if libc++_shared.so is not packaged the loader fails on launch with
# "library libc++_shared.so not found" and the app crashes. Gradle bundles it
# automatically -- this hand-rolled packaging must do the same.
set(MIKUPAN_ANDROID_STL_TYPE "${ANDROID_STL}")
if(NOT MIKUPAN_ANDROID_STL_TYPE)
    set(MIKUPAN_ANDROID_STL_TYPE "${CMAKE_ANDROID_STL_TYPE}")
endif()

set(MIKUPAN_ANDROID_NEEDS_LIBCXX_SHARED FALSE)
if(MIKUPAN_ANDROID_STL_TYPE STREQUAL "c++_shared")
    set(MIKUPAN_ANDROID_NEEDS_LIBCXX_SHARED TRUE)
endif()

# Scan the prebuilt shared libraries we package for a DT_NEEDED on
# libc++_shared.so. This catches third-party libs (e.g. FFmpeg) that pull in the
# shared runtime regardless of our own ANDROID_STL setting.
if(NOT MIKUPAN_ANDROID_NEEDS_LIBCXX_SHARED)
    if(CMAKE_READELF AND EXISTS "${CMAKE_READELF}")
        set(MIKUPAN_ANDROID_READELF "${CMAKE_READELF}")
    else()
        find_program(MIKUPAN_ANDROID_READELF_BIN
                NAMES llvm-readelf llvm-readelf.exe
                PATHS "${ANDROID_TOOLCHAIN_ROOT}/bin"
                NO_DEFAULT_PATH)
        find_program(MIKUPAN_ANDROID_READELF_BIN
                NAMES llvm-readelf llvm-readelf.exe)
        set(MIKUPAN_ANDROID_READELF "${MIKUPAN_ANDROID_READELF_BIN}")
    endif()

    set(MIKUPAN_ANDROID_PREBUILT_LIBS)
    foreach(prebuilt_lib_target IN LISTS MIKUPAN_ANDROID_SHARED_LIB_TARGETS)
        if(TARGET ${prebuilt_lib_target})
            get_target_property(prebuilt_lib_is_imported
                    ${prebuilt_lib_target} IMPORTED)
            get_target_property(prebuilt_lib_location
                    ${prebuilt_lib_target} IMPORTED_LOCATION)
            if(prebuilt_lib_is_imported
                    AND prebuilt_lib_location
                    AND EXISTS "${prebuilt_lib_location}")
                list(APPEND MIKUPAN_ANDROID_PREBUILT_LIBS
                        "${prebuilt_lib_location}")
            endif()
        endif()
    endforeach()
    list(REMOVE_DUPLICATES MIKUPAN_ANDROID_PREBUILT_LIBS)

    foreach(prebuilt_lib IN LISTS MIKUPAN_ANDROID_PREBUILT_LIBS)
        if(MIKUPAN_ANDROID_READELF)
            execute_process(
                    COMMAND "${MIKUPAN_ANDROID_READELF}" -d "${prebuilt_lib}"
                    OUTPUT_VARIABLE MIKUPAN_ANDROID_READELF_OUT
                    ERROR_QUIET)
            if(MIKUPAN_ANDROID_READELF_OUT MATCHES "libc\\+\\+_shared\\.so")
                set(MIKUPAN_ANDROID_NEEDS_LIBCXX_SHARED TRUE)
                break()
            endif()
        else()
            # No readelf available to inspect the prebuilt library: bundle the
            # shared runtime defensively so a c++_shared prebuilt cannot crash
            # the app at load.
            set(MIKUPAN_ANDROID_NEEDS_LIBCXX_SHARED TRUE)
            break()
        endif()
    endforeach()
endif()

set(MIKUPAN_ANDROID_LIBCXX_SHARED "")
if(MIKUPAN_ANDROID_NEEDS_LIBCXX_SHARED)
    if(ANDROID_ABI STREQUAL "arm64-v8a")
        set(MIKUPAN_ANDROID_LIBCXX_TRIPLE "aarch64-linux-android")
    elseif(ANDROID_ABI STREQUAL "armeabi-v7a")
        set(MIKUPAN_ANDROID_LIBCXX_TRIPLE "arm-linux-androideabi")
    elseif(ANDROID_ABI STREQUAL "x86_64")
        set(MIKUPAN_ANDROID_LIBCXX_TRIPLE "x86_64-linux-android")
    elseif(ANDROID_ABI STREQUAL "x86")
        set(MIKUPAN_ANDROID_LIBCXX_TRIPLE "i686-linux-android")
    elseif(ANDROID_ABI STREQUAL "riscv64")
        set(MIKUPAN_ANDROID_LIBCXX_TRIPLE "riscv64-linux-android")
    else()
        set(MIKUPAN_ANDROID_LIBCXX_TRIPLE "${CMAKE_ANDROID_ARCH_TRIPLE}")
    endif()

    foreach(libcxx_candidate
            "${CMAKE_SYSROOT}/usr/lib/${MIKUPAN_ANDROID_LIBCXX_TRIPLE}/libc++_shared.so"
            "${ANDROID_TOOLCHAIN_ROOT}/sysroot/usr/lib/${MIKUPAN_ANDROID_LIBCXX_TRIPLE}/libc++_shared.so"
            "${ANDROID_NDK}/toolchains/llvm/prebuilt/${ANDROID_HOST_TAG}/sysroot/usr/lib/${MIKUPAN_ANDROID_LIBCXX_TRIPLE}/libc++_shared.so"
            "${CMAKE_ANDROID_NDK}/toolchains/llvm/prebuilt/${ANDROID_HOST_TAG}/sysroot/usr/lib/${MIKUPAN_ANDROID_LIBCXX_TRIPLE}/libc++_shared.so")
        if(libcxx_candidate AND EXISTS "${libcxx_candidate}")
            set(MIKUPAN_ANDROID_LIBCXX_SHARED "${libcxx_candidate}")
            break()
        endif()
    endforeach()

    if(NOT MIKUPAN_ANDROID_LIBCXX_SHARED)
        file(GLOB_RECURSE MIKUPAN_ANDROID_LIBCXX_GLOB
                "${ANDROID_NDK}/toolchains/llvm/prebuilt/*/sysroot/usr/lib/${MIKUPAN_ANDROID_LIBCXX_TRIPLE}/libc++_shared.so"
                "${CMAKE_ANDROID_NDK}/toolchains/llvm/prebuilt/*/sysroot/usr/lib/${MIKUPAN_ANDROID_LIBCXX_TRIPLE}/libc++_shared.so")
        if(MIKUPAN_ANDROID_LIBCXX_GLOB)
            list(GET MIKUPAN_ANDROID_LIBCXX_GLOB 0 MIKUPAN_ANDROID_LIBCXX_SHARED)
        endif()
    endif()

    if(NOT MIKUPAN_ANDROID_LIBCXX_SHARED)
        message(FATAL_ERROR
                "A packaged library needs libc++_shared.so but it was not found "
                "for ${ANDROID_ABI} (triple ${MIKUPAN_ANDROID_LIBCXX_TRIPLE}). "
                "Looked under the NDK toolchain sysroot. Check your NDK "
                "installation or set ANDROID_TOOLCHAIN_ROOT.")
    endif()

    message(STATUS
            "Bundling Android C++ runtime into APK: "
            "${MIKUPAN_ANDROID_LIBCXX_SHARED}")
endif()

set(MIKUPAN_ANDROID_EXTRA_LIB_COMMANDS)
set(MIKUPAN_ANDROID_EXTRA_LIB_DEPENDS)
foreach(android_shared_lib IN LISTS MIKUPAN_ANDROID_SHARED_LIB_TARGETS)
    list(APPEND MIKUPAN_ANDROID_EXTRA_LIB_COMMANDS
            COMMAND "${CMAKE_COMMAND}" -E copy
                    "$<TARGET_FILE:${android_shared_lib}>"
                    "lib/${ANDROID_ABI}/$<TARGET_FILE_NAME:${android_shared_lib}>"
            COMMAND "${ZIP_BIN}" -u -q
                    "${MIKUPAN_ANDROID_UNALIGNED_APK}"
                    "lib/${ANDROID_ABI}/$<TARGET_FILE_NAME:${android_shared_lib}>")
    list(APPEND MIKUPAN_ANDROID_EXTRA_LIB_DEPENDS "${android_shared_lib}")
endforeach()

if(MIKUPAN_ANDROID_LIBCXX_SHARED)
    list(APPEND MIKUPAN_ANDROID_EXTRA_LIB_COMMANDS
            COMMAND "${CMAKE_COMMAND}" -E copy
                    "${MIKUPAN_ANDROID_LIBCXX_SHARED}"
                    "lib/${ANDROID_ABI}/libc++_shared.so"
            COMMAND "${ZIP_BIN}" -u -q
                    "${MIKUPAN_ANDROID_UNALIGNED_APK}"
                    "lib/${ANDROID_ABI}/libc++_shared.so")
endif()

add_custom_command(
        OUTPUT "${MIKUPAN_ANDROID_UNALIGNED_APK}"
        COMMAND "${CMAKE_COMMAND}" -E remove_directory "lib"
        COMMAND "${CMAKE_COMMAND}" -E remove_directory "assets"
        COMMAND "${CMAKE_COMMAND}" -E make_directory "lib/${ANDROID_ABI}"
        COMMAND "${CMAKE_COMMAND}" -E make_directory "assets"
        COMMAND "${CMAKE_COMMAND}" -E copy
                "$<TARGET_PROPERTY:MikuPan-apk-linked,OUTPUT>"
                "${MIKUPAN_ANDROID_UNALIGNED_APK}"
        COMMAND "${CMAKE_COMMAND}" -E copy
                "$<TARGET_PROPERTY:MikuPan-dex,OUTPUT>"
                "classes.dex"
        COMMAND "${ZIP_BIN}" -u -q -j
                "${MIKUPAN_ANDROID_UNALIGNED_APK}"
                "classes.dex"
        COMMAND "${CMAKE_COMMAND}" -E copy
                "$<TARGET_FILE:SDL3::SDL3-shared>"
                "lib/${ANDROID_ABI}/$<TARGET_FILE_NAME:SDL3::SDL3-shared>"
        COMMAND "${ZIP_BIN}" -u -q
                "${MIKUPAN_ANDROID_UNALIGNED_APK}"
                "lib/${ANDROID_ABI}/$<TARGET_FILE_NAME:SDL3::SDL3-shared>"
        COMMAND "${CMAKE_COMMAND}" -E copy
                "$<TARGET_FILE:MikuPan>"
                "lib/${ANDROID_ABI}/$<TARGET_FILE_NAME:MikuPan>"
        COMMAND "${ZIP_BIN}" -u -q
                "${MIKUPAN_ANDROID_UNALIGNED_APK}"
                "lib/${ANDROID_ABI}/$<TARGET_FILE_NAME:MikuPan>"
        ${MIKUPAN_ANDROID_EXTRA_LIB_COMMANDS}
        COMMAND "${CMAKE_COMMAND}" -E copy_directory
                "${CMAKE_SOURCE_DIR}/resources"
                "assets/resources"
        COMMAND "${ZIP_BIN}" -u -r -q
                "${MIKUPAN_ANDROID_UNALIGNED_APK}"
                "assets"
        DEPENDS
                MikuPan-apk-linked
                MikuPan-dex
                MikuPan
                SDL3::SDL3-shared
                ${MIKUPAN_ANDROID_EXTRA_LIB_DEPENDS}
                compile_shaders
        WORKING_DIRECTORY "${MIKUPAN_ANDROID_APK_WORK_DIR}"
        VERBATIM)
add_custom_target(MikuPan-unaligned-apk
        DEPENDS "${MIKUPAN_ANDROID_UNALIGNED_APK}")
set_property(TARGET MikuPan-unaligned-apk PROPERTY OUTPUT
        "${MIKUPAN_ANDROID_UNALIGNED_APK}")

sdl_apk_align(MikuPan-aligned-apk MikuPan-unaligned-apk
        OUTDIR "${MIKUPAN_ANDROID_INTERMEDIATES_DIR}")
sdl_apk_sign(MikuPan-apk MikuPan-aligned-apk
        KEYSTORE MikuPan-debug-keystore
        OUTPUT "${CMAKE_BINARY_DIR}/MikuPan.apk")

if(TARGET SdlAndroid::adb)
    add_custom_target(install-MikuPan
            COMMAND "${CMAKE_COMMAND}" -DACTION=install
                    "-DAPKS=$<TARGET_PROPERTY:MikuPan-apk,OUTPUT>"
                    -P "${sdl3_SOURCE_DIR}/cmake/android/SdlAndroidScript.cmake"
            DEPENDS MikuPan-apk
            VERBATIM)
    add_custom_target(start-MikuPan
            COMMAND "$<TARGET_FILE:SdlAndroid::adb>" shell am start-activity -S
                    "${MIKUPAN_ANDROID_PACKAGE}/org.libsdl.app.SDLActivity"
            VERBATIM)
    add_custom_target(uninstall-MikuPan
            COMMAND "$<TARGET_FILE:SdlAndroid::adb>" uninstall
                    "${MIKUPAN_ANDROID_PACKAGE}"
            VERBATIM)
endif()
