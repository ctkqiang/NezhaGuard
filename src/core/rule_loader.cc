//
// Created by 钟智强 on 2026/8/1.
//

#include "rule_loader.h"
#include "../utilities/logger.h"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <fstream>
#include <sstream>

namespace Nezha::Core {

    const char *rule_load_error_str(RuleLoadError e) noexcept {
        switch (e) {
            case RuleLoadError::FileNotFound:   return "规则文件未找到";
            case RuleLoadError::ParseError:      return "规则文件解析错误";
            case RuleLoadError::EmptyRules:      return "规则文件为空";
            case RuleLoadError::InvalidType:     return "无效的攻击类型";
            case RuleLoadError::InvalidSeverity: return "无效的严重级别";
            case RuleLoadError::InvalidScore:    return "无效的评分值";
        }
        return "未知错误";
    }

    // 从带引号的字符串中提取值: key: "value" → "value"
    static std::string extract_quoted(std::string_view line, const char *key) {
        std::string search = std::string(key) + ": \"";
        auto pos = line.find(search);
        if (pos == std::string_view::npos) {
            // try unquoted: key: value
            search = std::string(key) + ": ";
            pos = line.find(search);
            if (pos == std::string_view::npos) return {};
            pos += search.size();
            auto end = line.find_first_of(",}\n", pos);
            if (end == std::string_view::npos) end = line.size();
            auto val = line.substr(pos, end - pos);
            // trim
            while (!val.empty() && val.front() == ' ') val.remove_prefix(1);
            while (!val.empty() && val.back() == ' ') val.remove_suffix(1);
            return std::string(val);
        }
        pos += search.size();
        auto end = line.find('"', pos);
        if (end == std::string_view::npos) return {};
        return std::string(line.substr(pos, end - pos));
    }

    // 解析 attack type 字符串 → AttackType 枚举
    static std::expected<AttackType, RuleLoadError> parse_type(std::string_view s) {
        if (s == "SQLi")            return AttackType::SQLi;
        if (s == "XSS")             return AttackType::XSS;
        if (s == "PathTraversal")   return AttackType::PathTraversal;
        if (s == "CmdInjection")    return AttackType::CmdInjection;
        if (s == "FileInclusion")   return AttackType::FileInclusion;
        if (s == "Scanner")         return AttackType::Scanner;
        if (s == "BruteForce")      return AttackType::BruteForce;
        if (s == "PortScan")        return AttackType::PortScan;
        if (s == "Webshell")        return AttackType::Webshell;
        if (s == "Log4j")           return AttackType::Log4j;
        if (s == "BotActivity")     return AttackType::BotActivity;
        if (s == "RateAnomaly")     return AttackType::RateAnomaly;
        if (s == "UnknownMal")      return AttackType::UnknownMal;
        return std::unexpected(RuleLoadError::InvalidType);
    }

    // 解析 severity 字符串 → Severity 枚举
    static std::expected<Severity, RuleLoadError> parse_severity(std::string_view s) {
        if (s == "Critical" || s == "CRITICAL") return Severity::Critical;
        if (s == "Error"    || s == "ERROR")    return Severity::Error;
        if (s == "Warn"     || s == "WARN")     return Severity::Warn;
        if (s == "Info"     || s == "INFO")     return Severity::Info;
        if (s == "Debug"    || s == "DEBUG")    return Severity::Debug;
        if (s == "Trace"    || s == "TRACE")    return Severity::Trace;
        if (s == "Warn"     || s == "WARN")     return Severity::Warn;
        return std::unexpected(RuleLoadError::InvalidSeverity);
    }

    std::expected<std::vector<SigRule>, RuleLoadError>
    RuleLoader::load_from_file(const std::string &path) {
        std::ifstream f(path);
        if (!f.is_open()) return std::unexpected(RuleLoadError::FileNotFound);

        std::stringstream buf;
        buf << f.rdbuf();
        return parse_yaml(buf.str());
    }

