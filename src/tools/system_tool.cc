//
// Created by 钟智强 on 2026/8/7.
//

#include "system_tool.h"
#include "types.h"

#include <iostream>
#include <sstream>

namespace Nezha::Tools {

std::string format_plain(const ToolResult &result) {
    std::ostringstream ss;
    ss << result.tool << " — ";
    if (!result.ok) {
        ss << "错误: " << result.error << '\n';
        return ss.str();
    }

    ss << result.summary << "\n\n";

    // columns header
    for (size_t i = 0; i < result.columns.size(); ++i) {
        if (i > 0) ss << "  ";
        ss << result.columns[i];
    }
    ss << "\n──\n";

    // rows
    int limit = std::min(static_cast<int>(result.rows.size()), 100);
    for (int i = 0; i < limit; ++i) {
        for (size_t j = 0; j < result.rows[i].size(); ++j) {
            if (j > 0) ss << "  ";
            ss << result.rows[i][j];
        }
        ss << '\n';
    }

    if (static_cast<int>(result.rows.size()) > limit)
        ss << "… (还有 " << (result.rows.size() - limit) << " 行)\n";

    return ss.str();
}

int run_cli_tool(std::string_view name, const std::vector<std::string> &extra_args) {
    if (name == "list") {
        std::cout << "可用工具:\n";
        for (auto id : all_tool_ids())
            std::cout << "  " << tool_id_name(id) << " — " << tool_display_name(id) << '\n';
        return 0;
    }

    ToolId tid = tool_id_from_name(std::string(name));
    auto tool = make_tool(tid);
    if (!tool) {
        std::cerr << "未知工具: " << name << '\n';
        return 1;
    }

    ToolOptions opts;
    if (name == "grep" || name == "history") {
        for (size_t i = 0; i + 1 < extra_args.size(); ++i) {
            if (extra_args[i] == "--pattern") opts.pattern = extra_args[++i];
            else if (extra_args[i] == "--path") opts.path = extra_args[++i];
            else if (extra_args[i] == "--limit") opts.limit = std::stoi(extra_args[++i]);
            else if (extra_args[i] == "--context") opts.context_lines = std::stoi(extra_args[++i]);
            else if (extra_args[i] == "--hours") opts.hours_back = std::stoi(extra_args[++i]);
            else if (extra_args[i] == "--case") opts.case_sensitive = true;
        }
    } else if (name == "ps" || name == "netstat") {
        for (size_t i = 0; i + 1 < extra_args.size(); ++i) {
            if (extra_args[i] == "--limit") opts.limit = std::stoi(extra_args[++i]);
        }
    } else {
        for (size_t i = 0; i + 1 < extra_args.size(); ++i) {
            if (extra_args[i] == "--limit") opts.limit = std::stoi(extra_args[++i]);
            else if (extra_args[i] == "--hours") opts.hours_back = std::stoi(extra_args[++i]);
        }
    }

    auto result = tool->run(opts);
    std::cout << format_plain(result);
    return result.ok ? 0 : 1;
}

} // namespace Nezha::Tools