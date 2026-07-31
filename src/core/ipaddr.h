//
// Created by 钟智强 on 2026/7/30.
//

#ifndef NEZHAGUARD_IPADDR_H
#define NEZHAGUARD_IPADDR_H

#include <array>
#include <cstdint>
#include <string>
#include <string_view>

namespace Nezha::IPAddress {
    // 统一 16 字节地址：IPv4 以 ::ffff:a.b.c.d 映射存放，
    // v4/v6 共用同一套类型、hash、比较、容器 key，全系统无需 if(is_v4) 分支。
    class ipaddr {
    public:
        ipaddr() = default;

        static ipaddr from_v4(std::uint32_t host_order); // v4 主机序整数
        static ipaddr from_bytes(const std::uint8_t src[16]); // 直接拷 16 字节(如从包里)
        static bool parse(std::string_view text, ipaddr &out); // 解析 v4 或 v6 文本

        static std::string ResolveHostname(const std::string& ip);

        bool is_v4() const noexcept;

        bool is_v6() const noexcept { return !is_v4(); }

        bool is_loopback() const noexcept; // 127.0.0.0/8 或 ::1
        bool is_private() const noexcept; // RFC1918 / fc00::/7 (规则里排除内网)

        const std::array<std::uint8_t, 16> &bytes() const noexcept { return b_; }

        std::string to_string() const;

        bool operator==(const ipaddr &o) const noexcept { return b_ == o.b_; }
        bool operator!=(const ipaddr &o) const noexcept { return b_ != o.b_; }
        bool operator<(const ipaddr &o) const noexcept { return b_ < o.b_; }

    private:
        std::array<std::uint8_t, 16> b_{}; // 网络字节序(大端)
    };

    // 无序容器(unordered_map/set)所需 hash：std::array 无默认 std::hash。
    struct ipaddr_hash {
        std::size_t operator()(const ipaddr &a) const noexcept;
    };
}

#endif //NEZHAGUARD_IPADDR_H
