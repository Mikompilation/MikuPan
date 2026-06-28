include(FetchContent)

FetchContent_Declare(
        sdl3
        GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
        GIT_TAG ec0066aa0b8de1e8fca9e179ad3e33c0607e9a97
)
if(ANDROID)
    set(SDL_STATIC OFF)
    set(SDL_SHARED ON)
    set(SDL_ANDROID_JAR ON)
else()
    set(SDL_STATIC ON)
    set(SDL_SHARED OFF)
endif()
FetchContent_MakeAvailable(sdl3)

foreach(sdl_target SDL3-static SDL3-shared SDL3_test)
    if(TARGET ${sdl_target})
        set_target_properties(${sdl_target} PROPERTIES
                DISABLE_PRECOMPILE_HEADERS ON)
    endif()
endforeach()

if(ANDROID)
    set(MIKUPAN_SDL_ACTIVITY_JAVA
            "${sdl3_SOURCE_DIR}/android-project/app/src/main/java/org/libsdl/app/SDLActivity.java")
    if(EXISTS "${MIKUPAN_SDL_ACTIVITY_JAVA}")
        file(READ "${MIKUPAN_SDL_ACTIVITY_JAVA}" MIKUPAN_SDL_ACTIVITY_CONTENT)
        set(MIKUPAN_SDL_ACTIVITY_OPEN_FD_OLD [=[
        } catch (FileNotFoundException e) {
            e.printStackTrace();
            return -1;
        }
]=])
        set(MIKUPAN_SDL_ACTIVITY_OPEN_FD_NEW [=[
        } catch (Exception e) {
            e.printStackTrace();
            return -1;
        }
]=])
        if(MIKUPAN_SDL_ACTIVITY_CONTENT MATCHES "catch \\(FileNotFoundException e\\)")
            string(REPLACE "${MIKUPAN_SDL_ACTIVITY_OPEN_FD_OLD}"
                    "${MIKUPAN_SDL_ACTIVITY_OPEN_FD_NEW}"
                    MIKUPAN_SDL_ACTIVITY_CONTENT
                    "${MIKUPAN_SDL_ACTIVITY_CONTENT}")
            file(WRITE "${MIKUPAN_SDL_ACTIVITY_JAVA}" "${MIKUPAN_SDL_ACTIVITY_CONTENT}")
        endif()
    endif()

    set(MIKUPAN_SDL_ANDROID_C
            "${sdl3_SOURCE_DIR}/src/core/android/SDL_android.c")
    if(EXISTS "${MIKUPAN_SDL_ANDROID_C}")
        file(READ "${MIKUPAN_SDL_ANDROID_C}" MIKUPAN_SDL_ANDROID_CONTENT)
        set(MIKUPAN_SDL_OPEN_FD_JNI_OLD [=[
    jint fd = (*env)->CallStaticIntMethod(env, mActivityClass, midOpenFileDescriptor, jstringUri, jstringMode);
    (*env)->DeleteLocalRef(env, jstringUri);
]=])
        set(MIKUPAN_SDL_OPEN_FD_JNI_NEW [=[
    jint fd = (*env)->CallStaticIntMethod(env, mActivityClass, midOpenFileDescriptor, jstringUri, jstringMode);
    if ((*env)->ExceptionCheck(env)) {
        (*env)->ExceptionClear(env);
        fd = -1;
    }
    (*env)->DeleteLocalRef(env, jstringUri);
]=])
        string(FIND "${MIKUPAN_SDL_ANDROID_CONTENT}"
                "${MIKUPAN_SDL_OPEN_FD_JNI_NEW}"
                MIKUPAN_SDL_OPEN_FD_JNI_ALREADY_PATCHED)
        if(MIKUPAN_SDL_OPEN_FD_JNI_ALREADY_PATCHED EQUAL -1)
            string(REPLACE "${MIKUPAN_SDL_OPEN_FD_JNI_OLD}"
                    "${MIKUPAN_SDL_OPEN_FD_JNI_NEW}"
                    MIKUPAN_SDL_ANDROID_CONTENT
                    "${MIKUPAN_SDL_ANDROID_CONTENT}")
            file(WRITE "${MIKUPAN_SDL_ANDROID_C}" "${MIKUPAN_SDL_ANDROID_CONTENT}")
        endif()
    endif()
endif()
