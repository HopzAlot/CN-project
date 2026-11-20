#include "common/socket_compat.h"

#include <cstdlib>
#include <cstring>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
typedef int socklen_t;
#define CLOSE_SOCKET closesocket
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>
#define CLOSE_SOCKET close
#endif

bool net_init() {
#ifdef _WIN32
    WSADATA data;
    return WSAStartup(MAKEWORD(2, 2), &data) == 0;
#else
    return true;
#endif
}

void net_cleanup() {
#ifdef _WIN32
    WSACleanup();
#endif
}

bool udp_open(int port, sock_t& sock) {
    sock = INVALID_SOCK;
    sock_t s = (sock_t)socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (s == INVALID_SOCK) return false;

    int yes = 1;
    setsockopt(s, SOL_SOCKET, SO_REUSEADDR, (const char*)&yes, sizeof(yes));

    sockaddr_in addr;
    std::memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons((uint16_t)port);
    if (bind(s, (sockaddr*)&addr, sizeof(addr)) != 0) {
        CLOSE_SOCKET(s);
        return false;
    }
    sock = s;
    return true;
}

bool udp_enable_broadcast(sock_t sock) {
    int yes = 1;
    return setsockopt(sock, SOL_SOCKET, SO_BROADCAST, (const char*)&yes, sizeof(yes)) == 0;
}

int udp_local_port(sock_t sock) {
    sockaddr_in addr;
    socklen_t len = sizeof(addr);
    if (getsockname(sock, (sockaddr*)&addr, &len) != 0) return -1;
    return ntohs(addr.sin_port);
}

bool udp_send(sock_t sock, const std::string& ip, int port, const uint8_t* buf, int len) {
    sockaddr_in addr;
    std::memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons((uint16_t)port);
    if (inet_pton(AF_INET, ip.c_str(), &addr.sin_addr) != 1) return false;
    int sent = sendto(sock, (const char*)buf, len, 0, (sockaddr*)&addr, sizeof(addr));
    return sent == len;
}

int udp_recv(sock_t sock, uint8_t* buf, int cap, std::string& ip, int& port, int timeout_ms) {
    fd_set set;
    FD_ZERO(&set);
    FD_SET(sock, &set);
    timeval tv;
    tv.tv_sec = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;
    // first argument is ignored on Windows
    int ready = select((int)sock + 1, &set, nullptr, nullptr, &tv);
    if (ready < 0) return -1;
    if (ready == 0) return 0;

    sockaddr_in from;
    socklen_t from_len = sizeof(from);
    int n = recvfrom(sock, (char*)buf, cap, 0, (sockaddr*)&from, &from_len);
    if (n < 0) {
#ifdef _WIN32
        // Windows reports an ICMP "port unreachable" from an earlier send as an error here
        if (WSAGetLastError() == WSAECONNRESET) return 0;
#endif
        return -1;
    }

    char text[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &from.sin_addr, text, sizeof(text));
    ip = text;
    port = ntohs(from.sin_port);
    return n;
}

void udp_close(sock_t sock) {
    if (sock != INVALID_SOCK) CLOSE_SOCKET(sock);
}

bool parse_addr(const std::string& text, std::string& ip, int& port) {
    size_t colon = text.rfind(':');
    if (colon == std::string::npos || colon == 0 || colon + 1 >= text.size()) return false;
    ip = text.substr(0, colon);
    char* end = nullptr;
    long p = std::strtol(text.c_str() + colon + 1, &end, 10);
    if (*end != '\0' || p <= 0 || p > 65535) return false;
    port = (int)p;
    return true;
}
