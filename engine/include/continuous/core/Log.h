// Continuous Engine - logging facade.
//
// Why a facade rather than direct spdlog: we want the editor's in-process
// console panel to consume the same log stream as stdout, and we want to be
// able to swap or silence the backend in headless tools (cooker, packager).
#pragma once

#include "continuous/core/Macros.h"
#include "continuous/core/Types.h"

#include <fmt/format.h>

#include <functional>
#include <mutex>
#include <string>
#include <string_view>
#include <vector>

namespace cn::log {

enum class Level : u8 { Trace, Debug, Info, Warn, Error, Fatal };

struct Record {
    Level       level{Level::Info};
    std::string category;
    std::string message;
    f64         time_seconds{0.0};
};

using Sink = std::function<void(const Record&)>;

CN_API void init();
CN_API void shutdown();

CN_API void set_min_level(Level lvl);
CN_API Level min_level();

// Sinks may be added/removed from any thread; the logger serializes calls.
CN_API u32  add_sink(Sink sink);
CN_API void remove_sink(u32 id);

CN_API void emit(Level lvl, std::string_view category, std::string_view text);

template <typename... Args>
inline void log(Level lvl, std::string_view category, fmt::format_string<Args...> fmtstr, Args&&... args) {
    if (lvl < min_level()) return;
    emit(lvl, category, fmt::format(fmtstr, std::forward<Args>(args)...));
}

} // namespace cn::log

// Convenience macros - category = current source TU as string literal.
#define CN_TRACE(cat, ...) ::cn::log::log(::cn::log::Level::Trace, cat, __VA_ARGS__)
#define CN_DEBUG(cat, ...) ::cn::log::log(::cn::log::Level::Debug, cat, __VA_ARGS__)
#define CN_INFO(cat, ...)  ::cn::log::log(::cn::log::Level::Info,  cat, __VA_ARGS__)
#define CN_WARN(cat, ...)  ::cn::log::log(::cn::log::Level::Warn,  cat, __VA_ARGS__)
#define CN_ERROR(cat, ...) ::cn::log::log(::cn::log::Level::Error, cat, __VA_ARGS__)
#define CN_FATAL(cat, ...) ::cn::log::log(::cn::log::Level::Fatal, cat, __VA_ARGS__)
