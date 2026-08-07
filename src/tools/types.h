//
// Created by 钟智强 on 2026/8/7.
//

#ifndef NEZHAGUARD_TOOLS_TYPES_H
#define NEZHAGUARD_TOOLS_TYPES_H

#include <chrono>
#include <cstdint>
#include <string>
#include <vector>

namespace Nezha::Tools {

enum class ToolId : std::uint8_t {
    Lastlog,
    Journalctl,
    Lastb,
    Faillog,
    Netstat,
    Ps,
    History,
    Grep,
};

constexpr const char *tool_id_name(ToolId id) noexcept {
    switch (id) {
        case ToolId::Lastlog:    return "lastlog";
        case ToolId::Journalctl: return "journalctl";
        case ToolId::Lastb:      return "lastb";
        case ToolId::Faillog:    return "faillog";
        case ToolId::Netstat:    return "netstat";
        case ToolId::Ps:         return "ps";
        case ToolId::History:    return "history";
        case ToolId::Grep:       return "grep";
    }
    return "???";
}

constexpr const char *tool_display_name(ToolId id) noexcept {
    switch (id) {
        case ToolId::Lastlog:    return "最后登录";
        case ToolId::Journalctl: return "系统日志";
        case ToolId::Lastb:      return "失败登录";
        case ToolId::Faillog:    return "失败日志";
        case ToolId::Netstat:    return "网络连接";
        case ToolId::Ps:         return "进程列表";
        case ToolId::History:    return "命令历史";
        case ToolId::Grep:       return "日志搜索";
    }
    return "???";
}

inline ToolId tool_id_from_name(const std::string &name) {
    if (name == "lastlog")    return ToolId::Lastlog;
    if (name == "journalctl") return ToolId::Journalctl;
    if (name == "lastb")      return ToolId::Lastb;
    if (name == "faillog")    return ToolId::Faillog;
    if (name == "netstat")    return ToolId::Netstat;
    if (name == "ps")         return ToolId::Ps;
    if (name == "history")    return ToolId::History;
    if (name == "grep")       return ToolId::Grep;
    return ToolId::Ps;
}

struct ToolOptions {
    std::string pattern;
    std::string path;
    int limit = 200;
    int hours_back = 1;
    bool case_sensitive = false;
    int context_lines = 0;
};

struct ToolResult {
    ToolId id{};
    bool ok = false;
    std::string tool;
    std::string summary;
    std::string error;
    std::vector<std::string> columns;
    std::vector<std::vector<std::string>> rows;
    std::string raw_text;
    std::chrono::milliseconds elapsed_ms{};
};

std::string format_plain(const ToolResult &result);

} // namespace Nezha::Tools

#endif // NEZHAGUARD_TOOLS_TYPES_H