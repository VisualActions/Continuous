#include "Viewport.h"
#include "continuous/scene/Components.h"

#include <imgui.h>
#include <ImGuizmo.h>

namespace cnedit {

void ViewportPanel::draw(cn::Engine& eng, cn::ecs::Entity selected) {
    if (ImGui::Begin("Viewport")) {
        ImVec2 avail = ImGui::GetContentRegionAvail();
        u32 w = std::max(1, (int)avail.x);
        u32 h = std::max(1, (int)avail.y);

        // Configure scene camera from this orbit camera. We bypass the scene
        // camera by injecting one directly into the renderer for editor frames.
        cn::math::vec3 fwd = cn::math::vec3(
            std::cos(cn::math::rad(cam_pitch)) * std::sin(cn::math::rad(cam_yaw)),
            std::sin(cn::math::rad(cam_pitch)),
            std::cos(cn::math::rad(cam_pitch)) * std::cos(cn::math::rad(cam_yaw))
        );
        cam_pos = cam_target - fwd * cam_dist;

        cn::gfx::CameraData cd;
        cd.position = cam_pos;
        cd.view = cn::math::look_at(cam_pos, cam_target, {0, 1, 0});
        cd.projection = cn::math::perspective(cn::math::rad(fov_deg),
            (float)w / (float)std::max(1u, h), 0.05f, 1000.0f);
        cd.fov_y_rad = cn::math::rad(fov_deg);
        cd.near_z = 0.05f;
        cd.far_z  = 1000.0f;
        cd.clear_color = {0.05f, 0.06f, 0.08f, 1.0f};

        // Render scene into offscreen target.
        eng.scene().update_world_transforms();
        eng.renderer().begin_frame();
        eng.renderer().set_camera(cd);

        auto& world = eng.scene().world();
        world.each<cn::scene::TransformComponent, cn::scene::LightComponent>(
            [&](cn::ecs::Entity, cn::scene::TransformComponent& t, cn::scene::LightComponent& l) {
                cn::gfx::LightData ld;
                ld.position  = cn::math::vec3(t.world[3]);
                cn::math::quat rot = glm::quat_cast(cn::math::mat3(t.world));
                ld.direction = rot * cn::math::vec3(0, 0, 1);
                ld.color     = l.color;
                ld.intensity = l.intensity;
                ld.range     = l.range;
                ld.spot_inner= cn::math::rad(l.spot_inner_deg);
                ld.spot_outer= cn::math::rad(l.spot_outer_deg);
                ld.casts_shadow = l.casts_shadow;
                ld.type      = static_cast<cn::gfx::LightType>(static_cast<cn::u32>(l.type));
                eng.renderer().submit_light(ld);
            });

        auto& mgr = cn::asset::Manager::get();
        world.each<cn::scene::TransformComponent, cn::scene::MeshRenderer>(
            [&](cn::ecs::Entity, cn::scene::TransformComponent& t, cn::scene::MeshRenderer& mr) {
                if (!mr.visible) return;
                if (!mr.mesh && !mr.mesh_id.empty()) mr.mesh = mgr.load_mesh(mr.mesh_id);
                if (!mr.mesh) return;
                if (mr.materials.size() < mr.material_ids.size())
                    mr.materials.resize(mr.material_ids.size(), nullptr);
                for (cn::usize i = 0; i < mr.material_ids.size(); ++i)
                    if (!mr.materials[i] && !mr.material_ids[i].empty())
                        mr.materials[i] = mgr.load_material(mr.material_ids[i]);

                for (cn::u32 i = 0; i < (cn::u32)mr.mesh->subs().size(); ++i) {
                    cn::gfx::DrawItem it;
                    it.mesh = mr.mesh;
                    it.submesh = i;
                    cn::gfx::Material* mat = nullptr;
                    cn::u32 mid = mr.mesh->subs()[i].material_id;
                    if (mid < mr.materials.size()) mat = mr.materials[mid];
                    if (!mat) mat = mgr.load_material("_default");
                    it.material = mat;
                    it.transform = t.world;
                    it.world_aabb = mr.mesh->subs()[i].bounds.transformed(t.world);
                    eng.renderer().submit_draw(it);
                }
            });

        // Draw floor grid + axis on selected.
        eng.renderer().debug().grid(50.0f, 50, {0.4f, 0.4f, 0.4f, 1.0f});
        if (eng.scene().world().alive(selected)) {
            if (auto* t = eng.scene().world().get<cn::scene::TransformComponent>(selected)) {
                eng.renderer().debug().axes(t->world, 1.0f);
                eng.renderer().debug().aabb(cn::math::AABB{
                    cn::math::vec3(t->world[3]) - cn::math::vec3(0.5f),
                    cn::math::vec3(t->world[3]) + cn::math::vec3(0.5f)
                }, {1, 1, 0, 1});
            }
        }

        eng.renderer().render_offscreen(w, h);
        eng.renderer().end_frame();

        ID3D11ShaderResourceView* srv = eng.renderer().offscreen_srv();
        if (srv) {
            ImVec2 cursor = ImGui::GetCursorScreenPos();
            ImGui::Image((ImTextureID)(intptr_t)srv, avail);

            // Camera controls when hovering the image.
            bool hovered = ImGui::IsItemHovered();
            if (hovered) {
                float wheel = ImGui::GetIO().MouseWheel;
                if (wheel != 0) cam_dist = std::max(0.5f, cam_dist - wheel * 0.5f);

                if (ImGui::IsMouseDragging(ImGuiMouseButton_Right)) {
                    auto delta = ImGui::GetIO().MouseDelta;
                    cam_yaw   -= delta.x * 0.3f;
                    cam_pitch -= delta.y * 0.3f;
                    cam_pitch = std::clamp(cam_pitch, -89.0f, 89.0f);
                }
                if (ImGui::IsMouseDragging(ImGuiMouseButton_Middle)) {
                    auto delta = ImGui::GetIO().MouseDelta;
                    cn::math::vec3 right = glm::normalize(glm::cross({0, 1, 0}, fwd));
                    cn::math::vec3 up    = glm::normalize(glm::cross(fwd, right));
                    cam_target -= right * (delta.x * 0.01f * cam_dist);
                    cam_target += up    * (delta.y * 0.01f * cam_dist);
                }
            }

            // ImGuizmo for selected entity transform.
            ImGuizmo::SetOrthographic(false);
            ImGuizmo::SetDrawlist();
            ImGuizmo::SetRect(cursor.x, cursor.y, avail.x, avail.y);

            if (eng.scene().world().alive(selected)) {
                if (auto* t = eng.scene().world().get<cn::scene::TransformComponent>(selected)) {
                    ImGuizmo::OPERATION g_op =
                        op == Op::Translate ? ImGuizmo::TRANSLATE :
                        op == Op::Rotate    ? ImGuizmo::ROTATE    :
                                              ImGuizmo::SCALE;
                    ImGuizmo::MODE g_mode = space == Space::World ? ImGuizmo::WORLD : ImGuizmo::LOCAL;

                    cn::math::mat4 m = t->world;
                    ImGuizmo::Manipulate(glm::value_ptr(cd.view),
                                         glm::value_ptr(cd.projection),
                                         g_op, g_mode,
                                         glm::value_ptr(m));
                    if (ImGuizmo::IsUsing()) {
                        cn::math::mat4 parent = cn::math::mat4(1);
                        if (auto* h = eng.scene().world().get<cn::scene::HierarchyComponent>(selected)) {
                            if (h->parent.valid()) {
                                if (auto* pt = eng.scene().world().get<cn::scene::TransformComponent>(h->parent))
                                    parent = pt->world;
                            }
                        }
                        cn::math::mat4 local = glm::inverse(parent) * m;
                        t->local = cn::math::Transform::from_matrix(local);
                        t->dirty = true;
                        eng.scene().mark_dirty(selected);
                    }
                }
            }
        }

        // Toolbar overlay.
        ImGui::SetCursorScreenPos(ImVec2(ImGui::GetWindowPos().x + 8, ImGui::GetWindowPos().y + 30));
        if (ImGui::SmallButton("T")) op = Op::Translate; ImGui::SameLine();
        if (ImGui::SmallButton("R")) op = Op::Rotate;    ImGui::SameLine();
        if (ImGui::SmallButton("S")) op = Op::Scale;     ImGui::SameLine();
        ImGui::Text("|"); ImGui::SameLine();
        if (ImGui::SmallButton(space == Space::World ? "World" : "Local")) {
            space = (space == Space::World) ? Space::Local : Space::World;
        }
    }
    ImGui::End();
}

} // namespace cnedit
