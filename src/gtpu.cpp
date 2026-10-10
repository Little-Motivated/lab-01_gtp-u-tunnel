#include "gtpu.hpp"

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

namespace gtpu {

std::vector<uint8_t> encodeGtpu(const uint8_t *data, uint32_t teid, std::size_t len) {
    std::vector<uint8_t> raw(len + 8);
    raw[0] = gtp_first_byte;
    raw[1] = gtp_type;
    uint16_t l = htons(static_cast<uint16_t>(len));
    std::memcpy(raw.data() + 2, &l, 2);
    uint32_t teid_n = htonl(teid);
    std::memcpy(raw.data() + 4, &teid_n, 4);
    std::memcpy(raw.data() + 8, data, len);
    return raw;
}

}  // namespace gtpu
