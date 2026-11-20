#ifndef FILE_MESSAGES_H
#define FILE_MESSAGES_H

#include <cstdint>
#include <string>
#include <vector>

// Payload layouts of the file messages used by transfer (and storage for FILE_ANNOUNCE).
// Byte layouts are in docs/protocol.md. All encode_* append to out, all decode_* return
// false on a malformed payload.

struct FileAnnounceMsg {
    uint32_t file_id = 0;
    std::string name;
    uint64_t size = 0;
    uint32_t chunk_count = 0;
    uint32_t file_crc = 0;
};

// Used by CHUNK_REQUEST, CHUNK_ERR and HAVE (same two fields).
struct ChunkRefMsg {
    uint32_t file_id = 0;
    uint32_t chunk_index = 0;
};

struct ChunkDataMsg {
    uint32_t file_id = 0;
    uint32_t chunk_index = 0;
    uint16_t frag_no = 0;
    uint16_t frag_count = 0;
    uint32_t chunk_crc = 0;
    std::vector<uint8_t> data;   // at most FRAGMENT_SIZE bytes
};

#define CHUNK_DATA_FIELDS 16   // bytes before the data in a CHUNK_DATA payload

bool encode_file_announce(const FileAnnounceMsg& m, std::vector<uint8_t>& out);
bool decode_file_announce(const uint8_t* p, int len, FileAnnounceMsg& m);

bool encode_chunk_ref(const ChunkRefMsg& m, std::vector<uint8_t>& out);
bool decode_chunk_ref(const uint8_t* p, int len, ChunkRefMsg& m);

bool encode_chunk_data(const ChunkDataMsg& m, std::vector<uint8_t>& out);
bool decode_chunk_data(const uint8_t* p, int len, ChunkDataMsg& m);

#endif
