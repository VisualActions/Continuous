#include "continuous/ecs/World.h"
#include "continuous/core/Assert.h"

#include <algorithm>

namespace cn::ecs {

bool World::alive(Entity e) const {
    if (e.idx >= records_.size()) return false;
    return records_[e.idx].alive && records_[e.idx].generation == e.generation;
}

Entity World::create() {
    Entity e;
    if (!free_list_.empty()) {
        u32 idx = free_list_.back();
        free_list_.pop_back();
        records_[idx].alive = true;
        records_[idx].archetype = kInvalidU32;
        records_[idx].row = kInvalidU32;
        e.idx = idx;
        e.generation = records_[idx].generation;
    } else {
        e.idx = static_cast<u32>(records_.size());
        e.generation = 1;
        EntityRecord rec;
        rec.alive = true;
        rec.generation = 1;
        records_.push_back(rec);
    }
    ++live_;
    return e;
}

void World::destroy(Entity e) {
    if (!alive(e)) return;
    auto& rec = records_[e.idx];
    if (rec.archetype != kInvalidU32) {
        Entity moved = archetypes_[rec.archetype]->remove_row(rec.row);
        if (moved.valid()) records_[moved.idx].row = rec.row;
    }
    rec.alive = false;
    rec.archetype = kInvalidU32;
    rec.row = kInvalidU32;
    rec.generation++;
    free_list_.push_back(e.idx);
    --live_;
}

Archetype& World::get_or_create_(const ComponentMask& m) {
    auto it = archetype_index_.find(m);
    if (it != archetype_index_.end()) return *archetypes_[it->second];

    std::vector<u32> ids;
    ids.reserve(m.count());
    for (u32 i = 0; i < kMaxComponentTypes; ++i) if (m.test(i)) ids.push_back(i);
    auto a = std::make_unique<Archetype>(m, std::move(ids));
    u32 index = static_cast<u32>(archetypes_.size());
    archetype_index_.emplace(m, index);
    archetypes_.push_back(std::move(a));
    return *archetypes_.back();
}

u32 World::entity_count() const { return live_; }

} // namespace cn::ecs
