#pragma once

#include <netinet/in.h>

#include <cstdint>
#include <map>
#include <string>

namespace ue {

class Ue {
private:
    std::string ip;
    std::int32_t teid_dl;
    std::int32_t teid_ul;

public:
    Ue(std::string _ip, std::int32_t _teid_dl, std::int32_t _teid_ul);
    std::string get_ip() const noexcept;
    std::int32_t get_teid_ul() const noexcept;
};

class UeStorage {
private:
    std::map<std::int32_t, Ue> ues_by_teid;
    std::map<std::string, Ue> ues_by_ip;

public:
    UeStorage();
    Ue *getByTeid(std::int32_t teid);
    Ue *getByIp(std::string ip);
};
}  // namespace ue
