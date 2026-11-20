#include <cstring>

#include "check.h"
#include "common/crc32.h"

int main() {
    const char* s = "123456789";
    CHECK(crc32((const uint8_t*)s, 9) == 0xCBF43926u);
    CHECK(crc32(nullptr, 0) == 0);

    uint32_t c = 0;
    c = crc32_update(c, (const uint8_t*)s, 4);
    c = crc32_update(c, (const uint8_t*)s + 4, 5);
    CHECK(c == 0xCBF43926u);

    uint8_t buf[100];
    std::memset(buf, 7, sizeof(buf));
    uint32_t a = crc32(buf, sizeof(buf));
    buf[50] ^= 1;
    CHECK(crc32(buf, sizeof(buf)) != a);
    return test_result("crc32");
}
