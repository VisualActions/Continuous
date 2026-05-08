// Continuous Engine - core integer / float / handle typedefs.
#pragma once

#include <cstddef>
#include <cstdint>
#include <limits>

namespace cn {

using u8  = std::uint8_t;
using u16 = std::uint16_t;
using u32 = std::uint32_t;
using u64 = std::uint64_t;

using i8  = std::int8_t;
using i16 = std::int16_t;
using i32 = std::int32_t;
using i64 = std::int64_t;

using f32 = float;
using f64 = double;

using usize = std::size_t;
using isize = std::ptrdiff_t;

inline constexpr u32 kInvalidU32 = std::numeric_limits<u32>::max();
inline constexpr u64 kInvalidU64 = std::numeric_limits<u64>::max();

// Strongly-typed handle template - prevents accidentally mixing handle kinds.
template <typename Tag>
struct Handle {
    u32 idx        = kInvalidU32;
    u32 generation = 0;

    constexpr bool valid() const noexcept { return idx != kInvalidU32; }
    constexpr bool operator==(const Handle& o) const noexcept {
        return idx == o.idx && generation == o.generation;
    }
    constexpr bool operator!=(const Handle& o) const noexcept { return !(*this == o); }
};

// Handle hashing helper (place into std::unordered_map by writing your own
// std::hash specialization in the consuming module).
template <typename Tag>
constexpr u64 handle_hash(Handle<Tag> h) noexcept {
    return (static_cast<u64>(h.generation) << 32) | static_cast<u64>(h.idx);
}

} // namespace cn
