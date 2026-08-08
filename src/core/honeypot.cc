//
// Created by 钟智强 on 2026/7/30.
//

#include "honeypot.h"
#include "arena.h"
#include "ipaddr.h"
#include "../service/database_helper.h"
#include "../utilities/logger.h"

#include <algorithm>
#include <arpa/inet.h>
#include <chrono>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

namespace Nezha::Core {

    HoneypotListener::~HoneypotListener() { stop(); }
    void HoneypotListener::add_port(const HoneyPort &hp) { ports_.push_back(hp); }
    void HoneypotListener::add_port(uint16_t port, uint8_t proto, const char *svc) {
        ports_.push_back({port, proto, svc});
    }

    void HoneypotListener::start(Arena &arena, const HoneypotCallback &cb) {
        if (running_) return;
        running_ = true;
        for (const auto &hp: ports_) {
            threads_.push_back({});
            threads_.back().worker = std::thread([this, hp, &arena, cb]() {
                listen_port(hp, arena, cb);
            });
        }
    }

    void HoneypotListener::stop() {
        running_ = false;
        for (auto &t: threads_) {
            if (t.fd >= 0) { shutdown(t.fd, SHUT_RDWR); close(t.fd); t.fd = -1; }
            if (t.worker.joinable()) t.worker.join();
        }
        threads_.clear();
    }

    std::string HoneypotListener::get_peer_ip(int fd) {
        sockaddr_storage addr{};
        socklen_t len = sizeof(addr);
        char buf[INET6_ADDRSTRLEN]{};
        if (getpeername(fd, reinterpret_cast<sockaddr *>(&addr), &len) == 0) {
            if (addr.ss_family == AF_INET) {
                auto *s = reinterpret_cast<sockaddr_in *>(&addr);
                inet_ntop(AF_INET, &s->sin_addr, buf, sizeof(buf));
            } else if (addr.ss_family == AF_INET6) {
                auto *s = reinterpret_cast<sockaddr_in6 *>(&addr);
                inet_ntop(AF_INET6, &s->sin6_addr, buf, sizeof(buf));
            }
        }
        return buf;
    }

    // ── fake banners per service ──
    static const char *fake_banner(const char *service) {
        if (!service) return nullptr;
        std::string_view sv(service);
        if (sv == "SSH")         return "SSH-2.0-OpenSSH_8.9p1 Ubuntu-3ubuntu0.6\r\n";
        if (sv == "Telnet")      return "\377\375\030\377\375\040\377\375\043\377\375\047\r\nUbuntu 22.04 LTS\r\nlogin: ";
        if (sv == "MySQL")       return "N\x00\x00\x00\x0a" "8.0.36-0ubuntu0.22.04.1\x00\x01\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00";
        if (sv == "Redis")       return "-NOAUTH Authentication required.\r\n";
        if (sv == "PostgreSQL")  return "E\000\000\000\204SFATAL\000VFATAL\000C28000\000Mno pg_hba.conf entry\000Fauth.c\000L1234\000RClientAuthentication\000\000";
        if (sv == "MongoDB")     return "MongoDB 6.0.5\000\000\000\000\000\000\000\000\000\000\000\000\324\007\000\000\000\000\000\000";
        return nullptr;
    }

