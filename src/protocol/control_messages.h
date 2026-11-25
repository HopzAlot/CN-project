#ifndef CONTROL_MESSAGES_H
#define CONTROL_MESSAGES_H

#include <cstdint>
#include <string>
#include <vector>

// Payload layouts of docs/protocol.md for the control stream. Every encode_* appends to
// out and returns false if the message would not fit in MAX_PAYLOAD (or breaks a limit),
// every decode_* returns false on a payload that does not parse. The caller sends
// the payload with the matching MSG_* code.

const uint32_t CHAT_TEXT_MAX = 1000;   // bytes of chat text
const uint32_t CHAT_TO_EVERYONE = 0;   // target_id for a message to the whole group

// DISCOVER and JOIN: who I am and where to reach me. The IP is the packet's source address.
struct HelloMsg {
    std::string name;
    uint16_t port = 0;
};

struct PeerInfo {
    uint32_t id = 0;
    std::string name;
    uint32_t ip = 0;   // IPv4, host order
    uint16_t port = 0;
};

// WELCOME: the peer list. file_count FILE_ANNOUNCEs follow it as separate messages.
struct WelcomeMsg {
    std::vector<PeerInfo> peers;
    uint16_t file_count = 0;
};

struct ChatMsg {
    uint32_t target_id = CHAT_TO_EVERYONE;
    std::string text;
};

// Which chunks of a file a peer holds. Bit layout is in protocol.md, ChunkMap builds and reads it.
struct HaveMapMsg {
    uint32_t file_id = 0;
    uint32_t peer_id = 0;
    std::vector<uint8_t> bitmap;
};

bool encode_hello(const HelloMsg& m, std::vector<uint8_t>& out);
bool decode_hello(const uint8_t* p, size_t len, HelloMsg& m);

bool encode_welcome(const WelcomeMsg& m, std::vector<uint8_t>& out);
bool decode_welcome(const uint8_t* p, size_t len, WelcomeMsg& m);

bool encode_chat(const ChatMsg& m, std::vector<uint8_t>& out);
bool decode_chat(const uint8_t* p, size_t len, ChatMsg& m);

bool encode_have_map(const HaveMapMsg& m, std::vector<uint8_t>& out);
bool decode_have_map(const uint8_t* p, size_t len, HaveMapMsg& m);

// Bytes a HAVE_MAP bitmap has for a file with this many chunks. A receiver checks the
// decoded bitmap against it, because only the FILE_ANNOUNCE knows chunk_count.
size_t have_map_bitmap_bytes(uint32_t chunk_count);

#endif
