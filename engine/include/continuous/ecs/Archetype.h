// Archetype: storage for entities sharing the same set of component types.
// Entities are stored in parallel arrays, one per component type, plus a
// vector of Entity ids. This gives sequential access for system iteration.
#pragma once

#include "continuous/core/Macros.h"
#include "continuous/core/Types.h"
#include "continuous/ecs/Entity.h"

#include <bitset>
#include <cstring>
#include <functional>
#include <typeindex>
#include <unordered_map>
#include <vector>

namespace cn::ecs {

// Component type id - unique numeric tag. Capped to 64 distinct components
// (more than enough for our needs; can be widened later).
constexpr u32 kMaxComponentTypes = 64;
using ComponentMask = std::bitset<kMaxComponentTypes>;

struct ComponentTypeInfo {
    u32         id          = 0;
    const char* name        = "?";
    usize       size        = 0;
    usize       alignment   = 0;
    void      (*default_construct)(void*) = nullptr;
    void      (*destruct)(void*)          = nullptr;
    void      (*move)(void* dst, void* src) = nullptr;
};

CN_API u32 register_component_type(ComponentTypeInfo info);
CN_API const ComponentTypeInfo* get_component_info(u32 id);
CN_API u32 component_type_count();

// Per-component-type id (allocated lazily once at first use).
template <typename T>
CN_API u32 component_id();

class CN_API Archetype {
public:
    Archetype() = default;
    Archetype(ComponentMask mask, std::vector<u32> component_ids);

    // Adds an entity slot, returns the row index. The component data is left
    // default-constructed.
    u32 add_entity(Entity e);

    // Move-out the components for the given row, then swap-remove with last.
    // Returns the entity that was moved into the now-empty row (or kInvalid).
    Entity remove_row(u32 row);

    void* component_at(u32 component_id, u32 row) const;
    template <typename T> T* get(u32 row) {
        u32 cid = component_id<T>();
        return static_cast<T*>(component_at(cid, row));
    }

    u32 row_count() const { return row_count_; }
    const std::vector<Entity>& entities() const { return entities_; }
    const ComponentMask& mask() const { return mask_; }
    const std::vector<u32>& component_ids() const { return component_ids_; }

    // Direct access for systems iterating one type.
    void* component_array(u32 component_id) const;

private:
    void grow_(u32 new_capacity);

    ComponentMask           mask_;
    std::vector<u32>        component_ids_;       // sorted ascending
    std::vector<u8*>        arrays_;              // one buffer per component, parallel
    std::vector<usize>      strides_;             // sizeof(component) per type
    std::vector<Entity>     entities_;
    u32                     row_count_ = 0;
    u32                     capacity_  = 0;
};

} // namespace cn::ecs

// Inline component_id implementation.
namespace cn::ecs {

namespace detail {
template <typename T>
struct ComponentIdHolder {
    static u32 value;
};
template <typename T>
u32 ComponentIdHolder<T>::value = ([] () {
    ComponentTypeInfo info;
    info.id = 0;
    info.name = typeid(T).name();
    info.size = sizeof(T);
    info.alignment = alignof(T);
    info.default_construct = [](void* p) { new (p) T(); };
    info.destruct          = [](void* p) { static_cast<T*>(p)->~T(); };
    info.move              = [](void* dst, void* src) {
        new (dst) T(std::move(*static_cast<T*>(src)));
        static_cast<T*>(src)->~T();
    };
    return register_component_type(info);
})();
} // namespace detail

template <typename T>
inline u32 component_id() { return detail::ComponentIdHolder<T>::value; }

} // namespace cn::ecs
