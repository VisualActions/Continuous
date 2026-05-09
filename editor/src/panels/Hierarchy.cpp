#include "Hierarchy.h"
#include "continuous/scene/Components.h"

#include <imgui.h>

#include <string>

namespace cnedit {

static void draw_node(cn::scene::Scene& scene, cn::ecs::Entity e, cn::ecs::Entity& selected) {
    auto& w = scene.world();
    auto* nc = w.get<cn::scene::NameComponent>(e);
    std::string label = nc ? nc->name : "Entity";
    label += " (" + std::to_string(e.idx) + ")";

    auto children = scene.children_of(e);
    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth;
    if (children.empty()) flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
    if (selected == e) flags |= ImGuiTreeNodeFlags_Selected;

    bool open = ImGui::TreeNodeEx((void*)(intptr_t)e.idx, flags, "%s", label.c_str());
    if (ImGui::IsItemClicked()) selected = e;

    if (ImGui::BeginPopupContextItem()) {
        if (ImGui::MenuItem("Add Child")) {
            auto c = scene.create_entity("New Entity");
            scene.set_parent(c, e);
        }
        if (ImGui::MenuItem("Delete")) {
            scene.destroy_entity(e);
            if (open && !children.empty()) ImGui::TreePop();
            ImGui::EndPopup();
            return;
        }
        ImGui::EndPopup();
    }
    if (open && !children.empty()) {
        for (auto c : children) draw_node(scene, c, selected);
        ImGui::TreePop();
    }
}

void HierarchyPanel::draw(cn::scene::Scene& scene, cn::ecs::Entity& selected) {
    if (ImGui::Begin("Hierarchy")) {
        if (ImGui::Button("+ Empty")) scene.create_entity("New Entity");
        ImGui::SameLine();
        if (ImGui::Button("+ Cube")) {
            auto e = scene.create_entity("Cube");
            auto& mr = scene.world().add<cn::scene::MeshRenderer>(e);
            mr.mesh_id = "_procedural/cube";
            mr.material_ids = { "_default" };
        }
        ImGui::SameLine();
        if (ImGui::Button("+ Sphere")) {
            auto e = scene.create_entity("Sphere");
            auto& mr = scene.world().add<cn::scene::MeshRenderer>(e);
            mr.mesh_id = "_procedural/sphere";
            mr.material_ids = { "_default" };
        }
        ImGui::SameLine();
        if (ImGui::Button("+ Light")) {
            auto e = scene.create_entity("Directional Light");
            auto& l = scene.world().add<cn::scene::LightComponent>(e);
            l.type = cn::scene::LightType::Directional;
            l.casts_shadow = true;
        }
        ImGui::Separator();
        // Walk roots.
        scene.world().each<cn::scene::HierarchyComponent>(
            [&](cn::ecs::Entity e, cn::scene::HierarchyComponent& h) {
                if (!h.parent.valid()) draw_node(scene, e, selected);
            });
    }
    ImGui::End();
}

} // namespace cnedit
