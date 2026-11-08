#pragma once

#include <cstddef>
#include <cstdint>

struct AstreaIconDirectory {
    std::int32_t size;
    std::int32_t scale;
    std::int32_t minSize;
    std::int32_t maxSize;
    std::int32_t threshold;
    std::uint8_t type; // Fixed, Scalable, Threshold, in this order.
};

// The callback is synchronous. A successful lookup publishes the first
// available asset in that directory, in the caller's root/extension order.
extern "C" bool astrea_icon_choose_directory(
    const AstreaIconDirectory *directories, std::size_t count,
    std::int32_t size, std::int32_t scale,
    bool (*lookup)(void *, std::size_t), void *context);
