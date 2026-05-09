// PlayerController - drives the CharacterControllerComponent from input.
//
// Demonstrates: input system, character controller, scene transform reads.
#pragma once

#include "continuous/HotReload.h"
#include "continuous/math/Math.h"
#include "continuous/reflect/Reflect.h"

namespace sb {

class PlayerController : public cn::gameplay::Behavior {
public:
    void on_start (cn::gameplay::Context& ctx) override;
    void on_update(cn::gameplay::Context& ctx) override;
    const cn::reflect::TypeInfo* type_info() const override;

    cn::f32 walk_speed = 5.0f;
    cn::f32 run_speed  = 9.0f;
    cn::f32 mouse_sensitivity = 0.12f;
    cn::f32 yaw   = 0.0f;
    cn::f32 pitch = -10.0f;
    bool    invert_y = false;
};

} // namespace sb

CN_REFLECT_DECL(::sb::PlayerController)
