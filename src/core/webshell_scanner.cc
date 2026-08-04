//
// Created by 钟智强 on 2026/8/2.
//

#include "webshell_scanner.h"
#include "arena.h"
#include "../utilities/logger.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <format>
#include <fstream>
#include <sstream>

namespace Nezha::Core {
    namespace fs = std::filesystem;

    namespace {
        struct WsPattern {
            const char *pattern;
            const char *desc;
            double score;
            Severity level;
        };
    }

    static const WsPattern kPatterns[] = {
        {.pattern = "eval(", .desc = "eval 代码执行", .score = 90.0, .level = Severity::Critical},
        {.pattern = "assert(", .desc = "assert 代码执行", .score = 85.0, .level = Severity::Critical},
        {.pattern = "system(", .desc = "system 命令执行", .score = 90.0, .level = Severity::Critical},
        {.pattern = "exec(", .desc = "exec 命令执行", .score = 90.0, .level = Severity::Critical},
        {.pattern = "shell_exec(", .desc = "shell_exec 命令执行", .score = 90.0, .level = Severity::Critical},
        {.pattern = "passthru(", .desc = "passthru 命令执行", .score = 90.0, .level = Severity::Critical},
        {.pattern = "popen(", .desc = "popen 进程执行", .score = 85.0, .level = Severity::Error},
        {.pattern = "proc_open(", .desc = "proc_open 进程执行", .score = 85.0, .level = Severity::Error},
        {.pattern = "pcntl_exec(", .desc = "pcntl_exec 进程执行", .score = 85.0, .level = Severity::Error},
        {.pattern = "`$_", .desc = "反引号命令替换", .score = 85.0, .level = Severity::Error},
        {.pattern = "$(__", .desc = "命令替换混淆", .score = 80.0, .level = Severity::Error},
        {.pattern = "base64_decode(", .desc = "base64 解码执行", .score = 95.0, .level = Severity::Critical},
        {.pattern = "str_rot13(", .desc = "ROT13 解码", .score = 70.0, .level = Severity::Warn},
        {.pattern = "gzinflate(", .desc = "gzip 解压执行", .score = 80.0, .level = Severity::Error},
        {.pattern = "gzuncompress(", .desc = "gzip 解压", .score = 80.0, .level = Severity::Error},
        {.pattern = "convert_uudecode(", .desc = "UU 解码", .score = 60.0, .level = Severity::Warn},
        {.pattern = "file_get_contents(", .desc = "文件读取", .score = 60.0, .level = Severity::Warn},
        {.pattern = "file_put_contents(", .desc = "文件写入", .score = 75.0, .level = Severity::Error},
        {.pattern = "fopen(", .desc = "文件打开", .score = 55.0, .level = Severity::Warn},
        {.pattern = "fwrite(", .desc = "文件写入", .score = 70.0, .level = Severity::Warn},
        {.pattern = "unlink(", .desc = "文件删除", .score = 60.0, .level = Severity::Warn},
        {.pattern = "move_uploaded_file(", .desc = "上传文件移动", .score = 85.0, .level = Severity::Error},
        {.pattern = "phpinfo(", .desc = "PHP 信息泄露", .score = 65.0, .level = Severity::Warn},
        {.pattern = "get_current_user(", .desc = "当前用户探测", .score = 45.0, .level = Severity::Info},
        {.pattern = "getmyuid(", .desc = "UID 探测", .score = 45.0, .level = Severity::Info},
        {.pattern = "disk_free_space(", .desc = "磁盘探测", .score = 45.0, .level = Severity::Info},
        {.pattern = "fsockopen(", .desc = "socket 连接", .score = 75.0, .level = Severity::Error},
        {.pattern = "curl_exec(", .desc = "cURL 执行", .score = 70.0, .level = Severity::Warn},
        {.pattern = "stream_socket_client(", .desc = "流 socket", .score = 70.0, .level = Severity::Warn},
        {.pattern = "Runtime.getRuntime()", .desc = "Java 运行时执行", .score = 90.0, .level = Severity::Critical},
        {.pattern = "ProcessBuilder(", .desc = "Java 进程构建", .score = 85.0, .level = Severity::Critical},
        {.pattern = "ClassLoader(", .desc = "Java 类加载", .score = 80.0, .level = Severity::Error},
        {.pattern = "getServletContext(", .desc = "JSP Servlet 上下文", .score = 60.0, .level = Severity::Warn},
        {.pattern = "Server.CreateObject(", .desc = "ASP 对象创建", .score = 85.0, .level = Severity::Critical},
        {.pattern = "eval(request(", .desc = "ASP eval 请求", .score = 90.0, .level = Severity::Critical},
        {.pattern = "execute(request(", .desc = "ASP execute 请求", .score = 90.0, .level = Severity::Critical},
        {.pattern = "WScript.Shell", .desc = "WScript Shell 执行", .score = 90.0, .level = Severity::Critical},
        {.pattern = "%68%74%74%70", .desc = "URL 编码混淆", .score = 65.0, .level = Severity::Warn},
        {.pattern = "chr(", .desc = "chr 字符构造", .score = 60.0, .level = Severity::Warn},
        {.pattern = "$_REQUEST[", .desc = "请求变量读取", .score = 50.0, .level = Severity::Info},
        {.pattern = "$_POST[", .desc = "POST 变量读取", .score = 50.0, .level = Severity::Info},
        {.pattern = "$_GET[", .desc = "GET 变量读取", .score = 45.0, .level = Severity::Info},
        {.pattern = "$_COOKIE[", .desc = "Cookie 变量读取", .score = 55.0, .level = Severity::Warn},
        {.pattern = "$_SERVER[", .desc = "Server 变量读取", .score = 40.0, .level = Severity::Info},
        {.pattern = "preg_replace", .desc = "正则替换执行", .score = 80.0, .level = Severity::Error},
        {.pattern = "create_function(", .desc = "动态函数创建", .score = 85.0, .level = Severity::Critical},
        {.pattern = "call_user_func(", .desc = "回调函数执行", .score = 80.0, .level = Severity::Error},
        {.pattern = "array_map(", .desc = "数组映射执行", .score = 65.0, .level = Severity::Warn},
        {.pattern = "\"AUTH_USER\"", .desc = "大马认证头", .score = 90.0, .level = Severity::Critical},
        {.pattern = "\"AUTH_PASS\"", .desc = "大马认证密码", .score = 90.0, .level = Severity::Critical},
        {.pattern = "@ini_set(", .desc = "ini_set 配置修改", .score = 70.0, .level = Severity::Warn},
        {.pattern = "set_time_limit(0)", .desc = "超时解除", .score = 65.0, .level = Severity::Warn},
        {.pattern = "error_reporting(0)", .desc = "错误隐藏", .score = 70.0, .level = Severity::Warn},
        {.pattern = "ignore_user_abort(", .desc = "用户断开忽略", .score = 65.0, .level = Severity::Warn},
    };

