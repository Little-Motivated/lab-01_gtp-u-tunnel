#include <netinet/in.h>

#include <cstdint>
#include <map>
#include <string>

class Ue {
private:
    std::string ip;
    std::int32_t teid_dl;
    std::int32_t teid_ul;

public:
    Ue(std::string _ip, std::int32_t _teid_dl, std::int32_t _teid_ul)
        : ip(_ip), teid_dl(_teid_dl), teid_ul(_teid_ul) {
    }

    std::string get_ip() const noexcept {
        return ip;
    }

    std::int32_t get_teid_ul() const noexcept {
        return teid_ul;
    }
};

class UeStorage {
private:
    std::map<std::int32_t, Ue> ues_by_teid;
    std::map<std::string, Ue> ues_by_ip;

public:
    UeStorage() {
        for (int i = 1; i < 5; ++i) {
            Ue ue(std::string("192.168.7.") + std::to_string(i), i * 2, i * 2 + 1); // !!!
            ues_by_teid.emplace(ue.get_teid_ul(), Ue(std::string("192.168.7.") + std::to_string(i),
                                                     i * 2, i * 2 + 1));
            ues_by_ip.emplace(ue.get_ip(),
                              Ue(std::string("192.168.7.") + std::to_string(i), i * 2, i * 2 + 1));
        }
    }
}
