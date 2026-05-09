// Spinner - rotates the entity each frame. Smallest possible behavior - shows
// hot reload by changing the rotation axis or speed in code.
#pragma once

#include "continuous/HotReload.h"
#include "continuous/math/Math.h"
#include "continuous/reflect/Reflect.h"

namespace sb {

class Spinner : public cn::gameplay::Behavior {
public:
    void on_update(cn::gameplay::Context& ctx) override;
    const cn::reflect::TypeInfo* type_info() const override;

    cn::math::vec3 axis  {0.0f, 1.0f, 0.0f};
    cn::f32        speed_deg = 90.0f;
};

} // namespace sb

CN_REFLECT_DECL(::sb::Spinner)