    WebshellScanner::~WebshellScanner() { stop(); }

    void WebshellScanner::add_target(const ScanTarget &t) { targets_.push_back(t); }

    void WebshellScanner::add_target(const std::string &path, const std::string &desc) {
        targets_.push_back({.path = path, .description = desc});
    }

    void WebshellScanner::start(Arena &arena, const WebshellCallback &cb, int interval_sec) {
        if (running_) return;
        running_ = true;

        worker_ = std::thread([this, &arena, cb, interval_sec] {
            NZ_INFO("WebshellScanner: 启动，{} 个目标目录", targets_.size());

            while (running_) {
                for (auto results = scan_once(); const auto &r: results) {
                    event e{};

                    e.source = EventSource::Log;
                    e.proto = PROTO_TCP;
                    e.level = r.level;
                    e.msg = arena.intern(r.file_path);
                    e.fields.put(0, FieldVal::str(arena.intern(r.matched_pattern)));
                    e.fields.put(1, FieldVal::str(arena.intern(r.description)));
                    cb(e);
                }

                if (interval_sec <= 0) break;

                for (int i = 0; i < interval_sec && running_; ++i) {
                    std::this_thread::sleep_for(std::chrono::seconds(1));
                }
            }

            NZ_INFO("WebshellScanner: 退出，扫描 {} 文件，检出 {} 威胁", files_scanned_.load(), threats_found_.load());
        });
    }

