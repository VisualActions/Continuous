#pragma once

#include "continuous/Engine.h"
#include "panels/AssetBrowser.h"
#include "panels/Console.h"
#include "panels/Hierarchy.h"
#include "panels/Profiler.h"
#include "panels/Viewport.h"

namespace cnedit {

class EditorApp {
public:
    int run(int argc, char** argv);

private:
    void draw_menu_bar();
    void draw_inspector_panel();
    void draw_dockspace();

    cn::Engine               eng_;
    cn::ecs::Entity          selected_;
    HierarchyPanel           hierarchy_;
    ViewportPanel            viewport_;
    ConsolePanel             console_;
    AssetBrowserPanel        assets_;
    ProfilerPanel            profiler_;

    std::filesystem::path    last_scene_path_;
};

} // namespace cnedit
