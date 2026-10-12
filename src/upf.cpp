#include "upf.hpp"
#include "arp.hpp"
#include "gtpu.hpp"
#include "net.hpp"
#include "ue.hpp"

#include <boost/asio.hpp>
#include <iostream>

namespace asio = boost::asio;

namespace upf {

void Upf::receiveRaw() {
    sock_raw.async_receive(asio::buffer(dn_buf), [this](const boost::system::error_code &error_code,
                                                        std::size_t bytes_received) {
        if (!error_code) {
            std::cout << "Get " << bytes_received << " bytes from DN\n";
            processRawPacket(dn_buf.data(), bytes_received);
        }
        receiveRaw();
    });
}

void Upf::processRawPacket(std::uint8_t *data, std::size_t n) {
    net::EthHeader eth;
    if (net::parse_eth(data, n, eth) != 0) {
        std::cout << "processRawPacket: Invalid ethernet header\n";
        return;
    }

    if (eth.src_mac == UPF_MAC) {
        std::cout << "processRawPacket: Get UPF's packet. Cancel\n";
        return;
    }

    std::uint8_t *payload = data + 14;
    std::size_t payload_len = n - 14;
    arp_table.macUsed(eth.src_mac);
    if (eth.ether_type == ETH_P_ARP) {
        std::cout << "processRawPacket: Get ARP packet from DN\n";
        net::ArpHeader arp;
        if (net::parse_arp(payload, payload_len, arp) != 0) {
            std::cout << "processRawPacket: Invalid ARP header\n";
            return;
        }

        if (arp.opcode == 1) {
            std::cout << "processRawPacket: Get ARP request from DN\n";
            if (ue_storage.getByIp(arp.dst_ip) == nullptr) {
                std::cout << "processRawPacket: ARP request. There is no UE with IP: " << arp.dst_ip
                          << "\n";
                return;
            }
            net::ArpHeader arp_reply = arp;
            net::EthHeader eth_send = eth;
            net::build_arp(arp_reply, 2, UPF_MAC, arp.dst_ip, arp.src_mac, arp.src_ip);
            net::build_eth(eth_send, eth.src_mac, UPF_MAC, eth.ether_type);

            auto frame = raw(eth_send, arp_reply);
            if (!frame.has_value()) {
                std::cout << "processRawPacket: Failed to convert IP from string to binary form: "
                          << arp.src_ip << " or " << arp.dst_ip << "\n";
                return;
            }

            auto frame_sh = std::make_shared<std::array<std::uint8_t, 42UL>>(frame.value());
            sock_raw.async_send(
                asio::buffer(*frame_sh),
                [frame_sh](const boost::system::error_code &error_code, std::size_t) {
                    if (!error_code) {
                        std::cout << "processRawPacket: Reply to ARP to DN\n";
                    }
                });

        } else if (arp.opcode == 2) {
            std::cout << "processRawPacket: Get ARP reply from DN, IP: " << arp.src_ip << "\n";
            arp_table.create(arp.src_mac, arp.src_ip);
        }
    }
    if (eth.ether_type == ETH_P_IP) {
        std::cout << "processRawPacket: Get IP packet from DN\n";
        net::IpHeader ip;
        if (net::parse_ip(payload, payload_len, ip) != 0) {
            std::cout << "processRawPacket: Invalid IP header\n";
            return;
        }

        const ue::Ue *ue = ue_storage.getByIp(ip.dst_ip);
        if (ue == nullptr) {
            std::cout << "processRawPacket: There is no UE with this IP: " << ip.dst_ip << "\n";
            return;
        }
        auto raw_data_sh = std::make_shared<std::vector<std::uint8_t>>(
            gtpu::encodeGtpu(payload, ue->get_teid_dl(), payload_len));

        sock_udp.async_send_to(
            asio::buffer(*raw_data_sh), gnb_endpoint,
            [raw_data_sh](const boost::system::error_code &error_code, std::size_t) {
                if (error_code) {
                    std::cout << error_code.message() << "\n";
                    return;
                }
                std::cout << "processRawPacket: Send GTPU to gNB\n";
            });
    }
}

void Upf::receiveUdp() {
    sock_udp.async_receive(
        asio::buffer(gnb_buf),
        [this](const boost::system::error_code &error_code, std::size_t bytes_received) {
            if (!error_code) {
                std::cout << "Get " << bytes_received << " bytes from gNB\n";
                processUdpPacket(gnb_buf.data(), bytes_received);
            }
            receiveUdp();
        });
}

void Upf::processUdpPacket(std::uint8_t *data, std::size_t bytes_received) {
    std::uint8_t *payload = data + 8;
    std::size_t payload_len = bytes_received - 8;
    std::uint32_t teid;
    if (gtpu::decodeGtpu(data, bytes_received, teid, payload_len) != 0) {
        std::cout << "processUdpPacket: Invalid GTPU header\n";
        return;
    }

    const ue::Ue *ue = ue_storage.getByTeidUl(teid);
    if (ue == nullptr) {
        std::cout << "processUdpPacket: There is no UE with teid_ul " << teid << "\n";
        return;
    }
    auto mac = arp_table.getMac(DN_IP);
    if (!mac.has_value()) {
        std::cout << "processUdpPacket: There is no ip " << DN_IP << " in ARP-table"
                  << "\n";
        arp_table.addNotFoudIp(DN_IP);
        return;
    }
    net::EthHeader eth;
    net::build_eth(eth, mac.value(), UPF_MAC, ETH_P_IP);
    auto raw_data_sh = std::make_shared<std::vector<std::uint8_t>>(raw(eth, payload, payload_len));
    sock_raw.async_send(asio::buffer(*raw_data_sh),
                        [raw_data_sh](const boost::system::error_code &error_code, std::size_t) {
                            if (!error_code) {
                                std::cout << "processUdpPacket: Send IP to DN\n";
                            }
                        });
}

void Upf::arpTick() {
    arp_timer.expires_after(std::chrono::seconds(1));
    arp_timer.async_wait([this](const boost::system::error_code &error_code) {
        if (!error_code) {
            arpTick();
            sendArp();
        }
    });
}

void Upf::sendArp() {
    std::set<std::string> not_found_ips = arp_table.getNotFoundIps();
    for (auto ip : not_found_ips) {
        net::ArpHeader arp;
        net::EthHeader eth;
        net::build_arp(arp, 1, UPF_MAC, UPF_TO_DN_IP, BCAST_MAC, ip);
        net::build_eth(eth, BCAST_MAC, UPF_MAC, ETH_P_ARP);

        auto frame = raw(eth, arp);
        if (!frame.has_value()) {
            std::cout << "processRawPacket: Failed to convert IP from string to binary form: "
                      << arp.src_ip << " or " << arp.dst_ip << "\n";
            return;
        }

        auto frame_shared = std::make_shared<std::array<std::uint8_t, 42UL>>(frame.value());
        sock_raw.async_send(
            asio::buffer(*frame_shared),
            [this, frame_shared, ip](const boost::system::error_code &error_code, std::size_t) {
                if (!error_code) {
                    std::cout << "sendArp: Send ARP to IP " << ip << "\n";
                }
            });
    }
}

Upf::Upf(asio::io_context &_io) : io(_io), sock_raw(_io), sock_udp(_io), arp_timer(_io) {}

void Upf::start() {
    asio::generic::raw_protocol raw_protocol(AF_PACKET, SOCK_RAW);
    sock_raw.open(raw_protocol);
    sockaddr_ll addr = {};
    addr.sll_family = AF_PACKET;
    addr.sll_protocol = htons(ETH_P_ALL);
    addr.sll_ifindex = if_nametoindex("upf_cn_veth");
    if (addr.sll_ifindex == 0) {
        std::cout << "Start: Incorrect interface name: " << "upf_cn_veth" << "\n";
        return;
    }
    sock_raw.bind(asio::generic::basic_endpoint<asio::generic::raw_protocol>(
        reinterpret_cast<sockaddr *>(&addr), sizeof(addr)));

    asio::ip::udp::endpoint local_endpoint(asio::ip::make_address("192.168.9.1"), GTPU_PORT);
    sock_udp.open(local_endpoint.protocol());
    sock_udp.bind(local_endpoint);
    gnb_endpoint = asio::ip::udp::endpoint(asio::ip::make_address(GNB_IP), GTPU_PORT);

    receiveRaw();
    receiveUdp();
    arpTick();
}

}  // namespace upf
