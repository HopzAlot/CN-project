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

void ChunkMap::add_holder(uint32_t file_id, uint32_t index, uint32_t peer_id) {
    std::lock_guard<std::mutex> lock(mtx_);
    std::vector<uint32_t>& v = map_[std::make_pair(file_id, index)];
    if (std::find(v.begin(), v.end(), peer_id) == v.end()) v.push_back(peer_id);
}

void ChunkMap::remove_peer(uint32_t peer_id) {
    std::lock_guard<std::mutex> lock(mtx_);
    for (auto it = map_.begin(); it != map_.end();) {
        std::vector<uint32_t>& v = it->second;
        v.erase(std::remove(v.begin(), v.end(), peer_id), v.end());
        if (v.empty()) it = map_.erase(it);
        else ++it;
    }
}

std::vector<uint32_t> ChunkMap::holders(uint32_t file_id, uint32_t index) const {
    std::lock_guard<std::mutex> lock(mtx_);
    auto it = map_.find(std::make_pair(file_id, index));
    if (it == map_.end()) return std::vector<uint32_t>();
    return it->second;
}

std::vector<std::pair<uint32_t, uint32_t>> ChunkMap::all_chunks() const {
    std::lock_guard<std::mutex> lock(mtx_);
    std::vector<std::pair<uint32_t, uint32_t>> out;
    out.reserve(map_.size());
    for (const auto& kv : map_) out.push_back(kv.first);    // std::map keeps them sorted
    return out;
}

void ChunkMap::apply_bitmap(uint32_t file_id, uint32_t peer_id, uint32_t chunk_count,
                            const std::vector<uint8_t>& bitmap) {
    for (uint32_t i = 0; i < chunk_count; i++) {
        size_t byte = i / 8;
        if (byte >= bitmap.size()) break;
        if (bitmap[byte] & (0x80 >> (i % 8))) add_holder(file_id, i, peer_id);
    }
}

std::vector<uint8_t> ChunkMap::make_bitmap(uint32_t file_id, uint32_t peer_id,
                                           uint32_t chunk_count) const {
    std::vector<uint8_t> bm((chunk_count + 7) / 8, 0);
    std::lock_guard<std::mutex> lock(mtx_);
    for (uint32_t i = 0; i < chunk_count; i++) {
        auto it = map_.find(std::make_pair(file_id, i));
        if (it == map_.end()) continue;
        if (std::find(it->second.begin(), it->second.end(), peer_id) != it->second.end()) {
            bm[i / 8] |= static_cast<uint8_t>(0x80 >> (i % 8));
        }
    }
    return bm;
}