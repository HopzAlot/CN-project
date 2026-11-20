#include <atomic>
#include <cstdlib>
#include <iostream>
#include <random>
#include <sstream>
#include <string>
#include <thread>

#include "common/constants.h"
#include "common/log.h"
#include "common/socket_compat.h"
#include "common/time_util.h"
#include "protocol/packet.h"
#include "transfer/downloader.h"

struct Options {
    std::string name = "peer";
    int port = 5000;
    std::string join;   // "ip:port" of a bootstrap peer, empty = first peer
    bool verbose = false;
};

struct App {
    Options opt;
    sock_t sock = INVALID_SOCK;
    uint32_t my_id = 0;
    std::atomic<bool> running{true};
    Downloader download;
};

static App app;

static void usage() {
    std::cout << "usage: peer --name <name> [--port <port>] [--join <ip:port>] [--verbose]\n";
}

static bool parse_args(int argc, char** argv, Options& o) {
    for (int i = 1; i < argc; i++) {
        std::string a = argv[i];
        bool has_value = i + 1 < argc;
        if (a == "--name" && has_value) o.name = argv[++i];
        else if (a == "--port" && has_value) o.port = std::atoi(argv[++i]);
        else if (a == "--join" && has_value) o.join = argv[++i];
        else if (a == "--verbose") o.verbose = true;
        else return false;
    }
    if (o.port <= 0 || o.port > 65535 || o.name.empty()) return false;
    std::string ip;
    int port;
    if (!o.join.empty() && !parse_addr(o.join, ip, port)) return false;
    return true;
}

static uint32_t random_id() {
    std::random_device rd;
    std::mt19937 gen(rd());
    uint32_t id = 0;
    while (id == 0) id = (uint32_t)gen();
    return id;
}

// Unreliable packet straight on the socket. Reliable sends will go through transport.
static bool send_raw(const std::string& ip, int port, uint8_t type, const uint8_t* payload, int len) {
    PacketHeader h;
    h.type = type;
    h.stream = STREAM_CONTROL;
    h.sender_id = app.my_id;
    std::vector<uint8_t> pkt;
    if (!encode_packet(h, payload, len, pkt)) return false;
    return udp_send(app.sock, ip, port, pkt.data(), (int)pkt.size());
}

static void handle_packet(const PacketHeader& h, const uint8_t* payload, const std::string& ip, int port) {
    (void)payload;
    std::ostringstream s;
    s << msg_type_name(h.type) << " from " << std::hex << h.sender_id << std::dec << " at " << ip << ":"
      << port << " (" << h.length << " bytes)";
    // TODO(integration): hand reliable packets to transport, then route by type to
    // membership, chat, storage and download.on_chunk_data / on_chunk_err
    if (h.type == MSG_HEARTBEAT && !app.opt.verbose) log_line("received " + s.str());
    else log_debug(s.str());
}

static void receiver_loop() {
    uint8_t buf[MAX_DATAGRAM + 64];
    while (app.running) {
        std::string ip;
        int port = 0;
        int n = udp_recv(app.sock, buf, sizeof(buf), ip, port, 200);
        if (n <= 0) continue;
        PacketHeader h;
        const uint8_t* payload = nullptr;
        if (!decode_packet(buf, n, h, payload)) {
            log_debug("dropped bad packet from " + ip);
            continue;
        }
        if (h.sender_id == app.my_id) continue;
        handle_packet(h, payload, ip, port);
    }
}

static void timer_loop() {
    while (app.running) {
        app.download.tick(now_ms());
        // TODO(integration): transport retransmits and membership heartbeats run from here too
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
}

static void print_help() {
    log_line("commands:\n"
             "  chat <text>          send to everyone\n"
             "  list                 peers in the group\n"
             "  files                files in the group\n"
             "  share <path>         add a file to the group\n"
             "  download <name>      fetch a file from its holders\n"
             "  status               download progress\n"
             "  ping <ip:port>       send one raw packet to test the network\n"
             "  help, quit");
}

static void cmd_status() {
    static const char* names[] = {"idle", "running", "done", "failed"};
    uint32_t done, total;
    app.download.progress(done, total);
    DownloadStats s = app.download.stats();
    std::ostringstream o;
    o << "download: " << names[app.download.state()] << ", " << done << "/" << total << " chunks, "
      << s.requests << " requests, " << s.timeouts << " timeouts, " << s.crc_failures << " crc failures, "
      << s.failovers << " failovers";
    if (app.download.state() == DL_FAILED) o << " (" << app.download.fail_reason() << ")";
    log_line(o.str());
}

static void cmd_ping(const std::string& arg) {
    std::string ip;
    int port;
    if (!parse_addr(arg, ip, port)) {
        log_line("usage: ping <ip:port>");
        return;
    }
    log_line(send_raw(ip, port, MSG_HEARTBEAT, nullptr, 0) ? "sent" : "send failed");
}

static void not_yet(const std::string& what) {
    log_line(what + ": not connected yet (waiting for module integration)");
}

// Returns false when the user wants to quit.
static bool run_command(const std::string& line) {
    std::istringstream in(line);
    std::string cmd;
    in >> cmd;
    std::string rest;
    std::getline(in, rest);
    if (!rest.empty() && rest[0] == ' ') rest.erase(0, 1);

    if (cmd.empty()) return true;
    if (cmd == "quit" || cmd == "exit") return false;
    if (cmd == "help") print_help();
    else if (cmd == "status") cmd_status();
    else if (cmd == "ping") cmd_ping(rest);
    else if (cmd == "chat" || cmd == "list" || cmd == "files" || cmd == "share" || cmd == "download")
        not_yet(cmd);
    else log_line("unknown command, type help");
    return true;
}

static void cli_loop() {
    std::string line;
    while (app.running && std::getline(std::cin, line)) {
        if (!run_command(line)) break;
    }
}

int main(int argc, char** argv) {
    if (!parse_args(argc, argv, app.opt)) {
        usage();
        return 1;
    }
    log_set_verbose(app.opt.verbose);
    if (!net_init() || !udp_open(app.opt.port, app.sock)) {
        log_line("cannot open UDP port " + std::to_string(app.opt.port));
        return 1;
    }
    app.my_id = random_id();

    std::ostringstream hello;
    hello << "p2p_share peer, protocol v" << PROTOCOL_VERSION << ", name " << app.opt.name << ", id " << std::hex
          << app.my_id << std::dec << ", port " << app.opt.port;
    log_line(hello.str());
    if (!app.opt.join.empty()) log_line("bootstrap peer: " + app.opt.join + " (join not connected yet)");
    log_line("type help for commands");

    std::thread receiver(receiver_loop);
    std::thread timer(timer_loop);
    cli_loop();

    // TODO(integration): send LEAVE to every peer before closing
    app.running = false;
    receiver.join();
    timer.join();
    udp_close(app.sock);
    net_cleanup();
    return 0;
}
