#include "check.h"
#include "common/constants.h"
#include "protocol/bytes.h"
#include "protocol/file_messages.h"
#include "protocol/packet.h"

static void test_header() {
    PacketHeader h;
    h.type = MSG_CHAT;
    h.stream = STREAM_CONTROL;
    h.flags = FLAG_RELIABLE;
    h.sender_id = 0xA1B2C3D4;
    h.seq = 77;
    const uint8_t text[] = {'h', 'i'};
    std::vector<uint8_t> out;
    CHECK(encode_packet(h, text, 2, out));
    CHECK(out.size() == HEADER_SIZE + 2);
    // big endian on the wire
    CHECK(out[4] == 0xA1 && out[7] == 0xD4);
    CHECK(out[12] == 0 && out[13] == 2);

    PacketHeader d;
    const uint8_t* payload = nullptr;
    CHECK(decode_packet(out.data(), (int)out.size(), d, payload));
    CHECK(d.version == PROTOCOL_VERSION && d.type == MSG_CHAT && d.flags == FLAG_RELIABLE);
    CHECK(d.sender_id == 0xA1B2C3D4 && d.seq == 77 && d.length == 2);
    CHECK(payload[0] == 'h' && payload[1] == 'i');

    // truncated, wrong version, length mismatch, oversize payload
    CHECK(!decode_packet(out.data(), HEADER_SIZE - 1, d, payload));
    CHECK(!decode_packet(out.data(), (int)out.size() - 1, d, payload));
    std::vector<uint8_t> bad = out;
    bad[0] = 9;
    CHECK(!decode_packet(bad.data(), (int)bad.size(), d, payload));
    std::vector<uint8_t> big(MAX_PAYLOAD + 1, 0);
    CHECK(!encode_packet(h, big.data(), (int)big.size(), out));
    CHECK(encode_packet(h, nullptr, 0, out) && out.size() == HEADER_SIZE);
}

static void test_bytes() {
    ByteWriter w;
    w.put_u8(1);
    w.put_u16(0x0203);
    w.put_u64(0x0102030405060708ull);
    w.put_str("peer-a");
    ByteReader r(w.buf.data(), w.buf.size());
    uint8_t a; uint16_t b; uint64_t c; std::string s;
    CHECK(r.get_u8(a) && a == 1);
    CHECK(r.get_u16(b) && b == 0x0203);
    CHECK(r.get_u64(c) && c == 0x0102030405060708ull);
    CHECK(r.get_str(s) && s == "peer-a");
    CHECK(r.remaining() == 0);
    uint32_t x;
    CHECK(!r.get_u32(x));
}

static void test_file_messages() {
    FileAnnounceMsg fa;
    fa.file_id = 5; fa.name = "notes.pdf"; fa.size = 123456789ull; fa.chunk_count = 7536; fa.file_crc = 0xDEADBEEF;
    std::vector<uint8_t> p;
    CHECK(encode_file_announce(fa, p));
    FileAnnounceMsg fb;
    CHECK(decode_file_announce(p.data(), (int)p.size(), fb));
    CHECK(fb.file_id == 5 && fb.name == "notes.pdf" && fb.size == 123456789ull);
    CHECK(fb.chunk_count == 7536 && fb.file_crc == 0xDEADBEEF);
    CHECK(!decode_file_announce(p.data(), (int)p.size() - 1, fb));

    ChunkRefMsg ra; ra.file_id = 9; ra.chunk_index = 3;
    p.clear();
    CHECK(encode_chunk_ref(ra, p) && p.size() == 8);
    ChunkRefMsg rb;
    CHECK(decode_chunk_ref(p.data(), 8, rb) && rb.file_id == 9 && rb.chunk_index == 3);

    ChunkDataMsg da;
    da.file_id = 1; da.chunk_index = 2; da.frag_no = 15; da.frag_count = 16; da.chunk_crc = 42;
    da.data.assign(FRAGMENT_SIZE, 0xAB);
    p.clear();
    CHECK(encode_chunk_data(da, p));
    CHECK(p.size() == CHUNK_DATA_FIELDS + FRAGMENT_SIZE);
    CHECK(p.size() <= MAX_PAYLOAD);
    ChunkDataMsg db;
    CHECK(decode_chunk_data(p.data(), (int)p.size(), db));
    CHECK(db.frag_no == 15 && db.frag_count == 16 && db.chunk_crc == 42 && db.data == da.data);

    da.frag_no = 16;   // out of range
    p.clear();
    CHECK(!encode_chunk_data(da, p));

    // a full CHUNK_DATA packet must fit
    std::vector<uint8_t> pkt;
    PacketHeader h;
    da.frag_no = 0;
    p.clear();
    encode_chunk_data(da, p);
    CHECK(encode_packet(h, p.data(), (int)p.size(), pkt));
}

int main() {
    test_header();
    test_bytes();
    test_file_messages();
    return test_result("packet");
}
