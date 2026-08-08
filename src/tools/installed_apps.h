//
// Created by 钟智强 on 2026/8/8.
//

#ifndef NEZHAGUARD_TOOLS_INSTALLED_APPS_H
#define NEZHAGUARD_TOOLS_INSTALLED_APPS_H

#include "system_tool.h"

namespace Nezha::Tools {

class InstalledAppsTool final : public SystemTool {
public:
    [[nodiscard]] ToolId id() const noexcept override { return ToolId::InstalledApps; }
    ToolResult run(const ToolOptions &opts = {}) const override;
};

} // namespace Nezha::Tools

#endif // NEZHAGUARD_TOOLS_INSTALLED_APPS_H