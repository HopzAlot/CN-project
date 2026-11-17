#ifndef CONSTANTS_H
#define CONSTANTS_H

// Shared tunables. Change a number here, not inside a module.

#define PROTOCOL_VERSION   1
#define HEADER_SIZE        14      // bytes, see docs/protocol.md
#define MAX_PAYLOAD        1024    // keeps the datagram under the 1500 byte MTU

#define CHUNK_SIZE         16384   // 16 KB, sent as 16 packets
#define REPLICATION        2       // copies of every chunk

#define WINDOW_SIZE        16      // unacked packets allowed per peer/stream
#define RTO_MS             200     // retransmit timeout
#define MAX_RETRIES        10

#define HEARTBEAT_MS       1000
#define PEER_TIMEOUT_MS    5000

#define DISCOVERY_PORT     9000

#endif
