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
    ArpTableEntry() = default;
    ArpTableEntry(std::array<std::uint8_t, 6> _mac, std::string _ip);
    bool expired() const noexcept;
    void used() noexcept;
    std::array<std::uint8_t, 6> get_mac() const noexcept;
    std::string get_ip() const noexcept;
};

class ArpTable {
private:
    std::map<std::string, ArpTableEntry> by_ip{};
    std::map<std::array<std::uint8_t, 6>, ArpTableEntry> by_mac{};
    std::set<std::string> not_found_ips{};

public:
    ArpTable() = default;
    std::optional<std::array<std::uint8_t, 6>> getMac(std::string ip);
    void addNotFoudIp(std::string ip);
    std::set<std::string> getNotFoundIps() const noexcept;
    void macUsed(std::array<std::uint8_t, 6> mac);
    void create(std::array<std::uint8_t, 6> mac, std::string ip);
};

}  // namespace arp
