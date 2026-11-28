#ifndef REPLICATION_H
#define REPLICATION_H

#include "storage/placement.h"

#include <cstdint>
#include <utility>
#include <vector>

// Planning only. These functions decide WHO copies WHICH chunk to WHOM, from the ChunkMap and the
// list of live peers. They send nothing. Every peer runs the same function on the same data and
// gets the same plan, so each peer just does the actions that are its own (actions_from).

typedef std::pair<uint32_t, uint32_t> ChunkId;     // (file_id, index)

// Peer `from` sends chunk (file_id, index) to peer `to`.
struct CopyAction {
    uint32_t file_id;
    uint32_t index;
    uint32_t from;
    uint32_t to;
};

struct RereplicationPlan {
    std::vector<CopyAction> copies;     // chunks that get a new holder
    std::vector<ChunkId> lost;          // no live holder left, nobody can copy these
};

// Chunks with fewer than REPLICATION live holders (chunks with no live holder included).
std::vector<ChunkId> under_replicated(const ChunkMap& map, const std::vector<uint32_t>& live_peers);

// A peer died. For every chunk with fewer than REPLICATION live holders: the lowest id live holder
// sends it to the least loaded live peer that does not hold it yet (ties go to the lowest id).
RereplicationPlan plan_rereplication(const ChunkMap& map, const std::vector<uint32_t>& live_peers);

// A peer joined. `peers` is the sorted-or-not list of all live peers including new_peer. The new peer
// takes the chunks that round-robin placement (choose_holders) would have given it, about
// REPLICATION / n of all chunks. Each copy comes from a current holder, spread over the holders.
// The old holders keep their copy.
std::vector<CopyAction> plan_join_sync(const ChunkMap& map, const std::vector<uint32_t>& peers,
                                       uint32_t new_peer);

// The actions peer `self` has to send (from == self) or has to receive (to == self).
std::vector<CopyAction> actions_from(const std::vector<CopyAction>& plan, uint32_t self);
std::vector<CopyAction> actions_to(const std::vector<CopyAction>& plan, uint32_t self);

#endif