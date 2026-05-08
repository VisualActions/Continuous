#include "continuous/core/Log.h"

#include <atomic>
#include <chrono>
#include <cstdio>
#include <unordered_map>

#if defined(_WIN32)
    #define WIN32_LEAN_AND_MEAN
    #include <windows.h>
#endif

namespace cn::log {
namespace {

struct State {
    std::mutex                       mu;
    std::unordered_map<u32, Sink>    sinks;
    std::atomic<Level>               minLevel{Level::Trace};
    std::atomic<u32>                 nextId{1};
    std::chrono::steady_clock::time_point t0{std::chrono::steady_clock::now()};
    bool                             inited{false};
};

State& g() {
    static State s;
    return s;
}

const char* level_str(Level l) {
    switch (l) {
        case Level::Trace: return "TRACE";
        case Level::Debug: return "DEBUG";
        case Level::Info:  return "INFO ";
        case Level::Warn:  return "WARN ";
        case Level::Error: return "ERROR";
        case Level::Fatal: return "FATAL";
    }
    return "?????";
}

#if defined(_WIN32)
WORD level_color(Level l) {
    switch (l) {
        case Level::Trace: return FOREGROUND_BLUE | FOREGROUND_GREEN;
        case Level::Debug: return FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_INTENSITY;
        case Level::Info:  return FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE;
        case Level::Warn:  return FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_INTENSITY;
        case Level::Error: return FOREGROUND_RED | FOREGROUND_INTENSITY;
        case Level::Fatal: return FOREGROUND_RED | FOREGROUND_INTENSITY | BACKGROUND_BLUE;
    }
    return FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE;
}
#endif

void stdout_sink(const Record& r) {
#if defined(_WIN32)
    HANDLE h = ::GetStdHandle(STD_OUTPUT_HANDLE);
    CONSOLE_SCREEN_BUFFER_INFO info{};
    bool got = h && ::GetConsoleScreenBufferInfo(h, &info);
    if (h && got) ::SetConsoleTextAttribute(h, level_color(r.level));
    std::fprintf(stdout, "[%7.3f] %s [%s] %s\n", r.time_seconds, level_str(r.level),
                 r.category.c_str(), r.message.c_str());
    if (h && got) ::SetConsoleTextAttribute(h, info.wAttributes);
#else
    std::fprintf(stdout, "[%7.3f] %s [%s] %s\n", r.time_seconds, level_str(r.level),
                 r.category.c_str(), r.message.c_str());
#endif
    std::fflush(stdout);

#if defined(_WIN32)
    if (::IsDebuggerPresent()) {
        char buf[1024];
        std::snprintf(buf, sizeof(buf), "[%s] [%s] %s\n", level_str(r.level), r.category.c_str(), r.message.c_str());
        ::OutputDebugStringA(buf);
    }
#endif
}

} // namespace

void init() {
    auto& s = g();
    std::lock_guard<std::mutex> lk(s.mu);
    if (s.inited) return;
    s.inited = true;
    u32 id = s.nextId.fetch_add(1, std::memory_order_relaxed);
    s.sinks.emplace(id, &stdout_sink);
}

void shutdown() {
    auto& s = g();
    std::lock_guard<std::mutex> lk(s.mu);
    s.sinks.clear();
    s.inited = false;
}

void set_min_level(Level lvl) { g().minLevel.store(lvl, std::memory_order_relaxed); }
Level min_level() { return g().minLevel.load(std::memory_order_relaxed); }

u32 add_sink(Sink sink) {
    auto& s = g();
    std::lock_guard<std::mutex> lk(s.mu);
    u32 id = s.nextId.fetch_add(1, std::memory_order_relaxed);
    s.sinks.emplace(id, std::move(sink));
    return id;
}

void remove_sink(u32 id) {
    auto& s = g();
    std::lock_guard<std::mutex> lk(s.mu);
    s.sinks.erase(id);
}

void emit(Level lvl, std::string_view category, std::string_view text) {
    if (lvl < min_level()) return;
    auto& s = g();
    Record r;
    r.level    = lvl;
    r.category = std::string(category);
    r.message  = std::string(text);
    r.time_seconds = std::chrono::duration<f64>(std::chrono::steady_clock::now() - s.t0).count();

    std::lock_guard<std::mutex> lk(s.mu);
    for (auto& [id, sink] : s.sinks) {
        sink(r);
    }
}

} // namespace cn::log
