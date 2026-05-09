#include "NetDemoController.h"
#include "continuous/Engine.h"
#include "continuous/scene/Components.h"

#include <cmath>

CN_REFLECT_BEGIN(::sb::NetDemoController)
    CN_REFLECT_FIELD(server, Bool)
    CN_REFLECT_FIELD_RANGE(port, U32, 1024.0f, 65535.0f, 1.0f)
    CN_REFLECT_FIELD_RANGE(wobble_amplitude, F32, 0.0f, 10.0f, 0.05f)
    CN_REFLECT_FIELD_RANGE(wobble_speed,     F32, 0.0f, 10.0f, 0.05f)
CN_REFLECT_END(::sb::NetDemoController)

namespace sb {

void NetDemoController::on_start(cn::gameplay::Context& /*ctx*/) {
    auto& s = cn::net::global();
    if (s.mode() == cn::net::Mode::Offline) {
        if (server) s.start_server(port);
        else        s.start_client("127.0.0.1", port);
    }
    started = true;
    t_local = 0.0f;
}

void NetDemoController::on_update(cn::gameplay::Context& ctx) {
    if (!ctx.scene) return;
    auto* t = ctx.scene->world().get<cn::scene::TransformComponent>(owner);
    if (!t) return;
    t_local += ctx.dt;
    auto& s = cn::net::global();

    if (s.mode() == cn::net::Mode::Server) {
        // Authority: drive position with a wobble.
        cn::math::vec3 base = t->local.position;
        base.y = 1.0f + wobble_amplitude * std::sin(t_local * wobble_speed);
        t->local.position = base;
        t->dirty = true;

        cn::net::ReplicatedSnapshot snap;
        snap.net_id   = 1;
        snap.position = t->local.position;
        snap.rotation = t->local.rotation;
        s.server_set_local_snapshots({snap});
    } else if (s.mode() == cn::net::Mode::Client) {
        // Pull the latest snapshots and apply with interpolation.
        auto& v = s.client_latest_snapshots();
        if (!v.empty()) {
            cn::math::vec3 p; cn::math::quat q;
            cn::net::snapshot_interpolate(v.data(), (cn::u32)v.size(),
                t_local - 0.10f, p, q);
            t->local.position = p;
            t->local.rotation = q;
            t->dirty = true;
        }
    }
}

void NetDemoController::on_stop(cn::gameplay::Context& /*ctx*/) {
    cn::net::global().stop();
    started = false;
}

const cn::reflect::TypeInfo* NetDemoController::type_info() const {
    return cn::reflect::type_of<NetDemoController>();
}

} // namespace sb

CN_GAMEPLAY_REGISTER(sb::NetDemoController)
