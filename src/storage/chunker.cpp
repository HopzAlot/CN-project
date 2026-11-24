#include "storage/chunker.h"
#include "common/constants.h"
#include "common/crc32.h"
#include <fstream>

bool split_file(const std::string& path,
                std::vector<std::vector<uint8_t>>& chunks,
                uint64_t& file_size) {
    std::ifstream in(path, std::ios::binary);
    if (!in) return false;

    chunks.clear();
    file_size = 0;
    while (true) {
        std::vector<uint8_t> buf(CHUNK_SIZE);
        in.read(reinterpret_cast<char*>(buf.data()), CHUNK_SIZE);
        std::streamsize got = in.gcount();
        if (got <= 0) break;
        buf.resize(static_cast<size_t>(got));
        file_size += static_cast<uint64_t>(got);
        chunks.push_back(std::move(buf));
    }
    return true;
}

bool rebuild_file(const std::vector<std::vector<uint8_t>>& chunks,
                  const std::string& out_path) {
    std::ofstream out(out_path, std::ios::binary);
    if (!out) return false;

    for (const auto& c : chunks) {
        out.write(reinterpret_cast<const char*>(c.data()),
                  static_cast<std::streamsize>(c.size()));
    }
    return static_cast<bool>(out);
}

uint32_t chunk_crc(const std::vector<uint8_t>& chunk) {
    return crc32(chunk.data(), chunk.size());
}

uint32_t file_crc(const std::vector<std::vector<uint8_t>>& chunks) {
    uint32_t crc = 0;
    for (const auto& c : chunks) crc = crc32_update(crc, c.data(), c.size());
    return crc;
}