// Downloader against fake holders, no network. Time is simulated in 10 ms steps.

#include <cstdlib>
#include <deque>
#include <map>
#include <set>

#include "check.h"
#include "common/constants.h"
#include "common/crc32.h"
#include "transfer/chunk_sender.h"
#include "transfer/downloader.h"

enum Behaviour { GOOD, SILENT, CORRUPT, ERR };

struct Request { uint32_t peer, file, chunk; };

struct FakeNet {
    std::vector<uint8_t> file;
    std::map<uint32_t, Behaviour> peers;
    std::set<uint32_t> dead;
    std::deque<Request> queue;
    uint64_t now = 1000;
};

static FakeNet net;

static FileAnnounceMsg make_file(size_t size) {
    net.file.resize(size);
    for (size_t i = 0; i < size; i++) net.file[i] = (uint8_t)(std::rand() & 0xFF);
    FileAnnounceMsg f;
    f.file_id = 42;
    f.name = "test.bin";
    f.size = size;
    f.chunk_count = (uint32_t)((size + CHUNK_SIZE - 1) / CHUNK_SIZE);
    f.file_crc = crc32(net.file.data(), size);
    return f;
}

static bool fake_send(uint32_t peer, uint32_t file, uint32_t chunk) {
    net.queue.push_back(Request{peer, file, chunk});
    return true;
}

// Answers queued requests. Fragments are delivered backwards with one duplicate,
// as the transport only orders packets per stream and the test should not rely on it.
static void serve(Downloader& d) {
    std::deque<Request> q;
    q.swap(net.queue);
    for (const Request& r : q) {
        if (net.dead.count(r.peer)) continue;
        Behaviour b = net.peers[r.peer];
        if (b == SILENT) continue;
        if (b == ERR) {
            ChunkRefMsg e;
            e.file_id = r.file;
            e.chunk_index = r.chunk;
            d.on_chunk_err(r.peer, e, net.now);
            continue;
        }
        size_t off = (size_t)r.chunk * CHUNK_SIZE;
        size_t len = net.file.size() - off < CHUNK_SIZE ? net.file.size() - off : CHUNK_SIZE;
        std::vector<std::vector<uint8_t>> payloads;
        make_chunk_data(r.file, r.chunk, net.file.data() + off, len, payloads);
        // corrupt holder: crc was computed on good data, the bytes on the wire are damaged
        if (b == CORRUPT)
            for (auto& p : payloads) p[CHUNK_DATA_FIELDS] ^= 0x01;
        for (int k = (int)payloads.size() - 1; k >= 0; k--) {
            ChunkDataMsg m;
            decode_chunk_data(payloads[k].data(), (int)payloads[k].size(), m);
            d.on_chunk_data(r.peer, m, net.now);
            if (k == 0) d.on_chunk_data(r.peer, m, net.now);
        }
    }
}

static DownloadState run(Downloader& d, uint64_t limit_ms) {
    uint64_t end = net.now + limit_ms;
    while (net.now < end && d.state() == DL_RUNNING) {
        serve(d);
        net.now += 10;
        d.tick(net.now);
    }
    return d.state();
}

static std::vector<std::vector<uint32_t>> holders_all(uint32_t chunks, std::vector<uint32_t> peers) {
    return std::vector<std::vector<uint32_t>>(chunks, peers);
}

static void reset(std::map<uint32_t, Behaviour> peers) {
    net.peers = peers;
    net.dead.clear();
    net.queue.clear();
}

static bool same_file(Downloader& d) {
    std::vector<uint8_t> out;
    return d.take_file(out) && out == net.file;
}

static void test_all_good() {
    reset({{1, GOOD}, {2, GOOD}, {3, GOOD}});
    FileAnnounceMsg f = make_file(10 * CHUNK_SIZE + 777);   // last chunk is short
    Downloader d;
    CHECK(d.start(f, holders_all(f.chunk_count, {1, 2, 3}), fake_send, net.now));
    CHECK(run(d, 10000) == DL_DONE);
    CHECK(same_file(d));
    DownloadStats s = d.stats();
    // multi-source: every holder delivered something
    CHECK(s.chunks_from[1] > 0 && s.chunks_from[2] > 0 && s.chunks_from[3] > 0);
    CHECK(s.requests == (int)f.chunk_count);
}

static void test_silent_holder() {
    reset({{1, GOOD}, {2, SILENT}});
    FileAnnounceMsg f = make_file(6 * CHUNK_SIZE);
    Downloader d;
    d.start(f, holders_all(f.chunk_count, {1, 2}), fake_send, net.now);
    CHECK(run(d, 60000) == DL_DONE);
    CHECK(same_file(d));
    CHECK(d.stats().timeouts > 0);
    CHECK(d.stats().chunks_from[2] == 0);
}

