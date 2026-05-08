// Physics - Jolt integration. We expose a small surface that talks in our own
// types (math::vec3, scene::Entity) and forwards to Jolt under the hood.
#pragma once

#include "continuous/core/Macros.h"
#include "continuous/core/Types.h"
#include "continuous/ecs/Entity.h"
#include "continuous/math/Math.h"

#include <memory>
#include <vector>

namespace cn::physics {

struct CN_API RaycastHit {
    bool       hit = false;
    f32        distance = 0.0f;
    math::vec3 point   {0, 0, 0};
    math::vec3 normal  {0, 1, 0};
    ecs::Entity entity{};
};

struct CN_API ContactInfo {
    ecs::Entity a;
    ecs::Entity b;
    math::vec3 point  {0, 0, 0};
    math::vec3 normal {0, 1, 0};
};

class CN_API World {
public:
    World();
    ~World();
    CN_NONCOPYABLE(World);

    bool init();
    void shutdown();

    // Fixed-step simulation (60 Hz default). Caller passes wall delta; we
    // accumulate and run zero or more 1/60 ticks.
    void step(f32 dt_seconds);

    void  set_gravity(math::vec3 g);
    math::vec3 gravity() const;

    // Bodies are referenced by opaque u32 (Jolt::BodyID under the hood).
    u32 add_box   (ecs::Entity owner, math::vec3 position, math::quat rotation,
                    math::vec3 half_extents, f32 mass, bool is_static, bool trigger);
    u32 add_sphere(ecs::Entity owner, math::vec3 position, math::quat rotation,
                    f32 radius, f32 mass, bool is_static, bool trigger);
    u32 add_capsule(ecs::Entity owner, math::vec3 position, math::quat rotation,
                    f32 radius, f32 height, f32 mass, bool is_static, bool trigger);
    void remove_body(u32 id);
    bool valid(u32 id) const;

    // Per-frame writes / reads.
    void  set_position(u32 id, math::vec3 p);
    void  set_rotation(u32 id, math::quat q);
    void  set_linear_velocity (u32 id, math::vec3 v);
    void  set_angular_velocity(u32 id, math::vec3 v);
    math::vec3 linear_velocity (u32 id) const;
    math::vec3 angular_velocity(u32 id) const;
    void  add_force (u32 id, math::vec3 f);
    void  add_impulse(u32 id, math::vec3 imp);
    void  get_transform(u32 id, math::vec3& pos, math::quat& rot) const;
    void  set_friction(u32 id, f32 friction);
    void  set_restitution(u32 id, f32 r);

    // Character controller (Jolt CharacterVirtual).
    void* create_character(ecs::Entity owner, math::vec3 position, f32 radius, f32 height);
    void  destroy_character(void* ch);
    void  character_set_velocity(void* ch, math::vec3 v);
    void  character_get_transform(void* ch, math::vec3& pos, math::quat& rot) const;
    bool  character_is_grounded(void* ch) const;
    void  character_step(void* ch, math::vec3 wish_velocity, bool wish_jump, f32 dt);

    // Queries.
    RaycastHit raycast(math::vec3 origin, math::vec3 dir, f32 max_distance);

    // Debug draw via DebugDraw - the renderer pushes lines.
    void debug_draw(class gfx::DebugDraw& dd);

    // Contacts collected this frame (since last call).
    const std::vector<ContactInfo>& contacts() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

CN_API World& global();

} // namespace cn::physics
