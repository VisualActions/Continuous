// Continuous Engine - asserts. Always-on (CN_VERIFY), debug-only (CN_ASSERT).
#pragma once

#include "continuous/core/Log.h"
#include "continuous/core/Macros.h"

#include <fmt/format.h>

namespace cn::detail {
[[noreturn]] CN_API void abort_now();
CN_API void report_assert(const char* file, int line, const char* expr, const std::string& msg);
} // namespace cn::detail

#define CN_VERIFY(expr, ...)                                                              \
    do {                                                                                  \
        if (!(expr)) {                                                                    \
            ::cn::detail::report_assert(__FILE__, __LINE__, #expr, fmt::format(__VA_ARGS__)); \
            CN_DEBUG_BREAK();                                                             \
            ::cn::detail::abort_now();                                                    \
        }                                                                                 \
    } while (false)

#if defined(NDEBUG) && !defined(CN_FORCE_ASSERTS)
    #define CN_ASSERT(expr, ...) ((void)0)
#else
    #define CN_ASSERT(expr, ...) CN_VERIFY(expr, __VA_ARGS__)
#endif

#define CN_UNREACHABLE() CN_VERIFY(false, "unreachable")
