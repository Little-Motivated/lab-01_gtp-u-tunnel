#pragma once

#include "arp.hpp"
#include "ue.hpp"

static const std::uint8_t gtp_first_byte = 0b00110000;
static const std::uint8_t gtp_type = 0xff;

namespace gtpu {

std::vector<std::uint8_t> encodeGtpu(const std::uint8_t *data, std::uint32_t teid, std::size_t len);

int decodeGtpu(const std::uint8_t *data, std::size_t len, std::uint32_t &teid,
               std::size_t payload_len);

}  // namespace gtpu
