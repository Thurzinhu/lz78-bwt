#pragma once

#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string>

inline bool lz78_progress_enabled() {
    const char* value = std::getenv("LZ78_PROGRESS");
    return value != nullptr && value[0] != '\0' && value[0] != '0';
}

inline uint64_t current_rss_kb() {
    std::ifstream status("/proc/self/status");
    std::string key;

    while (status >> key) {
        if (key == "VmRSS:") {
            uint64_t value = 0;
            status >> value;
            return value;
        }

        std::string rest;
        std::getline(status, rest);
    }

    return 0;
}

inline void log_lz78_progress(const std::string& message) {
    if (!lz78_progress_enabled()) {
        return;
    }

    std::cerr << "[lz78] " << message
              << " rss_kb=" << current_rss_kb()
              << '\n';
}
