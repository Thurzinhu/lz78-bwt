#pragma once

#include <lz78_dictionary.hpp>

#include <algorithm>
#include <cstdint>
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

        void update(uint64_t ql, uint64_t qr, uint64_t value, uint64_t p, uint64_t l, uint64_t r) {
            if (qr < l || r < ql) return;
            if (ql <= l and r <= qr) {
                seg[p] = std::max(seg[p], value);
                return;
            }
            uint64_t mid = (l + r) / 2;
            update(ql, qr, value, p * 2, l, mid);
            update(ql, qr, value, p * 2 + 1, mid + 1, r);
        }

        uint64_t query(uint64_t pos, uint64_t p, uint64_t l, uint64_t r) const {
            if (pos < l or r < pos) return NONE;
            if (l == r) return seg[p];
            uint64_t mid = (l + r) / 2;
            return std::max({
                seg[p],
                query(pos, p * 2, l, mid),
                query(pos, p * 2 + 1, mid + 1, r)
            });
        }

    public:
        explicit SegmentTree(uint64_t size)
            : n(size),
              seg(4 * size + 4, NONE) {}

        void update(Interval interval, uint64_t value) {
            if (interval.empty()) return;
            update(interval.sp, interval.ep, value, 1, 0, n - 1);
        }

        uint64_t query(uint64_t pos) const {
            return query(pos, 1, 0, n - 1);
        }
    };

private:
    uint64_t n;

    std::unordered_map<uint64_t, SegmentTree> trees_by_length;

public:
    explicit SegTreeDictionary(const std::string& text)
        : n(text.size()) {}

    uint64_t interval_search(Interval interval, uint64_t length) const override {
        auto it = trees_by_length.find(length);

        if (it == trees_by_length.end()) {
            return NONE;
        }

        return it->second.query(interval.sp);
    }

    bool interval_insert(Interval interval, Factor factor) override {
        auto it = trees_by_length.find(factor.length);

        if (it == trees_by_length.end()) {
            auto result = trees_by_length.emplace(
                factor.length,
                SegmentTree(n)
            );

            it = result.first;
        }

        it->second.update(interval, factor.id);

        return true;
    }
};