#include "storage/placement.h"
#include "common/constants.h"

#include <algorithm>

bool choose_holders(uint32_t file_id, uint32_t index,
                    const std::vector<uint32_t>& peer_ids,
                    std::vector<uint32_t>& out) {
    out.clear();
    if (peer_ids.size() < REPLICATION) return false;

    // sorted, so every peer computes the same answer from the same peer list
    std::vector<uint32_t> sorted = peer_ids;
    std::sort(sorted.begin(), sorted.end());

    // round robin: consecutive chunks start at consecutive peers, so the load is even
    // and no peer ends up with the whole file
    size_t n = sorted.size();
    size_t start = (static_cast<size_t>(file_id) + index) % n;
    for (size_t k = 0; k < REPLICATION; k++) {
        out.push_back(sorted[(start + k) % n]);
    }
    return true;
}