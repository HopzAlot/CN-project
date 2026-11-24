#include "storage/chunker.h"
#include "common/constants.h"
#include "common/crc32.h"
#include <cstdio>
#include <fstream>
#include <iterator>

static bool write_test_file(const std::string& path, size_t size) {
    std::ofstream out(path, std::ios::binary);
    if (!out) return false;
    for (size_t i = 0; i < size; i++) {
        char b = static_cast<char>((i * 31 + i / 7) % 256);
        out.write(&b, 1);
    }
    return static_cast<bool>(out);
}

static bool read_all(const std::string& path, std::vector<char>& out) {
    std::ifstream in(path, std::ios::binary);
    if (!in) return false;
    out.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
    return true;
}

int main() {
    const size_t SIZE = 2500000;
    const char* in_path = "test_in.bin";
    const char* out_path = "test_out.bin";

    if (!write_test_file(in_path, SIZE)) { printf("FAIL: write\n"); return 1; }

    std::vector<std::vector<uint8_t>> chunks;
    uint64_t file_size = 0;
    if (!split_file(in_path, chunks, file_size)) { printf("FAIL: split\n"); return 1; }

    size_t expected = (SIZE + CHUNK_SIZE - 1) / CHUNK_SIZE;
    if (file_size != SIZE || chunks.size() != expected) {
        printf("FAIL: size %llu, chunks %zu, expected %zu\n",
               (unsigned long long)file_size, chunks.size(), expected);
        return 1;
    }

    if (!rebuild_file(chunks, out_path)) { printf("FAIL: rebuild\n"); return 1; }

    std::vector<char> a, b;
    if (!read_all(in_path, a) || !read_all(out_path, b) || a != b) {
        printf("FAIL: files differ\n");
        return 1;
    }

        // the chunk checksums must agree with the checksum of the whole file
    uint32_t whole = crc32(reinterpret_cast<const uint8_t*>(a.data()), a.size());
    if (file_crc(chunks) != whole) { printf("FAIL: file crc\n"); return 1; }
    if (chunk_crc(chunks[0]) != crc32(reinterpret_cast<const uint8_t*>(a.data()), CHUNK_SIZE)) {
        printf("FAIL: chunk crc\n");
        return 1;
    }

    // one flipped bit must change the checksum
    std::vector<uint8_t> damaged = chunks[0];
    damaged[100] ^= 1;
    if (chunk_crc(damaged) == chunk_crc(chunks[0])) {
        printf("FAIL: damaged chunk not detected\n");
        return 1;
    }
    std::remove(in_path);
    std::remove(out_path);

    printf("PASS: %zu chunks, last chunk %zu bytes\n", chunks.size(), chunks.back().size());
    return 0;
}