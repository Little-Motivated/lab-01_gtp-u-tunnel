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

const uint8_t ETH_HEADER_SIZE = 14;
const uint8_t ARP_HEADER_SIZE = 28;

const std::string NEXT_HOP_IP = "192.168.5.1";
const std::array<std::uint8_t, 6> UPF_MAC = {0xbb, 0x00, 0x00, 0x00, 0x00, 0x01};
const std::array<std::uint8_t, 6> BCAST_MAC = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};

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

struct ArpHeader {
    uint16_t hardware_type;
    uint16_t protocol_type;
    uint8_t hardware_size;
    uint8_t protocol_size;
    uint16_t opcode;
    std::array<std::uint8_t, 6> src_mac;
    std::string src_ip;
    std::array<std::uint8_t, 6> dst_mac;
    std::string dst_ip;
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

int parse_arp(uint8_t *data, std::size_t len, ArpHeader *arp) {
    if (len < 28) {
        return -1;
    }
    arp->hardware_type = ntohs(*reinterpret_cast<uint16_t *>(data));
    arp->protocol_type = ntohs(*reinterpret_cast<uint16_t *>(data + 2));
    arp->hardware_size = data[4];
    arp->protocol_size = data[5];
    arp->opcode = ntohs(*reinterpret_cast<uint16_t *>(data + 6));
    std::memcpy(arp->src_mac.data(), data + 8, 6);
    char buf[INET_ADDRSTRLEN];
    if (inet_ntop(AF_PACKET, data + 14, buf, sizeof(buf)) == NULL) {  // maybe error
        return -1;
    }
    arp->src_ip = buf;
    std::memcpy(arp->dst_mac.data(), data + 18, 6);
    if (inet_ntop(AF_PACKET, data + 24, buf, sizeof(buf)) == NULL) {  // maybe error
        return -1;
    }
    arp->dst_ip = buf;
    return 0;
}

void build_eth(EthHeader *eth, const std::array<std::uint8_t, 6> &src_mac,
               const std::array<std::uint8_t, 6> &dst_mac) {
    std::memcpy(eth->src_mac.data(), src_mac.data(), 6);
    std::memcpy(eth->dst_mac.data(), dst_mac.data(), 6);
}

void build_arp(ArpHeader *arp, uint16_t opcode, const std::array<std::uint8_t, 6> &src_mac,
               const std::string &src_ip, const std::array<std::uint8_t, 6> &dst_mac,
               const std::string &dst_ip) {
    arp->opcode = htons(opcode);
    std::memcpy(arp->src_mac.data(), src_mac.data(), 6);
    arp->src_ip = src_ip;
    std::memcpy(arp->dst_mac.data(), dst_mac.data(), 6);
    arp->dst_ip = dst_ip;
}

auto raw(const EthHeader &eth, const ArpHeader &arp) {
    std::array<std::uint8_t, ETH_HEADER_SIZE + ARP_HEADER_SIZE> frame;
    std::memcpy(frame.data(), eth.dst_mac.data(), 6);
    std::memcpy(frame.data() + 6, eth.src_mac.data(), 6);
    frame[12] = eth.ether_type;

    std::memcpy(frame.data() + 14, &arp.hardware_type, 2);
    std::memcpy(frame.data() + 16, &arp.protocol_type, 2);
    frame[18] = arp.hardware_size;
    frame[19] = arp.protocol_size;
    uint16_t opcode = htons(arp.opcode);
    std::memcpy(frame.data() + 20, &opcode, 2);

    std::memcpy(frame.data() + 22, arp.src_mac.data(), 6);
    uint32_t ip;
    inet_pton(AF_PACKET, arp.src_ip.data(), &ip);
    std::memcpy(frame.data() + 28, &ip, 4);
    std::memcpy(frame.data() + 32, arp.dst_mac.data(), 6);
    inet_pton(AF_PACKET, arp.dst_ip.data(), &ip);
    std::memcpy(frame.data() + 38, &ip, 4);
    return frame;
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

    void receiveRaw() {
        sock_raw.async_receive(asio::buffer(dn_buf), [this](boost::system::error_code &error_code,
                                                            std::size_t bytes_received) {
            if (!error_code) {
                processRawPacket(dn_buf.data(), bytes_received);
            }
            receiveRaw();
        });
    }

    void processRawPacket(std::uint8_t *data, std::size_t n) {
        EthHeader eth;
        if (parse_eth(data, n, &eth) != 0) {
            return;
        }
        arp_table.macIsUsed(eth.src_mac);
        if (eth.ether_type == ETH_P_ARP) {
            ArpHeader arp;
            if (parse_arp(data + 14, n - 14, &arp) != 0) {
                // throw
                return;
            }
            if (arp.opcode == 1) {
                if (ue_storage.getByIp(arp.dst_ip) == nullptr) {
                    return;
                }
                ArpHeader arp_reply = arp;
                EthHeader eth = eth;
                build_arp(&arp_reply, 2, UPF_MAC, arp.dst_ip, arp.src_mac, arp.src_ip);
                build_eth(&eth, UPF_MAC, eth.src_mac);
                auto frame = raw(eth, arp);
                sock_raw.send(asio::buffer(frame));

            } else if (arp.opcode == 2) {
            }
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

        receiveRaw();
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
