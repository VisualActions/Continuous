#include "AssetBrowser.h"
#include "continuous/asset/AssetManager.h"

#include <imgui.h>

namespace fs = std::filesystem;

namespace cnedit {

void AssetBrowserPanel::draw(cn::Engine& eng) {
    if (ImGui::Begin("Assets")) {
        if (current_.empty()) current_ = cn::asset::Manager::get().raw_root();
        ImGui::TextUnformatted(current_.string().c_str());
        ImGui::Separator();
        if (current_.has_parent_path() && current_ != cn::asset::Manager::get().raw_root().parent_path()) {
            if (ImGui::Selectable("..", false, ImGuiSelectableFlags_None)) {
                current_ = current_.parent_path();
            }
        }
        std::error_code ec;
        if (fs::exists(current_)) {
            for (auto& it : fs::directory_iterator(current_, ec)) {
                std::string name = it.path().filename().string();
                if (it.is_directory()) {
                    if (ImGui::Selectable(("[D] " + name).c_str(), false, ImGuiSelectableFlags_None,
                                          ImVec2(0, 0))) {
                        current_ = it.path();
                    }
                } else {
                    bool sel = false;
                    ImGui::Selectable(name.c_str(), &sel);
                }
            }
        }
        ImGui::Separator();
        if (ImGui::Button("Cook all to cooked/")) {
            // Walk and cook.
            auto& mgr = cn::asset::Manager::get();
            int n = 0;
            for (auto& it : fs::recursive_directory_iterator(mgr.raw_root(), ec)) {
                if (!it.is_regular_file()) continue;
                auto rel = fs::relative(it.path(), mgr.raw_root());
                fs::path dst = mgr.cooked_root() / rel;
                auto e = it.path().extension();
                if (e == ".gltf" || e == ".glb" || e == ".obj" || e == ".fbx") {
                    dst.replace_extension(".cmesh"); mgr.cook_mesh(it.path(), dst); ++n;
                } else if (e == ".png" || e == ".jpg" || e == ".jpeg" || e == ".tga") {
                    dst.replace_extension(".ctex"); mgr.cook_texture(it.path(), dst); ++n;
                } else if (e == ".wav" || e == ".ogg" || e == ".mp3" || e == ".flac") {
                    dst.replace_extension(".caud"); mgr.cook_audio(it.path(), dst); ++n;
                }
            }
            CN_INFO("editor", "asset browser cooked {} files", n);
        }
    }
    ImGui::End();
    (void)eng;
}

} // namespace cnedit
