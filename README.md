# LZ78 Factorization with BWT-Based Indexes

A C++17 implementation of LZ78 factorization using indexes based on the **Burrows–Wheeler Transform (BWT)** to support phrase recognition.

The project supports two indexing structures:

- **FM-index:** implemented using `sdsl::csa_wt<>` from the SDSL library.
- **Count-only r-index:** an adaptation of Nicola Prezza's r-index that retains the structures required for search operations while omitting persistent occurrence-location structures.

Both implementations share the same factorizer and support three dictionary representations: `linear`, `segtree`, and `hash`.

During factorization, the input text is read from a file, searches are performed using an index built over the reversed text, and factors are written incrementally. The dictionary maps search intervals and phrase lengths to phrase identifiers without storing the full textual content of each phrase.

The project also includes a decoder that reconstructs the original input from the factor file without requiring the index used during encoding.

## Requirements

- Linux with a C++17-compatible compiler.
- GNU Make.
- Git to clone the repository and its submodules.
- Python 3 for command-line interface tests (optional).

SDSL v3 is provided through the `sdsl-lite` submodule and does not require a system-wide installation. The `external/r-index` directory contains the adapted r-index implementation.

### Building

Clone the repository with its submodules and build the executable:

```
git clone --recurse-submodules https://github.com/Thurzinhu/lz78-bwt.git
cd lz78-bwt
make main.x
```

If the repository has already been cloned:

```
git submodule update --init --recursive
make main.x
```

The resulting executable is `main.x`.

**Note:** The Makefile uses `-march=native` and `-msse4.2`. On machines that do not support these options, adjust the compiler flags before building.

## Quick Start

The following example encodes a file using the count-only r-index and a hash table, reconstructs the input, and verifies that both files are identical.

```
# Create an example input file
printf 'abracadabra abracadabra\n' > input.txt

# Generate LZ78 factors
./main.x encode input.txt factors.bin hash --index rindex

# Reconstruct the original input
./main.x decode factors.bin restored.txt

# Verify the reconstruction
cmp input.txt restored.txt
```

If the files are identical, `cmp` exits with status code zero and produces no output.

**Warning:** Output files are overwritten. Use different paths for the input, factor file, and reconstructed text.

## Encoding

The encoding command has the following syntax:

```
./main.x encode <input> <output_factors> <dictionary> [--index <fm|rindex>] [--metrics <file.json>]
```

The dictionary argument is required. If `--index` is omitted, the default is `fm`.

### Available Indexes

| Option   | Implementation                                                                              |
| -------- | ------------------------------------------------------------------------------------------- |
| `fm`     | FM-index based on `sdsl::csa_wt<>`.                                                         |
| `rindex` | r-index adapted for search and counting, without persistent occurrence-location structures. |

The alias `rlbwt` is also accepted as a value for `--index` and selects the `rindex` implementation.

### Available Dictionaries

| Option    | Implementation                                                        |
| --------- | --------------------------------------------------------------------- |
| `linear`  | Vector of factors and intervals, using linear search.                 |
| `segtree` | Segment trees organized by phrase length.                             |
| `hash`    | Hash table with keys composed of a search interval and phrase length. |

The `hash` option is used in the main experimental evaluation of the undergraduate thesis.

### Examples

Encoding with the FM-index and a hash table:

```
./main.x encode input.txt factors_fm.bin hash --index fm
```

Encoding with the FM-index and a linear dictionary:

```
./main.x encode input.txt factors_linear.bin linear --index fm
```

Encoding with the r-index and segment trees:

```
./main.x encode input.txt factors_segtree.bin segtree --index rindex
```

To record execution metrics in JSON format:

```
./main.x encode input.txt factors.bin hash --index rindex --metrics metrics.json
```

The metrics file path must be different from the input and output paths.

## Decoding

Decoding requires only the factor file:

```
./main.x decode <input_factors> <output_text>
```

Example:

```
./main.x decode factors.bin restored.txt
```

The index and dictionary used during encoding do not need to be specified.

The decoder keeps reconstructed phrases in memory during processing.

### Factor File Format

Each factor is stored as a **9-byte record** containing the following fields:

