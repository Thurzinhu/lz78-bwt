#pragma once

#include <cstdint>
#include <string>

struct Interval {
    uint64_t sp;
    uint64_t ep;

    bool empty() const {
        return sp > ep;
    }

    bool contains(Interval other) const {
        return sp <= other.sp && ep >= other.ep;
    }
};

class BWTIndex {
public:
    virtual Interval full_range() const = 0;
    virtual Interval backward_search(Interval interval, unsigned char c) const = 0;
    virtual uint64_t size() const = 0;
    virtual void build(const std::string& text) = 0;
    virtual ~BWTIndex() = default;
};