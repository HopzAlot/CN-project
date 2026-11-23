#include "storage/chunk_store.h"

#include <filesystem>
#include <fstream>
#include <iterator>

namespace fs = std::filesystem;

static std::string chunk_path(const std::string& root, uint32_t file_id, uint32_t index) {
    return root + "/" + std::to_string(file_id) + "/" + std::to_string(index) + ".chk";
}

ChunkStore::ChunkStore(const std::string& root_dir) : root_dir_(root_dir) {}

bool ChunkStore::save(uint32_t file_id, uint32_t index, const std::vector<uint8_t>& data) {
    std::error_code ec;
    fs::create_directories(root_dir_ + "/" + std::to_string(file_id), ec);
    if (ec) return false;

    std::ofstream out(chunk_path(root_dir_, file_id, index), std::ios::binary);
    if (!out) return false;
    out.write(reinterpret_cast<const char*>(data.data()),
              static_cast<std::streamsize>(data.size()));
    return static_cast<bool>(out);
}

bool ChunkStore::load(uint32_t file_id, uint32_t index, std::vector<uint8_t>& out) const {
    std::ifstream in(chunk_path(root_dir_, file_id, index), std::ios::binary);
    if (!in) return false;
    out.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
    return true;
}

bool ChunkStore::has(uint32_t file_id, uint32_t index) const {
    std::error_code ec;
    return fs::is_regular_file(chunk_path(root_dir_, file_id, index), ec);
}