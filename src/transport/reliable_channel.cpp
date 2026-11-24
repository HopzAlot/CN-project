#include "transport/reliable_channel.h"

#include <algorithm>
#include <chrono>
#include <cstring>

#include "common/constants.h"
#include "common/time_util.h"
#include "protocol/bytes.h"

// A JOIN is resent for at most MAX_RETRIES * RTO_MS. Seen again inside this time it is a resend.
static const uint64_t JOIN_RESEND_MS = (MAX_RETRIES + 1) * RTO_MS;

static Delivered make_delivered(const Header& h, const std::string& ip, int port,
                                const uint8_t* payload) {
    Delivered d;
    d.sender_id = h.sender_id;
    d.ip = ip;
    d.port = port;
    d.type = h.type;
    d.stream = h.stream;
    d.payload.assign(payload, payload + h.length);
    return d;
}

ReliableChannel::ReliableChannel(sock_t sock, uint32_t my_id)
    : sock_(sock), my_id_(my_id), running_(false), loss_percent_(0), rng_(1) {
    stats_ = TransportStats();
}

ReliableChannel::~ReliableChannel() {
    stop();
}

void ReliableChannel::set_on_message(MessageFn fn) {
    on_message_ = fn;
}

void ReliableChannel::set_on_unreachable(UnreachableFn fn) {
    on_unreachable_ = fn;
}

bool ReliableChannel::start() {
    if (running_) return false;
    running_ = true;
    receiver_ = std::thread(&ReliableChannel::receiver_loop, this);
    timer_ = std::thread(&ReliableChannel::timer_loop, this);
    return true;
}

void ReliableChannel::stop() {
    if (!running_) return;
    running_ = false;
    if (receiver_.joinable()) receiver_.join();
    if (timer_.joinable()) timer_.join();
}

// ---------------------------------------------------------------- sending

bool ReliableChannel::send_reliable(const std::string& ip, int port, uint8_t stream, uint8_t type,
                                    const std::vector<uint8_t>& payload) {
    if (payload.size() > MAX_PAYLOAD) return false;
    std::lock_guard<std::mutex> lock(mu_);
    Endpoint to(ip, port);

    // a JOIN is a fresh start, so our old seq numbers for this peer must not survive it
    if (type == MSG_JOIN) reset_peer_locked(to);

    TxState& tx = tx_[TxKey(to, stream)];
    if (tx.queue.size() >= SEND_QUEUE_LIMIT) return false;
    tx.queue.push_back(Queued{type, payload});
    fill_window(to, stream, tx);
    return true;
}

bool ReliableChannel::send_unreliable(const std::string& ip, int port, uint8_t stream,
                                      uint8_t type, const std::vector<uint8_t>& payload) {
    if (payload.size() > MAX_PAYLOAD) return false;
    std::vector<uint8_t> d = encode_datagram(type, stream, false, my_id_, 0, payload);
    std::lock_guard<std::mutex> lock(mu_);
    return raw_send(ip, port, d);
}

// Moves queued messages into flight while the window has room. Assigning the seq here,
// not when queued, keeps seq numbers gap-free and in send order.
void ReliableChannel::fill_window(const Endpoint& to, uint8_t stream, TxState& tx) {
    while (!tx.queue.empty() && tx.in_flight.size() < WINDOW_SIZE) {
        Queued q = std::move(tx.queue.front());
        tx.queue.pop_front();

        uint32_t seq = tx.next_seq++;
        InFlight f;
        f.datagram = encode_datagram(q.type, stream, true, my_id_, seq, q.payload);
        f.last_sent_ms = now_ms();
        f.retries = 0;
        raw_send(to.first, to.second, f.datagram);
        tx.in_flight[seq] = std::move(f);
    }
}

