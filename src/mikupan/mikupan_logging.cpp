#include "mikupan_logging.h"

#include "spdlog/spdlog.h"

#include <cstdarg>

void info_log(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);

    char buffer[2048];
    int n = vsnprintf(buffer, sizeof(buffer), fmt, args);
    if (n < 0) return;

    // If message too long, dynamically allocate a buffer
    if (n >= (int)sizeof(buffer)) {
        std::vector<char> bigbuf(n + 1);
        vsnprintf(bigbuf.data(), bigbuf.size(), fmt, args);
        spdlog::info(bigbuf.data());
    } else {
        spdlog::info(buffer);
    }

    va_end(args);
}