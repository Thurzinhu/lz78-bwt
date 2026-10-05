#pragma once

#include <lz78_dictionary.hpp>

#include <algorithm>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

class SegTreeDictionary : public LZ78Dictionary {
private:
    static constexpr uint64_t NONE = 0;

    class SegmentTree {
    private:
        uint64_t n;
        std::vector<uint64_t> seg;
        uint64_t entries = 0;

        static std::size_t storage_size(uint64_t size) {
            const auto maximum = std::vector<uint64_t>().max_size();
            if (size > (maximum - 4) / 4) {
                throw std::length_error("SegTreeDictionary dense tree is too large");
            }
            return static_cast<std::size_t>(4 * size + 4);
        }

        // Range maximum updates remain on fully covered segments. Point
        // queries combine the tags on one root-to-leaf path; no push is needed.
        void update(uint64_t ql, uint64_t qr, uint64_t value,
                    uint64_t p, uint64_t l, uint64_t r) {
            if (qr < l || r < ql) return;
            if (ql <= l && r <= qr) {
                seg[p] = std::max(seg[p], value);
                return;
            }
            const uint64_t mid = l + (r - l) / 2;
            update(ql, qr, value, p * 2, l, mid);
            update(ql, qr, value, p * 2 + 1, mid + 1, r);
        }

        uint64_t query(uint64_t pos, uint64_t p, uint64_t l, uint64_t r) const {
            if (l == r) return seg[p];
            const uint64_t mid = l + (r - l) / 2;
            const uint64_t child = pos <= mid
                ? query(pos, p * 2, l, mid)
                : query(pos, p * 2 + 1, mid + 1, r);
            return std::max(seg[p], child);
        }

    public:
        explicit SegmentTree(uint64_t size)
            : n(size),
              seg(storage_size(size), NONE) {}

        void update(Interval interval, uint64_t value) {
            if (n == 0 || interval.empty() || interval.sp >= n) return;
            if (interval.ep >= n) {
                interval.ep = n - 1;
            }
            // For a fixed pattern length, valid BWT intervals are identical
            // or disjoint. A repeated final phrase does not add a new entry.
            if (query(interval.sp, 1, 0, n - 1) == NONE && value != NONE) {
                ++entries;
            }
            update(interval.sp, interval.ep, value, 1, 0, n - 1);
        }

        uint64_t query(Interval interval) const {
            if (n == 0 || interval.empty() || interval.ep >= n) return NONE;
            return query(interval.sp, 1, 0, n - 1);
        }

        uint64_t number_of_entries() const {
            return entries;
        }
    };

private:
    uint64_t n;

    std::unordered_map<uint64_t, SegmentTree> trees_by_length;

    static uint64_t index_size(uint64_t text_size) {
        if (text_size == std::numeric_limits<uint64_t>::max()) {
            throw std::length_error("SegTreeDictionary text size overflows the index size");
        }
        return text_size + 1;
    }

public:
    explicit SegTreeDictionary(const std::string& text)
        : SegTreeDictionary(static_cast<uint64_t>(text.size())) {}

    explicit SegTreeDictionary(uint64_t text_size)
        : n(index_size(text_size)) {}

    uint64_t interval_search(Interval interval, uint64_t length) const override {
        if (interval.empty() || interval.sp >= n) {
            return NONE;
        }

        auto it = trees_by_length.find(length);

        if (it == trees_by_length.end()) {
            return NONE;
        }

        return it->second.query(interval);
    }

    bool interval_insert(Interval interval, Factor factor) override {
        if (interval.empty() || interval.sp >= n) {
            return false;
        }

        auto it = trees_by_length.find(factor.length);

        if (it == trees_by_length.end()) {
            auto result = trees_by_length.try_emplace(factor.length, n);

            it = result.first;
        }

        it->second.update(interval, factor.id);

        return true;
    }

    int getNumberFactors(){
        return n;
    }
};
