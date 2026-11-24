#ifndef HEADER_H
#define HEADER_H

#include <cstdint>
#include <vector>

#include "common/constants.h"

// One UDP datagram = 14 byte header + payload, see docs/protocol.md.
// The transport only needs the header; it never looks inside a payload.
// Payload layouts and the other message types are not here, see docs/protocol.md.

const uint8_t FLAG_RELIABLE = 0x01;

// the only two message types the transport itself acts on
const uint8_t MSG_JOIN = 0x02;
const uint8_t MSG_ACK = 0x07;

struct Header {
    uint8_t version;
    uint8_t type;
    uint8_t stream;
    uint8_t flags;
    uint32_t sender_id;
    uint32_t seq;
    uint16_t length;

    bool reliable() const { return (flags & FLAG_RELIABLE) != 0; }
};

// Header + payload as bytes. flags and length are filled in here.
std::vector<uint8_t> encode_datagram(uint8_t type, uint8_t stream, bool reliable,
                                     uint32_t sender_id, uint32_t seq,
                                     const std::vector<uint8_t>& payload);

// Parses and checks the header of a received datagram. Returns false (drop it) when:
// shorter than the header, wrong version, 14 + length != size, length > MAX_PAYLOAD,
// sender_id is 0, or RELIABLE is set with seq 0.
// On success the payload is the bytes from buf + HEADER_SIZE, h.length long.
bool decode_datagram(const uint8_t* buf, size_t n, Header& h);

#endif
