#include "protocol/packet.h"

#include "common/constants.h"
#include "protocol/bytes.h"

bool encode_packet(const PacketHeader& h, const uint8_t* payload, int payload_len,
                   std::vector<uint8_t>& out) {
    if (payload_len < 0 || payload_len > MAX_PAYLOAD) return false;
    ByteWriter w;
    w.buf.reserve(HEADER_SIZE + payload_len);
    w.put_u8(PROTOCOL_VERSION);
    w.put_u8(h.type);
    w.put_u8(h.stream);
    w.put_u8(h.flags);
    w.put_u32(h.sender_id);
    w.put_u32(h.seq);
    w.put_u16((uint16_t)payload_len);
    if (payload_len > 0) w.put_bytes(payload, payload_len);
    out.swap(w.buf);
    return true;
}

bool decode_packet(const uint8_t* buf, int len, PacketHeader& h, const uint8_t*& payload) {
    if (len < HEADER_SIZE) return false;
    ByteReader r(buf, len);
    r.get_u8(h.version);
    r.get_u8(h.type);
    r.get_u8(h.stream);
    r.get_u8(h.flags);
    r.get_u32(h.sender_id);
    r.get_u32(h.seq);
    r.get_u16(h.length);
    if (h.version != PROTOCOL_VERSION) return false;
    if (h.length > MAX_PAYLOAD || (int)h.length != len - HEADER_SIZE) return false;
    payload = buf + HEADER_SIZE;
    return true;
}

const char* msg_type_name(uint8_t type) {
    switch (type) {
        case MSG_DISCOVER: return "DISCOVER";
        case MSG_JOIN: return "JOIN";
        case MSG_WELCOME: return "WELCOME";
        case MSG_HEARTBEAT: return "HEARTBEAT";
        case MSG_LEAVE: return "LEAVE";
        case MSG_CHAT: return "CHAT";
        case MSG_ACK: return "ACK";
        case MSG_FILE_ANNOUNCE: return "FILE_ANNOUNCE";
        case MSG_CHUNK_STORE: return "CHUNK_STORE";
        case MSG_HAVE: return "HAVE";
        case MSG_CHUNK_REQUEST: return "CHUNK_REQUEST";
        case MSG_CHUNK_DATA: return "CHUNK_DATA";
        case MSG_CHUNK_ERR: return "CHUNK_ERR";
        default: return "UNKNOWN";
    }
}
