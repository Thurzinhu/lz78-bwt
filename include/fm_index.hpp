#pragma once

#include <file_utils.hpp>
#include <index_interface.hpp>
#include <progress.hpp>

#include <sdsl/construct_config.hpp>
#include <sdsl/io.hpp>
#include <sdsl/suffix_arrays.hpp>

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <stdexcept>
#include <string>

class FMIndex : public BWTIndex {
private:
    sdsl::csa_wt<> csa;

    enum class FMIndexBuildMode {
        SpaceEfficient,
        Fast
    };
    class ScopedByteSaAlgorithm {
    private:
        sdsl::byte_sa_algo_type previous_;

    public:
        explicit ScopedByteSaAlgorithm(sdsl::byte_sa_algo_type algorithm)
            : previous_(sdsl::construct_config().byte_algo_sa) {
            sdsl::construct_config().byte_algo_sa = algorithm;
        }

        ~ScopedByteSaAlgorithm() {
            sdsl::construct_config().byte_algo_sa = previous_;
        }
    };

public:
    FMIndex() = default;
    
    explicit FMIndex(const std::string& text) {
        build(text);
    }

    void build(const std::string& text) override {
        if (text.find('$') != std::string::npos) {
            throw std::invalid_argument("text containing '$' is not supported");
        }
        sdsl::construct_im(csa, text, 1);
    }

    void build_from_file(const std::string& file_path) override {
        build_from_file(file_path, FMIndexBuildMode::SpaceEfficient);
    }

    void build_from_file(const std::string& file_path, FMIndexBuildMode mode) {
        log_lz78_progress("fm_index construct begin");

        sdsl::byte_sa_algo_type algorithm =
            mode == FMIndexBuildMode::Fast
                ? sdsl::LIBDIVSUFSORT
                : sdsl::SE_SAIS;

        ScopedByteSaAlgorithm scoped_algorithm(algorithm);

        const std::string cache_id = "lz78_fm_" + std::to_string(
            std::chrono::steady_clock::now().time_since_epoch().count()
        );

        sdsl::cache_config config(
            false,
            std::filesystem::temp_directory_path().string(),
            cache_id
        );

        sdsl::construct(csa, file_path, config, 1);

        log_lz78_progress("fm_index construct end");
    }

    Interval full_range() const override {
        return {0, size() - 1};
    }

    Interval backward_search(Interval interval, unsigned char c) const override {
        if (interval.empty()) {
            return interval;
        }

        Interval new_interval;
        sdsl::backward_search(csa, interval.sp, interval.ep, c, new_interval.sp, new_interval.ep);
        return new_interval;
    }

    uint64_t size() const override {
        return csa.size();
    }

    uint64_t size_in_bytes() const {
        return sdsl::size_in_bytes(csa);
    }
};
