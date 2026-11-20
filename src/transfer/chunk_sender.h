#ifndef CHUNK_SENDER_H
#define CHUNK_SENDER_H

#include <cstddef>
#include <cstdint>
#include <vector>

// Holder side of a download: turns one stored chunk into the CHUNK_DATA payloads
// to send on stream 1 in answer to a CHUNK_REQUEST.
bool make_chunk_data(uint32_t file_id, uint32_t chunk_index, const uint8_t* chunk, size_t len,
                     std::vector<std::vector<uint8_t>>& payloads);

#endif
