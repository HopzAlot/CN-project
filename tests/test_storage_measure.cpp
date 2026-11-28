// Storage measurement for the report. Prints storage per peer for different replication factors R
// and peer counts N, using the same round-robin rule as choose_holders. Also checks that rule
// against the real choose_holders for R = REPLICATION.
#include "check.h"
#include "storage/placement.h"
#include "common/constants.h"

#include <algorithm>
#include <cstdio>
#include <vector>

// same rule as choose_holders, but with R as a parameter
static std::vector<uint32_t> holders_r(uint32_t file_id, uint32_t index, size_t n, size_t r) {
    std::vector<uint32_t> out;
    size_t start = (static_cast<size_t>(file_id) + index) % n;
    for (size_t k = 0; k < r && k < n; k++) out.push_back(static_cast<uint32_t>((start + k) % n));
    return out;
}

int main() {
    const size_t file_bytes = 1024 * 1024;                                   // 1 MiB test file
    const size_t chunks = (file_bytes + CHUNK_SIZE - 1) / CHUNK_SIZE;        // 64 chunks

    // cross-check with the real function
    for (size_t n = 2; n <= 8; n++) {
        std::vector<uint32_t> ids;
        for (size_t i = 0; i < n; i++) ids.push_back(static_cast<uint32_t>(i + 1));
        for (uint32_t idx = 0; idx < chunks; idx++) {
            std::vector<uint32_t> real;
            CHECK(choose_holders(3, idx, ids, real));
            std::vector<uint32_t> mine = holders_r(3, idx, n, REPLICATION);
            CHECK(real.size() == mine.size());
            for (size_t k = 0; k < mine.size() && k < real.size(); k++) CHECK(real[k] == ids[mine[k]]);
        }
    }

    std::printf("file = %zu bytes, chunk = %d bytes, %zu chunks\n", file_bytes, (int)CHUNK_SIZE, chunks);
    std::printf("%2s %2s %12s %12s %12s %12s\n", "R", "N", "chunks/peer", "KiB/peer", "total KiB", "peer-loss ok");
    for (size_t r = 1; r <= 3; r++) {
        for (size_t n = 2; n <= 8; n++) {
            if (r > n) continue;
            std::vector<size_t> per(n, 0);
            for (uint32_t idx = 0; idx < chunks; idx++)
                for (uint32_t p : holders_r(3, idx, n, r)) per[p]++;
            size_t mx = *std::max_element(per.begin(), per.end());
            size_t mn = *std::min_element(per.begin(), per.end());
            size_t total = 0;
            for (size_t c : per) total += c;
            CHECK(total == chunks * r);
            CHECK(mx - mn <= r);                                             // load stays even
            std::printf("%2zu %2zu %5zu..%-5zu %12.1f %12.1f %12s\n", r, n, mn, mx,
                        mx * CHUNK_SIZE / 1024.0, total * CHUNK_SIZE / 1024.0, r >= 2 ? "yes" : "no");
        }
    }
    return test_result("test_storage_measure");
}