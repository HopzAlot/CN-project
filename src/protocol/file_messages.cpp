#include "protocol/file_messages.h"

#include "common/constants.h"
#include "protocol/bytes.h"

bool encode_file_announce(const FileAnnounceMsg& m, std::vector<uint8_t>& out) {
    ByteWriter w;
    w.put_u32(m.file_id);
    w.put_str(m.name);
    w.put_u64(m.size);
    w.put_u32(m.chunk_count);
    w.put_u32(m.file_crc);
    if (w.buf.size() > MAX_PAYLOAD) return false;
    out.insert(out.end(), w.buf.begin(), w.buf.end());
    return true;
}

bool decode_file_announce(const uint8_t* p, int len, FileAnnounceMsg& m) {
    ByteReader r(p, len);
    return r.get_u32(m.file_id) && r.get_str(m.name) && r.get_u64(m.size) &&
           r.get_u32(m.chunk_count) && r.get_u32(m.file_crc) && r.remaining() == 0;
}

bool encode_chunk_ref(const ChunkRefMsg& m, std::vector<uint8_t>& out) {
    ByteWriter w;
    w.put_u32(m.file_id);
    w.put_u32(m.chunk_index);
    out.insert(out.end(), w.buf.begin(), w.buf.end());
    return true;
}

bool decode_chunk_ref(const uint8_t* p, int len, ChunkRefMsg& m) {
    ByteReader r(p, len);
    return r.get_u32(m.file_id) && r.get_u32(m.chunk_index) && r.remaining() == 0;
}

bool encode_chunk_data(const ChunkDataMsg& m, std::vector<uint8_t>& out) {
    if (m.data.size() > FRAGMENT_SIZE) return false;
    if (m.frag_count == 0 || m.frag_no >= m.frag_count) return false;
    ByteWriter w;
    w.put_u32(m.file_id);
    w.put_u32(m.chunk_index);
    w.put_u16(m.frag_no);
    w.put_u16(m.frag_count);
    w.put_u32(m.chunk_crc);
    w.put_bytes(m.data.data(), m.data.size());
    out.insert(out.end(), w.buf.begin(), w.buf.end());
    return true;
}

bool decode_chunk_data(const uint8_t* p, int len, ChunkDataMsg& m) {
    ByteReader r(p, len);
    if (!(r.get_u32(m.file_id) && r.get_u32(m.chunk_index) && r.get_u16(m.frag_no) &&
          r.get_u16(m.frag_count) && r.get_u32(m.chunk_crc)))
        return false;
    if (m.frag_count == 0 || m.frag_no >= m.frag_count) return false;
    if (r.remaining() == 0 || r.remaining() > FRAGMENT_SIZE) return false;
    m.data.assign(r.cursor(), r.cursor() + r.remaining());
    return true;
}
