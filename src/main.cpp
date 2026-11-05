#include <indexes/fm_index.hpp>
#include <indexes/r_index.hpp>
#include <utils/file_utils.hpp>
#include <dictionaries/linear_dictionary.hpp>
#include <lz78_decoder.hpp>
#include <lz78_factorizer.hpp>
#include <utils/progress.hpp>
#include <dictionaries/seg_tree_dictionary.hpp>
#include <dictionaries/hash_dictionary.hpp>

#include <exception>
#include <iostream>
#include <memory>
#include <string>

namespace {

void print_usage(const char* program) {
    std::cerr
        << "Usage:\n"
        << "  " << program << " encode <input_text_file> <output_factors_file> <linear|segtree|hash> [--index <fm|rindex>]\n"
        << "  " << program << " decode <input_factors_file> <output_text_file>\n";
}

int encode(
    const std::string& input_path,
    const std::string& output_path,
    const std::string& dictionary_name,
    const std::string& index_name
) {
    const auto input_file = std::filesystem::weakly_canonical(input_path);
    const auto output_file = std::filesystem::weakly_canonical(output_path);
    if (input_file == output_file ||
        (std::filesystem::exists(output_path) && std::filesystem::equivalent(input_path, output_path))) {
        throw std::invalid_argument("input and output paths must be different");
    }
    std::unique_ptr<BWTIndex> index;
    if (index_name == "fm") index = std::make_unique<FMIndex>();
    else if (index_name == "rindex") index = std::make_unique<RIndex>();
    else throw std::invalid_argument("index must be 'fm' or 'rindex'");
    const uint64_t input_size = file_size(input_path);
    auto run = [&](LZ78Dictionary& dictionary) {
        LZ78Factorizer factorizer(*index, dictionary);
        factorizer.factorize_file(input_path, output_path);
        return 0;
    };

    if (dictionary_name == "linear") {
        LinearDictionary dictionary;
        return run(dictionary);
    }

    if (dictionary_name == "segtree") {
        SegTreeDictionary dictionary(input_size);
        return run(dictionary);
    }

    if (dictionary_name == "hash") {
        HashDictionary dictionary(input_size);
        return run(dictionary);
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
            if (argc < 5 || (argc - 5) % 2 != 0) {
                print_usage(argv[0]);
                return 1;
            }

            std::string index_name = "fm";
            bool seen_index = false;
            for (int i = 5; i < argc; i += 2) {
                const std::string option = argv[i], value = argv[i + 1];
                if (value.empty() || value.rfind("--", 0) == 0) {
                    throw std::invalid_argument("missing value for option: " + option);
                }
                if (option == "--index" && !seen_index) {
                    seen_index = true;
                    index_name = value == "rlbwt" ? "rindex" : value;
                    if (index_name != "fm" && index_name != "rindex") {
                        throw std::invalid_argument("index must be 'fm' or 'rindex'");
                    }
                } else {
                    throw std::invalid_argument("unknown or duplicate option: " + option);
                }
            }
            return encode(argv[2], argv[3], argv[4], index_name);
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
