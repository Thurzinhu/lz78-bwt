#pragma once

#include <cstdint>

struct Factor {
    uint64_t id;
    uint64_t parent_id;
    unsigned char symbol;
    uint64_t length;
    uint64_t sp;
    uint64_t ep;
};