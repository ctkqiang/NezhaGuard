//
// 哪吒网络安全 SIEM — 主入口
// 串联：抓包采集 + 日志监控 + 蜜罐监听 → 攻击检测引擎 → 告警管理
//

#include <csignal>
#include <cstdlib>
#include <ctime>
#include <format>
#include <sys/resource.h>
#include <sys/utsname.h>
#include <thread>
#include <unistd.h>
#include <unordered_map>

#if defined(__APPLE__)
#include <sys/sysctl.h>
#include <mach-o/dyld.h>
#endif

#include <filesystem>

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
#include "src/core/ipcn.h"
#include "src/core/active_response.h"
#include "src/core/tor_checker.h"
#include "src/core/log_watcher.h"
#include "src/core/protocol_stats.h"
#include "src/core/application_monitor.h"
#include "src/service/database_helper.h"
#include "src/service/notifier.h"
#include "src/utilities/logger.h"

using namespace Nezha;

namespace fs = std::filesystem;

namespace {
    struct AppPaths {
        std::string rules_dir; // rules/default.yaml
        std::string config_dir; // config/
        std::string data_dir; // data/ (DB + writable)
        std::string log_dir; // logs/
    };
}

// 解析应用路径：macOS .app bundle → ~/Library/Application Support +
// Bundle Resources；开发环境 → 当前目录
static AppPaths resolve_app_paths() {
    AppPaths p;

#if defined(__APPLE__)
    char exe[PATH_MAX];
    uint32_t sz = sizeof(exe);
    if (_NSGetExecutablePath(exe, &sz) == 0) {
        char real[PATH_MAX];
        if (realpath(exe, real)) {
            std::string ep(real);
            if (auto macos = ep.find(".app/Contents/MacOS/"); macos != std::string::npos) {
                // 运行于 .app bundle 内
                std::string resources = ep.substr(0, macos) + ".app/Contents/Resources/";
                std::string home = getenv("HOME") ? getenv("HOME") : "/tmp";
                std::string app_support = home + "/Library/Application Support/NezhaGuard";

                p.rules_dir = resources + "rules/";
                p.config_dir = resources + "config/";
                p.data_dir = app_support + "/data/";
                p.log_dir = app_support + "/logs/";
                return p;
            }
        }
    }
#endif
    // 开发环境 / CLI / Linux — 使用当前目录
    p.rules_dir = "rules/";
    p.config_dir = "config/";
    p.data_dir = "data/";
    p.log_dir = "logs/";
    return p;
}

static void ensure_dirs(const AppPaths &p) {
    std::error_code ec;
    fs::create_directories(p.data_dir, ec);
    fs::create_directories(p.log_dir, ec);
}

static const char *get_net_interface() {
    const char *env = std::getenv("NEZHA_INTERFACE");
    if (env && env[0] != '\0') return env;
#ifdef __linux__
    return "eth0";
#else
    return "en0";
#endif
}

static Core::PacketCapture *g_cap = nullptr;
static Core::AttackDetector *g_detector = nullptr;
static volatile bool g_running = true;

static AppPaths g_paths;

static void on_sighup(int) {
    if (g_detector) g_detector->reload_rules((g_paths.rules_dir + "default.yaml").c_str());
}

static std::string quarantine_detail(const std::string &ip) {
    for (const auto &r: Database::DatabaseHelper::GetQuarantineList()) {
        if (r.ip_address == ip) {
            return std::format("threat={:.0f} reason=\"{}\" since={}", r.threat_score, r.reason, r.quarantined_at);
        }
    }

    return {};
}

static void log_quarantine_block(const std::string &src_ip, const std::string &dst_ip, uint8_t proto, uint16_t dport,
                                 uint16_t sport) {
    const char *protoname = proto == PROTO_TCP ? "TCP" : proto == PROTO_UDP ? "UDP" : "ICMP";
    std::string detail = quarantine_detail(src_ip);

    if (proto == PROTO_ICMP) {
        NZ_WARN("\033[38;2;240,104,128m[封禁] {} {} → {}\033[0m  |  {}",
                protoname, src_ip, dst_ip, detail);
    } else {
        NZ_WARN("\033[38;2;240,104,128m[封禁] {} {}:{} → {}:{}\033[0m  |  {}  |  \033[38;2;200,168,240m威胁评分 95/100\033[0m",
                protoname, src_ip, sport, dst_ip, dport, detail);
    }
}

