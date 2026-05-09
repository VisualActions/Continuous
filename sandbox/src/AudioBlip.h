// AudioBlip - emits a periodic blip from the entity's position so we hear
// the 3D listener move around it.
#pragma once

#include "continuous/HotReload.h"
#include "continuous/reflect/Reflect.h"

namespace sb {

class AudioBlip : public cn::gameplay::Behavior {
public:
    void on_start (cn::gameplay::Context& ctx) override;
    void on_update(cn::gameplay::Context& ctx) override;
    const cn::reflect::TypeInfo* type_info() const override;

    cn::f32 interval = 1.5f;
    cn::f32 volume   = 0.7f;
    cn::f32 timer    = 0.0f;
};

} // namespace sb

CN_REFLECT_DECL(::sb::AudioBlip)
