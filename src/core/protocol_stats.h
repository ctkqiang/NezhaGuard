#pragma once

#include <atomic>
#include <cstdint>
#include <string>
#include <vector>

namespace Nezha::Core {

enum class ProtoClass : std::uint8_t {
    TCP,
    UDP,
    ICMP,
    HTTP,
    Other
};

inline const char *proto_class_name(ProtoClass pc) noexcept {
    switch (pc) {
        case ProtoClass::TCP:   return "TCP";
        case ProtoClass::UDP:   return "UDP";
        case ProtoClass::ICMP:  return "ICMP";
        case ProtoClass::HTTP:  return "HTTP";
        case ProtoClass::Other: return "Other";
        default:                return "???";
    }
}

struct ProtoSnapshot {
    uint64_t total_packets;
    uint64_t total_bytes;
    uint64_t tcp_pkts;
    uint64_t tcp_bytes;
    uint64_t udp_pkts;
    uint64_t udp_bytes;
    uint64_t icmp_pkts;
    uint64_t icmp_bytes;
    uint64_t http_pkts;
    uint64_t http_bytes;
    uint64_t other_pkts;
    uint64_t other_bytes;

    [[nodiscard]] uint64_t by_class(ProtoClass pc) const {
        switch (pc) {
            case ProtoClass::TCP:  return tcp_pkts;
            case ProtoClass::UDP:  return udp_pkts;
            case ProtoClass::ICMP: return icmp_pkts;
            case ProtoClass::HTTP: return http_pkts;
            default:               return other_pkts;
        }
    }

    [[nodiscard]] uint64_t bytes_by_class(ProtoClass pc) const {
        switch (pc) {
            case ProtoClass::TCP:  return tcp_bytes;
            case ProtoClass::UDP:  return udp_bytes;
            case ProtoClass::ICMP: return icmp_bytes;
            case ProtoClass::HTTP: return http_bytes;
            default:               return other_bytes;
        }
    }
};

class ProtocolStats {
public:
    static ProtocolStats &instance() noexcept {
        static ProtocolStats inst;
        return inst;
    }

    ProtocolStats(const ProtocolStats &) = delete;
    ProtocolStats &operator=(const ProtocolStats &) = delete;

    void record_packet(std::uint8_t ip_proto, std::size_t len, bool is_http) noexcept {
        total_packets_.fetch_add(1, std::memory_order_relaxed);
        total_bytes_.fetch_add(len, std::memory_order_relaxed);

        if (is_http) {
            http_pkts_.fetch_add(1, std::memory_order_relaxed);
            http_bytes_.fetch_add(len, std::memory_order_relaxed);
            // HTTP is over TCP, still count TCP as well
        }

        switch (ip_proto) {
            case 6: // TCP
                tcp_pkts_.fetch_add(1, std::memory_order_relaxed);
                tcp_bytes_.fetch_add(len, std::memory_order_relaxed);
                break;
            case 17: // UDP
                udp_pkts_.fetch_add(1, std::memory_order_relaxed);
                udp_bytes_.fetch_add(len, std::memory_order_relaxed);
                break;
            case 1: // ICMP
                icmp_pkts_.fetch_add(1, std::memory_order_relaxed);
                icmp_bytes_.fetch_add(len, std::memory_order_relaxed);
                break;
            default:
                other_pkts_.fetch_add(1, std::memory_order_relaxed);
                other_bytes_.fetch_add(len, std::memory_order_relaxed);
                break;
        }
    }

    [[nodiscard]] ProtoSnapshot snapshot() const noexcept {
        return {
            total_packets_.load(std::memory_order_relaxed),
            total_bytes_.load(std::memory_order_relaxed),
            tcp_pkts_.load(std::memory_order_relaxed),
            tcp_bytes_.load(std::memory_order_relaxed),
            udp_pkts_.load(std::memory_order_relaxed),
            udp_bytes_.load(std::memory_order_relaxed),
            icmp_pkts_.load(std::memory_order_relaxed),
            icmp_bytes_.load(std::memory_order_relaxed),
            http_pkts_.load(std::memory_order_relaxed),
            http_bytes_.load(std::memory_order_relaxed),
            other_pkts_.load(std::memory_order_relaxed),
            other_bytes_.load(std::memory_order_relaxed),
        };
    }

    void reset() noexcept {
        total_packets_.store(0, std::memory_order_relaxed);
        total_bytes_.store(0, std::memory_order_relaxed);
        tcp_pkts_.store(0, std::memory_order_relaxed);
        tcp_bytes_.store(0, std::memory_order_relaxed);
        udp_pkts_.store(0, std::memory_order_relaxed);
        udp_bytes_.store(0, std::memory_order_relaxed);
        icmp_pkts_.store(0, std::memory_order_relaxed);
        icmp_bytes_.store(0, std::memory_order_relaxed);
        http_pkts_.store(0, std::memory_order_relaxed);
        http_bytes_.store(0, std::memory_order_relaxed);
        other_pkts_.store(0, std::memory_order_relaxed);
        other_bytes_.store(0, std::memory_order_relaxed);
    }

private:
    ProtocolStats() = default;

    std::atomic<std::uint64_t> total_packets_{0};
    std::atomic<std::uint64_t> total_bytes_{0};
    std::atomic<std::uint64_t> tcp_pkts_{0};
    std::atomic<std::uint64_t> tcp_bytes_{0};
    std::atomic<std::uint64_t> udp_pkts_{0};
    std::atomic<std::uint64_t> udp_bytes_{0};
    std::atomic<std::uint64_t> icmp_pkts_{0};
    std::atomic<std::uint64_t> icmp_bytes_{0};
    std::atomic<std::uint64_t> http_pkts_{0};
    std::atomic<std::uint64_t> http_bytes_{0};
    std::atomic<std::uint64_t> other_pkts_{0};
    std::atomic<std::uint64_t> other_bytes_{0};
};

} // namespace Nezha::Core
