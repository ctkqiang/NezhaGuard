//
// Created by 钟智强 on 2026/8/7.
//

#ifndef NEZHAGUARD_TOOLS_HISTORY_H
#define NEZHAGUARD_TOOLS_HISTORY_H

#include "system_tool.h"

namespace Nezha::Tools {

class HistoryTool final : public SystemTool {
public:
    [[nodiscard]] ToolId id() const noexcept override { return ToolId::History; }
    ToolResult run(const ToolOptions &opts = {}) const override;
};

} // namespace Nezha::Tools

#endif // NEZHAGUARD_TOOLS_HISTORY_H