# Design

Every peer runs the same program. There is no server. One UDP socket per peer.

## Layers (each only talks to the one below it)

    app (CLI)
    chat | membership | storage + transfer
    transport   (reliable delivery, streams)
    protocol    (header, encode/decode)
    common      (sockets, crc32, constants)

## Threads per peer

- receiver: reads the socket, hands packets to transport
- timer: retransmits unacked packets, sends heartbeats, expires dead peers
- cli: reads the user's commands
- downloader workers: only while a download is running

Shared tables (peer table, chunk map) are guarded by one mutex each.

## Decisions

| Topic | Choice | Why |
|---|---|---|
| Reliability | Selective repeat, window 16, fixed 200 ms RTO, per-packet ACK | Simple, and it gives in-order, no-duplicate delivery per sender |
| Streams | Stream 0 = control + chat, stream 1 = file data. Each has its own seq numbers | A big transfer cannot block chat (head-of-line blocking) |
| Discovery | Bootstrap peer (`--join ip:port`) is the main way; LAN broadcast on port 9000 is optional | Several peers on one laptop cannot share a broadcast port; bootstrap always works |
| Membership | HEARTBEAT every 1 s; a peer silent for 5 s is dead; LEAVE for a clean exit | Detects join, leave and crash |
| Chunking | 16 KB chunks, each sent as 16 packets of 1 KB | Packets stay under MTU; chunks are small enough to spread around |
| Replication | 2 copies per chunk, on 2 different peers, never the whole file on one peer | Required by the brief; survives one peer leaving |
| Chunk map | Every peer keeps the full map `chunk -> holders`, updated by HAVE messages | No central index; any peer can start a download |
| Integrity | CRC32 per chunk, CRC32 of the whole file in FILE_ANNOUNCE | Short to write, catches corruption |
| Download | Window of chunk requests spread over different holders; on timeout or dead peer, retry the chunk on another holder | Multi-source plus failover |
| Re-replication | When a peer dies, chunks left with 1 copy get copied to a new peer by the lowest-id surviving holder | No two peers do the same work |
| Join | New peer receives peer list, file list, chunk map, then pulls chunks from the most loaded peers until balanced | Takes over its share |

## Requirement to module map

| Requirement | Module |
|---|---|
| Peer discovery and membership | membership |
| Group chat, ordered, no duplicates | chat + transport |
| Reliable data transfer over UDP | transport |
| Distributed file storage | storage |
| File availability at joining | membership + storage |
| Multi-source download | transfer |
| Fault tolerance | transfer + membership |
| Bonus: re-replication | storage |
| Bonus: private messages | chat (small) |
