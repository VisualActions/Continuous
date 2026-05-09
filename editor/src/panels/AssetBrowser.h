#pragma once

#include "continuous/Engine.h"

namespace cnedit {

class AssetBrowserPanel {
public:
    void draw(cn::Engine& eng);
private:
    std::filesystem::path current_;
};

} // namespace cnedit
