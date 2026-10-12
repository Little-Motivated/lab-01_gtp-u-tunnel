#pragma once

#include "arp.hpp"
#include "gtpu.hpp"
#include "ue.hpp"

#include <linux/if_ether.h>
#include <linux/if_packet.h>

#include <cstring>

const uint8_t ETH_HEADER_SIZE = 14;
const uint8_t ARP_HEADER_SIZE = 28;
const uint8_t ETH_ARP_HS = ETH_HEADER_SIZE + ARP_HEADER_SIZE;

const std::array<std::uint8_t, 6> UPF_MAC = {0xbb, 0x00, 0x00, 0x00, 0x00, 0x01};
const std::array<std::uint8_t, 6> BCAST_MAC = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};

const std::string UPF_TO_DN_IP = "192.168.5.2";
const std::string DN_IP = "192.168.5.1";
const std::string UPF_TO_GNB_IP = "192.168.9.1";
const std::string GNB_IP = "192.168.9.2";
const int GTPU_PORT = 2152;

namespace net {

struct EthHeader {
    std::array<std::uint8_t, 6> dst_mac{};
    std::array<std::uint8_t, 6> src_mac{};
    uint16_t ether_type = 0;
};

struct ArpHeader {
    uint16_t hardware_type = 1;
    uint16_t protocol_type = ETH_P_IP;
    uint8_t hardware_size = 6;
    uint8_t protocol_size = 4;
    uint16_t opcode = 0;
    std::array<std::uint8_t, 6> src_mac{};
    std::string src_ip;
    std::array<std::uint8_t, 6> dst_mac{};
    std::string dst_ip;
};

struct IpHeader {
    uint16_t data_size = 0;
    uint8_t protocol = 0;
    std::string src_ip;
    std::string dst_ip;
    uint8_t *data = nullptr;
};

int parse_eth(uint8_t *data, std::size_t len, EthHeader &eth);
int parse_arp(uint8_t *data, std::size_t len, ArpHeader &arp);
int parse_ip(uint8_t *data, std::size_t len, IpHeader &header);

void build_eth(EthHeader &eth, const std::array<std::uint8_t, 6> &dst_mac,
               const std::array<std::uint8_t, 6> &src_mac, uint16_t ether_type);
void build_arp(ArpHeader &arp, uint16_t opcode, const std::array<std::uint8_t, 6> &src_mac,
               const std::string &src_ip, const std::array<std::uint8_t, 6> &dst_mac,
               const std::string &dst_ip);
std::optional<std::array<std::uint8_t, ETH_ARP_HS>> raw(const EthHeader &eth, const ArpHeader &arp);
std::vector<uint8_t> raw(const EthHeader &eth, const uint8_t *payload, std::size_t payload_len);

}  // namespace net
