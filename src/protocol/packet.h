#ifndef PACKET_H
#define PACKET_H

#include <cstdint>
#include <vector>

// Header layout and message codes from docs/protocol.md.

enum MsgType {
    MSG_DISCOVER      = 0x01,
    MSG_JOIN          = 0x02,
    MSG_WELCOME       = 0x03,
    MSG_HEARTBEAT     = 0x04,
    MSG_LEAVE         = 0x05,
    MSG_CHAT          = 0x06,
    MSG_ACK           = 0x07,
    MSG_FILE_ANNOUNCE = 0x08,
    MSG_CHUNK_STORE   = 0x09,
    MSG_HAVE          = 0x0A,
    MSG_CHUNK_REQUEST = 0x0B,
    MSG_CHUNK_DATA    = 0x0C,
    MSG_CHUNK_ERR     = 0x0D
};

#define STREAM_CONTROL 0
#define STREAM_FILE    1

#define FLAG_RELIABLE  0x01

struct PacketHeader {
    uint8_t version = 0;
    uint8_t type = 0;
    uint8_t stream = 0;
    uint8_t flags = 0;
    uint32_t sender_id = 0;
    uint32_t seq = 0;
    uint16_t length = 0;   // set by encode_packet from payload_len
};

// Writes header + payload into out. Fails if the payload is larger than MAX_PAYLOAD.
// version and length are filled in here, the caller sets the rest.
bool encode_packet(const PacketHeader& h, const uint8_t* payload, int payload_len,
                   std::vector<uint8_t>& out);

// Parses a received datagram. payload points into buf (no copy).
// Fails on short buffer, wrong version, or length field not matching the datagram size.
bool decode_packet(const uint8_t* buf, int len, PacketHeader& h, const uint8_t*& payload);

// For logs: "CHUNK_DATA", "HEARTBEAT", ...
const char* msg_type_name(uint8_t type);

#endif
