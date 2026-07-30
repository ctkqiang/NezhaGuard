//
// Created by 钟智强 on 2026/7/30.
//

#ifndef NEZHAGUARD_HONEYPOT_H
#define NEZHAGUARD_HONEYPOT_H

#include "types.h"
#include "event.h"
#include <functional>
#include <string>
#include <thread>
#include <vector>
#include <atomic>

namespace Nezha::Core {
    class Arena;

    using HoneypotCallback = std::function<void(const event &)>;

    // 蜜罐监听端口配置
    struct HoneyPort {
        std::uint16_t port;
        std::uint8_t proto; // PROTO_TCP or PROTO_UDP
        const char *service; // 诱饵服务名 (如 "SSH", "MySQL", "Redis")
    };

    // 多端口 TCP/UDP 蜜罐：监听常见攻击端口，记录每个连接者为 event
    class HoneypotListener {
    public:
        HoneypotListener() = default;
        ~HoneypotListener();

        HoneypotListener(const HoneypotListener &) = delete;
        HoneypotListener &operator=(const HoneypotListener &) = delete;

        // 添加监听端口
        void add_port(const HoneyPort &hp);
        void add_port(std::uint16_t port, std::uint8_t proto, const char *service);

        // 启动所有端口监听（多线程），arena 驻留字符串，cb 每个连接回调
        void start(Arena &arena, HoneypotCallback cb);

        void stop();

        [[nodiscard]] bool running() const noexcept { return running_; }

    private:
        void listen_port(const HoneyPort &hp, Arena &arena, HoneypotCallback cb);
        static std::string get_peer_ip(int fd);

        struct PortThread {
            std::thread worker;
            int fd = -1;
        };
        std::vector<HoneyPort> ports_;
        std::vector<PortThread> threads_;
        std::atomic<bool> running_{false};
    };
}

#endif //NEZHAGUARD_HONEYPOT_H
