#pragma once

#include <index_interface.hpp>
#include <lz78_dictionary.hpp>
#include <factor.hpp>

#include <string>
#include <vector>
#include <algorithm>
#include <iostream>

class LZ78Factorizer {
private:
    BWTIndex& index;
    LZ78Dictionary& dictionary;
    std::vector<Factor> factors;

public:
    explicit LZ78Factorizer(BWTIndex& index, LZ78Dictionary& dictionary)
        :index(index), dictionary(dictionary) {}

    void factorize(const std::string& S) {
        std::string S_rev = get_reversed(S);
        index.build(S_rev);
        std::string T = S_rev + '$';
        int k = index.size(), i = 1;
        while (k > 0) {
            uint64_t l = 1;
            uint64_t j = 0;
            Interval interval = index.full_range();
            unsigned char unmatched;
            Factor b;
            do {
                unmatched = T[k - 1];
                interval = index.backward_search(interval, T[k - 1]);
                b = {i, j, unmatched, l, interval.sp, interval.ep};
                j = dictionary.interval_search(interval, l);
                k--, l++;
            } while (j != 0);
            dictionary.interval_insert(interval, b);
            factors.push_back(b);
            i++;
        }
    }

    std::string get_reversed(const std::string &S) const {
        std::string T = S;
        std::reverse(T.begin(), T.end());
        return T;
    }

    void print_factors() const {
        for (auto f : factors) {
            std::cout << "(" << f.parent_id << ", " << f.symbol << ") ";
        }
        std::cout << '\n';
    }
};