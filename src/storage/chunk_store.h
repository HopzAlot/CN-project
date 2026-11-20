#ifndef CHUNK_STORE_H
#define CHUNK_STORE_H

#include <cstdint>
#include <string>
#include <vector>

// Keeps chunks of shared files on disk, one file per chunk.
class ChunkStore {
public:
    // root_dir is this peer's folder, e.g. "peer_data/afaq"
    explicit ChunkStore(const std::string& root_dir);

    bool save(uint32_t file_id, uint32_t index, const std::vector<uint8_t>& data);
    bool load(uint32_t file_id, uint32_t index, std::vector<uint8_t>& out) const;
    bool has(uint32_t file_id, uint32_t index) const;

private:
    std::string root_dir_;
};

#endif