    // ── classify attacker technique ──
    static std::string classify_technique(const std::string &payload, const char *service) {
        if (payload.empty()) return "T1046: Network Service Scanning";
        std::string lower = payload;
        std::ranges::transform(lower, lower.begin(), [](unsigned char c) { return std::tolower(c); });

        if (lower.find("select") != std::string::npos || lower.find("union") != std::string::npos ||
            lower.find("drop") != std::string::npos || lower.find("insert") != std::string::npos)
            return "T1190: Exploit Public-Facing Application / SQLi";
        if (lower.find("/etc/passwd") != std::string::npos || lower.find("/bin/") != std::string::npos ||
            lower.find("cmd=") != std::string::npos || lower.find("exec") != std::string::npos)
            return "T1059: Command & Scripting Interpreter";
        if (lower.find("wget") != std::string::npos || lower.find("curl") != std::string::npos ||
            lower.find("base64") != std::string::npos || lower.find("eval") != std::string::npos)
            return "T1105: Ingress Tool Transfer";
        if (lower.find("nmap") != std::string::npos || lower.find("scan") != std::string::npos ||
            lower.find("probe") != std::string::npos)
            return "T1046: Network Service Scanning";
        if (lower.find("admin") != std::string::npos || lower.find("root") != std::string::npos ||
            lower.find("password") != std::string::npos || lower.find("ssh") != std::string::npos)
            return "T1110: Brute Force";
        if (lower.find("../") != std::string::npos || lower.find("..\\") != std::string::npos ||
            lower.find("etc/passwd") != std::string::npos)
            return "T1003: OS Credential Dumping / Path Traversal";
        if (lower.find("<?php") != std::string::npos || lower.find("<script") != std::string::npos ||
            lower.find("powershell") != std::string::npos || lower.find("cmd.exe") != std::string::npos)
            return "T1505: Server Software Component / Webshell";
        if (lower.find("user-agent") != std::string::npos || lower.find("sqlmap") != std::string::npos)
            return "T1595: Active Scanning";
        return "T1190: Exploit Public-Facing Application";
    }

    // ── read attacker payload with timeout ──
    static std::string read_payload(int fd, int timeout_ms = 3000) {
        std::string data;
        char buf[4096];
        auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout_ms);

        // set non-blocking
        int flags = fcntl(fd, F_GETFL, 0);
        fcntl(fd, F_SETFL, flags | O_NONBLOCK);

