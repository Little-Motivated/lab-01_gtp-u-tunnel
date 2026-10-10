#include "ue.hpp"

#include <netinet/in.h>

#include <cstdint>
#include <map>
#include <string>

namespace ue {

Ue::Ue(std::string _ip, std::int32_t _teid_dl, std::int32_t _teid_ul)
    : ip(_ip), teid_dl(_teid_dl), teid_ul(_teid_ul) {}

std::string Ue::get_ip() const noexcept {
    return ip;
}

std::int32_t Ue::get_teid_dl() const noexcept {
    return teid_dl;
}

UeStorage::UeStorage() {
    for (int i = 1; i < 5; ++i) {
        Ue ue(std::string("192.168.7.") + std::to_string(i), i * 2, i * 2 + 1);  // !!!
        ues_by_teid.emplace(ue.get_teid_dl(),
                            Ue(std::string("192.168.7.") + std::to_string(i), i * 2, i * 2 + 1));
        ues_by_ip.emplace(ue.get_ip(),
                          Ue(std::string("192.168.7.") + std::to_string(i), i * 2, i * 2 + 1));
    }
}

Ue *UeStorage::getByTeid(std::int32_t teid) {
    auto res = ues_by_teid.find(teid);
    return (res == ues_by_teid.end()) ? nullptr : &res->second;
}

Ue *UeStorage::getByIp(std::string ip) {
    auto res = ues_by_ip.find(ip);
    return (res == ues_by_ip.end()) ? nullptr : &res->second;
}
}  // namespace ue
