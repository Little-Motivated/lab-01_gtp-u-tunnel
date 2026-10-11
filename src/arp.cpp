#include "arp.hpp"
#include "ue.hpp"

#include <arpa/inet.h>
#include <linux/if_ether.h>
#include <linux/if_packet.h>
#include <net/if.h>
#include <stdio.h>
#include <sys/socket.h>
#include <unistd.h>

#include <algorithm>
#include <chrono>
#include <iostream>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <thread>
#include <vector>

namespace arp {

using monotonic_clock = std::chrono::steady_clock;

ArpTableEntry::ArpTableEntry(std::array<std::uint8_t, 6> _mac, std::string _ip)
    : mac(_mac), ip(_ip) {
    used();
}

bool ArpTableEntry::expired() const noexcept {
    return expire_at <= monotonic_clock().now();
}

void ArpTableEntry::used() noexcept {
    expire_at = monotonic_clock().now() + std::chrono::seconds(60);
}

std::array<std::uint8_t, 6> ArpTableEntry::get_mac() const noexcept {
    return mac;
}

std::string ArpTableEntry::get_ip() const noexcept {
    return ip;
}

std::optional<std::array<std::uint8_t, 6>> ArpTable::getMac(std::string ip) {
    auto elem = by_ip.find(ip);
    if (elem != by_ip.end() && !(*elem).second.expired()) {
        return (*elem).second.get_mac();
    }
    not_found_ips.insert(ip);
    return std::nullopt;
}

void ArpTable::addNotFoudIp(std::string ip) {
    not_found_ips.insert(ip);
}

std::set<std::string> ArpTable::getNotFoundIps() const noexcept {
    return not_found_ips;
}

void ArpTable::macUsed(std::array<std::uint8_t, 6> mac) {
    auto elem = by_mac.find(mac);
    if (elem != by_mac.end()) {
        std::cout << "                  Find by mac\n";
        elem->second.used();
    }
}

void ArpTable::create(std::array<std::uint8_t, 6> mac, std::string ip) {
    ArpTableEntry entry(mac, ip);
    by_mac[mac] = entry;
    by_ip[ip] = entry;
    auto it = not_found_ips.find(ip);
    if (it != not_found_ips.end()) {
        std::cout << "     Found ip " << *it << "\n";
        not_found_ips.erase(it);
    }
}

}  // namespace arp*/