    std::expected<std::vector<SigRule>, RuleLoadError>
    RuleLoader::parse_yaml(const std::string &content) {
        std::vector<SigRule> rules;
        int line_no = 0;

        for (size_t pos = 0; pos < content.size(); ) {
            auto line_end = content.find('\n', pos);
            if (line_end == std::string::npos) line_end = content.size();
            auto line = std::string_view(content.data() + pos, line_end - pos);
            ++line_no;
            pos = line_end + 1;

            // 跳过注释和空行和非规则行
            auto trimmed = line;
            while (!trimmed.empty() && (trimmed.front() == ' ' || trimmed.front() == '\t'))
                trimmed.remove_prefix(1);
            if (trimmed.empty() || trimmed[0] == '#') continue;
            if (trimmed.find("- {") == std::string_view::npos) continue;

            // 提取各字段
            auto type_str = extract_quoted(trimmed, "type");
            auto sev_str  = extract_quoted(trimmed, "severity");
            auto pattern  = extract_quoted(trimmed, "pattern");
            auto desc     = extract_quoted(trimmed, "desc");
            auto score_str = extract_quoted(trimmed, "score");

            if (type_str.empty() || sev_str.empty() || pattern.empty() ||
                desc.empty() || score_str.empty()) {
                NZ_WARN("规则第 {} 行字段不完整，跳过", line_no);
                continue;
            }

            auto type = parse_type(type_str);
            if (!type) {
                NZ_WARN("规则第 {} 行未知攻击类型: {}", line_no, type_str);
                continue;
            }

            auto sev = parse_severity(sev_str);
            if (!sev) {
                NZ_WARN("规则第 {} 行未知严重级别: {}", line_no, sev_str);
                continue;
            }

            double score = std::strtod(score_str.c_str(), nullptr);
            if (score < 0.0 || score > 100.0) {
                NZ_WARN("规则第 {} 行评分超出范围: {}", line_no, score);
                continue;
            }

            SigRule rule{};
            rule.type = *type;
            rule.level = *sev;
            rule.score = score;
            rule.pattern = pattern;  // 字符串所有权在 YAML content 之外需要复制
            rule.desc = desc;
            rules.push_back(std::move(rule));
        }

        if (rules.empty()) return std::unexpected(RuleLoadError::EmptyRules);

        // 深拷贝 pattern/desc：YAML content 生命周期结束后 string_view 会悬空
        for (auto &r : rules) {
            r.pattern = std::string(r.pattern);
            r.desc = std::string(r.desc);
        }

        NZ_INFO("规则加载完成: {} 条 (来自文件)", rules.size());
        return rules;
    }

