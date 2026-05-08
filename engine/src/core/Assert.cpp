#include "continuous/core/Assert.h"

#include <cstdlib>

namespace cn::detail {

void report_assert(const char* file, int line, const char* expr, const std::string& msg) {
    ::cn::log::emit(::cn::log::Level::Fatal, "assert",
                    fmt::format("{}:{}  ({})  {}", file, line, expr, msg));
}

[[noreturn]] void abort_now() {
    std::abort();
}

} // namespace cn::detail
