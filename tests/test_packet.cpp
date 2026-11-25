#include <cstring>

#include "check.h"
#include "protocol/packet.h"

static void test_codes_match_spec() {
    CHECK(MSG_DISCOVER == 0x01 && MSG_JOIN == 0x02 && MSG_WELCOME == 0x03);
    CHECK(MSG_HEARTBEAT == 0x04 && MSG_LEAVE == 0x05 && MSG_CHAT == 0x06);
    CHECK(MSG_ACK == 0x07 && MSG_FILE_ANNOUNCE == 0x08 && MSG_CHUNK_STORE == 0x09);
    CHECK(MSG_HAVE == 0x0A && MSG_CHUNK_REQUEST == 0x0B && MSG_CHUNK_DATA == 0x0C);
    CHECK(MSG_CHUNK_ERR == 0x0D && MSG_HAVE_MAP == 0x0E);
}

static void test_streams() {
    CHECK(stream_of(MSG_CHUNK_STORE) == STREAM_FILE);
    CHECK(stream_of(MSG_CHUNK_REQUEST) == STREAM_FILE);
    CHECK(stream_of(MSG_CHUNK_DATA) == STREAM_FILE);
    CHECK(stream_of(MSG_CHUNK_ERR) == STREAM_FILE);

    const uint8_t control[] = {MSG_DISCOVER, MSG_JOIN, MSG_WELCOME, MSG_HEARTBEAT, MSG_LEAVE,
                               MSG_CHAT, MSG_FILE_ANNOUNCE, MSG_HAVE, MSG_HAVE_MAP};
    for (uint8_t t : control) CHECK(stream_of(t) == STREAM_CONTROL);

    // HAVE and HAVE_MAP must not queue behind chunk data
    CHECK(stream_of(MSG_HAVE) != stream_of(MSG_CHUNK_DATA));
    CHECK(stream_of(0xEE) == STREAM_CONTROL);
}

static void test_names() {
    for (uint8_t t = 0x01; t <= 0x0E; t++) {
        CHECK(is_known_type(t));
        CHECK(std::strcmp(msg_type_name(t), "UNKNOWN") != 0);
    }
    CHECK(!is_known_type(0x00));
    CHECK(!is_known_type(0x0F));
    CHECK(std::strcmp(msg_type_name(MSG_CHUNK_DATA), "CHUNK_DATA") == 0);
    CHECK(std::strcmp(msg_type_name(MSG_HAVE_MAP), "HAVE_MAP") == 0);
    CHECK(std::strcmp(msg_type_name(0x00), "UNKNOWN") == 0);

    // every name is different
    for (uint8_t a = 0x01; a <= 0x0E; a++)
        for (uint8_t b = a + 1; b <= 0x0E; b++)
            CHECK(std::strcmp(msg_type_name(a), msg_type_name(b)) != 0);
}

int main() {
    test_codes_match_spec();
    test_streams();
    test_names();
    return test_result("packet");
}
