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
#include <vector>

using monotonic_clock = std::chrono::_V2::steady_clock;

class ArpTableEntry {
private:
    std::string mac;
    std::string ip;
    monotonic_clock::time_point expire_at;

public:
    ArpTableEntry(std::string _mac, std::string _ip) : mac(_mac), ip(_ip) {
        used();
    }

    bool expired() noexcept {
        return expire_at <= monotonic_clock().now();
    }

    void used() {
        expire_at = monotonic_clock().now() + std::chrono::seconds(60);
    }

    std::string get_mac() noexcept {
        return mac;
    }

    std::string get_ip() noexcept {
        return ip;
    }
};

class ArpTable {
private:
    std::map<std::string, ArpTableEntry> by_ip;
    std::map<std::string, ArpTableEntry> by_mac;
    std::multiset<std::string> not_found;

public:
    ArpTable() {
    }

    std::string getMac(std::string ip) {
        auto elem = by_ip.find(ip);
        if (elem != by_ip.end() && !(*elem).second.expired()) {
            return (*elem).second.get_mac();
        }
        not_found.insert(ip);
        return "ERROR";  //!!!
    }

    auto getNotFoundIps() {
        return not_found;
    }

    void macIsUsed(std::string mac) {
        auto elem = by_mac.find(mac);
        if (elem != by_mac.end()) {
            (*elem).second.used();
        }
    }

    void create(std::string mac, std::string ip) {
        ArpTableEntry entry(mac, ip);
        by_mac[mac] = entry;
        by_ip[ip] = entry;
        auto it = std::find(not_found.begin(), not_found.end(), mac);
        if (it != not_found.end()) {
            not_found.erase(it);
        }
    }
};

class ArpHandler {
private:
    int request_timeout_s = 5;

public:
    ArpTable(UeStorage _ueStorage, ArpTable _table, int dnSock) {
    }
};
