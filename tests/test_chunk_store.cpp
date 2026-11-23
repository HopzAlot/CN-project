#include <filesystem>

#include "check.h"
#include "common/constants.h"
#include "storage/chunk_store.h"

int main() {
    std::filesystem::remove_all("test_store_tmp");

    std::vector<uint8_t> data(CHUNK_SIZE);
    for (size_t i = 0; i < data.size(); i++) data[i] = static_cast<uint8_t>(i * 7);

    ChunkStore store("test_store_tmp");
    CHECK(!store.has(7, 3));
    CHECK(store.save(7, 3, data));
    CHECK(store.has(7, 3));
    CHECK(!store.has(7, 4));

    // a new object on the same folder stands for a restart
    ChunkStore again("test_store_tmp");
    std::vector<uint8_t> back;
    CHECK(again.load(7, 3, back));
    CHECK(back == data);

    std::vector<uint8_t> none;
    CHECK(!again.load(7, 4, none));

    std::filesystem::remove_all("test_store_tmp");
    return test_result("chunk_store");
}