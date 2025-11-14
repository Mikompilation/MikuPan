#include "logging.h"

#include "spdlog/spdlog.h"

#include <cstdarg>

void info_log(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    spdlog::info(fmt, args);
    va_end(args);
}