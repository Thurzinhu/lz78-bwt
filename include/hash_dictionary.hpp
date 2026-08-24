#pragma once

#include <lz78_dictionary.hpp>

#include <cstdint>
#include <map>
#include <string>
#include <unordered_map>

class HashDictionary : public LZ78Dictionary {
private:
    static constexpr uint64_t NONE = 0;

    struct Entry {
        uint64_t ep;
        uint64_t id;
    };

    uint64_t n;
    std::unordered_map<uint64_t, std::map<uint64_t, Entry>> intervals_by_length;

public:
    explicit HashDictionary(const std::string& text)
        : n(text.size() + 1) {}

    explicit HashDictionary(uint64_t text_size)
        : n(text_size + 1) {}

    uint64_t interval_search(Interval interval, uint64_t length) const override {
        if (interval.empty() || interval.sp >= n) {
            return NONE;
        }

        auto length_it = intervals_by_length.find(length);
        if (length_it == intervals_by_length.end()) {
            return NONE;
        }

        const auto& intervals = length_it->second;
        auto it = intervals.upper_bound(interval.sp);
        if (it == intervals.begin()) {
            return NONE;
        }

        --it;
        if (it->second.ep >= interval.ep) {
            return it->second.id;
        }

        return NONE;
    }

    bool interval_insert(Interval interval, Factor factor) override {
        if (interval.empty() || interval.sp >= n) {
            return false;
        }

        if (interval.ep >= n) {
            interval.ep = n - 1;
        }

        intervals_by_length[factor.length][interval.sp] = Entry{
            interval.ep,
            factor.id
        };

        return true;
    }
};
