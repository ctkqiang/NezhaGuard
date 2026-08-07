//
// Created by 钟智强 on 2026/8/7.
//

#include "history.h"

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <fstream>

namespace Nezha::Tools {

ToolResult HistoryTool::run(const ToolOptions &opts) const {
    auto t0 = std::chrono::steady_clock::now();

    ToolResult result;
    result.id = ToolId::History;
    result.tool = "history";
    result.columns = {"序号", "命令"};

    const char *home = std::getenv("HOME");
    if (!home) {
        result.ok = false;
        result.error = "无法获取 HOME 目录";
        return result;
    }

    std::string path = opts.path.empty()
        ? std::string(home) + "/.zsh_history"
        : opts.path;

    std::ifstream f(path);
    if (!f.is_open()) {
        // fallback to bash
        path = std::string(home) + "/.bash_history";
        f.open(path);
    }
    if (!f.is_open()) {
        result.ok = false;
        result.error = "无法读取 shell 历史文件";
        return result;
    }

    std::vector<std::string> lines;
    std::string line;
    while (std::getline(f, line)) {
        if (line.empty()) continue;
        // strip zsh extended history prefix ": <epoch>:0;"
        if (line[0] == ':' && line.size() > 2) {
            auto semi = line.find(';');
            if (semi != std::string::npos)
                line = line.substr(semi + 1);
        }
        lines.push_back(line);
    }

    int start = std::max(0, static_cast<int>(lines.size()) - opts.limit);
    int n = 1;
    for (int i = start; i < static_cast<int>(lines.size()); ++i) {
        result.rows.push_back({std::to_string(n++), lines[i]});
    }

    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - t0);
    result.elapsed_ms = elapsed;
    result.ok = true;
    result.summary = std::to_string(result.rows.size()) + " 条历史 (用时 " +
                     std::to_string(elapsed.count()) + "ms)";
    return result;
}

} // namespace Nezha::Tools