#include "PlayerController.h"
#include "continuous/Engine.h"
#include "continuous/platform/Input.h"
#include "continuous/scene/Components.h"

CN_REFLECT_BEGIN(::sb::PlayerController)
    CN_REFLECT_FIELD_RANGE(walk_speed,        F32, 0.0f, 30.0f, 0.1f)
    CN_REFLECT_FIELD_RANGE(run_speed,         F32, 0.0f, 50.0f, 0.1f)
    CN_REFLECT_FIELD_RANGE(mouse_sensitivity, F32, 0.0f, 2.0f,  0.01f)
    CN_REFLECT_FIELD(yaw,   F32)
    CN_REFLECT_FIELD(pitch, F32)
    CN_REFLECT_FIELD(invert_y, Bool)
CN_REFLECT_END(::sb::PlayerController)

namespace sb {

void PlayerController::on_start(cn::gameplay::Context& ctx) {
    if (!ctx.scene) return;
    // Grab mouse on play.
    if (ctx.window) ctx.window->set_capture_mouse(true);
}

void PlayerController::on_update(cn::gameplay::Context& ctx) {
    if (!ctx.scene) return;
    auto& w = ctx.scene->world();
    auto* t = w.get<cn::scene::TransformComponent>(owner);
    auto* cc = w.get<cn::scene::CharacterControllerComponent>(owner);
    if (!t || !cc) return;

    auto& in = cn::platform::Input::get();

    // Mouse-look.
    if (ctx.window && ctx.window->capture_mouse()) {
        auto md = in.mouse_delta();
        yaw   -= md.x * mouse_sensitivity;
        pitch -= (invert_y ? -md.y : md.y) * mouse_sensitivity;
        pitch = cn::math::clamp(pitch, -89.0f, 89.0f);
    }
    if (in.key_pressed(cn::platform::Key::Escape)) {
        if (ctx.window) ctx.window->set_capture_mouse(false);
    }
    if (in.mouse_pressed(cn::platform::MouseButton::Left)) {
        if (ctx.window) ctx.window->set_capture_mouse(true);
    }

    // Build camera basis from yaw.
    cn::math::quat q = glm::angleAxis(cn::math::rad(yaw), cn::math::vec3(0, 1, 0));
    t->local.rotation = q;
    t->dirty = true;

    cn::math::vec3 forward = q * cn::math::vec3(0, 0, 1);
    cn::math::vec3 right   = q * cn::math::vec3(1, 0, 0);

    cn::math::vec3 wish(0);
    bool sprint = in.key_down(cn::platform::Key::LeftShift);
    if (in.key_down(cn::platform::Key::W)) wish += forward;
    if (in.key_down(cn::platform::Key::S)) wish -= forward;
    if (in.key_down(cn::platform::Key::A)) wish -= right;
    if (in.key_down(cn::platform::Key::D)) wish += right;

    // Gamepad stick override.
    if (in.gamepad_connected(0)) {
        const auto& g = in.gamepad(0);
        cn::f32 lx = g.axis[(cn::usize)cn::platform::GamepadAxis::LeftX];
        cn::f32 ly = g.axis[(cn::usize)cn::platform::GamepadAxis::LeftY];
        wish += right * lx + forward * (-ly);
        cn::f32 rx = g.axis[(cn::usize)cn::platform::GamepadAxis::RightX];
        cn::f32 ry = g.axis[(cn::usize)cn::platform::GamepadAxis::RightY];
        yaw   -= rx * mouse_sensitivity * 200.0f * ctx.dt;
        pitch -= (invert_y ? -ry : ry) * mouse_sensitivity * 200.0f * ctx.dt;
    }

    if (glm::length(wish) > 0.001f) wish = glm::normalize(wish);
    cn::f32 spd = sprint ? run_speed : walk_speed;
    cc->wish_velocity = wish * spd;
    if (in.key_pressed(cn::platform::Key::Space) ||
        (in.gamepad_connected(0) && in.gamepad(0).pressed[(cn::usize)cn::platform::GamepadButton::A])) {
        cc->wish_jump = true;
    }
}

const cn::reflect::TypeInfo* PlayerController::type_info() const {
    return cn::reflect::type_of<PlayerController>();
}

} // namespace sb

CN_GAMEPLAY_REGISTER(sb::PlayerController)
