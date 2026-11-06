#pragma once

#include <algorithm>
#include <cstdint>
#include <memory>

struct AllocationStats {
    uint64_t allocated_bytes = 0;
    uint64_t peak_allocated_bytes = 0;
};

// All rebound allocators of one container share the same counters.
// Counts requested bytes only, excluding allocator overhead/fragmentation.
template <typename T>
class CountingAllocator {
    template <typename> friend class CountingAllocator;
    std::shared_ptr<AllocationStats> stats_;

public:
    using value_type = T;

    CountingAllocator() : stats_(std::make_shared<AllocationStats>()) {}
    explicit CountingAllocator(std::shared_ptr<AllocationStats> stats)
        : stats_(std::move(stats)) {}

    template <typename U>
    CountingAllocator(const CountingAllocator<U>& other) noexcept
        : stats_(other.stats_) {}

    T* allocate(std::size_t count) {
        T* pointer = std::allocator<T>{}.allocate(count);
        stats_->allocated_bytes += count * sizeof(T);
        stats_->peak_allocated_bytes = std::max(
            stats_->peak_allocated_bytes, stats_->allocated_bytes
        );
        return pointer;
    }

    void deallocate(T* pointer, std::size_t count) noexcept {
        stats_->allocated_bytes -= count * sizeof(T);
        std::allocator<T>{}.deallocate(pointer, count);
    }

    template <typename U>
    bool operator==(const CountingAllocator<U>& other) const noexcept {
        return stats_ == other.stats_;
    }

    template <typename U>
    bool operator!=(const CountingAllocator<U>& other) const noexcept {
        return !(*this == other);
    }
};
