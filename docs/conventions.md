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
- Nobody pushes directly to `main`. All work goes through a pull request (PR).
- Small commits, at least one per working day. Message: `module: what changed`, e.g. `transport: add ack handling`.
- Only edit another person's module after telling them.

### Pull request flow

1. Update main: `git checkout main` then `git pull origin main`.
2. Make your branch: `git checkout -b module/short-name`, e.g. `transport/sliding-window`.
3. Work and commit on your branch. Push it: `git push -u origin module/short-name`.
4. Before opening the PR, bring in the latest main: `git pull --rebase origin main`. Build and test again.
5. Open a PR on GitHub into `main`. Title like a commit message. In the description say what it does and how you tested it, in two or three lines.
6. At least one other member reads it and approves. The owner of the touched module reviews if it is not the author.
7. The author merges (Squash or Merge, either is fine, pick one and stay with it). Delete the branch.
8. Everyone pulls `main` again before starting the next task.

Keep PRs small, one task each, so reviews take minutes. Open a PR as soon as a task works, not at the end of the week.