static void on_signal(int) {
    NZ_WARN("正在关闭…");
    g_running = false;
    if (g_cap) g_cap->stop();
}

static int run_cli_mode() {
    auto paths = resolve_app_paths();
    ensure_dirs(paths);
    g_paths = paths;

    Core::HoneypotListener honeypot;

    Log::init_default((paths.log_dir + "nezha.log").c_str(), Log::Level::Info);
    Database::DatabaseHelper::InitializeQuarantineDatabase(paths.data_dir);
    Core::TorChecker tor_checker;

    tor_checker.initialize();
    auto qlist = Database::DatabaseHelper::GetQuarantineList();
    char hostname[256] = {0};

    gethostname(hostname, sizeof(hostname));

    // system info
    struct utsname uts{};
    long ncpu = sysconf(_SC_NPROCESSORS_ONLN);
    long page_size = sysconf(_SC_PAGESIZE);
    long phys_pages = sysconf(_SC_PHYS_PAGES);
    std::string os_name = uname(&uts) == 0 ? std::format("{} {} {}", uts.sysname, uts.release, uts.machine) : "unknown";

    NZ_INFO("");
    NZ_INFO("╔══════════════════════════════════════════════════════════════╗");
    NZ_INFO("║  哪吒网络安全 SIEM 系统                                      ║");
    NZ_INFO("║  NezhaGuard v{} — 蓝队主动防御平台                   ║",
            Configuration::ApplicationConstants::ApplicationVersion);
    NZ_INFO("╚══════════════════════════════════════════════════════════════╝");
    NZ_INFO("");
    NZ_INFO("  ◆ 系统信息");
    NZ_INFO("  OS:       {}", os_name);
    NZ_INFO("  CPU:      {} cores  |  Memory: {} MB", ncpu, (phys_pages * page_size) / (1024 * 1024));
    NZ_INFO("  Host:     {}  |  PID: {}", hostname, getpid());
    NZ_INFO("  User:     {}  |  Build: {} {} {}",
            getenv("USER") ? getenv("USER") : "unknown",
            Configuration::ApplicationConstants::ApplicationVersion, __DATE__, __TIME__);
    NZ_INFO("");
    NZ_INFO("  ◆ 引擎配置");
    NZ_INFO("  Mode:     CLI (Headless)  |  Interface: {}", get_net_interface());
    NZ_INFO("  BPF:      tcp or udp or icmp  |  Honeypots: 8 ports");
    NZ_INFO("  LogSrc:   4 files  |  Threshold: {}",
            Configuration::ApplicationConstants::AnomaliesQuarantineThreshold);
    NZ_INFO("  Dedup:    10s  |  Arena: 128KB  |  Tor: {} nodes",
            tor_checker.total_nodes());
    NZ_INFO("  Quarantine history: {} records", qlist.size());

    if (!qlist.empty()) {
        NZ_INFO("  ── 已隔离 IP 列表 ──");
        for (const auto &r: qlist) {
            NZ_INFO("    {}  [{}]  score={:.0f}", r.ip_address, r.reason, r.threat_score);
        }
    }

    Core::dump_network_info();
    Core::dump_arp_table();

    {
        auto ipcn = Core::IpCn::lookup_self();
        if (ipcn.valid) {
            std::string loc = ipcn.country;
            if (!ipcn.province.empty()) loc += std::format(" {}", ipcn.province);
            if (!ipcn.city.empty()) loc += std::format(" {}", ipcn.city);
            NZ_INFO("  公网 IPv4:       {}  ({}, {})", ipcn.ip, loc, ipcn.isp);
        }
    }

    Core::Arena arena(128 * 1024);
    Core::AttackDetector detector;

    detector.load_rules((paths.rules_dir + "default.yaml").c_str());
    g_detector = &detector;

    Core::AlertManager alerter;
    alerter.set_dedup_window(10);

    Service::Notifier::instance().load_config_dir((paths.config_dir + "notifier").c_str());

    alerter.set_callback([&](const Core::Alert &a) {
        Service::Notifier::instance().on_alert(a);

        if (a.level >= Severity::Error && a.count >= 5) {
            std::string ip(a.src_ip);
            std::string host = IPAddress::ipaddr::ResolveHostname(ip);

            if (host != ip)
                NZ_WARN("[聚合] {}  {} ({}), {} 次, 评分 {:.0f}",
                    attack_type_cstr(a.type), ip, host, a.count, a.score);
            else
                NZ_WARN("[聚合] {}  {}, {} 次, 评分 {:.0f}", attack_type_cstr(a.type), ip, a.count, a.score);
        }
    });

    Core::PacketCapture cap;
    if (cap.open(get_net_interface(), 65535, true, 1000)) {
        NZ_INFO("  ✓ 抓包引擎: {}", get_net_interface());
        cap.set_filter("tcp or udp or icmp");
    } else {
        NZ_WARN("  ✗ 抓包引擎启动失败 (需 root 权限)");
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
    NZ_INFO("  ✓ 蜜罐引擎: {} ports", std::size(honeypots));

    Core::LogWatcher log_watcher;
    Core::ApplicationMonitor app_monitor;
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
    NZ_INFO("  ✓ 日志引擎: {} sources", sizeof(log_paths) / sizeof(log_paths[0]));

    if constexpr (Configuration::ApplicationConstants::ShowOtherApplicationLogs) {
        int n = app_monitor.load_from_file((paths.config_dir + "monitor_apps.conf").c_str());
        if (n > 0) {
            app_monitor.start();
            NZ_INFO("  ✓ 应用监控: {} apps", n);
        } else {
            NZ_INFO("  - 应用监控: no config, skipped");
        }
    }
    std::signal(SIGINT, on_signal);
    std::signal(SIGTERM, on_signal);
    std::signal(SIGHUP, on_sighup);

    if (cap.is_open()) {
        cap.start([&](const std::uint8_t *raw, std::size_t len, const timeval &ts) {
            static uint64_t pkt_count = 0;
            static Nanos last_stats = 0;

            ++pkt_count;

            Core::event e{};
            if (!Core::ProtocolDecoder::decode(raw, len, ts, arena, e)) return;
            {
                const bool http = !e.msg.empty() && e.proto == PROTO_TCP;
                Core::ProtocolStats::instance().record_packet(e.proto, len, http);
            }

            if (Database::DatabaseHelper::IsIPQuarantined(e.src.to_string())) {
                static std::unordered_map<std::string, Nanos> last_warn;
                const Nanos now_ns = static_cast<Nanos>(ts.tv_sec) * 1'000'000'000ULL + ts.tv_usec * 1000ULL;

                const auto ip = e.src.to_string();

                if (const auto it = last_warn.find(ip);
                    it == last_warn.end() || (now_ns - it->second) > 10'000'000'000ULL) {
                    log_quarantine_block(ip, e.dst.to_string(), e.proto, e.dport, e.sport);
                    last_warn[ip] = now_ns;
                }

                Core::ActiveResponse::send_icmp_unreachable(e.dst.to_string(), ip, raw, len);

                return;
            }

            if (tor_checker.is_tor_exit(e.src.to_string())) {
                NZ_WARN("[Tor] 检测到 Tor 出口节点流量: {} → {}", e.src.to_string(), e.dst.to_string());
            }

            detector.analyze(e, arena, [&](const Core::Alert &a) { alerter.submit(a); });

            if (e.proto == PROTO_ICMP) {
                NZ_DEBUG("[ICMP] {} → {}  len={}B", e.src.to_string(), e.dst.to_string(), len);
            } else if (e.proto == PROTO_TCP) {
                NZ_TRACE("[TCP] {}:{} → {}:{}  len={}B  payload={}",
                         e.src.to_string(), e.sport,
                         e.dst.to_string(), e.dport, len, e.msg.size());
            } else {
                NZ_TRACE("[UDP] {}:{} → {}:{}  len={}B",
                         e.src.to_string(), e.sport,
                         e.dst.to_string(), e.dport, len);
            }

            static Nanos last_flush = 0;
            if (e.ts_ns - last_flush > 30'000'000'000ULL) {
                alerter.flush();
                last_flush = e.ts_ns;
            }

            if (last_stats == 0) last_stats = e.ts_ns;

            if (e.ts_ns - last_stats > 60'000'000'000ULL) {
                const auto ql = Database::DatabaseHelper::GetQuarantineList();
                auto s = Core::ProtocolStats::instance().snapshot();

                const double elapsed = (e.ts_ns - last_stats) / 1'000'000'000.0;
                auto arp_count = Core::arp_table_size();

                double mbps = (s.total_bytes * 8.0) / (elapsed * 1'000'000.0);

                NZ_INFO(
                    "◆ STATS | pkts:{} flow:{:.1f}MB rate:{:.0f}pps/{:.2f}Mbps | TCP:{} UDP:{} ICMP:{} HTTP:{} | alerts:{} quar:{} arp:{} tor:{} rules:{} arena:{}KB",
                    s.total_packets, s.total_bytes / 1'000'000.0,
                    s.total_packets / elapsed, mbps,
                    s.tcp_pkts, s.udp_pkts, s.icmp_pkts, s.http_pkts,
                    alerter.total_alerts(), ql.size(), arp_count,
                    tor_checker.total_nodes(), detector.rule_count(),
                    arena.bytes_used() / 1024
                );

                Core::ProtocolStats::instance().reset();
                pkt_count = 0;
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
    if constexpr (Configuration::ApplicationConstants::ShowOtherApplicationLogs) {
        app_monitor.stop();
    }
    alerter.flush();
    Log::Logger::instance().flush();
    auto final_qlist = Database::DatabaseHelper::GetQuarantineList();
    auto arp_count = Core::arp_table_size();
    struct rusage ru{};
    getrusage(RUSAGE_SELF, &ru);
    NZ_INFO("");
    NZ_INFO("╔══════════════════════════════════════════════════════════════╗");
    NZ_INFO("║  哪吒网络安全 SIEM — 运行摘要                               ║");
    NZ_INFO("╠══════════════════════════════════════════════════════════════╣");
    NZ_INFO("║  Alerts: {:<5}  |  Quarantined: {:<3}  |  Tor: {:<5}  |  ARP: {:<3}        ║",
            alerter.total_alerts(), final_qlist.size(), tor_checker.total_nodes(), arp_count);
    NZ_INFO("║  Rules: {:<5}   |  Arena: {:<4}KB   |  CPU user: {:.2f}s  sys: {:.2f}s  ║",
            detector.rule_count(), arena.bytes_used() / 1024,
            ru.ru_utime.tv_sec + ru.ru_utime.tv_usec / 1'000'000.0,
            ru.ru_stime.tv_sec + ru.ru_stime.tv_usec / 1'000'000.0);
#if defined(__APPLE__)
    NZ_INFO("║  RSS: {} MB                                                  ║", ru.ru_maxrss / (1024 * 1024));
#else
    NZ_INFO("║  RSS: {} MB                                                  ║", ru.ru_maxrss / 1024);
#endif
    NZ_INFO("╚══════════════════════════════════════════════════════════════╝");
    if (!final_qlist.empty()) {
        NZ_INFO("  ◆ Quarantined IPs:");
        for (const auto &r: final_qlist)
            NZ_INFO("    {}  [{}]  score={:.0f}", r.ip_address, r.reason, r.threat_score);
    }
    NZ_INFO("  Log: logs/nezha.log  |  DB: data/nezha_quarantine.db");
    return 0;
}


#ifndef NEZHAGUARD_CLI_ONLY
#include <QApplication>
#include <QDateTime>
#include <QString>

#include "src/views/monitor.h"
#include "src/views/gui_sink.h"
#include "src/views/log_model.h"

static int run_gui_mode(int argc, char *argv[]) {
    const char *log_paths[] = {
        "/var/log/nginx/access.log",
        "/var/log/apache2/access.log",
        "/var/log/auth.log",
        "/var/log/syslog",
    };

    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("哪吒网络安全 SIEM"));
    QApplication::setApplicationVersion(
        QString::fromLatin1(Configuration::ApplicationConstants::ApplicationVersion)
    );

    qputenv("QT_LOGGING_RULES", "qt.*=false");

    auto paths = resolve_app_paths();
    ensure_dirs(paths);
    g_paths = paths;

    Log::init_default(paths.log_dir + "nezha.log", Log::Level::Info);
    Database::DatabaseHelper::InitializeQuarantineDatabase(paths.data_dir);
    Core::TorChecker tor_checker;

    tor_checker.initialize();
    NZ_INFO("  哪吒网络安全 SIEM 系统 {} [蓝队模式]",
            Configuration::ApplicationConstants::ApplicationVersion
    );

    NZ_INFO("  触发阈值: {} 次  |  Tor 节点: {}  |  小黑屋住户: {} 位",
            Configuration::ApplicationConstants::AnomaliesQuarantineThreshold,
            tor_checker.total_nodes(),
            Database::DatabaseHelper::GetQuarantineList().size()
    );

    Core::dump_network_info();

    {
        if (auto ipcn = Core::IpCn::lookup_self(); ipcn.valid) {
            std::string loc = ipcn.country;
            if (!ipcn.province.empty()) loc += std::format(" {}", ipcn.province);
            if (!ipcn.city.empty()) loc += std::format(" {}", ipcn.city);
            NZ_INFO("  公网 IPv4:       {}  ({}, {})", ipcn.ip, loc, ipcn.isp);
        }
    }

    monitor window;
    window.init_models();
    window.showMaximized();

    if (auto sink = window.gui_sink()) Log::Logger::instance().add_sink(sink);


    Core::Arena arena(128 * 1024);
    Core::AttackDetector detector;
    detector.load_rules(paths.rules_dir + "default.yaml");
    g_detector = &detector;
    Core::AlertManager alerter;
    alerter.set_dedup_window(10);

    Service::Notifier::instance().load_config_dir(paths.config_dir + "notifier");
    Service::Notifier::instance().set_gui_callback([](const std::string &title, const std::string &body) {
#if defined(__APPLE__)
        const std::string cmd = std::format(
            R"(osascript -e 'display notification "{}" with title "{}" sound name "Glass"' 2>/dev/null)",
            body, title);
        std::system(cmd.c_str());
#elif defined(__linux__)
        std::string cmd = std::format("notify-send '{}' '{}' 2>/dev/null", title, body);
        std::system(cmd.c_str());
#endif
    });

    alerter.set_callback([&](const Core::Alert &a) {
        Service::Notifier::instance().on_alert(a);
        if (a.level >= Severity::Error && a.count >= 5) {
            std::string ip(a.src_ip);

            if (std::string host = IPAddress::ipaddr::ResolveHostname(ip); host != ip) {
                NZ_WARN("[攻击] {} 来自 {} ({}) | 频次: {} | 评分: {:.0f}/100 | 主机名: {}",
                        attack_type_cstr(a.type), ip, host, a.count, a.score, host);
            } else {
                NZ_WARN("[攻击] {} 来自 {} | 频次: {} | 评分: {:.0f}/100",
                        attack_type_cstr(a.type), ip, a.count, a.score);
            }
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

        QMetaObject::invokeMethod(&window, [window = &window, ts_ns, type_str, ip_str, cnt, sc, sev, type = a.type]() {
            auto ns = static_cast<time_t>(ts_ns / 1'000'000'000ULL);
            char ts[16];
            std::strftime(ts, sizeof(ts), "%H:%M:%S", std::localtime(&ns));
            auto qIp = QString::fromUtf8(ip_str.c_str());
            auto qType = QString::fromUtf8(type_str.c_str());
            window->append_alert(
                QString::fromUtf8(ts), qType, qIp, cnt, sc, sev);
            window->record_attacker(qIp, sc, qType);
            // ICMP/scan alerts also show in honeypot tab
            if (type == Core::AttackType::PortScan || type == Core::AttackType::Scanner) {
                auto now = QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss.zzz"));
                window->append_honeypot(now, qIp, 0, 0, QStringLiteral("ICMP 扫描"));
            }
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
        {.port = 9000, .proto = PROTO_TCP, .service = "QuestDB"},
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
                auto honeytime = QDateTime::fromMSecsSinceEpoch(e.ts_ns / 1'000'000, Qt::LocalTime)
                                     .toString(QStringLiteral("yyyy-MM-dd HH:mm:ss.zzz"));
                window.append_honeypot(
                    honeytime,
                    QString::fromStdString(e.src.to_string()),
                    e.sport, e.dport,
                    hp ? QString::fromUtf8(hp->service) : QStringLiteral("?"));
            }, Qt::QueuedConnection);
        });
    });

    NZ_INFO("蜜罐引擎: {} 个诱饵端口已就绪", std::size(honeypots));

    Core::LogWatcher log_watcher;
    Core::ApplicationMonitor app_monitor;

    for (const char *path: log_paths) {
        log_watcher.add_source({path, EventSource::Log, 0});
    }

    std::thread log_thread([&]() {
        log_watcher.start(arena, [&](const Core::event &e) {
            detector.analyze(e, arena, [&](const Core::Alert &a) { alerter.submit(a); });
        });
    });
    NZ_INFO("日志引擎: {} 个监控源已启动", sizeof(log_paths) / sizeof(log_paths[0]));

    if constexpr (Configuration::ApplicationConstants::ShowOtherApplicationLogs) {
        int n = app_monitor.load_from_file(paths.config_dir + "monitor_apps.conf");
        if (n > 0) {
            app_monitor.start();
            NZ_INFO("应用监控已启动: {} 个外部应用", n);
        } else {
            NZ_INFO("应用监控: 未找到配置 (config/monitor_apps.conf), 跳过");
        }
    }

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
                {
                    const bool http = !e.msg.empty() && e.proto == PROTO_TCP;
                    Core::ProtocolStats::instance().record_packet(e.proto, len, http);
                }

                if (Database::DatabaseHelper::IsIPQuarantined(e.src.to_string())) {
                    static std::unordered_map<std::string, Nanos> last_warn;
                    const Nanos now_ns = static_cast<Nanos>(ts.tv_sec) * 1'000'000'000ULL + ts.tv_usec * 1000ULL;

                    const auto ip = e.src.to_string();
                    const auto it = last_warn.find(ip);

                    if (it == last_warn.end() || (now_ns - it->second) > 10'000'000'000ULL) {
                        log_quarantine_block(ip, e.dst.to_string(), e.proto, e.dport, e.sport);
                        last_warn[ip] = now_ns;
                    }

                    Core::ActiveResponse::send_icmp_unreachable(e.dst.to_string(), ip, raw, len);

                    return;
                }

                detector.analyze(e, arena, [&](const Core::Alert &a) { alerter.submit(a); });

                if (e.proto == PROTO_ICMP) {
                    NZ_DEBUG("ICMP {} -> {}", e.src.to_string(), e.dst.to_string());
                } else {
                    NZ_TRACE("{} {}:{} -> :{}", e.proto == PROTO_TCP ? "TCP" : "UDP",
                             e.src.to_string(), e.sport, e.dport);
                }

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

    // 定时刷新告警 + 更新统计 (2s interval to reduce GUI thread pressure)
    QTimer *flush_timer = new QTimer(&window);
    QObject::connect(flush_timer, &QTimer::timeout, [&]() {
        alerter.flush();
        window.update_stats(window.log_model()->total(), alerter.total_alerts());
    });

    flush_timer->start(2000);

    // 信号 → 优雅退出
    std::signal(SIGINT, [](int) { QApplication::quit(); });
    std::signal(SIGTERM, [](int) { QApplication::quit(); });
    std::signal(SIGHUP, on_sighup);

    int ret = app.exec();

    NZ_INFO("正在停止所有引擎…");
    g_running = false;

    if (g_cap) g_cap->stop();
    if (cap_thread.joinable()) cap_thread.join();

    honeypot.stop();

    if (honey_thread.joinable()) honey_thread.join();
    log_watcher.stop();

    if (log_thread.joinable()) log_thread.join();
    if constexpr (Configuration::ApplicationConstants::ShowOtherApplicationLogs) {
        app_monitor.stop();
    }
    alerter.flush();

    Log::Logger::instance().flush();
    NZ_INFO("SIEM 已停止  共处理 {} 条告警", alerter.total_alerts());

    return ret;
}

#endif // NEZHAGUARD_CLI_ONLY


int main(const int argc, char *argv[]) {
    bool no_gui = false;

    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "-v") == 0 || std::strcmp(argv[i], "--verbose") == 0) {
            Log::Logger::instance().set_level(Log::Level::Debug);
        } else if (std::strcmp(argv[i], "--no-gui") == 0) {
            no_gui = true;
        }
    }

#ifdef NEZHAGUARD_CLI_ONLY
    (void) argc; (void) no_gui;
    return run_cli_mode();
#else
    if (no_gui) {
        return run_cli_mode();
    }

    bool show_gui = Configuration::ApplicationConstants::ShowGui;

    if (const char *gui_env = std::getenv("NEZHA_SHOW_GUI")) {
        show_gui = (std::strcmp(gui_env, "0") != 0 && std::strcmp(gui_env, "false") != 0);
    }

    if (show_gui) return run_gui_mode(argc, argv);

    return run_cli_mode();
#endif
}
