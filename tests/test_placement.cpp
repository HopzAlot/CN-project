#include <algorithm>
#include <map>
#include <set>

#include "check.h"
#include "common/constants.h"
#include "storage/placement.h"

int main() {
    std::vector<uint32_t> peers = {40, 10, 30, 20};
    const uint32_t chunks = 153;
    std::map<uint32_t, int> count;

    for (uint32_t i = 0; i < chunks; i++) {
        std::vector<uint32_t> h;
        CHECK(choose_holders(1, i, peers, h));
        CHECK(h.size() == REPLICATION);
        CHECK(std::set<uint32_t>(h.begin(), h.end()).size() == REPLICATION);
        for (uint32_t p : h) {
            CHECK(std::find(peers.begin(), peers.end(), p) != peers.end());
            count[p]++;
        }
    }

    for (uint32_t p : peers) {
        CHECK(count[p] < static_cast<int>(chunks));            // nobody holds the whole file
        CHECK(count[p] > 60 && count[p] < 93);                 // load is roughly even
    }

    // same input, same answer, whatever the order of the peer list
    std::vector<uint32_t> a, b, shuffled = {20, 40, 10, 30};
    CHECK(choose_holders(1, 5, peers, a));
    CHECK(choose_holders(1, 5, shuffled, b));
    CHECK(a == b);

    // too few peers
    std::vector<uint32_t> one = {1}, h;
    CHECK(!choose_holders(1, 0, one, h));

    return test_result("placement");
}