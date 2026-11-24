# Protocol specification (DRAFT v1)

Status: draft. To be frozen on Day 2. After that, changes need all four members to agree.

All integers are big endian (network byte order). Max payload = 1100 bytes (MAX_PAYLOAD), so a datagram is at most 14 + 1100 bytes. A payload never spans two packets.

Strings are a u16 length followed by that many bytes (UTF-8, no terminating zero).
Lists are a u16 count followed by the items.

`sender_id` is a random u32 chosen at startup (0 is not allowed). If a JOIN arrives with an id
already in the peer table from a different ip:port, the bootstrap rejects it and the new peer picks another.

## Header (14 bytes)

| Offset | Size | Field | Meaning |
|---|---|---|---|
| 0 | 1 | version | protocol version, currently 1 |
| 1 | 1 | type | message type, table below |
| 2 | 1 | stream | 0 = control and chat, 1 = file data |
| 3 | 1 | flags | bit 0 = RELIABLE (receiver must ACK), others 0 |
| 4 | 4 | sender_id | id of the sending peer |
| 8 | 4 | seq | sequence number, counted per (receiver, stream), starts at 1. Unreliable packets use 0 |
| 12 | 2 | length | payload length in bytes |

Packets with a wrong version or a length that does not match are dropped.

## Message types

| Code | Name | Stream | Reliable | Meaning |
|---|---|---|---|---|
| 0x01 | DISCOVER | 0 | no | broadcast "who is out there" |
| 0x02 | JOIN | 0 | yes | I want to join / I have joined |
| 0x03 | WELCOME | 0 | yes | reply to JOIN: the peer list |
| 0x04 | HEARTBEAT | 0 | no | I am alive |
| 0x05 | LEAVE | 0 | yes | I am leaving |
| 0x06 | CHAT | 0 | yes | chat text, to everyone or one peer |
| 0x07 | ACK | same as the acked packet | no | I received packet n |
| 0x08 | FILE_ANNOUNCE | 0 | yes | a file exists: name, size, chunk count, crc |
| 0x09 | CHUNK_STORE | 1 | yes | keep this chunk (owner pushes a chunk to a holder) |
| 0x0A | HAVE | 0 | yes | I now hold this chunk |
| 0x0B | CHUNK_REQUEST | 1 | yes | send me this chunk |
| 0x0C | CHUNK_DATA | 1 | yes | one fragment of a requested chunk |
| 0x0D | CHUNK_ERR | 1 | yes | I do not have that chunk |
| 0x0E | HAVE_MAP | 0 | yes | which chunks of a file a peer holds, as a bitmap |

## Streams

- Stream 0 carries everything small and urgent: membership, chat, file announcements, HAVE and HAVE_MAP.
- Stream 1 carries only bulk chunk traffic: CHUNK_STORE, CHUNK_REQUEST, CHUNK_DATA, CHUNK_ERR.
- Each (peer, stream) has its own seq numbers and its own window, so a running transfer cannot delay heartbeats, chat or HAVE.
- An ACK is sent on the same stream as the packet it acknowledges.

## Payload layouts

| Message | Payload |
|---|---|
| DISCOVER | str name, u16 listen port |
| JOIN | str name, u16 listen port |
| WELCOME | u16 peer_count, then per peer: u32 id, str name, u32 IPv4, u16 port. Then u16 file_count (FILE_ANNOUNCEs that follow) |
| HEARTBEAT | empty |
| LEAVE | empty |
| CHAT | u32 target_id (0 = everyone), str text (at most 1000 bytes) |
| ACK | u32 acked seq |
| FILE_ANNOUNCE | u32 file_id, str name, u64 size, u32 chunk_count, u32 file_crc32 |
| HAVE, CHUNK_REQUEST, CHUNK_ERR | u32 file_id, u32 chunk_index |
| CHUNK_STORE, CHUNK_DATA | u32 file_id, u32 chunk_index, u16 frag_no, u16 frag_count, u32 chunk_crc32, data (1 to 1024 bytes, the rest of the payload) |
| HAVE_MAP | u32 file_id, u32 peer_id, bitmap |

