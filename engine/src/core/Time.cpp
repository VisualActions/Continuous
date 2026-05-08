#include "continuous/core/Time.h"

#if defined(_WIN32)
    #define WIN32_LEAN_AND_MEAN
    #include <windows.h>
#else
    #include <chrono>
#endif

namespace cn {

#if defined(_WIN32)
static u64 qpc_freq() {
    LARGE_INTEGER f; QueryPerformanceFrequency(&f); return static_cast<u64>(f.QuadPart);
}
static u64 qpc_now() {
    LARGE_INTEGER c; QueryPerformanceCounter(&c); return static_cast<u64>(c.QuadPart);
}
u64 Clock::frequency() { return qpc_freq(); }

Clock::Clock() : start_(qpc_now()) {}
void Clock::reset() { start_ = qpc_now(); }
u64  Clock::ticks() const noexcept { return qpc_now() - start_; }
f64  Clock::seconds() const noexcept {
    static const f64 inv_f = 1.0 / static_cast<f64>(qpc_freq());
    return static_cast<f64>(ticks()) * inv_f;
}
#else
u64 Clock::frequency() {
    return std::chrono::high_resolution_clock::period::den / std::chrono::high_resolution_clock::period::num;
}
Clock::Clock() : start_(0) { reset(); }
void Clock::reset() {
    auto t = std::chrono::high_resolution_clock::now().time_since_epoch();
    start_ = static_cast<u64>(t.count());
}
u64 Clock::ticks() const noexcept {
    auto t = std::chrono::high_resolution_clock::now().time_since_epoch();
    return static_cast<u64>(t.count()) - start_;
}
f64 Clock::seconds() const noexcept {
    return static_cast<f64>(ticks()) *
           (static_cast<f64>(std::chrono::high_resolution_clock::period::num) /
            static_cast<f64>(std::chrono::high_resolution_clock::period::den));
}
#endif

FrameTimer::FrameTimer() {
    last_ = clock_.seconds();
}

f64 FrameTimer::tick() {
    f64 now = clock_.seconds();
    dt_     = now - last_;
    last_   = now;
    if (dt_ > 0.25) dt_ = 0.25; // clamp huge stalls (debug breakpoints, alt-tab)
    if (dt_ < 0.0)  dt_ = 0.0;

    if (frame_ == 0) {
        smoothed_dt_ = dt_;
    } else {
        const f64 a = 0.1;
        smoothed_dt_ = smoothed_dt_ * (1.0 - a) + dt_ * a;
    }
    elapsed_ += dt_;
    ++frame_;
    return dt_;
}

} // namespace cn
