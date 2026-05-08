#include "continuous/gfx/Material.h"

namespace cn::gfx {
// Material is currently a POD; this TU exists so the linker has a home for
// any future Material methods (e.g. dirty flags, hash, blend descriptor).
} // namespace cn::gfx
