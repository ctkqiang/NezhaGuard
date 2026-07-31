//
// Created by 钟智强 on 2026/7/30.
//

#include "ipaddr.h"

#include <cstring>
#include <arpa/inet.h>
#include <netdb.h>


namespace Nezha::IPAddress {
    // v4 映射前缀 ::ffff:0:0/96
    static const std::uint8_t kV4Prefix[12] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0xff, 0xff};

    ipaddr ipaddr::from_v4(std::uint32_t h) {
        ipaddr a;
        std::memcpy(a.b_.data(), kV4Prefix, 12);
        a.b_[12] = static_cast<std::uint8_t>((h >> 24) & 0xff);
        a.b_[13] = static_cast<std::uint8_t>((h >> 16) & 0xff);
        a.b_[14] = static_cast<std::uint8_t>((h >> 8) & 0xff);
        a.b_[15] = static_cast<std::uint8_t>(h & 0xff);
        return a;
    }

    ipaddr ipaddr::from_bytes(const std::uint8_t src[16]) {
        ipaddr a;
        std::memcpy(a.b_.data(), src, 16);
        return a;
    }

    bool ipaddr::is_v4() const noexcept {
        return std::memcmp(b_.data(), kV4Prefix, 12) == 0;
    }

    bool ipaddr::is_loopback() const noexcept {
        if (is_v4()) return b_[12] == 127; // 127.0.0.0/8
        for (int i = 0; i < 15; ++i) if (b_[i]) return false;
        return b_[15] == 1; // ::1
    }

    bool ipaddr::is_private() const noexcept {
        if (is_v4()) {
            std::uint8_t x = b_[12], y = b_[13];
            return x == 10 || // 10.0.0.0/8
                   (x == 172 && (y & 0xf0) == 16) || // 172.16.0.0/12
                   (x == 192 && y == 168); // 192.168.0.0/16
        }
        return (b_[0] & 0xfe) == 0xfc; // fc00::/7 (ULA)
    }

    std::string ipaddr::to_string() const {
        char buf[INET6_ADDRSTRLEN] = {0};
        if (is_v4()) {
            struct in_addr v4;
            std::memcpy(&v4, b_.data() + 12, 4);
            ::inet_ntop(AF_INET, &v4, buf, sizeof(buf));
        } else {
            struct in6_addr v6;
            std::memcpy(&v6, b_.data(), 16);
            ::inet_ntop(AF_INET6, &v6, buf, sizeof(buf));
        }
        return buf;
    }

    bool ipaddr::parse(std::string_view text, ipaddr &out) {
        char tmp[INET6_ADDRSTRLEN];
        if (text.empty() || text.size() >= sizeof(tmp)) return false;
        std::memcpy(tmp, text.data(), text.size());
        tmp[text.size()] = '\0';

        struct in_addr v4;
        if (::inet_pton(AF_INET, tmp, &v4) == 1) {
            // 先按 v4，成功则映射
            std::memcpy(out.b_.data(), kV4Prefix, 12);
            std::memcpy(out.b_.data() + 12, &v4, 4);
            return true;
        }
        struct in6_addr v6;
        if (::inet_pton(AF_INET6, tmp, &v6) == 1) {
            std::memcpy(out.b_.data(), &v6, 16);
            return true;
        }
        return false;
    }

    std::size_t ipaddr_hash::operator()(const ipaddr &a) const noexcept {
        std::uint64_t h = 1469598103934665603ULL; // FNV-1a offset basis
        for (std::uint8_t byte: a.bytes()) {
            h ^= byte;
            h *= 1099511628211ULL;
        }
        return static_cast<std::size_t>(h);
    }

    std::string ipaddr::ResolveHostname(const std::string &ip) {
        struct sockaddr_in sa {};
        sa.sin_family = AF_INET;
        if (::inet_pton(AF_INET, ip.c_str(), &sa.sin_addr) != 1)
            return ip;

        char host[NI_MAXHOST] = {0};
        int r = ::getnameinfo(reinterpret_cast<struct sockaddr *>(&sa), sizeof(sa),
                              host, sizeof(host), nullptr, 0, NI_NAMEREQD);
        if (r == 0) return host;
        return ip;
    }
}
