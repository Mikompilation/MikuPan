#include "mikupan_logging.h"

#include "spdlog/cfg/env.h"
#include "spdlog/sinks/stdout_color_sinks.h"
#include "spdlog/spdlog.h"

#ifdef __ANDROID__
#include "spdlog/sinks/android_sink.h"
#endif

#include <array>
#include <algorithm>
#include <cstdarg>
#include <cstdio>
#include <memory>
#include <mutex>
#include <vector>

namespace
{

constexpr const char* kLoggerName = "MikuPan";
constexpr const char* kPattern = "[%H:%M:%S.%e] [%^%l%$] %v";

std::mutex g_logger_mutex;
std::shared_ptr<spdlog::logger> g_logger;

spdlog::level::level_enum ToSpdlogLevel(MikuPan_LogLevel level)
{
    switch (level)
    {
    case MIKUPAN_LOG_TRACE:
        return spdlog::level::trace;
    case MIKUPAN_LOG_DEBUG:
        return spdlog::level::debug;
    case MIKUPAN_LOG_INFO:
        return spdlog::level::info;
    case MIKUPAN_LOG_WARN:
        return spdlog::level::warn;
    case MIKUPAN_LOG_ERROR:
        return spdlog::level::err;
    case MIKUPAN_LOG_CRITICAL:
        return spdlog::level::critical;
    case MIKUPAN_LOG_OFF:
        return spdlog::level::off;
    default:
        return spdlog::level::info;
    }
}

MikuPan_LogLevel FromSpdlogLevel(spdlog::level::level_enum level)
{
    switch (level)
    {
    case spdlog::level::trace:
        return MIKUPAN_LOG_TRACE;
    case spdlog::level::debug:
        return MIKUPAN_LOG_DEBUG;
    case spdlog::level::info:
        return MIKUPAN_LOG_INFO;
    case spdlog::level::warn:
        return MIKUPAN_LOG_WARN;
    case spdlog::level::err:
        return MIKUPAN_LOG_ERROR;
    case spdlog::level::critical:
        return MIKUPAN_LOG_CRITICAL;
    case spdlog::level::off:
        return MIKUPAN_LOG_OFF;
    default:
        return MIKUPAN_LOG_INFO;
    }
}

std::shared_ptr<spdlog::logger> CreateLogger()
{
    std::vector<spdlog::sink_ptr> sinks;

#ifdef __ANDROID__
    sinks.emplace_back(std::make_shared<spdlog::sinks::android_sink_mt>(
        kLoggerName));
#else
    sinks.emplace_back(std::make_shared<spdlog::sinks::stdout_color_sink_mt>());
#endif

    auto logger = std::make_shared<spdlog::logger>(
        kLoggerName, sinks.begin(), sinks.end());
    logger->set_pattern(kPattern);
    logger->set_level(spdlog::level::info);
    logger->flush_on(spdlog::level::warn);
    return logger;
}

void LogWithArgs(MikuPan_LogLevel level, const char* fmt, va_list args)
{
    auto logger = MikuPan::Logging::GetLogger();
    const spdlog::level::level_enum spdlog_level = ToSpdlogLevel(level);
    if (!logger->should_log(spdlog_level))
    {
        return;
    }

    if (fmt == nullptr)
    {
        static constexpr const char null_format[] = "(null log format)";
        logger->log(spdlog_level, spdlog::string_view_t(
            null_format, sizeof(null_format) - 1));
        return;
    }

    std::array<char, 2048> stack_buffer{};
    va_list args_copy;
    va_copy(args_copy, args);
    const int written = std::vsnprintf(
        stack_buffer.data(), stack_buffer.size(), fmt, args_copy);
    va_end(args_copy);

    if (written < 0)
    {
        logger->log(spdlog::level::err,
                    "Failed to format log message: {}", fmt);
        return;
    }

    if (static_cast<size_t>(written) < stack_buffer.size())
    {
        logger->log(spdlog_level, spdlog::string_view_t(
            stack_buffer.data(), static_cast<size_t>(written)));
        return;
    }

    std::vector<char> buffer(static_cast<size_t>(written) + 1);
    va_copy(args_copy, args);
    const int resized_written = std::vsnprintf(
        buffer.data(), buffer.size(), fmt, args_copy);
    va_end(args_copy);

    if (resized_written < 0)
    {
        logger->log(spdlog::level::err,
                    "Failed to format log message: {}", fmt);
        return;
    }

    const size_t message_length = std::min(
        static_cast<size_t>(resized_written), buffer.size() - 1);
    logger->log(spdlog_level,
                spdlog::string_view_t(buffer.data(), message_length));
}

void LogVariadic(MikuPan_LogLevel level, const char* fmt, va_list args)
{
    LogWithArgs(level, fmt, args);
}

}

namespace MikuPan::Logging
{

std::shared_ptr<spdlog::logger> GetLogger()
{
    std::lock_guard<std::mutex> lock(g_logger_mutex);
    if (!g_logger)
    {
        g_logger = CreateLogger();
        spdlog::set_default_logger(g_logger);
        spdlog::cfg::load_env_levels();
        spdlog::apply_logger_env_levels(g_logger);
    }

    return g_logger;
}

spdlog::logger& Logger()
{
    return *GetLogger();
}

}

void MikuPan_InitLogging(void)
{
    (void)MikuPan::Logging::GetLogger();
}

void MikuPan_ShutdownLogging(void)
{
    std::shared_ptr<spdlog::logger> logger;
    {
        std::lock_guard<std::mutex> lock(g_logger_mutex);
        logger = std::move(g_logger);
    }

    if (logger)
    {
        logger->flush();
    }

    spdlog::drop(kLoggerName);
}

void MikuPan_SetLogLevel(MikuPan_LogLevel level)
{
    MikuPan::Logging::GetLogger()->set_level(ToSpdlogLevel(level));
}

MikuPan_LogLevel MikuPan_GetLogLevel(void)
{
    return FromSpdlogLevel(MikuPan::Logging::GetLogger()->level());
}

void MikuPan_FlushLog(void)
{
    MikuPan::Logging::GetLogger()->flush();
}

void MikuPan_LogV(MikuPan_LogLevel level, const char* fmt, va_list args)
{
    LogWithArgs(level, fmt, args);
}

void MikuPan_Log(MikuPan_LogLevel level, const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    LogVariadic(level, fmt, args);
    va_end(args);
}

void trace_log(const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    LogVariadic(MIKUPAN_LOG_TRACE, fmt, args);
    va_end(args);
}

void debug_log(const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    LogVariadic(MIKUPAN_LOG_DEBUG, fmt, args);
    va_end(args);
}

void info_log(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    LogVariadic(MIKUPAN_LOG_INFO, fmt, args);
    va_end(args);
}

void warn_log(const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    LogVariadic(MIKUPAN_LOG_WARN, fmt, args);
    va_end(args);
}

void error_log(const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    LogVariadic(MIKUPAN_LOG_ERROR, fmt, args);
    va_end(args);
}

void err_log(const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    LogVariadic(MIKUPAN_LOG_ERROR, fmt, args);
    va_end(args);
}

void critical_log(const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    LogVariadic(MIKUPAN_LOG_CRITICAL, fmt, args);
    va_end(args);
}