    const std::vector<SigRule> &RuleLoader::default_rules() {
        static const std::vector<SigRule> builtin = {
            // SQLi
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
            // XSS
            {AttackType::XSS, Severity::Critical, 85.0, "<script", "脚本标签注入"},
            {AttackType::XSS, Severity::Error, 80.0, "javascript:", "JS 伪协议"},
            {AttackType::XSS, Severity::Error, 80.0, "onerror=", "事件处理器注入"},
            {AttackType::XSS, Severity::Error, 80.0, "onload=", "事件处理器注入"},
            {AttackType::XSS, Severity::Warn, 75.0, "<img", "可疑图片标签"},
            {AttackType::XSS, Severity::Warn, 75.0, "<iframe", "内嵌框架注入"},
            {AttackType::XSS, Severity::Warn, 75.0, "<svg", "SVG 注入"},
            {AttackType::XSS, Severity::Warn, 70.0, "alert(", "弹窗测试"},
            {AttackType::XSS, Severity::Warn, 70.0, "document.cookie", "Cookie 窃取"},
            // PathTraversal
            {AttackType::PathTraversal, Severity::Critical, 90.0, "/etc/passwd", "系统密码文件"},
            {AttackType::PathTraversal, Severity::Critical, 85.0, "/etc/shadow", "系统密码文件"},
            {AttackType::PathTraversal, Severity::Critical, 85.0, "C:\\Windows\\", "Windows 系统路径"},
            {AttackType::PathTraversal, Severity::Error, 80.0, "....//", "路径穿越变体"},
            {AttackType::PathTraversal, Severity::Error, 80.0, "..;/", "路径穿越变体"},
            {AttackType::PathTraversal, Severity::Error, 75.0, "../" "../" "../", "多层穿越"},
            {AttackType::PathTraversal, Severity::Warn, 70.0, "WEB-INF", "Java Web目录探针"},
            {AttackType::PathTraversal, Severity::Warn, 70.0, "wp-config.php", "WordPress 配置"},
            // CmdInjection
            {AttackType::CmdInjection, Severity::Critical, 95.0, ";wget", "命令注入下载"},
            {AttackType::CmdInjection, Severity::Critical, 95.0, ";curl", "命令注入下载"},
            {AttackType::CmdInjection, Severity::Critical, 90.0, "$(whoami)", "命令替换探测"},
            {AttackType::CmdInjection, Severity::Critical, 90.0, "`id`", "命令替换探测"},
            {AttackType::CmdInjection, Severity::Error, 85.0, "| /bin/bash", "管道执行"},
            {AttackType::CmdInjection, Severity::Error, 85.0, "/bin/sh -c", "Shell 执行"},
            {AttackType::CmdInjection, Severity::Error, 85.0, "cmd.exe /c", "Windows 命令执行"},
            {AttackType::CmdInjection, Severity::Error, 80.0, "&& cat ", "命令拼接"},
            {AttackType::CmdInjection, Severity::Warn, 75.0, "/dev/tcp/", "反向 Shell"},
            {AttackType::CmdInjection, Severity::Error, 80.0, "169.254.169.254", "AWS 元数据 SSRF"},
            {AttackType::CmdInjection, Severity::Error, 80.0, "metadata.google.internal", "GCP 元数据 SSRF"},
            // FileInclusion
            {AttackType::FileInclusion, Severity::Critical, 85.0, "=http://", "远程文件包含"},
            {AttackType::FileInclusion, Severity::Critical, 85.0, "=https://", "远程文件包含"},
            {AttackType::FileInclusion, Severity::Error, 80.0, "php://input", "PHP 输入流"},
            {AttackType::FileInclusion, Severity::Error, 80.0, "php://filter", "PHP 过滤器"},
            {AttackType::FileInclusion, Severity::Error, 80.0, "expect://", "Expect 封装器"},
            {AttackType::FileInclusion, Severity::Error, 80.0, "data://", "Data URI 封装器"},
            // Scanner
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
            // Webshell
            {AttackType::Webshell, Severity::Critical, 95.0, "eval(base64_decode", "PHP 编码执行"},
            {AttackType::Webshell, Severity::Critical, 90.0, "system($_", "PHP 系统执行"},
            {AttackType::Webshell, Severity::Critical, 90.0, "exec($_", "PHP 命令执行"},
            {AttackType::Webshell, Severity::Critical, 90.0, "shell_exec", "PHP Shell 执行"},
            {AttackType::Webshell, Severity::Error, 85.0, "assert($_", "PHP 断言执行"},
            {AttackType::Webshell, Severity::Error, 85.0, "preg_replace", "PHP 正则执行"},
            {AttackType::Webshell, Severity::Error, 85.0, ".php?cmd=", "Webshell 访问"},
            {AttackType::Webshell, Severity::Warn, 80.0, "wso.php", "WSO Webshell"},
            {AttackType::Webshell, Severity::Warn, 80.0, "c99.php", "C99 Webshell"},
            // Log4j
            {AttackType::Log4j, Severity::Critical, 95.0, "${jndi:ldap://", "Log4j JNDI 注入"},
            {AttackType::Log4j, Severity::Critical, 95.0, "${jndi:ldaps://", "Log4j JNDI 注入"},
            {AttackType::Log4j, Severity::Critical, 95.0, "${jndi:rmi://", "Log4j JNDI 注入"},
            {AttackType::Log4j, Severity::Critical, 95.0, "${jndi:dns://", "Log4j JNDI 注入"},
            {AttackType::Log4j, Severity::Error, 90.0, "${${::-j}${::-n}", "Log4j 混淆绕过"},
            // BotActivity
            {AttackType::BotActivity, Severity::Warn, 60.0, "AhrefsBot", "SEO 爬虫"},
            {AttackType::BotActivity, Severity::Warn, 60.0, "SemrushBot", "SEO 爬虫"},
            {AttackType::BotActivity, Severity::Warn, 60.0, "DotBot", "恶意爬虫"},
        };
        return builtin;
    }

} // namespace Nezha::Core
