# p2p_share

Serverless group chat and distributed file sharing over UDP (C++17, CMake).
Computer Networks semester project. Team: Hamna, Afaq, Hamdan, Rehan.

## Build

Linux / macOS:

    cmake -S . -B build
    cmake --build build
    ctest --test-dir build          runs every tests/test_*.cpp
    ./build/peer --name alice --port 5001

Windows (Visual Studio 2022 with the Desktop C++ workload, and CMake):

    cmake -S . -B build
    cmake --build build --config Release
    build\Release\peer.exe

## Debug and Release

The same code can be compiled two ways.

- **Debug**: no optimization, extra info for the debugger. Slow, but you can step through code and set breakpoints. Use it while developing.
- **Release**: optimized and fast. Use it for the demo and for **every measurement in the report**. Numbers from a Debug build are not realistic.

Linux (pick the type when configuring):

    cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
    cmake -S . -B build -DCMAKE_BUILD_TYPE=Release

Visual Studio (pick the type when building):

    cmake --build build --config Debug
    cmake --build build --config Release

In VS Code with the CMake Tools extension, choose the type from the bar at the bottom.
If you switch types on Linux, delete `build/` first or use a separate folder like `build-release/`.

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

Our `CMakeLists.txt` does five things:

1. Sets C++17.
2. Collects every `.cpp` under `src/` automatically into one library, `p2p_core`, so adding a new file needs no CMake edit.
3. Builds the `peer` program from `app/main.cpp` linked with that library.
4. Builds every `tests/test_*.cpp` as its own small program and registers it with `ctest`. To add a test, just create `tests/test_<module>.cpp` with a `main` that returns 0 on success (see `tests/check.h`).
5. Links the thread library, and `ws2_32` (Windows sockets) on Windows only.

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
- docs/conventions.md  code style, commenting rules, and the pull request workflow

## Workflow in one line

Pull `main`, make your own branch, commit daily, open a pull request when the task works. Nobody pushes to `main` directly.
