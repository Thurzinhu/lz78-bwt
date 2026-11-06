#pragma once

#include <factor.hpp>
#include <indexes/index_interface.hpp>

#include <cstdint>
#include <optional>

struct DictionaryMemoryStats {
    uint64_t allocated_bytes;
    uint64_t peak_allocated_bytes;
    uint64_t buckets;
    double load_factor;
    double max_load_factor;
};

class LZ78Dictionary {
public:
    virtual uint64_t interval_search(Interval interval, uint64_t length) const = 0;
    virtual bool interval_insert(Interval interval, Factor factor) = 0;
    virtual ~LZ78Dictionary() = default;
    virtual uint64_t number_of_dictionary_entries() = 0;
    virtual std::optional<DictionaryMemoryStats> memory_stats() const {
        return std::nullopt;
    }
};
