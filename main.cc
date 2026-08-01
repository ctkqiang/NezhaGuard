//
// 哪吒网络安全 SIEM — 主入口
// 串联：抓包采集 + 日志监控 + 蜜罐监听 → 攻击检测引擎 → 告警管理
//

#include <csignal>
#include <cstdlib>
#include <ctime>
#include <iostream>
#include <thread>
#include <unistd.h>
#include <unordered_map>

#include "src/contants.h"
#include "src/core/alert.h"
#include "src/core/arena.h"
#include "src/core/capture.h"
#include "src/core/decoder.h"
#include "src/core/detector.h"
#include "src/core/event.h"
#include "src/core/honeypot.h"
#include "src/core/ipaddr.h"
#include "src/core/net_util.h"
#include "src/core/active_response.h"
#include "src/core/tor_checker.h"
#include "src/core/log_watcher.h"
#include "src/service/database_helper.h"
#include "src/utilities/logger.h"

using namespace Nezha;

static const char* get_net_interface() {
    const char* env = std::getenv("NEZHA_INTERFACE");
    if (env && env[0] != '\0') return env;
#ifdef __linux__
    return "eth0";
#else
    return "en0";
#endif
}

static Core::PacketCapture *g_cap = nullptr;
static volatile bool g_running = true;

static void on_signal(int) {
    NZ_WARN("正在关闭…");
    g_running = false;
    if (g_cap) g_cap->stop();
}

