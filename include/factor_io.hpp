#pragma once

#include <factor.hpp>

#include <cstdint>
#include <fstream>
#include <stdexcept>
#include <string>

class FactorWriter {
private:
    std::ofstream out_;

    void write_uint64(uint64_t value) {
        char bytes[8];
        for (int i = 0; i < 8; ++i) {
            bytes[i] = static_cast<char>((value >> (8 * i)) & 0xff);
        }
        out_.write(bytes, 8);
        if (!out_) {
            throw std::runtime_error("failed to write factor file");
        }
    }

public:
    explicit FactorWriter(const std::string& path)
        : out_(path, std::ios::binary | std::ios::trunc) {
        if (!out_) {
            throw std::runtime_error("failed to open factor file for writing: " + path);
        }
    }

    void write(const Factor& factor) {
        write_uint64(factor.parent_id);
        const char symbol = static_cast<char>(factor.symbol);
        out_.write(&symbol, 1);
        if (!out_) {
            throw std::runtime_error("failed to write factor symbol");
        }
    }
};

class FactorReader {
private:
    std::ifstream in_;
    uint64_t next_id_ = 1;

    bool read_uint64(uint64_t& value, bool allow_eof) {
        char bytes[8];
        in_.read(bytes, 8);
        const std::streamsize count = in_.gcount();

        if (count == 0 && in_.eof() && allow_eof) {
            return false;
        }

        if (count != 8) {
            throw std::runtime_error("truncated factor file");
        }

        value = 0;
        for (int i = 0; i < 8; ++i) {
            value |= static_cast<uint64_t>(static_cast<unsigned char>(bytes[i])) << (8 * i);
        }

        return true;
    }

    void read_symbol(unsigned char& symbol) {
        char c = 0;
        in_.read(&c, 1);
        if (in_.gcount() != 1) {
            throw std::runtime_error("truncated factor file");
        }
        symbol = static_cast<unsigned char>(c);
    }

public:
    explicit FactorReader(const std::string& path)
        : in_(path, std::ios::binary) {
        if (!in_) {
            throw std::runtime_error("failed to open factor file for reading: " + path);
        }
    }

    bool read(Factor& factor) {
        uint64_t parent_id = 0;
        if (!read_uint64(parent_id, true)) {
            return false;
        }

        factor = {};
        factor.id = next_id_++;
        factor.parent_id = parent_id;
        read_symbol(factor.symbol);
        return true;
    }
};
