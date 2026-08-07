//
// Created by 钟智强 on 2026/8/7.
//

#ifndef NEZHAGUARD_TOOLS_EXEC_H
#define NEZHAGUARD_TOOLS_EXEC_H

#include <string>

namespace Nezha::Tools {

std::string run_command(const std::string &cmd, int timeout_ms = 15000);

} // namespace Nezha::Tools

#endif // NEZHAGUARD_TOOLS_EXEC_H