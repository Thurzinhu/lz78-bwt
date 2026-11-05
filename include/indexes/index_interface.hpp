#pragma once

#include <cstdint>
#include <optional>
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
    virtual void build_from_file(const std::string& file_path) = 0;
    virtual const char* name() const = 0;
    virtual uint64_t size_in_bytes() const = 0;
    virtual std::optional<uint64_t> number_of_runs() const { return std::nullopt; }
    virtual ~BWTIndex() = default;
};
