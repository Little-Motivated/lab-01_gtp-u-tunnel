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

class GTPUCodec {
private:
    int gtp_version = 1;
    int gtp_type = 0xff;

public:
    void encodeTpdu(int gtp_teid, int payload) {
    }
}
}  // namespace gtpu
