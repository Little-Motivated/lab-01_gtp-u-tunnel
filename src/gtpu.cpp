#include "gtpu.hpp"

#include <cstring>
#include <vector>

namespace gtpu {

std::vector<std::uint8_t> encodeGtpu(const std::uint8_t *data, std::uint32_t teid,
                                     std::size_t len) {
    std::vector<std::uint8_t> raw(len + 8);
    raw[0] = gtp_first_byte;
    raw[1] = gtp_type;
    std::uint16_t l = htons(static_cast<std::uint16_t>(len));
    std::memcpy(raw.data() + 2, &l, 2);
    std::uint32_t teid_n = htonl(teid);
    std::memcpy(raw.data() + 4, &teid_n, 4);
    std::memcpy(raw.data() + 8, data, len);
    return raw;
}

int decodeGtpu(const std::uint8_t *data, std::size_t len, std::uint32_t &teid,
               std::size_t payload_len) {
    if (len < 8) {
        return -1;
    }
    std::uint16_t tmp16;
    std::memcpy(&tmp16, data + 2, 2);
    std::size_t pl_len = ntohs(tmp16);
    if (pl_len != payload_len) {
        return -1;
    }
    std::uint32_t tmp32;
    std::memcpy(&tmp32, data + 4, 4);
    teid = ntohl(tmp32);
    return 0;
}

}  // namespace gtpu
