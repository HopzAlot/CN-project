#ifndef SOCKET_COMPAT_H
#define SOCKET_COMPAT_H

#include <cstdint>
#include <string>

// The only place that knows about Winsock vs BSD sockets.
// Everyone else uses these functions and never includes a system socket header.

// big enough for a Windows SOCKET (UINT_PTR) and a POSIX int
typedef std::intptr_t sock_t;
#define INVALID_SOCK ((sock_t)-1)

// Call once at program start / end. WSAStartup on Windows, nothing on Linux.
bool net_init();
void net_cleanup();

// UDP socket bound to 0.0.0.0:port. port 0 lets the OS pick one.
bool udp_open(int port, sock_t& sock);

// Allow sending to 255.255.255.255 (only needed for LAN discovery).
bool udp_enable_broadcast(sock_t sock);

// Port the socket is actually bound to (useful after udp_open(0, ...)).
int udp_local_port(sock_t sock);

bool udp_send(sock_t sock, const std::string& ip, int port, const uint8_t* buf, int len);

// Waits up to timeout_ms. Returns bytes read, 0 on timeout, -1 on error.
// ip and port are the sender.
int udp_recv(sock_t sock, uint8_t* buf, int cap, std::string& ip, int& port, int timeout_ms);

void udp_close(sock_t sock);

// Splits "1.2.3.4:5000". Returns false if the format is wrong.
bool parse_addr(const std::string& text, std::string& ip, int& port);

#endif
