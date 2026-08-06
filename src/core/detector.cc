//
// Created by 钟智强 on 2026/7/30.
//

#include "detector.h"
#include "arena.h"
#include "rule_loader.h"
#include "../contants.h"
#include "../service/database_helper.h"
#include "../utilities/logger.h"
#include <algorithm>
#include <cstring>

namespace Nezha::Core {
    const char *attack_type_cstr(AttackType t) noexcept {
        switch (t) {
            case AttackType::SQLi: return "SQL注入";
            case AttackType::XSS: return "XSS";
            case AttackType::PathTraversal: return "路径穿越";
            case AttackType::CmdInjection: return "命令注入";
            case AttackType::FileInclusion: return "文件包含";
            case AttackType::Scanner: return "扫描探测";
            case AttackType::BruteForce: return "暴力破解";
            case AttackType::PortScan: return "端口扫描";
            case AttackType::Webshell: return "Webshell";
            case AttackType::Log4j: return "Log4j漏洞";
            case AttackType::BotActivity: return "恶意爬虫";
            case AttackType::RateAnomaly: return "速率异常";
            case AttackType::UnknownMal: return "可疑行为";
        }
        return "?";
    }

    static const SigRule kSignatures[] = {
        {AttackType::SQLi, Severity::Critical, 90.0, "UNION SELECT", "联合查询注入"},
        {AttackType::SQLi, Severity::Critical, 90.0, "UNION ALL SELECT", "联合查询注入"},
        {AttackType::SQLi, Severity::Critical, 85.0, "INFORMATION_SCHEMA", "信息模式探测"},
        {AttackType::SQLi, Severity::Critical, 80.0, "SLEEP(", "时间盲注"},
        {AttackType::SQLi, Severity::Critical, 80.0, "BENCHMARK(", "时间盲注"},
        {AttackType::SQLi, Severity::Error, 70.0, "' OR 1=1", "条件永真注入"},
        {AttackType::SQLi, Severity::Error, 70.0, "\" OR 1=1", "条件永真注入"},
        {AttackType::SQLi, Severity::Error, 70.0, "' OR '1'='1", "条件永真注入"},
        {AttackType::SQLi, Severity::Error, 75.0, "'; DROP TABLE", "删表注入"},
        {AttackType::SQLi, Severity::Error, 70.0, "SELECT * FROM", "直接查询注入"},
        {AttackType::SQLi, Severity::Error, 65.0, "WAITFOR DELAY", "时间盲注(MS)"},
        {AttackType::SQLi, Severity::Warn, 60.0, "pg_sleep", "PostgreSQL 盲注"},
        {AttackType::SQLi, Severity::Warn, 60.0, "DBMS_PIPE.RECEIVE", "Oracle 盲注"},
        {AttackType::SQLi, Severity::Warn, 55.0, "ORDER BY ", "列数探测"},
        {AttackType::SQLi, Severity::Warn, 55.0, "GROUP BY ", "列数探测"},

        {AttackType::XSS, Severity::Critical, 85.0, "<script", "脚本标签注入"},
        {AttackType::XSS, Severity::Error, 80.0, "javascript:", "JS 伪协议"},
        {AttackType::XSS, Severity::Error, 80.0, "onerror=", "事件处理器注入"},
        {AttackType::XSS, Severity::Error, 80.0, "onload=", "事件处理器注入"},
        {AttackType::XSS, Severity::Warn, 75.0, "<img", "可疑图片标签"},
        {AttackType::XSS, Severity::Warn, 75.0, "<iframe", "内嵌框架注入"},
        {AttackType::XSS, Severity::Warn, 75.0, "<svg", "SVG 注入"},
        {AttackType::XSS, Severity::Warn, 70.0, "alert(", "弹窗测试"},
        {AttackType::XSS, Severity::Warn, 70.0, "document.cookie", "Cookie 窃取"},

        {AttackType::PathTraversal, Severity::Critical, 90.0, "/etc/passwd", "系统密码文件"},
        {AttackType::PathTraversal, Severity::Critical, 85.0, "/etc/shadow", "系统密码文件"},
        {AttackType::PathTraversal, Severity::Critical, 85.0, "C:\\Windows\\", "Windows 系统路径"},
        {AttackType::PathTraversal, Severity::Error, 80.0, "....//", "路径穿越变体"},
        {AttackType::PathTraversal, Severity::Error, 80.0, "..;/", "路径穿越变体"},
        {AttackType::PathTraversal, Severity::Error, 75.0, "../" "../" "../", "多层穿越"},
        {AttackType::PathTraversal, Severity::Warn, 70.0, "WEB-INF", "Java Web目录探针"},
        {AttackType::PathTraversal, Severity::Warn, 70.0, "wp-config.php", "WordPress 配置"},

        {AttackType::CmdInjection, Severity::Critical, 95.0, ";wget", "命令注入下载"},
        {AttackType::CmdInjection, Severity::Critical, 95.0, ";curl", "命令注入下载"},
        {AttackType::CmdInjection, Severity::Critical, 90.0, "$(whoami)", "命令替换探测"},
        {AttackType::CmdInjection, Severity::Critical, 90.0, "`id`", "命令替换探测"},
        {AttackType::CmdInjection, Severity::Error, 85.0, "| /bin/bash", "管道执行"},
        {AttackType::CmdInjection, Severity::Error, 85.0, "/bin/sh -c", "Shell 执行"},
        {AttackType::CmdInjection, Severity::Error, 85.0, "cmd.exe /c", "Windows 命令执行"},
        {AttackType::CmdInjection, Severity::Error, 80.0, "&& cat ", "命令拼接"},
        {AttackType::CmdInjection, Severity::Warn, 75.0, "/dev/tcp/", "反向 Shell"},

        {AttackType::FileInclusion, Severity::Critical, 85.0, "=http://", "远程文件包含"},
        {AttackType::FileInclusion, Severity::Critical, 85.0, "=https://", "远程文件包含"},
        {AttackType::FileInclusion, Severity::Error, 80.0, "php://input", "PHP 输入流"},
        {AttackType::FileInclusion, Severity::Error, 80.0, "php://filter", "PHP 过滤器"},
        {AttackType::FileInclusion, Severity::Error, 80.0, "expect://", "Expect 封装器"},
        {AttackType::FileInclusion, Severity::Error, 80.0, "data://", "Data URI 封装器"},

        {AttackType::Scanner, Severity::Error, 75.0, "nmap", "Nmap 扫描器"},
        {AttackType::Scanner, Severity::Error, 75.0, "sqlmap", "SQLMap 注入工具"},
        {AttackType::Scanner, Severity::Error, 75.0, "nikto", "Nikto 扫描器"},
        {AttackType::Scanner, Severity::Error, 75.0, "nessus", "Nessus 扫描器"},
        {AttackType::Scanner, Severity::Error, 75.0, "burpsuite", "Burp Suite"},
        {AttackType::Scanner, Severity::Error, 70.0, "acunetix", "Acunetix 扫描器"},
        {AttackType::Scanner, Severity::Error, 70.0, "gobuster", "目录爆破"},
        {AttackType::Scanner, Severity::Error, 70.0, "dirbuster", "目录爆破"},
        {AttackType::Scanner, Severity::Warn, 65.0, "masscan", "Masscan 扫描"},
        {AttackType::Scanner, Severity::Warn, 65.0, "zgrab", "Zgrab 扫描"},
        {AttackType::Scanner, Severity::Warn, 60.0, "python-requests", "自动化脚本"},
        {AttackType::Scanner, Severity::Warn, 60.0, "Go-http-client", "Go HTTP 客户端"},
        {AttackType::Scanner, Severity::Warn, 60.0, "curl/", "Curl 自动化"},

        {AttackType::Webshell, Severity::Critical, 95.0, "eval(base64_decode", "PHP 编码执行"},
        {AttackType::Webshell, Severity::Critical, 90.0, "system($_", "PHP 系统执行"},
        {AttackType::Webshell, Severity::Critical, 90.0, "exec($_", "PHP 命令执行"},
        {AttackType::Webshell, Severity::Critical, 90.0, "shell_exec", "PHP Shell 执行"},
        {AttackType::Webshell, Severity::Error, 85.0, "assert($_", "PHP 断言执行"},
        {AttackType::Webshell, Severity::Error, 85.0, "preg_replace", "PHP 正则执行"},
        {AttackType::Webshell, Severity::Error, 85.0, ".php?cmd=", "Webshell 访问"},
        {AttackType::Webshell, Severity::Warn, 80.0, "wso.php", "WSO Webshell"},
        {AttackType::Webshell, Severity::Warn, 80.0, "c99.php", "C99 Webshell"},

        {AttackType::Log4j, Severity::Critical, 95.0, "${jndi:ldap://", "Log4j JNDI 注入"},
        {AttackType::Log4j, Severity::Critical, 95.0, "${jndi:ldaps://", "Log4j JNDI 注入"},
        {AttackType::Log4j, Severity::Critical, 95.0, "${jndi:rmi://", "Log4j JNDI 注入"},
        {AttackType::Log4j, Severity::Critical, 95.0, "${jndi:dns://", "Log4j JNDI 注入"},
        {AttackType::Log4j, Severity::Error, 90.0, "${${::-j}${::-n}", "Log4j 混淆绕过"},

        {AttackType::BotActivity, Severity::Warn, 60.0, "AhrefsBot", "SEO 爬虫"},
        {AttackType::BotActivity, Severity::Warn, 60.0, "SemrushBot", "SEO 爬虫"},
        {AttackType::BotActivity, Severity::Warn, 60.0, "DotBot", "恶意爬虫"},

        {AttackType::Scanner, Severity::Error, 75.0, "wp-login.php", "WordPress 登录探测"},
        {AttackType::Scanner, Severity::Error, 75.0, "xmlrpc.php", "WordPress XMLRPC 攻击"},
        {AttackType::Scanner, Severity::Warn, 70.0, "wp-admin", "WordPress 后台探测"},
        {AttackType::Scanner, Severity::Warn, 65.0, "wp-content", "WordPress 路径探测"},
        {AttackType::Scanner, Severity::Warn, 65.0, "wp-includes", "WordPress 路径探测"},

        {AttackType::Scanner, Severity::Error, 75.0, ".env", ".env 文件探测"},
        {AttackType::Scanner, Severity::Error, 75.0, ".git/config", "Git 配置泄露"},
        {AttackType::Scanner, Severity::Warn, 70.0, "actuator", "Spring Boot 端点探测"},
        {AttackType::Scanner, Severity::Warn, 70.0, "jmx-console", "JMX 控制台探测"},
        {AttackType::Scanner, Severity::Warn, 70.0, "phpinfo.php", "PHP 信息泄露"},
        {AttackType::Scanner, Severity::Warn, 65.0, "robots.txt", "爬虫规则探测"},
        {AttackType::Scanner, Severity::Warn, 65.0, "sitemap.xml", "站点地图探测"},

        {AttackType::CmdInjection, Severity::Error, 80.0, "169.254.169.254", "AWS 元数据 SSRF"},
        {AttackType::CmdInjection, Severity::Error, 80.0, "metadata.google.internal", "GCP 元数据 SSRF"},
    };
    namespace {
        // 基于规则集的签名匹配，返回最佳匹配规则指针（指向 rules 中的元素）
        const SigRule *match_signature(const std::vector<SigRule> &rules,
                                       std::string_view msg, std::string_view ua) {
            const SigRule *best = nullptr;
            double best_score = 0.0;

            auto try_match = [&](std::string_view target, bool in_ua) {
                for (const auto &r : rules) {
                    if (r.type == AttackType::BotActivity && !in_ua) continue;
                    if (r.type == AttackType::Scanner && !in_ua
                        && r.pattern.starts_with("curl"))
                        continue;

                    auto it = std::search(
                        target.begin(), target.end(),
                        r.pattern.begin(), r.pattern.end(),
                        [](char a, char b) { return std::tolower(a) == std::tolower(b); }
                    );
                    if (it != target.end() && r.score > best_score) {
                        best = &r;
                        best_score = r.score;
                    }
                }
            };

            if (!msg.empty()) try_match(msg, false);
            if (!ua.empty()) try_match(ua, true);
            return best;
        }
    }