// =============================================================================
// CLI 模式 (原有控制台逻辑)
// =============================================================================
static int run_cli_mode() {
    Core::HoneypotListener honeypot;

    Log::init_default("logs/nezha.log", Log::Level::Info);
    Database::DatabaseHelper::InitializeQuarantineDatabase();
    Core::TorChecker tor_checker;
    tor_checker.initialize();
    auto qlist = Database::DatabaseHelper::GetQuarantineList();
    char hostname[256] = {0};
    gethostname(hostname, sizeof(hostname));
    NZ_INFO("══════════════════════════════════════════════════════════════");
    NZ_INFO("");
    NZ_INFO("  哪 吒 网 络 安 全  SIEM  系 统");
    NZ_INFO("  NezhaGuard v{} — 蓝队主动防御平台", Configuration::ApplicationConstants::ApplicationVersion);
    NZ_INFO("");
    NZ_INFO("  C++26  |  Qt6  |  libpcap  |  SQLite3  |  spdlog");
    NZ_INFO("  构建: {} {}", __DATE__, __TIME__);
    NZ_INFO("  主机: {}  |  PID: {}", hostname, getpid());
    NZ_INFO("");
    NZ_INFO("══════════════════════════════════════════════════════════════");
    NZ_INFO("  [配置摘要]");
    NZ_INFO("  运行模式:      蓝队 CLI 控制台 (无 GUI)");
    NZ_INFO("  网卡接口:      {}", get_net_interface());
    NZ_INFO("  抓包过滤器:    tcp or udp or icmp");
    NZ_INFO("  隔离阈值:      {} 次异常触发自动隔离",
            Configuration::ApplicationConstants::AnomaliesQuarantineThreshold);
    NZ_INFO("  告警去重窗口:  10 秒");
    NZ_INFO("  Arena 块大小:  128 KB (每 30s 回收)");
    NZ_INFO("  历史隔离记录:  {} 条", qlist.size());
    if (!qlist.empty()) {
        NZ_INFO("  ── 已隔离 IP 列表 ──");
        for (const auto &r : qlist)
            NZ_INFO("    {}  [{}]  score={:.0f}", r.ip_address, r.reason, r.threat_score);
    }
    NZ_INFO("══════════════════════════════════════════════════════════════");
    Core::dump_network_info();

    Core::Arena arena(128 * 1024);
    Core::AttackDetector detector;
    Core::AlertManager alerter;
    alerter.set_dedup_window(10);

    alerter.set_callback([&](const Core::Alert &a) {
        if (a.level >= Severity::Error && a.count >= 5) {
            std::string ip(a.src_ip);
            std::string host = IPAddress::ipaddr::ResolveHostname(ip);
            if (host != ip)
                NZ_WARN("[聚合] {}  {} ({}), {} 次, 评分 {:.0f}",
                    attack_type_cstr(a.type), ip, host, a.count, a.score);
            else
                NZ_WARN("[聚合] {}  {}, {} 次, 评分 {:.0f}",
                    attack_type_cstr(a.type), ip, a.count, a.score);
        }
    });

    Core::PacketCapture cap;
    if (cap.open(get_net_interface(), 65535, true, 1000)) {
        NZ_INFO("抓包引擎已启动: {}", get_net_interface());
        cap.set_filter("tcp or udp or icmp");
    } else {
        NZ_WARN("抓包引擎启动失败 (需 root 权限)");
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
            NZ_DEBUG("蜜罐连接 {}:{} → :{}", e.src.to_string(), e.sport, e.dport);
        }
    });
    NZ_INFO("蜜罐引擎已启动: {} 端口", sizeof(honeypots) / sizeof(honeypots[0]));

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
    NZ_INFO("日志引擎已启动: {} 监控源", sizeof(log_paths) / sizeof(log_paths[0]));

    std::signal(SIGINT, on_signal);
    std::signal(SIGTERM, on_signal);

    if (cap.is_open()) {
        cap.start([&](const std::uint8_t *raw, std::size_t len, const timeval &ts) {
            static uint64_t pkt_count = 0;
            static Nanos last_stats = 0;
            ++pkt_count;
            Core::event e{};
            if (!Core::ProtocolDecoder::decode(raw, len, ts, arena, e)) return;
            if (Database::DatabaseHelper::IsIPQuarantined(e.src.to_string())) {
                static std::unordered_map<std::string, Nanos> last_warn;
                Nanos now_ns = static_cast<Nanos>(ts.tv_sec) * 1'000'000'000ULL + ts.tv_usec * 1000ULL;
                auto ip = e.src.to_string();
                auto it = last_warn.find(ip);
                if (it == last_warn.end() || (now_ns - it->second) > 10'000'000'000ULL) {
                    NZ_WARN("已隔离 IP 被拦截: {}", ip);
                    last_warn[ip] = now_ns;
                }
                Core::ActiveResponse::send_icmp_unreachable(
                    e.dst.to_string(), ip, raw, len);
                return;
            }
            if (tor_checker.is_tor_exit(e.src.to_string())) {
                NZ_WARN("[Tor] 检测到 Tor 出口节点流量: {} → {}",
                        e.src.to_string(), e.dst.to_string());
            }
            detector.analyze(e, arena, [&](const Core::Alert &a) { alerter.submit(a); });
            if (e.proto == PROTO_ICMP)
                NZ_DEBUG("[ICMP] {} → {}  len={}",
                     e.src.to_string(), e.dst.to_string(), len);
            else if (e.proto == PROTO_TCP)
                NZ_TRACE("[TCP] {}:{} → {}:{}  len={}",
                     e.src.to_string(), e.sport,
                     e.dst.to_string(), e.dport, len);
            else
                NZ_TRACE("[UDP] {}:{} → {}:{}  len={}",
                     e.src.to_string(), e.sport,
                     e.dst.to_string(), e.dport, len);
            static Nanos last_flush = 0;
            if (e.ts_ns - last_flush > 30'000'000'000ULL) {
                alerter.flush();
                last_flush = e.ts_ns;
            }
            if (last_stats == 0) last_stats = e.ts_ns;
            if (e.ts_ns - last_stats > 60'000'000'000ULL) {
                auto ql = Database::DatabaseHelper::GetQuarantineList();
                double elapsed = (e.ts_ns - last_stats) / 1'000'000'000.0;
                NZ_INFO("[统计] 包: {}  |  告警: {}  |  隔离: {}  |  速率: {:.0f} pps  |  Tor节点: {}",
                        pkt_count, alerter.total_alerts(), ql.size(),
                        pkt_count / elapsed, tor_checker.total_nodes());
                last_stats = e.ts_ns;
            }
        });
    } else {
        NZ_INFO("运行中 (无网络采集)，Ctrl+C 退出");
        while (g_running) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
            alerter.flush();
        }
    }

    NZ_INFO("正在停止所有引擎…");
    honeypot.stop();
    log_watcher.stop();
    alerter.flush();
    Log::Logger::instance().flush();
    auto final_qlist = Database::DatabaseHelper::GetQuarantineList();
    NZ_INFO("");
    NZ_INFO("══════════════════════════════════════════════════════════════");
    NZ_INFO("");
    NZ_INFO("  哪 吒 网 络 安 全  SIEM  系 统  已 停 止");
    NZ_INFO("");
    NZ_INFO("  ── 本次运行摘要 ──");
    NZ_INFO("  累计告警:     {}", alerter.total_alerts());
    NZ_INFO("  当前隔离 IP:  {}", final_qlist.size());
    NZ_INFO("  Tor 出口节点: {} 个已缓存", tor_checker.total_nodes());
    NZ_INFO("");
    if (!final_qlist.empty()) {
        NZ_INFO("  ── 隔离列表 ──");
        for (const auto &r: final_qlist)
            NZ_INFO("    {}  [{}]  score={:.0f}", r.ip_address, r.reason, r.threat_score);
        NZ_INFO("");
    }
    NZ_INFO("  日志文件: logs/nezha.log");
    NZ_INFO("  隔离数据库: data/nezha_quarantine.db");
    NZ_INFO("");
    NZ_INFO("══════════════════════════════════════════════════════════════");
    return 0;
}


