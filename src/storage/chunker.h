#ifndef CHUNKER_H
#define CHUNKER_H

#include <cstdint>
#include <string>
#include <vector>

// Cuts a file into CHUNK_SIZE pieces. The last piece can be shorter.
bool split_file(const std::string& path,
                std::vector<std::vector<uint8_t>>& chunks,
                uint64_t& file_size);

// Writes the chunks back as one file, in order.
bool rebuild_file(const std::vector<std::vector<uint8_t>>& chunks,
                  const std::string& out_path);

#endif