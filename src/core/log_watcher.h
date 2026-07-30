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

    using LogEventCallback = std::function<void(const event &)>;

    struct LogSource {
        std::string path;
        EventSource source_type;
        AppId app_id = 0;
    };

    // tail -f 模式跟踪多个日志源，逐行解析；文件轮转后自动重开
    class LogWatcher {
    public:
        LogWatcher() = default;
        ~LogWatcher();

        LogWatcher(const LogWatcher &) = delete;
        LogWatcher &operator=(const LogWatcher &) = delete;

        void add_source(const LogSource &src);
        void start(Arena &arena, LogEventCallback cb);
        void stop();

        [[nodiscard]] bool running() const noexcept { return running_; }

    private:
        void watch_loop(Arena *arena, LogEventCallback *cb);
        bool parse_line(std::string_view line, Arena &arena, const LogSource &src, event &out);

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
