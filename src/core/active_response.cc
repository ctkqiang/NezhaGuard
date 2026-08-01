#include "active_response.h"
#include "../utilities/logger.h"

#include <arpa/inet.h>
#include <cstring>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <netinet/ip_icmp.h>
#include <sys/socket.h>
#include <unistd.h>

namespace Nezha::Core {

    namespace {
        // RFC 1624 incremental checksum update
        std::uint16_t in_cksum(const void *data, std::size_t len) {
            auto *p = static_cast<const std::uint16_t *>(data);
            std::uint32_t sum = 0;
            while (len > 1) { sum += *p++; len -= 2; }
            if (len) sum += *reinterpret_cast<const std::uint8_t *>(p);
            sum = (sum >> 16) + (sum & 0xffff);
            sum += (sum >> 16);
            return static_cast<std::uint16_t>(~sum);
        }
    }

    bool ActiveResponse::send_icmp_unreachable(const std::string &src_ip,
                                                const std::string &dst_ip,
                                                const std::uint8_t *original_pkt,
                                                std::size_t original_len) {
        int sock = ::socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
        if (sock < 0) {
            NZ_DEBUG("ActiveResponse: 无法创建原始 ICMP socket");
            return false;
        }

        // ICMP Unreachable packet = IP header placeholder + ICMP header + original IP header + 8 bytes
        constexpr std::size_t kIpHdrLen = 20;
        constexpr std::size_t kIcmpHdrLen = 8;
        std::size_t orig_copy = std::min(original_len, kIpHdrLen + 8);
        std::size_t total = kIcmpHdrLen + orig_copy;
        static constexpr std::size_t kMaxIcmpPkt = kIcmpHdrLen + kIpHdrLen + 8;
        std::uint8_t pkt[kMaxIcmpPkt];
        std::memset(pkt, 0, sizeof(pkt));

        auto *icmp = reinterpret_cast<struct icmp *>(pkt);
        icmp->icmp_type = ICMP_UNREACH;
        icmp->icmp_code = ICMP_UNREACH_HOST;
        icmp->icmp_cksum = 0;
        if (original_pkt && original_len > 0)
            std::memcpy(pkt + kIcmpHdrLen, original_pkt, orig_copy);
        icmp->icmp_cksum = in_cksum(pkt, total);

        struct sockaddr_in sa {};
        sa.sin_family = AF_INET;
        ::inet_pton(AF_INET, dst_ip.c_str(), &sa.sin_addr);

        ssize_t sent = ::sendto(sock, pkt, total, 0,
                                reinterpret_cast<struct sockaddr *>(&sa),
                                sizeof(sa));
        ::close(sock);
        if (sent < 0) {
            NZ_DEBUG("ActiveResponse: ICMP unreachable 发送失败");
            return false;
        }
        NZ_WARN("ActiveResponse: → ICMP Unreachable 已发送至 {}", dst_ip);
        return true;
    }

    bool ActiveResponse::send_tcp_rst(const std::string &src_ip,
                                       const std::string &dst_ip,
                                       std::uint16_t sport,
                                       std::uint16_t dport) {
        int sock = ::socket(AF_INET, SOCK_RAW, IPPROTO_RAW);
        if (sock < 0) {
            NZ_DEBUG("ActiveResponse: 无法创建原始 IP socket");
            return false;
        }

        int on = 1;
        if (::setsockopt(sock, IPPROTO_IP, IP_HDRINCL, &on, sizeof(on)) < 0) {
            ::close(sock);
            return false;
        }

        constexpr std::size_t kTotal = sizeof(struct ip) + sizeof(struct tcphdr);
        std::uint8_t pkt[kTotal];
        std::memset(pkt, 0, kTotal);

        auto *ip = reinterpret_cast<struct ip *>(pkt);
        ip->ip_v = 4;
        ip->ip_hl = 5;
        ip->ip_tos = 0;
        ip->ip_len = htons(kTotal);
        ip->ip_id = 0;
        ip->ip_off = 0;
        ip->ip_ttl = 64;
        ip->ip_p = IPPROTO_TCP;
        ::inet_pton(AF_INET, src_ip.c_str(), &ip->ip_src);
        ::inet_pton(AF_INET, dst_ip.c_str(), &ip->ip_dst);
        ip->ip_sum = in_cksum(ip, sizeof(struct ip));

        auto *tcp = reinterpret_cast<struct tcphdr *>(pkt + sizeof(struct ip));
        tcp->th_sport = htons(sport);
        tcp->th_dport = htons(dport);
        tcp->th_seq = 0;
        tcp->th_ack = 0;
        tcp->th_off = 5;
        tcp->th_flags = TH_RST | TH_ACK;
        tcp->th_win = 0;

        struct sockaddr_in sa {};
        sa.sin_family = AF_INET;
        ::inet_pton(AF_INET, dst_ip.c_str(), &sa.sin_addr);

        ssize_t sent = ::sendto(sock, pkt, kTotal, 0,
                                reinterpret_cast<struct sockaddr *>(&sa),
                                sizeof(sa));
        ::close(sock);
        if (sent < 0) {
            NZ_DEBUG("ActiveResponse: TCP RST 发送失败");
            return false;
        }
        NZ_WARN("ActiveResponse: → TCP RST 已发送至 {}:{}", dst_ip, dport);
        return true;
    }

} // namespace Nezha::Core
