// Continuous Engine - custom allocators.
//
// Linear (bump) allocator: O(1) alloc, no individual free, reset wipes all.
//   Used for per-frame scratch and asset cooking temp.
// Stack allocator: like linear but with markers - free back to a marker.
//   Used for nested scopes that need temporary space.
// Pool allocator: fixed-size blocks, free-list. Used for ECS chunk slabs and
//   transient particle/audio voice slots.
// Frame allocator: double-buffered linear, swap each frame. Used for render
//   command lists.
//
// Tracking: a global allocation registry counts live bytes per category for
// the leak report at shutdown. The registry is opt-in via track().
#pragma once

#include "continuous/core/Macros.h"
#include "continuous/core/Types.h"

#include <atomic>
#include <cstddef>
#include <mutex>
#include <string>
#include <unordered_map>

namespace cn::mem {

// ----------------------------------------------------------------------------
// Tracking registry.
// ----------------------------------------------------------------------------
struct CategoryStats {
    std::atomic<i64> live_bytes{0};
    std::atomic<i64> peak_bytes{0};
    std::atomic<i64> alloc_count{0};
    std::atomic<i64> free_count{0};
};

CN_API CategoryStats& category(const char* name);
CN_API void track_alloc(const char* category, isize bytes);
CN_API void track_free (const char* category, isize bytes);
CN_API void dump_leaks(); // Called on engine shutdown.

// ----------------------------------------------------------------------------
// Aligned malloc/free (Win32 _aligned_malloc).
// ----------------------------------------------------------------------------
CN_API void* aligned_alloc(usize size, usize align);
CN_API void  aligned_free (void* ptr);

// ----------------------------------------------------------------------------
// Linear allocator.
// ----------------------------------------------------------------------------
class CN_API LinearAllocator {
public:
    LinearAllocator() = default;
    LinearAllocator(usize capacity, const char* category = "linear");
    ~LinearAllocator();
    CN_NONCOPYABLE(LinearAllocator);
    LinearAllocator(LinearAllocator&& other) noexcept;
    LinearAllocator& operator=(LinearAllocator&& other) noexcept;

    void* allocate(usize size, usize align = alignof(std::max_align_t));
    template <typename T, typename... A>
    T* construct(A&&... a) {
        void* p = allocate(sizeof(T), alignof(T));
        return new (p) T(std::forward<A>(a)...);
    }

    void  reset();
    usize used()     const noexcept { return offset_; }
    usize capacity() const noexcept { return capacity_; }

private:
    void release_();
    u8*         buffer_   = nullptr;
    usize       capacity_ = 0;
    usize       offset_   = 0;
    const char* category_ = "linear";
};

// ----------------------------------------------------------------------------
// Stack allocator.
// ----------------------------------------------------------------------------
class CN_API StackAllocator {
public:
    using Marker = usize;

    StackAllocator() = default;
    StackAllocator(usize capacity, const char* category = "stack");
    ~StackAllocator();
    CN_NONCOPYABLE(StackAllocator);

    void*  allocate(usize size, usize align = alignof(std::max_align_t));
    Marker mark() const noexcept { return offset_; }
    void   release(Marker m) noexcept { offset_ = m; }
    void   reset() noexcept { offset_ = 0; }

private:
    u8*         buffer_   = nullptr;
    usize       capacity_ = 0;
    usize       offset_   = 0;
    const char* category_ = "stack";
};

// ----------------------------------------------------------------------------
// Pool allocator.
// ----------------------------------------------------------------------------
class CN_API PoolAllocator {
public:
    PoolAllocator() = default;
    PoolAllocator(usize block_size, usize block_count, usize align = alignof(std::max_align_t),
                  const char* category = "pool");
    ~PoolAllocator();
    CN_NONCOPYABLE(PoolAllocator);

    void* allocate();
    void  free(void* ptr);

    usize block_size()  const noexcept { return block_size_; }
    usize block_count() const noexcept { return block_count_; }
    usize live_count()  const noexcept { return live_count_; }

private:
    u8*         buffer_      = nullptr;
    void*       free_head_   = nullptr;
    usize       block_size_  = 0;
    usize       block_count_ = 0;
    usize       live_count_  = 0;
    const char* category_    = "pool";
};

// ----------------------------------------------------------------------------
// Frame allocator (double buffered linear).
// ----------------------------------------------------------------------------
class CN_API FrameAllocator {
public:
    FrameAllocator() = default;
    FrameAllocator(usize per_frame_capacity, const char* category = "frame");
    void* allocate(usize size, usize align = alignof(std::max_align_t));
    void  flip(); // call once per frame
private:
    LinearAllocator buffers_[2];
    int             current_ = 0;
};

} // namespace cn::mem
