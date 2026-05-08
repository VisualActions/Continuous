// Scene = ECS world + scene graph hierarchy + transform propagation +
// JSON/binary serialization.
#pragma once

#include "continuous/ecs/World.h"
#include "continuous/scene/Components.h"

#include <filesystem>
#include <string>
#include <unordered_set>

namespace cn::scene {

class CN_API Scene {
public:
    Scene() = default;
    ~Scene() = default;
    CN_NONCOPYABLE(Scene);

    ecs::World& world() { return world_; }
    const ecs::World& world() const { return world_; }

    // Create entity with NameComponent + TransformComponent + HierarchyComponent.
    ecs::Entity create_entity(std::string name = {});
    void        destroy_entity(ecs::Entity e);

    // Hierarchy.
    void set_parent(ecs::Entity child, ecs::Entity parent);
    ecs::Entity parent_of(ecs::Entity e) const;
    std::vector<ecs::Entity> children_of(ecs::Entity e) const;

    // Mark transform dirty + recursively dirty children.
    void mark_dirty(ecs::Entity e);
    // Update world matrices for any entity whose local changed (or any of its
    // ancestors). Cheap when nothing changed.
    void update_world_transforms();

    // Save / load.
    bool save_json  (const std::filesystem::path& p) const;
    bool load_json  (const std::filesystem::path& p);
    bool save_binary(const std::filesystem::path& p) const;
    bool load_binary(const std::filesystem::path& p);

    // For systems that want a stable iteration order.
    void each_entity(std::function<void(ecs::Entity)> fn) const;

    void clear();

private:
    void propagate_(ecs::Entity e, const math::mat4& parent_world);

    ecs::World world_;
};

} // namespace cn::scene
