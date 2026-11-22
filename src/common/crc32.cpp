#include "common/crc32.h"

static uint32_t table[256];
static bool table_ready = false;

static void build_table() {
    for (uint32_t i = 0; i < 256; i++) {
        uint32_t c = i;
        for (int k = 0; k < 8; k++) c = (c & 1) ? (0xEDB88320u ^ (c >> 1)) : (c >> 1);
        table[i] = c;
    }
    table_ready = true;
}

// static initializer so the table exists before any thread calls crc32
static struct TableInit { TableInit() { build_table(); } } table_init;

uint32_t crc32_update(uint32_t crc, const uint8_t* data, size_t len) {
    if (!table_ready) build_table();
    uint32_t c = crc ^ 0xFFFFFFFFu;
    for (size_t i = 0; i < len; i++) c = table[(c ^ data[i]) & 0xFF] ^ (c >> 8);
    return c ^ 0xFFFFFFFFu;
}

uint32_t crc32(const uint8_t* data, size_t len) {
    return crc32_update(0, data, len);
}
