//
// Created by 钟智强 on 2026/8/2.
//

#ifndef NEZHAGUARD_WEBSHELL_SCANNER_H
#define NEZHAGUARD_WEBSHELL_SCANNER_H

#include "types.h"
#include "event.h"
#include <atomic>

#include <functional>
#include <string>
#include <thread>
#include <vector>

namespace Nezha::Core {
    class Arena;

    using WebshellCallback = std::function<void(const event &)>;

    struct ScanTarget {
        std::string path;         // 扫描目录或文件路径
        std::string description;  // 可读描述
    };

    struct ScanResult {
        std::string file_path;
        std::string matched_pattern;
        std::string description;
        double score = 0.0;
        Severity level = Severity::Info;
    };

    class WebshellScanner {
    public:
        WebshellScanner() = default;
        ~WebshellScanner();

        WebshellScanner(const WebshellScanner &) = delete;
        WebshellScanner &operator=(const WebshellScanner &) = delete;

        // 添加扫描目标目录
        void add_target(const ScanTarget &t);
        void add_target(const std::string &path, const std::string &desc);

        // 启动后台扫描线程；interval_sec 为扫描间隔（0 = 仅一次）
        void start(Arena &arena, const WebshellCallback &cb, int interval_sec = 3600);

        void stop();

        // 同步扫描一次（非线程），返回检出结果
        [[nodiscard]] std::vector<ScanResult> scan_once();

        [[nodiscard]] bool running() const noexcept { return running_; }

        [[nodiscard]] std::size_t files_scanned() const noexcept { return files_scanned_; }
        [[nodiscard]] std::size_t threats_found() const noexcept { return threats_found_; }

        // 诊断/测试用公开方法
        static std::string read_file(const std::string &path);
        static bool is_webshell_ext(std::string_view path);
        static double shannon_entropy(std::string_view data);
        static std::vector<ScanResult> analyze_file(
            const std::string &path, std::string_view content);

    private:
        std::vector<ScanTarget> targets_;
        std::thread worker_;
        std::atomic<bool> running_{false};
        std::atomic<std::size_t> files_scanned_{0};
        std::atomic<std::size_t> threats_found_{0};
    };

} // namespace Nezha::Core

#endif //NEZHAGUARD_WEBSHELL_SCANNER_H
