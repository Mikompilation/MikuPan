#ifndef MIKUPAN_ASSERT_H
#define MIKUPAN_ASSERT_H

#include "mikupan/debug/mikupan_logging.h"

#if defined(_MSC_VER)
#include <intrin.h>
#define MIKUPAN_DEBUG_BREAK() __debugbreak()
#elif defined(__clang__) || defined(__GNUC__)
#define MIKUPAN_DEBUG_BREAK() __builtin_trap()
#else
#include <signal.h>
#define MIKUPAN_DEBUG_BREAK() raise(SIGTRAP)
#endif

#ifndef MIKUPAN_ENABLE_ASSERTS
#if defined(NDEBUG)
#define MIKUPAN_ENABLE_ASSERTS 0
#else
#define MIKUPAN_ENABLE_ASSERTS 1
#endif
#endif

#if MIKUPAN_ENABLE_ASSERTS
#define MIKUPAN_ASSERT_BREAK() MIKUPAN_DEBUG_BREAK()
#else
#define MIKUPAN_ASSERT_BREAK() ((void)0)
#endif

#define MIKUPAN_ASSERT(expr)                                                   \
    do                                                                         \
    {                                                                          \
        if (!(expr))                                                           \
        {                                                                      \
            critical_log("MIKUPAN_ASSERT failed: %s at %s:%d (%s)",            \
                         #expr, __FILE__, __LINE__, __func__);                 \
            MikuPan_FlushLog();                                                \
            MIKUPAN_ASSERT_BREAK();                                            \
        }                                                                      \
    } while (0)

#define MIKUPAN_ASSERT_MSG(expr, ...)                                          \
    do                                                                         \
    {                                                                          \
        if (!(expr))                                                           \
        {                                                                      \
            critical_log("MIKUPAN_ASSERT failed: %s at %s:%d (%s)",            \
                         #expr, __FILE__, __LINE__, __func__);                 \
            critical_log(__VA_ARGS__);                                         \
            MikuPan_FlushLog();                                                \
            MIKUPAN_ASSERT_BREAK();                                            \
        }                                                                      \
    } while (0)

#endif // MIKUPAN_ASSERT_H
