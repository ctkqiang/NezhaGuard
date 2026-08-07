//
// Created by 钟智强 on 2026/8/7.
//

#include "grep.h"

#include <algorithm>
#include <chrono>
#include <fstream>

namespace Nezha::Tools {

ToolResult GrepTool::run(const ToolOptions &opts) const {
    auto t0 = std::chrono::steady_clock::now();

    ToolResult result;
    result.id = ToolId::Grep;
    result.tool = "grep";
    result.columns = {"行号", "内容"};

    std::string path = opts.path.empty() ? "logs/nezha.log" : opts.path;
    std::ifstream f(path);
    if (!f.is_open()) {
        result.ok = false;
        result.error = "无法打开文件: " + path;
        return result;
    }

    std::vector<std::string> ctx;
    int line_no = 0;
    int matched = 0;
    int after = 0;

    std::string pattern = opts.pattern;
    if (!opts.case_sensitive) {
        std::ranges::transform(pattern, pattern.begin(),
                               [](unsigned char c) { return std::tolower(c); });
    }

    auto match_line = [&](const std::string &line) -> bool {
        std::string haystack = line;
        if (!opts.case_sensitive)
            std::ranges::transform(haystack, haystack.begin(),
                                   [](unsigned char c) { return std::tolower(c); });
        return haystack.find(pattern) != std::string::npos;
    };

    std::string line;
    while (std::getline(f, line)) {
        ++line_no;
        if (after > 0) {
            result.rows.push_back({std::to_string(line_no), line});
            --after;
            if (after == 0) result.rows.push_back({"──", ""});
            continue;
        }

        if (match_line(line)) {
            ++matched;
            // context before
            for (int i = 0; i < opts.context_lines && i < static_cast<int>(ctx.size()); ++i) {
                int ctx_no = line_no - static_cast<int>(ctx.size()) + i;
                result.rows.push_back({std::to_string(ctx_no), ctx[i]});
            }
            ctx.clear();

            result.rows.push_back({std::to_string(line_no), line});
            after = opts.context_lines;
            if (after > 0) result.rows.push_back({"──", ""});

            if (opts.limit > 0 && matched >= opts.limit) break;
        } else if (opts.context_lines > 0) {
            ctx.push_back(line);
            if (static_cast<int>(ctx.size()) > opts.context_lines)
                ctx.erase(ctx.begin());
        }
    }

    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - t0);
    result.elapsed_ms = elapsed;
    result.ok = true;
    result.summary = std::to_string(matched) + " 条匹配 (用时 " +
                     std::to_string(elapsed.count()) + "ms)";
    return result;
}

} // namespace Nezha::Tools