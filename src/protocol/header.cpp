#include "protocol/header.h"

#include "protocol/bytes.h"

std::vector<uint8_t> encode_datagram(uint8_t type, uint8_t stream, bool reliable,
                                     uint32_t sender_id, uint32_t seq,
                                     const std::vector<uint8_t>& payload) {
    ByteWriter w;
    w.buf.reserve(HEADER_SIZE + payload.size());
    w.put_u8(PROTOCOL_VERSION);
    w.put_u8(type);
    w.put_u8(stream);
    w.put_u8(reliable ? FLAG_RELIABLE : 0);
    w.put_u32(sender_id);
    w.put_u32(seq);
    w.put_u16((uint16_t)payload.size());
    w.put_bytes(payload.data(), payload.size());
    return w.buf;
}

bool decode_datagram(const uint8_t* buf, size_t n, Header& h) {
    ByteReader r(buf, n);
    if (!r.get_u8(h.version) || !r.get_u8(h.type) || !r.get_u8(h.stream) || !r.get_u8(h.flags) ||
        !r.get_u32(h.sender_id) || !r.get_u32(h.seq) || !r.get_u16(h.length))
        return false;

    if (h.version != PROTOCOL_VERSION) return false;
    if (h.length > MAX_PAYLOAD) return false;
    if (HEADER_SIZE + (size_t)h.length != n) return false;
    if (h.sender_id == 0) return false;
    // a reliable packet needs a real seq, otherwise it could never be ordered or ACKed
    if (h.reliable() && h.seq == 0) return false;
    return true;
}
