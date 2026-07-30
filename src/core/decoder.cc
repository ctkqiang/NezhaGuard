//
// Created by 钟智强 on 2026/7/30.
//

#include "decoder.h"
#include "arena.h"
#include "ipaddr.h"
#include <cstring>
#include <algorithm>
#include <arpa/inet.h>

namespace Nezha::Core {

    // ----- 协议头结构（仅用于指针解读，不做内存拷贝）-----
#pragma pack(push, 1)
    struct EthHdr {
        std::uint8_t dst[6];
        std::uint8_t src[6];
        std::uint16_t ethertype; // 网络字节序
    };

    struct Ipv4Hdr {
        std::uint8_t ver_ihl;
        std::uint8_t dscp_ecn;
        std::uint16_t total_len;
        std::uint16_t id;
        std::uint16_t frag;
        std::uint8_t ttl;
        std::uint8_t proto;
        std::uint16_t csum;
        std::uint8_t src[4];
        std::uint8_t dst[4];
    };

    struct Ipv6Hdr {
        std::uint32_t ver_tc_fl;
        std::uint16_t payload_len;
        std::uint8_t next_hdr;
        std::uint8_t hop_limit;
        std::uint8_t src[16];
        std::uint8_t dst[16];
    };

    struct TcpHdr {
        std::uint16_t sport;
        std::uint16_t dport;
        std::uint32_t seq;
        std::uint32_t ack;
        std::uint8_t data_offs; // 高4位=头长(×4)
        std::uint8_t flags;
        std::uint16_t window;
        std::uint16_t csum;
        std::uint16_t urg;
    };

    struct UdpHdr {
        std::uint16_t sport;
        std::uint16_t dport;
        std::uint16_t len;
        std::uint16_t csum;
    };
#pragma pack(pop)

    static constexpr std::uint16_t ETHERTYPE_IPV4 = 0x0800;
    static constexpr std::uint16_t ETHERTYPE_IPV6 = 0x86DD;

    // 常见 HTTP 端口
    static bool is_http_port(std::uint16_t port) {
        switch (port) {
            case 80: case 443: case 8080: case 8443:
            case 8000: case 8888: case 9090:
                return true;
            default: return false;
        }
    }

    bool ProtocolDecoder::decode(const std::uint8_t *raw,
                                 std::size_t len,
                                 const timeval &ts,
                                 Arena &arena,
                                 event &out) {
        if (len < sizeof(EthHdr)) return false;

        const auto *eth = reinterpret_cast<const EthHdr *>(raw);
        std::uint16_t etype = ntohs(eth->ethertype);

        // VLAN 标签跳过 4 字节，重新取 EtherType
        if (etype == 0x8100) {
            if (len < sizeof(EthHdr) + 4) return false;
            raw += sizeof(EthHdr) + 4;
            len -= sizeof(EthHdr) + 4;
            if (len < 2) return false;
            etype = ntohs(*reinterpret_cast<const std::uint16_t *>(raw - 2));
        } else {
            raw += sizeof(EthHdr);
            len -= sizeof(EthHdr);
        }

        const std::uint8_t *l4_hdr = nullptr;
        std::size_t l4_len = 0;
        std::uint8_t proto = 0;

        // --- L3: IP 层 ---
        if (etype == ETHERTYPE_IPV4) {
            if (len < sizeof(Ipv4Hdr)) return false;
            const auto *ip4 = reinterpret_cast<const Ipv4Hdr *>(raw);
            std::uint8_t ihl = (ip4->ver_ihl & 0x0f) * 4;
            if (ihl < 20 || len < ihl) return false;

            proto = ip4->proto;
            out.src = IPAddress::ipaddr::from_v4(ntohl(*reinterpret_cast<const std::uint32_t *>(ip4->src)));
            out.dst = IPAddress::ipaddr::from_v4(ntohl(*reinterpret_cast<const std::uint32_t *>(ip4->dst)));
            out.proto = proto;

            l4_hdr = raw + ihl;
            l4_len = len - ihl;

        } else if (etype == ETHERTYPE_IPV6) {
            if (len < sizeof(Ipv6Hdr)) return false;
            const auto *ip6 = reinterpret_cast<const Ipv6Hdr *>(raw);

            proto = ip6->next_hdr;
            out.src = IPAddress::ipaddr::from_bytes(ip6->src);
            out.dst = IPAddress::ipaddr::from_bytes(ip6->dst);
            out.proto = proto;

            l4_hdr = raw + sizeof(Ipv6Hdr);
            l4_len = len - sizeof(Ipv6Hdr);

        } else {
            return false; // 非 IP 包
        }

        // 时间戳
        out.ts_ns = static_cast<Nanos>(ts.tv_sec) * 1'000'000'000ULL
                    + static_cast<Nanos>(ts.tv_usec) * 1000ULL;

        out.source = EventSource::Packet;

        // --- L4: TCP / UDP / ICMP ---
        if (proto == PROTO_TCP && l4_len >= sizeof(TcpHdr)) {
            const auto *tcp = reinterpret_cast<const TcpHdr *>(l4_hdr);
            out.sport = ntohs(tcp->sport);
            out.dport = ntohs(tcp->dport);

            std::uint8_t tcp_hdr_len = (tcp->data_offs >> 4) * 4;
            if (tcp_hdr_len < sizeof(TcpHdr)) tcp_hdr_len = sizeof(TcpHdr);

            // HTTP 负载解析
            if (is_http_port(out.sport) || is_http_port(out.dport)) {
                std::size_t pay_off = tcp_hdr_len;
                if (pay_off < l4_len) {
                    parse_http(l4_hdr + pay_off, l4_len - pay_off, arena, out);
                }
            }
        } else if (proto == PROTO_UDP && l4_len >= sizeof(UdpHdr)) {
            const auto *udp = reinterpret_cast<const UdpHdr *>(l4_hdr);
            out.sport = ntohs(udp->sport);
            out.dport = ntohs(udp->dport);
        } else if (proto == PROTO_ICMP) {
            out.sport = 0;
            out.dport = 0;
        }

        return true;
    }

