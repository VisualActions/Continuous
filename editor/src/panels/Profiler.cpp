#include "Profiler.h"

#include <imgui.h>

namespace cnedit {

void ProfilerPanel::draw(cn::Engine& eng) {
    dt_history_[head_] = (float)eng.timer().dt() * 1000.0f;
    head_ = (head_ + 1) % kHistory;

    if (ImGui::Begin("Profiler")) {
        ImGui::Text("FPS: %.1f", eng.timer().fps());
        ImGui::Text("dt:  %.2f ms (smoothed %.2f ms)",
                    eng.timer().dt() * 1000.0,
                    eng.timer().smoothed_dt() * 1000.0);
        ImGui::Separator();
        ImGui::Text("Renderer:");
        ImGui::BulletText("Visible items: %u",  eng.renderer().stat_visible_items());
        ImGui::BulletText("Draw calls:    %u",  eng.renderer().stat_draw_calls());
        ImGui::Separator();
        ImGui::Text("Network:");
        ImGui::BulletText("Mode: %s",
            eng.net().mode() == cn::net::Mode::Server ? "Server" :
            eng.net().mode() == cn::net::Mode::Client ? "Client" : "Offline");
        ImGui::BulletText("Bytes in: %llu  out: %llu",
            (unsigned long long)eng.net().bytes_in(),
            (unsigned long long)eng.net().bytes_out());
        ImGui::Separator();
        // Plot dt history.
        float values[kHistory];
        for (cn::u32 i = 0; i < kHistory; ++i) values[i] = dt_history_[(head_ + i) % kHistory];
        ImGui::PlotLines("dt(ms)", values, kHistory, 0, nullptr, 0.0f, 33.3f, ImVec2(0, 80));
    }
    ImGui::End();
}

} // namespace cnedit
