#pragma once

#include "ue.hpp"

#include <chrono>
#include <map>
#include <set>
#include <string>

namespace arp {

using monotonic_clock = std::chrono::steady_clock;

class ArpTableEntry {
private:
    std::array<std::uint8_t, 6> mac;
    std::string ip;
    monotonic_clock::time_point expire_at;

public:
    ArpTableEntry(std::array<std::uint8_t, 6> _mac, std::string _ip);
    bool expired() const noexcept;
    void used();
    std::array<std::uint8_t, 6> get_mac() const noexcept;
    std::string get_ip() const noexcept;
};

class ArpTable {
private:
    std::map<std::string, ArpTableEntry> by_ip;
    std::map<std::array<std::uint8_t, 6>, ArpTableEntry> by_mac;
    std::multiset<std::string> not_found;

public:
    ArpTable();
    std::array<std::uint8_t, 6> getMac(std::string ip);
    auto getNotFoundIps() const;
    void macIsUsed(std::array<std::uint8_t, 6> mac);
    void create(std::array<std::uint8_t, 6> mac, std::string ip);
};

class ArpHandler {
private:
    int request_timeout_s = 5;
    ue::UeStorage ue_storage;
    //??? dn_sock;
    ArpTable arp_table;
    std::vector<int> pending_requests;
    //??? update_loop;

public:
    ArpHandler(ue::UeStorage _ueStorage, ArpTable _table /*, *??? dnSock*/);
    void run();  // async
    void tick();
    void stop();
    void onPacketFromDn();
    void handleArp();
};
}  // namespace arp
