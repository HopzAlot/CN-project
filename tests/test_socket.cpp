#include <string>

#include "check.h"
#include "common/socket_compat.h"

int main() {
    CHECK(net_init());

    sock_t a, b;
    CHECK(udp_open(0, a));
    CHECK(udp_open(0, b));
    int port_b = udp_local_port(b);
    CHECK(port_b > 0);

    const uint8_t msg[] = "hello";
    CHECK(udp_send(a, "127.0.0.1", port_b, msg, 5));

    uint8_t buf[64];
    std::string ip;
    int port = 0;
    int n = udp_recv(b, buf, sizeof(buf), ip, port, 1000);
    CHECK(n == 5);
    CHECK(std::string((char*)buf, 5) == "hello");
    CHECK(ip == "127.0.0.1");
    CHECK(port == udp_local_port(a));

    // nothing waiting -> timeout returns 0
    CHECK(udp_recv(b, buf, sizeof(buf), ip, port, 50) == 0);
    CHECK(!udp_send(a, "not-an-ip", port_b, msg, 5));

    std::string pip;
    int pport;
    CHECK(parse_addr("192.168.1.5:5001", pip, pport) && pip == "192.168.1.5" && pport == 5001);
    CHECK(!parse_addr("192.168.1.5", pip, pport));
    CHECK(!parse_addr("1.2.3.4:99999", pip, pport));
    CHECK(!parse_addr("1.2.3.4:50x", pip, pport));

    udp_close(a);
    udp_close(b);
    net_cleanup();
    return test_result("socket");
}
