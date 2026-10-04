#pragma once

#include "ue.hpp"

#include <chrono>
#include <map>
#include <set>
#include <string>

using monotonic_clock = std::chrono::_V2::steady_clock;

class ArpTableEntry {
private:
    std::string mac;
    std::string ip;
    monotonic_clock::time_point expire_at;

public:
    ArpTableEntry(std::string _mac, std::string _ip);
    bool expired() const noexcept;
    void used();
    std::string get_mac() const noexcept;
    std::string get_ip() const noexcept;
};

class ArpTable {
private:
    std::map<std::string, ArpTableEntry> by_ip;
    std::map<std::string, ArpTableEntry> by_mac;
    std::multiset<std::string> not_found;

public:
    ArpTable();
    std::string getMac(std::string ip) const;
    auto getNotFoundIps() const;
    void macIsUsed(std::string mac);
    void create(std::string mac, std::string ip);
};

/*class ArpHandler {
private:
    int request_timeout_s = 5;

public:
    ArpTable(UeStorage _ueStorage, ArpTable _table, int dnSock);
};*/
