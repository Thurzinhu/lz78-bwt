#include <fm_index.hpp>
#include <file_utils.hpp>
#include <linear_dictionary.hpp>
#include <lz78_decoder.hpp>
#include <lz78_factorizer.hpp>
#include <progress.hpp>
#include <seg_tree_dictionary.hpp>
#include <hash_dictionary.hpp>

#include <exception>
#include <iostream>
#include <string>

namespace {

void print_usage(const char* program) {
    std::cerr
        << "Usage:\n"
        << "  " << program << " encode <input_text_file> <output_factors_file> <linear|segtree|hash>\n"
        << "  " << program << " decode <input_factors_file> <output_text_file>\n";
}

void print_memory_report(const FMIndex& index) {
    const uint64_t fm_index_bytes = index.size_in_bytes();
    const double fm_index_mib = fm_index_bytes / (1024.0 * 1024.0);

    std::cerr << "[lz78] memory"
              << " fm_index_bytes=" << fm_index_bytes
              << " fm_index_mib=" << fm_index_mib
              << " rss_kb=" << current_rss_kb()
              << '\n';
}

int encode(
    const std::string& input_path,
    const std::string& output_path,
    const std::string& dictionary_name
) {
    FMIndex index;
    const uint64_t input_size = file_size(input_path);

    if (dictionary_name == "linear") {
        LinearDictionary dictionary;
        LZ78Factorizer factorizer(index, dictionary);
        factorizer.factorize_file(input_path, output_path);
        print_memory_report(index);
        return 0;
    }

    if (dictionary_name == "segtree") {
        SegTreeDictionary dictionary(input_size);
        LZ78Factorizer factorizer(index, dictionary);
        factorizer.factorize_file(input_path, output_path);
        print_memory_report(index);
        return 0;
    }

    if (dictionary_name == "hash") {
        HashDictionary dictionary(input_size);
        LZ78Factorizer factorizer(index, dictionary);
        factorizer.factorize_file(input_path, output_path);
        print_memory_report(index);
        return 0;
    }

    throw std::invalid_argument("dictionary must be 'linear', 'segtree', or 'hash'");
}

} // namespace

int main(int argc, char** argv) {
    try {
        if (argc < 2) {
            print_usage(argv[0]);
            return 1;
        }

        const std::string mode = argv[1];

        if (mode == "encode") {
            if (argc != 5) {
                print_usage(argv[0]);
                return 1;
            }

            return encode(argv[2], argv[3], argv[4]);
        }

        if (mode == "decode") {
            if (argc != 4) {
                print_usage(argv[0]);
                return 1;
            }

            lz78_decode_file(argv[2], argv[3]);
            return 0;
        }

        print_usage(argv[0]);
        return 1;
    } catch (const std::exception& ex) {
        std::cerr << "error: " << ex.what() << '\n';
        return 1;
    }
}
