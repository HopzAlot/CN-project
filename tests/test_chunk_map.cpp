#include "check.h"
#include "storage/placement.h"

int main() {
    ChunkMap m;
    CHECK(m.holders(1, 0).empty());
    m.add_holder(1, 0, 10);
    m.add_holder(1, 0, 10);                 // same peer twice counts once
    m.add_holder(1, 0, 20);
    CHECK(m.holders(1, 0).size() == 2);
    m.remove_peer(10);
    CHECK(m.holders(1, 0).size() == 1 && m.holders(1, 0)[0] == 20);
    m.remove_peer(20);
    CHECK(m.holders(1, 0).empty());

    // bitmap: peer 5 holds chunks 0, 3 and 9 of file 2
    ChunkMap a;
    a.add_holder(2, 0, 5);
    a.add_holder(2, 3, 5);
    a.add_holder(2, 9, 5);
    a.add_holder(2, 4, 6);                  // another peer, must not appear
    std::vector<uint8_t> bm = a.make_bitmap(2, 5, 10);
    CHECK(bm.size() == 2);
    CHECK(bm[0] == 0x90);                   // chunks 0 and 3
    CHECK(bm[1] == 0x40);                   // chunk 9, unused bits stay 0
    CHECK(a.make_bitmap(2, 5, 153).size() == 20);

    // a new peer rebuilds the map from the bitmap
    ChunkMap b;
    b.apply_bitmap(2, 5, 10, bm);
    CHECK(b.holders(2, 0).size() == 1 && b.holders(2, 0)[0] == 5);
    CHECK(b.holders(2, 3).size() == 1);
    CHECK(b.holders(2, 9).size() == 1);
    CHECK(b.holders(2, 1).empty());
    CHECK(b.holders(2, 4).empty());

    return test_result("chunk_map");
}