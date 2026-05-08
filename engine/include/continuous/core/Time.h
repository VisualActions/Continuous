// Continuous Engine - high resolution clock & frame timing.
#pragma once

#include "continuous/core/Macros.h"
#include "continuous/core/Types.h"

namespace cn {

class CN_API Clock {
public:
    Clock();
    void  reset();
    f64   seconds() const noexcept; // since reset
    u64   ticks()   const noexcept; // since reset, in QueryPerformanceCounter ticks
    static u64 frequency();
private:
    u64 start_ = 0;
};

class CN_API FrameTimer {
public:
    FrameTimer();
    // Call once per frame. Returns delta seconds (clamped to a reasonable max).
    f64 tick();

    f64 dt()              const noexcept { return dt_; }
    f64 smoothed_dt()     const noexcept { return smoothed_dt_; }
    f64 fps()             const noexcept { return smoothed_dt_ > 0 ? 1.0 / smoothed_dt_ : 0.0; }
    f64 elapsed_seconds() const noexcept { return elapsed_; }
    u64 frame_index()     const noexcept { return frame_; }

private:
    Clock clock_;
    f64   last_       = 0.0;
    f64   dt_         = 0.0;
    f64   smoothed_dt_= 0.0;
    f64   elapsed_    = 0.0;
    u64   frame_      = 0;
};

} // namespace cn
