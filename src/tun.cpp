#include <arpa/inet.h>
#include <linux/if_ether.h>
#include <linux/if_packet.h>
#include <net/if.h>
#include <stdio.h>
#include <sys/socket.h>
#include <unistd.h>

#include <boost/asio.hpp>
#include <chrono>
#include <iostream>
#include <string>

#include "arp.hpp"
#include "gtpu.hpp"
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
    std::array<std::uint8_t, 6> dst_mac{};
    std::array<std::uint8_t, 6> src_mac{};
    uint16_t ether_type = 0;
};

int parse_eth(uint8_t *data, std::size_t len, EthHeader *eth) {
    if (len < 14) {
        return -1;
    }
    std::memcpy(eth->dst_mac.data(), data, 6);
    std::memcpy(eth->src_mac.data(), data + 6, 6);
    eth->ether_type = ntohs(*reinterpret_cast<std::uint16_t *>(data + 12));
    return 0;
}

struct ArpHeader {
    uint16_t hardware_type = 0;
    uint16_t protocol_type = 0;
    uint8_t hardware_size = 0;
    uint8_t protocol_size = 0;
    uint16_t opcode = 0;
    std::array<std::uint8_t, 6> src_mac{};
    std::string src_ip;
    std::array<std::uint8_t, 6> dst_mac{};
    std::string dst_ip;
};

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
    if (inet_ntop(AF_INET, data + 14, buf, sizeof(buf)) == NULL) {
        return -1;
    }
    arp->src_ip = buf;
    std::memcpy(arp->dst_mac.data(), data + 18, 6);
    if (inet_ntop(AF_INET, data + 24, buf, sizeof(buf)) == NULL) {
        return -1;
    }
    arp->dst_ip = buf;
    return 0;
}

struct IpHeader {
    uint16_t data_size = 0;
    uint8_t protocol = 0;
    std::string src_ip;
    std::string dst_ip;
    uint8_t *data = nullptr;
};

int parse_ip(uint8_t *data, std::size_t len, IpHeader *header) {
    if (len < 20) {
        return -1;
    }
    uint8_t header_size = (data[0] & 0b00001111) * 4;
    if (len < header_size) {
        return -1;
    }

    header->data = data + header_size;
    uint16_t data_size;
    std::memcpy(&data_size, data + 2, 2);
    header->data_size = ntohs(data_size);
    header->protocol = data[9];
    char buf[INET_ADDRSTRLEN];
    if (inet_ntop(AF_INET, data + 12, buf, sizeof(buf)) == NULL) {
        return -1;
    }
    header->src_ip = buf;
    if (inet_ntop(AF_INET, data + 16, buf, sizeof(buf)) == NULL) {
        return -1;
    }
    header->dst_ip = buf;
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
    arp->opcode = opcode;
    std::memcpy(arp->src_mac.data(), src_mac.data(), 6);
    arp->src_ip = src_ip;
    std::memcpy(arp->dst_mac.data(), dst_mac.data(), 6);
    arp->dst_ip = dst_ip;
}

auto raw(const EthHeader &eth, const ArpHeader &arp) {
    std::array<std::uint8_t, ETH_HEADER_SIZE + ARP_HEADER_SIZE> frame = {};
    std::memcpy(frame.data(), eth.dst_mac.data(), 6);
    std::memcpy(frame.data() + 6, eth.src_mac.data(), 6);
    uint16_t ether_type = htons(eth.ether_type);
    std::memcpy(frame.data() + 12, &ether_type, 2);

    uint16_t ht = htons(arp.hardware_type);
    uint16_t pt = htons(arp.protocol_type);
    std::memcpy(frame.data() + 14, &ht, 2);
    std::memcpy(frame.data() + 16, &pt, 2);
    frame[18] = arp.hardware_size;
    frame[19] = arp.protocol_size;
    uint16_t opcode = htons(arp.opcode);
    std::memcpy(frame.data() + 20, &opcode, 2);

    std::memcpy(frame.data() + 22, arp.src_mac.data(), 6);
    inet_pton(AF_INET, arp.src_ip.data(), frame.data() + 28);  // maybe error
    std::memcpy(frame.data() + 32, arp.dst_mac.data(), 6);
    inet_pton(AF_INET, arp.dst_ip.data(), frame.data() + 38);  // maybe error
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
        sock_raw.async_receive(
            asio::buffer(dn_buf),
            [this](const boost::system::error_code &error_code, std::size_t bytes_received) {
                if (!error_code) {
                    std::cout << "Get " << bytes_received << " bytes from DN\n";
                    processRawPacket(dn_buf.data(), bytes_received);
                }
                receiveRaw();
            });
    }

    void processRawPacket(std::uint8_t *data, std::size_t n) {
        std::cout << "processRawPacket: Enter\n";
        EthHeader eth;
        if (parse_eth(data, n, &eth) != 0) {
            std::cout << "processRawPacket: Invalid ethernet header\n";
            return;
        }

        if (eth.src_mac == UPF_MAC) {
            std::cout << "processRawPacket: Get UPF's packet\n";
            return;
        }

        std::uint8_t *payload = data + 14;
        std::size_t payload_len = n - 14;
        arp_table.macIsUsed(eth.src_mac);
        std::cout << "Check 1\n";
        if (eth.ether_type == ETH_P_ARP) {
            std::cout << "Check 2\n";
            ArpHeader arp;
            if (parse_arp(payload, payload_len, &arp) != 0) {
                std::cout << "processRawPacket: Invalid ARP header\n";
                // throw
                return;
            }
            if (arp.opcode == 1) {
                std::cout << "Check 2.1\n";
                if (ue_storage.getByIp(arp.dst_ip) == nullptr) {
                    std::cout << "processRawPacket: ARP-request. There is no ue with this ip\n";
                    return;
                }
                ArpHeader arp_reply = arp;
                EthHeader eth_send = eth;
                build_arp(&arp_reply, 2, UPF_MAC, arp.dst_ip, arp.src_mac, arp.src_ip);
                build_eth(&eth_send, UPF_MAC, eth.src_mac);
                auto frame = raw(eth_send, arp_reply);
                sock_raw.send(asio::buffer(frame));  // async

            } else if (arp.opcode == 2) {
                std::cout << "Check 2.2\n";
                arp_table.create(arp.src_mac, arp.src_ip);
            }
            std::cout << "Check 2.3\n";
        }
        if (eth.ether_type == ETH_P_IP) {
            std::cout << "Check 3\n";
            IpHeader ip;
            if (parse_ip(payload, payload_len, &ip) != 0) {
                std::cout << "processRawPacket: IP. Invalid ip header\n";
                return;
            }

            ue::Ue *ue = ue_storage.getByIp(ip.dst_ip);
            std::cout << ue->get_ip() << "\n";
            if (ue == nullptr) {
                std::cout << "processRawPacket: IP. There is no ue with this ip\n";
                return;
            }
            auto raw_data = gtpu::encodeGtpu(payload, ue->get_teid_dl(), payload_len);
            sock_udp.send_to(asio::buffer(raw_data), gnb_endpoint);  // async
        }
        std::cout << "Check 4\n";
    }

public:
    Upf(asio::io_context &_io) : io(_io), sock_raw(_io), sock_udp(_io), timer(_io) {}

    void start() {
        asio::generic::raw_protocol raw_protocol(AF_PACKET, SOCK_RAW);
        sock_raw.open(raw_protocol);
        sockaddr_ll addr = {};
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
    asio::io_context io;
    Upf upf(io);
    upf.start();
    io.run();
}
