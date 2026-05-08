#include "continuous/ecs/Archetype.h"
#include "continuous/core/Allocator.h"
#include "continuous/core/Assert.h"
#include "continuous/core/Log.h"

#include <algorithm>
#include <mutex>

namespace cn::ecs {

namespace {
struct Registry {
    std::mutex mu;
    std::vector<ComponentTypeInfo> types;
};

Registry& registry() {
    static Registry r;
    return r;
}
}

u32 register_component_type(ComponentTypeInfo info) {
    auto& r = registry();
    std::lock_guard<std::mutex> lk(r.mu);
    info.id = static_cast<u32>(r.types.size());
    CN_VERIFY(info.id < kMaxComponentTypes, "Too many component types registered");
    r.types.push_back(info);
    return info.id;
}

const ComponentTypeInfo* get_component_info(u32 id) {
    auto& r = registry();
    std::lock_guard<std::mutex> lk(r.mu);
    if (id >= r.types.size()) return nullptr;
    return &r.types[id];
}

u32 component_type_count() {
    auto& r = registry();
    std::lock_guard<std::mutex> lk(r.mu);
    return static_cast<u32>(r.types.size());
}

// ---------------------------------------------------------------------------
// Archetype
// ---------------------------------------------------------------------------
Archetype::Archetype(ComponentMask mask, std::vector<u32> ids)
    : mask_(mask), component_ids_(std::move(ids))
{
    arrays_.resize(component_ids_.size(), nullptr);
    strides_.resize(component_ids_.size(), 0);
    for (usize i = 0; i < component_ids_.size(); ++i) {
        const auto* info = get_component_info(component_ids_[i]);
        strides_[i] = info ? info->size : 0;
    }
}

void Archetype::grow_(u32 new_capacity) {
    for (usize i = 0; i < component_ids_.size(); ++i) {
        const auto* info = get_component_info(component_ids_[i]);
        if (!info) continue;
        u8* new_buf = static_cast<u8*>(mem::aligned_alloc(new_capacity * info->size,
                                                         info->alignment ? info->alignment : 16));
        // Move-construct existing rows into the new buffer.
        for (u32 r = 0; r < row_count_; ++r) {
            u8* src = arrays_[i] + r * info->size;
            u8* dst = new_buf    + r * info->size;
            if (info->move) info->move(dst, src);
            else            std::memcpy(dst, src, info->size);
        }
        if (arrays_[i]) mem::aligned_free(arrays_[i]);
        arrays_[i] = new_buf;
    }
    capacity_ = new_capacity;
    entities_.reserve(new_capacity);
}

u32 Archetype::add_entity(Entity e) {
    if (row_count_ >= capacity_) {
        u32 nc = capacity_ == 0 ? 8u : capacity_ * 2;
        grow_(nc);
    }
    u32 row = row_count_++;
    entities_.push_back(e);
    for (usize i = 0; i < component_ids_.size(); ++i) {
        const auto* info = get_component_info(component_ids_[i]);
        if (info && info->default_construct) {
            info->default_construct(arrays_[i] + row * info->size);
        }
    }
    return row;
}

Entity Archetype::remove_row(u32 row) {
    if (row >= row_count_) return kInvalidEntity;
    u32 last = row_count_ - 1;
    Entity moved = kInvalidEntity;
    if (row != last) {
        for (usize i = 0; i < component_ids_.size(); ++i) {
            const auto* info = get_component_info(component_ids_[i]);
            if (!info) continue;
            u8* dst = arrays_[i] + row  * info->size;
            u8* src = arrays_[i] + last * info->size;
            if (info->destruct) info->destruct(dst);
            if (info->move) info->move(dst, src);
            else            std::memcpy(dst, src, info->size);
        }
        moved = entities_[last];
        entities_[row] = moved;
    } else {
        // destruct components in row
        for (usize i = 0; i < component_ids_.size(); ++i) {
            const auto* info = get_component_info(component_ids_[i]);
            if (!info) continue;
            if (info->destruct) info->destruct(arrays_[i] + row * info->size);
        }
    }
    entities_.pop_back();
    --row_count_;
    return moved;
}

void* Archetype::component_at(u32 cid, u32 row) const {
    auto it = std::lower_bound(component_ids_.begin(), component_ids_.end(), cid);
    if (it == component_ids_.end() || *it != cid) return nullptr;
    usize idx = it - component_ids_.begin();
    return arrays_[idx] + row * strides_[idx];
}

void* Archetype::component_array(u32 cid) const {
    auto it = std::lower_bound(component_ids_.begin(), component_ids_.end(), cid);
    if (it == component_ids_.end() || *it != cid) return nullptr;
    usize idx = it - component_ids_.begin();
    return arrays_[idx];
}

} // namespace cn::ecs
