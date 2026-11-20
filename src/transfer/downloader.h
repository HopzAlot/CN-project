#ifndef DOWNLOADER_H
#define DOWNLOADER_H

#include <cstdint>
#include <functional>
#include <map>
#include <mutex>
#include <set>
#include <string>
#include <vector>

#include "protocol/file_messages.h"

// Downloads one file from many holders at once.
// It does not touch the network: it asks for chunks through RequestFn and is fed
// CHUNK_DATA / CHUNK_ERR / peer-death events by whoever owns the socket.
// Every call that needs the time takes `now` (ms, from now_ms()), so tests can fake the clock.
// Thread-safe: receiver thread, timer thread and CLI may all call in.

enum DownloadState { DL_IDLE, DL_RUNNING, DL_DONE, DL_FAILED };

struct DownloadStats {
    uint64_t start_ms = 0;
    uint64_t end_ms = 0;
    int requests = 0;        // CHUNK_REQUESTs sent
    int timeouts = 0;
    int crc_failures = 0;
    int errors = 0;          // CHUNK_ERR received or send failed
    int failovers = 0;       // in-flight chunks moved because their holder died
    int64_t last_failover_ms = -1;   // peer death -> all its chunks received elsewhere
    std::map<uint32_t, int> chunks_from;   // peer id -> chunks delivered
};

class Downloader {
public:
    // Returns false if the request could not be handed to the transport.
    typedef std::function<bool(uint32_t peer_id, uint32_t file_id, uint32_t chunk_index)> RequestFn;

    // holders[i] = peers that have chunk i. Fails if sizes do not match the announce.
    bool start(const FileAnnounceMsg& file, const std::vector<std::vector<uint32_t>>& holders,
               RequestFn send_request, uint64_t now);

    void on_chunk_data(uint32_t from, const ChunkDataMsg& m, uint64_t now);
    void on_chunk_err(uint32_t from, const ChunkRefMsg& m, uint64_t now);
    void on_peer_dead(uint32_t peer, uint64_t now);
    void on_holder_added(uint32_t file_id, uint32_t chunk_index, uint32_t peer, uint64_t now);

    // Call every ~50-100 ms: handles timeouts and sends new requests.
    void tick(uint64_t now);

    DownloadState state();
    std::string fail_reason();
    uint32_t file_id();
    void progress(uint32_t& done, uint32_t& total);
    DownloadStats stats();

    // After DL_DONE: the whole file, already checked against the file crc32.
    bool take_file(std::vector<uint8_t>& out);

private:
    enum SlotStatus { WAITING, IN_FLIGHT, DONE };

    struct ChunkSlot {
        SlotStatus status = WAITING;
        std::vector<uint32_t> holders;
        std::set<uint32_t> bad;         // holders that failed this chunk
        uint32_t peer = 0;              // current holder while IN_FLIGHT
        uint64_t sent_ms = 0;
        uint64_t no_holder_since = 0;   // 0 = has a usable holder
        int tries = 0;
        uint32_t expect_len = 0;
        uint16_t expect_frags = 0;
        uint32_t crc = 0;
        std::vector<uint8_t> buf;
        std::vector<bool> got;
        int got_count = 0;
    };

    struct Pending { uint32_t peer, file_id, chunk; };

    std::mutex mu;
    DownloadState st = DL_IDLE;
    std::string reason;
    FileAnnounceMsg info;
    std::vector<ChunkSlot> slots;
    std::set<uint32_t> dead;
    std::map<uint32_t, int> inflight_per_peer;
    int inflight = 0;
    uint32_t done_count = 0;
    RequestFn send_fn;
    DownloadStats st_stats;
    std::vector<uint8_t> result;
    uint64_t failover_start = 0;
    std::set<uint32_t> failover_chunks;

    // all helpers below expect mu to be held
    void fail(const std::string& why, uint64_t now);
    void release(ChunkSlot& s, uint32_t index, bool holder_failed, uint64_t now);   // undo IN_FLIGHT
    bool pick_holder(ChunkSlot& s, uint32_t& out);
    void fill_window(std::vector<Pending>& out, uint64_t now);
    void finish(uint64_t now);
    void send_pending(const std::vector<Pending>& p, uint64_t now);
};

#endif
