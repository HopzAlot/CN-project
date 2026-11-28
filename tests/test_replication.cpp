#include "check.h"
#include "storage/replication.h"
#include "common/constants.h"

#include <algorithm>
#include <set>

static bool has_copy(const std::vector<CopyAction>& v, uint32_t f, uint32_t i, uint32_t from, uint32_t to) {
    for (const CopyAction& a : v)
        if (a.file_id == f && a.index == i && a.from == from && a.to == to) return true;
    return false;
}

int main() {
    // 4 peers (1..4), 8 chunks of file 7, placed round-robin like the real code
    std::vector<uint32_t> peers = {1, 2, 3, 4};
    ChunkMap map;
    for (uint32_t i = 0; i < 8; i++) {
        std::vector<uint32_t> h;
        CHECK(choose_holders(7, i, peers, h));
        for (uint32_t p : h) map.add_holder(7, i, p);
    }
    CHECK(map.all_chunks().size() == 8);
    CHECK(under_replicated(map, peers).empty());
    CHECK(plan_rereplication(map, peers).copies.empty());

    // peer 2 dies
    std::vector<uint32_t> live = {1, 3, 4};
    std::vector<ChunkId> under = under_replicated(map, live);
    CHECK(!under.empty());
    RereplicationPlan plan = plan_rereplication(map, live);
    CHECK(plan.lost.empty());
    CHECK(plan.copies.size() == under.size());
    for (const CopyAction& a : plan.copies) {
        std::vector<uint32_t> h = map.holders(a.file_id, a.index);
        CHECK(std::find(h.begin(), h.end(), a.from) != h.end());     // source holds it
        CHECK(a.from != 2 && a.to != 2);                              // dead peer not used
        CHECK(std::find(h.begin(), h.end(), a.to) == h.end());       // target does not hold it
    }
    // apply the plan: every chunk is back to REPLICATION live copies
    ChunkMap after;
    for (const ChunkId& c : map.all_chunks())
        for (uint32_t h : map.holders(c.first, c.second)) after.add_holder(c.first, c.second, h);
    after.remove_peer(2);
    for (const CopyAction& a : plan.copies) after.add_holder(a.file_id, a.index, a.to);
    CHECK(under_replicated(after, live).empty());

    // same input gives same plan
    RereplicationPlan plan2 = plan_rereplication(map, live);
    CHECK(plan2.copies.size() == plan.copies.size());
    for (size_t k = 0; k < plan.copies.size(); k++)
        CHECK(has_copy(plan2.copies, plan.copies[k].file_id, plan.copies[k].index,
                       plan.copies[k].from, plan.copies[k].to));

    // actions_from / actions_to split the plan
    size_t sent = 0, got = 0;
    for (uint32_t p : live) { sent += actions_from(plan.copies, p).size(); got += actions_to(plan.copies, p).size(); }
    CHECK(sent == plan.copies.size() && got == plan.copies.size());

    // chunk whose only holders are dead is reported lost
    ChunkMap m2;
    m2.add_holder(1, 0, 5);
    m2.add_holder(1, 0, 6);
    RereplicationPlan p3 = plan_rereplication(m2, std::vector<uint32_t>{1, 2});
    CHECK(p3.copies.empty() && p3.lost.size() == 1);

    // one survivor, one other live peer: copy goes survivor -> other
    ChunkMap m3;
    m3.add_holder(1, 0, 1);
    m3.add_holder(1, 0, 2);
    RereplicationPlan p4 = plan_rereplication(m3, std::vector<uint32_t>{1, 3});
    CHECK(p4.copies.size() == 1 && has_copy(p4.copies, 1, 0, 1, 3));

    // only one live peer: nothing can be copied, no crash
    RereplicationPlan p5 = plan_rereplication(m3, std::vector<uint32_t>{1});
    CHECK(p5.copies.empty() && p5.lost.empty());

    // join: peer 5 joins the 4 peers
    std::vector<uint32_t> five = {1, 2, 3, 4, 5};
    std::vector<CopyAction> js = plan_join_sync(map, five, 5);
    CHECK(!js.empty());
    CHECK(js.size() <= 8);
    for (const CopyAction& a : js) {
        std::vector<uint32_t> h = map.holders(a.file_id, a.index);
        CHECK(a.to == 5);
        CHECK(std::find(h.begin(), h.end(), a.from) != h.end());
    }
    // expected fair share: about 8*2/5 = 3 chunks
    CHECK(js.size() >= 2 && js.size() <= 4);
    // joining peer already holding things gets nothing extra; unknown peer gets nothing
    CHECK(plan_join_sync(map, five, 9).empty());
    CHECK(plan_join_sync(map, std::vector<uint32_t>{1}, 1).empty());
    CHECK(plan_join_sync(map, peers, 3).size() <= 8);

    return test_result("test_replication");
}