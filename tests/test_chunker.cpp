#include "storage/chunker.h"
#include "common/constants.h"

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

    if (!write_test_file("build/test_in.bin", SIZE)) { printf("FAIL: write\n"); return 1; }

    std::vector<std::vector<uint8_t>> chunks;
    uint64_t file_size = 0;
    if (!split_file("build/test_in.bin", chunks, file_size)) { printf("FAIL: split\n"); return 1; }

    size_t expected = (SIZE + CHUNK_SIZE - 1) / CHUNK_SIZE;
    if (file_size != SIZE || chunks.size() != expected) {
        printf("FAIL: size %llu, chunks %zu, expected %zu\n",
               (unsigned long long)file_size, chunks.size(), expected);
        return 1;
    }

    if (!rebuild_file(chunks, "build/test_out.bin")) { printf("FAIL: rebuild\n"); return 1; }

    std::vector<char> a, b;
    if (!read_all("build/test_in.bin", a) || !read_all("build/test_out.bin", b) || a != b) {
        printf("FAIL: files differ\n");
        return 1;
    }

    printf("PASS: %zu chunks, last chunk %zu bytes\n", chunks.size(), chunks.back().size());
    return 0;
}