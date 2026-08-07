//
// Created by 钟智强 on 2026/8/7.
//

#include "netstat.h"
#include "exec.h"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <vector>

#if defined(__APPLE__)
#include <ifaddrs.h>
#include <arpa/inet.h>
#include <net/if.h>
#elif defined(__linux__)
#include <ifaddrs.h>
#include <arpa/inet.h>
#include <net/if.h>
#endif

namespace Nezha::Tools {
namespace {

#if defined(__APPLE__)
void parse_netstat_output(const std::string &raw, std::vector<std::vector<std::string>> &rows, int limit) {
    std::istringstream ss(raw);
    std::string line;
    int count = 0;
    while (std::getline(ss, line) && count < limit) {
        if (line.empty() || line[0] == '\t' || line.starts_with("Active") ||
            line.starts_with("Proto"))
            continue;
        std::istringstream ls(line);
        std::string proto, recvq, sendq, local, remote, state;
        ls >> proto >> recvq >> sendq >> local >> remote >> state;
        if (proto.empty()) continue;
        rows.push_back({proto, local, remote, state});
        ++count;
    }
}
#elif defined(__linux__)
void parse_proc_net(const std::string &path, const std::string &proto,
                    std::vector<std::vector<std::string>> &rows, int limit) {
    std::ifstream f(path);
    if (!f.is_open()) return;
    std::string line;
    std::getline(f, line); // skip header
    int count = 0;
    while (std::getline(f, line) && count < limit) {
        std::istringstream ls(line);
        std::string local_hex, remote_hex, state_hex, rest;
        ls >> ws >> local_hex >> remote_hex >> state_hex;
        std::getline(ls, rest);

        auto decode_addr = [](const std::string &hex) -> std::string {
            auto colon = hex.find(':');
            if (colon == std::string::npos) return hex;
            uint32_t addr = std::stoul(hex.substr(0, colon), nullptr, 16);
            uint16_t port = std::stoul(hex.substr(colon + 1), nullptr, 16);
            char buf[64];
            in_addr in{};
            in.s_addr = htonl(addr);
            inet_ntop(AF_INET, &in, buf, sizeof(buf));
            return std::string(buf) + ":" + std::to_string(port);
        };

        std::string state_str = "?";
        uint8_t st = static_cast<uint8_t>(std::stoul(state_hex, nullptr, 16));
        const char *tcp_states[] = {"ESTAB", "SYN_SENT", "SYN_RECV", "FIN_W1",
                                    "FIN_W2", "TIME_WT", "CLOSE", "CLOSE_WT",
                                    "LAST_ACK", "LISTEN", "CLOSING"};
        if (st < 11) state_str = tcp_states[st];

        rows.push_back({proto, decode_addr(local_hex), decode_addr(remote_hex), state_str});
        ++count;
    }
}
#endif

} // anon

ToolResult NetstatTool::run(const ToolOptions &opts) const {
    auto t0 = std::chrono::steady_clock::now();
    ToolResult result;
    result.id = ToolId::Netstat;
    result.tool = "netstat";
    result.columns = {"协议", "本地地址", "远端地址", "状态"};

    std::vector<std::vector<std::string>> rows;

#if defined(__APPLE__)
    result.raw_text = run_command(
        "netstat -an -p tcp 2>/dev/null | head -" + std::to_string(opts.limit + 2));
    parse_netstat_output(result.raw_text, rows, opts.limit);

    // add interface list
    struct ifaddrs *ifap = nullptr;
    if (getifaddrs(&ifap) == 0 && ifap) {
        for (auto *ifa = ifap; ifa; ifa = ifa->ifa_next) {
            if (!ifa->ifa_addr) continue;
            if (ifa->ifa_addr->sa_family != AF_INET) continue;
            char buf[64];
            inet_ntop(AF_INET, &((sockaddr_in *)ifa->ifa_addr)->sin_addr, buf, sizeof(buf));
            rows.push_back({"IFACE", std::string(ifa->ifa_name), std::string(buf),
                            (ifa->ifa_flags & IFF_UP) ? "UP" : "DOWN"});
        }
        freeifaddrs(ifap);
    }

#elif defined(__linux__)
    parse_proc_net("/proc/net/tcp", "TCP", rows, opts.limit / 2);
    parse_proc_net("/proc/net/udp", "UDP", rows, opts.limit / 2);

    struct ifaddrs *ifap = nullptr;
    if (getifaddrs(&ifap) == 0 && ifap) {
        for (auto *ifa = ifap; ifa; ifa = ifa->ifa_next) {
            if (!ifa->ifa_addr) continue;
            if (ifa->ifa_addr->sa_family != AF_INET) continue;
            char buf[64];
            inet_ntop(AF_INET, &((sockaddr_in *)ifa->ifa_addr)->sin_addr, buf, sizeof(buf));
            rows.push_back({"IFACE", std::string(ifa->ifa_name), std::string(buf),
                            (ifa->ifa_flags & IFF_UP) ? "UP" : "DOWN"});
        }
        freeifaddrs(ifap);
    }
#endif

    result.rows = std::move(rows);
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - t0);
    result.elapsed_ms = elapsed;
    result.ok = true;
    result.summary = std::to_string(result.rows.size()) + " 连接 (用时 " +
                     std::to_string(elapsed.count()) + "ms)";
    return result;
}

} // namespace Nezha::Tools