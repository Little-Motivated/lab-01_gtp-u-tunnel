#include <arpa/inet.h>
#include <linux/if_ether.h>
#include <linux/if_packet.h>
#include <net/if.h>
#include <stdio.h>
#include <sys/socket.h>
#include <unistd.h>

#include <boost/asio.hpp>
#include <chrono>
#include <string>

#include "arp.hpp"
#include "ue.hpp"

namespace asio = boost::asio;

const std::string NEXT_HOP_IP = "192.168.5.1";
const std::string GNB_IP = "192.168.9.2";
const std::string UPF_IP = "192.168.5.2";
const std::string UPF_MAC = "bb:00:00:00:00:01";
const std::string BCAST_MAC = "ff:ff:ff:ff:ff:ff";

class Upf {
private:
    asio::io_context &io;
    asio::generic::raw_protocol::socket sock_raw;
    asio::ip::udp::socket sock_udp;
    asio::ip::udp::endpoint endpoint;
    asio::steady_timer timer;

    arp::ArpTable arp_table;
    ue::UeStorage ue_storage;

public:
    Upf(asio::io_context &_io) : io(_io), sock_raw(_io), sock_udp(_io), timer(_io) {}

    void start() {
        asio::generic::raw_protocol raw_protocol(AF_PACKET, SOCK_RAW);
        sock_raw.open(raw_protocol);
        sockaddr_ll addr = {0};
        addr.sll_family = AF_PACKET;
        addr.sll_protocol = htons(ETH_P_ALL);
        addr.sll_ifindex = if_nametoindex("upf_cn_veth");  // maybe error

        sock_raw.bind(asio::generic::basic_endpoint<asio::generic::raw_protocol>(
            reinterpret_cast<sockaddr *>(&addr), sizeof(addr)));
    }
};

int main() {
    Socket sock_dn(AF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
    sock_dn.bind(AF_PACKET, htons(ETH_P_ALL), if_nametoindex("upf_cn_veth"));

    boost::asio::io_context io;
    boost::asio::generic::raw_protocol::socket sock(io);
    sock.open();

    sockaddr_ll addr = {0};
    addr.sll_family = AF_PACKET;
    addr.sll_protocol = htons(ETH_P_ALL);
    addr.sll_ifindex = if_nametoindex("upf_cn_veth");

    bind(sock.native_handle(), (sockaddr *)&addr, sizeof(addr));
}