| Field       | Size    | Representation                          |
| ----------- | ------- | --------------------------------------- |
| `parent_id` | 8 bytes | Unsigned integer, little-endian.        |
| `symbol`    | 1 byte  | Byte appended to the referenced phrase. |

Phrase identifiers are sequential and start at `1`. Identifier `0` represents the empty phrase.

The file has no header. BWT intervals and phrase lengths are used internally during factorization but are not stored in the output.

For each factor, the decoder reconstructs the phrase using:

```
phrase[id] = phrase[parent_id] + symbol
```

The output format uses fixed-size records and does not include entropy coding.

## Input Restrictions and Memory Usage

The implementation has the following restrictions:

- Empty files and inputs containing the `$` character are rejected.
- The r-index also reserves bytes `0x00` and `0x01`. In the FM-index, byte `0x00` is used as a terminator by SDSL.
- The `HashDictionary` version described here uses internal 32-bit fields, with bounds checks for the input size and stored values.
- Index construction generates a temporary file containing the reversed text, along with auxiliary cache files. Sufficient disk space is required.
- The temporary directory can be configured using the `TMPDIR` environment variable.

The r-index construction process uses temporary structures, including a suffix array. Therefore, **a compact final index does not necessarily imply low memory consumption during construction**.

## How Factorization Works

The index is built over the reversed input text. This allows *backward search* operations to recognize phrases while the input symbols are processed from left to right.

The factorizer maintains a search interval `[sp, ep]` and the candidate phrase length `ℓ`. The dictionary maps this key to a phrase identifier:

```
(sp, ep, ℓ) → id
```

The length is necessary because patterns of different lengths may share the same BWT interval.

### Pseudocode

The following pseudocode summarizes phrase recognition. The `lookup` operation returns `0` when the queried key is not present in the dictionary.

```
FACTORIZE(S, index_type, dictionary_type):
    validate_input(S)

    I ← build_index(reverse(S), index_type)
    D ← create_dictionary(dictionary_type)

    k ← 0
    next_id ← 1

    while k < length(S):
        interval ← I.full_interval()
        phrase_length ← 0
        parent_id ← 0

        repeat:
            symbol ← S[k]
            interval ← I.backward_search(interval, symbol)
            phrase_length ← phrase_length + 1

            factor ← (parent_id, symbol)
            existing_id ← D.lookup(interval, phrase_length)

            k ← k + 1

            if existing_id = 0 or k = length(S):
                break

            parent_id ← existing_id

        D.insert(interval, phrase_length, next_id)
        emit(factor)

        next_id ← next_id + 1
```

The algorithm continues extending a phrase while the candidate sequence is already present in the dictionary. When an extension that has not yet been registered is found, it emits a new factor and starts recognizing the next phrase.

If the input ends with a phrase that is already present in the dictionary, the implementation still emits the final factor in `(parent_id, symbol)` format, without appending a special symbol.

The factorizer is implemented in `include/lz78_factorizer.hpp`.

During file-based encoding, the reversed text is stored on disk, input symbols are read as processing advances, and factors are written directly to the output file.

## Dependencies and References

### SDSL — Succinct Data Structure Library

A succinct data structure library used to implement the FM-index.

- [SDSL v3 — xxsds/sdsl-lite](https://github.com/xxsds/sdsl-lite)
- [Original SDSL — simongog/sdsl-lite](https://github.com/simongog/sdsl-lite)

The project uses SDSL v3 through the `sdsl-lite` submodule.

### r-index

The r-index implementation is based on Nicola Prezza's project, developed from the work of Travis Gagie, Gonzalo Navarro, and Nicola Prezza.

- [Original repository — nicolaprezza/r-index](https://github.com/nicolaprezza/r-index)
- Original implementation README
- MIT License
- Adaptation documentation

The `external/r-index` directory contains local adaptations for SDSL v3 and a variant focused on counting operations.

**Bibliographic reference:**

GAGIE, T.; NAVARRO, G.; PREZZA, N. *Fully Functional Suffix Trees and Optimal Text Searching in BWT-Runs Bounded Space*. Journal of the ACM, vol. 67, no. 1, article 2, 2020.