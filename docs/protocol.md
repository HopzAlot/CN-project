# Protocol specification (DRAFT v1)

Status: draft. To be frozen on Day 2. After that, changes need all four members to agree.

All integers are big endian (network byte order). Max datagram = 14 + 1024 bytes.

## Header (14 bytes)

| Offset | Size | Field | Meaning |
|---|---|---|---|
| 0 | 1 | version | protocol version, currently 1 |
| 1 | 1 | type | message type, table below |
| 2 | 1 | stream | 0 = control and chat, 1 = file data |
| 3 | 1 | flags | bit 0 = RELIABLE (receiver must ACK), others 0 |
| 4 | 4 | sender_id | id of the sending peer |
| 8 | 4 | seq | sequence number, counted per (receiver, stream) |
| 12 | 2 | length | payload length in bytes |

Packets with a wrong version or a length that does not match are dropped.

## Message types

| Code | Name | Reliable | Payload |
|---|---|---|---|
| 0x01 | DISCOVER | no | name (broadcast, "who is out there") |
| 0x02 | JOIN | yes | name, listen port |
| 0x03 | WELCOME | yes | peer list, file list, chunk map |
| 0x04 | HEARTBEAT | no | nothing |
| 0x05 | LEAVE | yes | nothing |
| 0x06 | CHAT | yes | text (private message adds target id) |
| 0x07 | ACK | no | stream, acked seq |
| 0x08 | FILE_ANNOUNCE | yes | file id, name, size, chunk count, file crc32 |
| 0x09 | CHUNK_STORE | yes | file id, chunk index, data (owner pushes a chunk to a holder) |
| 0x0A | HAVE | yes | file id, chunk index (peer says it now holds this chunk) |
| 0x0B | CHUNK_REQUEST | yes | file id, chunk index |
| 0x0C | CHUNK_DATA | yes | file id, chunk index, fragment no, fragment count, chunk crc32, data |
| 0x0D | CHUNK_ERR | yes | file id, chunk index (I do not have it) |

Exact payload layouts are filled in when each module is written.

## Reliability rules

- A reliable packet is resent every RTO (200 ms) until its ACK arrives, at most 10 times, then the peer is treated as unreachable.
- The receiver ACKs every reliable packet, including duplicates (the earlier ACK may have been lost).
- Per (sender, stream) the receiver delivers in seq order, buffers early packets, drops duplicates.
- Sender keeps at most 16 unacked packets per (receiver, stream).

## Sequences

Join:

    new -> bootstrap : JOIN
    bootstrap -> new : WELCOME (peers, files, chunk map)
    new -> every peer: JOIN              (so all learn about it)
    peers -> new     : HEARTBEAT from now on

Chat:

    sender -> each peer : CHAT (stream 0, seq n)
    peer -> sender      : ACK n

Download of one chunk:

    downloader -> holder : CHUNK_REQUEST
    holder -> downloader : CHUNK_DATA x16   (stream 1)
    downloader -> holder : ACK for each fragment
    downloader checks crc32; on failure or timeout asks another holder

Crash:

    no HEARTBEAT for 5 s -> peer removed from table -> chunks with < 2 holders re-replicated
