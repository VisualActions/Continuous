#include "Console.h"

#include <imgui.h>

namespace cnedit {

ConsolePanel::ConsolePanel() {
    sink_id_ = cn::log::add_sink([this](const cn::log::Record& r) {
        std::lock_guard<std::mutex> lk(mu_);
        records_.push_back(r);
        if (records_.size() > 4096) records_.pop_front();
    });
}

ConsolePanel::~ConsolePanel() {
    if (sink_id_) cn::log::remove_sink(sink_id_);
}

static ImVec4 color_for(cn::log::Level l) {
    switch (l) {
        case cn::log::Level::Trace: return {0.6f, 0.6f, 0.6f, 1};
        case cn::log::Level::Debug: return {0.6f, 0.7f, 1.0f, 1};
        case cn::log::Level::Info:  return {1.0f, 1.0f, 1.0f, 1};
        case cn::log::Level::Warn:  return {1.0f, 0.8f, 0.3f, 1};
        case cn::log::Level::Error: return {1.0f, 0.4f, 0.4f, 1};
        case cn::log::Level::Fatal: return {1.0f, 0.2f, 0.2f, 1};
    }
    return {1, 1, 1, 1};
}

void ConsolePanel::draw() {
    if (ImGui::Begin("Console")) {
        if (ImGui::Button("Clear")) {
            std::lock_guard<std::mutex> lk(mu_);
            records_.clear();
        }
        ImGui::SameLine();
        ImGui::Checkbox("Auto-scroll", &auto_scroll_);
        ImGui::Separator();
        if (ImGui::BeginChild("log", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar)) {
            std::lock_guard<std::mutex> lk(mu_);
            for (auto& r : records_) {
                ImGui::PushStyleColor(ImGuiCol_Text, color_for(r.level));
                ImGui::TextUnformatted("");
                ImGui::SameLine();
                ImGui::Text("[%7.3f] [%s] %s",
                            r.time_seconds, r.category.c_str(), r.message.c_str());
                ImGui::PopStyleColor();
            }
            if (auto_scroll_ && ImGui::GetScrollY() >= ImGui::GetScrollMaxY())
                ImGui::SetScrollHereY(1.0f);
        }
        ImGui::EndChild();
    }
    ImGui::End();
}

} // namespace cnedit
