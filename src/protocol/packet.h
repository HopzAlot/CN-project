#ifndef PACKET_H
#define PACKET_H

#include <cstdint>

#include "protocol/header.h"

// Message type codes of docs/protocol.md. MSG_JOIN and MSG_ACK live in header.h.
const uint8_t MSG_DISCOVER = 0x01;
const uint8_t MSG_WELCOME = 0x03;
const uint8_t MSG_HEARTBEAT = 0x04;
const uint8_t MSG_LEAVE = 0x05;
const uint8_t MSG_CHAT = 0x06;
const uint8_t MSG_FILE_ANNOUNCE = 0x08;
const uint8_t MSG_CHUNK_STORE = 0x09;
const uint8_t MSG_HAVE = 0x0A;
const uint8_t MSG_CHUNK_REQUEST = 0x0B;
const uint8_t MSG_CHUNK_DATA = 0x0C;
const uint8_t MSG_CHUNK_ERR = 0x0D;
const uint8_t MSG_HAVE_MAP = 0x0E;

const uint8_t STREAM_CONTROL = 0;   // membership, chat, announcements, HAVE, HAVE_MAP
const uint8_t STREAM_FILE = 1;      // only CHUNK_STORE, CHUNK_REQUEST, CHUNK_DATA, CHUNK_ERR

// Stream a message type is sent on. Unknown types go on the control stream.
uint8_t stream_of(uint8_t type);

// True for the codes above, false for anything else.
bool is_known_type(uint8_t type);

// For logs: "CHUNK_DATA", "HEARTBEAT", ... or "UNKNOWN".
const char* msg_type_name(uint8_t type);

#endif
