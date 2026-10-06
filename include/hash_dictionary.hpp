#pragma once

#include <lz78_dictionary.hpp>

#include <cstdint>
#include <cstddef>
#include <string>
#include <unordered_map>
#include <utility>

class HashDictionary : public LZ78Dictionary {
private:
    static constexpr uint64_t NONE = 0;

    using IntervalKey = std::pair<uint64_t, uint64_t>;
    using LookupKey = std::pair<IntervalKey, uint64_t>;

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

    uint64_t n;
    std::unordered_map<LookupKey, uint64_t, PairHash> entries_by_interval_and_length;

public:
    explicit HashDictionary(const std::string& text)
        : n(text.size() + 1) {}

    explicit HashDictionary(uint64_t text_size)
        : n(text_size + 1) {}

    uint64_t interval_search(Interval interval, uint64_t length) const override {
        if (interval.empty() || interval.sp >= n) {
            return NONE;
        }

        const LookupKey key{{interval.sp, interval.ep}, length};
        auto it = entries_by_interval_and_length.find(key);
        return it == entries_by_interval_and_length.end() ? NONE : it->second;
    }

    bool interval_insert(Interval interval, Factor factor) override {
        if (interval.empty() || interval.sp >= n) {
            return false;
        }

        if (interval.ep >= n) {
            interval.ep = n - 1;
        }

        entries_by_interval_and_length[{{interval.sp, interval.ep}, factor.length}] = factor.id;

        return true;
    }

    uint64_t number_of_dictionary_entries(){
        return entries_by_interval_and_length.size();
    }
};
