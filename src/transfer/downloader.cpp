#include "transfer/downloader.h"

#include "common/constants.h"
#include "common/crc32.h"

bool Downloader::start(const FileAnnounceMsg& file, const std::vector<std::vector<uint32_t>>& holders,
                       RequestFn send_request, uint64_t now) {
    std::vector<Pending> pending;
    {
        std::lock_guard<std::mutex> lock(mu);
        if (st == DL_RUNNING) return false;
        uint64_t expect_chunks = (file.size + CHUNK_SIZE - 1) / CHUNK_SIZE;
        if (file.chunk_count != expect_chunks || holders.size() != file.chunk_count) return false;

        info = file;
        send_fn = send_request;
        st = DL_RUNNING;
        reason.clear();
        result.clear();
        dead.clear();
        inflight_per_peer.clear();
        inflight = 0;
        done_count = 0;
        failover_chunks.clear();
        st_stats = DownloadStats();
        st_stats.start_ms = now;

        slots.assign(file.chunk_count, ChunkSlot());
        for (uint32_t i = 0; i < file.chunk_count; i++) {
            ChunkSlot& s = slots[i];
            s.holders = holders[i];
            uint64_t left = file.size - (uint64_t)i * CHUNK_SIZE;
            s.expect_len = (uint32_t)(left < CHUNK_SIZE ? left : CHUNK_SIZE);
            s.expect_frags = (uint16_t)((s.expect_len + FRAGMENT_SIZE - 1) / FRAGMENT_SIZE);
        }
        if (file.chunk_count == 0) finish(now);
        else fill_window(pending, now);
    }
    send_pending(pending, now);
    return true;
}

void Downloader::on_chunk_data(uint32_t from, const ChunkDataMsg& m, uint64_t now) {
    std::vector<Pending> pending;
    {
        std::lock_guard<std::mutex> lock(mu);
        if (st != DL_RUNNING || m.file_id != info.file_id || m.chunk_index >= slots.size()) return;
        ChunkSlot& s = slots[m.chunk_index];
        // late data from a holder we already gave up on is ignored
        if (s.status != IN_FLIGHT || s.peer != from) return;

        bool last = (m.frag_no + 1 == s.expect_frags);
        uint32_t want_len = last ? s.expect_len - (uint32_t)m.frag_no * FRAGMENT_SIZE : FRAGMENT_SIZE;
        bool bad_shape = m.frag_count != s.expect_frags || m.data.size() != want_len ||
                         (s.got_count > 0 && m.chunk_crc != s.crc);
        if (bad_shape) {
            st_stats.crc_failures++;
            release(s, m.chunk_index, true, now);
        } else if (!s.got[m.frag_no]) {
            s.got[m.frag_no] = true;
            s.got_count++;
            s.crc = m.chunk_crc;
            std::copy(m.data.begin(), m.data.end(), s.buf.begin() + (size_t)m.frag_no * FRAGMENT_SIZE);

            if (s.got_count == s.expect_frags) {
                if (crc32(s.buf.data(), s.buf.size()) != s.crc) {
                    st_stats.crc_failures++;
                    release(s, m.chunk_index, true, now);
                } else {
                    uint32_t peer = s.peer;
                    inflight--;
                    inflight_per_peer[peer]--;
                    s.status = DONE;
                    s.got.clear();
                    done_count++;
                    st_stats.chunks_from[peer]++;
                    if (failover_chunks.erase(m.chunk_index) && failover_chunks.empty())
                        st_stats.last_failover_ms = (int64_t)(now - failover_start);
                    if (done_count == slots.size()) finish(now);
                }
            }
        }
        if (st == DL_RUNNING) fill_window(pending, now);
    }
    send_pending(pending, now);
}

void Downloader::on_chunk_err(uint32_t from, const ChunkRefMsg& m, uint64_t now) {
    std::vector<Pending> pending;
    {
        std::lock_guard<std::mutex> lock(mu);
        if (st != DL_RUNNING || m.file_id != info.file_id || m.chunk_index >= slots.size()) return;
        ChunkSlot& s = slots[m.chunk_index];
        if (s.status != IN_FLIGHT || s.peer != from) return;
        st_stats.errors++;
        release(s, m.chunk_index, true, now);
        if (st == DL_RUNNING) fill_window(pending, now);
    }
    send_pending(pending, now);
}

void Downloader::on_peer_dead(uint32_t peer, uint64_t now) {
    std::vector<Pending> pending;
    {
        std::lock_guard<std::mutex> lock(mu);
        if (st != DL_RUNNING || dead.count(peer)) return;
        dead.insert(peer);
        bool moved = false;
        for (uint32_t i = 0; i < slots.size(); i++) {
            ChunkSlot& s = slots[i];
            if (s.status == IN_FLIGHT && s.peer == peer) {
                // a dead holder is not this chunk's fault, so it does not use up a try
                s.tries--;
                release(s, i, true, now);
                failover_chunks.insert(i);
                st_stats.failovers++;
                moved = true;
            }
        }
        if (moved) failover_start = now;
        if (st == DL_RUNNING) fill_window(pending, now);
    }
    send_pending(pending, now);
}

void Downloader::on_holder_added(uint32_t file_id, uint32_t chunk_index, uint32_t peer, uint64_t now) {
    std::vector<Pending> pending;
    {
        std::lock_guard<std::mutex> lock(mu);
        if (st != DL_RUNNING || file_id != info.file_id || chunk_index >= slots.size()) return;
        ChunkSlot& s = slots[chunk_index];
        for (uint32_t h : s.holders)
            if (h == peer) return;
        s.holders.push_back(peer);
        dead.erase(peer);
        fill_window(pending, now);
    }
    send_pending(pending, now);
}

