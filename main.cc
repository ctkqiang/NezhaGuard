//
// 哪吒网络安全 SIEM — 主入口
// 串联：抓包采集 + 日志监控 + 蜜罐监听 → 攻击检测引擎 → 告警管理
//

#include <csignal>
#include <iostream>
#include <thread>

#include "src/core/alert.h"
#include "src/core/arena.h"
#include "src/core/capture.h"
#include "src/core/decoder.h"
#include "src/core/detector.h"
#include "src/core/event.h"
#include "src/core/honeypot.h"
#include "src/core/ipaddr.h"
#include "src/core/log_watcher.h"
#include "src/utilities/logger.h"

using namespace Nezha;

static Core::PacketCapture *g_cap = nullptr;
static volatile bool g_running = true;

static void on_signal(int) {
    NZ_WARN("收到退出信号，正在停止…");
    g_running = false;

    if (g_cap) g_cap->stop();
}

int main() {
    Core::HoneypotListener honeypot;

    Log::init_default("logs/nezha.log", Log::Level::Info);
    NZ_INFO("哪吒网络安全 SIEM 系统启动");

    Core::Arena arena(128 * 1024);
    Core::AttackDetector detector;
    Core::AlertManager alerter;

    alerter.set_dedup_window(120);

    alerter.set_callback([&](const Core::Alert &a) {
        if (a.level >= Severity::Error && a.count >= 5) {
            NZ_WARN("[聚合告警] {} 共检测 {} 次, 威胁评分={:.0f}",
                    attack_type_cstr(a.type), a.count, a.score);
        }
    });


    Core::PacketCapture cap;
    if (cap.open("en0", 65535, true, 1000)) {
        NZ_INFO("网络采集: en0 已打开");
        cap.set_filter("tcp or udp or icmp");
    } else {
        NZ_WARN("网络采集: en0 打开失败 (需要 root), 仅运行日志/蜜罐模式");
    }

    g_cap = &cap;

    const Core::HoneyPort honeypots[] = {
        {.port = 22, .proto = PROTO_TCP, .service = "SSH"},
        {.port = 23, .proto = PROTO_TCP, .service = "Telnet"},
        {.port = 3306, .proto = PROTO_TCP, .service = "MySQL"},
        {.port = 6379, .proto = PROTO_TCP, .service = "Redis"},
        {.port = 27017, .proto = PROTO_TCP, .service = "MongoDB"},
        {.port = 5432, .proto = PROTO_TCP, .service = "PostgreSQL"},
        {.port = 8080, .proto = PROTO_TCP, .service = "HTTP-Alt"},
        {.port = 8443, .proto = PROTO_TCP, .service = "HTTPS-Alt"},
    };
    for (auto &hp: honeypots) honeypot.add_port(hp);

    honeypot.start(arena, [&](const Core::event &e) {
        detector.analyze(e, arena, [&](const Core::Alert &a) { alerter.submit(a); });

        if (Log::Logger::instance().enabled(Log::Level::Debug)) {
            NZ_DEBUG("[蜜罐] {}:{} → {}",
                     e.src.to_string(), e.sport, e.dport);
        }
    });
    NZ_INFO("蜜罐: {} 个端口已启动", sizeof(honeypots) / sizeof(honeypots[0]));


    Core::LogWatcher log_watcher;
    const char *log_paths[] = {
        "/var/log/nginx/access.log",
        "/var/log/apache2/access.log",
        "/var/log/auth.log",
        "/var/log/syslog",
    };

    for (const char *path: log_paths) {
        log_watcher.add_source({path, EventSource::Log, 0});
    }

    log_watcher.start(arena, [&](const Core::event &e) {
        detector.analyze(e, arena, [&](const Core::Alert &a) { alerter.submit(a); });
    });

    NZ_INFO("日志监控: {} 个源已启动", sizeof(log_paths) / sizeof(log_paths[0]));

    std::signal(SIGINT, on_signal);
    std::signal(SIGTERM, on_signal);

    if (cap.is_open()) {
        cap.start([&](const std::uint8_t *raw, std::size_t len, const timeval &ts) {
            Core::event e{};
            if (!Core::ProtocolDecoder::decode(raw, len, ts, arena, e)) return;

            detector.analyze(e, arena, [&](const Core::Alert &a) { alerter.submit(a); });

            static Nanos last_flush = 0;
            if (e.ts_ns - last_flush > 30'000'000'000ULL) {
                alerter.flush();
                last_flush = e.ts_ns;
            }
        });
    } else {
        NZ_INFO("运行中 (无网络采集)，Ctrl+C 退出");
        while (g_running) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
            alerter.flush();
        }
    }

    NZ_INFO("正在停止所有模块…");

    honeypot.stop();
    log_watcher.stop();
    alerter.flush();

    Log::Logger::instance().flush();

    NZ_INFO("哪吒 SIEM 已停止，共处理 {} 条告警", alerter.total_alerts());

    return 0;
}
