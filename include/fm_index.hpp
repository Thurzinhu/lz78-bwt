#pragma once

#include <index_interface.hpp>

#include <sdsl/suffix_arrays.hpp>

#include <cstdint>

class FMIndex : public BWTIndex {
private:
    sdsl::csa_wt<> csa;

public:
    FMIndex() = default;
    
    explicit FMIndex(const std::string& text) {
        build(text);
    }

    void build(const std::string& text) override {
        sdsl::construct_im(csa, text, 1);
    }

    Interval full_range() const override {
        return {0, size() - 1};
    }

    Interval backward_search(Interval interval, unsigned char c) const override {
        Interval new_interval;
        sdsl::backward_search(csa, interval.sp, interval.ep, c, new_interval.sp, new_interval.ep);
        return new_interval;
    }

    uint64_t size() const override {
        return csa.size();
    }
};
