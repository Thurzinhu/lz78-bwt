#include <linear_dictionary.hpp>
#include <seg_tree_dictionary.hpp>
#include <fm_index.hpp>
#include <lz78_factorizer.hpp>
#include <string>

int main() {
    std::string S = "abracadabra";
    FMIndex index;
    LinearDictionary dictionary;
    SegTreeDictionary dictionarySeg = SegTreeDictionary(S);

    LZ78Factorizer lz78 = LZ78Factorizer(index, dictionary);
    LZ78Factorizer lz78_2 = LZ78Factorizer(index, dictionarySeg);
    
    lz78.factorize(S);
    lz78.print_factors();

    lz78_2.factorize(S);
    lz78_2.print_factors();
}