bool ReliableChannel::raw_send(const std::string& ip, int port, const std::vector<uint8_t>& d) {
    stats_.datagrams_sent++;
    if (loss_percent_ > 0 && (int)(rng_() % 100) < loss_percent_) {
        stats_.simulated_drops++;
        return true;
    }
    return udp_send(sock_, ip, port, d.data(), (int)d.size());
}

void ReliableChannel::send_ack(const std::string& ip, int port, uint8_t stream, uint32_t seq) {
    ByteWriter w;
    w.put_u32(seq);
    raw_send(ip, port, encode_datagram(MSG_ACK, stream, false, my_id_, 0, w.buf));
    stats_.acks_sent++;
}

// ---------------------------------------------------------------- receiving

void ReliableChannel::receiver_loop() {
    // one byte more than MAX_DATAGRAM, so an oversized datagram is noticed instead of cut to fit
    std::vector<uint8_t> buf(MAX_DATAGRAM + 1);
    while (running_) {
        std::string ip;
        int port = 0;
        int n = udp_recv(sock_, buf.data(), (int)buf.size(), ip, port, 50);
        if (n < 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            continue;
        }
        if (n == 0) continue;

        std::vector<Delivered> out;
        {
            std::lock_guard<std::mutex> lock(mu_);
            handle_datagram(buf.data(), (size_t)n, ip, port, out);
        }
        // callbacks run without the lock, so they can call send_* without deadlocking
        if (on_message_)
            for (const Delivered& d : out) on_message_(d);
    }
}

void ReliableChannel::handle_datagram(const uint8_t* buf, size_t n, const std::string& ip,
                                      int port, std::vector<Delivered>& out) {
    Header h;
    if (n > MAX_DATAGRAM || !decode_datagram(buf, n, h)) {
        stats_.invalid++;
        return;
    }
    // a broadcast DISCOVER comes back to the sender too
    if (h.sender_id == my_id_) {
        stats_.invalid++;
        return;
    }

    const uint8_t* payload = buf + HEADER_SIZE;
    id_at_[Endpoint(ip, port)] = h.sender_id;

    if (h.type == MSG_ACK) {
        handle_ack(h, ip, port, payload);
    } else if (h.reliable()) {
        handle_reliable(h, ip, port, payload, out);
    } else {
        out.push_back(make_delivered(h, ip, port, payload));
        stats_.delivered++;
    }
}

void ReliableChannel::handle_ack(const Header& h, const std::string& ip, int port,
                                 const uint8_t* payload) {
    if (h.length != 4) {
        stats_.invalid++;
        return;
    }
    ByteReader r(payload, h.length);
    uint32_t seq = 0;
    r.get_u32(seq);

    Endpoint from(ip, port);
    auto it = tx_.find(TxKey(from, h.stream));
    if (it == tx_.end()) return;

    // ACK n means "I have exactly packet n". An unknown or repeated ACK removes nothing.
    it->second.in_flight.erase(seq);
    // the window has room again, send what was waiting
    fill_window(from, h.stream, it->second);
}

void ReliableChannel::handle_reliable(const Header& h, const std::string& ip, int port,
                                      const uint8_t* payload, std::vector<Delivered>& out) {
    RxKey key(h.sender_id, h.stream);

    if (h.type == MSG_JOIN && h.seq == 1) {
        // A JOIN starts a fresh numbering on both sides. But a JOIN whose ACK got lost is
        // resent, also with seq 1. Wiping again would throw away the replies we are still
        // sending (WELCOME ...). A JOIN seen again soon after the reset is such a resend:
        // it only gets ACKed below, as a duplicate.
        auto it = rx_.find(key);
        bool resend = it != rx_.end() && it->second.join_delivered &&
                      now_ms() - it->second.join_time_ms <= JOIN_RESEND_MS;
        if (!resend) {
            reset_peer_locked(Endpoint(ip, port));
            RxState& fresh = rx_[key];
            fresh.join_delivered = true;
            fresh.join_time_ms = now_ms();
        }
    }

    RxState& rx = rx_[key];

    // ACK even duplicates, the first ACK may have been lost
    if (h.seq < rx.expected) {
        send_ack(ip, port, h.stream, h.seq);
        stats_.duplicates++;
        return;
    }
    // too far ahead to buffer. No ACK, so the sender keeps it and tries again later.
    if (h.seq >= rx.expected + WINDOW_SIZE) {
        stats_.invalid++;
        return;
    }

    send_ack(ip, port, h.stream, h.seq);
    if (rx.early.count(h.seq)) {
        stats_.duplicates++;
        return;
    }

    if (h.seq != rx.expected) {
        rx.early[h.seq] = make_delivered(h, ip, port, payload);
        return;
    }

    out.push_back(make_delivered(h, ip, port, payload));
    rx.expected++;
    stats_.delivered++;
    // the gap is filled, release everything that was waiting behind it
    for (auto next = rx.early.find(rx.expected); next != rx.early.end();
         next = rx.early.find(rx.expected)) {
        out.push_back(std::move(next->second));
        rx.early.erase(next);
        rx.expected++;
        stats_.delivered++;
    }
}

