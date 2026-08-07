//
// Created by 钟智强 on 2026/8/7.
//

#include "lastlog.h"
#include "exec.h"

#include <chrono>
#include <sstream>
#include <vector>

namespace Nezha::Tools {

ToolResult LastlogTool::run(const ToolOptions &opts) const {
    auto t0 = std::chrono::steady_clock::now();
    ToolResult result;
    result.id = ToolId::Lastlog;
    result.tool = "lastlog";
    result.columns = {"用户名", "终端", "主机", "最后登录"};

    std::string cmd;
#if defined(__linux__)
    cmd = "lastlog 2>/dev/null | head -" + std::to_string(opts.limit + 1);
#elif defined(__APPLE__)
    cmd = "last -F -n " + std::to_string(opts.limit) + " 2>/dev/null";
#else
    result.ok = false;
    result.error = "平台不支持";
    return result;
#endif

    result.raw_text = run_command(cmd);
    std::istringstream ss(result.raw_text);
    std::string line;
    bool header_skipped = false;

    while (std::getline(ss, line)) {
        if (line.empty() || line.starts_with("wtmp begins") ||
            line.starts_with("reboot") || line.starts_with("shutdown"))
            continue;
        if (!header_skipped) { header_skipped = true; continue; }

        std::vector<std::string> fields;
        std::istringstream ls(line);
        std::string token;
        while (ls >> token) fields.push_back(token);

        if (fields.size() >= 3) {
            std::string user = fields[0];
            std::string tty = fields.size() > 1 ? fields[1] : "-";
            std::string host = fields.size() > 2 ? fields[2] : "-";
            std::string time;
            for (size_t i = 3; i < fields.size(); ++i) {
                if (!time.empty()) time += " ";
                time += fields[i];
            }
            if (time.empty()) time = "从未登录";
            result.rows.push_back({user, tty, host, time});
        }
    }

    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - t0);
    result.elapsed_ms = elapsed;
    result.ok = true;
    result.summary = std::to_string(result.rows.size()) + " 条记录 (用时 " +
                     std::to_string(elapsed.count()) + "ms)";
    return result;
}

} // namespace Nezha::Tools