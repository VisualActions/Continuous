#include "Spinner.h"
#include "continuous/Engine.h"
#include "continuous/scene/Components.h"

CN_REFLECT_BEGIN(::sb::Spinner)
    CN_REFLECT_FIELD(axis, Vec3)
    CN_REFLECT_FIELD_RANGE(speed_deg, F32, -1080.0f, 1080.0f, 1.0f)
CN_REFLECT_END(::sb::Spinner)

namespace sb {

void Spinner::on_update(cn::gameplay::Context& ctx) {
    if (!ctx.scene) return;
    auto* t = ctx.scene->world().get<cn::scene::TransformComponent>(owner);
    if (!t) return;
    cn::f32 a = cn::math::rad(speed_deg) * ctx.dt;
    cn::math::vec3 ax = glm::length(axis) > 0 ? glm::normalize(axis) : cn::math::vec3(0, 1, 0);
    cn::math::quat dq = glm::angleAxis(a, ax);
    t->local.rotation = glm::normalize(dq * t->local.rotation);
    t->dirty = true;
}

const cn::reflect::TypeInfo* Spinner::type_info() const {
    return cn::reflect::type_of<Spinner>();
}

} // namespace sb

CN_GAMEPLAY_REGISTER(sb::Spinner)
