#pragma once

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>

class TemporaryFile {
private:
    std::filesystem::path path_;

public:
    TemporaryFile() {
        const auto seed = std::chrono::steady_clock::now()
            .time_since_epoch()
            .count();

        for (uint64_t attempt = 0; attempt < 100; ++attempt) {
            path_ = std::filesystem::temp_directory_path() /
                ("lz78_srev_" + std::to_string(seed) + "_" +
                    std::to_string(attempt) + ".tmp");

            if (!std::filesystem::exists(path_)) {
                return;
            }
        }

        throw std::runtime_error("failed to create a unique temporary file path");
    }

    ~TemporaryFile() {
        std::error_code ec;
        std::filesystem::remove(path_, ec);
    }

    std::string string() const {
        return path_.string();
    }
};

inline uint64_t file_size(const std::string& path) {
    return static_cast<uint64_t>(std::filesystem::file_size(path));
}

inline bool file_contains_byte(const std::string& path, unsigned char value) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        throw std::runtime_error("failed to open input file: " + path);
    }

    constexpr std::size_t block_size = 1 << 20;
    std::vector<char> buffer(block_size);

    while (in) {
        in.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
        const std::streamsize count = in.gcount();
        for (std::streamsize i = 0; i < count; ++i) {
            if (static_cast<unsigned char>(buffer[static_cast<std::size_t>(i)]) == value) {
                return true;
            }
        }
    }

    return false;
}

inline void create_reversed_file(
    const std::string& input_path,
    const std::string& output_path
) {
    namespace fs = std::filesystem;

    if (fs::absolute(input_path) == fs::absolute(output_path)) {
        throw std::invalid_argument("input and output paths must be different");
    }

    std::ifstream in(input_path, std::ios::binary);
    if (!in) {
        throw std::runtime_error("failed to open input file: " + input_path);
    }

    std::ofstream out(output_path, std::ios::binary | std::ios::trunc);
    if (!out) {
        throw std::runtime_error("failed to open output file: " + output_path);
    }

    constexpr uint64_t block_size = 1 << 20;
    uint64_t remaining = file_size(input_path);
    std::vector<char> buffer;

    while (remaining > 0) {
        const uint64_t to_read = std::min(block_size, remaining);
        remaining -= to_read;

        buffer.resize(static_cast<std::size_t>(to_read));

        in.clear();
        in.seekg(static_cast<std::streamoff>(remaining), std::ios::beg);
        if (!in) {
            throw std::runtime_error("failed to seek input file: " + input_path);
        }

        in.read(buffer.data(), static_cast<std::streamsize>(to_read));
        if (in.gcount() != static_cast<std::streamsize>(to_read)) {
            throw std::runtime_error("failed to read input file: " + input_path);
        }

        for (char c : buffer) {
            if (static_cast<unsigned char>(c) == static_cast<unsigned char>('$')) {
                throw std::invalid_argument("input containing '$' is not supported");
            }
        }

        std::reverse(buffer.begin(), buffer.end());

        out.write(buffer.data(), static_cast<std::streamsize>(buffer.size()));
        if (!out) {
            throw std::runtime_error("failed to write reversed file: " + output_path);
        }
    }
}

class DiskText {
private:
    std::string path_;
    uint64_t size_;
    mutable std::ifstream in_;

public:
    explicit DiskText(const std::string& path)
        : path_(path),
          size_(file_size(path)),
          in_(path, std::ios::binary) {
        if (!in_) {
            throw std::runtime_error("failed to open text file: " + path);
        }
    }

    uint64_t size() const {
        return size_;
    }

    unsigned char operator[](uint64_t pos) const {
        if (pos >= size_) {
            throw std::out_of_range("DiskText position out of range");
        }

        in_.clear();
        in_.seekg(static_cast<std::streamoff>(pos), std::ios::beg);
        if (!in_) {
            throw std::runtime_error("failed to seek text file: " + path_);
        }

        char c = 0;
        in_.get(c);
        if (!in_) {
            throw std::runtime_error("failed to read text file: " + path_);
        }

        return static_cast<unsigned char>(c);
    }
};
