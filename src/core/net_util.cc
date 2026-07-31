//
// Created by 钟智强 on 2026/7/31.
//

#include "net_util.h"

#include <arpa/inet.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <netinet/in.h>
#include <sys/socket.h>
#ifndef __linux__
#include <net/if_dl.h>
#include <net/route.h>
#include <sys/sysctl.h>
#endif

#include <cstring>
#include <format>
#include <fstream>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#include "../utilities/logger.h"

namespace Nezha::Core {

    void dump_local_ips() {
        ifaddrs *ifap = nullptr;
        if (getifaddrs(&ifap) != 0) {
            NZ_WARN("无法获取本地网络接口列表");
            return;
        }

        for (ifaddrs *ifa = ifap; ifa; ifa = ifa->ifa_next) {
            if (!ifa->ifa_addr) continue;
            if (ifa->ifa_flags & IFF_LOOPBACK) continue;

            int family = ifa->ifa_addr->sa_family;
            char buf[INET6_ADDRSTRLEN] = {0};

            if (family == AF_INET) {
                auto *sin = reinterpret_cast<sockaddr_in *>(ifa->ifa_addr);
                inet_ntop(AF_INET, &sin->sin_addr, buf, sizeof(buf));
                NZ_DEBUG("本地IP: {} ({})", buf, ifa->ifa_name);
            } else if (family == AF_INET6) {
                auto *sin6 = reinterpret_cast<sockaddr_in6 *>(ifa->ifa_addr);
                if (IN6_IS_ADDR_LINKLOCAL(&sin6->sin6_addr)) continue;
                inet_ntop(AF_INET6, &sin6->sin6_addr, buf, sizeof(buf));
                NZ_DEBUG("本地IPv6: {} ({})", buf, ifa->ifa_name);
            }
        }

        freeifaddrs(ifap);
    }

    static std::set<std::string> collect_local_ips() {
        std::set<std::string> ips;
        ifaddrs *ifap = nullptr;
        if (getifaddrs(&ifap) != 0) return ips;

        for (ifaddrs *ifa = ifap; ifa; ifa = ifa->ifa_next) {
            if (!ifa->ifa_addr) continue;
            if (ifa->ifa_addr->sa_family == AF_INET) {
                char buf[INET_ADDRSTRLEN] = {0};
                auto *sin = reinterpret_cast<sockaddr_in *>(ifa->ifa_addr);
                inet_ntop(AF_INET, &sin->sin_addr, buf, sizeof(buf));
                ips.insert(buf);
            }
        }
        freeifaddrs(ifap);
        return ips;
    }

    static bool is_multicast_mac(const uint8_t mac[6]) {
        return (mac[0] & 0x01) != 0;
    }

    static bool is_zero_mac(const uint8_t mac[6]) {
        for (int i = 0; i < 6; ++i)
            if (mac[i] != 0) return false;
        return true;
    }

#ifdef __linux__
    void dump_arp_table() {
        std::ifstream arp("/proc/net/arp");
        if (!arp.is_open()) {
            NZ_WARN("无法读取 /proc/net/arp");
            return;
        }

        auto local_ips = collect_local_ips();
        std::set<std::string> seen;
        std::string line;
        int count = 0;

        std::getline(arp, line); // skip header
        while (std::getline(arp, line)) {
            std::istringstream iss(line);
            std::string ip, hw_type, flags, mac, mask, dev;
            iss >> ip >> hw_type >> flags >> mac >> mask >> dev;
            if (ip.empty() || mac.empty() || mac == "00:00:00:00:00:00") continue;
            if (local_ips.count(ip)) continue;
            std::string key = ip + "@" + mac;
            if (seen.insert(key).second) {
                NZ_DEBUG("ARP: {} @ {} ({})", ip, mac, dev);
                ++count;
            }
        }
        if (count == 0) NZ_INFO("ARP表: 无活跃远程条目");
    }

