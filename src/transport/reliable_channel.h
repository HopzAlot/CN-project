#ifndef RELIABLE_CHANNEL_H
#define RELIABLE_CHANNEL_H

#include <atomic>
#include <cstdint>
#include <deque>
#include <functional>
#include <map>
#include <mutex>
#include <random>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "common/socket_compat.h"
#include "protocol/header.h"

// Reliable, ordered delivery on top of one UDP socket (selective repeat, see docs/design.md).
//
// Sending: send_reliable keeps a packet until the peer ACKs it, resending every RTO_MS.
//          send_unreliable fires once with seq 0 (HEARTBEAT, DISCOVER).
// Receiving: every valid packet comes out of the on_message callback, reliable ones
//            in seq order with no duplicates, per (sender, stream).
//
// Threads: start() runs a receiver thread (reads the socket) and a timer thread (resends).
// on_message is always called from the receiver thread, one message at a time, with no
// internal lock held, so it may call send_* itself. Keep it short.

// One message handed up to the application.
struct Delivered {
    uint32_t sender_id;
    std::string ip;
    int port;
    uint8_t type;
    uint8_t stream;
    std::vector<uint8_t> payload;
};

// Counters for the report (goodput, retransmissions vs loss).
struct TransportStats {
    uint64_t datagrams_sent;     // every send attempt: first sends, resends and ACKs, even the ones the simulator drops
    uint64_t retransmissions;    // resends caused by RTO
    uint64_t acks_sent;
    uint64_t delivered;          // messages passed to on_message
    uint64_t duplicates;         // reliable packets we had already delivered or buffered
    uint64_t invalid;            // dropped: bad header, out of window, own packet
    uint64_t simulated_drops;    // outgoing datagrams thrown away by set_loss_percent
};

class ReliableChannel {
public:
    typedef std::function<void(const Delivered&)> MessageFn;
    // peer stopped answering: MAX_RETRIES resends of one packet went unacked
    typedef std::function<void(const std::string& ip, int port)> UnreachableFn;

    // The socket must already be open (udp_open). The channel does not close it.
    ReliableChannel(sock_t sock, uint32_t my_id);
    ~ReliableChannel();

    void set_on_message(MessageFn fn);
    void set_on_unreachable(UnreachableFn fn);

    // Set both callbacks before start().
    bool start();
    void stop();

    // Queues the message. At most WINDOW_SIZE packets per (peer, stream) are in flight,
    // the rest wait in a queue. Returns false if the payload is over MAX_PAYLOAD or the
    // queue for that peer and stream is full (the caller should slow down).
    // Sending a JOIN first wipes all transport state for that peer (see protocol.md).
    bool send_reliable(const std::string& ip, int port, uint8_t stream, uint8_t type,
                       const std::vector<uint8_t>& payload);

    // One datagram, no ACK, no resend. ip can be 255.255.255.255 for a broadcast.
    bool send_unreliable(const std::string& ip, int port, uint8_t stream, uint8_t type,
                         const std::vector<uint8_t>& payload);

    // Call when a peer is removed from the peer table: drops its queues and seq state.
    void forget_peer(const std::string& ip, int port);

    // Drops this percent (0-100) of outgoing datagrams, ACKs and resends included.
    // For tests and the loss measurements only.
    void set_loss_percent(int percent, uint32_t seed = 1);

    TransportStats stats();
    // Packets sent to this peer and stream that are still waiting for an ACK / for window space.
    size_t unacked(const std::string& ip, int port, uint8_t stream);
    size_t queued(const std::string& ip, int port, uint8_t stream);

private:
    // Sender side, one per (peer address, stream)
    struct InFlight {
        std::vector<uint8_t> datagram;   // kept ready to resend
        uint64_t last_sent_ms;
        int retries;
    };
    struct Queued {
        uint8_t type;
        std::vector<uint8_t> payload;
    };
    struct TxState {
        uint32_t next_seq = 1;
        std::map<uint32_t, InFlight> in_flight;
        std::deque<Queued> queue;
    };

    // Receiver side, one per (sender id, stream)
    struct RxState {
        uint32_t expected = 1;                    // next seq to deliver
        std::map<uint32_t, Delivered> early;      // arrived out of order, waiting for the gap
        bool join_delivered = false;              // this state was started by a JOIN
        uint64_t join_time_ms = 0;                // when that JOIN arrived
    };

    typedef std::pair<std::string, int> Endpoint;
    typedef std::pair<Endpoint, uint8_t> TxKey;
    typedef std::pair<uint32_t, uint8_t> RxKey;

    void receiver_loop();
    void timer_loop();
    // all of these need mu_ held. Messages to hand up are collected in out and delivered
    // after the lock is released.
    void handle_datagram(const uint8_t* buf, size_t n, const std::string& ip, int port,
                         std::vector<Delivered>& out);
    void handle_ack(const Header& h, const std::string& ip, int port,
                    const uint8_t* payload);
    void handle_reliable(const Header& h, const std::string& ip, int port,
                         const uint8_t* payload, std::vector<Delivered>& out);

    bool raw_send(const std::string& ip, int port, const std::vector<uint8_t>& datagram);
    void send_ack(const std::string& ip, int port, uint8_t stream, uint32_t seq);
    void fill_window(const Endpoint& to, uint8_t stream, TxState& tx);
    void reset_peer_locked(const Endpoint& e);

    sock_t sock_;
    uint32_t my_id_;
    std::atomic<bool> running_;
    std::thread receiver_;
    std::thread timer_;

    std::mutex mu_;   // guards everything below
    std::map<TxKey, TxState> tx_;
    std::map<RxKey, RxState> rx_;
    std::map<Endpoint, uint32_t> id_at_;   // last sender id seen at each address
    TransportStats stats_;
    int loss_percent_;
    std::mt19937 rng_;

    MessageFn on_message_;
    UnreachableFn on_unreachable_;
};

#endif
