#pragma once

#include "arp.hpp"
#include "ue.hpp"

#include <boost/asio.hpp>

namespace asio = boost::asio;

namespace upf {

class Upf {
private:
    asio::io_context &io;
    asio::generic::raw_protocol::socket sock_raw;
    asio::ip::udp::socket sock_udp;
    asio::ip::udp::endpoint gnb_endpoint;
    asio::steady_timer arp_timer;

    arp::ArpTable arp_table;
    ue::UeStorage ue_storage;

    std::array<std::uint8_t, 4096> dn_buf;
    std::array<std::uint8_t, 4096> gnb_buf;

    void receiveRaw();
    void processRawPacket(std::uint8_t *data, std::size_t n);
    void receiveUdp();
    void processUdpPacket(std::uint8_t *data, std::size_t bytes_received);
    void arpTick();
    void sendArp();

public:
    Upf(asio::io_context &_io);
    void start();
};

}  // namespace upf
