#pragma once

#include <indexes/index_interface.hpp>
#include <dictionaries/lz78_dictionary.hpp>
#include <factor.hpp>
#include <factor_io.hpp>
#include <utils/file_utils.hpp>
#include <utils/progress.hpp>
#include <utils/metrics.hpp>

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
    uint64_t factor_count_ = 0;
    RunMetrics* metrics_;

    template <typename Action>
    void measure_phase(const std::string& name, Action action) {
        if (metrics_) metrics_->measure(name, action);
        else action();
    }

    template <typename Text, typename Emit>
    void factorize_text(const Text& text, Emit emit) {
        factor_count_ = 0;
        uint64_t k = 0;
        uint64_t next_id = 1;
        uint64_t processed = 0;
        uint64_t progress_step = 10'000'000;
        uint64_t next_progress = progress_step;

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
                    if (metrics_) metrics_->sample(processed, factor_count_, dictionary);
                    log_lz78_progress(
                        "factorize processed=" + std::to_string(processed) +
                        " factors=" + std::to_string(factor_count_)
                    );
                    next_progress += progress_step;
                }

                if (existing_id == 0 || k >= text.size()) {
                    break;
                }

                parent_id = existing_id;
                ++length;
            }

            dictionary.interval_insert(interval, factor);
            emit(factor);
            ++factor_count_;
            ++next_id;
        }

        log_lz78_progress(
            "factorize processed=" + std::to_string(processed) +
            " factors=" + std::to_string(factor_count_)
        );
        if (metrics_) metrics_->sample(processed, factor_count_, dictionary);
        std::cout << "# of entries on the dict " << dictionary.number_of_dictionary_entries() << '\n';
    }

public:
    explicit LZ78Factorizer(BWTIndex& index, LZ78Dictionary& dictionary, RunMetrics* metrics = nullptr)
        :index(index), dictionary(dictionary), metrics_(metrics) {}

    void factorize(const std::string& S) {
        if (S.empty()) {
            throw std::invalid_argument("input text is empty");
        }
        if (S.find('$') != std::string::npos) {
            throw std::invalid_argument("input containing '$' is not supported");
        }

        index.build(get_reversed(S));
        factors.clear();
        factorize_text(S, [this](const Factor& factor) {
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

        TemporaryFile reversed_file;
        const std::string reversed_file_path = reversed_file.string();

        measure_phase("reverse_input", [&] {
            create_reversed_file(input_path, reversed_file_path);
        });

        measure_phase(std::string("build_") + index.name() + "_index", [&] {
            index.build_from_file(reversed_file_path);
        });

        DiskText text(input_path);
        FactorWriter writer(output_factors_path);

        log_lz78_progress("factorize begin");
        measure_phase("factorize", [&] {
            factorize_text(text, [&writer](const Factor& factor) {
                writer.write(factor);
            });
            writer.close();
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

    uint64_t factor_count() const {
        return factor_count_;
    }
};
