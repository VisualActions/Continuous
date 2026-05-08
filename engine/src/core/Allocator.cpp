#include "continuous/core/Allocator.h"
#include "continuous/core/Assert.h"
#include "continuous/core/Log.h"

#include <cstdlib>
#include <cstring>
#include <unordered_map>
#include <vector>

#if defined(_WIN32)
    #include <malloc.h>
#endif

namespace cn::mem {
namespace {

struct Registry {
    std::mutex mu;
    std::unordered_map<std::string, CategoryStats*> map;
};

Registry& registry() {
    static Registry r;
    return r;
}

usize align_up(usize n, usize a) {
    return (n + (a - 1)) & ~(a - 1);
}

} // namespace

CategoryStats& category(const char* name) {
    auto& r = registry();
    std::lock_guard<std::mutex> lk(r.mu);
    auto it = r.map.find(name);
    if (it == r.map.end()) {
        auto* s = new CategoryStats(); // intentional leak - lives for program duration
        it = r.map.emplace(name, s).first;
    }
    return *it->second;
}

void track_alloc(const char* category_name, isize bytes) {
    auto& s = category(category_name);
    i64 live = s.live_bytes.fetch_add(bytes, std::memory_order_relaxed) + bytes;
    i64 peak = s.peak_bytes.load(std::memory_order_relaxed);
    while (live > peak && !s.peak_bytes.compare_exchange_weak(peak, live, std::memory_order_relaxed)) {}
    s.alloc_count.fetch_add(1, std::memory_order_relaxed);
}

void track_free(const char* category_name, isize bytes) {
    auto& s = category(category_name);
    s.live_bytes.fetch_sub(bytes, std::memory_order_relaxed);
    s.free_count.fetch_add(1, std::memory_order_relaxed);
}

void dump_leaks() {
    auto& r = registry();
    std::lock_guard<std::mutex> lk(r.mu);
    bool any = false;
    for (auto& [name, s] : r.map) {
        i64 live = s->live_bytes.load();
        if (live != 0) {
            any = true;
            CN_WARN("mem", "LEAK '{}': {} bytes live, {} allocs / {} frees, peak {} bytes",
                    name, live, s->alloc_count.load(), s->free_count.load(), s->peak_bytes.load());
        } else {
            CN_INFO("mem", "OK   '{}': {} allocs / {} frees, peak {} bytes",
                    name, s->alloc_count.load(), s->free_count.load(), s->peak_bytes.load());
        }
    }
    if (!any) CN_INFO("mem", "no leaks detected");
}

void* aligned_alloc(usize size, usize align) {
#if defined(_WIN32)
    return _aligned_malloc(size, align);
#else
    void* p = nullptr;
    if (posix_memalign(&p, align, size) != 0) return nullptr;
    return p;
#endif
}

void aligned_free(void* ptr) {
#if defined(_WIN32)
    _aligned_free(ptr);
#else
    std::free(ptr);
#endif
}

// ----------------------------------------------------------------------------
// LinearAllocator
// ----------------------------------------------------------------------------
LinearAllocator::LinearAllocator(usize capacity, const char* cat)
    : capacity_(capacity), category_(cat) {
    buffer_ = static_cast<u8*>(aligned_alloc(capacity, 64));
    CN_VERIFY(buffer_, "linear allocator OOM (capacity={})", capacity);
    track_alloc(category_, static_cast<isize>(capacity));
}

LinearAllocator::~LinearAllocator() { release_(); }

LinearAllocator::LinearAllocator(LinearAllocator&& o) noexcept
    : buffer_(o.buffer_), capacity_(o.capacity_), offset_(o.offset_), category_(o.category_) {
    o.buffer_ = nullptr; o.capacity_ = 0; o.offset_ = 0;
}

LinearAllocator& LinearAllocator::operator=(LinearAllocator&& o) noexcept {
    if (this != &o) {
        release_();
        buffer_ = o.buffer_; capacity_ = o.capacity_; offset_ = o.offset_; category_ = o.category_;
        o.buffer_ = nullptr; o.capacity_ = 0; o.offset_ = 0;
    }
    return *this;
}

void LinearAllocator::release_() {
    if (buffer_) {
        track_free(category_, static_cast<isize>(capacity_));
        aligned_free(buffer_);
        buffer_ = nullptr; capacity_ = 0; offset_ = 0;
    }
}

void* LinearAllocator::allocate(usize size, usize align) {
    usize aligned = align_up(offset_, align);
    if (aligned + size > capacity_) return nullptr;
    void* p = buffer_ + aligned;
    offset_ = aligned + size;
    return p;
}

void LinearAllocator::reset() { offset_ = 0; }

// ----------------------------------------------------------------------------
// StackAllocator
// ----------------------------------------------------------------------------
StackAllocator::StackAllocator(usize capacity, const char* cat)
    : capacity_(capacity), category_(cat) {
    buffer_ = static_cast<u8*>(aligned_alloc(capacity, 64));
    CN_VERIFY(buffer_, "stack allocator OOM");
    track_alloc(category_, static_cast<isize>(capacity));
}

StackAllocator::~StackAllocator() {
    if (buffer_) {
        track_free(category_, static_cast<isize>(capacity_));
        aligned_free(buffer_);
    }
}

void* StackAllocator::allocate(usize size, usize align) {
    usize aligned = align_up(offset_, align);
    if (aligned + size > capacity_) return nullptr;
    void* p = buffer_ + aligned;
    offset_ = aligned + size;
    return p;
}

// ----------------------------------------------------------------------------
// PoolAllocator
// ----------------------------------------------------------------------------
PoolAllocator::PoolAllocator(usize block_size, usize block_count, usize align, const char* cat)
    : block_size_(align_up(block_size < sizeof(void*) ? sizeof(void*) : block_size, align)),
      block_count_(block_count), category_(cat) {
    usize total = block_size_ * block_count_;
    buffer_ = static_cast<u8*>(aligned_alloc(total, align));
    CN_VERIFY(buffer_, "pool allocator OOM");
    track_alloc(category_, static_cast<isize>(total));
    // Build free list.
    free_head_ = buffer_;
    for (usize i = 0; i + 1 < block_count_; ++i) {
        u8* cur  = buffer_ + i * block_size_;
        u8* next = buffer_ + (i + 1) * block_size_;
        std::memcpy(cur, &next, sizeof(void*));
    }
    void* term = nullptr;
    std::memcpy(buffer_ + (block_count_ - 1) * block_size_, &term, sizeof(void*));
}

PoolAllocator::~PoolAllocator() {
    if (buffer_) {
        track_free(category_, static_cast<isize>(block_size_ * block_count_));
        aligned_free(buffer_);
    }
}

void* PoolAllocator::allocate() {
    if (!free_head_) return nullptr;
    void* p = free_head_;
    void* next;
    std::memcpy(&next, free_head_, sizeof(void*));
    free_head_ = next;
    ++live_count_;
    return p;
}

void PoolAllocator::free(void* ptr) {
    if (!ptr) return;
    std::memcpy(ptr, &free_head_, sizeof(void*));
    free_head_ = ptr;
    --live_count_;
}

// ----------------------------------------------------------------------------
// FrameAllocator
// ----------------------------------------------------------------------------
FrameAllocator::FrameAllocator(usize per_frame_capacity, const char* cat) {
    buffers_[0] = LinearAllocator(per_frame_capacity, cat);
    buffers_[1] = LinearAllocator(per_frame_capacity, cat);
}

void* FrameAllocator::allocate(usize size, usize align) {
    return buffers_[current_].allocate(size, align);
}

void FrameAllocator::flip() {
    current_ = 1 - current_;
    buffers_[current_].reset();
}

} // namespace cn::mem
