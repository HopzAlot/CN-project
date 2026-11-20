#ifndef CRC32_H
#define CRC32_H

#include <cstddef>
#include <cstdint>

// Standard CRC-32 (same as zlib / PNG). crc32("123456789") == 0xCBF43926.
uint32_t crc32(const uint8_t* data, size_t len);

// For data that arrives in pieces: start with crc = 0, feed every piece in order.
uint32_t crc32_update(uint32_t crc, const uint8_t* data, size_t len);

#endif
