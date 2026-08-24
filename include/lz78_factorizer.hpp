#pragma once

#include <index_interface.hpp>
#include <lz78_dictionary.hpp>
#include <factor.hpp>
#include <factor_io.hpp>
#include <file_utils.hpp>
#include <progress.hpp>

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

class LZ78Factorizer {
private:
    BWTIndex& index;
    LZ78Dictionary& dictionary;
    std::vector<Factor> factors;

    template <typename Text, typename Emit>
    void factorize_reversed_text(const Text& text, Emit emit) {
        uint64_t k = 0;
        uint64_t next_id = 1;
        uint64_t processed = 0;
        uint64_t next_progress = 10'000'000;

        while (k < text.size()) {
            uint64_t length = 1;
            uint64_t parent_id = 0;
            Interval interval = index.full_range();
            Factor factor{};

            while (k < text.size()) {
                const unsigned char symbol = text[k];
                interval = index.backward_search(interval, symbol);
                factor = {next_id, parent_id, symbol, length, interval.sp, interval.ep};

                const uint64_t existing_id = dictionary.interval_search(interval, length);
                ++k;
                ++processed;

                if (processed >= next_progress) {
                    log_lz78_progress(
                        "factorize processed=" + std::to_string(processed) +
                        " factors=" + std::to_string(next_id)
                    );
                    next_progress += 10'000'000;
                }

                if (existing_id == 0 || k >= text.size()) {
                    break;
                }

                parent_id = existing_id;
                ++length;
            }

            dictionary.interval_insert(interval, factor);
            emit(factor);
            ++next_id;
        }
    }

public:
    explicit LZ78Factorizer(BWTIndex& index, LZ78Dictionary& dictionary)
        :index(index), dictionary(dictionary) {}

    void factorize(const std::string& S) {
        if (S.empty()) {
            throw std::invalid_argument("input text is empty");
        }
        if (S.find('$') != std::string::npos) {
            throw std::invalid_argument("input containing '$' is not supported");
        }

        std::string S_rev = get_reversed(S);
        index.build(S_rev);
        factors.clear();
        factorize_reversed_text(S_rev, [this](const Factor& factor) {
            factors.push_back(factor);
        });
    }

    void factorize_file(
        const std::string& input_path,
        const std::string& output_factors_path
    ) {
        if (file_size(input_path) == 0) {
            throw std::invalid_argument("input file is empty");
        }

        index.build_from_file(input_path);

        DiskText text(input_path);
        FactorWriter writer(output_factors_path);

        log_lz78_progress("factorize begin");
        factorize_reversed_text(text, [&writer](const Factor& factor) {
            writer.write(factor);
        });
        log_lz78_progress("factorize end");
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

    const std::vector<Factor>& get_factors() const {
        return factors;
    }
};
