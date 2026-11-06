#pragma once

#include <dictionaries/lz78_dictionary.hpp>
#include <utils/counting_allocator.hpp>

#include <cstdint>
#include <cstddef>
#include <limits>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>

class HashDictionary : public LZ78Dictionary {
private:
    static constexpr uint32_t NONE = 0;

    using IntervalKey = std::pair<uint32_t, uint32_t>;
    using LookupKey = std::pair<IntervalKey, uint32_t>;

    struct PairHash {
        static uint64_t mix(uint64_t value) noexcept {
            value += 0x9e3779b97f4a7c15ULL;
            value = (value ^ (value >> 30)) * 0xbf58476d1ce4e5b9ULL;
            value = (value ^ (value >> 27)) * 0x94d049bb133111ebULL;
            return value ^ (value >> 31);
        }

        static std::size_t combine(uint64_t lhs, uint64_t rhs) noexcept {
            const uint64_t lhs_hash = mix(lhs);
            const uint64_t rhs_hash = mix(rhs);
            return static_cast<std::size_t>(
                lhs_hash ^ (rhs_hash + 0x9e3779b97f4a7c15ULL + (lhs_hash << 6) + (lhs_hash >> 2))
            );
        }

        std::size_t operator()(const IntervalKey& key) const noexcept {
            return combine(key.first, key.second);
        }

        std::size_t operator()(const LookupKey& key) const noexcept {
            return combine((*this)(key.first), key.second);
        }
    };

    static uint32_t checked_value(uint64_t value, const char* field) {
        if (value > std::numeric_limits<uint32_t>::max()) {
            throw std::overflow_error(
                std::string("HashDictionary: ") + field +
                " exceeds the 32-bit limit (4294967295)"
            );
        }
        return static_cast<uint32_t>(value);
    }

    static LookupKey checked_key(Interval interval, uint64_t length) {
        return {{checked_value(interval.sp, "interval.sp"),
                 checked_value(interval.ep, "interval.ep")},
                checked_value(length, "factor length")};
    }

    // Includes the terminator: UINT32_MAX text bytes need 2^32 index positions.
    uint64_t n;
    using Entry = std::pair<const LookupKey, uint32_t>;
    std::shared_ptr<AllocationStats> allocations_ = std::make_shared<AllocationStats>();
    std::unordered_map<LookupKey, uint32_t, PairHash,
        std::equal_to<LookupKey>, CountingAllocator<Entry>> entries_by_interval_and_length{
            CountingAllocator<Entry>(allocations_)
        };

public:
    explicit HashDictionary(const std::string& text)
        : HashDictionary(text.size()) {}

    explicit HashDictionary(uint64_t text_size)
        : n(uint64_t{checked_value(text_size, "text size")} + 1) {}

    HashDictionary(const HashDictionary& other)
        : n(other.n),
          entries_by_interval_and_length(other.entries_by_interval_and_length,
              CountingAllocator<Entry>(allocations_)) {}

    HashDictionary& operator=(const HashDictionary& other) {
        if (this != &other) {
            n = other.n;
            // The map retains this dictionary's nonpropagating allocator.
            entries_by_interval_and_length = other.entries_by_interval_and_length;
        }
        return *this;
    }

    uint64_t interval_search(Interval interval, uint64_t length) const override {
        if (interval.empty()) {
            return NONE;
        }

        const LookupKey key = checked_key(interval, length);
        if (interval.sp >= n) {
            return NONE;
        }

        auto it = entries_by_interval_and_length.find(key);
        return it == entries_by_interval_and_length.end() ? NONE : it->second;
    }

    bool interval_insert(Interval interval, Factor factor) override {
        if (interval.empty()) {
            return false;
        }

        LookupKey key = checked_key(interval, factor.length);
        const uint32_t id = checked_value(factor.id, "factor id");
        if (interval.sp >= n) {
            return false;
        }

        if (interval.ep >= n) {
            key.first.second = static_cast<uint32_t>(n - 1);
        }

        entries_by_interval_and_length[key] = id;

        return true;
    }

    uint64_t number_of_dictionary_entries(){
        return entries_by_interval_and_length.size();
    }

    std::optional<DictionaryMemoryStats> memory_stats() const override {
        return DictionaryMemoryStats{
            allocations_->allocated_bytes,
            allocations_->peak_allocated_bytes,
            entries_by_interval_and_length.bucket_count(),
            entries_by_interval_and_length.load_factor(),
            entries_by_interval_and_length.max_load_factor()
        };
    }
};
