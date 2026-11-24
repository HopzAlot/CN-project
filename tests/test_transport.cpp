#include <algorithm>
#include <atomic>
#include <chrono>
#include <functional>
#include <mutex>
#include <thread>

#include "check.h"
#include "common/socket_compat.h"
#include "protocol/bytes.h"
#include "protocol/header.h"
#include "transport/reliable_channel.h"

static const char* LO = "127.0.0.1";
static const uint8_t T_CHAT = 0x06;
static const uint8_t T_HEARTBEAT = 0x04;
static const uint8_t T_CHUNK = 0x0C;

static std::vector<uint8_t> num(uint32_t v) {
    ByteWriter w;
    w.put_u32(v);
    return w.buf;
}

static uint32_t read_num(const std::vector<uint8_t>& p) {
    ByteReader r(p.data(), p.size());
    uint32_t v = 0;
    r.get_u32(v);
    return v;
}

static bool wait_until(const std::function<bool()>& cond, int timeout_ms) {
    for (int waited = 0; waited < timeout_ms; waited += 10) {
        if (cond()) return true;
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    return cond();
}

static void pause_ms(int ms) {
    std::this_thread::sleep_for(std::chrono::milliseconds(ms));
}

// A peer on a real loopback socket. echo = send every reliable message straight back.
struct Node {
    sock_t sock;
    int port;
    std::mutex mu;
    std::vector<Delivered> inbox;
    std::atomic<int> unreachable;
    ReliableChannel ch;

    Node(uint32_t id, bool echo = false) : sock(INVALID_SOCK), port(0), unreachable(0), ch(open_sock(sock), id) {
        port = udp_local_port(sock);
        ch.set_on_message([this, echo](const Delivered& d) {
            {
                std::lock_guard<std::mutex> lock(mu);
                inbox.push_back(d);
            }
            if (echo) ch.send_reliable(d.ip, d.port, d.stream, d.type, d.payload);
        });
        ch.set_on_unreachable([this](const std::string&, int) { unreachable++; });
        ch.start();
    }
    ~Node() {
        ch.stop();
        udp_close(sock);
    }
    static sock_t open_sock(sock_t& out) {
        udp_open(0, out);
        return out;
    }
    size_t count() {
        std::lock_guard<std::mutex> lock(mu);
        return inbox.size();
    }
    std::vector<Delivered> messages() {
        std::lock_guard<std::mutex> lock(mu);
        return inbox;
    }
};

// A socket we drive by hand, to forge exact packets.
struct Raw {
    sock_t sock;
    int port;
    Raw() : sock(INVALID_SOCK), port(0) {
        udp_open(0, sock);
        port = udp_local_port(sock);
    }
    ~Raw() { udp_close(sock); }
    void send(int to_port, uint8_t type, uint8_t stream, bool reliable, uint32_t sender,
              uint32_t seq, const std::vector<uint8_t>& payload) {
        std::vector<uint8_t> d = encode_datagram(type, stream, reliable, sender, seq, payload);
        udp_send(sock, LO, to_port, d.data(), (int)d.size());
    }
    // next datagram that arrived here, false if none within timeout_ms
    bool recv(Header& h, std::vector<uint8_t>& payload, int timeout_ms) {
        uint8_t buf[2048];
        std::string ip;
        int p = 0;
        int n = udp_recv(sock, buf, sizeof(buf), ip, p, timeout_ms);
        if (n <= 0 || !decode_datagram(buf, (size_t)n, h)) return false;
        payload.assign(buf + HEADER_SIZE, buf + n);
        return true;
    }
};

// ---------------------------------------------------------------- datagram format

static void test_datagram_format() {
    // the ACK example from the protocol notes, byte for byte
    std::vector<uint8_t> d = encode_datagram(MSG_ACK, 0, false, 0x1234, 0, num(5));
    const uint8_t expect[] = {0x01, 0x07, 0x00, 0x00, 0x00, 0x00, 0x12, 0x34, 0x00,
                              0x00, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x05};
    CHECK(d.size() == 18);
    CHECK(std::equal(d.begin(), d.end(), expect));

    Header h;
    CHECK(decode_datagram(d.data(), d.size(), h));
    CHECK(h.type == MSG_ACK && h.stream == 0 && !h.reliable());
    CHECK(h.sender_id == 0x1234 && h.seq == 0 && h.length == 4);

    // CHAT "hi" to everyone, seq 3, reliable: 22 bytes
    ByteWriter w;
    w.put_u32(0);
    w.put_str("hi");
    std::vector<uint8_t> chat = encode_datagram(T_CHAT, 0, true, 0x1234, 3, w.buf);
    CHECK(chat.size() == 22);
    CHECK(chat[3] == FLAG_RELIABLE && chat[13] == 8);
    CHECK(decode_datagram(chat.data(), chat.size(), h) && h.reliable() && h.seq == 3);

    // heartbeat is header only
    CHECK(encode_datagram(T_HEARTBEAT, 0, false, 7, 0, {}).size() == HEADER_SIZE);
}

static void test_datagram_rejects_bad_packets() {
    Header h;
    std::vector<uint8_t> ok = encode_datagram(T_CHAT, 0, true, 5, 1, num(1));
    CHECK(decode_datagram(ok.data(), ok.size(), h));

    CHECK(!decode_datagram(ok.data(), HEADER_SIZE - 1, h));      // too short
    CHECK(!decode_datagram(ok.data(), ok.size() - 1, h));        // length says more than is there
    std::vector<uint8_t> longer = ok;
    longer.push_back(0);
    CHECK(!decode_datagram(longer.data(), longer.size(), h));    // trailing garbage

    std::vector<uint8_t> bad_version = ok;
    bad_version[0] = 2;
    CHECK(!decode_datagram(bad_version.data(), bad_version.size(), h));

    std::vector<uint8_t> zero_id = encode_datagram(T_CHAT, 0, true, 0, 1, num(1));
    CHECK(!decode_datagram(zero_id.data(), zero_id.size(), h));

    std::vector<uint8_t> reliable_seq0 = encode_datagram(T_CHAT, 0, true, 5, 0, num(1));
    CHECK(!decode_datagram(reliable_seq0.data(), reliable_seq0.size(), h));

    std::vector<uint8_t> big(MAX_PAYLOAD + 1, 0);
    std::vector<uint8_t> too_big = encode_datagram(T_CHAT, 0, true, 5, 1, big);
    CHECK(!decode_datagram(too_big.data(), too_big.size(), h));

    std::vector<uint8_t> max(MAX_PAYLOAD, 0);
    std::vector<uint8_t> full = encode_datagram(T_CHAT, 0, true, 5, 1, max);
    CHECK(full.size() == MAX_DATAGRAM);
    CHECK(decode_datagram(full.data(), full.size(), h));
}

// ---------------------------------------------------------------- delivery rules

static void test_unreliable_is_not_acked() {
    Node a(1001), b(1002);
    CHECK(a.ch.send_unreliable(LO, b.port, 0, T_HEARTBEAT, {}));
    CHECK(wait_until([&] { return b.count() == 1; }, 2000));
    CHECK(b.messages()[0].type == T_HEARTBEAT && b.messages()[0].sender_id == 1001);
    pause_ms(100);
    CHECK(b.ch.stats().acks_sent == 0);
    CHECK(a.unreachable == 0);
}

static void test_out_of_order_and_duplicates() {
    Node b(2000);
    Raw r;
    // 3, 2, 2 again, 1: delivered once each, in order 1 2 3
    r.send(b.port, T_CHAT, 0, true, 777, 3, num(3));
    r.send(b.port, T_CHAT, 0, true, 777, 2, num(2));
    r.send(b.port, T_CHAT, 0, true, 777, 2, num(2));
    CHECK(wait_until([&] { return b.ch.stats().duplicates == 1; }, 1000));
    CHECK(b.count() == 0);   // nothing may come out before seq 1
    r.send(b.port, T_CHAT, 0, true, 777, 1, num(1));
    CHECK(wait_until([&] { return b.count() == 3; }, 1000));
    pause_ms(100);
    std::vector<Delivered> got = b.messages();
    CHECK(got.size() == 3);
    CHECK(read_num(got[0].payload) == 1 && read_num(got[1].payload) == 2 && read_num(got[2].payload) == 3);
    CHECK(b.ch.stats().acks_sent == 4);   // every reliable packet is ACKed, the duplicate too

    // expected is now 4. seq 20 is 16 ahead, outside the window: dropped and NOT acked
    r.send(b.port, T_CHAT, 0, true, 777, 20, num(20));
    CHECK(wait_until([&] { return b.ch.stats().invalid == 1; }, 1000));
    CHECK(b.ch.stats().acks_sent == 4);
    // seq 19 is the last one inside the window: buffered and acked
    r.send(b.port, T_CHAT, 0, true, 777, 19, num(19));
    CHECK(wait_until([&] { return b.ch.stats().acks_sent == 5; }, 1000));
    CHECK(b.count() == 3);
}

static void test_garbage_and_own_packets_dropped() {
    Node b(2000);
    Raw r;
    uint8_t junk[] = {1, 2, 3};
    udp_send(r.sock, LO, b.port, junk, 3);
    r.send(b.port, T_CHAT, 0, true, 2000, 1, num(1));   // claims to be b itself
    CHECK(wait_until([&] { return b.ch.stats().invalid == 2; }, 1000));
    CHECK(b.count() == 0);
}

static void test_streams_are_independent() {
    Node b(2000);
    Raw r;
    // stream 1 seq 1 is missing, stream 0 must not care
    r.send(b.port, T_CHUNK, 1, true, 777, 2, num(12));
    r.send(b.port, T_CHAT, 0, true, 777, 1, num(1));
    CHECK(wait_until([&] { return b.count() == 1; }, 1000));
    CHECK(b.messages()[0].stream == 0);
    r.send(b.port, T_CHUNK, 1, true, 777, 1, num(11));
    CHECK(wait_until([&] { return b.count() == 3; }, 1000));
    std::vector<Delivered> got = b.messages();
    CHECK(got[1].stream == 1 && read_num(got[1].payload) == 11);
    CHECK(got[2].stream == 1 && read_num(got[2].payload) == 12);
}

// ---------------------------------------------------------------- JOIN reset

static void test_join_resets_receiver() {
    Node b(2000);
    Raw r;
    for (uint32_t s = 1; s <= 5; s++) r.send(b.port, T_CHAT, 0, true, 777, s, num(s));
    CHECK(wait_until([&] { return b.count() == 5; }, 1000));

    // peer 777 restarts its numbering with a JOIN: without the reset this is a "duplicate"
    r.send(b.port, MSG_JOIN, 0, true, 777, 1, num(0));
    CHECK(wait_until([&] { return b.count() == 6; }, 1000));
    r.send(b.port, T_CHAT, 0, true, 777, 2, num(99));
    CHECK(wait_until([&] { return b.count() == 7; }, 1000));
    CHECK(b.messages()[6].type == T_CHAT && read_num(b.messages()[6].payload) == 99);
}

static void test_resent_join_is_not_a_second_reset() {
    Node b(2000);
    Raw r;
    r.send(b.port, MSG_JOIN, 0, true, 777, 1, num(0));
    r.send(b.port, MSG_JOIN, 0, true, 777, 1, num(0));   // the first ACK was lost, sender resends
    r.send(b.port, T_CHAT, 0, true, 777, 2, num(5));
    CHECK(wait_until([&] { return b.count() == 2; }, 1000));
    pause_ms(100);
    CHECK(b.count() == 2);
    CHECK(b.messages()[0].type == MSG_JOIN && b.messages()[1].type == T_CHAT);
    CHECK(b.ch.stats().duplicates == 1);
    CHECK(b.ch.stats().acks_sent == 3);
}

static void test_join_resend_after_more_traffic() {
    Node b(2000);
    Raw r;
    r.send(b.port, MSG_JOIN, 0, true, 777, 1, num(0));
    r.send(b.port, T_CHAT, 0, true, 777, 2, num(2));
    r.send(b.port, T_CHAT, 0, true, 777, 3, num(3));
    CHECK(wait_until([&] { return b.count() == 3; }, 1000));
    // the ACK of the JOIN was lost, so it comes again after seq 2 and 3 were delivered
    r.send(b.port, MSG_JOIN, 0, true, 777, 1, num(0));
    r.send(b.port, T_CHAT, 0, true, 777, 4, num(4));
    CHECK(wait_until([&] { return b.count() == 4; }, 1000));
    pause_ms(100);
    CHECK(b.count() == 4);   // no second JOIN, and seq 4 was not mistaken for a gap
    CHECK(read_num(b.messages()[3].payload) == 4);
    CHECK(b.ch.stats().duplicates == 1);
}

static void test_join_again_after_resend_window_is_a_real_rejoin() {
    Node b(2000);
    Raw r;
    r.send(b.port, MSG_JOIN, 0, true, 777, 1, num(0));
    r.send(b.port, T_CHAT, 0, true, 777, 2, num(2));
    CHECK(wait_until([&] { return b.count() == 2; }, 1000));
    pause_ms((MAX_RETRIES + 2) * RTO_MS);   // no resend can still be on its way
    r.send(b.port, MSG_JOIN, 0, true, 777, 1, num(0));
    r.send(b.port, T_CHAT, 0, true, 777, 2, num(22));
    CHECK(wait_until([&] { return b.count() == 4; }, 1000));
    CHECK(read_num(b.messages()[3].payload) == 22);
}

static void test_join_resets_sender() {
    Node a(1001), b(1002);
    for (uint32_t i = 0; i < 5; i++) a.ch.send_reliable(LO, b.port, 0, T_CHAT, num(i));
    CHECK(wait_until([&] { return b.count() == 5 && a.ch.unacked(LO, b.port, 0) == 0; }, 2000));

    // a sends a JOIN: its seq to b starts again at 1, and b must accept that
    a.ch.send_reliable(LO, b.port, 0, MSG_JOIN, num(0));
    for (uint32_t i = 5; i < 8; i++) a.ch.send_reliable(LO, b.port, 0, T_CHAT, num(i));
    CHECK(wait_until([&] { return b.count() == 9; }, 2000));
    std::vector<Delivered> got = b.messages();
    CHECK(got[5].type == MSG_JOIN);
    for (uint32_t i = 6; i < 9; i++) CHECK(got[i].type == T_CHAT && read_num(got[i].payload) == i - 1);
}

// ---------------------------------------------------------------- sender rules

static void test_window_of_16() {
    Raw silent;   // listens but never ACKs
    Node a(1001);
    for (uint32_t i = 0; i < 100; i++) CHECK(a.ch.send_reliable(LO, silent.port, 0, T_CHAT, num(i)));
    CHECK(a.ch.unacked(LO, silent.port, 0) == WINDOW_SIZE);
    CHECK(a.ch.queued(LO, silent.port, 0) == 100 - WINDOW_SIZE);

    Header h;
    std::vector<uint8_t> p;
    for (uint32_t s = 1; s <= WINDOW_SIZE; s++) {
        CHECK(silent.recv(h, p, 500));
        CHECK(h.seq == s && h.reliable() && h.sender_id == 1001);
    }
    CHECK(!silent.recv(h, p, 30));   // the 17th must wait for an ACK

    // ACK seq 1: the window slides and seq 17 goes out
    silent.send(a.port, MSG_ACK, 0, false, 99, 0, num(1));
    CHECK(silent.recv(h, p, 500));
    CHECK(h.seq == 17 && read_num(p) == 16);
    CHECK(a.ch.unacked(LO, silent.port, 0) == WINDOW_SIZE);
}

static void test_retransmit_then_unreachable() {
    Raw silent;
    Node a(1001);
    a.ch.send_reliable(LO, silent.port, 0, T_CHAT, num(1));

    // never ACKed: the same seq 1 comes again after RTO_MS
    Header h;
    std::vector<uint8_t> p;
    CHECK(silent.recv(h, p, 500));
    CHECK(h.seq == 1);
    CHECK(silent.recv(h, p, RTO_MS + 300));
    CHECK(h.seq == 1);
    CHECK(a.ch.stats().retransmissions >= 1);

    // after MAX_RETRIES resends the peer is declared unreachable and its state is dropped
    CHECK(wait_until([&] { return a.unreachable == 1; }, (MAX_RETRIES + 3) * RTO_MS));
    CHECK(a.ch.unacked(LO, silent.port, 0) == 0);
    CHECK(a.ch.stats().retransmissions == MAX_RETRIES);
}

static void test_payload_too_big() {
    Node a(1001);
    std::vector<uint8_t> big(MAX_PAYLOAD + 1, 0);
    CHECK(!a.ch.send_reliable(LO, 9, 0, T_CHAT, big));
    CHECK(!a.ch.send_unreliable(LO, 9, 0, T_CHAT, big));
}

// ---------------------------------------------------------------- loss

static void test_ordered_delivery_under_loss() {
    Node a(1001), b(1002, true);   // b echoes everything back, so both directions are tested
    a.ch.set_loss_percent(20, 7);
    b.ch.set_loss_percent(20, 8);

    const uint32_t N = 150;
    for (uint32_t i = 0; i < N; i++) CHECK(a.ch.send_reliable(LO, b.port, 0, T_CHAT, num(i)));

    CHECK(wait_until([&] { return b.count() == N && a.count() == N; }, 30000));
    CHECK(wait_until([&] { return a.ch.unacked(LO, b.port, 0) == 0 && b.ch.unacked(LO, a.port, 0) == 0; }, 10000));
    pause_ms(500);   // a late duplicate would show up here

    std::vector<Delivered> at_b = b.messages(), at_a = a.messages();
    CHECK(at_b.size() == N && at_a.size() == N);
    for (uint32_t i = 0; i < at_b.size() && i < N; i++) CHECK(read_num(at_b[i].payload) == i);
    for (uint32_t i = 0; i < at_a.size() && i < N; i++) CHECK(read_num(at_a[i].payload) == i);
    CHECK(a.ch.stats().retransmissions > 0);
    CHECK(a.ch.stats().simulated_drops > 0);
    CHECK(a.unreachable == 0 && b.unreachable == 0);
}

static void test_two_streams_under_loss() {
    Node a(1001), b(1002);
    a.ch.set_loss_percent(15, 3);
    b.ch.set_loss_percent(15, 4);

    const uint32_t N = 60;
    for (uint32_t i = 0; i < N; i++) {
        a.ch.send_reliable(LO, b.port, 0, T_CHAT, num(i));
        a.ch.send_reliable(LO, b.port, 1, T_CHUNK, num(i));
    }
    CHECK(wait_until([&] { return b.count() == 2 * N; }, 30000));
    pause_ms(300);

    uint32_t next0 = 0, next1 = 0;
    for (const Delivered& d : b.messages()) {
        if (d.stream == 0) CHECK(read_num(d.payload) == next0++);
        else CHECK(read_num(d.payload) == next1++);
    }
    CHECK(next0 == N && next1 == N);
}

int main() {
    net_init();
    test_datagram_format();
    test_datagram_rejects_bad_packets();
    test_unreliable_is_not_acked();
    test_out_of_order_and_duplicates();
    test_garbage_and_own_packets_dropped();
    test_streams_are_independent();
    test_join_resets_receiver();
    test_resent_join_is_not_a_second_reset();
    test_join_resend_after_more_traffic();
    test_join_again_after_resend_window_is_a_real_rejoin();
    test_join_resets_sender();
    test_window_of_16();
    test_retransmit_then_unreachable();
    test_payload_too_big();
    test_ordered_delivery_under_loss();
    test_two_streams_under_loss();
    net_cleanup();
    return test_result("transport");
}
