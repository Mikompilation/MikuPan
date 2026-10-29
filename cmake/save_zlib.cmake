# CBS save archives use a private static zlib dependency.
include(FetchContent)
set(ZLIB_BUILD_SHARED OFF CACHE BOOL "Disable shared zlib." FORCE)
set(ZLIB_BUILD_STATIC ON CACHE BOOL "Build static zlib for save archives." FORCE)
set(ZLIB_BUILD_TESTING OFF CACHE BOOL "Disable zlib examples/tests." FORCE)
set(ZLIB_INSTALL OFF CACHE BOOL "Do not install the private zlib dependency." FORCE)
FetchContent_Declare(save_zlib
    URL https://zlib.net/fossils/zlib-1.3.2.tar.gz
    URL_HASH SHA256=bb329a0a2cd0274d05519d61c667c062e06990d72e125ee2dfa8de64f0119d16
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
FetchContent_MakeAvailable(save_zlib)
