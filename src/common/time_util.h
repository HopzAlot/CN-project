#ifndef TIME_UTIL_H
#define TIME_UTIL_H

#include <chrono>
#include <cstdint>

// Monotonic milliseconds, for timeouts and measurements. Not wall-clock time.
inline uint64_t now_ms() {
    return (uint64_t)std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::steady_clock::now().time_since_epoch())
        .count();
}

#endif
