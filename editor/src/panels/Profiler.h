#pragma once

#include "continuous/Engine.h"

namespace cnedit {

class ProfilerPanel {
public:
    void draw(cn::Engine& eng);
private:
    static constexpr cn::u32 kHistory = 256;
    float dt_history_[kHistory] = {};
    cn::u32 head_ = 0;
};

} // namespace cnedit
