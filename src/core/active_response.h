#ifndef NEZHAGUARD_ACTIVE_RESPONSE_H
#define NEZHAGUARD_ACTIVE_RESPONSE_H

#include <string>

namespace Nezha::Core {

class ActiveResponse {
public:
    ActiveResponse() = delete;

    static bool send_icmp_unreachable(const std::string &src_ip,
                                      const std::string &dst_ip,
                                      const std::uint8_t *original_pkt,
                                      std::size_t original_len);

    static bool send_tcp_rst(const std::string &src_ip,
                             const std::string &dst_ip,
                             std::uint16_t sport,
                             std::uint16_t dport);
};

}

#endif //NEZHAGUARD_ACTIVE_RESPONSE_H
