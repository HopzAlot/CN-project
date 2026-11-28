#include "storage/replication.h"
#include "common/constants.h"

#include <algorithm>
#include <map>
#include <set>

namespace {

bool contains(const std::vector<uint32_t>& v, uint32_t x) {
    return std::find(v.begin(), v.end(), x) != v.end();
}

// holders of one chunk that are alive, sorted by id
std::vector<uint32_t> live_holders(const ChunkMap& map, const ChunkId& c, const std::set<uint32_t>& live) {
    std::vector<uint32_t> out;
    for (uint32_t h : map.holders(c.first, c.second)) {
        if (live.count(h)) out.push_back(h);
    }
    std::sort(out.begin(), out.end());
    return out;
}

}  // namespace

std::vector<ChunkId> under_replicated(const ChunkMap& map, const std::vector<uint32_t>& live_peers) {
    std::set<uint32_t> live(live_peers.begin(), live_peers.end());
    std::vector<ChunkId> out;
    for (const ChunkId& c : map.all_chunks()) {
        if (live_holders(map, c, live).size() < REPLICATION) out.push_back(c);
    }
    return out;
}

RereplicationPlan plan_rereplication(const ChunkMap& map, const std::vector<uint32_t>& live_peers) {
    std::set<uint32_t> live(live_peers.begin(), live_peers.end());
    std::vector<ChunkId> chunks = map.all_chunks();

    // how many chunks every live peer holds now, so the new copies go to the least loaded peer
    std::map<uint32_t, size_t> load;            // sorted by id, so ties go to the lowest id
    for (uint32_t p : live) load[p] = 0;
    for (const ChunkId& c : chunks) {
        for (uint32_t h : live_holders(map, c, live)) load[h]++;
    }

    RereplicationPlan plan;
    for (const ChunkId& c : chunks) {
        std::vector<uint32_t> alive = live_holders(map, c, live);
        if (alive.empty()) {
            plan.lost.push_back(c);
            continue;
        }
        if (alive.size() >= REPLICATION) continue;

        uint32_t from = alive.front();          // lowest id surviving holder does the copy
        size_t need = REPLICATION - alive.size();
        for (size_t k = 0; k < need; k++) {
            bool found = false;
            uint32_t best = 0;
            size_t best_load = 0;
            for (const auto& kv : load) {
                if (contains(alive, kv.first)) continue;
                if (!found || kv.second < best_load) {
                    found = true;
                    best = kv.first;
                    best_load = kv.second;
                }
            }
            if (!found) break;                  // fewer live peers than REPLICATION, nothing to do
            CopyAction a;
            a.file_id = c.first;
            a.index = c.second;
            a.from = from;
            a.to = best;
            plan.copies.push_back(a);
            alive.push_back(best);
            load[best]++;
        }
    }
    return plan;
}

std::vector<CopyAction> plan_join_sync(const ChunkMap& map, const std::vector<uint32_t>& peers,
                                       uint32_t new_peer) {
    std::vector<CopyAction> plan;
    std::vector<uint32_t> sorted = peers;
    std::sort(sorted.begin(), sorted.end());
    sorted.erase(std::unique(sorted.begin(), sorted.end()), sorted.end());

    auto it = std::find(sorted.begin(), sorted.end(), new_peer);
    if (it == sorted.end() || sorted.size() < 2) return plan;
    size_t n = sorted.size();
    size_t rank = static_cast<size_t>(it - sorted.begin());

    for (const ChunkId& c : map.all_chunks()) {
        std::vector<uint32_t> holders = map.holders(c.first, c.second);
        if (holders.empty() || contains(holders, new_peer)) continue;

        // same round-robin as choose_holders: the chunk starts at peer (file_id + index) % n and
        // goes to REPLICATION consecutive peers. The new peer takes the chunks that would include it.
        bool mine = false;
        for (size_t k = 0; k < REPLICATION; k++) {
            if ((static_cast<size_t>(c.first) + c.second + k) % n == rank) mine = true;
        }
        if (!mine) continue;

        std::sort(holders.begin(), holders.end());
        CopyAction a;
        a.file_id = c.first;
        a.index = c.second;
        a.from = holders[(static_cast<size_t>(c.first) + c.second) % holders.size()];
        a.to = new_peer;
        plan.push_back(a);
    }
    return plan;
}

std::vector<CopyAction> actions_from(const std::vector<CopyAction>& plan, uint32_t self) {
    std::vector<CopyAction> out;
    for (const CopyAction& a : plan) if (a.from == self) out.push_back(a);
    return out;
}

std::vector<CopyAction> actions_to(const std::vector<CopyAction>& plan, uint32_t self) {
    std::vector<CopyAction> out;
    for (const CopyAction& a : plan) if (a.to == self) out.push_back(a);
    return out;
}