    void ProtocolDecoder::parse_http(const std::uint8_t *payload,
                                     std::size_t len,
                                     Arena &arena,
                                     event &e) {
        if (len < 8) return;

        // 只解析 HTTP 请求（以方法开头）
        const char *data = reinterpret_cast<const char *>(payload);
        std::string_view view(data, len);

        // 找到 CRLF 结束的第一行（请求行）
        auto crlf = view.find("\r\n");
        if (crlf == std::string_view::npos) crlf = view.find('\n');
        std::string_view req_line = (crlf != std::string_view::npos)
                                    ? view.substr(0, crlf) : view;

        // 请求行: METHOD SP PATH SP VERSION
        auto sp1 = req_line.find(' ');
        if (sp1 == std::string_view::npos) return;
        auto sp2 = req_line.find(' ', sp1 + 1);
        if (sp2 == std::string_view::npos) sp2 = req_line.size();

        std::string_view method = req_line.substr(0, sp1);

        // 验证是合法 HTTP 方法
        static const char *methods[] = {
            "GET", "POST", "PUT", "DELETE", "HEAD", "OPTIONS",
            "PATCH", "CONNECT", "TRACE"
        };
        bool valid = false;
        for (const char *m: methods) {
            if (method == m) { valid = true; break; }
        }
        if (!valid) return;

        std::string_view path = req_line.substr(sp1 + 1, sp2 - sp1 - 1);

        // 基础消息: METHOD + path
        char buf[256];
        int n = snprintf(buf, sizeof(buf), "%.*s %.*s",
                         static_cast<int>(method.size()), method.data(),
                         static_cast<int>(path.size()), path.data());
        e.msg = arena.intern(std::string_view(buf, static_cast<std::size_t>(n)));

        // 提取 Host 头
        if (crlf != std::string_view::npos && crlf + 2 < view.size()) {
            std::string_view headers = view.substr(crlf + 2);
            auto host_pos = headers.find("Host:");
            if (host_pos == std::string_view::npos) host_pos = headers.find("host:");
            if (host_pos != std::string_view::npos) {
                auto host_end = headers.find("\r\n", host_pos);
                if (host_end == std::string_view::npos) host_end = headers.find('\n', host_pos);
                if (host_end == std::string_view::npos) host_end = headers.size();
                std::string_view host_val = headers.substr(host_pos + 5, host_end - host_pos - 5);
                // trim 首尾空白
                while (!host_val.empty() && (host_val.front() == ' ' || host_val.front() == '\t'))
                    host_val.remove_prefix(1);
                while (!host_val.empty() && (host_val.back() == ' ' || host_val.back() == '\t'))
                    host_val.remove_suffix(1);
                e.fields.put(1, FieldVal::str(arena.intern(host_val)));
            }
        }

        // 记录 HTTP 方法
        e.fields.put(0, FieldVal::str(arena.intern(method)));
    }

}
