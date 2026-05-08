// ECS World - the entity registry. Entities live with a generation counter so
// stale handles compare unequal. Every entity is mapped to (archetype, row);
// adding/removing a component re-creates that entity in a different archetype.
#pragma once

#include "continuous/ecs/Archetype.h"
#include "continuous/ecs/Entity.h"
#include "continuous/core/Macros.h"

#include <functional>
#include <memory>
#include <unordered_map>
#include <vector>

namespace cn::ecs {

class CN_API World {
public:
    World() = default;
    ~World() = default;
    CN_NONCOPYABLE(World);

    Entity create();
    void   destroy(Entity e);
    bool   alive(Entity e) const;

    template <typename T, typename... Args>
    T& add(Entity e, Args&&... args);

    template <typename T>
    void remove(Entity e);

    template <typename T>
    T* get(Entity e);

    template <typename T>
    const T* get(Entity e) const;

    template <typename T>
    bool has(Entity e) const;

    // Iterate entities matching a component mask. Body signature:
    //   void(Entity, T1&, T2&, ...)
    template <typename... Cs, typename Fn>
    void each(Fn&& fn);

    u32 entity_count() const;

    // Visit every archetype + matching mask (used by serialization).
    template <typename Fn>
    void visit_archetypes(Fn fn) const {
        for (auto& a : archetypes_) fn(*a);
    }

    // Visit individual entities (slow path; used by inspector / scene save).
    template <typename Fn>
    void each_entity(Fn fn) const;

private:
    struct EntityRecord {
        u32 archetype = kInvalidU32;
        u32 row       = kInvalidU32;
        u32 generation = 0;
        bool alive    = false;
    };

    Archetype& get_or_create_(const ComponentMask& m);
    void       move_entity_  (Entity e, u32 new_arch);

    std::vector<EntityRecord>                records_;
    std::vector<u32>                         free_list_;
    std::vector<std::unique_ptr<Archetype>>  archetypes_;
    std::unordered_map<ComponentMask, u32>   archetype_index_;
    u32                                      live_ = 0;
};

} // namespace cn::ecs

// std::hash for std::bitset (so unordered_map<ComponentMask, ...> works).
namespace std {
template <>
struct hash<std::bitset<cn::ecs::kMaxComponentTypes>> {
    cn::usize operator()(const std::bitset<cn::ecs::kMaxComponentTypes>& b) const noexcept {
        cn::usize h = 0;
        for (cn::usize i = 0; i < b.size(); i += 64) {
            cn::u64 word = 0;
            for (cn::usize bit = 0; bit < 64 && (i + bit) < b.size(); ++bit)
                if (b.test(i + bit)) word |= (1ull << bit);
            h ^= std::hash<cn::u64>{}(word) + 0x9e3779b9ull + (h << 6) + (h >> 2);
        }
        return h;
    }
};
}