#include <QApplication>
#include <QTimer>
#include <QString>

#include "src/views/monitor.h"
#include "src/views/gui_sink.h"
#include "src/views/log_model.h"

static int run_gui_mode(int argc, char *argv[]) {
    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("哪吒网络安全 SIEM"));
    app.setApplicationVersion(
        QString::fromLatin1(Configuration::ApplicationConstants::ApplicationVersion));
    qputenv("QT_LOGGING_RULES", "qt.*=false");

    Log::init_default("logs/nezha.log", Log::Level::Info);
    Database::DatabaseHelper::InitializeQuarantineDatabase();
    Core::TorChecker tor_checker;
    tor_checker.initialize();
    NZ_INFO("  哪吒网络安全 SIEM 系统 {} [蓝队模式]",
            Configuration::ApplicationConstants::ApplicationVersion
    );

    NZ_INFO("  隔离阈值: {} 次  |  Tor 节点: {}  |  历史隔离: {} 条",
            Configuration::ApplicationConstants::AnomaliesQuarantineThreshold,
            tor_checker.total_nodes(),
            Database::DatabaseHelper::GetQuarantineList().size()
    );

    Core::dump_network_info();

    monitor window;
    window.init_models();
    window.show();

    if (auto sink = window.gui_sink()) Log::Logger::instance().add_sink(sink);


    Core::Arena arena(128 * 1024);
    Core::AttackDetector detector;
    Core::AlertManager alerter;
    alerter.set_dedup_window(10);

    alerter.set_callback([&](const Core::Alert &a) {
        if (a.level >= Severity::Error && a.count >= 5) {
            std::string ip(a.src_ip);
            std::string host = IPAddress::ipaddr::ResolveHostname(ip);
            if (host != ip)
                NZ_WARN("[聚合] {}  {} ({}), {} 次, 评分 {:.0f}",
                    attack_type_cstr(a.type), ip, host, a.count, a.score);
            else
                NZ_WARN("[聚合] {}  {}, {} 次, 评分 {:.0f}",
                    attack_type_cstr(a.type), ip, a.count, a.score);
        }
        uint64_t ts_ns = a.ts_ns;
        std::string type_str = attack_type_cstr(a.type);
        std::string ip_str(a.src_ip);
        int cnt = static_cast<int>(a.count);
        double sc = a.score;
        QString sev = [](Severity s) -> QString {
            switch (s) {
                case Severity::Critical: return QStringLiteral("CRIT");
                case Severity::Error: return QStringLiteral("ERROR");
                case Severity::Warn: return QStringLiteral("WARN");
                default: return QStringLiteral("INFO");
            }
        }(a.level);

        QMetaObject::invokeMethod(&window, [window = &window, ts_ns, type_str, ip_str, cnt, sc, sev]() {
            auto ns = static_cast<time_t>(ts_ns / 1'000'000'000ULL);
            char ts[16];
            std::strftime(ts, sizeof(ts), "%H:%M:%S", std::localtime(&ns));
            window->append_alert(
                QString::fromUtf8(ts),
                QString::fromUtf8(type_str.c_str()),
                QString::fromUtf8(ip_str.c_str()),
                cnt, sc, sev);
        }, Qt::QueuedConnection);
    });

    Core::HoneypotListener honeypot;
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

    std::thread honey_thread([&]() {
        honeypot.start(arena, [&](const Core::event &e) {
            detector.analyze(e, arena, [&](const Core::Alert &a) { alerter.submit(a); });
            QMetaObject::invokeMethod(&window, [&, e]() {
                const Core::HoneyPort *hp = nullptr;
                for (auto &h: honeypots)
                    if (h.port == e.dport) {
                        hp = &h;
                        break;
                    }
                window.append_honeypot(
                    QString::number(e.ts_ns),
                    QString::fromStdString(e.src.to_string()),
                    e.sport, e.dport,
                    hp ? QString::fromUtf8(hp->service) : QStringLiteral("?"));
            }, Qt::QueuedConnection);
        });
    });
    NZ_INFO("蜜罐引擎已启动: {} 端口", sizeof(honeypots) / sizeof(honeypots[0]));

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

    std::thread log_thread([&]() {
        log_watcher.start(arena, [&](const Core::event &e) {
            detector.analyze(e, arena, [&](const Core::Alert &a) { alerter.submit(a); });
        });
    });
    NZ_INFO("日志引擎已启动: {} 监控源", sizeof(log_paths) / sizeof(log_paths[0]));

    Core::PacketCapture cap;
    std::thread cap_thread;
    if (cap.open(get_net_interface(), 65535, true, 1000)) {
        NZ_INFO("抓包引擎已启动: {}", get_net_interface());
        cap.set_filter("tcp or udp or icmp");
        g_cap = &cap;
        cap_thread = std::thread([&]() {
            cap.start([&](const std::uint8_t *raw, std::size_t len, const timeval &ts) {
                Core::event e{};

                if (!Core::ProtocolDecoder::decode(raw, len, ts, arena, e)) return;
                if (Database::DatabaseHelper::IsIPQuarantined(e.src.to_string())) {
                    static std::unordered_map<std::string, Nanos> last_warn;
                    Nanos now_ns = static_cast<Nanos>(ts.tv_sec) * 1'000'000'000ULL + ts.tv_usec * 1000ULL;
                    auto ip = e.src.to_string();
                    auto it = last_warn.find(ip);
                    if (it == last_warn.end() || (now_ns - it->second) > 10'000'000'000ULL) {
                        NZ_WARN("已隔离 IP 被拦截: {}", ip);
                        last_warn[ip] = now_ns;
                    }
                    Core::ActiveResponse::send_icmp_unreachable(
                        e.dst.to_string(), ip, raw, len);
                    return;
                }
                detector.analyze(e, arena, [&](const Core::Alert &a) { alerter.submit(a); });
                if (e.proto == PROTO_ICMP)
                    NZ_DEBUG("ICMP {} -> {}", e.src.to_string(), e.dst.to_string());
                else
                    NZ_TRACE("{} {}:{} -> :{}", e.proto == PROTO_TCP ? "TCP" : "UDP",
                         e.src.to_string(), e.sport, e.dport);
                static Nanos last_flush = 0;
                if (e.ts_ns - last_flush > 30'000'000'000ULL) {
                    alerter.flush();
                    last_flush = e.ts_ns;
                }
            });
        });
    } else {
        NZ_WARN("抓包引擎启动失败 (需 root 权限)");
    }

    // 定时刷新告警 + 更新统计
    QTimer *flush_timer = new QTimer(&window);
    QObject::connect(flush_timer, &QTimer::timeout, [&]() {
        alerter.flush();
        window.update_stats(window.log_model()->total(), alerter.total_alerts());
    });
    flush_timer->start(1000);

    // 信号 → 优雅退出
    std::signal(SIGINT, [](int) { QApplication::quit(); });
    std::signal(SIGTERM, [](int) { QApplication::quit(); });

    int ret = app.exec();

    NZ_INFO("正在停止所有引擎…");
    g_running = false;
    if (g_cap) g_cap->stop();
    if (cap_thread.joinable()) cap_thread.join();
    honeypot.stop();
    if (honey_thread.joinable()) honey_thread.join();
    log_watcher.stop();
    if (log_thread.joinable()) log_thread.join();
    alerter.flush();
    Log::Logger::instance().flush();
    NZ_INFO("SIEM 已停止  共处理 {} 条告警", alerter.total_alerts());

    return ret;
}


int main(int argc, char *argv[]) {
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "-v") == 0 || std::strcmp(argv[i], "--verbose") == 0)
            Log::Logger::instance().set_level(Log::Level::Debug);
    }

    bool show_gui = Configuration::ApplicationConstants::ShowGui;
    const char* gui_env = std::getenv("NEZHA_SHOW_GUI");
    if (gui_env) show_gui = (std::strcmp(gui_env, "0") != 0 && std::strcmp(gui_env, "false") != 0);

    if (show_gui) {
        return run_gui_mode(argc, argv);
    }
    return run_cli_mode();
}
