#include <string>

#include "check.h"
#include "common/constants.h"
#include "protocol/bytes.h"
#include "protocol/file_messages.h"
#include "protocol/header.h"
#include "protocol/packet.h"

static FileAnnounceMsg make_announce(uint64_t size) {
    FileAnnounceMsg a;
    a.file_id = 5;
    a.name = "notes.pdf";
    a.size = size;
    a.chunk_count = chunks_for_size(size);
    a.file_crc = 0xDEADBEEF;
    return a;
}

static void test_chunks_for_size() {
    CHECK(chunks_for_size(0) == 0);
    CHECK(chunks_for_size(1) == 1);
    CHECK(chunks_for_size(CHUNK_SIZE) == 1);
    CHECK(chunks_for_size(CHUNK_SIZE + 1) == 2);
    CHECK(chunks_for_size(10 * CHUNK_SIZE + 777) == 11);
}

static void test_file_announce() {
    FileAnnounceMsg a = make_announce(123456789ull);
    std::vector<uint8_t> p;
    CHECK(encode_file_announce(a, p));

    FileAnnounceMsg b;
    CHECK(decode_file_announce(p.data(), p.size(), b));
    CHECK(b.file_id == 5 && b.name == "notes.pdf" && b.size == 123456789ull);
    CHECK(b.chunk_count == a.chunk_count && b.file_crc == 0xDEADBEEF);

    for (size_t n = 0; n < p.size(); n++) CHECK(!decode_file_announce(p.data(), n, b));
    p.push_back(0);
    CHECK(!decode_file_announce(p.data(), p.size(), b));

    FileAnnounceMsg empty_file = make_announce(0);
    std::vector<uint8_t> e;
    CHECK(encode_file_announce(empty_file, e));
    CHECK(decode_file_announce(e.data(), e.size(), b) && b.chunk_count == 0);
}

static void test_file_announce_limits() {
    std::vector<uint8_t> out;

    FileAnnounceMsg wrong_count = make_announce(5 * CHUNK_SIZE);
    wrong_count.chunk_count = 4;
    CHECK(!encode_file_announce(wrong_count, out) && out.empty());

    FileAnnounceMsg no_name = make_announce(100);
    no_name.name.clear();
    CHECK(!encode_file_announce(no_name, out));

    FileAnnounceMsg long_name = make_announce(100);
    long_name.name = std::string(FILE_NAME_MAX, 'a');
    CHECK(encode_file_announce(long_name, out));
    CHECK(out.size() <= MAX_PAYLOAD);
    out.clear();
    long_name.name.push_back('a');
    CHECK(!encode_file_announce(long_name, out));

    // the biggest file whose bitmap still fits in one HAVE_MAP
    FileAnnounceMsg biggest = make_announce((uint64_t)FILE_CHUNKS_MAX * CHUNK_SIZE);
    CHECK(encode_file_announce(biggest, out));
    out.clear();
    FileAnnounceMsg too_big = make_announce((uint64_t)FILE_CHUNKS_MAX * CHUNK_SIZE + 1);
    CHECK(!encode_file_announce(too_big, out));

    // a peer that does not follow the rules is rejected when decoding too
    ByteWriter w;
    w.put_u32(1);
    w.put_str("x");
    w.put_u64(5 * CHUNK_SIZE);
    w.put_u32(99);   // chunk_count does not match size
    w.put_u32(0);
    FileAnnounceMsg b;
    CHECK(!decode_file_announce(w.buf.data(), w.buf.size(), b));
}

static void test_chunk_ref() {
    ChunkRefMsg a;
    a.file_id = 9;
    a.chunk_index = 3;
    std::vector<uint8_t> p;
    CHECK(encode_chunk_ref(a, p) && p.size() == 8);
    CHECK(p[3] == 9 && p[7] == 3);

    ChunkRefMsg b;
    CHECK(decode_chunk_ref(p.data(), 8, b) && b.file_id == 9 && b.chunk_index == 3);
    CHECK(!decode_chunk_ref(p.data(), 7, b));
    CHECK(!decode_chunk_ref(p.data(), 0, b));
    p.push_back(0);
    CHECK(!decode_chunk_ref(p.data(), p.size(), b));
}

static ChunkDataMsg make_fragment(uint16_t no, uint16_t count, size_t bytes) {
    ChunkDataMsg d;
    d.file_id = 1;
    d.chunk_index = 2;
    d.frag_no = no;
    d.frag_count = count;
    d.chunk_crc = 42;
    d.data.assign(bytes, 0xAB);
    return d;
}

static void test_chunk_data() {
    ChunkDataMsg a = make_fragment(15, 16, FRAGMENT_SIZE);
    std::vector<uint8_t> p;
    CHECK(encode_chunk_data(a, p));
    CHECK(p.size() == CHUNK_DATA_FIELDS + FRAGMENT_SIZE);

    ChunkDataMsg b;
    CHECK(decode_chunk_data(p.data(), p.size(), b));
    CHECK(b.file_id == 1 && b.chunk_index == 2 && b.frag_no == 15 && b.frag_count == 16);
    CHECK(b.chunk_crc == 42 && b.data == a.data);

    // a full fragment fits in a datagram, with the header in front
    std::vector<uint8_t> datagram = encode_datagram(MSG_CHUNK_DATA, STREAM_FILE, true, 7, 1, p);
    CHECK(datagram.size() == HEADER_SIZE + p.size());
    CHECK(p.size() <= MAX_PAYLOAD);

    ChunkDataMsg last = make_fragment(0, 1, 1);
    std::vector<uint8_t> one;
    CHECK(encode_chunk_data(last, one) && one.size() == CHUNK_DATA_FIELDS + 1);
    CHECK(decode_chunk_data(one.data(), one.size(), b) && b.data.size() == 1);
}

static void test_chunk_data_rejects() {
    std::vector<uint8_t> out;
    CHECK(!encode_chunk_data(make_fragment(16, 16, 10), out));            // frag_no out of range
    CHECK(!encode_chunk_data(make_fragment(0, 0, 10), out));              // no fragments
    CHECK(!encode_chunk_data(make_fragment(0, 17, 10), out));             // more than a chunk holds
    CHECK(!encode_chunk_data(make_fragment(0, 16, 0), out));              // no data
    CHECK(!encode_chunk_data(make_fragment(0, 16, FRAGMENT_SIZE + 1), out));
    CHECK(out.empty());

    std::vector<uint8_t> p;
    CHECK(encode_chunk_data(make_fragment(3, 16, 100), p));
    ChunkDataMsg b;
    for (size_t n = 0; n <= CHUNK_DATA_FIELDS; n++) CHECK(!decode_chunk_data(p.data(), n, b));

    std::vector<uint8_t> big = p;
    big.resize(CHUNK_DATA_FIELDS + FRAGMENT_SIZE + 1);
    CHECK(!decode_chunk_data(big.data(), big.size(), b));

    std::vector<uint8_t> bad_no = p;
    bad_no[8] = 0;
    bad_no[9] = 16;   // frag_no = 16 with frag_count 16
    CHECK(!decode_chunk_data(bad_no.data(), bad_no.size(), b));
}

int main() {
    test_chunks_for_size();
    test_file_announce();
    test_file_announce_limits();
    test_chunk_ref();
    test_chunk_data();
    test_chunk_data_rejects();
    return test_result("file_messages");
}
