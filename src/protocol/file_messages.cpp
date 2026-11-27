#include "protocol/file_messages.h"

#include "common/constants.h"
#include "protocol/bytes.h"

uint32_t chunks_for_size(uint64_t size) {
    return (uint32_t)((size + CHUNK_SIZE - 1) / CHUNK_SIZE);
}

static bool valid_announce(const FileAnnounceMsg& m) {
    if (m.name.empty() || m.name.size() > FILE_NAME_MAX) return false;
    if (m.size > (uint64_t)FILE_CHUNKS_MAX * CHUNK_SIZE) return false;
    return m.chunk_count == chunks_for_size(m.size);
}

bool encode_file_announce(const FileAnnounceMsg& m, std::vector<uint8_t>& out) {
    if (!valid_announce(m)) return false;
    ByteWriter w;
    w.put_u32(m.file_id);
    w.put_str(m.name);
    w.put_u64(m.size);
    w.put_u32(m.chunk_count);
    w.put_u32(m.file_crc);
    out.insert(out.end(), w.buf.begin(), w.buf.end());
    return true;
}

bool decode_file_announce(const uint8_t* p, size_t len, FileAnnounceMsg& m) {
    ByteReader r(p, len);
    if (!r.get_u32(m.file_id) || !r.get_str(m.name) || !r.get_u64(m.size) ||
        !r.get_u32(m.chunk_count) || !r.get_u32(m.file_crc) || r.remaining() != 0)
        return false;
    return valid_announce(m);
}

bool encode_chunk_ref(const ChunkRefMsg& m, std::vector<uint8_t>& out) {
    ByteWriter w;
    w.put_u32(m.file_id);
    w.put_u32(m.chunk_index);
    out.insert(out.end(), w.buf.begin(), w.buf.end());
    return true;
}

bool decode_chunk_ref(const uint8_t* p, size_t len, ChunkRefMsg& m) {
    ByteReader r(p, len);
    return r.get_u32(m.file_id) && r.get_u32(m.chunk_index) && r.remaining() == 0;
}

static bool valid_fragment(uint16_t frag_no, uint16_t frag_count, size_t data_size) {
    if (frag_count == 0 || frag_count > CHUNK_SIZE / FRAGMENT_SIZE) return false;
    if (frag_no >= frag_count) return false;
    return data_size >= 1 && data_size <= FRAGMENT_SIZE;
}

bool encode_chunk_data(const ChunkDataMsg& m, std::vector<uint8_t>& out) {
    if (!valid_fragment(m.frag_no, m.frag_count, m.data.size())) return false;
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

bool decode_chunk_data(const uint8_t* p, size_t len, ChunkDataMsg& m) {
    ByteReader r(p, len);
    if (!r.get_u32(m.file_id) || !r.get_u32(m.chunk_index) || !r.get_u16(m.frag_no) ||
        !r.get_u16(m.frag_count) || !r.get_u32(m.chunk_crc))
        return false;
    if (!valid_fragment(m.frag_no, m.frag_count, r.remaining())) return false;
    m.data.assign(r.cursor(), r.cursor() + r.remaining());
    return true;
}
