//
// Created by 钟智强 on 2026/8/7.
//

#include "faillog.h"
#include "exec.h"

#include <algorithm>
#include <chrono>
#include <fstream>
#include <sstream>

namespace Nezha::Tools {

ToolResult FaillogTool::run(const ToolOptions &opts) const {
    auto t0 = std::chrono::steady_clock::now();
    ToolResult result;
    result.id = ToolId::Faillog;
    result.tool = "faillog";
    result.columns = {"用户名", "失败次数", "最后失败时间"};

#if defined(__linux__)
    std::ifstream f("/var/log/faillog", std::ios::binary);
    if (!f.is_open()) {
        result.ok = false;
        result.error = "无法打开 /var/log/faillog (需要 root 权限)";
        return result;
    }

    struct faillog_entry {
        short fail_cnt;
        int fail_time;
        char fail_line[32];
    };

    faillog_entry entry{};
    int count = 0;
    while (f.read(reinterpret_cast<char *>(&entry), sizeof(entry)) && count < opts.limit) {
        if (entry.fail_cnt > 0) {
            char time[32];
            time_t t = entry.fail_time;
            std::strftime(time, sizeof(time), "%Y-%m-%d %H:%M:%S", std::localtime(&t));
            std::string line(entry.fail_line, strnlen(entry.fail_line, sizeof(entry.fail_line)));
            result.rows.push_back({"uid+" + std::to_string(count), std::to_string(entry.fail_cnt),
                                   std::string(time)});
            ++count;
        }
    }

#elif defined(__APPLE__)
    std::string cmd = "log show --last " + std::to_string(opts.hours_back) +
                      "h --style compact --predicate 'eventMessage CONTAINS[c] \"failed\"' "
                      "2>/dev/null | head -" + std::to_string(opts.limit + 5);
    result.raw_text = run_command(cmd, 20000);

    std::istringstream ss(result.raw_text);
    std::string line;
    while (std::getline(ss, line)) {
        if (line.empty() || line.find("Timestamp") != std::string::npos) continue;
        // extract timestamp, process, message
        std::istringstream ls(line);
        std::string ts, host, proc;
        ls >> ts >> host >> proc;
        std::string msg;
        std::getline(ls, msg);
        if (!msg.empty() && msg[0] == ' ') msg = msg.substr(1);
        if (msg.empty()) continue;

        auto find_lower = [](const std::string &s, const std::string &pat) {
            std::string lower = s;
            std::ranges::transform(lower, lower.begin(),
                                   [](unsigned char c) { return std::tolower(c); });
            return lower.find(pat) != std::string::npos;
        };

        if (find_lower(msg, "failed") || find_lower(msg, "auth") ||
            find_lower(msg, "error")) {
            result.rows.push_back({proc, "—", ts + " " + msg.substr(0, 60)});
        }
    }
#endif

    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - t0);
    result.elapsed_ms = elapsed;
    result.ok = true;
    result.summary = std::to_string(result.rows.size()) + " 条失败日志 (用时 " +
                     std::to_string(elapsed.count()) + "ms)";
    return result;
}

} // namespace Nezha::Tools