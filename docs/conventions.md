# Conventions

## Code

- C++17. Standard library only. No external libraries.
- Threads: `std::thread`, `std::mutex`. No pthread, no Windows API outside `common/socket_compat`.
- Sockets: only through `common/socket_compat.h`, so the same code builds on Linux and Windows.
- Keep it plain: structs, vectors, maps, small functions. No templates, no inheritance trees, no clever tricks.
- Functions return `bool` for success or failure. No exceptions.
- A function should fit on one screen.

## Naming

| Thing | Style | Example |
|---|---|---|
| files | snake_case | `peer_table.cpp` |
| classes, structs | PascalCase | `PeerTable` |
| functions, variables | snake_case | `send_packet` |
| constants, macros | UPPER_CASE | `CHUNK_SIZE` |

One header and one source per class. Header guard like `PEER_TABLE_H`. No `using namespace std` in headers.

## Comments

- Only where the code does not explain itself: the reason, a trap, a format.
- One line, `//`, plain words.
- No banners, no author tags, no commented-out code, no comment that repeats the code.

Good:

    // ACK even duplicates, the first ACK may have been lost
Bad:

    // loop over the peers
    for (...)

## Git

- `main` always builds. Never push code that does not compile.
- One branch per task: `module/short-name`, e.g. `transport/sliding-window`.
- Small commits, at least one per working day. Message: `module: what changed`, e.g. `transport: add ack handling`.
- Before pushing: `git pull --rebase origin main`.
- Merge to `main` when the task works. Another member glances at it first.
- Only edit another person's module after telling them.