// ---------------------------------------------------------------- timer

void ReliableChannel::timer_loop() {
    while (running_) {
        std::this_thread::sleep_for(std::chrono::milliseconds(20));

        std::vector<Endpoint> dead;
        {
            std::lock_guard<std::mutex> lock(mu_);
            uint64_t now = now_ms();
            for (auto& entry : tx_) {
                const Endpoint& to = entry.first.first;
                for (auto& kv : entry.second.in_flight) {
                    InFlight& f = kv.second;
                    if (now - f.last_sent_ms < RTO_MS) continue;
                    if (f.retries >= MAX_RETRIES) {
                        if (std::find(dead.begin(), dead.end(), to) == dead.end()) dead.push_back(to);
                        break;
                    }
                    f.retries++;
                    f.last_sent_ms = now;
                    stats_.retransmissions++;
                    raw_send(to.first, to.second, f.datagram);
                }
            }
            // give up on the whole peer, both streams, so nothing keeps resending into the void
            for (const Endpoint& e : dead) reset_peer_locked(e);
        }
        if (on_unreachable_)
            for (const Endpoint& e : dead) on_unreachable_(e.first, e.second);
    }
}

// ---------------------------------------------------------------- state

void ReliableChannel::reset_peer_locked(const Endpoint& e) {
    for (auto it = tx_.begin(); it != tx_.end();) {
        if (it->first.first == e) it = tx_.erase(it);
        else ++it;
    }
    auto id = id_at_.find(e);
    if (id == id_at_.end()) return;
    for (auto it = rx_.begin(); it != rx_.end();) {
        if (it->first.first == id->second) it = rx_.erase(it);
        else ++it;
    }
}

void ReliableChannel::forget_peer(const std::string& ip, int port) {
    std::lock_guard<std::mutex> lock(mu_);
    Endpoint e(ip, port);
    reset_peer_locked(e);
    id_at_.erase(e);
}

void ReliableChannel::set_loss_percent(int percent, uint32_t seed) {
    std::lock_guard<std::mutex> lock(mu_);
    loss_percent_ = std::max(0, std::min(100, percent));
    rng_.seed(seed);
}

TransportStats ReliableChannel::stats() {
    std::lock_guard<std::mutex> lock(mu_);
    return stats_;
}

size_t ReliableChannel::unacked(const std::string& ip, int port, uint8_t stream) {
    std::lock_guard<std::mutex> lock(mu_);
    auto it = tx_.find(TxKey(Endpoint(ip, port), stream));
    return it == tx_.end() ? 0 : it->second.in_flight.size();
}

size_t ReliableChannel::queued(const std::string& ip, int port, uint8_t stream) {
    std::lock_guard<std::mutex> lock(mu_);
    auto it = tx_.find(TxKey(Endpoint(ip, port), stream));
    return it == tx_.end() ? 0 : it->second.queue.size();
}
