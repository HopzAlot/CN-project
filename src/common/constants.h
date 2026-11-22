#ifndef CONSTANTS_H
#define CONSTANTS_H

// Shared tunables. Change a number here, not inside a module.

#define PROTOCOL_VERSION   1
#define HEADER_SIZE        14      // bytes, see docs/protocol.md
#define MAX_PAYLOAD        1100    // 1024 data + CHUNK_DATA fields, datagram stays under the 1500 byte MTU
#define MAX_DATAGRAM       (HEADER_SIZE + MAX_PAYLOAD)

#define CHUNK_SIZE         16384   // 16 KB, sent as 16 packets
#define FRAGMENT_SIZE      1024    // data bytes per CHUNK_DATA packet
#define REPLICATION        2       // copies of every chunk

#define WINDOW_SIZE        16      // unacked packets allowed per peer/stream
#define RTO_MS             200     // retransmit timeout
#define MAX_RETRIES        10

#define HEARTBEAT_MS       1000
#define PEER_TIMEOUT_MS    5000

#define DISCOVERY_PORT     9000

#define DOWNLOAD_WINDOW    8       // chunk requests in flight per download
#define PER_PEER_REQUESTS  4       // at most this many of them to one holder
#define CHUNK_TIMEOUT_MS   3000    // whole chunk, transport already retries single packets
#define CHUNK_MAX_TRIES    6       // attempts per chunk over all holders before giving up
#define NO_HOLDER_WAIT_MS  10000   // a chunk with no live holder waits this long for re-replication

#endif
