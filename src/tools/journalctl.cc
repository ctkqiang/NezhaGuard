//
// Created by 钟智强 on 2026/8/7.
//

#include "journalctl.h"
#include "exec.h"

#include <chrono>
#include <sstream>

namespace Nezha::Tools {

ToolResult JournalctlTool::run(const ToolOptions &opts) const {
    auto t0 = std::chrono::steady_clock::now();
    ToolResult result;
    result.id = ToolId::Journalctl;
    result.tool = "journalctl";
    result.columns = {"时间", "进程", "内容"};

    std::string cmd;
    std::string limit_str = std::to_string(opts.limit);
    std::string hours_str = std::to_string(opts.hours_back);

#if defined(__linux__)
    cmd = "journalctl -n " + limit_str + " --no-pager -o short-iso 2>/dev/null";
    if (!opts.pattern.empty())
        cmd += " | grep -i '" + opts.pattern + "' | head -" + limit_str;
#elif defined(__APPLE__)
    cmd = "log show --last " + hours_str + "h --style compact --info 2>/dev/null";
    if (!opts.pattern.empty())
        cmd += " --predicate 'eventMessage CONTAINS[c] \"" + opts.pattern + "\"'";
    cmd += " | head -" + limit_str;
#else
    result.ok = false;
    result.error = "平台不支持 systemd journal";
    return result;
#endif

    result.raw_text = run_command(cmd, 25000);
    std::istringstream ss(result.raw_text);
    std::string line;

#if defined(__APPLE__)
    // macOS log show output: "timestamp host proc[pid]: message"
    while (std::getline(ss, line)) {
        if (line.empty() || line.starts_with("Timestamp") || line.starts_with("Filter"))
            continue;
        std::istringstream ls(line);
        std::string ts, host, proc;
        ls >> ts >> host >> proc;
        std::string msg;
        std::getline(ls, msg);
        if (!msg.empty() && msg[0] == ' ') msg = msg.substr(1);
        if (msg.size() > 120) msg = msg.substr(0, 120) + "…";
        result.rows.push_back({ts, proc, msg});
    }
#elif defined(__linux__)
    // journalctl short-iso output: "2026-08-07T12:00:00+0800 host proc[pid]: message"
    while (std::getline(ss, line)) {
        if (line.empty()) continue;
        std::istringstream ls(line);
        std::string ts, host, proc;
        ls >> ts >> host >> proc;
        std::string msg;
        std::getline(ls, msg);
        if (!msg.empty() && msg[0] == ' ') msg = msg.substr(1);
        if (msg.size() > 120) msg = msg.substr(0, 120) + "…";
        result.rows.push_back({ts, proc, msg});
    }
#endif

    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - t0);
    result.elapsed_ms = elapsed;
    result.ok = true;
    result.summary = std::to_string(result.rows.size()) + " 条日志 (用时 " +
                     std::to_string(elapsed.count()) + "ms)";
    return result;
}

} // namespace Nezha::Tools