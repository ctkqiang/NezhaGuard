//
// Created by 钟智强 on 2026/8/2.
//

#ifndef NEZHAGUARD_IPCN_H
#define NEZHAGUARD_IPCN_H

#include <string>
#include <mutex>
#include <chrono>

namespace Nezha::Core {

struct IpCnResult {
    std::string ip;
    std::string country;
    std::string province;
    std::string city;
    std::string district;
    std::string isp;
    bool valid = false;
};

class IpCn {
public:
    IpCn() = delete;

    static IpCnResult lookup_self();
    static IpCnResult lookup(const std::string &ip);

private:
    static bool fetch_ticket(std::string &ticket_out);
    static IpCnResult parse(const std::string &json);

    static std::mutex ticket_mtx_;
    static std::string cached_ticket_;
    static std::chrono::steady_clock::time_point ticket_expiry_;
};

} // namespace Nezha::Core

#endif //NEZHAGUARD_IPCN_H
