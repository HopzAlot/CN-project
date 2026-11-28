#ifndef PLACEMENT_H
#define PLACEMENT_H

#include <cstdint>
#include <map>
#include <mutex>
#include <utility>
#include <vector>

// Knows which peers hold which chunk. Updated by HAVE messages.
class ChunkMap {
public:
    void add_holder(uint32_t file_id, uint32_t index, uint32_t peer_id);
    void remove_peer(uint32_t peer_id);
    std::vector<uint32_t> holders(uint32_t file_id, uint32_t index) const;
        // Every (file_id, index) that has at least one holder, in sorted order.
    std::vector<std::pair<uint32_t, uint32_t>> all_chunks() const;
        // HAVE_MAP bitmap, chunk i is bit 7 - (i % 8) of byte i / 8, see docs/protocol.md
    void apply_bitmap(uint32_t file_id, uint32_t peer_id, uint32_t chunk_count,
                      const std::vector<uint8_t>& bitmap);
    std::vector<uint8_t> make_bitmap(uint32_t file_id, uint32_t peer_id,
                                     uint32_t chunk_count) const;

private:
    mutable std::mutex mtx_;
    std::map<std::pair<uint32_t, uint32_t>, std::vector<uint32_t>> map_;
};

// Picks REPLICATION different peers for one chunk. Returns false if there are too few peers.
bool choose_holders(uint32_t file_id, uint32_t index,
                    const std::vector<uint32_t>& peer_ids,
                    std::vector<uint32_t>& out);

#endif