#include <string>

#include "check.h"
#include "common/constants.h"
#include "protocol/bytes.h"
#include "protocol/control_messages.h"

static PeerInfo make_peer(uint32_t id, const std::string& name) {
    PeerInfo p;
    p.id = id;
    p.name = name;
    p.ip = 0xC0A80105;   // 192.168.1.5
    p.port = 5000 + (uint16_t)id;
    return p;
}

static void test_hello() {
    HelloMsg a;
    a.name = "hamdan";
    a.port = 5001;
    std::vector<uint8_t> p;
    CHECK(encode_hello(a, p));
    CHECK(p.size() == 2 + 6 + 2);

    HelloMsg b;
    CHECK(decode_hello(p.data(), p.size(), b));
    CHECK(b.name == "hamdan" && b.port == 5001);

    CHECK(!decode_hello(p.data(), p.size() - 1, b));
    p.push_back(0);
    CHECK(!decode_hello(p.data(), p.size(), b));   // trailing byte

    HelloMsg big;
    big.name = std::string(MAX_PAYLOAD, 'x');
    std::vector<uint8_t> q;
    CHECK(!encode_hello(big, q) && q.empty());
}

static void test_welcome() {
    WelcomeMsg a;
    a.peers.push_back(make_peer(1, "afaq"));
    a.peers.push_back(make_peer(2, "hamna"));
    a.file_count = 3;
    std::vector<uint8_t> p;
    CHECK(encode_welcome(a, p));
    CHECK(p.size() == 2 + (4 + 2 + 4 + 4 + 2) + (4 + 2 + 5 + 4 + 2) + 2);

    WelcomeMsg b;
    CHECK(decode_welcome(p.data(), p.size(), b));
    CHECK(b.peers.size() == 2 && b.file_count == 3);
    CHECK(b.peers[1].id == 2 && b.peers[1].name == "hamna");
    CHECK(b.peers[0].ip == 0xC0A80105 && b.peers[0].port == 5001);

    WelcomeMsg empty;
    std::vector<uint8_t> e;
    CHECK(encode_welcome(empty, e) && e.size() == 4);
    CHECK(decode_welcome(e.data(), e.size(), b) && b.peers.empty() && b.file_count == 0);

    // every cut-off prefix must be rejected
    for (size_t n = 0; n < p.size(); n++) CHECK(!decode_welcome(p.data(), n, b));
}

static void test_welcome_limits() {
    // docs/protocol.md: about 49 peers with 10 character names fit in one WELCOME
    WelcomeMsg w;
    for (uint32_t i = 1; i <= 49; i++) w.peers.push_back(make_peer(i, "0123456789"));
    std::vector<uint8_t> p;
    CHECK(encode_welcome(w, p));
    CHECK(p.size() <= MAX_PAYLOAD);

    w.peers.push_back(make_peer(50, "0123456789"));
    std::vector<uint8_t> q;
    CHECK(!encode_welcome(w, q));
    CHECK(q.empty());
}

static void test_welcome_lying_count() {
    // count says 60000 peers but nothing follows, must fail without allocating
    const uint8_t lie[] = {0xEA, 0x60, 0x00, 0x00};
    WelcomeMsg w;
    CHECK(!decode_welcome(lie, sizeof(lie), w));
}

static void test_chat() {
    ChatMsg a;
    a.target_id = CHAT_TO_EVERYONE;
    a.text = "hello team";
    std::vector<uint8_t> p;
    CHECK(encode_chat(a, p));
    CHECK(p.size() == 4 + 2 + 10);

    ChatMsg b;
    CHECK(decode_chat(p.data(), p.size(), b));
    CHECK(b.target_id == 0 && b.text == "hello team");

    a.target_id = 0xA1B2C3D4;
    p.clear();
    CHECK(encode_chat(a, p));
    CHECK(p[0] == 0xA1 && p[3] == 0xD4);
    CHECK(decode_chat(p.data(), p.size(), b) && b.target_id == 0xA1B2C3D4);

    ChatMsg limit;
    limit.text = std::string(CHAT_TEXT_MAX, 'a');
    std::vector<uint8_t> l;
    CHECK(encode_chat(limit, l) && l.size() == 4 + 2 + CHAT_TEXT_MAX);
    CHECK(l.size() <= MAX_PAYLOAD);
    CHECK(decode_chat(l.data(), l.size(), b) && b.text.size() == CHAT_TEXT_MAX);

    limit.text.push_back('a');
    std::vector<uint8_t> too_long;
    CHECK(!encode_chat(limit, too_long) && too_long.empty());

    // a peer that does not follow the limit is rejected when decoding too
    ByteWriter w;
    w.put_u32(0);
    w.put_str(std::string(CHAT_TEXT_MAX + 1, 'a'));
    CHECK(!decode_chat(w.buf.data(), w.buf.size(), b));

    CHECK(!decode_chat(p.data(), p.size() - 1, b));
}

static void test_have_map() {
    HaveMapMsg a;
    a.file_id = 7;
    a.peer_id = 0xDEADBEEF;
    a.bitmap = {0xA0, 0x01};
    std::vector<uint8_t> p;
    CHECK(encode_have_map(a, p));
    CHECK(p.size() == 8 + 2);

    HaveMapMsg b;
    CHECK(decode_have_map(p.data(), p.size(), b));
    CHECK(b.file_id == 7 && b.peer_id == 0xDEADBEEF && b.bitmap == a.bitmap);

    // a bitmap that is empty carries no information
    HaveMapMsg none;
    std::vector<uint8_t> q;
    CHECK(!encode_have_map(none, q) && q.empty());
    CHECK(!decode_have_map(p.data(), 8, b));
    CHECK(!decode_have_map(p.data(), 7, b));

    // docs/protocol.md: the biggest bitmap is MAX_PAYLOAD - 8 bytes = 8736 chunks
    HaveMapMsg big;
    big.bitmap.assign(MAX_PAYLOAD - 8, 0xFF);
    std::vector<uint8_t> bp;
    CHECK(encode_have_map(big, bp) && bp.size() == MAX_PAYLOAD);
    big.bitmap.push_back(0);
    std::vector<uint8_t> over;
    CHECK(!encode_have_map(big, over));
}

static void test_bitmap_size() {
    CHECK(have_map_bitmap_bytes(0) == 0);
    CHECK(have_map_bitmap_bytes(1) == 1);
    CHECK(have_map_bitmap_bytes(8) == 1);
    CHECK(have_map_bitmap_bytes(9) == 2);
    CHECK(have_map_bitmap_bytes(8736) == 1092);
}

int main() {
    test_hello();
    test_welcome();
    test_welcome_limits();
    test_welcome_lying_count();
    test_chat();
    test_have_map();
    test_bitmap_size();
    return test_result("control_messages");
}