    AttackDetector::IpPortKey AttackDetector::make_key(const event &e) noexcept {
        const auto &b = e.src.bytes();
        std::uint64_t h = 0;
        for (int i = 0; i < 8; ++i) h = (h << 8) | b[i];
        return (h << 16) | (static_cast<std::uint64_t>(e.sport) & 0xFFFF);
    }

    void AttackDetector::expire_counters(Nanos now_ns) {
        if (now_ns - last_expire_ < 60'000'000'000ULL) return;
        last_expire_ = now_ns;

        for (auto it = rates_.begin(); it != rates_.end();) {
            if (now_ns - it->second.last_ns > 300'000'000'000ULL) {
                it = rates_.erase(it);
            } else {
                ++it;
            }
        }
    }

    AttackDetector::AttackDetector() {
        // 从编译期内置签名初始化规则表
        for (const auto &r : kSignatures)
            rules_.push_back(r);
    }

    bool AttackDetector::load_rules(const std::string &path) {
        RuleLoader loader;
        auto result = loader.load_from_file(path);
        if (!result) {
            NZ_WARN("规则加载失败: {} (保留现有 {} 条规则)",
                    rule_load_error_str(result.error()), rules_.size());
            return false;
        }
        if (result->empty()) {
            NZ_WARN("规则文件为空，保留现有 {} 条规则", rules_.size());
            return false;
        }
        rules_ = std::move(*result);
        NZ_INFO("规则热加载完成: {} 条", rules_.size());
        return true;
    }

    void AttackDetector::reload_rules(const std::string &path) {
        load_rules(path);
    }

    void AttackDetector::analyze(const event &e, Arena &arena, AlertCallback cb) {
        if (!cb) return;
        Nanos now = e.ts_ns
                        ? e.ts_ns
                        : std::chrono::duration_cast<std::chrono::nanoseconds>(
                            std::chrono::system_clock::now().time_since_epoch()).count();

        std::string_view ua;
        const FieldVal *ua_val = e.fields.get(5);
        if (ua_val && ua_val->kind == FieldVal::Kind::Str) ua = ua_val->s;

        const SigRule *rule = match_signature(rules_, e.msg, ua);
        if (rule) {
            Alert a{};
            a.type = rule->type;
            a.level = rule->level;
            a.score = rule->score;
            a.ts_ns = now;
            a.evidence = arena.intern(rule->pattern);
            a.detail = arena.intern(rule->desc);
            a.src_ip = arena.intern(e.src.to_string());
            cb(a);
            return;
        }

        const FieldVal *status_val = e.fields.get(4);
        if (status_val && status_val->kind == FieldVal::Kind::Int) {
            auto status = status_val->i;
            if (status >= 500) {
                Alert a{};
                a.type = AttackType::UnknownMal;
                a.level = Severity::Warn;
                a.score = 40.0;
                a.ts_ns = now;
                a.detail = arena.intern("5xx 服务端错误");
                a.src_ip = arena.intern(e.src.to_string());
                cb(a);
                return;
            }
        }

        IpPortKey key = make_key(e);
        auto &entry = rates_[key];
        entry.count++;
        entry.last_ns = now;

        if (e.proto == PROTO_ICMP) {
            if ((entry.count == 3) || (entry.count > 3 && entry.count % 5 == 0)) {
                Alert a{};
                a.type = AttackType::PortScan;
                a.level = entry.count > 100
                              ? Severity::Critical
                              : entry.count > 50
                                    ? Severity::Error
                                    : entry.count > 20
                                          ? Severity::Warn
                                          : Severity::Info;
                a.score = std::min(95.0, 40.0 + entry.count * 0.8);
                a.ts_ns = now;
                a.count = entry.count;
                a.detail = arena.intern("ICMP Ping 洪流");
                a.src_ip = arena.intern(e.src.to_string());
                cb(a);
            }

            constexpr auto kThreshold = Configuration::ApplicationConstants::AnomaliesQuarantineThreshold;
            if (entry.count >= static_cast<uint32_t>(kThreshold)) {
                std::string ip = e.src.to_string();
                if (!Database::DatabaseHelper::IsIPQuarantined(ip)) {
                    double qscore = std::min(95.0, 40.0 + entry.count * 0.8);
                    Database::DatabaseHelper::QuarantineIP(
                        ip, "ICMP 异常流量超过阈值", qscore);
                }
            }
        } else if (entry.count > 100 && entry.count % 50 == 0) {
            Alert a{};
            a.type = AttackType::RateAnomaly;
            a.level = entry.count > 500 ? Severity::Critical : entry.count > 200 ? Severity::Error : Severity::Warn;
            a.score = std::min(95.0, 50.0 + entry.count * 0.1);
            a.ts_ns = now;
            a.count = entry.count;
            a.detail = arena.intern("高频访问");
            a.src_ip = arena.intern(e.src.to_string());
            cb(a);
        }

        expire_counters(now);
    }
}
