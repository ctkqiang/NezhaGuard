//
// Created by 钟智强 on 2026/7/30.
//

#ifndef NEZHAGUARD_LOG_WATCHER_H
#define NEZHAGUARD_LOG_WATCHER_H

#include "types.h"
#include "event.h"
#include <functional>
#include <string>
#include <thread>
#include <vector>
#include <atomic>

namespace Nezha::Core {
    class Arena;

    // 日志采集回调
    using LogEventCallback = std::function<void(const event &)>;

    // 日志源配置
    struct LogSource {
        std::string path;        // 日志文件路径
        EventSource source_type; // Log 或 Honeypot
        AppId app_id = 0;        // 应用标识
    };

    // 日志文件监控器：tail -f 模式跟踪多个日志源，逐行解析生成 event
    class LogWatcher {
    public:
        LogWatcher() = default;
        ~LogWatcher();

        LogWatcher(const LogWatcher &) = delete;
        LogWatcher &operator=(const LogWatcher &) = delete;

        // 添加监控源
        void add_source(const LogSource &src);

        // 启动监控线程，arena 用于字符串驻留，cb 每行调用
        void start(Arena &arena, LogEventCallback cb);

        // 停止监控
        void stop();

        [[nodiscard]] bool running() const noexcept { return running_; }

    private:
        void watch_loop(Arena *arena, LogEventCallback *cb);
        bool parse_line(std::string_view line, Arena &arena, const LogSource &src, event &out);

        // 各日志格式解析
        static bool parse_combined(std::string_view line, Arena &arena, event &out);
        static bool parse_syslog(std::string_view line, Arena &arena, event &out);
        static bool parse_auth(std::string_view line, Arena &arena, event &out);
        static bool parse_json(std::string_view line, Arena &arena, event &out);

        std::vector<LogSource> sources_;
        std::thread worker_;
        std::atomic<bool> running_{false};
    };
}

#endif //NEZHAGUARD_LOG_WATCHER_H
