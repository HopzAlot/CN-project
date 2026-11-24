#include "check.h"
#include "protocol/bytes.h"

static void test_round_trip() {
    ByteWriter w;
    w.put_u8(1);
    w.put_u16(0x0203);
    w.put_u32(0xA1B2C3D4);
    w.put_u64(0x0102030405060708ull);
    w.put_str("peer-a");

    ByteReader r(w.buf.data(), w.buf.size());
    uint8_t a;
    uint16_t b;
    uint32_t c;
    uint64_t d;
    std::string s;
    CHECK(r.get_u8(a) && a == 1);
    CHECK(r.get_u16(b) && b == 0x0203);
    CHECK(r.get_u32(c) && c == 0xA1B2C3D4);
    CHECK(r.get_u64(d) && d == 0x0102030405060708ull);
    CHECK(r.get_str(s) && s == "peer-a");
    CHECK(r.remaining() == 0);
}

static void test_big_endian() {
    ByteWriter w;
    w.put_u32(0xA1B2C3D4);
    w.put_str("hi");
    CHECK(w.buf.size() == 8);
    CHECK(w.buf[0] == 0xA1 && w.buf[1] == 0xB2 && w.buf[2] == 0xC3 && w.buf[3] == 0xD4);
    // string = u16 length, then the bytes, no terminating zero
    CHECK(w.buf[4] == 0 && w.buf[5] == 2 && w.buf[6] == 'h' && w.buf[7] == 'i');
}

static void test_short_buffer() {
    const uint8_t two[] = {0x01, 0x02};
    ByteReader r(two, 2);
    uint32_t x;
    CHECK(!r.get_u32(x));
    // a failed read must not leave the reader in the middle of the data
    uint8_t y;
    CHECK(!r.get_u8(y));

    // string whose length field is larger than the data that follows
    const uint8_t lying[] = {0x00, 0x09, 'a', 'b'};
    ByteReader r2(lying, 4);
    std::string s;
    CHECK(!r2.get_str(s));
}

static void test_empty_and_long_strings() {
    ByteWriter w;
    w.put_str("");
    ByteReader r(w.buf.data(), w.buf.size());
    std::string s = "not empty";
    CHECK(r.get_str(s) && s.empty());

    ByteWriter big;
    big.put_str(std::string(70000, 'x'));   // longer than a u16 can describe, cut to 65535
    ByteReader rb(big.buf.data(), big.buf.size());
    CHECK(rb.get_str(s) && s.size() == 65535);
}

static void test_bytes() {
    const uint8_t src[] = {9, 8, 7, 6};
    ByteWriter w;
    w.put_bytes(src, 4);
    ByteReader r(w.buf.data(), w.buf.size());
    uint8_t out[4];
    CHECK(r.get_bytes(out, 4) && out[0] == 9 && out[3] == 6);
    CHECK(!r.get_bytes(out, 1));
}

int main() {
    test_round_trip();
    test_big_endian();
    test_short_buffer();
    test_empty_and_long_strings();
    test_bytes();
    return test_result("bytes");
}
