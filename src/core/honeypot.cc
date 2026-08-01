//
// Created by 钟智强 on 2026/7/30.
//

#include "honeypot.h"
#include "arena.h"
#include "ipaddr.h"
#include <arpa/inet.h>
#include <chrono>
#include <cstring>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

namespace Nezha::Core {

    HoneypotListener::~HoneypotListener() { stop(); }

    void HoneypotListener::add_port(const HoneyPort &hp) {
        ports_.push_back(hp);
    }

    void HoneypotListener::add_port(std::uint16_t port, std::uint8_t proto, const char *service) {
        ports_.push_back({port, proto, service});
    }

    void HoneypotListener::start(Arena &arena, HoneypotCallback cb) {
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
            if (t.fd >= 0) {
                shutdown(t.fd, SHUT_RDWR);
                close(t.fd);
                t.fd = -1;
            }
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

    void HoneypotListener::listen_port(const HoneyPort &hp, Arena &arena, HoneypotCallback cb) {
        int type = (hp.proto == PROTO_UDP) ? SOCK_DGRAM : SOCK_STREAM;
        int fd = socket(AF_INET6, type, 0);
        if (fd < 0) return;

        int opt = 1;
        setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
        /* 双栈: IPv6 socket 同时接受 IPv4 映射连接 */
        int v6only = 0;
        setsockopt(fd, IPPROTO_IPV6, IPV6_V6ONLY, &v6only, sizeof(v6only));
        int flags = fcntl(fd, F_GETFL, 0);
        if (flags != -1) fcntl(fd, F_SETFL, flags | O_NONBLOCK);

        sockaddr_in6 addr{};
        addr.sin6_family = AF_INET6;
        addr.sin6_port = htons(hp.port);
        addr.sin6_addr = in6addr_any;

        if (bind(fd, reinterpret_cast<sockaddr *>(&addr), sizeof(addr)) < 0) {
            close(fd);
            return;
        }

        if (hp.proto == PROTO_TCP && listen(fd, 16) < 0) {
            close(fd);
            return;
        }

        for (auto &t: threads_) {
            if (t.fd < 0) { t.fd = fd; break; }
        }

        char buf[1024];
        while (running_) {
            fd_set rfds;
            FD_ZERO(&rfds);
            FD_SET(fd, &rfds);
            timeval tv{1, 0}; // 1 秒超时

            if (select(fd + 1, &rfds, nullptr, nullptr, &tv) <= 0) continue;

            if (hp.proto == PROTO_UDP) {
                sockaddr_storage from{};
                socklen_t from_len = sizeof(from);
                ssize_t n = recvfrom(fd, buf, sizeof(buf) - 1, 0,
                                     reinterpret_cast<sockaddr *>(&from), &from_len);
                if (n > 0 && cb) {
                    event e{};
                    e.ts_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
                                  std::chrono::system_clock::now().time_since_epoch())
                                  .count();
                    e.source = EventSource::Honeypot;
                    e.proto = PROTO_UDP;
                    e.dport = hp.port;
                    e.sport = 0;

                    char ip_buf[INET6_ADDRSTRLEN]{};
                    if (from.ss_family == AF_INET) {
                        auto *s = reinterpret_cast<sockaddr_in *>(&from);
                        inet_ntop(AF_INET, &s->sin_addr, ip_buf, sizeof(ip_buf));
                        e.sport = ntohs(s->sin_port);
                    }
                    IPAddress::ipaddr::parse(ip_buf, e.src);
                    e.msg = arena.intern(std::string_view(buf, static_cast<std::size_t>(n)));
                    e.fields.put(0, FieldVal::str(arena.intern(hp.service)));
                    cb(e);
                }
            } else {
                int client = accept(fd, nullptr, nullptr);
                if (client >= 0) {
                    if (cb) {
                        event e{};
                        e.ts_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
                                      std::chrono::system_clock::now().time_since_epoch())
                                      .count();
                        e.source = EventSource::Honeypot;
                        e.proto = PROTO_TCP;
                        e.dport = hp.port;

                        std::string ip = get_peer_ip(client);
                        IPAddress::ipaddr::parse(ip, e.src);
                        e.msg = arena.intern("TCP 连接");
                        e.fields.put(0, FieldVal::str(arena.intern(hp.service)));
                        cb(e);
                    }
                    close(client);
                }
            }
        }
        close(fd);
    }

}
