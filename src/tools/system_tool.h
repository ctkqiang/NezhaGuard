//
// Created by 钟智强 on 2026/8/7.
//

#ifndef NEZHAGUARD_TOOLS_SYSTEM_TOOL_H
#define NEZHAGUARD_TOOLS_SYSTEM_TOOL_H

#include "types.h"

#include <memory>
#include <string>
#include <vector>

namespace Nezha::Tools {

class SystemTool {
public:
    virtual ~SystemTool() = default;
    SystemTool(const SystemTool &) = delete;
    SystemTool &operator=(const SystemTool &) = delete;

    [[nodiscard]] virtual ToolId id() const noexcept = 0;
    virtual ToolResult run(const ToolOptions &opts = {}) const = 0;

protected:
    SystemTool() = default;
};

std::unique_ptr<SystemTool> make_tool(ToolId id);
std::vector<ToolId> all_tool_ids();
int run_cli_tool(std::string_view name, const std::vector<std::string> &extra_args);

} // namespace Nezha::Tools

#endif // NEZHAGUARD_TOOLS_SYSTEM_TOOL_H