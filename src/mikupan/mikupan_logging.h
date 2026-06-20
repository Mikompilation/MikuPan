#ifndef MIKUPAN_LOGGING_H
#define MIKUPAN_LOGGING_H

#include <stdarg.h>

#if defined(__GNUC__) || defined(__clang__)
#define MIKUPAN_PRINTF_FORMAT(format_index, first_arg) \
    __attribute__((format(printf, format_index, first_arg)))
#else
#define MIKUPAN_PRINTF_FORMAT(format_index, first_arg)
#endif

#ifdef __cplusplus
extern "C"
{
#endif

typedef enum MikuPan_LogLevel
{
    MIKUPAN_LOG_TRACE = 0,
    MIKUPAN_LOG_DEBUG,
    MIKUPAN_LOG_INFO,
    MIKUPAN_LOG_WARN,
    MIKUPAN_LOG_ERROR,
    MIKUPAN_LOG_CRITICAL,
    MIKUPAN_LOG_OFF
} MikuPan_LogLevel;

void MikuPan_InitLogging(void);
void MikuPan_ShutdownLogging(void);
void MikuPan_SetLogLevel(MikuPan_LogLevel level);
MikuPan_LogLevel MikuPan_GetLogLevel(void);
void MikuPan_FlushLog(void);

void MikuPan_LogV(MikuPan_LogLevel level, const char* fmt, va_list args)
    MIKUPAN_PRINTF_FORMAT(2, 0);
void MikuPan_Log(MikuPan_LogLevel level, const char* fmt, ...)
    MIKUPAN_PRINTF_FORMAT(2, 3);

void trace_log(const char* fmt, ...) MIKUPAN_PRINTF_FORMAT(1, 2);
void debug_log(const char* fmt, ...) MIKUPAN_PRINTF_FORMAT(1, 2);
void info_log(const char* fmt, ...) MIKUPAN_PRINTF_FORMAT(1, 2);
void warn_log(const char* fmt, ...) MIKUPAN_PRINTF_FORMAT(1, 2);
void error_log(const char* fmt, ...) MIKUPAN_PRINTF_FORMAT(1, 2);
void err_log(const char* fmt, ...) MIKUPAN_PRINTF_FORMAT(1, 2);
void critical_log(const char* fmt, ...) MIKUPAN_PRINTF_FORMAT(1, 2);

#ifdef __cplusplus
}
#endif

#ifdef __cplusplus

#include <memory>
#include <spdlog/spdlog.h>

namespace MikuPan::Logging
{

std::shared_ptr<spdlog::logger> GetLogger();
spdlog::logger& Logger();

}

#endif

#endif //MIKUPAN_LOGGING_H
