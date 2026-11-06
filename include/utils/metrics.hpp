#pragma once

#include <dictionaries/lz78_dictionary.hpp>
#include <utils/progress.hpp>

#include <chrono>
#include <iomanip>
#include <locale>
#include <ostream>
#include <string>
#include <vector>

inline void write_json_string(std::ostream& out, const std::string& value) {
    static constexpr char hex[] = "0123456789abcdef";
    out << '"';
    for (unsigned char c : value) {
        if (c == '"' || c == '\\') {
            out << '\\' << static_cast<char>(c);
        } else if (c < 0x20) {
            out << "\\u00" << hex[c >> 4] << hex[c & 15];
        } else {
            out << static_cast<char>(c);
        }
    }
    out << '"';
}

struct PhaseMetrics {
    std::string name;
    double seconds;
    uint64_t rss_begin_kib;
    uint64_t rss_end_kib;
    uint64_t process_peak_rss_kib;
};

struct ProgressMetrics {
    uint64_t processed_bytes;
    uint64_t factor_count;
    uint64_t dictionary_entries;
    double elapsed_seconds;
    uint64_t rss_kib;
    std::optional<DictionaryMemoryStats> hash;
};

struct RunMetrics {
    using Clock = std::chrono::steady_clock;
    Clock::time_point started = Clock::now();
    std::vector<PhaseMetrics> phases;
    std::vector<ProgressMetrics> progress_samples;

    double elapsed_seconds() const {
        return std::chrono::duration<double>(Clock::now() - started).count();
    }

    template <typename Action>
    void measure(const std::string& name, Action action) {
        const uint64_t rss_begin = current_rss_kb();
        const auto begin = Clock::now();
        action();
        const double seconds = std::chrono::duration<double>(Clock::now() - begin).count();
        phases.push_back({name, seconds, rss_begin, current_rss_kb(), peak_rss_kb()});
    }

    void sample(uint64_t processed, uint64_t factors, LZ78Dictionary& dictionary) {
        progress_samples.push_back({processed, factors, dictionary.number_of_dictionary_entries(),
            elapsed_seconds(), current_rss_kb(), dictionary.memory_stats()});
    }
};

inline void write_hash_metrics(std::ostream& out, const std::optional<DictionaryMemoryStats>& hash) {
    out << "\"hash_allocated_bytes\":";
    if (hash) out << hash->allocated_bytes; else out << "null";
    out << ",\"hash_peak_allocated_bytes\":";
    if (hash) out << hash->peak_allocated_bytes; else out << "null";
    out << ",\"hash_buckets\":";
    if (hash) out << hash->buckets; else out << "null";
    out << ",\"hash_load_factor\":";
    if (hash) out << hash->load_factor; else out << "null";
    out << ",\"hash_max_load_factor\":";
    if (hash) out << hash->max_load_factor; else out << "null";
}

inline void write_metrics_json(
    std::ostream& out, const RunMetrics& metrics, const std::string& input_path,
    const std::string& output_path, const std::string& dictionary_name,
    uint64_t input_bytes, uint64_t output_bytes, uint64_t factors,
    const BWTIndex& index, LZ78Dictionary& dictionary
) {
    out.imbue(std::locale::classic());
    out << std::fixed << std::setprecision(6);
    const auto hash = dictionary.memory_stats();
    const uint64_t index_bytes = index.size_in_bytes();
    const bool is_fm = std::string(index.name()) == "fm";
    out << "{\"schema_version\":2,\"index\":";
    write_json_string(out, index.name());
    out << ",\"input_path\":";
    write_json_string(out, input_path);
    out << ",\"output_path\":";
    write_json_string(out, output_path);
    out << ",\"dictionary\":";
    write_json_string(out, dictionary_name);
    out << ",\"compiler\":";
    write_json_string(out, __VERSION__);
    out << ",\"input_bytes\":" << input_bytes
        << ",\"output_bytes\":" << output_bytes
        << ",\"factor_count\":" << factors
        << ",\"dictionary_entries\":" << dictionary.number_of_dictionary_entries()
        << ",\"index_serialized_bytes\":" << index_bytes
        << ",\"bwt_runs\":";
    if (auto runs = index.number_of_runs()) out << *runs; else out << "null";
    out << ",\"fm_serialized_bytes\":";
    if (is_fm) out << index_bytes; else out << "null";
    out << ',';
    write_hash_metrics(out, hash);
    out << ",\"fm_hash_estimated_bytes\":";
    if (is_fm && hash) out << index_bytes + hash->allocated_bytes; else out << "null";
    out << ",\"index_hash_estimated_bytes\":";
    if (hash) out << index_bytes + hash->allocated_bytes; else out << "null";
    out << ",\"elapsed_seconds\":" << metrics.elapsed_seconds()
        << ",\"rss_final_kib\":" << current_rss_kb()
        << ",\"process_peak_rss_kib\":" << peak_rss_kb()
        << ",\"phases\":[";
    bool first = true;
    for (const auto& phase : metrics.phases) {
        if (!first) out << ',';
        first = false;
        out << "{\"name\":";
        write_json_string(out, phase.name);
        out << ",\"seconds\":" << phase.seconds
            << ",\"rss_begin_kib\":" << phase.rss_begin_kib
            << ",\"rss_end_kib\":" << phase.rss_end_kib
            << ",\"process_peak_rss_kib\":" << phase.process_peak_rss_kib << '}';
    }
    out << "],\"progress_samples\":[";
    first = true;
    for (const auto& sample : metrics.progress_samples) {
        if (!first) out << ',';
        first = false;
        out << "{\"processed_bytes\":" << sample.processed_bytes
            << ",\"factor_count\":" << sample.factor_count
            << ",\"dictionary_entries\":" << sample.dictionary_entries
            << ",\"elapsed_seconds\":" << sample.elapsed_seconds
            << ",\"rss_kib\":" << sample.rss_kib << ',';
        write_hash_metrics(out, sample.hash);
        out << '}';
    }
    out << "]}\n";
}

void print_memory_report(const BWTIndex& index, uint64_t index_bytes, LZ78Dictionary& dictionary, uint64_t factors) {
    const double index_mib = index_bytes / (1024.0 * 1024.0);

    std::cerr << "[lz78] memory"
              << " index=" << index.name()
              << " index_bytes=" << index_bytes
              << " index_mib=" << index_mib
              << " factor_count=" << factors
              << " dictionary_entries=" << dictionary.number_of_dictionary_entries();
    if (const auto hash = dictionary.memory_stats()) {
        std::cerr << " hash_allocated_bytes=" << hash->allocated_bytes
                  << " hash_peak_allocated_bytes=" << hash->peak_allocated_bytes
                  << " hash_buckets=" << hash->buckets
                  << " hash_load_factor=" << hash->load_factor
                  << " index_hash_estimated_bytes=" << index_bytes + hash->allocated_bytes;
    }
    std::cerr
              << " rss_kb=" << current_rss_kb()
              << '\n';
}