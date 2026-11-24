#include "protocol/bytes.h"

#include <cstring>

void ByteWriter::put_u8(uint8_t v) {
    buf.push_back(v);
}

void ByteWriter::put_u16(uint16_t v) {
    buf.push_back((uint8_t)(v >> 8));
    buf.push_back((uint8_t)v);
}

void ByteWriter::put_u32(uint32_t v) {
    put_u16((uint16_t)(v >> 16));
    put_u16((uint16_t)v);
}

void ByteWriter::put_u64(uint64_t v) {
    put_u32((uint32_t)(v >> 32));
    put_u32((uint32_t)v);
}

void ByteWriter::put_str(const std::string& s) {
    size_t n = s.size() > 0xFFFF ? 0xFFFF : s.size();
    put_u16((uint16_t)n);
    buf.insert(buf.end(), s.begin(), s.begin() + n);
}

void ByteWriter::put_bytes(const uint8_t* data, size_t len) {
    buf.insert(buf.end(), data, data + len);
}

bool ByteReader::get_u8(uint8_t& v) {
    if (remaining() < 1) { pos = len; return false; }
    v = data[pos++];
    return true;
}

bool ByteReader::get_u16(uint16_t& v) {
    if (remaining() < 2) { pos = len; return false; }
    v = (uint16_t)((data[pos] << 8) | data[pos + 1]);
    pos += 2;
    return true;
}

bool ByteReader::get_u32(uint32_t& v) {
    uint16_t hi, lo;
    if (!get_u16(hi) || !get_u16(lo)) return false;
    v = ((uint32_t)hi << 16) | lo;
    return true;
}

bool ByteReader::get_u64(uint64_t& v) {
    uint32_t hi, lo;
    if (!get_u32(hi) || !get_u32(lo)) return false;
    v = ((uint64_t)hi << 32) | lo;
    return true;
}

bool ByteReader::get_str(std::string& s) {
    uint16_t n;
    if (!get_u16(n)) return false;
    if (remaining() < n) { pos = len; return false; }
    s.assign((const char*)data + pos, n);
    pos += n;
    return true;
}

bool ByteReader::get_bytes(uint8_t* out, size_t n) {
    if (remaining() < n) { pos = len; return false; }
    std::memcpy(out, data + pos, n);
    pos += n;
    return true;
}