        while (true) {
            auto now = std::chrono::steady_clock::now();
            if (now >= deadline) break;
            int remaining = std::chrono::duration_cast<std::chrono::milliseconds>(deadline - now).count();

            fd_set rfds;
            FD_ZERO(&rfds);
            FD_SET(fd, &rfds);
            timeval tv{remaining / 1000, (remaining % 1000) * 1000};
            if (select(fd + 1, &rfds, nullptr, nullptr, &tv) <= 0) break;

            ssize_t n = read(fd, buf, sizeof(buf) - 1);
            if (n <= 0) break;
            buf[n] = '\0';
            data.append(buf, n);
            if (data.size() > 8192) break; // 8KB max
        }
        // strip non-printables for display
        std::string clean;
        for (char c : data) {
            if (c >= 32 && c < 127) clean += c;
            else if (c == '\n' || c == '\r' || c == '\t') clean += c;
            else clean += '.';
        }
        if (clean.size() > 500) clean = clean.substr(0, 500) + "…";
        return clean;
    }

    void HoneypotListener::listen_port(const HoneyPort &hp, Arena &arena, const HoneypotCallback &cb) {
        int type = (hp.proto == PROTO_UDP) ? SOCK_DGRAM : SOCK_STREAM;
        int fd = socket(AF_INET6, type, 0);
        if (fd < 0) return;

        int opt = 1;
        setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
        int v6only = 0;
        setsockopt(fd, IPPROTO_IPV6, IPV6_V6ONLY, &v6only, sizeof(v6only));
        int flags = fcntl(fd, F_GETFL, 0);
        if (flags != -1) fcntl(fd, F_SETFL, flags | O_NONBLOCK);

        sockaddr_in6 addr{};
        addr.sin6_family = AF_INET6;
        addr.sin6_port = htons(hp.port);
        addr.sin6_addr = in6addr_any;

        if (bind(fd, reinterpret_cast<sockaddr *>(&addr), sizeof(addr)) < 0) { close(fd); return; }
        if (hp.proto == PROTO_TCP && listen(fd, 16) < 0) { close(fd); return; }

        for (auto &t: threads_) { if (t.fd < 0) { t.fd = fd; break; } }

        while (running_) {
            fd_set rfds;
            FD_ZERO(&rfds);
            FD_SET(fd, &rfds);
            timeval tv{1, 0};
            if (select(fd + 1, &rfds, nullptr, nullptr, &tv) <= 0) continue;

            if (hp.proto == PROTO_UDP) {
                char buf[1024];
                sockaddr_storage from{};
                socklen_t from_len = sizeof(from);
                ssize_t n = recvfrom(fd, buf, sizeof(buf) - 1, 0,
                                     reinterpret_cast<sockaddr *>(&from), &from_len);
                if (n > 0 && cb) {
                    event e{};
                    e.ts_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
                                  std::chrono::system_clock::now().time_since_epoch()).count();
                    e.source = EventSource::Honeypot;
                    e.proto = PROTO_UDP;
                    e.dport = hp.port;
                    char ip_buf[INET6_ADDRSTRLEN]{};
                    if (from.ss_family == AF_INET) {
                        auto *s = reinterpret_cast<sockaddr_in *>(&from);
                        inet_ntop(AF_INET, &s->sin_addr, ip_buf, sizeof(ip_buf));
                        e.sport = ntohs(s->sin_port);
                    }
                    IPAddress::ipaddr::parse(ip_buf, e.src);
                    e.msg = arena.intern(std::string_view(buf, static_cast<size_t>(n)));
                    e.fields.put(0, FieldVal::str(arena.intern(hp.service)));
                    cb(e);
                }
            } else {
                if (int client = accept(fd, nullptr, nullptr); client >= 0) {
                    std::string ip = get_peer_ip(client);
                    std::string sport_str = "?";

                    sockaddr_storage peer{};
                    socklen_t peer_len = sizeof(peer);
                    if (getpeername(client, reinterpret_cast<sockaddr *>(&peer), &peer_len) == 0) {
                        if (peer.ss_family == AF_INET)
                            sport_str = std::to_string(ntohs(reinterpret_cast<sockaddr_in *>(&peer)->sin_port));
                    }

                    // send fake banner
                    if (auto *banner = fake_banner(hp.service)) {
                        send(client, banner, strlen(banner), 0);
                    }

                    // read attacker payload
                    std::string payload = read_payload(client, 3000);

                    // classify technique
                    std::string technique = classify_technique(payload, hp.service);

                    // ── log to database ──
                    {
                        Database::HoneypotSessionRecord rec;
                        rec.attacker_ip = ip;
                        rec.attacker_port = sport_str;
                        rec.service = hp.service;
                        rec.payload = payload;
                        rec.technique = technique;
                        rec.threat_score = payload.empty() ? 30.0 : 65.0 + std::min(payload.size() / 100.0, 35.0);
                        Database::DatabaseHelper::InsertHoneypotSession(rec);
                    }

                    NZ_WARN("[蜜罐] {}  {}:{}  {}  |  {}  |  载荷:{} |  评分:{:.0f}",
                            hp.service, ip, sport_str,
                            technique,
                            payload.empty() ? "(扫描)" : payload.substr(0, 80),
                            payload.empty() ? 30.0 : 65.0 + std::min(payload.size() / 100.0, 35.0));

                    // ── emit event to detector ──
                    if (cb) {
                        event e{};
                        e.ts_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
                                      std::chrono::system_clock::now().time_since_epoch()).count();
                        e.source = EventSource::Honeypot;
                        e.proto = PROTO_TCP;
                        e.dport = hp.port;
                        IPAddress::ipaddr::parse(ip, e.src);
                        std::string evt_msg = std::string(hp.service) + ": " + payload;
                        e.msg = arena.intern(evt_msg);
                        e.fields.put(0, FieldVal::str(arena.intern(hp.service)));
                        e.fields.put(1, FieldVal::str(arena.intern(technique)));
                        cb(e);
                    }
                    close(client);
                }
            }
        }
        close(fd);
    }

}
