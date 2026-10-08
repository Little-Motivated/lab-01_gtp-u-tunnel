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
#include <map>
#include <set>
#include <string>
#include <thread>
#include <vector>

namespace arp {

using monotonic_clock = std::chrono::_V2::steady_clock;

ArpTableEntry::ArpTableEntry(std::string _mac, std::string _ip) : mac(_mac), ip(_ip) {
    used();
}

bool ArpTableEntry::expired() const noexcept {
    return expire_at <= monotonic_clock().now();
}

void ArpTableEntry::used() {
    expire_at = monotonic_clock().now() + std::chrono::seconds(60);
}

std::string ArpTableEntry::get_mac() const noexcept {
    return mac;
}

std::string ArpTableEntry::get_ip() const noexcept {
    return ip;
}

ArpTable::ArpTable() {
}

std::string ArpTable::getMac(std::string ip) {
    auto elem = by_ip.find(ip);
    if (elem != by_ip.end() && !(*elem).second.expired()) {
        return (*elem).second.get_mac();
    }
    not_found.insert(ip);
    return "ERROR";  //!!!
}

auto ArpTable::getNotFoundIps() const {
    return not_found;
}

void ArpTable::macIsUsed(std::string mac) {
    auto elem = by_mac.find(mac);
    if (elem != by_mac.end()) {
        (*elem).second.used();
    }
}

void ArpTable::create(std::string mac, std::string ip) {
    ArpTableEntry entry(mac, ip);
    by_mac[mac] = entry;
    by_ip[ip] = entry;
    auto it = std::find(not_found.begin(), not_found.end(), mac);
    if (it != not_found.end()) {
        not_found.erase(it);
    }
}

ArpHandler::ArpHandler(UeStorage _ue_storage, ArpTable _arp_table, /*??? dnSock*/)
    : ue_storage(_ue_storage), arp_table(_arp_table) {
    /*update_loop = ???*/
}

void ArpHandler::run() {
    while (true) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        tick();
    }
}

void ArpHandler::tick() {
}
}  // namespace arp