    static int count_arp_entries() {
        std::ifstream arp("/proc/net/arp");
        if (!arp.is_open()) return 0;
        int n = 0;
        std::string line;
        std::getline(arp, line);
        while (std::getline(arp, line)) ++n;
        return n;
    }
#else
    void dump_arp_table() {
        int mib[6] = {CTL_NET, PF_ROUTE, 0, AF_INET, NET_RT_FLAGS, RTF_LLINFO};
        std::vector<char> buf;
        std::size_t needed = 0;

        if (sysctl(mib, 6, nullptr, &needed, nullptr, 0) != 0) {
            NZ_WARN("无法查询ARP表大小: {}", strerror(errno));
            return;
        }
        if (needed == 0) {
            NZ_INFO("ARP表: 无条目");
            return;
        }

        buf.resize(needed * 2);
        needed = buf.size();
        if (sysctl(mib, 6, buf.data(), &needed, nullptr, 0) != 0) {
            NZ_WARN("无法读取ARP表: {}", strerror(errno));
            return;
        }

        auto local_ips = collect_local_ips();
        std::set<std::string> seen;
        char ip_buf[INET_ADDRSTRLEN];
        char mac_buf[18];
        int count = 0;

        for (char *p = buf.data(); p < buf.data() + needed;) {
            auto *rtm = reinterpret_cast<rt_msghdr *>(p);
            if (rtm->rtm_version != RTM_VERSION) break;

            if (!(rtm->rtm_flags & RTF_LLINFO)) { p += rtm->rtm_msglen; continue; }
            if (rtm->rtm_flags & RTF_LOCAL) { p += rtm->rtm_msglen; continue; }
            if (rtm->rtm_flags & RTF_BROADCAST) { p += rtm->rtm_msglen; continue; }
            if (rtm->rtm_flags & RTF_MULTICAST) { p += rtm->rtm_msglen; continue; }

            auto *sa = reinterpret_cast<sockaddr *>(rtm + 1);
            int addrs = rtm->rtm_addrs;

            const char *ip_str = nullptr;
            uint8_t mac[6] = {0};
            bool has_mac = false;

            for (int i = 0; i < RTAX_MAX && sa; ++i) {
                if (addrs & (1 << i)) {
                    int slen = sa->sa_len > 0 ? sa->sa_len : static_cast<uint8_t>(sizeof(sockaddr));
                    if (i == RTAX_DST && sa->sa_family == AF_INET) {
                        auto *sin = reinterpret_cast<sockaddr_in *>(sa);
                        ip_str = inet_ntop(AF_INET, &sin->sin_addr, ip_buf, sizeof(ip_buf));
                    }
                    if (i == RTAX_GATEWAY && sa->sa_family == AF_LINK) {
                        auto *sdl = reinterpret_cast<sockaddr_dl *>(sa);
                        if (sdl->sdl_alen == 6) {
                            std::memcpy(mac, LLADDR(sdl), 6);
                            has_mac = true;
                        }
                    }
                    sa = reinterpret_cast<sockaddr *>(reinterpret_cast<char *>(sa) + slen);
                }
            }

            if (!ip_str || !has_mac) { p += rtm->rtm_msglen; continue; }

            if (is_multicast_mac(mac) || is_zero_mac(mac)) { p += rtm->rtm_msglen; continue; }

            snprintf(mac_buf, sizeof(mac_buf),
                     "%02x:%02x:%02x:%02x:%02x:%02x",
                     mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
            std::string mac_str(mac_buf);

            if (mac_str == "ff:ff:ff:ff:ff:ff") { p += rtm->rtm_msglen; continue; }

            if (local_ips.count(ip_str)) { p += rtm->rtm_msglen; continue; }

            std::string key = std::string(ip_str) + "@" + mac_str;
            if (seen.insert(key).second) {
                NZ_DEBUG("ARP: {} @ {}", ip_str, mac_str);
                ++count;
            }

            p += rtm->rtm_msglen;
        }

        if (count == 0) NZ_INFO("ARP表: 无活跃远程条目");
    }

    static int count_local_ips() {
        int n = 0;
        ifaddrs *ifap = nullptr;
        if (getifaddrs(&ifap) == 0) {
            for (ifaddrs *ifa = ifap; ifa; ifa = ifa->ifa_next)
                if (ifa->ifa_addr && !(ifa->ifa_flags & IFF_LOOPBACK) && ifa->ifa_addr->sa_family == AF_INET)
                    ++n;
            freeifaddrs(ifap);
        }
        return n;
    }

    static int count_arp_entries() {
        int mib[6] = {CTL_NET, PF_ROUTE, 0, AF_INET, NET_RT_FLAGS, RTF_LLINFO};
        std::size_t needed = 0;
        if (sysctl(mib, 6, nullptr, &needed, nullptr, 0) != 0 || needed == 0) return 0;
        std::vector<char> buf(needed * 2);
        needed = buf.size();
        if (sysctl(mib, 6, buf.data(), &needed, nullptr, 0) != 0) return 0;
        int n = 0;
        for (char *p = buf.data(); p < buf.data() + needed;) {
            auto *rtm = reinterpret_cast<rt_msghdr *>(p);
            if (rtm->rtm_version != RTM_VERSION) break;
            if ((rtm->rtm_flags & RTF_LLINFO) && !(rtm->rtm_flags & RTF_LOCAL)) ++n;
            p += rtm->rtm_msglen;
        }
        return n;
    }
#endif

    void dump_network_info() {
        NZ_INFO("  本地接口: {} 个  |  ARP 条目: {} 个",
                count_local_ips(), count_arp_entries());
        dump_local_ips();
        dump_arp_table();
    }

}
