#pragma once

#include <factor.hpp>
#include <index_interface.hpp>

#include <cstdint>

class LZ78Dictionary {
public:
    virtual uint64_t interval_search(Interval interval, uint64_t length) const = 0;
    virtual bool interval_insert(Interval interval, Factor factor) = 0;
    virtual ~LZ78Dictionary() = default;
    virtual int getNumberFactors() = 0;
};