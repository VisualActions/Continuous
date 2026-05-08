#pragma once

#include "continuous/core/Types.h"

namespace cn::ecs {

struct EntityTag {};
using Entity = Handle<EntityTag>;
inline constexpr Entity kInvalidEntity{ kInvalidU32, 0 };

} // namespace cn::ecs
