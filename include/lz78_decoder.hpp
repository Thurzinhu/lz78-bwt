#pragma once

#include <factor_io.hpp>

#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>

inline void lz78_decode_file(
    const std::string& input_factors_path,
    const std::string& output_text_path
) {
    FactorReader reader(input_factors_path);
    std::ofstream out(output_text_path, std::ios::binary | std::ios::trunc);
    if (!out) {
        throw std::runtime_error("failed to open output text file: " + output_text_path);
    }

    std::vector<std::string> phrases;
    phrases.emplace_back();

    Factor factor{};
    while (reader.read(factor)) {
        if (factor.id != phrases.size()) {
            throw std::runtime_error("factor ids must be contiguous and start at 1");
        }

        if (factor.parent_id >= phrases.size()) {
            throw std::runtime_error("factor references an unknown parent id");
        }

        std::string phrase = phrases[factor.parent_id];
        phrase.push_back(static_cast<char>(factor.symbol));

        out.write(phrase.data(), static_cast<std::streamsize>(phrase.size()));
        if (!out) {
            throw std::runtime_error("failed to write decoded text");
        }

        phrases.push_back(std::move(phrase));
    }
}
