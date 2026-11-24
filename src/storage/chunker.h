#ifndef CHUNKER_H
#define CHUNKER_H

#include <cstdint>
#include <string>
#include <vector>

// Cuts a file into CHUNK_SIZE pieces. The last piece can be shorter.
bool split_file(const std::string& path,
                std::vector<std::vector<uint8_t>>& chunks,
                uint64_t& file_size);
                // CRC32 of one chunk. It goes into CHUNK_DATA and CHUNK_STORE as chunk_crc32.
uint32_t chunk_crc(const std::vector<uint8_t>& chunk);

// CRC32 of the whole file, built from its chunks in order. It goes into FILE_ANNOUNCE as file_crc32.
uint32_t file_crc(const std::vector<std::vector<uint8_t>>& chunks);

// Writes the chunks back as one file, in order.
bool rebuild_file(const std::vector<std::vector<uint8_t>>& chunks,
                  const std::string& out_path);

#endif