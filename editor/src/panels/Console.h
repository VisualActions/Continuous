#pragma once

#include "continuous/core/Log.h"

#include <deque>
#include <mutex>
#include <vector>

namespace cnedit {

class ConsolePanel {
public:
    ConsolePanel();
    ~ConsolePanel();
    void draw();

private:
    std::mutex                    mu_;
    std::deque<cn::log::Record>   records_;
    cn::u32                       sink_id_ = 0;
    bool                          auto_scroll_ = true;
};

} // namespace cnedit
