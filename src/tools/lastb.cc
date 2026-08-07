//
// Created by 钟智强 on 2026/8/7.
//

#include "lastb.h"
#include "exec.h"

#include <chrono>
#include <fstream>
#include <sstream>
#include <utmpx.h>

namespace Nezha::Tools {

ToolResult LastbTool::run(const ToolOptions &opts) const {
    auto t0 = std::chrono::steady_clock::now();
    ToolResult result;
    result.id = ToolId::Lastb;
    result.tool = "lastb";
    result.columns = {"用户名", "终端", "主机", "时间"};

#if defined(__linux__)
    // parse /var/log/btmp via utmpx
    std::ifstream f("/var/log/btmp", std::ios::binary);
    if (!f.is_open()) {
        result.ok = false;
        result.error = "无法打开 /var/log/btmp (需要 root 权限)";
        return result;
    }

    struct utmpx entry{};
    int count = 0;
    while (f.read(reinterpret_cast<char *>(&entry), sizeof(entry)) && count < opts.limit) {
        if (entry.ut_type == LOGIN_PROCESS || entry.ut_type == USER_PROCESS) {
            std::string user(entry.ut_user, strnlen(entry.ut_user, sizeof(entry.ut_user)));
            std::string tty(entry.ut_line, strnlen(entry.ut_line, sizeof(entry.ut_line)));
            std::string host(entry.ut_host, strnlen(entry.ut_host, sizeof(entry.ut_host)));
            char time[32];
            std::strftime(time, sizeof(time), "%Y-%m-%d %H:%M:%S",
                          std::localtime(&entry.ut_tv.tv_sec));
            result.rows.push_back({user, tty, host, std::string(time)});
            ++count;
        }
    }

#elif defined(__APPLE__)
    std::string cmd = "last -n " + std::to_string(opts.limit) + " 2>/dev/null";
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
            std::string tty = fields[1];
            std::string host = (fields[2][0] >= '0' && fields[2][0] <= '9') ? "-" : fields[2];
            std::string time;
            int start = host == "-" ? 2 : 3;
            for (size_t i = start; i < fields.size(); ++i) {
                if (!time.empty()) time += " ";
                time += fields[i];
            }
            result.rows.push_back({user, tty, host, time});
        }
    }
#endif

    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - t0);
    result.elapsed_ms = elapsed;
    result.ok = true;
    result.summary = std::to_string(result.rows.size()) + " 条失败登录 (用时 " +
                     std::to_string(elapsed.count()) + "ms)";
    return result;
}

} // namespace Nezha::Tools