void Downloader::tick(uint64_t now) {
    std::vector<Pending> pending;
    {
        std::lock_guard<std::mutex> lock(mu);
        if (st != DL_RUNNING) return;
        for (uint32_t i = 0; i < slots.size() && st == DL_RUNNING; i++) {
            ChunkSlot& s = slots[i];
            if (s.status == IN_FLIGHT && now - s.sent_ms >= CHUNK_TIMEOUT_MS) {
                st_stats.timeouts++;
                release(s, i, true, now);
            } else if (s.status == WAITING && s.no_holder_since != 0 &&
                       now - s.no_holder_since >= NO_HOLDER_WAIT_MS) {
                fail("no live holder for chunk " + std::to_string(i), now);
            }
        }
        if (st == DL_RUNNING) fill_window(pending, now);
    }
    send_pending(pending, now);
}

DownloadState Downloader::state() {
    std::lock_guard<std::mutex> lock(mu);
    return st;
}

std::string Downloader::fail_reason() {
    std::lock_guard<std::mutex> lock(mu);
    return reason;
}

uint32_t Downloader::file_id() {
    std::lock_guard<std::mutex> lock(mu);
    return info.file_id;
}

void Downloader::progress(uint32_t& done, uint32_t& total) {
    std::lock_guard<std::mutex> lock(mu);
    done = done_count;
    total = (uint32_t)slots.size();
}

DownloadStats Downloader::stats() {
    std::lock_guard<std::mutex> lock(mu);
    return st_stats;
}

bool Downloader::take_file(std::vector<uint8_t>& out) {
    std::lock_guard<std::mutex> lock(mu);
    if (st != DL_DONE) return false;
    out.swap(result);
    result.clear();
    return true;
}

void Downloader::fail(const std::string& why, uint64_t now) {
    st = DL_FAILED;
    reason = why;
    st_stats.end_ms = now;
    slots.clear();
}

// Takes a chunk out of flight. If the holder failed, it is skipped for this chunk next time.
void Downloader::release(ChunkSlot& s, uint32_t index, bool holder_failed, uint64_t now) {
    if (s.status != IN_FLIGHT) return;
    inflight--;
    inflight_per_peer[s.peer]--;
    if (holder_failed) s.bad.insert(s.peer);
    s.status = WAITING;
    s.buf.clear();
    s.got.clear();
    s.got_count = 0;
    if (holder_failed && s.tries >= CHUNK_MAX_TRIES)
        fail("chunk " + std::to_string(index) + " failed " + std::to_string(s.tries) + " times", now);
}

// Least busy live holder that has not failed this chunk. When every live holder has failed it
// once, they all get another chance (until CHUNK_MAX_TRIES).
bool Downloader::pick_holder(ChunkSlot& s, uint32_t& out) {
    for (int pass = 0; pass < 2; pass++) {
        bool any_live = false;
        bool found = false;
        int best_load = 0;
        int best_total = 0;
        for (uint32_t h : s.holders) {
            if (dead.count(h)) continue;
            any_live = true;
            if (s.bad.count(h)) continue;
            int load = inflight_per_peer[h];
            if (load >= PER_PEER_REQUESTS) continue;
            int total = st_stats.chunks_from[h];
            if (!found || load < best_load || (load == best_load && total < best_total)) {
                found = true;
                out = h;
                best_load = load;
                best_total = total;
            }
        }
        if (found) return true;
        if (!any_live || s.bad.empty()) return false;
        s.bad.clear();
    }
    return false;
}

void Downloader::fill_window(std::vector<Pending>& out, uint64_t now) {
    for (uint32_t i = 0; i < slots.size() && inflight < DOWNLOAD_WINDOW; i++) {
        ChunkSlot& s = slots[i];
        if (s.status != WAITING) continue;

        bool has_live = false;
        for (uint32_t h : s.holders)
            if (!dead.count(h)) has_live = true;
        if (!has_live) {
            if (s.no_holder_since == 0) s.no_holder_since = now;
            continue;
        }
        s.no_holder_since = 0;

        uint32_t peer;
        if (!pick_holder(s, peer)) continue;   // holders busy, try again later
        s.status = IN_FLIGHT;
        s.peer = peer;
        s.sent_ms = now;
        s.tries++;
        s.buf.assign(s.expect_len, 0);
        s.got.assign(s.expect_frags, false);
        s.got_count = 0;
        inflight++;
        inflight_per_peer[peer]++;
        st_stats.requests++;
        out.push_back(Pending{peer, info.file_id, i});
    }
}

void Downloader::finish(uint64_t now) {
    result.clear();
    result.reserve((size_t)info.size);
    for (ChunkSlot& s : slots) result.insert(result.end(), s.buf.begin(), s.buf.end());
    slots.clear();
    if (crc32(result.data(), result.size()) != info.file_crc) {
        result.clear();
        fail("file crc32 mismatch", now);
        return;
    }
    st = DL_DONE;
    st_stats.end_ms = now;
}

// Called without the lock, so the transport can call back into us without a deadlock.
void Downloader::send_pending(const std::vector<Pending>& p, uint64_t now) {
    std::vector<Pending> failed;
    for (const Pending& x : p)
        if (!send_fn(x.peer, x.file_id, x.chunk)) failed.push_back(x);
    if (failed.empty()) return;

    std::lock_guard<std::mutex> lock(mu);
    for (const Pending& x : failed) {
        if (st != DL_RUNNING || x.file_id != info.file_id || x.chunk >= slots.size()) return;
        ChunkSlot& s = slots[x.chunk];
        if (s.status == IN_FLIGHT && s.peer == x.peer) {
            st_stats.errors++;
            release(s, x.chunk, true, now);
        }
    }
}
