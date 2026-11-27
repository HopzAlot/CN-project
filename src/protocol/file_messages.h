#ifndef FILE_MESSAGES_H
#define FILE_MESSAGES_H

#include <cstdint>
#include <string>
#include <vector>

// Payload layouts of docs/protocol.md for file announcements and chunk transfer.
// Every encode_* appends to out and returns false if the message breaks a limit (out is
// left untouched), every decode_* returns false on a payload that does not parse.

const uint32_t FILE_NAME_MAX = 255;        // bytes, so FILE_ANNOUNCE fits in one packet
const uint32_t FILE_CHUNKS_MAX = 8736;     // largest bitmap a HAVE_MAP can carry
const uint32_t CHUNK_DATA_FIELDS = 16;     // bytes in front of the data of CHUNK_DATA / CHUNK_STORE

struct FileAnnounceMsg {
    uint32_t file_id = 0;
    std::string name;
    uint64_t size = 0;
    uint32_t chunk_count = 0;   // must be ceil(size / CHUNK_SIZE)
    uint32_t file_crc = 0;
};

// HAVE, CHUNK_REQUEST and CHUNK_ERR all carry the same two fields.
struct ChunkRefMsg {
    uint32_t file_id = 0;
    uint32_t chunk_index = 0;
};

// CHUNK_DATA and CHUNK_STORE have the same layout, one fragment of a chunk.
struct ChunkDataMsg {
    uint32_t file_id = 0;
    uint32_t chunk_index = 0;
    uint16_t frag_no = 0;
    uint16_t frag_count = 0;
    uint32_t chunk_crc = 0;      // crc32 of the whole chunk, the same in every fragment
    std::vector<uint8_t> data;   // 1 to FRAGMENT_SIZE bytes
};

bool encode_file_announce(const FileAnnounceMsg& m, std::vector<uint8_t>& out);
bool decode_file_announce(const uint8_t* p, size_t len, FileAnnounceMsg& m);

bool encode_chunk_ref(const ChunkRefMsg& m, std::vector<uint8_t>& out);
bool decode_chunk_ref(const uint8_t* p, size_t len, ChunkRefMsg& m);

bool encode_chunk_data(const ChunkDataMsg& m, std::vector<uint8_t>& out);
bool decode_chunk_data(const uint8_t* p, size_t len, ChunkDataMsg& m);

// Number of chunks a file of this size has.
uint32_t chunks_for_size(uint64_t size);

#endif
