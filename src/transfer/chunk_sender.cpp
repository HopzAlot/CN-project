#include "transfer/chunk_sender.h"

#include "common/constants.h"
#include "common/crc32.h"
#include "protocol/file_messages.h"

bool make_chunk_data(uint32_t file_id, uint32_t chunk_index, const uint8_t* chunk, size_t len,
                     std::vector<std::vector<uint8_t>>& payloads) {
    payloads.clear();
    if (len == 0 || len > CHUNK_SIZE) return false;

    ChunkDataMsg m;
    m.file_id = file_id;
    m.chunk_index = chunk_index;
    m.frag_count = (uint16_t)((len + FRAGMENT_SIZE - 1) / FRAGMENT_SIZE);
    m.chunk_crc = crc32(chunk, len);

    for (uint16_t k = 0; k < m.frag_count; k++) {
        size_t off = (size_t)k * FRAGMENT_SIZE;
        size_t n = len - off < FRAGMENT_SIZE ? len - off : FRAGMENT_SIZE;
        m.frag_no = k;
        m.data.assign(chunk + off, chunk + off + n);
        std::vector<uint8_t> p;
        if (!encode_chunk_data(m, p)) return false;
        payloads.push_back(p);
    }
    return true;
}
