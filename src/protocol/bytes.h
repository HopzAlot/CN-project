#ifndef BYTES_H
#define BYTES_H

#include <cstdint>
#include <string>
#include <vector>

// Big-endian (network order) writing and reading, used by every payload encoder.
// Strings are written as u16 length + bytes, no terminating zero.

struct ByteWriter {
    std::vector<uint8_t> buf;

    void put_u8(uint8_t v);
    void put_u16(uint16_t v);
    void put_u32(uint32_t v);
    void put_u64(uint64_t v);
    void put_str(const std::string& s);
    void put_bytes(const uint8_t* data, size_t len);
};

// Every get_* returns false (and leaves the reader failed) if the buffer is too short.
struct ByteReader {
    const uint8_t* data;
    size_t len;
    size_t pos;

    ByteReader(const uint8_t* d, size_t n) : data(d), len(n), pos(0) {}

    bool get_u8(uint8_t& v);
    bool get_u16(uint16_t& v);
    bool get_u32(uint32_t& v);
    bool get_u64(uint64_t& v);
    bool get_str(std::string& s);
    bool get_bytes(uint8_t* out, size_t n);
    size_t remaining() const { return len - pos; }
    const uint8_t* cursor() const { return data + pos; }
};

#endif
