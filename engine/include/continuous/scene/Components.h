// The standard set of components every gameplay project starts with.
//
// All components are POD-friendly (or close to) so the ECS storage can move
// them around with memcpy. Each component is reflected so the inspector and
// serializer can read/write it.
#pragma once

#include "continuous/core/Macros.h"
#include "continuous/core/Types.h"
#include "continuous/ecs/Entity.h"
#include "continuous/gfx/Material.h"
#include "continuous/gfx/Mesh.h"
#include "continuous/math/Math.h"
#include "continuous/math/MathReflect.h"
#include "continuous/reflect/Reflect.h"

#include <string>
#include <vector>

namespace cn::scene {

// ----------------------------------------------------------------------------
// NameComponent: optional human-readable name for an entity.
// ----------------------------------------------------------------------------
struct CN_API NameComponent {
    std::string name;
};

// ----------------------------------------------------------------------------
// TransformComponent: local transform plus a cached world matrix.
// Hierarchy is held in HierarchyComponent (parent + children).
// ----------------------------------------------------------------------------
struct CN_API TransformComponent {
    math::Transform local{};
    math::mat4      world  = math::mat4(1.0f);
    bool            dirty  = true;

    static constexpr const char* kName = "Transform";
};

// ----------------------------------------------------------------------------
// HierarchyComponent: tracks parent + first-child + sibling pointers.
// We use Entity ids - cheap to serialize.
// ----------------------------------------------------------------------------
struct CN_API HierarchyComponent {
    ecs::Entity parent     {};
    ecs::Entity first_child{};
    ecs::Entity next_sibling{};
    ecs::Entity prev_sibling{};
};

// ----------------------------------------------------------------------------
// CameraComponent: camera + clear color + viewport (full window by default).
// ----------------------------------------------------------------------------
struct CN_API CameraComponent {
    f32 fov_y_deg   = 60.0f;
    f32 near_z      = 0.1f;
    f32 far_z       = 500.0f;
    math::vec4 clear_color{0.05f, 0.06f, 0.08f, 1.0f};
    bool primary    = true;
};

// ----------------------------------------------------------------------------
// LightComponent.
// ----------------------------------------------------------------------------
enum class LightType : u32 { Directional = 0, Point = 1, Spot = 2 };

struct CN_API LightComponent {
    LightType  type      = LightType::Directional;
    math::vec3 color     {1, 1, 1};
    f32        intensity = 1.0f;
    f32        range     = 20.0f;
    f32        spot_inner_deg = 20.0f;
    f32        spot_outer_deg = 30.0f;
    bool       casts_shadow = false;
};

// ----------------------------------------------------------------------------
// MeshRenderer: mesh + per-submesh material slots.
// ----------------------------------------------------------------------------
struct CN_API MeshRenderer {
    std::string mesh_id;             // resolved on load to gfx::Mesh*
    gfx::Mesh*  mesh = nullptr;
    std::vector<std::string> material_ids;
    std::vector<gfx::Material*> materials;
    bool cast_shadow = true;
    bool visible = true;
};

// ----------------------------------------------------------------------------
// Physics: rigid body shape + mass.
// ----------------------------------------------------------------------------
enum class ColliderShape : u32 { Box, Sphere, Capsule };

struct CN_API RigidBodyComponent {
    ColliderShape shape = ColliderShape::Box;
    math::vec3 size  {1, 1, 1};       // box half-extents / capsule (radius, height, _)
    f32        mass = 1.0f;
    f32        friction = 0.6f;
    f32        restitution = 0.1f;
    bool       is_static = false;
    bool       is_trigger = false;
    u32        body_id = 0;           // jolt BodyID (0 = unattached)
    math::vec3 linear_velocity {0,0,0};
    math::vec3 angular_velocity{0,0,0};
};

struct CN_API CharacterControllerComponent {
    f32 height = 1.8f;
    f32 radius = 0.4f;
    f32 step_height = 0.3f;
    f32 max_walk_slope_deg = 50.0f;
    f32 move_speed = 5.0f;
    f32 jump_speed = 6.0f;
    bool grounded = false;
    math::vec3 wish_velocity {0, 0, 0};
    bool wish_jump = false;
    void* native = nullptr; // jolt CharacterVirtual*
};

// ----------------------------------------------------------------------------
// AudioSourceComponent: 3D sound emitter.
// ----------------------------------------------------------------------------
struct CN_API AudioSourceComponent {
    std::string clip_id;
    f32  volume = 1.0f;
    f32  pitch  = 1.0f;
    bool loop   = false;
    bool play_on_start = false;
    bool spatialize    = true;
    f32  min_distance  = 1.0f;
    f32  max_distance  = 50.0f;
    void* native = nullptr; // ma_sound*
};

struct CN_API AudioListenerComponent {
    bool primary = true;
};

// ----------------------------------------------------------------------------
// Networking: replicated state + role.
// ----------------------------------------------------------------------------
enum class NetRole : u32 { None = 0, Authority = 1, Replicated = 2 };

struct CN_API NetReplicateComponent {
    NetRole role = NetRole::None;
    u32     net_id = 0;
    f32     send_interval = 1.0f / 20.0f;
    f32     accumulator   = 0.0f;
    bool    interpolate   = true;
    // snapshot interpolation buffer: <time, position, rotation>
    struct Snap { f32 t = 0; math::vec3 p; math::quat r; };
    Snap    snaps[4];
    u32     snap_head = 0;
    u32     snap_count = 0;
};

// ----------------------------------------------------------------------------
// Script component: links an entity to a gameplay class instance from the
// hot-reloaded gameplay DLL.
// ----------------------------------------------------------------------------
struct CN_API ScriptComponent {
    std::string class_name;
    void*       instance = nullptr; // owning ptr to a Gameplay::Behavior subclass
};

} // namespace cn::scene

// Reflection declarations.
CN_REFLECT_DECL(::cn::scene::NameComponent)
CN_REFLECT_DECL(::cn::scene::TransformComponent)
CN_REFLECT_DECL(::cn::scene::CameraComponent)
CN_REFLECT_DECL(::cn::scene::LightType)
CN_REFLECT_DECL(::cn::scene::LightComponent)
CN_REFLECT_DECL(::cn::scene::MeshRenderer)
CN_REFLECT_DECL(::cn::scene::ColliderShape)
CN_REFLECT_DECL(::cn::scene::RigidBodyComponent)
CN_REFLECT_DECL(::cn::scene::CharacterControllerComponent)
CN_REFLECT_DECL(::cn::scene::AudioSourceComponent)
CN_REFLECT_DECL(::cn::scene::AudioListenerComponent)
CN_REFLECT_DECL(::cn::scene::NetRole)
CN_REFLECT_DECL(::cn::scene::NetReplicateComponent)
CN_REFLECT_DECL(::cn::scene::ScriptComponent)
