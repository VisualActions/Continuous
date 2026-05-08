// Physics - Jolt integration.
//
// Layout follows the standard Jolt sample pattern:
//   - global TempAllocator and JobSystemThreadPool (one per World)
//   - PhysicsSystem with our own object/broadphase layer filters
//   - body interface for runtime add/remove/transform
//
// We choose CharacterVirtual (kinematic-ish) for the player controller
// because it gives us responsive control and built-in slope/step handling.

#include "continuous/physics/Physics.h"
#include "continuous/core/Assert.h"
#include "continuous/core/Log.h"
#include "continuous/gfx/DebugDraw.h"

#include <Jolt/Jolt.h>
#include <Jolt/RegisterTypes.h>
#include <Jolt/Core/Factory.h>
#include <Jolt/Core/JobSystemThreadPool.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Body/BodyInterface.h>
#include <Jolt/Physics/Character/CharacterVirtual.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/Collision/Shape/RotatedTranslatedShape.h>
#include <Jolt/Physics/Collision/RayCast.h>
#include <Jolt/Physics/Collision/CastResult.h>
#include <Jolt/Physics/Collision/CollisionCollectorImpl.h>
#include <Jolt/Physics/Collision/NarrowPhaseQuery.h>
#include <Jolt/Physics/PhysicsSettings.h>
#include <Jolt/Physics/PhysicsSystem.h>

#include <atomic>
#include <cstdarg>
#include <cstdio>
#include <unordered_map>

namespace cn::physics {

using namespace JPH;
using namespace JPH::literals;

// ------- Layers ------------------------------------------------------------
namespace Layers {
    static constexpr ObjectLayer NON_MOVING = 0;
    static constexpr ObjectLayer MOVING     = 1;
    static constexpr ObjectLayer NUM        = 2;
}
namespace BroadPhaseLayers {
    static constexpr BroadPhaseLayer NON_MOVING(0);
    static constexpr BroadPhaseLayer MOVING(1);
    static constexpr u32             NUM = 2;
}

class ObjectLayerPairFilterImpl final : public ObjectLayerPairFilter {
public:
    bool ShouldCollide(ObjectLayer a, ObjectLayer b) const override {
        switch (a) {
            case Layers::NON_MOVING: return b == Layers::MOVING;
            case Layers::MOVING:     return true;
            default: return false;
        }
    }
};

class BPLayerInterfaceImpl final : public BroadPhaseLayerInterface {
public:
    BPLayerInterfaceImpl() {
        layers_[Layers::NON_MOVING] = BroadPhaseLayers::NON_MOVING;
        layers_[Layers::MOVING]     = BroadPhaseLayers::MOVING;
    }
    u32 GetNumBroadPhaseLayers() const override { return BroadPhaseLayers::NUM; }
    BroadPhaseLayer GetBroadPhaseLayer(ObjectLayer ol) const override { return layers_[ol]; }
#if defined(JPH_EXTERNAL_PROFILE) || defined(JPH_PROFILE_ENABLED)
    const char* GetBroadPhaseLayerName(BroadPhaseLayer) const override { return "?"; }
#endif
private:
    BroadPhaseLayer layers_[Layers::NUM];
};

class ObjectVsBroadPhaseLayerFilterImpl final : public ObjectVsBroadPhaseLayerFilter {
public:
    bool ShouldCollide(ObjectLayer a, BroadPhaseLayer b) const override {
        switch (a) {
            case Layers::NON_MOVING: return b == BroadPhaseLayers::MOVING;
            case Layers::MOVING:     return true;
            default: return false;
        }
    }
};

// ------- Trace + assert callbacks -----------------------------------------
static void trace_impl(const char* fmt, ...) {
    char buf[1024];
    va_list ap; va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    CN_INFO("jolt", "{}", buf);
}
#ifdef JPH_ENABLE_ASSERTS
static bool assert_impl(const char* expr, const char* msg, const char* file, JPH::uint line) {
    CN_ERROR("jolt", "{}:{} {} {}", file, line, expr, msg ? msg : "");
    return true;
}
#endif

// ------- World impl --------------------------------------------------------
struct World::Impl {
    BPLayerInterfaceImpl bp_layers;
    ObjectVsBroadPhaseLayerFilterImpl ovbp_filter;
    ObjectLayerPairFilterImpl olp_filter;

