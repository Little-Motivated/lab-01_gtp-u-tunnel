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
const std::string UPF_MAC = "bb:00:00:00:00:01";
const std::string BCAST_MAC = "ff:ff:ff:ff:ff:ff";

const std::string UPF_TO_DN_IP = "192.168.5.2";
const std::string DN_IP = "192.168.5.1";
const std::string UPF_TO_GNB_IP = "192.168.9.1";
const std::string GNB_IP = "192.168.9.2";
const int GTPU_PORT = 2152;

struct EthHeader {
    std::array<std::uint8_t, 6> dst_mac;
    std::array<std::uint8_t, 6> src_mac;
    uint16_t ether_type;
};

int parse_eth(uint8_t *data, std::size_t len, EthHeader *eth) {
    if (len < 14) {
        return -1;
    }
    std::memcpy(eth->dst_mac.data(), data, 6);
    std::memcpy(eth->src_mac.data(), data + 6, 6);
    eth->ether_type = htons(*reinterpret_cast<std::uint16_t *>(data + 12));
    return 0;
}

class Upf {
private:
    asio::io_context &io;
    asio::generic::raw_protocol::socket sock_raw;
    asio::ip::udp::socket sock_udp;
    asio::ip::udp::endpoint gnb_endpoint;
    asio::steady_timer timer;

    arp::ArpTable arp_table;
    ue::UeStorage ue_storage;

    std::array<std::uint8_t, 4096> dn_buf;
    std::array<std::uint8_t, 4096> gnb_buf;

    void receive_raw() {
        sock_raw.async_receive(asio::buffer(dn_buf), [this](boost::system::error_code &error_code,
                                                            std::size_t bytes_received) {
            if (!error_code) {
                process_raw_packet(dn_buf.data(), bytes_received);
            }
            receive_raw();
        });
    }

    void process_raw_packet(std::uint8_t *data, std::size_t n) {
        EthHeader eth;
        if (parse_eth(data, n, &eth) != 0) {
            return;
        }
        arp_table.macIsUsed(eth.src_mac);
        if (eth.ether_type == ETH_P_ARP) {
        }
        if (eth.ether_type == ETH_P_IP) {
        }
    }

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

        asio::ip::udp::endpoint local_endpoint(asio::ip::make_address("192.168.9.1"), GTPU_PORT);
        sock_udp.open(local_endpoint.protocol());
        sock_udp.bind(local_endpoint);
        gnb_endpoint = asio::ip::udp::endpoint(asio::ip::make_address(GNB_IP), GTPU_PORT);

        receive_raw();
    }
};

int main() {
    boost::asio::io_context io;
    boost::asio::generic::raw_protocol::socket sock(io);
    sock.open();

    sockaddr_ll addr = {0};
    addr.sll_family = AF_PACKET;
    addr.sll_protocol = htons(ETH_P_ALL);
    addr.sll_ifindex = if_nametoindex("upf_cn_veth");

    bind(sock.native_handle(), (sockaddr *)&addr, sizeof(addr));
}
