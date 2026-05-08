#include "continuous/math/MathReflect.h"

// We can't reflect glm::vec types as Struct via offsetof because they're
// non-standard layout in some configs. Instead, the field types Vec*/Quat
// already understand the value as a primitive. We register Transform/AABB
// with their Vec3/Quat sub-fields.

CN_REFLECT_BEGIN(::cn::math::Transform)
    CN_REFLECT_FIELD(position, Vec3)
    CN_REFLECT_FIELD(rotation, Quat)
    CN_REFLECT_FIELD(scale,    Vec3)
CN_REFLECT_END(::cn::math::Transform)

CN_REFLECT_BEGIN(::cn::math::AABB)
    CN_REFLECT_FIELD(min, Vec3)
    CN_REFLECT_FIELD(max, Vec3)
CN_REFLECT_END(::cn::math::AABB)

// Stubs for the smaller atoms - inspector handles them as primitives directly.
CN_REFLECT_BEGIN(::cn::math::vec2)
CN_REFLECT_END(::cn::math::vec2)
CN_REFLECT_BEGIN(::cn::math::vec3)
CN_REFLECT_END(::cn::math::vec3)
CN_REFLECT_BEGIN(::cn::math::vec4)
CN_REFLECT_END(::cn::math::vec4)
CN_REFLECT_BEGIN(::cn::math::quat)
CN_REFLECT_END(::cn::math::quat)