// Templated method definitions.
namespace cn::ecs {

template <typename T, typename... Args>
T& World::add(Entity e, Args&&... args) {
    CN_ASSERT(alive(e), "add() on dead entity");
    auto& rec = records_[e.idx];
    u32 cid = component_id<T>();

    Archetype* old_arch = (rec.archetype != kInvalidU32) ? archetypes_[rec.archetype].get() : nullptr;
    ComponentMask new_mask = old_arch ? old_arch->mask() : ComponentMask{};
    new_mask.set(cid, true);

    Archetype& new_arch = get_or_create_(new_mask);

    u32 new_row = new_arch.add_entity(e);
    // Copy over existing components from old archetype (move).
    if (old_arch) {
        for (u32 oid : old_arch->component_ids()) {
            const auto* info = get_component_info(oid);
            if (!info) continue;
            void* dst = new_arch.component_at(oid, new_row);
            void* src = old_arch->component_at(oid, rec.row);
            if (info->destruct) info->destruct(dst); // drop default
            if (info->move) info->move(dst, src); else std::memcpy(dst, src, info->size);
        }
        Entity moved = old_arch->remove_row(rec.row);
        if (moved.valid()) records_[moved.idx].row = rec.row;
    }
    // Construct the new component in place.
    T* slot = static_cast<T*>(new_arch.component_at(cid, new_row));
    const auto* info = get_component_info(cid);
    if (info && info->destruct) info->destruct(slot);
    new (slot) T(std::forward<Args>(args)...);

    rec.archetype = static_cast<u32>(&new_arch - archetypes_[0].get());
    // Recompute the archetype index using lookup, since pointer arithmetic
    // assumes contiguous storage which std::vector<unique_ptr> does provide.
    rec.archetype = archetype_index_[new_mask];
    rec.row = new_row;
    return *slot;
}

template <typename T>
void World::remove(Entity e) {
    if (!alive(e)) return;
    auto& rec = records_[e.idx];
    if (rec.archetype == kInvalidU32) return;
    u32 cid = component_id<T>();
    Archetype* old_arch = archetypes_[rec.archetype].get();
    if (!old_arch->mask().test(cid)) return;

    ComponentMask new_mask = old_arch->mask();
    new_mask.set(cid, false);
    Archetype* new_arch = nullptr;
    u32 new_row = kInvalidU32;
    if (new_mask.any()) {
        new_arch = &get_or_create_(new_mask);
        new_row = new_arch->add_entity(e);
        // Move components except the removed one.
        for (u32 oid : old_arch->component_ids()) {
            if (oid == cid) continue;
            const auto* info = get_component_info(oid);
            if (!info) continue;
            void* dst = new_arch->component_at(oid, new_row);
            void* src = old_arch->component_at(oid, rec.row);
            if (info->destruct) info->destruct(dst);
            if (info->move) info->move(dst, src); else std::memcpy(dst, src, info->size);
        }
    }
    Entity moved = old_arch->remove_row(rec.row);
    if (moved.valid()) records_[moved.idx].row = rec.row;
    if (new_arch) {
        rec.archetype = archetype_index_[new_mask];
        rec.row = new_row;
    } else {
        rec.archetype = kInvalidU32;
        rec.row = kInvalidU32;
    }
}

template <typename T>
T* World::get(Entity e) {
    if (!alive(e)) return nullptr;
    auto& rec = records_[e.idx];
    if (rec.archetype == kInvalidU32) return nullptr;
    return static_cast<T*>(archetypes_[rec.archetype]->component_at(component_id<T>(), rec.row));
}

template <typename T>
const T* World::get(Entity e) const {
    if (!alive(e)) return nullptr;
    auto& rec = records_[e.idx];
    if (rec.archetype == kInvalidU32) return nullptr;
    return static_cast<const T*>(archetypes_[rec.archetype]->component_at(component_id<T>(), rec.row));
}

template <typename T>
bool World::has(Entity e) const {
    if (!alive(e)) return false;
    auto& rec = records_[e.idx];
    if (rec.archetype == kInvalidU32) return false;
    return archetypes_[rec.archetype]->mask().test(component_id<T>());
}

template <typename... Cs, typename Fn>
void World::each(Fn&& fn) {
    ComponentMask want;
    u32 cids[] = { component_id<Cs>()... };
    for (u32 c : cids) want.set(c, true);
    for (auto& a : archetypes_) {
        if ((a->mask() & want) != want) continue;
        for (u32 r = 0; r < a->row_count(); ++r) {
            Entity e = a->entities()[r];
            fn(e, *static_cast<Cs*>(a->component_at(component_id<Cs>(), r))...);
        }
    }
}

template <typename Fn>
void World::each_entity(Fn fn) const {
    for (auto& a : archetypes_) {
        for (u32 r = 0; r < a->row_count(); ++r) fn(a->entities()[r]);
    }
}

} // namespace cn::ecs
