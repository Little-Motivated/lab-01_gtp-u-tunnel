#include "arp.hpp"
#include "ue.hpp"

#include <cstring>
#include <string>

static const uint8_t gtp_first_byte = 0b00110000;
static const uint8_t gtp_type = 0xff;

namespace gtpu {

std::vector<uint8_t> encodeGtpu(const uint8_t *data, uint32_t teid, std::size_t len);

}  // namespace gtpu