static void test_corrupt_holder() {
    reset({{1, CORRUPT}, {2, GOOD}});
    FileAnnounceMsg f = make_file(5 * CHUNK_SIZE + 1);
    Downloader d;
    d.start(f, holders_all(f.chunk_count, {1, 2}), fake_send, net.now);
    CHECK(run(d, 60000) == DL_DONE);
    CHECK(same_file(d));
    CHECK(d.stats().crc_failures > 0);
}

static void test_err_holder() {
    reset({{1, ERR}, {2, GOOD}});
    FileAnnounceMsg f = make_file(4 * CHUNK_SIZE);
    Downloader d;
    d.start(f, holders_all(f.chunk_count, {1, 2}), fake_send, net.now);
    CHECK(run(d, 60000) == DL_DONE);
    CHECK(same_file(d));
    CHECK(d.stats().errors > 0);
}

static void test_failover() {
    // peer 2 crashed but membership only notices after PEER_TIMEOUT_MS
    reset({{1, GOOD}, {2, GOOD}, {3, GOOD}});
    FileAnnounceMsg f = make_file(40 * CHUNK_SIZE);
    Downloader d;
    // chunks alternate between {1,2} and {2,3}, like real placement
    std::vector<std::vector<uint32_t>> h(f.chunk_count);
    for (uint32_t i = 0; i < f.chunk_count; i++) h[i] = (i % 2) ? std::vector<uint32_t>{1, 2} : std::vector<uint32_t>{2, 3};
    net.peers[2] = SILENT;   // peer 2 looks alive but has crashed: requests to it vanish
    d.start(f, h, fake_send, net.now);
    uint64_t t0 = net.now;
    while (d.state() == DL_RUNNING && net.now - t0 < 60000) {
        if (net.now - t0 == PEER_TIMEOUT_MS) d.on_peer_dead(2, net.now);
        serve(d);
        net.now += 10;
        d.tick(net.now);
    }
    CHECK(d.state() == DL_DONE);
    CHECK(same_file(d));
    DownloadStats s = d.stats();
    CHECK(s.chunks_from[2] == 0);
    CHECK(s.chunks_from[1] > 0 && s.chunks_from[3] > 0);
}

static void test_failover_report() {
    // peer dies while it has requests in flight: they move at once, not after CHUNK_TIMEOUT_MS
    reset({{1, SILENT}, {2, GOOD}});
    FileAnnounceMsg f = make_file(4 * CHUNK_SIZE);
    Downloader d;
    d.start(f, holders_all(f.chunk_count, {1, 2}), fake_send, net.now);
    d.on_peer_dead(1, net.now);
    CHECK(d.stats().failovers > 0);
    CHECK(run(d, 1000) == DL_DONE);   // well under CHUNK_TIMEOUT_MS
    CHECK(d.stats().last_failover_ms >= 0);
    CHECK(d.stats().timeouts == 0);
    CHECK(same_file(d));
}

static void test_no_holder() {
    reset({{1, GOOD}});
    FileAnnounceMsg f = make_file(3 * CHUNK_SIZE);
    Downloader d;
    d.start(f, holders_all(f.chunk_count, {1}), fake_send, net.now);
    net.queue.clear();
    d.on_peer_dead(1, net.now);
    CHECK(run(d, NO_HOLDER_WAIT_MS + 1000) == DL_FAILED);
    CHECK(!d.fail_reason().empty());

    // same, but re-replication puts the chunks on peer 5 in time
    reset({{1, GOOD}, {5, GOOD}});
    Downloader d2;
    d2.start(f, holders_all(f.chunk_count, {1}), fake_send, net.now);
    net.queue.clear();
    d2.on_peer_dead(1, net.now);
    net.now += 2000;
    d2.tick(net.now);
    for (uint32_t i = 0; i < f.chunk_count; i++) d2.on_holder_added(f.file_id, i, 5, net.now);
    CHECK(run(d2, 5000) == DL_DONE);
    CHECK(same_file(d2));
}

static void test_bad_input() {
    reset({{1, GOOD}});
    FileAnnounceMsg f = make_file(3 * CHUNK_SIZE);
    Downloader d;
    f.chunk_count = 2;
    CHECK(!d.start(f, holders_all(2, {1}), fake_send, net.now));

    FileAnnounceMsg e = make_file(0);
    Downloader d2;
    CHECK(d2.start(e, {}, fake_send, net.now));
    CHECK(d2.state() == DL_DONE);
}

int main() {
    std::srand(1);
    test_all_good();
    test_silent_holder();
    test_corrupt_holder();
    test_err_holder();
    test_failover();
    test_failover_report();
    test_no_holder();
    test_bad_input();
    return test_result("downloader");
}
