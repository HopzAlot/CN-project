# Plan

Start: 17 Nov 2026 (Day 1). Deadline: 30 Nov 2026 (13 days). Last day is spare time, not work time.

## Who owns what

| Member | Module | Folder |
|---|---|---|
| Rehan | repo, CMake, common, protocol, transfer (download, failover), CLI/app | `common/` `protocol/` `transfer/` `app/` |
| Hamdan | reliable UDP: ack, retransmit, window, streams | `transport/` |
| Hamna | discovery, heartbeat, peer table, chat | `membership/` `chat/` |
| Afaq | chunking, chunk store, placement, re-replication, join sync | `storage/` |

Everyone writes the report section for their own module and measures their own part.

| Member | Measurement for the report |
|---|---|
| Hamdan | goodput and retransmissions vs packet loss (0, 5, 10, 20 %) |
| Hamna | join time, crash detection time, chat delivery under loss |
| Afaq | storage per peer vs replication factor and number of peers |
| Rehan | download time vs number of sources, failover time when a peer is killed |

## Timeline

| Days | Dates | Goal |
|---|---|---|
| 1-2 | 17-18 Nov | Understand the project. Repo, CMake, folders. Freeze protocol.md and the header files (the interfaces between modules). |
| 3-6 | 19-22 Nov | Each module works alone, with a small test in `tests/`. |
| 7-9 | 23-25 Nov | Integration: peers find each other, chat works, a file downloads. |
| 10-11 | 26-27 Nov | Fault tolerance: kill a peer mid-download, re-replication, join sync, packet loss testing. |
| 12 | 28 Nov | Measurements, graphs. |
| 13 | 29 Nov | Report, 4-peer demo rehearsal, zip. |
| - | 30 Nov | Deadline. Spare. |

## Why this split works

Modules meet only at small interfaces (the headers). Hamdan can test transport with a plain echo program, Afaq can test storage on files without a network, Hamna can test membership with fake packets. Nobody waits for another person after Day 2.

## Day 2 task for everyone

Each owner writes the public header of their module (class name, function signatures, one line each), commits it, and the others read it. This is the contract.
