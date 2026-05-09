// Reflection-driven inspector. Renders any reflected struct via ImGui widgets.
#pragma once

#include "continuous/reflect/Reflect.h"

namespace cnedit {

bool draw_inspector(const cn::reflect::TypeInfo& ti, void* obj);

} // namespace cnedit