The peer's IP in JOIN and DISCOVER is not sent, the receiver uses the address the packet came from.

HAVE_MAP bitmap: `ceil(chunk_count / 8)` bytes, `chunk_count` comes from the FILE_ANNOUNCE of that file.
Chunk `i` is bit `7 - (i % 8)` of byte `i / 8` (chunk 0 is the top bit of byte 0). Unused bits are 0.
A set bit means the peer holds that chunk. FILE_ANNOUNCE is always sent before HAVE_MAP, and both are on stream 0, so the order is kept.

## Chunk transfer

CHUNK_STORE and CHUNK_DATA are the same layout, only the meaning differs: CHUNK_STORE is saved by the receiver as its own copy, CHUNK_DATA is the answer to a CHUNK_REQUEST.

- A chunk is at most CHUNK_SIZE (16 KB) and is cut into fragments of FRAGMENT_SIZE (1024 bytes). A full chunk is 16 fragments, the last chunk of a file may be shorter and have fewer.
- `frag_count = ceil(chunk_length / 1024)`. Fragment `k` carries bytes `[k * 1024, k * 1024 + len)` of the chunk.
- Every fragment carries the same `chunk_crc32`, the crc32 of the whole chunk.
- The receiver collects all fragments, rebuilds the chunk and checks the crc32. On a mismatch it discards the chunk.
- After saving a CHUNK_STORE chunk the holder sends HAVE to every peer. The owner treats that HAVE as the confirmation. No HAVE after CHUNK_TIMEOUT_MS means the owner retries, or picks another holder.
- A CHUNK_REQUEST for a chunk the peer does not have is answered with CHUNK_ERR.

## Reliability rules

- A reliable packet is resent every RTO (200 ms) until its ACK arrives, at most 10 times, then the peer is treated as unreachable.
- The receiver ACKs every reliable packet, including duplicates (the earlier ACK may have been lost).
- Per (sender, stream) the receiver delivers in seq order, buffers early packets, drops duplicates.
- Sender keeps at most 16 unacked packets per (receiver, stream).
- ACKs are per packet (selective): ACK n means "I have exactly packet n", not "everything up to n". An early packet is ACKed and buffered, the sender resends only what stays unacked.
- Unreliable packets (flag 0: DISCOVER, HEARTBEAT, ACK) use seq 0 and take no part in ordering, duplicate checks or ACKs. Otherwise one lost heartbeat would leave a gap that blocks every later chat message.
- The receiver decides by the RELIABLE flag. A reliable packet with seq 0 is invalid and dropped.
- Reorder buffer is bounded: per (sender, stream) the receiver accepts only seq in `[expected, expected + WINDOW_SIZE)` and drops anything outside it.

### JOIN resets transport state

Seq numbers are per (peer, stream). If one side forgot a peer (timeout) and the other did not, the new seq 1 looks like an old duplicate and is dropped. So a JOIN always means a fresh start:

- A peer that **sends** a JOIN to X wipes all its send and receive state for X (both streams) first, so its seq to X starts again at 1.
- A peer that **receives** a JOIN (seq 1) from X wipes all its send and receive state for X (both streams) before processing the JOIN.
- Exception, a resend: a JOIN is resent when its ACK is lost, and the resend also has seq 1. A JOIN seq 1 from X that arrives less than `(MAX_RETRIES + 1) * RTO_MS` (2.2 s) after the last reset for X is a resend. It is only ACKed, not delivered again and not a reset. Wiping again would throw away the WELCOME and FILE_ANNOUNCEs still being sent to X, and X would wait for them forever.
- A real rejoin after that time (for example after a 5 s timeout) is a new reset.

The `sender_id` is not changed on rejoin, because chunk ownership (`ChunkMap`, HAVE) is tied to it.

## Limits

A payload never spans two packets, so every message must fit in MAX_PAYLOAD (1100 bytes).

