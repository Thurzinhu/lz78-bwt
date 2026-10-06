#pragma once

#include <lz78_dictionary.hpp>

#include <cstdint>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

class SegTreeDictionary : public LZ78Dictionary {
private:
    static constexpr uint64_t NONE = 0;

    class SegmentTree {
    private:
        struct Node {
            uint64_t sp;
            uint64_t ep;
            uint64_t id;
            uint32_t left;
            uint32_t right;
            uint32_t priority;
        };

        uint64_t n;
        uint32_t root = 0;
        std::vector<Node> nodes;

        static uint32_t priority_for(uint64_t value) {
            value += 0x9e3779b97f4a7c15ULL;
            value = (value ^ (value >> 30)) * 0xbf58476d1ce4e5b9ULL;
            value = (value ^ (value >> 27)) * 0x94d049bb133111ebULL;
            value = value ^ (value >> 31);
            return static_cast<uint32_t>(value >> 32);
        }

        uint32_t rotate_right(uint32_t p) {
            const uint32_t q = nodes[p].left;
            nodes[p].left = nodes[q].right;
            nodes[q].right = p;
            return q;
        }

        uint32_t rotate_left(uint32_t p) {
            const uint32_t q = nodes[p].right;
            nodes[p].right = nodes[q].left;
            nodes[q].left = p;
            return q;
        }

        uint32_t insert(uint32_t p, uint32_t node_index) {
            if (p == 0) {
                return node_index;
            }

            if (nodes[node_index].sp < nodes[p].sp) {
                nodes[p].left = insert(nodes[p].left, node_index);
                if (nodes[nodes[p].left].priority > nodes[p].priority) {
                    p = rotate_right(p);
                }
            } else if (nodes[node_index].sp > nodes[p].sp) {
                nodes[p].right = insert(nodes[p].right, node_index);
                if (nodes[nodes[p].right].priority > nodes[p].priority) {
                    p = rotate_left(p);
                }
            } else {
                nodes[p].ep = nodes[node_index].ep;
                nodes[p].id = nodes[node_index].id;
            }

            return p;
        }

    public:
        explicit SegmentTree(uint64_t size)
            : n(size),
              nodes(1, Node{0, 0, NONE, 0, 0, 0}) {}

        void update(Interval interval, uint64_t value) {
            if (n == 0 || interval.empty() || interval.sp >= n) return;
            if (interval.ep >= n) {
                interval.ep = n - 1;
            }
            if (nodes.size() > static_cast<uint64_t>(UINT32_MAX)) {
                throw std::runtime_error("too many intervals in SegTreeDictionary");
            }

            const uint32_t node_index = static_cast<uint32_t>(nodes.size());
            nodes.push_back(Node{
                interval.sp,
                interval.ep,
                value,
                0,
                0,
                priority_for(interval.sp)
            });
            root = insert(root, node_index);
        }

        uint64_t query(Interval interval) const {
            if (n == 0 || interval.empty() || interval.sp >= n) return NONE;

            uint32_t current = root;
            uint32_t candidate = 0;

            while (current != 0) {
                if (nodes[current].sp <= interval.sp) {
                    candidate = current;
                    current = nodes[current].right;
                } else {
                    current = nodes[current].left;
                }
            }

            if (candidate != 0 && nodes[candidate].ep >= interval.ep) {
                return nodes[candidate].id;
            }

            return NONE;
        }

        uint64_t get_nodes_size() const {
            return nodes.size();
        }
    };

private:
    uint64_t n;

    std::unordered_map<uint64_t, SegmentTree> trees_by_length;

public:
    explicit SegTreeDictionary(const std::string& text)
        : n(text.size() + 1) {}

    explicit SegTreeDictionary(uint64_t text_size)
        : n(text_size + 1) {}

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
            auto result = trees_by_length.emplace(
                factor.length,
                SegmentTree(n)
            );

            it = result.first;
        }

        it->second.update(interval, factor.id);

        return true;
    }

    uint64_t number_of_dictionary_entries(){
        uint64_t total = 0;
        for (const auto [_, tree] : trees_by_length){
            total += tree.get_nodes_size();
        }
        return total;
    }
};
