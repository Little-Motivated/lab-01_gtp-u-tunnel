#include "arp.hpp"
#include "gtpu.hpp"
#include "net.hpp"
#include "ue.hpp"
#include "upf.hpp"

#include <boost/asio.hpp>
#include <iostream>

namespace asio = boost::asio;

int main() {
    asio::io_context io;
    upf::Upf upf(io);
    try {
        upf.start();
        io.run();
    } catch (std::exception &e) {
        std::cout << e.what() << "\n";
        return 1;
    }
}
