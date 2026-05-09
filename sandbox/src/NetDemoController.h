// NetDemoController - kicks off a loopback server+client and replicates the
// owning entity's position so we can demonstrate snapshot interpolation.
#pragma once

#include "continuous/HotReload.h"
#include "continuous/net/Net.h"
#include "continuous/reflect/Reflect.h"

namespace sb {

class NetDemoController : public cn::gameplay::Behavior {
public:
    void on_start (cn::gameplay::Context& ctx) override;
    void on_update(cn::gameplay::Context& ctx) override;
    void on_stop  (cn::gameplay::Context& ctx) override;
    const cn::reflect::TypeInfo* type_info() const override;

    bool   server = true;
    cn::u16 port   = 24631;
    cn::f32 wobble_amplitude = 1.0f;
    cn::f32 wobble_speed     = 1.0f;
    bool   started = false;
    cn::f32 t_local = 0.0f;
};

} // namespace sb

CN_REFLECT_DECL(::sb::NetDemoController)
