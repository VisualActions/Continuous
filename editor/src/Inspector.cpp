#include "Inspector.h"
#include "continuous/math/Math.h"

#include <imgui.h>

#include <cstring>

using namespace cn::reflect;

namespace cnedit {

static bool draw_field(const FieldInfo& f, void* obj) {
    bool changed = false;
    cn::u8* base = static_cast<cn::u8*>(obj) + f.offset;
    ImGui::PushID(f.name.c_str());
    switch (f.type) {
        case FieldType::Bool:
            changed = ImGui::Checkbox(f.name.c_str(), reinterpret_cast<bool*>(base));
            break;
        case FieldType::I32: {
            int v = *reinterpret_cast<cn::i32*>(base);
            if (f.drag_min < f.drag_max)
                changed = ImGui::DragInt(f.name.c_str(), &v, f.drag_speed, (int)f.drag_min, (int)f.drag_max);
            else
                changed = ImGui::DragInt(f.name.c_str(), &v, f.drag_speed);
            *reinterpret_cast<cn::i32*>(base) = v;
        } break;
        case FieldType::U32: {
            int v = (int)*reinterpret_cast<cn::u32*>(base);
            changed = ImGui::DragInt(f.name.c_str(), &v, f.drag_speed, 0, INT_MAX);
            if (v < 0) v = 0;
            *reinterpret_cast<cn::u32*>(base) = (cn::u32)v;
        } break;
        case FieldType::F32:
            if (f.drag_min < f.drag_max)
                changed = ImGui::SliderFloat(f.name.c_str(), reinterpret_cast<float*>(base),
                                             f.drag_min, f.drag_max);
            else
                changed = ImGui::DragFloat(f.name.c_str(), reinterpret_cast<float*>(base),
                                            f.drag_speed > 0 ? f.drag_speed : 0.1f);
            break;
        case FieldType::F64: {
            double v = *reinterpret_cast<double*>(base);
            changed = ImGui::DragScalar(f.name.c_str(), ImGuiDataType_Double, &v, f.drag_speed);
            *reinterpret_cast<double*>(base) = v;
        } break;
        case FieldType::String: {
            auto& s = *reinterpret_cast<std::string*>(base);
            char buf[512]; buf[0] = 0;
            std::strncpy(buf, s.c_str(), sizeof(buf) - 1);
            if (ImGui::InputText(f.name.c_str(), buf, sizeof(buf))) {
                s = buf;
                changed = true;
            }
        } break;
        case FieldType::Vec2:
            changed = ImGui::DragFloat2(f.name.c_str(), reinterpret_cast<float*>(base), f.drag_speed);
            break;
        case FieldType::Vec3:
            changed = ImGui::DragFloat3(f.name.c_str(), reinterpret_cast<float*>(base), f.drag_speed);
            break;
        case FieldType::Vec4:
            changed = ImGui::DragFloat4(f.name.c_str(), reinterpret_cast<float*>(base), f.drag_speed);
            break;
        case FieldType::Color:
            changed = ImGui::ColorEdit4(f.name.c_str(), reinterpret_cast<float*>(base),
                                        ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_HDR);
            break;
        case FieldType::Quat: {
            auto* q = reinterpret_cast<cn::math::quat*>(base);
            cn::math::vec3 e = glm::degrees(glm::eulerAngles(*q));
            float arr[3] = { e.x, e.y, e.z };
            if (ImGui::DragFloat3(f.name.c_str(), arr, 0.5f)) {
                *q = cn::math::quat(glm::radians(cn::math::vec3(arr[0], arr[1], arr[2])));
                changed = true;
            }
        } break;
        case FieldType::Enum: {
            cn::i64 raw = 0;
            std::memcpy(&raw, base, std::min<cn::usize>(f.size, sizeof(cn::i64)));
            const char* preview = "?";
            for (auto& ev : f.enum_values) if (ev.value == raw) { preview = ev.name.c_str(); break; }
            if (ImGui::BeginCombo(f.name.c_str(), preview)) {
                for (auto& ev : f.enum_values) {
                    bool sel = ev.value == raw;
                    if (ImGui::Selectable(ev.name.c_str(), sel)) {
                        raw = ev.value;
                        std::memcpy(base, &raw, std::min<cn::usize>(f.size, sizeof(cn::i64)));
                        changed = true;
                    }
                }
                ImGui::EndCombo();
            }
        } break;
        case FieldType::Struct:
            if (f.inner) {
                if (ImGui::TreeNodeEx(f.name.c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
                    if (draw_inspector(*f.inner, base)) changed = true;
                    ImGui::TreePop();
                }
            }
            break;
        case FieldType::VectorOf: {
            cn::usize n = f.vec_size(base);
            if (ImGui::TreeNodeEx(f.name.c_str(), ImGuiTreeNodeFlags_DefaultOpen, "%s [%zu]", f.name.c_str(), n)) {
                for (cn::usize i = 0; i < n; ++i) {
                    ImGui::PushID((int)i);
                    if (f.inner) draw_inspector(*f.inner, f.vec_at(base, i));
                    if (ImGui::SmallButton("Remove")) { f.vec_erase(base, i); changed = true; --i; --n; }
                    ImGui::PopID();
                }
                if (ImGui::Button("Add")) {
                    f.vec_push_default(base);
                    changed = true;
                }
                ImGui::TreePop();
            }
        } break;
        default:
            ImGui::TextDisabled("%s : (unsupported)", f.name.c_str());
            break;
    }
    if (!f.tooltip.empty() && ImGui::IsItemHovered()) {
        ImGui::SetTooltip("%s", f.tooltip.c_str());
    }
    ImGui::PopID();
    return changed;
}

bool draw_inspector(const TypeInfo& ti, void* obj) {
    bool any = false;
    for (auto& f : ti.fields) any |= draw_field(f, obj);
    return any;
}

} // namespace cnedit
