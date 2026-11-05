// Copyright (c) 2017, Nicola Prezza. All rights reserved.
// Use of the original r-index code is governed by the MIT license in ../LICENSE.
// LOCAL: count-only adaptation. Original joint work: Travis Gagie,
// Gonzalo Navarro and Nicola Prezza. See README.md and LOCAL_CHANGES.md.
#pragma once
#include "rle_string.hpp"
#include <sdsl/construct.hpp>
#include <array>
#include <filesystem>
#include <fstream>
#include <stdexcept>

namespace ri {
class r_index_count {
    rle_string_sd bwt_;
    std::array<uint64_t,256> F_{};

    struct cache_scope {
        sdsl::cache_config config{true, std::filesystem::temp_directory_path().string()};
        sdsl::byte_sa_algo_type previous = sdsl::construct_config().byte_algo_sa;
        std::filesystem::path directory;
        cache_scope() {
            directory = std::filesystem::path(config.dir) / ("rindex-count-" + config.id);
            if (!std::filesystem::create_directory(directory))
                throw std::runtime_error("failed to create private r-index cache directory");
            config.dir = directory.string();
        }
        ~cache_scope() {
            sdsl::construct_config().byte_algo_sa = previous;
            std::error_code ignored;
            std::filesystem::remove_all(directory, ignored);
        }
    };
    static void validate(unsigned char c) {
        if(c==0 || c==1 || c=='$') throw std::invalid_argument("r-index reserves NUL, byte 0x01 and '$'");
    }
    template<class Writer> void construct_text(Writer writer) {
        cache_scope scope;
        auto& cc=scope.config;
        auto text_path=sdsl::cache_file_name(sdsl::conf::KEY_TEXT,cc);
        cc.file_map[sdsl::conf::KEY_TEXT]=text_path;
        {
            sdsl::int_vector_buffer<8> text(text_path,std::ios::out);
            writer(text);
            text.push_back(0);
        }
        sdsl::construct_config().byte_algo_sa=sdsl::SE_SAIS;
        sdsl::construct_sa<8>(cc);
        sdsl::construct_bwt<8>(cc);
        const auto path=sdsl::cache_file_name(sdsl::conf::KEY_BWT,cc);
        auto next=rle_string_sd::from_bwt_file(path);
        std::array<uint64_t,256> counts{}, cumulative{};
        {
            sdsl::int_vector_buffer<8> input(path);
            for(uint64_t i=0;i<input.size();++i) { unsigned c=input[i]; ++counts[c?c:1]; }
        }
        for(unsigned c=1;c<256;++c) cumulative[c]=cumulative[c-1]+counts[c-1];
        bwt_=std::move(next); F_=cumulative;
    }
public:
    r_index_count() = default;
    explicit r_index_count(const std::string& s) {
        construct_text([&](auto& text) { for(unsigned char c:s) {validate(c);text.push_back(c);} });
    }
    void build_from_file(const std::string& path) {
        std::ifstream in(path,std::ios::binary);
        if(!in) throw std::runtime_error("failed to open input file: "+path);
        construct_text([&](auto& text) {
            std::array<char,65536> block;
            while(in) {
                in.read(block.data(),block.size());
                for(std::streamsize i=0;i<in.gcount();++i) {
                    auto c=static_cast<unsigned char>(block[i]);validate(c);text.push_back(c);
                }
            }
            if(in.bad()) throw std::runtime_error("failed to read input file: "+path);
        });
    }
    range_t full_range() const { return bwt_size()?range_t{0,bwt_size()-1}:range_t{1,0}; }
    range_t LF(range_t range, unsigned char c) const {
        if(range.first>range.second || !bwt_size()) return {1,0};
        if(range.second>=bwt_size()) throw std::out_of_range("BWT interval outside index");
        if(c==255 ? F_[c]==bwt_size() : F_[c]>=F_[c+1]) return {1,0};
        const auto begin=bwt_.rank(range.first,c), end=bwt_.rank(range.second+1,c);
        if(begin==end) return {1,0};
        return {F_[c]+begin,F_[c]+end-1};
    }
    uint64_t bwt_size() const { return bwt_.size(); }
    uint64_t number_of_runs() const { return bwt_size()?bwt_.number_of_runs():0; }
    uint64_t serialize(std::ostream& out) const {
        const char header[8]={'R','I','C','O','U','N','T',1};
        out.write(header,sizeof(header));
        out.write(reinterpret_cast<const char*>(F_.data()),sizeof(F_));
        return sizeof(header)+sizeof(F_)+bwt_.serialize(out);
    }
};
}
