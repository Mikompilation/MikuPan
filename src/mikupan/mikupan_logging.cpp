#include "mikupan_logging.h"

#include "spdlog/spdlog.h"

#include <cstdarg>
#include <cstdio>
#include <vector>

#ifdef __ANDROID__
#include <android/log.h>
#endif

void info_log(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    va_list args_copy;
    va_copy(args_copy, args);

    char buffer[2048];
    int n = vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);
    if (n < 0)
    {
        va_end(args_copy);
        return;
    }

    if (n >= (int)sizeof(buffer))
    {
        std::vector<char> bigbuf(n + 1);
        vsnprintf(bigbuf.data(), bigbuf.size(), fmt, args_copy);
        spdlog::info("{}", bigbuf.data());
#ifdef __ANDROID__
        __android_log_print(ANDROID_LOG_INFO, "MikuPan", "%s", bigbuf.data());
#endif
    }
    else
    {
        spdlog::info("{}", buffer);
#ifdef __ANDROID__
        __android_log_print(ANDROID_LOG_INFO, "MikuPan", "%s", buffer);
#endif
    }

    va_end(args_copy);
}