| Limit | Value | Why |
|---|---|---|
| Peers in WELCOME | about 49 | 4 bytes of counts, then 12 + name length per peer (22 bytes for a 10 character name): (1100 - 4) / 22 |
| Chat text | 1000 bytes | CHAT payload is 4 (target_id) + 2 (length) + text |
| File size | about 136 MiB (8736 chunks) | HAVE_MAP has 8 fixed bytes, so the bitmap is at most 1092 bytes = 8736 bits, times CHUNK_SIZE (16 KB) |
| File name | keep short (under 255 bytes) | FILE_ANNOUNCE must fit in one packet |

Senders must check these limits before sending. Receivers drop a packet whose payload does not parse.

## Sequences

Join:

    new -> bootstrap : JOIN
    bootstrap -> new : WELCOME (peer list, number of files)
    bootstrap -> new : FILE_ANNOUNCE, one per file
    bootstrap -> new : HAVE_MAP, one per (file, peer that holds chunks)
    new -> every other peer : JOIN       (so all learn about it)
    peers -> new     : HEARTBEAT from now on

Join sync (new peer takes its share of chunks):

    new -> holders : CHUNK_REQUEST, then CHUNK_DATA back, until it holds its fair share
    new -> every peer : HAVE for each chunk it saved

Chat:

    sender -> each peer : CHAT (stream 0, seq n)
    peer -> sender      : ACK n

Sharing a file (owner):

    owner -> everyone : FILE_ANNOUNCE
    owner -> each of the 2 holders of chunk i : CHUNK_STORE x frag_count   (stream 1)
    holder -> everyone : HAVE (after crc32 check and save)

Download of one chunk:

    downloader -> holder : CHUNK_REQUEST
    holder -> downloader : CHUNK_DATA x16   (stream 1)
    downloader -> holder : ACK for each fragment
    downloader checks crc32; on failure or timeout asks another holder

Crash:

    no HEARTBEAT for 5 s -> peer removed from table -> chunks with < 2 holders re-replicated

## Using the transport (rules for every module)

The transport is `ReliableChannel` in `src/transport/reliable_channel.h`. The header codec (`encode_datagram`, `decode_datagram`, `Header`, `MSG_JOIN`, `MSG_ACK`) is in `src/protocol/header.h`. Do not write a second header codec. `protocol/packet.h` is for the payload encode/decode and the other message type codes, and should include `protocol/header.h` for the two it already defines.

- Send with `send_reliable(ip, port, stream, type, payload)` for everything that must arrive: JOIN, WELCOME, LEAVE, CHAT, FILE_ANNOUNCE, HAVE, HAVE_MAP and all chunk messages. Use `send_unreliable` for HEARTBEAT and DISCOVER only. ACKs are sent by the transport, never by other modules.
- JOIN is always sent with `send_reliable` on stream 0. That is what triggers the state reset above.
- Per (peer, stream) at most WINDOW_SIZE (16) packets are in flight, the rest wait in a queue of SEND_QUEUE_LIMIT (4096). When `send_reliable` returns false the queue is full or the payload is over MAX_PAYLOAD. Slow down and try again, do not ignore it. The downloader must respect this.
- Received messages arrive in `on_message`, always on the receiver thread, one at a time, in seq order per (sender, stream). Keep the handler short. It may call `send_*` itself, it must not block for long (no waiting for a download, no sleeping).
- `on_unreachable(ip, port)` means 10 resends of one packet got no ACK. Membership should treat the peer as dead and call `forget_peer(ip, port)`. `forget_peer` must also be called whenever a peer is removed from the peer table, so its queues and seq numbers are dropped.
- Packets whose `sender_id` is our own id are dropped, so our own DISCOVER broadcast does not come back to us.
- The first JOIN to a bootstrap peer is sent to `ip:port` before its id is known. That is fine, the transport tracks the sender state by address and the receiver state by `sender_id`.
- Message type codes that the transport itself reads: JOIN (reset rule) and ACK. All other types are passed up untouched.
