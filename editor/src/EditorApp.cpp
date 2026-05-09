#include "EditorApp.h"
#include "ImGuiBackend.h"
#include "Inspector.h"

#include "continuous/asset/AssetManager.h"
#include "continuous/core/IO.h"
#include "continuous/scene/Components.h"

#include <imgui.h>
#include <ImGuizmo.h>
#include <SDL3/SDL.h>

#include <filesystem>

namespace cnedit {

void EditorApp::draw_menu_bar() {
    if (ImGui::BeginMainMenuBar()) {
        if (ImGui::BeginMenu("Scene")) {
            if (ImGui::MenuItem("New")) { eng_.scene().clear(); selected_ = {}; }
            if (ImGui::MenuItem("Open ...")) {
                if (!last_scene_path_.empty()) eng_.scene().load_json(last_scene_path_);
                else eng_.scene().load_json("scene.json");
                last_scene_path_ = "scene.json";
            }
            if (ImGui::MenuItem("Save")) {
                if (last_scene_path_.empty()) last_scene_path_ = "scene.json";
                eng_.scene().save_json(last_scene_path_);
            }
            if (ImGui::MenuItem("Save As..."))
                eng_.scene().save_json("scene.json"), last_scene_path_ = "scene.json";
            ImGui::Separator();
            if (ImGui::MenuItem("Quit")) eng_.window().set_should_close(true);
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Play")) {
            bool pm = eng_.play_mode();
            if (ImGui::MenuItem(pm ? "Stop" : "Play")) eng_.set_play_mode(!pm);
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Build")) {
            if (ImGui::MenuItem("Cook Assets...")) {
                int n = 0;
                auto& mgr = cn::asset::Manager::get();
                std::error_code ec;
                for (auto& it : std::filesystem::recursive_directory_iterator(mgr.raw_root(), ec)) {
                    if (!it.is_regular_file()) continue;
                    auto rel = std::filesystem::relative(it.path(), mgr.raw_root());
                    auto dst = mgr.cooked_root() / rel;
                    auto e = it.path().extension();
                    if (e == ".gltf" || e == ".glb" || e == ".obj" || e == ".fbx") {
                        dst.replace_extension(".cmesh"); mgr.cook_mesh(it.path(), dst); ++n;
                    } else if (e == ".png" || e == ".jpg" || e == ".jpeg" || e == ".tga") {
                        dst.replace_extension(".ctex"); mgr.cook_texture(it.path(), dst); ++n;
                    } else if (e == ".wav" || e == ".ogg" || e == ".mp3" || e == ".flac") {
                        dst.replace_extension(".caud"); mgr.cook_audio(it.path(), dst); ++n;
                    }
                }
                CN_INFO("editor", "cooked {} files", n);
            }
            if (ImGui::MenuItem("Package Game...")) {
                // delegate to a tool side-by-side - we just save scene + write a marker file.
                eng_.scene().save_json("scene.json");
                CN_INFO("editor", "Use scripts/package.bat to build a redistributable folder.");
            }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Help")) {
            if (ImGui::MenuItem("About")) {
                ImGui::OpenPopup("About Continuous");
            }
            ImGui::EndMenu();
        }
        ImGui::EndMainMenuBar();
    }
}

void EditorApp::draw_inspector_panel() {
    if (ImGui::Begin("Inspector")) {
        if (!eng_.scene().world().alive(selected_)) {
            ImGui::TextDisabled("(nothing selected)");
        } else {
            auto& w = eng_.scene().world();
            // Drive inspector field-by-field per known component type.
            auto inspect = [&](auto* c, const char* key) {
                if (!c) return;
                ImGui::PushID(key);
                if (ImGui::CollapsingHeader(key, ImGuiTreeNodeFlags_DefaultOpen)) {
                    auto* ti = cn::reflect::type_of<std::remove_pointer_t<decltype(c)>>();
                    if (ti) cnedit::draw_inspector(*ti, c);
                }
                ImGui::PopID();
            };

            inspect(w.get<cn::scene::NameComponent>(selected_),                "Name");
            inspect(w.get<cn::scene::TransformComponent>(selected_),           "Transform");
            inspect(w.get<cn::scene::CameraComponent>(selected_),              "Camera");
            inspect(w.get<cn::scene::LightComponent>(selected_),               "Light");
            inspect(w.get<cn::scene::MeshRenderer>(selected_),                 "Mesh Renderer");
            inspect(w.get<cn::scene::RigidBodyComponent>(selected_),           "Rigid Body");
            inspect(w.get<cn::scene::CharacterControllerComponent>(selected_), "Character Controller");
            inspect(w.get<cn::scene::AudioSourceComponent>(selected_),         "Audio Source");
            inspect(w.get<cn::scene::AudioListenerComponent>(selected_),       "Audio Listener");
            inspect(w.get<cn::scene::NetReplicateComponent>(selected_),        "Net Replicate");
            inspect(w.get<cn::scene::ScriptComponent>(selected_),              "Script");

            // Inspect the script's own fields (if registered).
            if (auto* sc = w.get<cn::scene::ScriptComponent>(selected_)) {
                if (sc->instance) {
                    auto* ti = cn::gameplay::Registry::get().type_for(sc->class_name);
                    if (ti && ImGui::CollapsingHeader(("Script: " + sc->class_name).c_str(),
                                                     ImGuiTreeNodeFlags_DefaultOpen)) {
                        cnedit::draw_inspector(*ti, sc->instance);
                    }
                }
            }

            ImGui::Separator();
            if (ImGui::Button("+ Add Component")) ImGui::OpenPopup("AddComp");
            if (ImGui::BeginPopup("AddComp")) {
                if (ImGui::MenuItem("Camera"))           w.add<cn::scene::CameraComponent>(selected_);
                if (ImGui::MenuItem("Light"))            w.add<cn::scene::LightComponent>(selected_);
                if (ImGui::MenuItem("Mesh Renderer"))    w.add<cn::scene::MeshRenderer>(selected_);
                if (ImGui::MenuItem("Rigid Body"))       w.add<cn::scene::RigidBodyComponent>(selected_);
                if (ImGui::MenuItem("Character"))        w.add<cn::scene::CharacterControllerComponent>(selected_);
                if (ImGui::MenuItem("Audio Source"))     w.add<cn::scene::AudioSourceComponent>(selected_);
                if (ImGui::MenuItem("Audio Listener"))   w.add<cn::scene::AudioListenerComponent>(selected_);
                if (ImGui::MenuItem("Net Replicate"))    w.add<cn::scene::NetReplicateComponent>(selected_);
                if (ImGui::BeginMenu("Script")) {
                    for (auto& cls : cn::gameplay::Registry::get().class_names()) {
                        if (ImGui::MenuItem(cls.c_str())) {
                            auto& sc = w.has<cn::scene::ScriptComponent>(selected_)
                                       ? *w.get<cn::scene::ScriptComponent>(selected_)
                                       : w.add<cn::scene::ScriptComponent>(selected_);
                            sc.class_name = cls;
                            if (sc.instance) {
                                delete static_cast<cn::gameplay::Behavior*>(sc.instance);
                                sc.instance = nullptr;
                            }
                            auto inst = cn::gameplay::Registry::get().create(cls);
                            if (inst) { inst->owner = selected_; sc.instance = inst.release(); }
                        }
                    }
                    ImGui::EndMenu();
                }
                ImGui::EndPopup();
            }
        }
    }
    ImGui::End();
}

void EditorApp::draw_dockspace() {
    static bool first = true;
    ImGuiViewport* vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(vp->WorkPos);
    ImGui::SetNextWindowSize(vp->WorkSize);
    ImGui::SetNextWindowViewport(vp->ID);
    ImGuiWindowFlags flags = ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoTitleBar |
        ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::Begin("DockSpace", nullptr, flags);
    ImGui::PopStyleVar(3);
    ImGuiID dock = ImGui::GetID("MainDock");
    ImGui::DockSpace(dock, ImVec2(0, 0), ImGuiDockNodeFlags_PassthruCentralNode);
    if (first) { first = false; }
    ImGui::End();
}

int EditorApp::run(int /*argc*/, char** /*argv*/) {
    cn::EngineConfig cfg;
    cfg.window.title = "Continuous Editor";
    cfg.window.width = 1600;
    cfg.window.height = 900;
    cfg.enable_editor = true;
    cfg.enable_debug_layer = true;
    cfg.start_play_mode = false;
    cfg.gameplay_dll = cn::io::executable_dir() / "sandbox_gameplay.dll";

    if (!eng_.init(cfg)) return 1;
    eng_.set_imgui_enabled(true);

    if (!cnedit::imgui_init(eng_.window(), eng_.device())) return 2;

    eng_.window().set_event_callback([](void* ev) { cnedit::imgui_handle_event(ev); });

    eng_.set_imgui_hook([&](float /*dt*/) {
        cnedit::imgui_new_frame();
        ImGuizmo::BeginFrame();

        draw_dockspace();
        draw_menu_bar();
        draw_inspector_panel();
        hierarchy_.draw(eng_.scene(), selected_);
        viewport_.draw(eng_, selected_);
        console_.draw();
        assets_.draw(eng_);
        profiler_.draw(eng_);

        // Render ImGui ON TOP of swapchain back buffer.
        cnedit::imgui_render(eng_.device(), eng_.swapchain().back_buffer_rtv());
    });

    int rc = eng_.run();
    cnedit::imgui_shutdown();
    return rc;
}

} // namespace cnedit