    void WebshellScanner::stop() {
        running_ = false;
        if (worker_.joinable()) worker_.join();
    }

    std::vector<ScanResult> WebshellScanner::scan_once() {
        std::vector<ScanResult> all_results;

        files_scanned_ = 0;
        threats_found_ = 0;

        for (const auto &t: targets_) {
            std::error_code ec;

            if (!fs::exists(t.path, ec)) {
                NZ_DEBUG("WebshellScanner: 目标不存在，跳过: {}", t.path);
                continue;
            }
            if (fs::is_regular_file(t.path, ec)) {
                auto r = analyze_file(t.path, read_file(t.path));
                all_results.insert(all_results.end(), r.begin(), r.end());
            } else if (fs::is_directory(t.path, ec)) {
                for (auto it = fs::recursive_directory_iterator(t.path, ec);
                     it != fs::recursive_directory_iterator(); ++it) {
                    if (!running_) break;

                    if (it->is_regular_file() && is_webshell_ext(it->path().extension().string())) {
                        auto content = read_file(it->path().string());

                        if (!content.empty()) {
                            auto r = analyze_file(it->path().string(), content);
                            all_results.insert(all_results.end(), r.begin(), r.end());
                        }
                    }
                }
            }
        }

        files_scanned_ = all_results.size();
        threats_found_ = all_results.size();

        NZ_INFO("WebshellScanner: 扫描完成，检出 {} 个可疑文件", all_results.size());
        return all_results;
    }

    std::string WebshellScanner::read_file(const std::string &path) {
        std::ifstream f(path, std::ios::binary);
        if (!f.is_open()) return {};

        std::stringstream buf;
        buf << f.rdbuf();

        return buf.str();
    }

    bool WebshellScanner::is_webshell_ext(std::string_view path) {
        static constexpr std::array<std::string_view, 9> kExts = {
            ".php",
            ".phtml",
            ".php5",
            ".jsp",
            ".asp",
            ".aspx",
            ".py",
            ".js",
            ".sql",
        };

        return std::ranges::any_of(kExts, [&](auto e) {
            return path.ends_with(e);
        });
    }

    double WebshellScanner::shannon_entropy(std::string_view data) {
        if (data.empty()) return 0.0;

        std::array<int, 256> freq{};
        for (auto c: data) ++freq[static_cast<std::uint8_t>(c)];

        double entropy = 0.0;
        const auto n = static_cast<double>(data.size());

        for (const int f: freq) {
            if (f == 0) continue;
            const double p = static_cast<double>(f) / n;

            entropy -= p * std::log2(p);
        }

        return entropy;
    }

    std::vector<ScanResult> WebshellScanner::analyze_file(
        const std::string &path, std::string_view content) {
        std::vector<ScanResult> results;

        for (const auto &p: kPatterns) {
            const auto it = std::search(
                content.begin(), content.end(),
                p.pattern, p.pattern + std::strlen(p.pattern),
                [](char a, char b) { return std::tolower(a) == std::tolower(b); }
            );

            if (it != content.end()) {
                results.push_back({
                    .file_path = path, .matched_pattern = p.pattern, .description = p.desc, .score = p.score,
                    .level = p.level
                });
            }
        }

        double entropy = shannon_entropy(content);
        if (entropy > 5.5 && content.size() > 200) {
            results.push_back({
                path, "high_entropy",
                std::format("高熵值 ({:.1f}): 可能经过混淆编码", entropy),
                50.0 + std::min(entropy - 5.5, 3.0) * 10.0,
                entropy > 6.5 ? Severity::Error : Severity::Warn
            });
        }

        if (content.size() < 80 && !results.empty()) {
            results.push_back({
                path, "mini_shell", "极小文件含恶意特征，疑似一句话木马",
                85.0, Severity::Critical
            });
        }

        std::ranges::sort(results, [](const auto &a, const auto &b) {
            return a.score > b.score;
        });

        for (auto &r: results) {
            if (r.score > 98.0) r.score = 98.0;
        }

        return results;
    }
}
