#include "net.hpp"
#include "arp.hpp"
#include "gtpu.hpp"
#include "ue.hpp"

#include <arpa/inet.h>
#include <linux/if_ether.h>

#include <array>
#include <cstring>
#include <optional>
#include <vector>

namespace net {

int parse_eth(uint8_t *data, std::size_t len, EthHeader &eth) {
    if (len < 14) {
        return -1;
    }
    std::memcpy(eth.dst_mac.data(), data, 6);
    std::memcpy(eth.src_mac.data(), data + 6, 6);
    eth.ether_type = ntohs(*reinterpret_cast<std::uint16_t *>(data + 12));
    return 0;
}

int parse_arp(uint8_t *data, std::size_t len, ArpHeader &arp) {
    if (len < 28) {
        return -1;
    }
    arp.hardware_type = ntohs(*reinterpret_cast<uint16_t *>(data));
    arp.protocol_type = ntohs(*reinterpret_cast<uint16_t *>(data + 2));
    arp.hardware_size = data[4];
    arp.protocol_size = data[5];
    arp.opcode = ntohs(*reinterpret_cast<uint16_t *>(data + 6));
    std::memcpy(arp.src_mac.data(), data + 8, 6);
    char buf[INET_ADDRSTRLEN];
    if (inet_ntop(AF_INET, data + 14, buf, sizeof(buf)) == NULL) {
        return -1;
    }
    arp.src_ip = buf;
    std::memcpy(arp.dst_mac.data(), data + 18, 6);
    if (inet_ntop(AF_INET, data + 24, buf, sizeof(buf)) == NULL) {
        return -1;
    }
    arp.dst_ip = buf;
    return 0;
}

int parse_ip(uint8_t *data, std::size_t len, IpHeader &header) {
    if (len < 20) {
        return -1;
    }
    uint8_t header_size = (data[0] & 0b00001111) * 4;
    if (len < header_size) {
        return -1;
    }

    header.data = data + header_size;
    uint16_t data_size;
    std::memcpy(&data_size, data + 2, 2);
    header.data_size = ntohs(data_size);
    header.protocol = data[9];
    char buf[INET_ADDRSTRLEN];
    if (inet_ntop(AF_INET, data + 12, buf, sizeof(buf)) == NULL) {
        return -1;
    }
    header.src_ip = buf;
    if (inet_ntop(AF_INET, data + 16, buf, sizeof(buf)) == NULL) {
        return -1;
    }
    header.dst_ip = buf;
    return 0;
}

void build_eth(EthHeader &eth, const std::array<std::uint8_t, 6> &dst_mac,
               const std::array<std::uint8_t, 6> &src_mac, uint16_t ether_type) {
    std::memcpy(eth.dst_mac.data(), dst_mac.data(), 6);
    std::memcpy(eth.src_mac.data(), src_mac.data(), 6);
    eth.ether_type = ether_type;
}

void build_arp(ArpHeader &arp, uint16_t opcode, const std::array<std::uint8_t, 6> &src_mac,
               const std::string &src_ip, const std::array<std::uint8_t, 6> &dst_mac,
               const std::string &dst_ip) {
    arp.opcode = opcode;
    std::memcpy(arp.src_mac.data(), src_mac.data(), 6);
    arp.src_ip = src_ip;
    std::memcpy(arp.dst_mac.data(), dst_mac.data(), 6);
    arp.dst_ip = dst_ip;
}

std::optional<std::array<std::uint8_t, ETH_ARP_HS>> raw(const EthHeader &eth,
                                                        const ArpHeader &arp) {
    std::array<std::uint8_t, ETH_ARP_HS> frame = {};
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
    if (inet_pton(AF_INET, arp.src_ip.data(), frame.data() + 28) != 1) {
        return std::nullopt;
    }
    std::memcpy(frame.data() + 32, arp.dst_mac.data(), 6);
    if (inet_pton(AF_INET, arp.dst_ip.data(), frame.data() + 38) != 1) {
        return std::nullopt;
    }
    return frame;
}

std::vector<uint8_t> raw(const EthHeader &eth, const uint8_t *payload, std::size_t payload_len) {
    std::vector<uint8_t> raw_data(payload_len + 14);
    std::memcpy(raw_data.data(), eth.dst_mac.data(), 6);
    std::memcpy(raw_data.data() + 6, eth.src_mac.data(), 6);
    uint16_t ether_type = htons(eth.ether_type);
    std::memcpy(raw_data.data() + 12, &ether_type, 2);

    std::memcpy(raw_data.data() + 14, payload, payload_len);
    return raw_data;
}

}  // namespace net
