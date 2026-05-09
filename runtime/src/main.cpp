// Continuous runtime entry. Loads ./scene.json, the gameplay dll
// ./sandbox_gameplay.dll, and runs the engine in play mode.

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "continuous/Engine.h"
#include "continuous/core/IO.h"
#include "continuous/core/Log.h"
#include "continuous/scene/Components.h"
#include "continuous/ui/UI.h"

int APIENTRY wWinMain(HINSTANCE, HINSTANCE, PWSTR, int) {
    cn::EngineConfig cfg;
    cfg.window.title = "Continuous";
    cfg.window.width = 1600;
    cfg.window.height = 900;
    cfg.start_play_mode = true;
    cfg.gameplay_dll = cn::io::executable_dir() / "sandbox_gameplay.dll";

    cn::Engine eng;
    if (!eng.init(cfg)) return 1;

    auto scene_path = cn::io::executable_dir() / "scene.json";
    if (std::filesystem::exists(scene_path)) {
        eng.scene().load_json(scene_path);
    } else {
        // Try cooked path under assets/cooked/.
        auto cooked_scene = cn::io::executable_dir() / "assets" / "cooked" / "sandbox.scene";
        if (std::filesystem::exists(cooked_scene)) eng.scene().load_json(cooked_scene);
    }

    eng.set_imgui_hook([&](float /*dt*/) {
        // Draw an in-game HUD via the engine's ui::Context.
        auto& hud = eng.hud();
        char fps_buf[64];
        std::snprintf(fps_buf, sizeof(fps_buf), "FPS %.0f", eng.timer().fps());
        hud.text({16, 16}, fps_buf, {1, 1, 1, 1}, 1.5f);

        hud.begin_panel({16, 56}, {280, 200}, "Continuous");
        hud.layout_label("WASD : move");
        hud.layout_label("Space: jump");
        hud.layout_label("Mouse: look");
        hud.layout_label("Esc  : release mouse");
        hud.layout_separator();
        char buf[128];
        std::snprintf(buf, sizeof(buf), "Entities: %u",
                      eng.scene().world().entity_count());
        hud.layout_label(buf);
        std::snprintf(buf, sizeof(buf), "Draws: %u   Visible: %u",
                      eng.renderer().stat_draw_calls(),
                      eng.renderer().stat_visible_items());
        hud.layout_label(buf);
        std::snprintf(buf, sizeof(buf), "Net: %s",
                      eng.net().mode() == cn::net::Mode::Server ? "server" :
                      eng.net().mode() == cn::net::Mode::Client ? "client" : "offline");
        hud.layout_label(buf);
        hud.end_panel();

        // Crosshair.
        cn::math::vec2 c{(cn::f32)eng.swapchain().width() / 2.0f,
                         (cn::f32)eng.swapchain().height() / 2.0f};
        hud.rect({c.x - 2, c.y - 2}, {4, 4}, {1, 1, 1, 0.7f});
    });

    return eng.run();
}
