# What is MikuPan?
MikuPan is an in progress port of Fatal Frame 1 PS2 to PC running natively, no emulation. Still very early work.


![logo](resources/mikupan.png)

[![Discord](https://badgen.net/badge/icon/discord?icon=discord&label)](https://discord.gg/Ap4Sfcmwd9)
![Downloads](https://img.shields.io/github/downloads/Mikompilation/MikuPan/total)

# Build
For now, building in `debug` is highly recommended due to issues related to the `release` build. The build process was made to automatically fetch all required dependencies of the project.

## Assets
You need to copy the files `IMG_HD.BIN` and `IMG_BD.BIN` from the NTSC-U (USA) ISO of Fatal Frame to the folder containing the `MikuPan` executable.

PAL (EU), NTSC-J (Japan) ISOs will *NOT* work.
The executable will be located at `{CMake-Build-Directory}/MikuPan`.


![logo](extern/required-files.png)

## Windows
You need either `MinGW` or `Cygwin` with `gcc` and `CMake` in order to build `MikuPan`. `MSVC` will *NOT* work. 

## Linux
`GCC` and `CMake` required to build `MikuPan`.

## Android
Android builds use the NDK CMake toolchain and produce a debug-signed APK with
the `MikuPan-apk` target. Requirements: Android SDK 35 or newer, Android NDK
r28c or newer, Java JDK, Ninja, and a `zip` executable.

Android FFmpeg is consumed as prebuilt shared libraries. The preferred layout is
a merged `libffmpeg.so` for each ABI:

```text
extern/ffmpeg/android/arm64-v8a/include
extern/ffmpeg/android/arm64-v8a/lib/libffmpeg.so
```

The library may also live directly in the ABI folder:

```text
extern/ffmpeg/android/arm64-v8a/include
extern/ffmpeg/android/arm64-v8a/libffmpeg.so
```

The legacy local folder name is also supported:

```text
extern/ffmpeg/include
extern/ffmpeg-android/arm64-v8a/libffmpeg.so
```

Split FFmpeg libraries are also supported:

```text
extern/ffmpeg/android/arm64-v8a/include
extern/ffmpeg/android/arm64-v8a/lib/libavcodec.so
extern/ffmpeg/android/arm64-v8a/lib/libavformat.so
extern/ffmpeg/android/arm64-v8a/lib/libavutil.so
extern/ffmpeg/android/arm64-v8a/lib/libswscale.so
extern/ffmpeg/android/arm64-v8a/lib/libswresample.so
```

The default root is `extern/ffmpeg/android`. Set
`MIKUPAN_ANDROID_FFMPEG_ROOT` if your FFmpeg Android package lives somewhere
else. The root must contain ABI folders such as `arm64-v8a`.

If your headers are not under the ABI folder, set
`MIKUPAN_ANDROID_FFMPEG_INCLUDE_DIR` to the directory that contains
`libavcodec/avcodec.h`.

For a single merged library in a custom location, set
`MIKUPAN_ANDROID_FFMPEG_LIBRARY` directly to `libffmpeg.so`.

All native Android shared libraries must be compatible with 16 KB page-size
devices. MikuPan's Android build passes `-Wl,-z,max-page-size=16384` for
libraries it links locally, including SDL3. Prebuilt FFmpeg `.so` files must be
built with the same linker flag before they are packaged into the APK.

Example arm64 build:

```powershell
$env:ANDROID_HOME="C:\Users\willi\AppData\Local\Android\Sdk"
$env:ANDROID_NDK_HOME="$env:ANDROID_HOME\ndk\27.0.12077973"

$ninja="$env:ANDROID_HOME\cmake\3.22.1\bin\ninja.exe"
$bash="C:\msys64\usr\bin\bash.exe"
$make="C:\msys64\usr\bin\make.exe"
$zip="C:\msys64\usr\bin\zip.exe"

$buildTools="$env:ANDROID_HOME\build-tools\37.0.0"
$jbr="C:\Program Files\Android\Android Studio\jbr\bin"

cmake -S . -B cmake-build-android-arm64 -G Ninja `                
   -DCMAKE_MAKE_PROGRAM="$ninja" `
   -DBASH_EXECUTABLE="$bash" `
   -DMAKE_EXECUTABLE="$make" `
   -DCMAKE_TOOLCHAIN_FILE="$env:ANDROID_NDK_HOME\build\cmake\android.toolchain.cmake" `
   -DSDL_ANDROID_HOME="$env:ANDROID_HOME" `
   -DANDROID_ABI=arm64-v8a `
   -DANDROID_PLATFORM=android-21 `
   -DANDROID_STL=c++_static `
   -DCMAKE_BUILD_TYPE=Release

cmake --build cmake-build-android-arm64 --target MikuPan-apk
```

The APK is written to `cmake-build-android-arm64/MikuPan.apk`. You can also use
`install-MikuPan`, `start-MikuPan`, and `uninstall-MikuPan` targets when `adb` is
available. Set `-DMIKUPAN_ANDROID_APK=OFF` if you only want the Android native
`libmain.so` build.

The packaging automatically bundles the NDK's `libc++_shared.so` into
`lib/<abi>/` whenever it is needed — either because `-DANDROID_STL=c++_shared`
was used, or because a prebuilt library being packaged (such as FFmpeg) was
itself built against the shared C++ runtime. This prevents the app from crashing
on launch with "library libc++_shared.so not found". With `c++_static` and
prebuilt libraries that do not link the shared runtime, it is not bundled.

The APK packages MikuPan's own `resources/` directory. The Fatal Frame data is
not packaged; after installing, copy the required files to the app data folder:

```powershell
adb shell mkdir -p /sdcard/Android/data/io.github.mikompilation.mikupan/files
adb push IMG_HD.BIN /sdcard/Android/data/io.github.mikompilation.mikupan/files/IMG_HD.BIN
adb push IMG_BD.BIN /sdcard/Android/data/io.github.mikompilation.mikupan/files/IMG_BD.BIN
```

On Android 11 and newer, normal file managers usually cannot write to
`/sdcard/Android/data/...`. For a user-managed folder, put the files somewhere
like `Documents/MikuPan`, then use MikuPan's in-game folder picker to grant
access to that folder.

## OSX
Not yet supported due to lack of hardware to test it, but it should be easily supported.

# Special Thanks
Thank you to prinsep for making the logo!
