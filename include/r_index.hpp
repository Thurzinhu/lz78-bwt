#pragma once

#include <index_interface.hpp>
#include <progress.hpp>
#include "../external/r-index/internal/r_index_count.hpp"

#include <fstream>
#include <iterator>
#include <optional>
#include <stdexcept>
#include <string>

// Count-only adaptation of Nicola Prezza’s r-index; see external/r-index/LICENSE.
class RIndex : public BWTIndex {
    std::optional<ri::r_index_count> index_;

public:
    RIndex() = default;
    explicit RIndex(const std::string& text) { build(text); }

    void build(const std::string& text) override {
        if (text.find_first_of(std::string("\0\1$", 3)) != std::string::npos) {
            throw std::invalid_argument("input containing NUL, byte 0x01 or '$' is not supported by r-index");
        }
        index_ = ri::r_index_count(text); // Original constructor; SE-SAIS by default.
    }

    void build_from_file(const std::string& path) override {
        log_lz78_progress("rindex construct begin");
        ri::r_index_count next;
        next.build_from_file(path);
        index_ = std::move(next);
        log_lz78_progress("rindex construct end");
    }

    Interval full_range() const override {
        if (!index_) return {1, 0};
        const auto range = index_->full_range();
        return {range.first, range.second};
    }

    Interval backward_search(Interval interval, unsigned char c) const override {
        if (!index_ || interval.empty()) return {1, 0};
        if (interval.ep >= size()) throw std::out_of_range("BWT interval outside index");
        const auto range = index_->LF({interval.sp, interval.ep}, c);
        return {range.first, range.second};
    }

    uint64_t size() const override { return index_ ? index_->bwt_size() : 0; }
    
    const char* name() const override { return "rindex"; }
    
    std::optional<uint64_t> number_of_runs() const override {
        return index_ ? index_->number_of_runs() : 0;
    }

    uint64_t size_in_bytes() const override {
        if (!index_) return 0;
        sdsl::nullstream stream;
        return index_->serialize(stream);
    }
};
