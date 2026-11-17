# p2p_share

Serverless group chat and distributed file sharing over UDP (C++17, CMake).
Computer Networks semester project. Team: Hamna, Afaq, Hamdan, Rehan.

## Build

Linux / macOS:

    cmake -S . -B build
    cmake --build build
    ./build/peer

Windows (Visual Studio or MSYS2/MinGW, with CMake installed):

    cmake -S . -B build
    cmake --build build --config Release
    build\Release\peer.exe        (MSVC)   or   build\peer.exe   (MinGW)

## Why CMake

Half the team is on Linux and half on Windows. Each has a different compiler
(g++, MSVC, MinGW) and a different build tool (make, Visual Studio, ninja), so
one hand-written Makefile or Visual Studio project would only work for some of us.

CMake solves this. `CMakeLists.txt` describes the project once (which files,
which C++ version, which libraries). CMake reads it and generates the build
files for whatever machine it runs on. Everyone runs the same two commands and
gets a working build.

## How it works

    cmake -S . -B build        step 1: configure, reads CMakeLists.txt and writes build files into build/
    cmake --build build        step 2: compile, calls the real compiler and produces the peer program

Our `CMakeLists.txt` does four things:

1. Sets C++17.
2. Collects every `.cpp` under `src/` automatically, so adding a new file needs no CMake edit.
3. Builds the `peer` program from `app/main.cpp` plus those files.
4. Links the thread library, and `ws2_32` (Windows sockets) on Windows only.

Everything generated goes into `build/`, which git ignores. If something acts
strange, delete `build/` and configure again. Never commit it.

## Folder layout

    app/        main.cpp, command line and CLI loop
    src/
      common/       constants, socket wrapper, logging, crc32
      protocol/     packet header and message encode/decode
      transport/    reliable delivery over UDP (ack, retransmit, window)
      membership/   discovery, heartbeat, peer table
      chat/         group chat
      storage/      chunking, chunk store, replica placement
      transfer/     multi-source download, failover
    tests/      small test programs per module
    tools/      helper scripts (make test files, loss simulation)
    docs/       protocol spec, design, plan, conventions, report

## Docs

- docs/getting-started.md  setup and a checklist for each member
- docs/design.md       how the system works and why
- docs/protocol.md     packet format and message sequences
- docs/plan.md         who does what, day by day, git workflow
- docs/conventions.md  code style and commenting rules
