#pragma once

#include <dictionaries/lz78_dictionary.hpp>

#include <cstdint>
#include <vector>

class LinearDictionary : public LZ78Dictionary {
private:
    std::vector<std::pair<Interval, Factor>> intervals;

public:
    uint64_t interval_search(Interval interval, uint64_t length) const override {
        for (const auto& [stored_interval, factor] : intervals) {
            if (stored_interval.contains(interval) and (factor.length == length)) {
                return factor.id;
            }
        }
        return 0;
    }

    bool interval_insert(Interval interval, Factor factor) override {
        intervals.emplace_back(interval, factor);
        return true;
    }

    uint64_t number_of_dictionary_entries(){
        return intervals.size();
    }
};
