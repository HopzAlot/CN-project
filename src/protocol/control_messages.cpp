#include "protocol/control_messages.h"

#include "common/constants.h"
#include "protocol/bytes.h"

// out is only touched when the whole message fits
static bool append_if_fits(const ByteWriter& w, std::vector<uint8_t>& out) {
    if (w.buf.size() > MAX_PAYLOAD) return false;
    out.insert(out.end(), w.buf.begin(), w.buf.end());
    return true;
}

bool encode_hello(const HelloMsg& m, std::vector<uint8_t>& out) {
    if (m.name.size() > MAX_PAYLOAD) return false;
    ByteWriter w;
    w.put_str(m.name);
    w.put_u16(m.port);
    return append_if_fits(w, out);
}

bool decode_hello(const uint8_t* p, size_t len, HelloMsg& m) {
    ByteReader r(p, len);
    return r.get_str(m.name) && r.get_u16(m.port) && r.remaining() == 0;
}

bool encode_welcome(const WelcomeMsg& m, std::vector<uint8_t>& out) {
    if (m.peers.size() > 0xFFFF) return false;
    ByteWriter w;
    w.put_u16((uint16_t)m.peers.size());
    for (const PeerInfo& peer : m.peers) {
        if (peer.name.size() > MAX_PAYLOAD) return false;
        w.put_u32(peer.id);
        w.put_str(peer.name);
        w.put_u32(peer.ip);
        w.put_u16(peer.port);
        if (w.buf.size() > MAX_PAYLOAD) return false;
    }
    w.put_u16(m.file_count);
    return append_if_fits(w, out);
}

bool decode_welcome(const uint8_t* p, size_t len, WelcomeMsg& m) {
    ByteReader r(p, len);
    uint16_t count;
    if (!r.get_u16(count)) return false;

    // a peer takes at least 12 bytes, so a bigger count is a lie, not an allocation request
    if ((size_t)count * 12 > r.remaining()) return false;

    m.peers.clear();
    for (uint16_t i = 0; i < count; i++) {
        PeerInfo peer;
        if (!r.get_u32(peer.id) || !r.get_str(peer.name) || !r.get_u32(peer.ip) ||
            !r.get_u16(peer.port))
            return false;
        m.peers.push_back(peer);
    }
    return r.get_u16(m.file_count) && r.remaining() == 0;
}

bool encode_chat(const ChatMsg& m, std::vector<uint8_t>& out) {
    if (m.text.size() > CHAT_TEXT_MAX) return false;
    ByteWriter w;
    w.put_u32(m.target_id);
    w.put_str(m.text);
    return append_if_fits(w, out);
}

bool decode_chat(const uint8_t* p, size_t len, ChatMsg& m) {
    ByteReader r(p, len);
    if (!r.get_u32(m.target_id) || !r.get_str(m.text) || r.remaining() != 0) return false;
    return m.text.size() <= CHAT_TEXT_MAX;
}

bool encode_have_map(const HaveMapMsg& m, std::vector<uint8_t>& out) {
    if (m.bitmap.empty()) return false;
    ByteWriter w;
    w.put_u32(m.file_id);
    w.put_u32(m.peer_id);
    w.put_bytes(m.bitmap.data(), m.bitmap.size());
    return append_if_fits(w, out);
}

bool decode_have_map(const uint8_t* p, size_t len, HaveMapMsg& m) {
    ByteReader r(p, len);
    if (!r.get_u32(m.file_id) || !r.get_u32(m.peer_id)) return false;
    if (r.remaining() == 0) return false;
    m.bitmap.assign(r.cursor(), r.cursor() + r.remaining());
    return true;
}

size_t have_map_bitmap_bytes(uint32_t chunk_count) {
    return ((size_t)chunk_count + 7) / 8;
}
