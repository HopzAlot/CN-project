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

- docs/design.md       how the system works and why
- docs/protocol.md     packet format and message sequences
- docs/plan.md         who does what, day by day, git workflow
- docs/conventions.md  code style and commenting rules
