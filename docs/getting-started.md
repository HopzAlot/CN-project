# Getting started

## Setup (everyone, once)

1. Install a compiler and CMake.
   - Linux: `sudo apt install build-essential cmake git`
   - Windows: Git, CMake, and either Visual Studio (Desktop C++ workload) or MSYS2/MinGW. The whole team uses the same one, decide together.
2. Clone the repo.
3. Set your own git identity, so commits show who did what:

        git config user.name "Your Name"
        git config user.email "you@example.com"

4. Build and run once:

        cmake -S . -B build
        cmake --build build
        peer          (build/peer on Linux)

   You should see `p2p_share peer, protocol v1`. If not, tell Hamdan.
5. Read `docs/design.md`, `docs/protocol.md` and `docs/conventions.md`.

## Daily routine

    git pull --rebase origin main
    git checkout -b module/short-name       (once per task)
    ... work, build, test ...
    git add <your files>
    git commit -m "module: what changed"
    git push origin module/short-name

Commit at least once a day, even if the task is half done. Merge to `main` only when it builds.

## Checklists

Tick these off as you go. Dates are from `docs/plan.md`.

### Rehan: transport (`src/transport/`)

- [ ] Day 1-2: write `transport/reliable_channel.h` (send, receive callback, per-peer and per-stream state). Commit it.
- [ ] Day 3: send with sequence numbers, receiver sends ACK for every reliable packet.
- [ ] Day 4: retransmit on timeout (timer thread), give up after MAX_RETRIES.
- [ ] Day 5: sliding window of 16, reorder buffer, drop duplicates, two streams.
- [ ] Day 6: test program in `tests/` with artificial packet loss (drop X % in the send path). Check order and no duplicates at 20 % loss.
- [ ] Day 12: measure goodput and retransmissions at 0, 5, 10, 20 % loss. Write the report section.

### Hamdan: foundation, protocol, transfer, CLI (`app/`, `src/common/`, `src/protocol/`, `src/transfer/`)

- [ ] Day 1: get the Windows build working. Fix `CMakeLists.txt` if needed.
- [ ] Day 1-2: write `common/socket_compat.h/.cpp` (create, bind, sendto, recvfrom, close; Windows startup). Commit early, everyone needs it.
- [ ] Day 2: write `protocol/packet.h/.cpp` (header struct, encode, decode) following `docs/protocol.md`. Run the freeze discussion for the protocol.
- [ ] Day 3-4: CLI loop in `app/main.cpp` (`--name`, `--port`, `--join`; commands chat, list, files, download, quit).
- [ ] Day 5-6: download logic in `transfer/` that works on a local test chunk map: request chunks from several holders in parallel, check CRC32, retry.
- [ ] Day 10-11: failover when a holder dies mid-download.
- [ ] Day 12: measure download time vs number of sources and failover time. Write the report section.

### Hamna: membership and chat (`src/membership/`, `src/chat/`)

- [ ] Day 1-2: write `membership/peer_table.h` and `chat/chat.h`. Commit them.
- [ ] Day 3: peer table (add, remove, list, last-seen time), guarded by a mutex.
- [ ] Day 4: JOIN and WELCOME handling, so a new peer gets the peer list from a bootstrap peer.
- [ ] Day 5: HEARTBEAT sending and a timer that removes peers silent for 5 s. LEAVE handling.
- [ ] Day 6: chat send to every peer over the transport, print received messages.
- [ ] Day 7-9: optional LAN broadcast discovery, private messages.
- [ ] Day 12: measure join time, crash detection time, chat delivery under loss. Write the report section.

### Afaq: storage (`src/storage/`)

- [ ] Day 1-2: write `storage/chunk_store.h` and `storage/placement.h`. Commit them.
- [ ] Day 3: split a file into 16 KB chunks, compute CRC32 per chunk and per file. Rebuild a file from chunks.
- [ ] Day 4: chunk store on disk under `peer_data/<name>/` (save, load, has).
- [ ] Day 5: placement: choose 2 different peers per chunk, so no peer holds a whole file.
- [ ] Day 6: chunk map (`chunk -> holders`) and HAVE updates. Test with fake peers.
- [ ] Day 7-9: join sync, a new peer takes its share of chunks.
- [ ] Day 10-11: re-replication when a peer dies.
- [ ] Day 12: measure storage per peer vs replication and peer count. Write the report section.

## Everyone

- [ ] Day 2: after the headers are committed, read each other's headers. Raise problems now, not on Day 8.
- [ ] Day 7-9: integration sessions together, 4 peers on real machines.
- [ ] Day 13: demo rehearsal. Kill a peer mid-download at least three times.