    std::unique_ptr<TempAllocator>         temp_alloc;
    std::unique_ptr<JobSystemThreadPool>   jobs;
    std::unique_ptr<PhysicsSystem>         system;

    std::unordered_map<u32, ecs::Entity>   id_to_owner;
    std::vector<ContactInfo>               contacts;
    std::vector<std::unique_ptr<CharacterVirtual>> characters;

    f32 accumulator = 0.0f;
    static constexpr f32 step = 1.0f / 60.0f;
    bool inited = false;
};

World::World() : impl_(std::make_unique<Impl>()) {}
World::~World() { shutdown(); }

bool World::init() {
    if (impl_->inited) return true;

    RegisterDefaultAllocator();
    Trace = trace_impl;
#ifdef JPH_ENABLE_ASSERTS
    AssertFailed = assert_impl;
#endif
    Factory::sInstance = new Factory();
    RegisterTypes();

    impl_->temp_alloc = std::make_unique<TempAllocatorImpl>(16 * 1024 * 1024);
    impl_->jobs = std::make_unique<JobSystemThreadPool>(cMaxPhysicsJobs, cMaxPhysicsBarriers,
        std::thread::hardware_concurrency() - 1);

    impl_->system = std::make_unique<PhysicsSystem>();
    impl_->system->Init(
        65536, 0, 65536, 10240,
        impl_->bp_layers, impl_->ovbp_filter, impl_->olp_filter);
    impl_->system->SetGravity(Vec3(0, -9.81f, 0));
    impl_->inited = true;
    CN_INFO("physics", "Jolt initialized");
    return true;
}

void World::shutdown() {
    if (!impl_->inited) return;
    impl_->characters.clear();
    impl_->system.reset();
    impl_->jobs.reset();
    impl_->temp_alloc.reset();
    UnregisterTypes();
    delete Factory::sInstance;
    Factory::sInstance = nullptr;
    impl_->inited = false;
}

void World::step(f32 dt) {
    if (!impl_->inited) return;
    impl_->accumulator += dt;
    int max_steps = 4;
    while (impl_->accumulator >= Impl::step && max_steps-- > 0) {
        impl_->system->Update(Impl::step, 1, impl_->temp_alloc.get(), impl_->jobs.get());
        impl_->accumulator -= Impl::step;
    }
}

void World::set_gravity(math::vec3 g) {
    impl_->system->SetGravity(Vec3(g.x, g.y, g.z));
}
math::vec3 World::gravity() const {
    Vec3 g = impl_->system->GetGravity();
    return math::vec3(g.GetX(), g.GetY(), g.GetZ());
}

static MotionType motion_for(bool is_static, bool trigger) {
    (void)trigger;
    return is_static ? EMotionType::Static : EMotionType::Dynamic;
}

u32 World::add_box(ecs::Entity owner, math::vec3 p, math::quat q, math::vec3 he,
                    f32 mass, bool is_static, bool trigger)
{
    if (!impl_->inited) return 0;
    BoxShapeSettings ss(Vec3(he.x, he.y, he.z));
    ss.SetEmbedded();
    ShapeRefC shape = ss.Create().Get();
    BodyCreationSettings bcs(shape,
        RVec3(p.x, p.y, p.z),
        Quat(q.x, q.y, q.z, q.w),
        motion_for(is_static, trigger),
        is_static ? Layers::NON_MOVING : Layers::MOVING);
    bcs.mIsSensor = trigger;
    bcs.mOverrideMassProperties = EOverrideMassProperties::CalculateInertia;
    bcs.mMassPropertiesOverride.mMass = mass;
    BodyInterface& bi = impl_->system->GetBodyInterface();
    BodyID id = bi.CreateAndAddBody(bcs, EActivation::Activate);
    impl_->id_to_owner[id.GetIndexAndSequenceNumber()] = owner;
    return id.GetIndexAndSequenceNumber();
}

u32 World::add_sphere(ecs::Entity owner, math::vec3 p, math::quat q, f32 r,
                      f32 mass, bool is_static, bool trigger) {
    SphereShapeSettings ss(r);
    ss.SetEmbedded();
    ShapeRefC shape = ss.Create().Get();
    BodyCreationSettings bcs(shape, RVec3(p.x, p.y, p.z), Quat(q.x, q.y, q.z, q.w),
        motion_for(is_static, trigger),
        is_static ? Layers::NON_MOVING : Layers::MOVING);
    bcs.mIsSensor = trigger;
    bcs.mOverrideMassProperties = EOverrideMassProperties::CalculateInertia;
    bcs.mMassPropertiesOverride.mMass = mass;
    BodyID id = impl_->system->GetBodyInterface().CreateAndAddBody(bcs, EActivation::Activate);
    impl_->id_to_owner[id.GetIndexAndSequenceNumber()] = owner;
    return id.GetIndexAndSequenceNumber();
}

u32 World::add_capsule(ecs::Entity owner, math::vec3 p, math::quat q, f32 r, f32 h,
                       f32 mass, bool is_static, bool trigger) {
    f32 cyl_h = std::max(0.0f, h - 2.0f * r);
    CapsuleShapeSettings ss(cyl_h * 0.5f, r);
    ss.SetEmbedded();
    ShapeRefC shape = ss.Create().Get();
    BodyCreationSettings bcs(shape, RVec3(p.x, p.y, p.z), Quat(q.x, q.y, q.z, q.w),
        motion_for(is_static, trigger),
        is_static ? Layers::NON_MOVING : Layers::MOVING);
    bcs.mIsSensor = trigger;
    bcs.mOverrideMassProperties = EOverrideMassProperties::CalculateInertia;
    bcs.mMassPropertiesOverride.mMass = mass;
    BodyID id = impl_->system->GetBodyInterface().CreateAndAddBody(bcs, EActivation::Activate);
    impl_->id_to_owner[id.GetIndexAndSequenceNumber()] = owner;
    return id.GetIndexAndSequenceNumber();
}

static BodyID make_id(u32 raw) { return BodyID(raw); }

void World::remove_body(u32 id) {
    if (!impl_->inited || id == 0) return;
    BodyID bid = make_id(id);
    auto& bi = impl_->system->GetBodyInterface();
    bi.RemoveBody(bid);
    bi.DestroyBody(bid);
    impl_->id_to_owner.erase(id);
}

bool World::valid(u32 id) const {
    return id != 0 && impl_->id_to_owner.count(id) > 0;
}

void World::set_position(u32 id, math::vec3 p) {
    if (!valid(id)) return;
    impl_->system->GetBodyInterface().SetPosition(make_id(id),
        RVec3(p.x, p.y, p.z), EActivation::Activate);
}
void World::set_rotation(u32 id, math::quat q) {
    if (!valid(id)) return;
    impl_->system->GetBodyInterface().SetRotation(make_id(id),
        Quat(q.x, q.y, q.z, q.w), EActivation::Activate);
}
void World::set_linear_velocity(u32 id, math::vec3 v) {
    if (!valid(id)) return;
    impl_->system->GetBodyInterface().SetLinearVelocity(make_id(id), Vec3(v.x, v.y, v.z));
}
void World::set_angular_velocity(u32 id, math::vec3 v) {
    if (!valid(id)) return;
    impl_->system->GetBodyInterface().SetAngularVelocity(make_id(id), Vec3(v.x, v.y, v.z));
}
math::vec3 World::linear_velocity(u32 id) const {
    if (!valid(id)) return {};
    Vec3 v = impl_->system->GetBodyInterface().GetLinearVelocity(make_id(id));
    return math::vec3(v.GetX(), v.GetY(), v.GetZ());
}
math::vec3 World::angular_velocity(u32 id) const {
    if (!valid(id)) return {};
    Vec3 v = impl_->system->GetBodyInterface().GetAngularVelocity(make_id(id));
    return math::vec3(v.GetX(), v.GetY(), v.GetZ());
}
void World::add_force(u32 id, math::vec3 f) {
    if (!valid(id)) return;
    impl_->system->GetBodyInterface().AddForce(make_id(id), Vec3(f.x, f.y, f.z));
}
void World::add_impulse(u32 id, math::vec3 imp) {
    if (!valid(id)) return;
    impl_->system->GetBodyInterface().AddImpulse(make_id(id), Vec3(imp.x, imp.y, imp.z));
}
void World::get_transform(u32 id, math::vec3& pos, math::quat& rot) const {
    if (!valid(id)) return;
    RVec3 p; Quat q;
    impl_->system->GetBodyInterface().GetPositionAndRotation(make_id(id), p, q);
    pos = math::vec3(p.GetX(), p.GetY(), p.GetZ());
    rot = math::quat(q.GetW(), q.GetX(), q.GetY(), q.GetZ());
}
void World::set_friction(u32 id, f32 friction) {
    if (!valid(id)) return;
    impl_->system->GetBodyInterface().SetFriction(make_id(id), friction);
}
void World::set_restitution(u32 id, f32 r) {
    if (!valid(id)) return;
    impl_->system->GetBodyInterface().SetRestitution(make_id(id), r);
}

// ---------- Character ------------------------------------------------------
struct CharacterUserData { ecs::Entity owner; };

void* World::create_character(ecs::Entity owner, math::vec3 pos, f32 r, f32 h) {
    if (!impl_->inited) return nullptr;
    f32 cyl_h = std::max(0.0f, h - 2.0f * r);
    Ref<CharacterVirtualSettings> s = new CharacterVirtualSettings();
    s->mShape = RotatedTranslatedShapeSettings(Vec3(0, h * 0.5f, 0), Quat::sIdentity(),
        new CapsuleShape(cyl_h * 0.5f, r)).Create().Get();
    s->mUp = Vec3::sAxisY();
    s->mMaxSlopeAngle = DegreesToRadians(50.0f);
    s->mMass = 70.0f;
    auto* ch = new CharacterVirtual(s, RVec3(pos.x, pos.y, pos.z), Quat::sIdentity(),
                                    impl_->system.get());
    impl_->characters.emplace_back(ch);
    auto* ud = new CharacterUserData{ owner };
    ch->SetUserData(reinterpret_cast<uint64>(ud));
    return ch;
}

void World::destroy_character(void* p) {
    if (!p) return;
    auto* ch = static_cast<CharacterVirtual*>(p);
    if (auto* ud = reinterpret_cast<CharacterUserData*>(ch->GetUserData())) {
        delete ud;
    }
    for (auto it = impl_->characters.begin(); it != impl_->characters.end(); ++it) {
        if (it->get() == ch) { impl_->characters.erase(it); return; }
    }
}

void World::character_set_velocity(void* p, math::vec3 v) {
    if (!p) return;
    auto* ch = static_cast<CharacterVirtual*>(p);
    ch->SetLinearVelocity(Vec3(v.x, v.y, v.z));
}
void World::character_get_transform(void* p, math::vec3& pos, math::quat& rot) const {
    if (!p) return;
    auto* ch = static_cast<CharacterVirtual*>(p);
    RVec3 wp = ch->GetPosition();
    Quat  wr = ch->GetRotation();
    pos = math::vec3(wp.GetX(), wp.GetY(), wp.GetZ());
    rot = math::quat(wr.GetW(), wr.GetX(), wr.GetY(), wr.GetZ());
}
bool World::character_is_grounded(void* p) const {
    if (!p) return false;
    auto* ch = static_cast<CharacterVirtual*>(p);
    return ch->GetGroundState() == CharacterVirtual::EGroundState::OnGround;
}
void World::character_step(void* p, math::vec3 wish, bool wish_jump, f32 dt) {
    if (!p) return;
    auto* ch = static_cast<CharacterVirtual*>(p);
    Vec3 g = impl_->system->GetGravity();
    Vec3 v = ch->GetLinearVelocity();
    // Apply gravity to vertical component
    if (ch->GetGroundState() == CharacterVirtual::EGroundState::OnGround) {
        v.SetY(0);
        if (wish_jump) v.SetY(6.0f);
    } else {
        v += g * dt;
    }
    // Horizontal control.
    v.SetX(wish.x);
    v.SetZ(wish.z);
    if (wish.y != 0) v.SetY(wish.y);
    ch->SetLinearVelocity(v);

    CharacterVirtual::ExtendedUpdateSettings settings;
    ch->ExtendedUpdate(dt, g, settings,
        impl_->system->GetDefaultBroadPhaseLayerFilter(Layers::MOVING),
        impl_->system->GetDefaultLayerFilter(Layers::MOVING),
        {}, {}, *impl_->temp_alloc);
}

// ---------- Raycast --------------------------------------------------------
RaycastHit World::raycast(math::vec3 origin, math::vec3 dir, f32 max_distance) {
    RaycastHit out;
    if (!impl_->inited) return out;

    RRayCast ray(RVec3(origin.x, origin.y, origin.z),
                 Vec3(dir.x, dir.y, dir.z) * max_distance);
    RayCastResult r;
    if (impl_->system->GetNarrowPhaseQuery().CastRay(ray, r,
            impl_->system->GetDefaultBroadPhaseLayerFilter(Layers::MOVING),
            impl_->system->GetDefaultLayerFilter(Layers::MOVING)))
    {
        out.hit = true;
        out.distance = r.mFraction * max_distance;
        Vec3 hp = ray.GetPointOnRay(r.mFraction);
        out.point  = math::vec3(hp.GetX(), hp.GetY(), hp.GetZ());
        // surface normal from body
        BodyLockRead lk(impl_->system->GetBodyLockInterface(), r.mBodyID);
        if (lk.Succeeded()) {
            Vec3 n = lk.GetBody().GetWorldSpaceSurfaceNormal(r.mSubShapeID2,
                                                              RVec3(out.point.x, out.point.y, out.point.z));
            out.normal = math::vec3(n.GetX(), n.GetY(), n.GetZ());
        }
        auto it = impl_->id_to_owner.find(r.mBodyID.GetIndexAndSequenceNumber());
        if (it != impl_->id_to_owner.end()) out.entity = it->second;
    }
    return out;
}

void World::debug_draw(gfx::DebugDraw& dd) {
    if (!impl_->inited) return;
    BodyIDVector ids;
    impl_->system->GetBodies(ids);
    for (auto& id : ids) {
        BodyLockRead lk(impl_->system->GetBodyLockInterface(), id);
        if (!lk.Succeeded()) continue;
        const Body& b = lk.GetBody();
        AABox box = b.GetWorldSpaceBounds();
        dd.aabb({{box.mMin.GetX(), box.mMin.GetY(), box.mMin.GetZ()},
                 {box.mMax.GetX(), box.mMax.GetY(), box.mMax.GetZ()}},
                math::vec4(0.4f, 1.0f, 0.4f, 1.0f));
    }
}

const std::vector<ContactInfo>& World::contacts() const { return impl_->contacts; }

World& global() {
    static World w;
    return w;
}

} // namespace cn::physics
